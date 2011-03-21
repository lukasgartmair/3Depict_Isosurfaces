/* 
 * APTClasses.h - Generic APT components code
 * Copyright (C) 2010  D Haley
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
using std::pair;
using std::string;
using std::vector;
using std::ifstream;
using std::cerr;


#include <limits>
#include <cstring>


//No two entries in table may match. NUM_ELEMENTS contains number of entries
const char *cpAtomNaming[][2] = { 
	{"Hydrogen","H"},
	{"Helium","He"},
	{"Lithium","Li"},
	{"Beryllium","Be"},
	{"Boron","B"},
	{"Carbon","C"},
	{"Nitrogen","N"},
	{"Oxygen","O"},
	{"Fluorine","F"},
	{"Neon","Ne"},
	{"Sodium","Na"},
	{"Magnesium","Mg"},
	{"Aluminium","Al"},
	{"Silicon","Si"},
	{"Phosphorus","P"},
	{"Sulfur","S"},
	{"Chlorine","Cl"},
	{"Argon","Ar"},
	{"Potassium","K"},
	{"Calcium","Ca"},
	{"Scandium","Sc"},
	{"Titanium","Ti"},
	{"Vanadium","V"},
	{"Chromium","Cr"},
	{"Manganese","Mn"},
	{"Iron","Fe"},
	{"Cobalt","Co"},
	{"Nickel","Ni"},
	{"Copper","Cu"},
	{"Zinc","Zn"},
	{"Gallium","Ga"},
	{"Germanium","Ge"},
	{"Arsenic","As"},
	{"Selenium","Se"},
	{"Bromine","Br"},
	{"Krypton","Kr"},
	{"Rubidium","Rb"},
	{"Strontium","Sr"},
	{"Yttrium","Y"},
	{"Zirconium","Zr"},
	{"Niobium","Nb"},
	{"Molybdenum","Mo"},
	{"Technetium","Tc"},
	{"Ruthenium","Ru"},
	{"Rhodium","Rh"},
	{"Palladium","Pd"},
	{"Silver","Ag"},
	{"Cadmium","Cd"},
	{"Indium","In"},
	{"Tin","Sn"},
	{"Antimony","Sb"},
	{"Tellurium","Te"},
	{"Iodine","I"},
	{"Xenon","Xe"},
	{"Caesium","Cs"},
	{"Barium","Ba"},
	{"Lanthanum","La"},
	{"Cerium","Ce"},
	{"Praseodymium","Pr"},
	{"Neodymium","Nd"},
	{"Promethium","Pm"},
	{"Samarium","Sm"},
	{"Europium","Eu"},
	{"Gadolinium","Gd"},
	{"Terbium","Tb"},
	{"Dysprosium","Dy"},
	{"Holmium","Ho"},
	{"Erbium","Er"},
	{"Thulium","Tm"},
	{"Ytterbium","Yb"},
	{"Lutetium","Lu"},
	{"Hafnium","Hf"},
	{"Tantalum","Ta"},
	{"Tungsten","W"},
	{"Rhenium","Re"},
	{"Osmium","Os"},
	{"Iridium","Ir"},
	{"Platinum","Pt"},
	{"Gold","Au"},
	{"Mercury","Hg"},
	{"Thallium","Tl"},
	{"Lead","Pb"},
	{"Bismuth","Bi"},
	{"Polonium","Po"},
	{"Astatine","At"},
	{"Radon","Rn"},
	{"Francium","Fr"},
	{"Radium","Ra"},
	{"Actinium","Ac"},
	{"Thorium","Th"},
	{"Protactinium","Pa"},
	{"Uranium","U"},
	{"Neptunium","Np"},
	{"Plutonium","Pu"},
	{"Americium","Am"},
	{"Curium","Cm"},
	{"Berkelium","Bk"},
	{"Californium","Cf"},
	{"Einsteinium","Es"},
	{"Fermium","Fm"},
	{"Mendelevium","Md"},
	{"Nobelium","No"},
	{"Lawrencium","Lr"},
	{"Rutherfordium","Rf"},
	{"Dubnium","Db"},
	{"Seaborgium","Sg"},
	{"Bohrium","Bh"},
	{"Hassium","Hs"},
	{"Meitnerium","Mt"},
	{"Darmstadtium","Ds"},
	{"Roentgenium","Rg"},
	{"Ununbium","Uub"},
	{"Ununtrium","Uut"},
	{"Ununquadium","Uuq"},
	{"Ununpentium","Uup"},
	{"Ununhexium","Uuh"},
	{"Ununseptium","Uus"},
	{"Ununoctium","Uuo"},
};

const char *posErrStrings[] = { "",
       				"Memory allocation failure on POS load",
				"Error opening pos file",
				"Pos file empty",
				"Pos file size appears to have non-integer number of entries",
				"Error reading from pos file (after open)",
				"Error - Found NaN in pos file",
				"Pos load aborted by interrupt."
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
//


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

unsigned int LimitLoadPosFile(unsigned int inputnumcols, unsigned int outputnumcols, unsigned int index[], vector<IonHit> &posIons,const char *posFile, size_t limitCount,
	       	unsigned int &progress, bool (*callback)(),bool strongSampling)
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
	//scan ahead for contigous data regions, and load that where possible.
	//Or switch between different algorithms based upon ionsToLoad.size()/	
	std::ios::pos_type  nextIonPos;
	for(size_t ui=0;ui<ionsToLoad.size(); ui++)
	{
		nextIonPos =  ionsToLoad[ui]*maxCols;
		
		if(CFile.tellg() !=nextIonPos )
			CFile.seekg(nextIonPos);

		CFile.read(buffer,BUFFERSIZE);

		for (int i = 0; i < outputnumcols; i++) // iterate through floats
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
			if(!(*callback)())
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
			unsigned int &progress, bool (*callback)())
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
	
	//Technically this is dependant upon the buffer size.
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
				if(!(*callback)())
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

IonHit::IonHit() : massToCharge(0.0f), pos(0,0,0)
{
	//At this point i deliberately dont initialise the point class
	//as in DEBUG mode, the point class will catch failure to init
}

IonHit::IonHit(const IonHit &obj2)
{
	massToCharge=obj2.massToCharge;
	pos = obj2.pos;
}

IonHit::IonHit(Point3D p, float newMass)
{
	massToCharge=newMass;
	pos=p;
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

IonHit IonHit::operator=(const IonHit &obj)
{
	massToCharge=obj.massToCharge;
	pos = obj.pos;

	return *this;
}

IonHit IonHit::operator+(const Point3D &obj)
{
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



RangeFile::RangeFile()
{
	errState=0;
}

unsigned int RangeFile::write(std::ostream &f) const
{

	ASSERT(colours.size() == ionNames.size());

	//File header
	f << ionNames.size() << " " ; 
	f << ranges.size() << std::endl;

	//Colour and longname data
	for(unsigned int ui=0;ui<ionNames.size() ; ui++)
	{
		f << ionNames[ui].second<< std::endl;
		f << ionNames[ui].first << " " << colours[ui].red << " " << colours[ui].green << " " << colours[ui].blue << std::endl;

	}	

	//Construct the table header
	f<< "-------------";
	for(unsigned int ui=0;ui<ionNames.size() ; ui++)
	{
		f << " " << ionNames[ui].first;
	}

	f << std::endl;
	//Construct the range table
	for(unsigned int ui=0;ui<ranges.size() ; ui++)
	{
		f << ". " << ranges[ui].first << " " << ranges[ui].second ;

		//Now put the "1" in the correct column
		for(unsigned int uj=0;uj<ionNames.size(); uj++)
		{
			if(uj == ionIDs[ui])
				f << " " << 1;
			else
				f << " " << 0;
		}

		f << std::endl;
		
	}

	return 0;
}

unsigned int RangeFile::write(const char *filename) const
{
	std::ofstream f(filename);
	if(!f)
		return 1;

	ASSERT(colours.size() == ionNames.size());

	//File header
	f << ionNames.size() << " " ; 
	f << ranges.size() << std::endl;

	//Colour and longname data
	for(unsigned int ui=0;ui<ionNames.size() ; ui++)
	{
		f << ionNames[ui].second<< std::endl;
		f << ionNames[ui].first << " " << colours[ui].red << " " << colours[ui].green << " " << colours[ui].blue << std::endl;

	}	

	//Construct the table header
	f<< "-------------";
	for(unsigned int ui=0;ui<ionNames.size() ; ui++)
	{
		f << " " << ionNames[ui].first;
	}

	f << std::endl;
	//Construct the range table
	for(unsigned int ui=0;ui<ranges.size() ; ui++)
	{
		f << ". " << ranges[ui].first << " " << ranges[ui].second ;

		//Now put the "1" in the correct column
		for(unsigned int uj=0;uj<ionNames.size(); uj++)
		{
			if(uj == ionIDs[ui])
				f << " " << 1;
			else
				f << " " << 0;
		}

		f << std::endl;
		
	}

	return 0;
}

unsigned int RangeFile::open(const char *rangeFilename, unsigned int fileFormat)
{
	FILE *fpRange;
	fpRange=fopen(rangeFilename,"r");
	if (fpRange== NULL) 
	{
		errState = RANGE_ERR_OPEN;
		return errState;
	}
	
	switch(fileFormat)
	{
		//Oak-Ridge "Format" - this is based purely on example, as no standard exists
		//the classic example is from Miller, "Atom probe: Analysis at the atomic scale"
		//but alternate output forms exist. Our only strategy is to try to be as acoommodating
		//as reasonably possible
		case RANGE_FORMAT_ORNL:
		{
			char inBuffer[256];
			unsigned int tempInt;
			unsigned int assignedFlag;

			unsigned int numRanges;
			unsigned int numIons;	
			
			
			//Read out the number of ions and ranges int hef ile	
			if(fscanf(fpRange, "%64u %64u", &numIons, &numRanges) != 2)
			{
				errState=RANGE_ERR_FORMAT;
				fclose(fpRange);
				return errState;
			}
			
			if (!(numIons && numRanges))
			{
				errState= RANGE_ERR_EMPTY;
				fclose(fpRange);
				return errState;
			}
			
			//Reserve Storage
			ionNames.reserve(numIons);
			colours.reserve(numIons);
			ranges.reserve(numRanges);
			ionIDs.reserve(numRanges);
			
			RGBf colourStruct;
			pair<string, string> namePair;
			
			//Read ion short and full names as well as colour info
			for(unsigned int i=0; i<numIons; i++)
			{
				//Spin until we get to a new line. 
				//Certain programs emit range files that have
				//some string of unknown purpose
				//after the colour specification
				if(fpeek(fpRange)== ' ')
				{
					int peekVal;
					//Gobble chars until we hit the newline
					do
					{
						fgetc(fpRange);
						peekVal=fpeek(fpRange);
					}
					while(peekVal != (int)'\n'&&
					    	peekVal !=(int)'\r' && peekVal != EOF);

					//eat another char if we are using 
					//windows newlines
					if(peekVal== '\r')
						fgetc(fpRange);
				}
				//Read the input for long name (max 256 chars)
				if(!fscanf(fpRange, " %256s", inBuffer))
				{
					errState=RANGE_ERR_FORMAT;
					fclose(fpRange);
					return errState;
				}
					
				namePair.second = inBuffer;


				//Read short name
				if(!fscanf(fpRange, " %256s", inBuffer))
				{
					errState=RANGE_ERR_FORMAT;
					fclose(fpRange);
					return errState;
				}
				
				namePair.first= inBuffer;
				//Read Red green blue data
				if(!fscanf(fpRange,"%128f %128f %128f",&(colourStruct.red),
					&(colourStruct.green),&(colourStruct.blue)))
				{
					errState=RANGE_ERR_FORMAT;
					fclose(fpRange);
					return errState;
				}
				
				ionNames.push_back(namePair);	
				colours.push_back(colourStruct);	
			}	
			
			fgets((char *)inBuffer, 256, fpRange); /* skip over <LF> */
			fgets((char *)inBuffer, 256, fpRange); /* skip the comment line */


			//Load in each range file line
			tempInt=0;
			pair<float,float> massPair;
			unsigned int badRanges=0;	
			for(unsigned int i=0; i<numRanges; i++)
			{
				//read dummy char
				do
				{
					if(!fread(inBuffer, 1 , 1 , fpRange))
					{
						errState=RANGE_ERR_FORMAT;
						fclose(fpRange);
						return errState;
					}

				}
				while(inBuffer[0] != '.');
				
				if(!fscanf(fpRange, " %128f %128f",&massPair.first, 
							&massPair.second))
				{
					errState=RANGE_ERR_FORMAT;
					fclose(fpRange);
					return errState;
				}

				if(massPair.first >= massPair.second)
				{
					errState=RANGE_ERR_DATA;
					fclose(fpRange);
					return errState;
				}

				ranges.push_back(massPair);	
				//Load the range data line
				assignedFlag=0;
				for(unsigned int j=0; j<numIons; j++)
				{
					if(!fscanf(fpRange, "%64u",&tempInt))
					{
						errState=RANGE_ERR_FORMAT;
						fclose(fpRange);
						return errState;
					}
					
					
					if(tempInt == 1)
					{
						assignedFlag=1;
						ionIDs.push_back(j);
					}
				}

				if(!assignedFlag)
				{
					//OK, so that range was useless
					//the range file had a null line. 

					//In a nice world no sane program would ever make
					//a range file like this.
					ranges.pop_back();
					badRanges++;
				}

			}
				

			//Prevent rangefiles that have no valid ranges
			//from being loaded
			if(badRanges >= numRanges)
			{
				errState=RANGE_ERR_DATA;
				fclose(fpRange);
				return errState;

			}

			numRanges-=badRanges;
		
			break;	
		}
		//Cameca "ENV" file format.. I have a couple of examples, but nothing more.
		//There is no specification of this format publicly available
		case RANGE_FORMAT_ENV:
		{
			//Ruoen group "environment file" format
			//This is not a standard file format, so the
			//reader is a best-effort implementation, based upon example
			const unsigned int MAX_LINE_SIZE=4096;	
			char *inBuffer = new char[MAX_LINE_SIZE];
			unsigned int numRanges;
			unsigned int numIons;	

			bool beyondRanges=false;
			bool haveNumRanges=false;	
			bool haveNameBlock=false;	
			vector<string> strVec;

			//Read file until we get beyond the range length
			while(!beyondRanges && !feof(fpRange) )
			{
				fgets((char *)inBuffer, MAX_LINE_SIZE, fpRange);
				//Trick to "strip" the buffer
				nullifyMarker(inBuffer,'#');

				string s;
				s=inBuffer;

				s=stripWhite(s);

				if(!s.size())
					continue;

				//Try different delimiters to split string
				splitStrsRef(s.c_str(),"\t ",strVec);
			
				stripZeroEntries(strVec);

				if(!strVec.size())
					continue;

				if(!haveNumRanges)
				{
					//num ranges should have two entries, num ions and ranges
					if(strVec.size() != 2)
					{
						fclose(fpRange);
						delete[] inBuffer;
						return RANGE_ERR_FORMAT;
					}

					stream_cast(numIons,strVec[0]);
					stream_cast(numRanges,strVec[1]);

					haveNumRanges=true;
				}
				else
				{
					//Do we still have to process the name block?
					if(!haveNameBlock)
					{
						//Just exiting name block,
						if(strVec.size() == 5)
							haveNameBlock=true;
						else if(strVec.size() == 4)
						{
							//Entry in name block
							if(!strVec[0].size())
							{
								delete[] inBuffer;
								fclose(fpRange);
								return RANGE_ERR_FORMAT;
							}

							//Check that name consists only of 
							//readable ascii chars, or period
							for(unsigned int ui=0; ui<strVec[0].size(); ui++)
							{
								if(!isdigit(strVec[0][ui]) &&
									!isalpha(strVec[0][ui]) && strVec[0][ui] != '.')
								{
									fclose(fpRange);
									delete[] inBuffer;
									return RANGE_ERR_FORMAT;
								}
							}
							//Env file contians only the long name, so use that for both the short and long names
							ionNames.push_back(std::make_pair(strVec[0],strVec[0]));
							//Use the colours (positions 1-3)
							RGBf colourStruct;
							if(stream_cast(colourStruct.red,strVec[1])
								||stream_cast(colourStruct.green,strVec[2])
							 	||stream_cast(colourStruct.blue,strVec[3]) )
							{
								fclose(fpRange);
								delete[] inBuffer;
								return RANGE_ERR_FORMAT;

							}

							if(colourStruct.red >1.0 || colourStruct.red < 0.0f ||
							   colourStruct.green >1.0 || colourStruct.green < 0.0f ||
							   colourStruct.blue >1.0 || colourStruct.blue < 0.0f )
							{
								fclose(fpRange);
								delete[] inBuffer;
								return RANGE_ERR_FORMAT;

							}
							
									
							colours.push_back(colourStruct);
						}
						else 
						{
							//Well thats not right,
							//we should be looking at the first entry in the
							//range block....
							fclose(fpRange);
							delete[] inBuffer;
							return RANGE_ERR_FORMAT;
						}
					}

					if(haveNameBlock)
					{
						//We are finished
						if(strVec.size() == 5)
						{
							unsigned int thisIonID;
							thisIonID=(unsigned int)-1;
							for(unsigned int ui=0;ui<ionNames.size();ui++)
							{
								if(strVec[0] == ionNames[ui].first)
								{
									thisIonID=ui;
									break;
								}

							}
						
							if(thisIonID==(unsigned int) -1)
							{
								fclose(fpRange);
								delete[] inBuffer;
								return RANGE_ERR_FORMAT;
							}

							float rangeStart,rangeEnd;
							if(stream_cast(rangeStart,strVec[1]))
							{
								fclose(fpRange);
								delete[] inBuffer;
								return RANGE_ERR_FORMAT;
							}
							if(stream_cast(rangeEnd,strVec[2]))
							{
								fclose(fpRange);
								delete[] inBuffer;
								return RANGE_ERR_FORMAT;
							}

							ranges.push_back(std::make_pair(rangeStart,rangeEnd));

							ionIDs.push_back(thisIonID);
						}
						else
							beyondRanges=true;

					}

				}
			
				
			}	

			if(feof(fpRange))
			{
				fclose(fpRange);
				delete[] inBuffer;
				return RANGE_ERR_FORMAT;
			}	
		
			delete[] inBuffer;
			break;	
		}
		//Imago (now Cameca), "RRNG" file format. Again, neither standard 
		//nor formal specification exists (sigh). Purely implemented by example.
		case RANGE_FORMAT_RRNG:
		{
			const unsigned int MAX_LINE_SIZE=4096;
			char *inBuffer = new char[MAX_LINE_SIZE];
			unsigned int numRanges;
			unsigned int numBasicIons;


			//Different "blocks" that can be read
			enum {
				BLOCK_NONE,
				BLOCK_IONS,
				BLOCK_RANGES,
			};

			unsigned int curBlock=BLOCK_NONE;
			bool haveSeenIonBlock;
			haveSeenIonBlock=false;
			numRanges=0;
			numBasicIons=0;
			vector<string> basicIonNames;

			while (!feof(fpRange))
			{
				fgets((char *)inBuffer, MAX_LINE_SIZE, fpRange);
				//Trick to "strip" the buffer, assuming # is a comment
				nullifyMarker(inBuffer,'#');

				string s;
				s=inBuffer;

				s=stripWhite(s);
				if (!s.size())
					continue;

				if (s == "[Ions]")
				{
					curBlock=BLOCK_IONS;
					continue;
				}
				else if (s == "[Ranges]")
				{
					curBlock=BLOCK_RANGES;
					continue;
				}

				switch (curBlock)
				{
				case BLOCK_NONE:
					break;
				case BLOCK_IONS:
				{

					//The ion section in RRNG seems almost completely redundant.
					//This does not actually contain information about the ions
					//in the file (this is in the ranges section).
					//Rather it tells you what the constituents are from which
					//complex ions may be formed. Apply Palm to Face.

					vector<string> split;
					splitStrsRef(s.c_str(),'=',split);

					if (split.size() != 2)
					{
						delete[] inBuffer;
						fclose(fpRange);
						return RANGE_ERR_FORMAT;
					}

					std::string stmp;
					stmp=lowercase(split[0]);

					haveSeenIonBlock=true;


					if (stmp == "number")
					{
						//Should not have set the
						//number of ions yet.
						if (numBasicIons !=0)
						{
							delete[] inBuffer;
							fclose(fpRange);
							return RANGE_ERR_FORMAT;
						}
						//Set the number of ions
						stream_cast(numBasicIons, split[1]);

						if (numBasicIons == 0)
						{
							delete[] inBuffer;
							fclose(fpRange);
							return RANGE_ERR_FORMAT;
						}
					}
					else if (split[0].size() >3)
					{
						stmp = lowercase(split[0].substr(0,3));
						if (stmp == "ion")
						{
							//OK, its an ion.
							basicIonNames.push_back(split[1]);

							if (basicIonNames.size()  > numBasicIons)
						{
							delete[] inBuffer;
							fclose(fpRange);
							return RANGE_ERR_FORMAT;
						}
						}
						else
						{
							delete[] inBuffer;
							fclose(fpRange);
							return RANGE_ERR_FORMAT;
						}
					}
					break;
				}
				case BLOCK_RANGES:
				{

					//Altough it looks like the blocks are independant.
					//it is more complicated to juggle a parser with them
					//out of dependency order, as  a second pass would
					//need to be done.
					if (!haveSeenIonBlock)
					{
						delete[] inBuffer;
						fclose(fpRange);
						return RANGE_ERR_FORMAT;
					}
					vector<string> split;

					if (s.size() > 6)
					{
						splitStrsRef(s.c_str(),'=',split);

						if (split.size() != 2)
						{
							delete[] inBuffer;
							fclose(fpRange);
							return RANGE_ERR_FORMAT;
						}

						if (lowercase(split[0].substr(0,5)) == "numbe")
						{

							//Should not have set the num ranges yet
							if (numRanges)
							{
								delete[] inBuffer;
								fclose(fpRange);
								return RANGE_ERR_FORMAT;
							}

							if (stream_cast(numRanges,split[1]))
							{
							
								delete[] inBuffer;
								fclose(fpRange);
								return RANGE_ERR_FORMAT;
							}

							if (!numRanges)
							{
								delete[] inBuffer;
								fclose(fpRange);
								return RANGE_ERR_FORMAT;
							}

						}
						else if ( lowercase(split[0].substr(0,5)) == "range")
						{
							//Try to decode a range line, as best we can.
							//These appear to come in a variety of flavours,
							//which makes this section quite long.

							//OK, so the format here is a bit wierd
							//we first have to strip the = bit
							//then we have to step across fields,
							//I assume the fields are in order (or missing)
							//* denotes 0 or more,  + denotes 1 or more, brackets denote grouping
							// Vol: [0-9]* ([A-z]+: [0-9]+)* (Name:[0-9]*([A-z]+[0-9]*)+ Color:[0-F][0-F][0-F][0-F][0-F][0-F]
							// Examples:
							// Range1=31.8372 32.2963 Vol:0.01521 Zn:1 Color:999999
							// or
							// Range1=31.8372 32.2963 Zn:1 Color:999999
							// or
							// Range1=95.3100 95.5800 Vol:0.04542 Zn:1 Sb:1 Color:00FFFF
							// or
							// Range1=95.3100 95.5800 Vol:0.04542 Name:1Zn1Sb1 Color:00FFFF
							// or
							// Range1=95.3100 95.5800 Vol:0.04542 Zn:1 Sb:1 Name:1Zn1Sb1 Color:00FFFF

							//Starting positions (string index)
							//of range start and end
							//Range1=31.8372 32.2963 Vol:0.01521 Zn:1 Color:999999
							//	 	^rngmid ^rngend
							size_t rngMidIdx,rngEndIdx;
							string rngStart,rngEnd;
							rngMidIdx = split[1].find_first_of(' ');

							if (rngMidIdx == std::string::npos)
							{
								delete[] inBuffer;
								fclose(fpRange);
								return RANGE_ERR_FORMAT;
							}

							rngEndIdx=split[1].find_first_of(' ',rngMidIdx+1);
							if (rngEndIdx == std::string::npos)
							{
								delete[] inBuffer;
								fclose(fpRange);
								return RANGE_ERR_FORMAT;
							}

							rngStart = split[1].substr(0,rngMidIdx);
							rngEnd = split[1].substr(rngMidIdx+1,rngEndIdx-(rngMidIdx+1));


							//Strip the range
							string strTmp;
							strTmp = split[1].substr(rngEndIdx+1);//,split[1].size()-(rngEndIdx+1));

							//Split the remaining field into key:value pairs
							split.clear();
							splitStrsRef(strTmp.c_str(),' ',split);

							RGBf col;
							bool haveColour,haveNameField;
							haveColour=false;
							haveNameField=false;
							string strIonNameTmp,strNameFieldValue;

							for (unsigned int ui=0; ui<split.size(); ui++)
							{
								//':' is the key:value delimiter (Self reference is hilarious.)
								size_t colonPos;


								colonPos = split[ui].find_first_of(':');

								if (colonPos == std::string::npos)
								{
									delete[] inBuffer;
									fclose(fpRange);
									return RANGE_ERR_FORMAT;
								}

								//Extract the key value pairs
								string key,value;
								key=split[ui].substr(0,colonPos);
								value=split[ui].substr(colonPos+1);
								if (lowercase(key) == "vol")
								{
									//Do nothing
								}
								else if (lowercase(key) == "name")
								{
									//OK, so we need to handle two wierd cases
									//1) we have the name section and the ion section
									//2) we have the name section and no ion section

									//Rant:WTF is the point of the "name" section. They are 
									//encoding information in a redundant and irksome fashion!
									//Better to store the charge (if thats what you wnat) in a F-ing charge
									//section, and the multiplicity is already bloody defined elsewhere!
									
									//This won't be known until we complete the parse
									haveNameField=true;
									strNameFieldValue=value;
								}
								else if (lowercase(key) == "color")
								{
									haveColour=true;


									if (value.size() !=6)
									{
										delete[] inBuffer;
										fclose(fpRange);
										return RANGE_ERR_FORMAT;
									}

									//Use our custom string parser,
									//which requires a leading #,
									//in lowercase
									value = string("#") + lowercase(value);
									unsigned char r,g,b,a;

									parseColString(value,r,g,b,a);

									col.red = (float)r/255.0f;
									col.green=(float)g/255.0f;
									col.blue=(float)b/255.0f;
								}
								else
								{
									//Basic ion was not in the original [Ions] list. This is not right
									unsigned int pos=(unsigned int)-1;
									for (unsigned int uj=0; uj<basicIonNames.size(); uj++)
									{
										if (basicIonNames[uj] == key)
										{
											pos = uj;
											break;
										}
									}

									if (pos == (unsigned int)-1)
									{
										delete[] inBuffer;
										fclose(fpRange);
										return RANGE_ERR_FORMAT;
									}
									//Check the multiplicty of the ion. should be an integer > 0
									unsigned int uintVal;
									if (stream_cast(uintVal,value) || !uintVal)
									{
										delete[] inBuffer;
										fclose(fpRange);
										return RANGE_ERR_FORMAT;
									}

									//If it is  1, then use straight name. Otherwise try to give it a
									//chemical formula look by mixing in the multiplicty after the key
									if (uintVal==1)
										strIonNameTmp+=key;
									else
										strIonNameTmp+=key+value;
								}
							}

							if (!haveColour )
							{
								delete[] inBuffer;
								fclose(fpRange);
								return RANGE_ERR_FORMAT;
							}

							//Get the range values
							float rngStartV,rngEndV;
							if(strIonNameTmp.size() || haveNameField)
							{
								if (stream_cast(rngStartV,rngStart))
								{
									delete[] inBuffer;
									fclose(fpRange);
									return RANGE_ERR_FORMAT;
								}

								if (stream_cast(rngEndV,rngEnd))
								{
									delete[] inBuffer;
									fclose(fpRange);
									return RANGE_ERR_FORMAT;
								}
							}

							//So the ion field appears to be optional (that is,
							//ivas emits RRNGs that do not contain an ion listed in the 
							//ion field). It is unclear what the purpose of these lines is,
							//so we shall ignore it, in preference to aborting
							if(strIonNameTmp.size())
							{
								//Check to see if we have this ion.
								//If we dont, we create a new one.
								//if we do, we check that the colours match
								//and if not reject the file parsing.
								unsigned int pos=(unsigned int)(-1);
								for (unsigned int ui=0; ui<ionNames.size(); ui++)
								{
									if (ionNames[ui].first == strIonNameTmp)
									{
										pos=ui;
										break;
									}
								}

								ranges.push_back(std::make_pair(rngStartV,rngEndV));
								if (pos == (unsigned int) -1)
								{
									//This is new. Create a new ion,
									//and use its ionID
									ionNames.push_back(std::make_pair(strIonNameTmp,
												     strIonNameTmp));
									colours.push_back(col);
									ionIDs.push_back(ionNames.size()-1);
								}
								else
								{
									//we have the name already
									ionIDs.push_back(pos);
								}
							}
							else if(haveNameField)
							{
								//OK, so we don't have the ion section
								//but we do have the field section
								//let us decode the name field in order to retrieve 
								//the ion information which was missing (for whatever reason)
								if(!strNameFieldValue.size())
								{
									delete[] inBuffer;
									fclose(fpRange);
									return RANGE_ERR_FORMAT;
								}


								unsigned int chargeStrStop=0;
								for(unsigned int ui=0;ui<strNameFieldValue.size();ui++)
								{
									if(strNameFieldValue[ui] <'0' || strNameFieldValue[ui] > '9')
									{
										chargeStrStop =ui;
										break;
									}

								}

								//Strip off the charge value, to get the ion name
								strNameFieldValue =strNameFieldValue.substr(chargeStrStop,strNameFieldValue.size()-chargeStrStop);

								//Check to see if we have this ion.
								//If we dont, we create a new one.
								//if we do, we check that the colours match
								//and if not reject the file parsing.
								unsigned int pos=(unsigned int)(-1);
								for (unsigned int ui=0; ui<ionNames.size(); ui++)
								{
									if (ionNames[ui].first == strNameFieldValue)
									{
										pos=ui;
										break;
									}
								}

								ranges.push_back(std::make_pair(rngStartV,rngEndV));
								if (pos == (unsigned int) -1)
								{
									//This is new. Create a new ion,
									//and use its ionID
									ionNames.push_back(std::make_pair(strNameFieldValue,
												     strNameFieldValue));
									colours.push_back(col);
									ionIDs.push_back(ionNames.size()-1);
								}
								else
								{
									//we have the name already
									ionIDs.push_back(pos);
								}

							}
						}
						else
						{
							delete[] inBuffer;
							fclose(fpRange);
							return RANGE_ERR_FORMAT;
						}

					}
					break;
				}
				default:
					ASSERT(false);
					break;
				}
			}

			delete[] inBuffer;
			break;
		}
		default:
			ASSERT(false);
			fclose(fpRange);
			return RANGE_ERR_FORMAT;
	}


	
	fclose(fpRange);
	
	
	if(!isSelfConsistent())
	{
		//Failed self consistency check
		return RANGE_ERR_DATA;
	}
	return 0;
}


