/*
 * APTClasses.h - Generic APT components header 
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

#ifndef APTCLASSES_H
#define APTCLASSES_H
#include "basics.h"
#include "endianTest.h"
#include "mathfuncs.h"
#include "common.h"

#include <string>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <utility>
#include <cmath>
#include <cstring>//memcpy
#include <fstream>
#include <new>//std::bad_alloc


using std::vector;

class IonHit;
class Point3D;
class RangeFile;


const unsigned int PROGRESS_REDUCE=5000;

extern const char *posErrStrings[];

//!Errors that can be encountered when openning pos files
enum posErrors
{
	POS_ALLOC_FAIL=1,
	POS_OPEN_FAIL,
	POS_EMPTY_FAIL,
	POS_SIZE_MODULUS_ERR,	
	POS_READ_FAIL,
	POS_NAN_LOAD_ERROR,
	POS_ABORT_FAIL,
	POS_ERR_FINAL // Not actually an error, but tells us where the end of the num is.
};

#ifdef __LITTLE_ENDIAN__
	//swaps bytes
	void floatSwapBytes(float *inFloat);
#endif

#ifdef DEBUG
	//Dump point in form (x,y,z) to stderr
	void DumpPoint(const Point3D &pt);

	//!Make a pos file from a set of points, assigning them a mass of "mass"
	void makePos(const vector<Point3D> &points, float mass, const char *name);
#endif

//!make a pos file from a set of a set of IonHits
void makePos(const vector<IonHit> &points, const char *name);

//!make/append to a pos file from a set of a set of IonHits
void appendPos(const vector<IonHit> &points, const char *name);

//!Print  short summmary of the ions in a list
void PrintSummary(std::vector<IonHit> &posIons, RangeFile &rangeFile);


//!Returns the limits of a dataset of Ions
void dataLimits(vector<IonHit> &posIons, Point3D &low, Point3D &upper);




//! Returns the shortest distance between a facet and a Point 
/* The inputs are the facet points (ABC) and the point P.
 * distance is shortest using standard plane version 
 * \f$ D = \vec{AB} \cdot \vec{n} \f$ 
 * iff dot products to each combination of \f$ \left( AP,BP,CP \right) \leq 0 \f$
 * otherwise closest point is on the boundary of the simplex.
 * tested by shortest distance to each line segment (E is shortest pt. AB is line segement)
 * \f$ \vec{E} = \frac{\vec{AB}}{|\vec{AB}|} 
 *  ( \vec{PB} \cdot \vec{AB})\f$
 */
float distanceToFacet(const Point3D &fA, const Point3D &fB, 
			const Point3D &fC, const Point3D &p, const Point3D &normal);


//! Returns the shortest distance between a line segment and a given point
/* The inpus are the ends of the line segment and the point. Uses the formula that 
 * \f$ 
 * D = \abs{\vec{PE}}\f$
 * \f[ 
 * \mathrm{~if~} \vec{PA} \cdot \vec{AB} > 0 
 * \rightarrow \vec{PE} = \vec{A} 
 * \f]
 * \f[
 * \mathrm{~if~} \vec{AB} \cdot \vec{PB} > 0 ~\&~ \vec{PA} \cdot \vec{AB} < 0
 * \rightarrow \vec{PB} \cdot \frac{\vec{AB}}{\abs{\vec{AB}}} 
 * \f]
 * \f[
 * \mathrm{~if~} \vec{PB} \cdot \vec{AB} < 0 
 * \rightarrow \vec{B} 
 * \f]
 */
float distanceToSegment(const Point3D &fA, const Point3D &fB, const Point3D &p);


//!Check which way vectors attached to two 3D points "point",  - "together", "apart" or "in common"
// 
unsigned int vectorPointDir(const Point3D &fA, const Point3D &fB, 
				const Point3D &vA, const Point3D &vB);


void DumpIonInfo(const std::vector<IonHit> &vec, 
				std::ostream &strm);

//!Return the minimum of 2 objects(inline)
template<class T> inline T min2(T T1, T T2)
{
	if(T1 < T2)
		return T1;
	else
		return T2;
}



