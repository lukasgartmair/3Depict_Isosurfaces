/*
 * APTClasses.h - Generic APT components header 
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

#ifndef APTCLASSES_H
#define APTCLASSES_H

#include "common/basics.h"

#include <cstring>//memcpy
#include <new>//std::bad_alloc

//!Allowable export ion formats
enum
{
	IONFORMAT_POS=1
};

using std::vector;

class IonHit;
class Point3D;


const unsigned int PROGRESS_REDUCE=5000;

extern const char *POS_ERR_STRINGS[];

extern const char *ION_TEXT_ERR_STRINGS[];

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


//obtain a vector of points from an ion hit vector, by stripping out only the 3D point information 
void getPointsFromIons(const vector<IonHit> &ions, vector<Point3D> &pts);

//!make/append to a pos file from a set of a set of IonHits
void appendPos(const vector<IonHit> &points, const char *name);

//!Set the bounds from an array of ion hits
BoundCube getIonDataLimits(const vector<IonHit> &p);//

//!Get the sum of all Point3Ds in an ion vector
void getPointSum(const std::vector<IonHit> &points,Point3D &centroid);

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
		IonHit(const Point3D &p, float massToCharge);

		void setHit(float *arr) { pos.setValueArr(arr); massToCharge=arr[3];};
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
		const IonHit &operator=(const IonHit &obj);
		float operator[](unsigned int ui) const;	
		IonHit operator+(const Point3D &obj);
};	


//!Load a pos file directly into a single ion list
/*! Pos files are fixed record size files, with data stored as 4byte
 * big endian floating point. (IEEE 574?). Data is stored as
 * x,y,z,mass/charge. 
 * */
//!Load a pos file into a T of IonHits
unsigned int GenericLoadFloatFile(unsigned int inputnumcols, unsigned int outputnumcols, 
		unsigned int index[], vector<IonHit> &posIons,const char *posFile, 
				unsigned int &progress, bool (*callback)(bool));


unsigned int LimitLoadPosFile(unsigned int inputnumcols, unsigned int outputnumcols, unsigned int index[], 
			vector<IonHit> &posIons,const char *posFile, size_t limitCount,
					       	unsigned int &progress, bool (*callback)(bool),bool strongRandom);



unsigned int limitLoadTextFile(unsigned int numColsTotal, unsigned int selectedCols[], 
			vector<IonHit> &posIons,const char *posFile, const char *deliminator, const size_t limitCount,
					       	unsigned int &progress, bool (*callback)(bool),bool strongRandom);


#endif