bool RangeFile::isSelfConsistent() const
{
	//Check that ranges don't overlap.
	for(unsigned int ui=0;ui<ranges.size();ui++)
	{
		for(unsigned int uj=0; uj<ranges.size();uj++)
		{
			if(ui==uj)
				continue;

			//check that not sitting inside a range
			if(ranges[ui].first > ranges[uj].first &&
					ranges[ui].first < ranges[uj].second )
				return false;
			if(ranges[ui].second > ranges[uj].first &&
					ranges[ui].second < ranges[uj].second )
				return false;

			//check for not spanning a range
			if(ranges[ui].first < ranges[uj].first &&
					ranges[ui].second > ranges[uj].second)
				return false;

		}
	}

	return true;
}

bool RangeFile::isRanged(float mass) const
{
	unsigned int numRanges = ranges.size();
	
	for(unsigned int ui=0; ui<numRanges; ui++)
	{
		if( mass >= ranges[ui].first  &&
				mass <= ranges[ui].second )
			return true;
	}
	
	return false;
}

bool RangeFile::isRanged(const IonHit &ion) const
{
	return isRanged(ion.getMassToCharge());
}

bool RangeFile::range(vector<IonHit> &ions, string ionShortName) 
{
	vector<IonHit> rangedVec;
	
	//find the Ion ID of what we want
	unsigned int targetIonID=(unsigned int)-1;
	for(unsigned int ui=0; ui<ionNames.size(); ui++)
	{
		if(ionNames[ui].first == ionShortName)
		{
			targetIonID = ui;
			break;
		}
	}

	if(targetIonID == (unsigned int)(-1))
		return false;

	//find the ranges that have that ionID
	vector<unsigned int> subRanges;
	for(unsigned int ui=0; ui<ionIDs.size(); ui++)
	{
		if(ionIDs[ui] == targetIonID)
			subRanges.push_back(ui);
	}
	
	unsigned int numIons=ions.size();
	rangedVec.reserve(numIons);

	unsigned int numSubRanges = subRanges.size();
	
	for(unsigned int ui=0; ui<numIons; ui++)
	{
		for(unsigned int uj=0; uj<numSubRanges; uj++)
		{
			if( ions[ui].getMassToCharge() >= ranges[subRanges[uj]].first  &&
					ions[ui].getMassToCharge() <= ranges[subRanges[uj]].second )
			{
				rangedVec.push_back(ions[ui]);
				break;
			}
		}
	}

	//Do the switcheroonie
	//such that the un-ranged ions are destroyed
	//and the ranged ions are kept 
	ions.swap(rangedVec);
	return true;
}