//!Return the minimum of 3 objects (inline)
template<class T> inline T min3(T T1,T T2,T T3)
{
	//check Tor T1 smallest
	if(T1 < T2 && T1 < T3)
		return T1;

	//either T2 or T3 is smallest
	if(T2 < T3)
		return T2;
	else
		return T3;
}

//Range file strucutres
//=========
//!Data holder for colour as float
typedef struct RGB
{
	float red;
	float green;
	float blue;
} RGB;

enum{ RANGE_ERR_OPEN =1, 
	RANGE_ERR_FORMAT,
	RANGE_ERR_EMPTY,
	RANGE_ERR_NOASSIGN,
	RANGE_ERR_DATA
	};

enum{ RANGE_FORMAT_ORNL=1,
	RANGE_FORMAT_ENV,
	RANGE_FORMAT_RRNG,
	RANGE_FORMAT_END_OF_ENUM //not a format, just end of enumueration.
};
//=========




//Pos file structure
//===========
//!Record as stored in a .POS file
typedef struct IONHIT
{
	float pos[3];
	float massToCharge;
} IONHIT;
//=========

enum PointDir{ 	POINTDIR_TOGETHER =0,
                POINTDIR_IN_COMMON,
                POINTDIR_APART
             };
             
//!Create an pos file from a vector of IonHits
unsigned int IonVectorToPos(const std::vector<IonHit> &ionVec, 
					const std::string &filename);
             
//!Create a "quality"-metric pos file,
/*! The generated pos file is from  replacing mass to charge with the 
 * chosen quality tally based  metric ie quality = good_counts/bad_counts. 
 */
bool createQualityPos(const std::vector<Point3D> &p, 
	const std::vector<std::pair<unsigned int, unsigned int> > &q, 
						const std::string &str);

//!This is a data holding class for POS file ions, from
/* Pos ions are typically obtained via reconstructed apt detector hits
 * and are of form (x,y,z mass/charge)
 */
class IonHit
{
	private:
		float massToCharge; // mass to charge ratio in Atomic Mass Units per (charge on electron)
		Point3D pos; //position (xyz) in nm
	public:
		IonHit();
		//copy constructor
		IonHit(const IonHit &);
		IonHit(Point3D p, float massToCharge);

		void setHit(const IONHIT *hit);
		void setMassToCharge(float newMassToCharge);
		void setPos(const Point3D &pos);
		void setPos(float fX, float fY, float fZ)
			{ pos.setValue(fX,fY,fZ);};
		Point3D getPos() const;
		inline const Point3D &getPosRef() const {return pos;};
		//returns true if any of the 4 data pts are NaN
		bool hasNaN();

#ifdef __LITTLE_ENDIAN__		
		void switchEndian();
#endif
		//this does the endian switch for you
		//but you must supply a valid array.
		void makePosData(float *floatArr) const;
		float getMassToCharge() const;
		IonHit operator=(const IonHit &obj);
		float operator[](unsigned int ui) const;	
		IonHit operator+(const Point3D &obj);
};	


//!Data storage and retrieval class for .rng files
class RangeFile
{
	private:
		//These vectors will contain the number of ions
		
		//The first element is the shortname for the Ion
		//the second is the full name
		std::vector<std::pair<std::string,std::string> > ionNames;
		//This holds the colours for the ions
		std::vector<RGB> colours;
		
		//This will contains the number of ranges
		//
		//This holds the min and max masses for the range
		std::vector<std::pair<float,float> > ranges;
		//The ion ID number for each range
		std::vector<unsigned int> ionIDs;
		unsigned int errState;

		//!Performs limited checks for self consistency.
		bool isSelfConsistent() const;
	public:
		RangeFile();
		//!Open a specified range file
		unsigned int open(const char *rangeFile, unsigned int format=RANGE_FORMAT_ORNL);	
		//!Print the error associated with the current range file state
		void printErr(std::ostream &strm);
		//!Get the number of unique ranges
		unsigned int getNumRanges() const;
		//!Get the number of ranges for a given ion ID
		unsigned int getNumRanges(unsigned int ionID) const;
		//!Get the number of unique ions
		unsigned int getNumIons() const;
		//!Retrieve the start and end of a given range as a pair(start,end)
		std::pair<float,float> getRange(unsigned int ) const;
		//!Retrieve a given colour from the ion ID
		RGB getColour(unsigned int) const;
		//!Set the colour using the ion ID
		void setColour(unsigned int, const RGB &r);

		
		//!Retrieve the colour from a given ion ID

