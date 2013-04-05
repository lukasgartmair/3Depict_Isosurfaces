/* 
 * APTClasses.h - Generic APT components code
 * Copyright (C) 2013  D Haley
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "APTClasses.h"
#include "../../common/stringFuncs.h"

#include "../../common/translation.h"


using std::pair;
using std::string;
using std::vector;
using std::ifstream;
using std::make_pair;

const char *POS_ERR_STRINGS[] = { "",
       				NTRANS("Memory allocation failure on POS load"),
				NTRANS("Error opening pos file"),
				NTRANS("Pos file empty"),
				NTRANS("Pos file size appears to have non-integer number of entries"),
				NTRANS("Error reading from pos file (after open)"),
				NTRANS("Error - Found NaN in pos file"),
				NTRANS("Pos load aborted by interrupt.")
};

enum
{
	TEXT_ERR_OPEN=1,
	TEXT_ERR_ONLY_HEADER,
	TEXT_ERR_REOPEN,
	TEXT_ERR_READ_CONTENTS,
	TEXT_ERR_FORMAT,
	TEXT_ERR_NUM_FIELDS,
	TEXT_ERR_ALLOC_FAIL,
	TEXT_ERR_ENUM_END //not an error, just end of enum
};

const char *ION_TEXT_ERR_STRINGS[] = { "",
       					NTRANS("Error opening file"),
       					NTRANS("No numerical data found"),
       					NTRANS("Error re-opening file, after first scan"),
       					NTRANS("Unable to read file contents after open"),
					NTRANS("Error interpreting field in file"),
					NTRANS("Incorrect number of fields in file"),
					NTRANS("Unable to allocate memory to store data"),
					};
//!Create an pos file from a vector of IonHits
unsigned int IonVectorToPos(const vector<IonHit> &ionVec, const string &filename)
{
	std::ofstream CFile(filename.c_str(),std::ios::binary);
	float floatBuffer[4];

	if (!CFile)
		return 1;

	for (unsigned int ui=0; ui<ionVec.size(); ui++)
	{
		ionVec[ui].makePosData(floatBuffer);
		CFile.write((char *)floatBuffer,4*sizeof(float));
	}
	return 0;
}



void appendPos(const vector<IonHit> &points, const char *name)
{
	std::ofstream posFile(name,std::ios::binary|std::ios::app);	

	float data[4];	
	
	for(unsigned int ui=0; ui< points.size(); ui++)
	{
		points[ui].makePosData(data);
		posFile.write((char *)data, 4*sizeof(float));
	}
}

void getPointsFromIons(const vector<IonHit> &ions, vector<Point3D> &p)
{
	p.resize(ions.size());
#pragma omp parallel for
	for(size_t ui=0;ui<ions.size();ui++)
		p[ui] = ions[ui].getPosRef();
}

unsigned int LimitLoadPosFile(unsigned int inputnumcols, unsigned int outputnumcols, unsigned int index[], vector<IonHit> &posIons,const char *posFile, size_t limitCount,
	       	unsigned int &progress, bool (*callback)(bool),bool strongSampling)
{

	//Function is only defined for 4 columns here.
	ASSERT(outputnumcols == 4);
	//buffersize must be a power of two and at least outputnumcols*sizeof(float)
	const unsigned int NUMROWS=1;
	const unsigned int BUFFERSIZE=inputnumcols * sizeof(float) * NUMROWS;
	const unsigned int BUFFERSIZE2=outputnumcols * sizeof(float) * NUMROWS;
	char *buffer=new char[BUFFERSIZE];

	
	if(!buffer)
		return POS_ALLOC_FAIL;

	char *buffer2=new char[BUFFERSIZE2];
	if(!buffer2)
	{
		delete[] buffer;
		return POS_ALLOC_FAIL;
	}

	//open pos file
	std::ifstream CFile(posFile,std::ios::binary);

	if(!CFile)
	{
		delete[] buffer;
		delete[] buffer2;
		return POS_OPEN_FAIL;
	}
	
	CFile.seekg(0,std::ios::end);
	size_t fileSize=CFile.tellg();

	if(!fileSize)
	{
		delete[] buffer;
		delete[] buffer2;
		return POS_EMPTY_FAIL;
	}
	
	CFile.seekg(0,std::ios::beg);
	
	//calculate the number of points stored in the POS file
	size_t pointCount=0;
	size_t maxIons;
	size_t maxCols = inputnumcols * sizeof(float);
	//regular case
	
	if(fileSize % maxCols)
	{
		delete[] buffer;
		delete[] buffer2;
		return POS_SIZE_MODULUS_ERR;	
	}

	maxIons =fileSize/maxCols;
	limitCount=std::min(limitCount,maxIons);

	//If we are going to load the whole file, don't use a sampling method to do it.
	if(limitCount == maxIons)
	{
		//Close the file
		CFile.close();
		delete[] buffer;
		delete[] buffer2;
		//Try opening it using the normal functions
		return GenericLoadFloatFile(inputnumcols, outputnumcols, index, posIons,posFile,progress, callback);
	}

	//Use a sampling method to load the pos file
	std::vector<size_t> ionsToLoad;
	try
	{
		posIons.resize(limitCount);

		RandNumGen rng;
		rng.initTimer();
		unsigned int dummy;
		randomDigitSelection(ionsToLoad,maxIons,rng,
				limitCount,dummy,callback,strongSampling);
	}
	catch(std::bad_alloc)
	{
		delete[] buffer;
		delete[] buffer2;
		return POS_ALLOC_FAIL;
	}


	//sort again	
	GreaterWithCallback<size_t> g(callback,PROGRESS_REDUCE);
	std::sort(ionsToLoad.begin(),ionsToLoad.end(),g);

	unsigned int curProg = PROGRESS_REDUCE;	

	//TODO: probably not very nice to the disk drive. would be better to
	//scan ahead for contiguous data regions, and load that where possible.
	//Or switch between different algorithms based upon ionsToLoad.size()/	
	std::ios::pos_type  nextIonPos;
	for(size_t ui=0;ui<ionsToLoad.size(); ui++)
	{
		nextIonPos =  ionsToLoad[ui]*maxCols;
		
		if(CFile.tellg() !=nextIonPos )
			CFile.seekg(nextIonPos);

		CFile.read(buffer,BUFFERSIZE);

		for (size_t i = 0; i < outputnumcols; i++) // iterate through floats
			memcpy(&(buffer2[i * sizeof(float)]), &(buffer[index[i] * sizeof(float)]), sizeof(float));
		
		if(!CFile.good())
			return POS_READ_FAIL;
		posIons[ui].setHit((float*)buffer2);
		//Data bytes stored in pos files are big
		//endian. flip as required
		#ifdef __LITTLE_ENDIAN__
			posIons[ui].switchEndian();	
		#endif
		
		if(posIons[ui].hasNaN())
		{
			delete[] buffer;
			delete[] buffer2;
			return POS_NAN_LOAD_ERROR;	
		}
			
		pointCount++;
		if(!curProg--)
		{

			progress= (unsigned int)((float)(CFile.tellg())/((float)fileSize)*100.0f);
			curProg=PROGRESS_REDUCE;
			if(!(*callback)(false))
			{
				delete[] buffer;
				posIons.clear();
				return POS_ABORT_FAIL;
				
			}
		}
				
	}

	delete[] buffer;
	delete[] buffer2;
	return 0;
}

unsigned int GenericLoadFloatFile(unsigned int inputnumcols, unsigned int outputnumcols, 
		unsigned int index[], vector<IonHit> &posIons,const char *posFile, 
			unsigned int &progress, bool (*callback)(bool))
{
	ASSERT(outputnumcols==4); //Due to ionHit.setHit
	//buffersize must be a power of two and at least sizeof(float)*outputnumCols
	const unsigned int NUMROWS=512;
	const unsigned int BUFFERSIZE=inputnumcols * sizeof(float) * NUMROWS;
	const unsigned int BUFFERSIZE2=outputnumcols * sizeof(float) * NUMROWS;

	char *buffer=new char[BUFFERSIZE];
	
	if(!buffer)
		return POS_ALLOC_FAIL;
	
	char *buffer2=new char[BUFFERSIZE2];
	if(!buffer2)
	{
		delete[] buffer;
		return POS_ALLOC_FAIL;
	}
	//open pos file
	std::ifstream CFile(posFile,std::ios::binary);
	
	if(!CFile)
	{
		delete[] buffer;
		delete[] buffer2;
		return POS_OPEN_FAIL;
	}
	
	CFile.seekg(0,std::ios::end);
	size_t fileSize=CFile.tellg();
	
	if(!fileSize)
	{
		delete[] buffer;
		delete[] buffer2;
		return POS_EMPTY_FAIL;
	}
	
	CFile.seekg(0,std::ios::beg);
	
	//calculate the number of points stored in the POS file
	IonHit hit;
	typedef struct IONHIT
	{
		float pos[3];
		float massToCharge;
	} IONHIT;
	size_t pointCount=0;
	//regular case
	size_t curBufferSize=BUFFERSIZE;
	size_t curBufferSize2=BUFFERSIZE2;
	
	if(fileSize % (inputnumcols * sizeof(float)))
	{
		delete[] buffer;
		delete[] buffer2;
		return POS_SIZE_MODULUS_ERR;	
	}
	
	try
	{
		posIons.resize(fileSize/(inputnumcols*sizeof(float)));
	}
	catch(std::bad_alloc)
	{
		delete[] buffer;
		delete[] buffer2;
		return POS_ALLOC_FAIL;
	}
	
	
	while(fileSize < curBufferSize) {
		curBufferSize = curBufferSize >> 1;
		curBufferSize2 = curBufferSize2 >> 1;
	}		
	
	//Technically this is dependent upon the buffer size.
	unsigned int curProg = 10000;	
	size_t ionP=0;
	int maxCols = inputnumcols * sizeof(float);
	int maxPosCols = outputnumcols * sizeof(float);
	do
	{
		//Taking curBufferSize chunks at a time, read the input file
		while((size_t)CFile.tellg() <= fileSize-curBufferSize)
		{
			CFile.read(buffer,curBufferSize);
			if(!CFile.good())
			{
				delete[] buffer;
				delete[] buffer2;
				return POS_READ_FAIL;
			}
			
			for (unsigned int j = 0; j < NUMROWS; j++) // iterate through rows
			{
				for (unsigned int i = 0; i < outputnumcols; i++) // iterate through floats
				{
					memcpy(&(buffer2[j * maxPosCols + i * sizeof(float)]), 
						&(buffer[j * maxCols + index[i] * sizeof(float)]), sizeof(float));
				}
			}
			
			unsigned int ui;
			for(ui=0; ui<curBufferSize2; ui+=(sizeof(IONHIT)))
			{
				hit.setHit((float*)(buffer2+ui));
				//Data bytes stored in pos files are big
				//endian. flip as required
				#ifdef __LITTLE_ENDIAN__
					hit.switchEndian();	
				#endif
				
				if(hit.hasNaN())
				{
					delete[] buffer;
					delete[] buffer2;
					return POS_NAN_LOAD_ERROR;	
				}
				posIons[ionP] = hit;
				ionP++;
				
				pointCount++;
			}	
			
			if(!curProg--)
			{
				progress= (unsigned int)((float)(CFile.tellg())/((float)fileSize)*100.0f);
				curProg=PROGRESS_REDUCE;
				if(!(*callback)(false))
				{
					delete[] buffer;
					delete[] buffer2;
					posIons.clear();
					return POS_ABORT_FAIL;
				
				}
			}
				
		}

		curBufferSize = curBufferSize >> 1 ;
		curBufferSize2 = curBufferSize2 >> 1 ;
	}while(curBufferSize2 >= sizeof(IONHIT));
	
	ASSERT((unsigned int)CFile.tellg() == fileSize);
	delete[] buffer;
	delete[] buffer2;
	
	return 0;
}


//TODO: Add progress
unsigned int limitLoadTextFile(unsigned int numColsTotal, unsigned int selectedCols[], 
			vector<IonHit> &posIons,const char *textFile, const char *delim, const size_t limitCount,
				unsigned int &progress, bool (*callback)(bool),bool strongRandom)
{

	ASSERT(numColsTotal);
	ASSERT(textFile);

	vector<size_t> newLinePositions;
	std::vector<std::string> subStrs;

	//Do a brute force scan through the dataset
	//to locate newlines.
	char *buffer;
	const int BUFFER_SIZE=16384; //This is totally a guess. I don't know what is best.


	//sort the selected columns into increasing order
	vector<int> sortedCols;
	for(unsigned int ui=0;ui<numColsTotal;ui++)
		sortedCols.push_back(selectedCols[ui]);

	std::sort(sortedCols.begin(),sortedCols.end());
	//the last entry in the sorted list tells us how many
	// entries we need in each line at a minimum
	unsigned int maxCol=sortedCols[sortedCols.size()-1];

	ifstream CFile(textFile,std::ios::binary);

	if(!CFile)
		return TEXT_ERR_OPEN;


	//seek to the end of the file
	//to get the filesize
	size_t maxPos,bytesToRead,curPos;
	CFile.seekg(0,std::ios::end);
	maxPos=CFile.tellg();

	CFile.close();

	CFile.open(textFile);

	curPos=0;


	//Scan through file for end of header.
	//we define this as the split value being able to generate 
	//1) Enough data to make interpretable columns
	//2) Enough columns that can be interpreted.
	while(!CFile.eof() && curPos < maxPos)
	{
		string s;
		curPos = CFile.tellg();
		getline(CFile,s);

		if(!CFile.good())
			return TEXT_ERR_READ_CONTENTS;

		splitStrsRef(s.c_str(),delim,subStrs);	
		stripZeroEntries(subStrs);

		//Not enough entries in this line
		//to be interpretable
		if(subStrs.size() <maxCol)
			continue;

		bool enoughSubStrs;
		enoughSubStrs=true;
		for(unsigned int ui=0;ui<numColsTotal; ui++)
		{
			if(selectedCols[ui] >=subStrs.size())
			{
				enoughSubStrs=false;
				break;	
			}
		}

		if(!enoughSubStrs)
			continue;

		//Unable to stream
		bool unStreamable;
		unStreamable=false;
		for(unsigned int ui=0; ui<numColsTotal; ui++)
		{
			float f;
			if(stream_cast(f,subStrs[selectedCols[ui]]))
			{
				unStreamable=true;
				break;
			}

		}

		//well, we can stream this, so it assume it is not the header.
		if(!unStreamable)
			break;	
	}	

	//could not find any data.. only header.
	if(CFile.eof() || curPos >=maxPos)
		return TEXT_ERR_ONLY_HEADER;


	CFile.close();

	//Re-open the file in binary mode to find the newlines
	CFile.open(textFile,std::ios::binary);

	if(!CFile)
		return TEXT_ERR_REOPEN;


	//Jump to beyond the header
	CFile.seekg(curPos);


	//keep a beginning of file marker
	newLinePositions.push_back(curPos);
	bool seenNumeric=false;
	buffer = new char[BUFFER_SIZE];
	while(!CFile.eof() && curPos < maxPos)
	{
		if(!CFile.good())
		{
			delete[] buffer;
			return TEXT_ERR_READ_CONTENTS;
		}
		//read up to BUFFER_SIZE bytes from the file
		//but only if they are available
		bytesToRead = std::min(maxPos-curPos,(size_t)BUFFER_SIZE);

		CFile.read(buffer,bytesToRead);

		//check that this buffer contains numeric info	
		for(unsigned int ui=0;ui<bytesToRead; ui++)
		{
			//Check for a unix-style endline
			//or the latter part of a windows endline
			//either or, whatever.
			if( buffer[ui] == '\n')
			{
				//Check that we have not hit a run of non-numeric data
				if(seenNumeric)
					newLinePositions.push_back(ui+curPos);
			} 
			else if(buffer[ui] >= '0' && buffer[ui] <='9')
				seenNumeric=true;
		}
		
		curPos+=bytesToRead;	
	
	}
	
	//Don't keep any newline that hits the end of the file, but do keep a zero position
	if(newLinePositions.size())
		newLinePositions.pop_back();
	CFile.close();


	//OK, so now we know where those pesky endlines are. This gives us jump positions
	//to new lines in the file. Each component must have some form of numeric data 
	//preceding it. That numeric data may not be fully parseable, but we assume it is until we know better.
	//
	//If it is *not* parseable, just throw an error when we find that out.

	//If we are going to load the whole file, don't use a sampling method to do it.
	if(limitCount >=newLinePositions.size())
	{
		delete[] buffer;

		vector<vector<float> > data;
		vector<string> header;
	
		//Just use the non-sampling method to load.	
		if(loadTextData(textFile,data,header,delim))
			return TEXT_ERR_FORMAT;

		if(data.size() !=4)
			return TEXT_ERR_NUM_FIELDS;

		posIons.resize(data[0].size());
		for(size_t ui=0;ui<data[0].size();ui++)
		{

			ASSERT(numColsTotal==4);
			//This is output specific
			//and assumes that we have exactly 4 input cols
			//to match ot our output vector of pos ions
			posIons[ui].setPos(Point3D(data[selectedCols[0]][ui],
						data[selectedCols[1]][ui],
						data[selectedCols[2]][ui]));
			posIons[ui].setMassToCharge(data[selectedCols[3]][ui]);
		}

		return 0;	
	}

	//Generate some random positions to load
	std::vector<size_t> dataToLoad;
	try
	{
		posIons.resize(limitCount);

		RandNumGen rng;
		rng.initTimer();
		unsigned int dummy;
		randomDigitSelection(dataToLoad,newLinePositions.size(),rng,
				limitCount,dummy,callback,strongRandom);
	}
	catch(std::bad_alloc)
	{
		delete[] buffer;
		return TEXT_ERR_ALLOC_FAIL;
	}


	//Sort the data such that we are going to
	//always jump forwards in the file; better disk access and whatnot.
	GreaterWithCallback<size_t> g(callback,PROGRESS_REDUCE);
	std::sort(dataToLoad.begin(),dataToLoad.end(),g);

	//OK, so we have  a list of newlines
	//that we can use as entry points for random seek.
	//We also have some random entry points (sorted).
	// Now re-open the file in text mode and try to load the
	// data specified at the offsets


	//Open file in text mode
	CFile.open(textFile);

	if(!CFile)
	{
		delete[] buffer;
		return TEXT_ERR_REOPEN;
	}


	//OK, now jump to each of the random positions,
	//as dictated by the endline position 
	//and attempt a parsing there.

	subStrs.clear();
	for(size_t ui=0;ui<dataToLoad.size();ui++)	
	{

		std::ios::pos_type  nextIonPos;
		//Jump to position immediately after the newline
		nextIonPos = (newLinePositions[dataToLoad[ui]]+1);
		
		if(CFile.tellg() !=nextIonPos )
			CFile.seekg(nextIonPos);

		string s;

		getline(CFile,s);

		//Now attempt to scan the line for the selected columns.
		//split around whatever delimiter we can find
		splitStrsRef(s.c_str(),delim,subStrs);	

		if(subStrs.size() < numColsTotal)
		{
			//FIXME: Allow skipping of bad lines
			delete[] buffer;
			return TEXT_ERR_NUM_FIELDS;
		}

		float f[4];

		for(size_t uj=0;uj<sortedCols.size();uj++)
		{

			if(stream_cast(f[uj],subStrs[sortedCols[uj]]))
			{
				//FIXME: Allow skipping bad lines
				//Can't parse line.. Abort.
				delete[] buffer;
				return TEXT_ERR_FORMAT;
			}
		}

		ASSERT(ui<posIons.size());
		posIons[ui].setHit(f);
	}

	delete[] buffer;
	return 0;

}

IonHit::IonHit() 
{
	//At this point I deliberately don't initialise the point class
	//as in DEBUG mode, the point class will catch failure to init
}

IonHit::IonHit(const IonHit &obj2) : massToCharge(obj2.massToCharge), pos(obj2.pos)
{
}

IonHit::IonHit(const Point3D &p, float newMass) : massToCharge(newMass), pos(p)
{
}

void IonHit::setMassToCharge(float newMass)
{
	massToCharge=newMass;
}

float IonHit::getMassToCharge() const
{
	return massToCharge;
}	


void IonHit::setPos(const Point3D &p)
{
	pos=p;
}

#ifdef __LITTLE_ENDIAN__
void IonHit::switchEndian()
{
	
	pos.switchEndian();
	floatSwapBytes(&(massToCharge));
}
#endif

const IonHit &IonHit::operator=(const IonHit &obj)
{
	massToCharge=obj.massToCharge;
	pos = obj.pos;

	return *this;
}

IonHit IonHit::operator+(const Point3D &obj)
{
	//FIXME: I think this is wrong???
	ASSERT(false);
	pos.add(obj);	
	return *this;
}

void IonHit::makePosData(float *floatArr) const
{
	ASSERT(floatArr);
	//copy positional information
	pos.copyValueArr(floatArr);

	//copy mass to charge data
	*(floatArr+3) = massToCharge;
		
	#ifdef __LITTLE_ENDIAN__
		floatSwapBytes(floatArr);
		floatSwapBytes((floatArr+1));
		floatSwapBytes((floatArr+2));
		floatSwapBytes((floatArr+3));
	#endif
}

Point3D IonHit::getPos() const
{
	return pos;
}	

bool IonHit::hasNaN()
{
	return (std::isnan(massToCharge) || std::isnan(pos[0]) || 
				std::isnan(pos[1]) || std::isnan(pos[2]));
}





void getPointSum(const std::vector<IonHit> &points,Point3D &centroid)
{
	//TODO: Paralellise me
	centroid=Point3D(0,0,0);
	for(unsigned int ui=0;ui<points.size();ui++)
		centroid+=points[ui].getPos();
}

BoundCube getIonDataLimits(const std::vector<IonHit> &points)
{
	ASSERT(points.size());

	BoundCube b;	
	b.setInverseLimits();	
#ifndef OPENMP
	float bounds[3][2];
	for(unsigned int ui=0;ui<3;ui++)
	{
		bounds[ui][0]=std::numeric_limits<float>::max();
		bounds[ui][1]=-std::numeric_limits<float>::max();
	}
	
	for(unsigned int ui=0; ui<points.size(); ui++)
	{

		for(unsigned int uj=0; uj<3; uj++)
		{
			Point3D p;
			p=points[ui].getPos();
			if(p.getValue(uj) < bounds[uj][0])
				bounds[uj][0] = p.getValue(uj);
			
			if(p.getValue(uj) > bounds[uj][1])
				bounds[uj][1] = p.getValue(uj);
		}
	}

	b.setBounds(bounds[0][0],bounds[1][0],
			bounds[2][0],bounds[0][1],
			bounds[1][1],bounds[2][1]);
#else
	// parallel version
	vector<BoundCube> cubes;

	unsigned int nT=omp_get_max_threads();
	cubes.resize(nT);
	for(unsigned int ui=0;ui<cubes.size();ui++)
		cube[ui].setInverseLimits();

	unsigned int tCount=1;
	#pragma omp parallel for reduction(tCount|+)
	for(unsigned int ui=0;ui<points.size();ui++)
	{
		Point3D p;
		p=points[ui].getPos();
		for(unsigned int uj=0;uj<3;uj++)
		{
			b.setBounds(uj,0,std::min(b.getBound(uj,0),p[uj]));
			b.setBounds(uj,1,std::min(b.getBound(uj,0),p[uj]));
		}
	}

	for(unsigned int ui=0;ui<std::min(tCount,nT);ui++)
		b.expand(cubes[ui]);

#endif

	return b;
}