void RangeFile::range(vector<IonHit> &ions)
{
	vector<IonHit> rangedVec;

	unsigned int numIons=ions.size();
	rangedVec.reserve(numIons);

	for(unsigned int ui=0; ui<numIons; ui++)
	{
		if(isRanged(ions[ui]))
			rangedVec.push_back(ions[ui]);
	}

	//Do the switcheroonie
	//such that the un-ranged ions are destroyed
	//and the ranged ions are kept 
	ions.swap(rangedVec);
}

void RangeFile::rangeByRangeID(vector<IonHit> &ions, unsigned int rangeID)
{
	vector<IonHit> rangedVec;
	
	unsigned int numIons=ions.size();
	rangedVec.reserve(numIons);
	
	for(unsigned int ui=0; ui<numIons; ui++)
	{
		if( ions[ui].getMassToCharge() >= ranges[rangeID].first  &&
		   ions[ui].getMassToCharge() <= ranges[rangeID].second )
		{
			rangedVec.push_back(ions[ui]);
			break;
		}
	}
	
	//Do the switcheroonie
	//such that the un-ranged ions are destroyed
	//and the ranged ions are kept 
	ions.swap(rangedVec);
}


void RangeFile::printErr(std::ostream &strm)
{	
	const char *errString=0;
		
	switch(errState)
	{
		case RANGE_ERR_OPEN:
			errString="Error opening file, check name and permissions";
			break;
		case RANGE_ERR_FORMAT:
			errString="Error reading file, unexpected format, are you sure it is a proper range file?";
			break;
		case RANGE_ERR_EMPTY:
			errString = "Range file appears to be empty, check is proper range file and non empty";
			break;
		case RANGE_ERR_DATA:
			errString = "Range file appears to contain malformed data, check things like start and ends of m/c are not flipped";	
			break;
		default:
			ASSERT(0);
			errString="Unknown error! -- possible bug";
	}
	
	strm << errString << std::endl;
}