		//!Get the ion's ID from a specified mass
		/*! Returns the ions ID if there exists a range that 
		 * contains this mass. Otherwise (unsigned int)-1 is returned
		 */
		unsigned int getIonID(float mass) const;
		//!Get the ion ID from a given range ID
		/*!No validation checks are performed outside debug mode. Ion
		 range *must* exist*/
		unsigned int getIonID(unsigned int range) const;
		//!Get the ion ID from its short name
		unsigned int getIonID(const char *name) const;	
		
		//!Set the ion ID for a given range
		void setIonID(unsigned int range, unsigned int newIonId);

		//!returns true if a specified mass is ranged
		bool isRanged(float mass) const;
		//! Returns true if an ion is ranged
		bool isRanged(const IonHit &) const;
		//!Clips out ions that are not inside the range
		void range(std::vector<IonHit> &ionHits);
		//!Clips out ions that dont match the specified ion name
		/*! Returns false if the ion name given doesn't match
		 *  any in the rangefile (case sensitive) 
		 */	
		bool range(std::vector<IonHit> &ionHits,
				std::string shortIonName);

		//!Clips out ions that dont lie in the specified range number 
		/*! Returns false if the range does not exist 
		 *  any in the rangefile (case sensitive) 
		 */	
		bool rangeByID(std::vector<IonHit> &ionHits,
				unsigned int range);
		//!Get the short name or long name of a speicifed ionID
		/*! Pass shortname=false to retireve the long name 
		 * ionID passed in must exist. No checking outside debug mode
		 */
		std::string getName(unsigned int ionID,bool shortName=true) const;

		std::string getName(const IonHit &ion, bool shortName) const;

		//!set the short name for a given ion	
		void setIonShortName(unsigned int ionID, const std::string &newName);

		//!Set the long name for a given ion
		void setIonLongName(unsigned int ionID, const std::string &newName);

		//!Check to see if an atom is ranged
		/*! Returns true if rangefile holds at least one range with shortname
		 * corresponding input value. Case sensitivite search is default
		 */
		bool isRanged(std::string shortName, bool caseSensitive=true);

		//!Write the stream to a file 
		unsigned int write(std::ostream &o) const;
		unsigned int write(const char *datafile) const;

		//!Return the atomic number of the element from either the long or short version of the atomic name 
		/*
		 * Short name takes precedence
		 * 
		 * Example : if range is "H" or "Hydrogen" function returns 1 
		 * Returns 0 on error (bad atomic name)
		 */
		unsigned int atomicNumberFromRange(unsigned int range) const;
		
	
		//!Get atomic number from ion ID
		unsigned int atomicNumberFromIonID(unsigned int ionID) const;

		//!Get a range ID from mass to charge 
		unsigned int getRangeID(float mass) const;

		//!Swap a range file with this one
		void swap(RangeFile &rng);

		//!Move a range's mass to a new location
		bool moveRange(unsigned int range, bool limit, float newMass);
		
};

//Number of elements stored in the table
const unsigned int NUM_ELEMENTS=119;

//!Load a pos file directly into a single ion list
/*! Pos files are fixed record size files, with data stored as 4byte
 * big endian floating point. (IEEE 574?). Data is stored as
 * x,y,z,mass/charge. 
 * */
