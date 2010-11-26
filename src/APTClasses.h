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


//!make a pos file from a set of a set of IonHits
unsigned int IonVectorToPos(const vector<IonHit> &points, const std::string &name);

//!make/append to a pos file from a set of a set of IonHits
void appendPos(const vector<IonHit> &points, const char *name);

//!Set the bounds from an array of ion hits
BoundCube getIonDataLimits(const vector<IonHit> &p);//

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
		void rangeByRangeID(std::vector<IonHit> &ionHits,
					unsigned int rangeID);
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
		//!Move both of a range's masses to a new location
		bool moveBothRanges(unsigned int range, float newLow, float newHigh);
		
};

//Number of elements stored in the table
const unsigned int NUM_ELEMENTS=119;

//!Load a pos file directly into a single ion list
/*! Pos files are fixed record size files, with data stored as 4byte
 * big endian floating point. (IEEE 574?). Data is stored as
 * x,y,z,mass/charge. 
 * */
//!Load a pos file into a T of IonHits
unsigned int GenericLoadFloatFile(int inputnumcols, int outputnumcols, 
		int index[], vector<IonHit> &posIons,const char *posFile, 
				unsigned int &progress, bool (*callback)());


unsigned int LimitLoadPosFile(int inputnumcols, int outputnumcols, int index[], 
			vector<IonHit> &posIons,const char *posFile, size_t limitCount,
					       	unsigned int &progress, bool (*callback)(),bool strongRandom);

#endif