unsigned int RangeFile::getNumRanges() const
{
	return ranges.size();
}

unsigned int RangeFile::getNumRanges(unsigned int ionID) const
{
	unsigned int res=0;
	for(unsigned int ui=0;ui<ranges.size();ui++)
	{
		if( getIonID(ui) == ionID)
			res++;
	}

	return res;
}

unsigned int RangeFile::getNumIons() const
{
	return ionNames.size();
}

pair<float,float> RangeFile::getRange(unsigned int ui) const
{
	return ranges[ui];
}

RGBf RangeFile::getColour(unsigned int ui) const
{
	ASSERT(ui <  colours.size());
	return colours[ui];
}

unsigned int RangeFile::getIonID(float mass) const
{
	unsigned int numRanges = ranges.size();
	
	for(unsigned int ui=0; ui<numRanges; ui++)
	{
		if( mass >= ranges[ui].first  &&
				mass <= ranges[ui].second )
			return ionIDs[ui];
	}
	
	return (unsigned int)-1;
}

unsigned int RangeFile::getRangeID(float mass) const
{
	unsigned int numRanges = ranges.size();
	
	for(unsigned int ui=0; ui<numRanges; ui++)
	{
		if( mass >= ranges[ui].first  &&
				mass <= ranges[ui].second )
			return ui;
	}
	
	return (unsigned int)-1;
}