//!Load a pos file into a T of IonHits
template<class T>
unsigned int GenericLoadFloatFile(int numcols, int index[], T &posIons,const char *posFile, unsigned int &progress, bool (*callback)())
{
	//buffersize must be a power of two and at least sizeof(IONHIT)
	const unsigned int NUMROWS=512;
	const unsigned int numPosCols = 4;
	const unsigned int BUFFERSIZE=numcols * sizeof(float) * NUMROWS;
	const unsigned int BUFFERSIZE2=numPosCols * sizeof(float) * NUMROWS;
	char *buffer=new char[BUFFERSIZE];
	char *buffer2=new char[BUFFERSIZE2];
	
	if(!buffer)
		return POS_ALLOC_FAIL;
	
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
	IONHIT *hitStruct;
	size_t pointCount=0;
	//regular case
	size_t curBufferSize=BUFFERSIZE;
	size_t curBufferSize2=BUFFERSIZE2;
	
	if(fileSize % (numcols * sizeof(float)))
	{
		delete[] buffer;
		delete[] buffer2;
		return POS_SIZE_MODULUS_ERR;	
	}
	
	try
	{
		posIons.resize(fileSize/sizeof(IONHIT));
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
	int maxCols = numcols * sizeof(float);
	int maxPosCols = numPosCols * sizeof(float);
	do
	{
		while((size_t)CFile.tellg() <= fileSize-curBufferSize)
		{
			CFile.read(buffer,curBufferSize);
			if(!CFile.good())
			{
				delete[] buffer;
				delete[] buffer2;
				return POS_READ_FAIL;
			}
			
			for (int j = 0; j < NUMROWS; j++) // iterate through rows
			{
				for (int i = 0; i < numPosCols; i++) // iterate through floats
				{
					memcpy(&(buffer2[j * maxPosCols + i * sizeof(float)]), 
						&(buffer[j * maxCols + index[i] * sizeof(float)]), sizeof(float));
				}
			}
			
			hitStruct = (IONHIT *)buffer2; 
			unsigned int ui;
			for(ui=0; ui<curBufferSize2; ui+=(sizeof(IONHIT)))
			{
				hit.setHit(hitStruct);
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
				hitStruct++;
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
	ASSERT(pointCount*sizeof(IONHIT) == fileSize);
	delete[] buffer;
	delete[] buffer2;
	
	return 0;
}


template<class T>
unsigned int LimitLoadPosFile(int numcols, int index[], T &posIons,const char *posFile, size_t limitCount,
	       	unsigned int &progress, bool (*callback)())
{
	//buffersize must be a power of two and at least sizeof(IONHIT)
	const unsigned int NUMROWS=1;
	const unsigned int numPosCols = 4;
	const unsigned int BUFFERSIZE=numcols * sizeof(float) * NUMROWS;
	const unsigned int BUFFERSIZE2=numPosCols * sizeof(float) * NUMROWS;
	char *buffer=new char[BUFFERSIZE];
	char *buffer2=new char[BUFFERSIZE2];
	
	if(!buffer)
		return POS_ALLOC_FAIL;

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
	int maxCols = numcols * sizeof(float);
	int maxPosCols = numPosCols * sizeof(float);
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
		return GenericLoadFloatFile(numcols, index, posIons,posFile,progress, callback);
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
				limitCount,dummy,callback);
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
	IONHIT hit;
	std::ios::pos_type  nextIonPos;
	for(size_t ui=0;ui<ionsToLoad.size(); ui++)
	{
		nextIonPos =  ionsToLoad[ui]*maxCols;
		
		if(CFile.tellg() !=nextIonPos )
			CFile.seekg(nextIonPos);

		CFile.read(buffer,BUFFERSIZE);

		for (int i = 0; i < numPosCols; i++) // iterate through floats
			memcpy(&(buffer2[i * sizeof(float)]), &(buffer[index[i] * sizeof(float)]), sizeof(float));
		
		memcpy((char *)(&hit), buffer2, sizeof(IONHIT));
		
		if(!CFile.good())
			return POS_READ_FAIL;
		posIons[ui].setHit(&hit);
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

template<class T>
unsigned int LimitRestrictLoadPosFile(T &posIons,const char *posFile, size_t limitCount,
	       const BoundCube &bound,	unsigned int &progress, bool (*callback)())
{
	//buffersize must be a power of two and at least sizeof(IONHIT)
	const unsigned int BUFFERSIZE=8192;
	char *buffer=new char[BUFFERSIZE];
	
	if(!buffer)
		return POS_ALLOC_FAIL;

	//open pos file
	std::ifstream CFile(posFile,std::ios::binary);

	if(!CFile)
	{
		delete[] buffer;
		return POS_OPEN_FAIL;
	}
	
	CFile.seekg(0,std::ios::end);
	size_t fileSize=CFile.tellg();
	
	if(!fileSize)
	{
		delete[] buffer;
		return POS_EMPTY_FAIL;
	}
	
	CFile.seekg(0,std::ios::beg);
	
	//calculate the number of points stored in the POS file
	size_t pointCount=0;
	size_t maxIons;
	//regular case
	
	if(fileSize % sizeof(IONHIT))
	{
		delete[] buffer;
		return POS_SIZE_MODULUS_ERR;	
	}

	maxIons =fileSize/sizeof(IONHIT);
	limitCount=std::min(limitCount,maxIons);

	//If we are going to load the whole file, don't use a sampling method to do it.
	if(limitCount == maxIons)
	{
		//Close the file
		CFile.close();
		delete[] buffer;
		//Try opening it using the normal functions
		int index[4] = {
				0, 1, 2, 3
				};
		return GenericLoadFloatFile(4, index, posIons,posFile,progress, callback);
	}

	//Use a sampling method to load the pos file
	try
	{
		posIons.resize(limitCount);
	}
	catch(std::bad_alloc)
	{
		return POS_ALLOC_FAIL;
	}


	RandNumGen rng;
	rng.initTimer();
	
	const size_t RECORD_WIDTH=16;

	//FIXME: If limitCount is near maxIons, this will be 
	//very inefficient better to remember only the gaps in that case.

	//OK, this algorithm is a bit tricky. We need to try to load
	//the limit size, but discard ions that lie outside the 
	//desired bounding volume. We don't know in advance how many we will get
	//until we inspect the data, so we have to guess that all of them fit, then
	//keep trying more until we run out of ions.
	//
	//So keep adding new ions, limitCount at a time, checking that the new
	//indicies are unique in the load.
	
	//Random sequence to load for this pass and already loaded sequence respectively
	std::vector<size_t> ionsToLoad,ionsLoaded;
	do
	{
		//Step 1: Generate random sequence
		//====
		
		//Create an array of random numbers  to load
		for(size_t ui=0; ui<limitCount; ui++)
			ionsToLoad[ui] = (rng.genUniformDev()*(maxIons-1));

		//Sort & Remove repeated indices 
		std::sort(ionsToLoad.begin(),ionsToLoad.end());
		std::unique(ionsToLoad.begin(),ionsToLoad.end());	

		//Step 2: Remove any entries we have already loaded from the list
		//===
		size_t seenCount;
		seenCount=0;
		
		//Sort already loaded ions before doing binary search below
		std::sort(ionsLoaded.begin(),ionsLoaded.end());

		for(unsigned int ui=0;ui<ionsToLoad.size()-seenCount;ui++)
		{
			//Keep swapping out bad entries until we find a good one
			while(std::binary_search(ionsLoaded.begin(),ionsLoaded.end(),
						ionsToLoad[ui]) && ui!= ionsToLoad.size()-seenCount)
			{
				//Swap this "bad" ion out for a fresh one
				std::swap(ionsToLoad[ui],ionsToLoad[ionsToLoad.size()-seenCount]);
				seenCount++;
			}

		}
		
		//Sort the vector front again, as we have disrupted the order during discard
		//(should be near sorted, so not too costly)
		//Ignore the back indicies,a as we wont use them anymore.
		//Being sorted means that we don't have to spin the disc backwards, and can jump forwards
		//every time.
		std::sort(ionsToLoad.begin(),ionsToLoad.end()-seenCount);
		//===
		

		unsigned int curProg = PROGRESS_REDUCE;	
		IONHIT hit;
		std::ios::pos_type  nextIonPos;
		unsigned int ionStart=0;
		for(size_t ui=0;ui<ionsToLoad.size()-seenCount; ui++)
		{
			nextIonPos =  ionsToLoad[ui]*sizeof(IONHIT);
			ionsLoaded.push_back(ionsToLoad[ui]);
			
			if(CFile.tellg() !=nextIonPos )
				CFile.seekg(nextIonPos);

			//Read the hit
			CFile.read((char *)&hit,sizeof(IONHIT));
			if(!CFile.good())
				return POS_READ_FAIL;

	
			//Reject the ion if it is not in the 
			//target volume	
			if(bound.containsPt(Point3D(hit.pos[0],hit.pos[1],hit.pos[2])))
			{
				posIons[pointCount].setHit(&hit);
				//Data bytes stored in pos files are big
				//endian. flip as required
				#ifdef __LITTLE_ENDIAN__
					posIons[pointCount].switchEndian();	
				#endif
				//Check for nan. If we find one, abort
				if(posIons[pointCount].hasNaN())
				{
					delete[] buffer;
					return POS_NAN_LOAD_ERROR;	
				}

				pointCount++;
			}
			
			//Update progress bar	
			if(!curProg--)
			{

				progress= (unsigned int)((float)(pointCount)/((float)limitCount)*100.0f);
				curProg=PROGRESS_REDUCE;

				if(!(*callback)())
				{
					delete[] buffer;
					posIons.clear();
					return POS_ABORT_FAIL;
					
				}
			}
		}

	//Keep loading until we can do no more, or we have enough.
	}while(pointCount < limitCount && ionsLoaded.size() != fileSize/RECORD_WIDTH); 

	delete[] buffer;
	return 0;
}



//!Load a pos file into a T of IonHits, excluding ions not within the bounds
template<class T>
unsigned int RestrictLoadPosFile(T &posIons,const char *posFile, const BoundCube &b,
		unsigned int &progress, bool (*callback)())
{

	ASSERT(b.isValid());
	//buffersize must be a power of two and at least sizeof(IONHIT)
	const unsigned int BUFFERSIZE=8192;
	char *buffer=new char[BUFFERSIZE];
	
	if(!buffer)
		return POS_ALLOC_FAIL;

	//open pos file
	std::ifstream CFile(posFile,std::ios::binary);

	if(!CFile)
	{
		delete[] buffer;
		return POS_OPEN_FAIL;
	}
	
	CFile.seekg(0,std::ios::end);
	size_t fileSize=CFile.tellg();
	
	if(!fileSize)
	{
		delete[] buffer;
		return POS_EMPTY_FAIL;
	}
	
	CFile.seekg(0,std::ios::beg);
	
	//calculate the number of points stored in the POS file
	IonHit hit;
	IONHIT *hitStruct;
	size_t pointCount=0;
	//regular case
	size_t curBufferSize=BUFFERSIZE;
	
	if(fileSize % sizeof(IONHIT))
	{
		delete[] buffer;
		return POS_SIZE_MODULUS_ERR;	
	}

	try
	{
		posIons.resize(fileSize/sizeof(IONHIT));
	}
	catch(std::bad_alloc)
	{
		return POS_ALLOC_FAIL;
	}

	
	while(fileSize < curBufferSize)
		curBufferSize = curBufferSize >> 1;

	//Technically this is dependant upon the buffer size.
	unsigned int curProg = PROGRESS_REDUCE;	
	size_t ionP=0;
	do
	{
		while((size_t)CFile.tellg() <= fileSize-curBufferSize)
		{
			CFile.read(buffer,curBufferSize);
			if(!CFile.good())
			{
				delete[] buffer;
				return POS_READ_FAIL;
			}

			hitStruct = (IONHIT *)buffer; 
			unsigned int ui;
			for(ui=0; ui<curBufferSize; ui+=(sizeof(IONHIT)))
			{
				hit.setHit(hitStruct);
				//Data bytes stored in pos files are big
				//endian. flip as required
				#ifdef __LITTLE_ENDIAN__
					hit.switchEndian();	
				#endif
				
				if(hit.hasNaN())
				{
					delete[] buffer;
					return POS_NAN_LOAD_ERROR;	
				}

				if(b.containsPt(hit.getPos()))
				{
					posIons[ionP] = hit;
					ionP++;
					
					pointCount++;
				}

				hitStruct++;
			}	
			
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

		curBufferSize = curBufferSize >> 1 ;
	}while(curBufferSize >= sizeof(IONHIT));

	ASSERT((unsigned int)CFile.tellg() == fileSize);
	ASSERT(pointCount*sizeof(IONHIT) == fileSize);
	delete[] buffer;

	return 0;
}

#endif