unsigned int RangeFile::getIonID(unsigned int range) const
{
	ASSERT(range < ranges.size());

	return ionIDs[range];
}

unsigned int RangeFile::getIonID(const char *name) const
{
	for(unsigned int ui=0; ui<ionNames.size(); ui++)
	{
		if(ionNames[ui].first == name)
			return ui;
	}

	return (unsigned int)-1;	
}

std::string RangeFile::getName(unsigned int ionID,bool shortName) const
{
	ASSERT(ionID < ionNames.size());
	if(shortName)
		return ionNames[ionID].first;
	else
		return ionNames[ionID].second;
}	

std::string RangeFile::getName(const IonHit &ion, bool shortName) const
{
	ASSERT(isRanged(ion));
	
	if(shortName)
		return ionNames[getIonID(ion.getMassToCharge())].first;
	else
		return ionNames[getIonID(ion.getMassToCharge())].second;
}

bool RangeFile::rangeByID(vector<IonHit> &ionHits, unsigned int rng)
{
	//This is a bit slack, could be faster, but should work.
	return range(ionHits,ionNames[rng].first);
}

bool RangeFile::isRanged(string shortName, bool caseSensitive)
{
	if(caseSensitive)
	{
		for(unsigned int ui=ionNames.size(); ui--; )
		{
			if(ionNames[ui].first == shortName)
				return true;
		}
	}
	else
	{
		for(unsigned int ui=ionNames.size(); ui--; )
		{
			//perform a series of case independent 
			//string comparisons 
			string str;
			str = ionNames[ui].first;

			if(str.size() !=shortName.size())
				continue;
			
			bool next;
			next=false;
			for(unsigned int ui=str.size(); ui--; )
			{
				if(tolower(str[ui]) != 
					tolower(shortName[ui]))
				{
					next=true;
					break;
				}
			}
			
			if(!next)
				return true;
		}
	}

	return false;
}

unsigned int RangeFile::atomicNumberFromRange(unsigned int range) const
{
	if(range > ranges.size())
		return 0;

	string str= getName(getIonID(range));

	for(unsigned int ui=0; ui<NUM_ELEMENTS; ui++)
	{
		if(str ==  string(cpAtomNaming[ui][0])
				|| str == string(cpAtomNaming[ui][1]))
			return ui+1;
	}

	return 0;	
}

unsigned int RangeFile::atomicNumberFromIonID(unsigned int ionID) const
{
	if(ionID > ionIDs.size())
		return 0;

	string str= getName(ionID);

	for(unsigned int ui=0; ui<NUM_ELEMENTS; ui++)
	{
		if(str ==  string(cpAtomNaming[ui][0])
				|| str == string(cpAtomNaming[ui][1]))
			return ui+1;
	}

	return 0;	
}

void RangeFile::setColour(unsigned int id, const RGBf &r) 
{
	ASSERT(id < colours.size());
	colours[id] = r;
}


void RangeFile::setIonShortName(unsigned int id,  const std::string &newName)
{
	ionNames[id].first=newName;
}

void RangeFile::setIonLongName(unsigned int id, const std::string &newName)
{
	ionNames[id].second = newName;
}

void  RangeFile::swap(RangeFile &r)
{
	using std::swap;
	swap(ionNames,r.ionNames);
	swap(colours,r.colours);
	swap(ranges,r.ranges);
	swap(ionIDs,r.ionIDs);
	swap(errState,r.errState);

}

bool RangeFile::moveRange(unsigned int rangeId, bool limit, float newMass)
{

	//Check for moving past other part of range -- "inversion"
	if(limit)
	{
		//Move upper range
		if(newMass <= ranges[rangeId].first)
			return false;
	}
	else
	{
		if(newMass >= ranges[rangeId].second)
			return false;
	}

	//Check that moving this range will not cause any overlaps with 
	//other ranges
	for(unsigned int ui=0; ui<ranges.size(); ui++)
	{
		if( ui == rangeId)
			continue;

		if(limit)
		{
			//moving high range
			//check for overlap on first
			if((ranges[rangeId].first < ranges[ui].first &&
					newMass > ranges[ui].first))
			       return false;
			
			if((ranges[rangeId].first < ranges[ui].second &&
					newMass > ranges[ui].second))
			       return false;
		}
		else
		{
			//moving low range
			//check for overlap on first
			if((ranges[rangeId].second > ranges[ui].first &&
					newMass < ranges[ui].first))
			       return false;
			
			if((ranges[rangeId].second > ranges[ui].second &&
					newMass < ranges[ui].second))
			       return false;
		}

	}

	if(limit)
		ranges[rangeId].second = newMass;
	else
		ranges[rangeId].first= newMass;

	return true;
}

bool RangeFile::moveBothRanges(unsigned int rangeId, float newLow, float newHigh)
{

	//Check that moving this range will not cause any overlaps with 
	//other ranges
	for(unsigned int ui=0; ui<ranges.size(); ui++)
	{
		if( ui == rangeId)
			continue;

		//moving high range
		//check for overlap on first
		if((ranges[rangeId].first < ranges[ui].first &&
				newHigh > ranges[ui].first))
		       return false;
		
		if((ranges[rangeId].first < ranges[ui].second &&
				newHigh > ranges[ui].second))
		       return false;
		//moving low range
		//check for overlap on first
		if((ranges[rangeId].second > ranges[ui].first &&
				newLow < ranges[ui].first))
		       return false;
		
		if((ranges[rangeId].second > ranges[ui].second &&
				newLow < ranges[ui].second))
		       return false;
	}

	ranges[rangeId].second = newHigh;
	ranges[rangeId].first= newLow;

	return true;
}


void RangeFile::setIonID(unsigned int range, unsigned int newIonId)
{
	ASSERT(newIonId < ionIDs.size());
	ionIDs[range] = newIonId;
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
