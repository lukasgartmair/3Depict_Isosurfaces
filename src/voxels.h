 /* Copyright (C) 2008  D. Haley
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

#ifndef VOXELS_H
#define VOXELS_H

#include "basics.h"

const unsigned int MAX_CALLBACK=500;

//FIXME: This is a hack. my class layout is completely borked. the voxels
//need to be able to count ions, so I need a declaration. APTClasses.h,
//basics.h and common.h need to be refactored
#include "APTClasses.h"

#include <vector>
#include <deque>
#include <cmath>
#include <iostream>
#include <cstdlib>
#include <list>
#include <stack>
#include <fstream>
#include <algorithm>
#include <queue>

using namespace std;

#ifdef USE_STXXL
#ifdef _OPENMP
#error STXXL is not thread safe, and cannot be used with openMP at this time.
#endif
#include <stxxl.h>
#endif

#include <gsl/gsl_linalg.h>
#include <gsl/gsl_eigen.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef _OPENMP
	#include <omp.h>
#endif

//!Boundary clipping mode
/*! These constants defines the behaviour of the routines when presented with a
 * boundary value problem, such as in convolution.
 */
enum{	
	BOUND_CLIP=1,
	BOUND_HOLD,
	BOUND_DERIV_HOLD,
	BOUND_MIRROR
};


enum{
	ADJ_ALL,
	ADJ_PLUS
};

//Error codes
enum{
	VOXELS_BAD_FILE_READ=1,
	VOXELS_BAD_FILE_OPEN,
	VOXELS_BAD_FILE_SIZE,
	VOXELS_OUT_OF_MEMORY,
};

//Must be power of two (buffer size when loading files, in sizeof(T)s)
const unsigned int ITEM_BUFFER_SIZE=65536;

//!Clipping direction constants
/*! Controls the clipping direction when performing clipping operations
 */
enum {
	CLIP_NONE=0,
       CLIP_LOWER_SOUTH_WEST,
       CLIP_UPPER_NORTH_EAST,
};


enum {
	VOXEL_ABORT_ERR,
	VOXEL_BOUNDS_INVALID_ERR,
};

/*
template<class T> class TRIPLET
{
	public:
		T x,y,z;
		//Equality operator
		TRIPLET operator=(const TRIPLET<T> &t){ x=t.x; y=t.y; z=t.z; return *this;};
		//used for sorting; exact definition defined only internally
		bool operator<(const TRIPLET<T> &t) const;
		//Equality
		bool operator==(const TRIPLET<T> &t) const { return t.x==x && t.y==y && t.z==z;};
};

inline std::ostream& operator<<(std::ostream &stream, const TRIPLET &pt)
{
	stream << "(" << pt.x << "," << pt.y << "," << pt.z << ")";
	return stream;
}
*/

//Convenient C++ tricks
template<class T>
void push_sorted (std::list<T> & storage, T item)
{
	storage.insert(std::lower_bound(storage.begin(), storage.end(), item),item);
}




#ifdef _OPENMP
//Auto-detect the openMP iterator type.
#if ( __GNUC__ > 4 && __GNUC_MINOR__ > 3 )
	#define MP_INT_TYPE unsigned long long
#else
	#define MP_INT_TYPE long long 
#endif
#else
	#define MP_INT_TYPE unsigned long long
#endif


//!Template class that stores 3D voxel data
/*! To instantiate this class, objects must have
 * basic mathematical operators, such as * + - and =
 */
template<class T> class Voxels
{
	private:
		//!Number of bins in data set (X,Y,Z)
		unsigned long long binCount[3];

		//!Voxel array 
#ifndef USE_STXXL
		std::vector<T> voxels;
#endif
		//!Alternate description for voxel array
		/*VECTOR_GENERATOR<double,8,16,8*1024*1024,RC,lru>
		 * result:external vector of double's , with 8 blocks per page,
		16 pages, 8MB blocks, Random Cyclic allocation and lru cache replacement strategy */
#ifdef USE_STXXL
		stxxl::vector<T,12,stxxl::lru_pager<8>, 8*1024*1024> voxels;
#endif

		//FIXME: Before switching to getDataRef, we have to replace all calls to getDataRef with getDataIt - stxxl does not guarantee valid pointers, but does for valid references

		//!Scaling value for furthest bound of the dataset. Dataset is assumed to sit in a rectilinear volume from (0,0,0)
		Point3D minBound, maxBound;
	
		bool (*callback)(void);
			
	public:
		//!Constructor.
		Voxels();
		//!Destructor
		~Voxels();

		//!Swap object contents with other voxel object
		void swap(Voxels<T> &v);

		//!Clone this into another object
		void clone(Voxels<T> &newClone) const;

		//!Set the value of a point in the dataset
		void setPoint(const Point3D &pt, const T &val);
		//!Retrieve the value of a datapoint
		T getPointData(const Point3D &pt) const;


		//!Retrieve the XYZ voxel location associated with a given position
		void getIndex(unsigned long long &x, unsigned long long &y,
				unsigned long long &z, const Point3D &p) const;
		
		//!Get the position associated with an XYZ voxel
		Point3D getPoint(unsigned long long x, 
				unsigned long long y, unsigned long long z) const;
		//!Retrieve the value of a specific voxel
		inline T getData(unsigned long long x, unsigned long long y, unsigned long long z) const;
		//!Retrieve value of the nth voxel
		inline T getData(unsigned long long i) const { return voxels[i];}
		//!Retrieve the value of the nth voxel
		/* No guarantees are made about the ordering of the voxel, however each voxel will be obtained uniquely
		 * by its ith index
		 */
		//const T &getDataRef(unsigned long long i) const { return voxels[i];};

		//!Returns underlying pointer --  UNSUPPORTED FUNCTION -- USE AT YOUR PERIL
		/* No guarantees are made about world not exploding
		 * Function use may cause operator harm! Do not use 
		 * unless you know what you are doing... even then its not really a good idea. 
		 * Just a nasty hack I needed one day..
		 */
		//const T *getDataPtr() const { return voxels;};


		void setEntry(unsigned long long n, const T &val) { voxels[n] = val;};
		//!Retrieve a reference to the data ata  given position
		//const T &getDataRef(unsigned long long x, unsigned long long y, unsigned long long z) const;
		//!Set the value of a point in the dataset
		void setData(unsigned long long x, unsigned long long y, unsigned long long z, const T &val);
		//!Set the value of nth point in the dataset
		void setData(unsigned long long n, const T &val);
		
		//!Get the size of the data field
		void getSize(unsigned long long &x, unsigned long long &y, unsigned long long &z) const;
		unsigned long long getSize() const {return voxels.size();};

		//!Resize the data field
		/*! This will destroy any data that was already in place
		 * If the data needs to be preserved use "resizeKeepData"
		 * Data will *not* be initialised
		 */
		unsigned long long resize(unsigned long long newX, unsigned long long newY, unsigned long long newZ, const Point3D &newMinBound=Point3D(0.0f,0.0f,0.0f), const Point3D &newMaxBound=Point3D(1.0f,1.0f,1.0f));
		
		//!Resize the data field, maintaining data as best as possible
		/*! This will preserve data by resizing as much as possible 
		 * about the origin. If the bounds are extended, the "fill" value is used
		 * by default iff doFill is set to true. 
		 */
		unsigned long long resizeKeepData(unsigned long long &newX, unsigned long long &newY, unsigned long long &newZ, 
					unsigned int direction=CLIP_LOWER_SOUTH_WEST, const Point3D &newMinBound=Point3D(0.0f,0.0f,0.0f), const Point3D &newMaxBound=Point3D(1.0f,1.0f,1.0f), const T &fill=T(0),bool doFill=false);


		//!Rebin the data by a given rate
		/*! This will perform a quick and dirty rebin operation, where groups of datablocks 
		 * are binned into a single cell. Number of blocks binned is rate^3. Field must be larger than rate
		 * in all directions. Currently only CLIP_NONE is supported.
		 */
		void rebin(Voxels<T> &dest, unsigned long long rate, unsigned long long clipMode=CLIP_NONE) const;
		
		//!Get the total value of the data field.
		/*! An "initial value" is provided to provide the definition of a zero value
		 */
		T getSum(const T &initialVal=T(0.0)) const;

		//!Fill all voxels with a given value
		void fill(const T &val);
		//!Get the bounding size
		Point3D getMinBounds() const;
		Point3D getMaxBounds() const;
		///! Get the spacing for a unit cell
		Point3D getPitch() const;
		//!Set the bounding size
		void setBounds(const Point3D &pMin, const Point3D &pMax);

		//!Initialise the voxel storage
		unsigned long long init(unsigned long long nX,unsigned long long nY,unsigned long long nZ, const BoundCube &bound);
		//!Initialise the voxel storage
		unsigned long long init(unsigned long long nX,unsigned long long nY, unsigned long long nZ);
		//!Load the voxels from file
		/*! Format is flat unsigned long longs in column major
		 * return codes:
		 * 1: File open error 
		 * 2: Data size error
		 */
		unsigned long long loadFile(const char *cpFilename, unsigned long long nX,
						unsigned long long nY, unsigned long long nZ, bool silent=false);
		//!Write the voxel objects in column major written out to file
		/*! Format is flat objects ("T"s) in column major format.
		 * Returns nonzero on failure
		 */
		unsigned long long writeFile(const char *cpFilename) const;

		//! Detect the position of the peaks in the dataset -- UNTESTED/UNCONFIRMED. USE AT YOUR PERIL.
		/*! Uses a simple gaussian smoothed SSD to locate peaks
		 *  sigmaThresh is the sigma used to threshold the negative of the SSD.
		 *  sigmaSmooth is the width of the gaussian kernel 
		 *  kernelLen is the size of the gaussian kernel (eg 3 produces a 3 by 3 by 3 kernel)
		 */
		void peakDetect(std::vector<Point3D> &dataVec,float sigmaThresh, 
			float sigmaSmooth, unsigned long long kernelLen) const;

		//!Run convolution over this data, placing the correlation data into "result"
		/*! 
		 * Datasets MUST have the same pitch (spacing) for the result to be defined
		 * template type must have a  T(0.0) constructor that intialises it to some "zero"
		 */
		unsigned long long convolve(const Voxels<T> &templateVec, Voxels<T> &result,
							 bool showProgress=true,unsigned long long boundMode=BOUND_CLIP) const; 

		
		//!Set this object to a normalised gaussian kernel, centered around the middle of the dataset
		void setGaussianKernelCube(float stdDev, float bound, unsigned long long sideLen);
		
		//!Second derivative by difference approximation 
		/*! Returns the first order central difference approximation to the
		 * second derivative. Bound mode can (when implemented) bye used to compute
		 * appropriate differences.
		 */
		void secondDifference(Voxels<T> &result, unsigned long long boundMode=BOUND_CLIP) const;


		//!Find the positions of the voxels that are above or below a given value
		/*! Returns the positions of the voxels' centroids for voxels that have, by default,
		 * a value greater than that of thresh. This behaviour can by reversed to "lesser than"
		 * by setting lowerEq to false
		 */
		void thresholdForPosition(std::vector<Point3D> &p, const T &thresh, bool lowerEq=false);



		//!Construct a spherical kernel
		/* The spherical result is located at the centre of the voxel set
		 * sideLen -The side length is the diameter of the sphere
		 * bound -the bounding value that the voxels contain themselves within
		 * val - the value that the voxels take on for a full sphere.
		 * antialiasingLevel - the number of times to subdivide the voxels into 8 parts 
		 *  and test these voxels for internal/external. The resultant fraction of  inside/outside voxels is summed.
		 */
		void makeSphericalKernel(unsigned long long sideLen, float bound, const T &val, unsigned int antialiasLevel=0);

		//!Return the sizeof value for the T type
		/*! Maybe there is a better way to do this, I don't know
		 */
		unsigned long long sizeofType() const { return sizeof(T);}; 


		//!Binarise the data into a result vector
		/* On thresholding (val > thresh), set the value to "onThresh". 
		 * Otherwise set to "offthresh"
		 */
		void binarise(Voxels<T> &result,const T &thresh, const T &onThresh, 
				const T &offThresh) const;
	
		//!Calculate the moravec-harris eigenvalues for feature detection
		/* Computes the eigenvalues of the moravec-harris detector
		 * See Harris, C. & Stephens, M. A combined corner and edge detector Alvey vision conference, 1998, 15, 50
		 */	
		void moravecHarrisFeatures(Voxels<float> &eigA, Voxels<float> &eigB,Voxels<float> &eigC ) const;

		//!Empty the data
		/*Remove the data from the voxel set
		 */
		void clear() { voxels.clear();};

		//!Find minimum in dataset
		T min() const;

		//!Find maximum in dataset
		T max() const;	

		//!Given some coordinates and radii, generate a voxel dataset of solid spheres superimposed
		/*! The radius specified is in the bounding units
		 *  clear value optionally wipes the dataset before continuing 
		 */
		void fillSpheresByPosition( const std::vector<Point3D> &spherePos, float rad, const T &value, bool doErase=true);


		//!Generate a histogram of data values. / operator must be defined for target type
		/*! Data bins are evenly spaced between min and max. Formula for binning is (val-min())/(max()-min())*histBinCount
		 */
		int histogram(std::vector<unsigned long long> &v, unsigned long long histBinCount) const;

		//!Find the largest or smallest n objects
		void findSorted(std::vector<unsigned long long> &x, std::vector<unsigned long long> &y, std::vector<unsigned long long> &z, unsigned long long n, bool largest=true) const;

		//!Generate a dataset that consists of the counts of points in a given voxel
		/*! Ensure that the voxel scaling factors 
		 * are set by calling ::setBounds() with the 
		 * appropriate parameters for your data.
		 * Disabling nowrap allows you to "saturate" your
		 * data field in the case of dense regions, rather 
		 * than let wrap-around errors occur
		 */
		int countPoints( const std::vector<Point3D> &points, bool noWrap=true, bool doErase=false);
		int countPoints( const std::vector<IonHit> &points, bool noWrap=true, bool doErase=false);
	
		//! divide countPoints by volume to get density
		int calculateDensity();

		//!increment the position specified
		inline void increment(unsigned long long x, unsigned long long y, unsigned long long z){
			typedef unsigned long long ull;
	//Typecast everything to at least 64 bit uints.
	voxels[(ull)z*(ull)binCount[1]*(ull)binCount[0]
		+(ull)y*(ull)binCount[0] + (ull)x]++;}
	
		void setCallbackMethod(bool (*cb)(void)) {callback = cb;}

		//!Element wise devision	
		void operator/=(const Voxels<T> &v);
	
};

//!Convert one type of voxel into another by assignment operator
template<class T, class U>
void castVoxels(const Voxels<T> &src, Voxels<U> &dest)
{
	unsigned long long x,y,z;

	//Resize the dest
	src.getSize(x,y,z);
	
	dest.resize(x,y,z,src.getBounds());

	unsigned long long numEntries=x*y*z;
#pragma omp parallel for 
	for(MP_INT_TYPE ui=0; ui <(MP_INT_TYPE)numEntries; ui++)
	{
		dest.setEntry(ui,src.getData(ui));
	}

}

//!Use one counting type to sum counts in a voxel of given type
template<class T, class U>
void sumVoxels(const Voxels<T> &src, U &counter)
{
	unsigned long long nx,ny,nz;

	src.getSize(nx,ny,nz);
	
	counter=0;
	for(unsigned long long ui=0; ui<nx; ui++)
	{
		for(unsigned long long uj=0; uj<ny; uj++)	
		{
			for(unsigned long long uk=0; uk<nz; uk++)	
				counter+=src.getData(ui,uj,uk);
		}
	}

}

//====
template<class T>
Voxels<T>::Voxels() : voxels(), minBound(Point3D(0,0,0)), maxBound(Point3D(1,1,1))
{
}

template<class T>
Voxels<T>::~Voxels()
{
}


template<class T>
void Voxels<T>::clone(Voxels<T> &newVox) const
{
	newVox.binCount[0]=binCount[0];
	newVox.binCount[1]=binCount[1];
	newVox.binCount[2]=binCount[2];

	newVox.voxels.resize(voxels.size());

	std::copy(voxels.begin(),voxels.end(),newVox.voxels.begin());

}

template<class T>
void Voxels<T>::setPoint(const Point3D &point,const T &val)
{
	ASSERT(voxels.size());
	unsigned long long pos[3];
	for(unsigned long long ui=0;ui<3;ui++)
		pos[ui] = (unsigned long long)round(point[ui]*(float)binCount[ui]);


	voxels[pos[2]*binCount[1]*binCount[0] + pos[1]*binCount[0] + pos[0]]=val; 
}

template<class T>
void Voxels<T>::setData(unsigned long long x, unsigned long long y, 
			unsigned long long z, const T &val)
{
	ASSERT(voxels.size());

	ASSERT( x < binCount[0] && y < binCount[1] && z < binCount[2]);
	voxels[z*binCount[1]*binCount[0] + y*binCount[0] + x]=val; 
}

template<class T>
inline void Voxels<T>::setData(unsigned long long n, const T &val)
{
	ASSERT(voxels.size());
	ASSERT(n<voxels.size());

	voxels[n] =val; 
}

template<class T>
T Voxels<T>::getPointData(const Point3D &point) const
{
	ASSERT(voxels.size());
	unsigned long long pos[3];
	for(unsigned long long ui=0;ui<3;ui++)
		pos[ui] = (unsigned long long)round(point[ui]*(float)binCount[ui]);

	return voxels[pos[2]*binCount[1]*binCount[0] + pos[1]*binCount[0] + pos[0]]; 
}

template<class T>
Point3D Voxels<T>::getPoint(unsigned long long x, unsigned long long y, unsigned long long z) const
{
	ASSERT(x < binCount[0] && y<binCount[1] && z<binCount[2]);

	return Point3D((float)x/(float)binCount[0]*(maxBound[0]-minBound[0]) + minBound[0],
			(float)y/(float)binCount[1]*(maxBound[1]-minBound[1]) + minBound[1],
			(float)z/(float)binCount[2]*(maxBound[2]-minBound[2]) + minBound[2]);
}

template<class T>
Point3D Voxels<T>::getPitch() const
{
	return Point3D((float)1.0/(float)binCount[0]*(maxBound[0]-minBound[0]),
			(float)1.0/(float)binCount[1]*(maxBound[1]-minBound[1]),
			(float)1.0/(float)binCount[2]*(maxBound[2]-minBound[2]));
}

template<class T>
void Voxels<T>::getSize(unsigned long long &x, unsigned long long &y, unsigned long long &z) const
{
	ASSERT(voxels.size());
	x=binCount[0];
	y=binCount[1];
	z=binCount[2];
}

template<class T>
unsigned long long Voxels<T>::resize(unsigned long long x, unsigned long long y, unsigned long long z, const Point3D &newMinBound, const Point3D &newMaxBound) 
{
	voxels.clear();

	binCount[0] = x;
	binCount[1] = y;
	binCount[2] = z;


	minBound=newMinBound;
	maxBound=newMaxBound;

	unsigned long long binCountMax = binCount[0]*binCount[1]*binCount[2];
	try
	{
	voxels.resize(binCountMax);
	}
	catch(...)
	{
		return 1;
	}

	return 0;
}

template<class T>
unsigned long long Voxels<T>::resizeKeepData(unsigned long long &newX, unsigned long long &newY, unsigned long long &newZ, 
			unsigned int direction, const Point3D &newMinBound, const Point3D &newMaxBound, const T &fill,bool doFill)
{

	ASSERT(doFill);
	ASSERT(direction==CLIP_LOWER_SOUTH_WEST);

	Voxels<T> v;
	
	if(v.resize(newX,newY,newZ))
		return 1;

	switch(direction)
	{
		case CLIP_LOWER_SOUTH_WEST:
		{
			minBound=newMinBound;
			maxBound=newMaxBound;
			unsigned long long itStop[3];
			itStop[0]=std::min(newX,binCount[0]);
			itStop[1]=std::min(newY,binCount[1]);
			itStop[2]=std::min(newZ,binCount[2]);

			unsigned long long itMax[3];
			itMax[0]=std::max(newX,binCount[0]);
			itMax[1]=std::max(newY,binCount[1]);
			itMax[2]=std::max(newZ,binCount[2]);

			if(doFill)
			{
				//Duplicate into new value, if currently inside bounding box
				//This logic will be a bit slow, owing to repeated "if"ing, but
				//it is easy to understand. Other logics would have many more
				//corner cases
				bool spin=false;
#pragma omp parallel for
				for(unsigned long long ui=0;ui<itMax[0];ui++)
				{
					if(spin)
						continue;

					for(unsigned long long uj=0;uj<itMax[1];uj++)
					{
						for(unsigned long long uk=0;uk<itMax[2];uk++)
						{
							if(itStop[0]< binCount[0] && 
								itStop[1]<binCount[1] && itStop[2] < binCount[2])
								v.setData(ui,uj,uk,getData(ui,uj,uk));
							else
								v.setData(ui,uj,uk,fill);
						}
					}

#pragma omp critical
					{
					if(!(*callback)())
						spin=true;
					}
				}

				if(spin)
					return VOXEL_ABORT_ERR;
			}



			break;
		}

		default:
			//Not implemented
			ASSERT(false);
	}

	return 0;
}

template<class T>
Point3D Voxels<T>::getMinBounds() const
{
	ASSERT(voxels.size());
	return minBound;
}
										 
template<class T>
Point3D Voxels<T>::getMaxBounds() const
{
	ASSERT(voxels.size());
	return maxBound;
}
										 
template<class T>
void Voxels<T>::setBounds(const Point3D &pMin, const Point3D &pMax)
{
	ASSERT(voxels.size());
	minBound=pMin;
	maxBound=pMax;
}

template<class T>
unsigned long long Voxels<T>::init(unsigned long long nX, unsigned long long nY,
	       				unsigned long long nZ, const BoundCube &bound)
{
	binCount[0]=nX;
	binCount[1]=nY;
	binCount[2]=nZ;

	//voxels.clear();
	typedef unsigned long long ull;
	ull binCountMax;
       
	//TODO: need to check validity of boundcube
	bound.getBounds(minBound, maxBound);
	binCountMax= (ull)binCount[0]*(ull)binCount[1]*(ull)binCount[2];

	voxels.resize(binCountMax);

#pragma omp parallel for
	for(unsigned long long ui=0; ui<binCountMax; ui++) 
		voxels[ui]=0;

	return 0;
}

template<class T>
unsigned long long Voxels<T>::init(unsigned long long nX, unsigned long long nY, unsigned long long nZ)

{
	Point3D pMin(0,0,0), pMax(nX,nY,nZ); 

	return init(nX,nY,nZ,pMin,pMax);
}

template<class T>
unsigned long long Voxels<T>::loadFile(const char *cpFilename, unsigned long long nX, unsigned long long nY, unsigned long long nZ , bool silent)
{
	std::ifstream CFile(cpFilename,std::ios::binary);

	if(!CFile)
		return VOXELS_BAD_FILE_OPEN;
	
	CFile.seekg(0,std::ios::end);
	
	
	unsigned long long fileSize = CFile.tellg();
	if(fileSize !=nX*nY*nZ*sizeof(T))
		return VOXELS_BAD_FILE_SIZE;

	resize(nX,nY,nZ,Point3D(nX,nY,nZ));
	
	CFile.seekg(0,std::ios::beg);

	unsigned int curBufferSize=ITEM_BUFFER_SIZE*sizeof(T);
	unsigned char *buffer = new unsigned char[curBufferSize];

	//Shrink the buffer size by factors of 2
	//in the case of small datasets
	while(fileSize < curBufferSize)
		curBufferSize = curBufferSize >> 1;

	
	//Draw a progress bar
	if(!silent)
	{
		cerr << std::endl << "|";
		for(unsigned int ui=0; ui<100; ui++)
			cerr << ".";
		cerr << "| 100%" << std::endl << "|";
	}
		
	unsigned int lastFrac=0;
	unsigned int ui=0;
	unsigned int pts=0;
	do
	{
	
		//Still have data? Keep going	
		while((unsigned long long)CFile.tellg() <= fileSize-curBufferSize)
		{
			//Update progress bar
			if(!silent & ((unsigned int)(((float)CFile.tellg()*100.0f)/(float)fileSize) > lastFrac))
			{
				cerr << ".";
				pts++;
				lastFrac=(unsigned int)(((float)CFile.tellg()*100.0f)/(float)fileSize) ;	
			}

			//Read a chunk from the file
			CFile.read((char *)buffer,curBufferSize);
	
			if(!CFile.good())
				return VOXELS_BAD_FILE_READ;

			//Place the chunk contents into ram
			for(unsigned long long position=0; position<curBufferSize; position+=(sizeof(T)))
				voxels[ui++] = (*((T *)(buffer+position)));
		}
				
		//Halve the buffer size
		curBufferSize = curBufferSize >> 1 ;

	}while(curBufferSize> sizeof(T)); //This does a few extra loops. Not many 

	delete[] buffer;

	//Fill out the progress bar
	if(!silent)
	{
		while(pts++ <100)
			cerr << ".";
	
		cerr << "| done" << std::endl;
	}

	return 0;
}

template<class T>
unsigned long long Voxels<T>::writeFile(const char *filename) const
{

	ASSERT(voxels.size())

	std::ofstream file(filename, std::ios::binary);

	if(!file)
		return 1;

	
	for(unsigned long long ui=0; ui<binCount[0]*binCount[1]*binCount[2]; ui++)
	{
		T v;
		v=voxels[ui];
		file.write((char *)&v,sizeof(T));
		if(!file.good())
			return 2;
	}
	return 0;
}


template<class T>
T Voxels<T>::getSum(const T &initialValue) const
{
	ASSERT(voxels.size());
	T returnVal=initialValue;
	T val;
#pragma omp parallel for private(val) reduction(+:returnVal) 
	for(MP_INT_TYPE ui=0; ui<binCount[0] ; ui++)
	{
		for(unsigned long long uj=0; uj<binCount[1]; uj++ )
		{
			for(unsigned long long uk=0; uk<binCount[1]; uk++) 
			{
				val=getData(ui,uj,uk);	
				returnVal+=val; 
			}
		}
	}

	return returnVal;
}

template<class T>
void Voxels<T>::peakDetect(std::vector<Point3D> &dataVec,float sigmaThresh, 
			float sigmaSmooth, unsigned long long kernelSize) const
{
	ASSERT(voxels.size());

	//A (very) simple peak detection algorithm
	//This uses smoothed second differentials 
	//to compute a response function.
	
	//Ensure sanity of input params
	
	//Is the kernel bigger than the dataset. That is not good !
	ASSERT(kernelSize < binCount[0] || kernelSize < binCount[1] || kernelSize <binCount[2]);
	
	//compute the second differential
	Voxels<T> ssd;
	secondDifference(ssd);

	Voxels<T> kernel,result;

	//Kernel size must be odd
	//confirm with bitwise and
	ASSERT(kernelSize & 1)

	//Shit. How do I make this work. The kernel must be rectangular. 
	//So our voxels must be rectangular
	//ASSERT(voxelsAreRectangular); FIXME
	float kernelBound=(maxBound[0]-minBound[0])*kernelSize/binCount[0];
	kernel.setGaussianKernelCube(sigmaSmooth,kernelBound,kernelSize);
	//Convolve to get the smoothed second difference
	ssd.convolve(kernel,result);

	//Threshold to get the position. 
	//Account for the offset (1 voxel) induced by central diff.


	

}

template<class T>
void Voxels<T>::swap(Voxels<T> &kernel)
{
	std::swap(binCount[0],kernel.binCount[0]);
	std::swap(binCount[1],kernel.binCount[1]);
	std::swap(binCount[2],kernel.binCount[2]);

	std::swap(voxels,kernel.voxels);
	
	std::swap(maxBound,kernel.maxBound);
	std::swap(minBound,kernel.minBound);
}

template<class T>
unsigned long long Voxels<T>::convolve(const Voxels<T> &kernel, Voxels<T> &result,  bool showProgress, unsigned long long boundMode) const
{
	ASSERT(voxels.size());

	//Check the kernel can fit within this datasest
	unsigned long long x,y,z;
	kernel.getSize(x,y,z);
	if(binCount[0] <x || binCount[1]< y || binCount[2] < z)
		return 1;

	//Loop through all of the voxel elements, setting the
	//result voxels to the convolution of the template
	//with the data 

	if(showProgress)
	{
		cerr << std::endl <<  "|";
		for(unsigned long long ui=0; ui<100; ui++)
			cerr << "." ; 
		cerr << "|" << std::endl << "|";
	}
	

	unsigned long long progress=0;
	unsigned long long its=0;
	switch(boundMode)
	{
		case BOUND_CLIP:
		{
			//Calculate the bounding size of the resultant box
			Point3D resultMinBound, resultMaxBound;
			resultMinBound=minBound*Point3D((float)(binCount[0]-x)/(float)binCount[0],
				(float)(binCount[1]-y)/(float)binCount[1],(float)(binCount[2]-z)/(float)binCount[2] );
			resultMaxBound=maxBound*Point3D((float)(binCount[0]-x)/(float)binCount[0],
				(float)(binCount[1]-y)/(float)binCount[1],(float)(binCount[2]-z)/(float)binCount[2] );
			//Set up the result box
			unsigned long long sX,sY,sZ;
			sX = binCount[0] -x;
			sY = binCount[1] -y;
			sZ = binCount[2] -z;
			float itsToDo=(float)sX*(float)sY;

			if(result.resize(sX,sY,sZ,resultMinBound, resultMaxBound))
				return VOXELS_OUT_OF_MEMORY;

			bool spin=false;
			//loop through each data element
			//Forgive the poor indenting, but the looping is very deep
#pragma omp parallel for shared(progress, its)
			for (MP_INT_TYPE ui=0;ui<(MP_INT_TYPE)(binCount[0]-x); ui++)
			{
				if(spin)
					continue;
			for (unsigned long long uj=0;uj<binCount[1]-y; uj++)
			{
#pragma omp critical
				{
					its++;
					//Update progress bar
					if(showProgress && progress < (unsigned long long) ((float)its/itsToDo*100) )
					{
						cerr <<  ".";
						progress = (unsigned long long) ((float)its/itsToDo*100);
					}
					if(!(*callback)())
						spin=true;
				}
			for (unsigned long long uk=0;uk<binCount[2]-z; uk++)
			{
				T tally;
				tally=T(0.0);

				//	T origVal;
				//Convolve this element with the kernel 
				for(unsigned long long ul=0; ul<x; ul++)
				{
				for(unsigned long long um=0; um<y; um++)
				{
				for(unsigned long long un=0; un<z; un++)
				{
					tally+=(getData(ui+ ul,uj + um,uk + un))*
							kernel.getData(ul,um,un);
				}
				}
				}

				result.setData(ui,uj,uk,tally);	
			}
			}
			
			}

			if(spin)
				return VOXEL_ABORT_ERR;

			break;
		}
		case BOUND_HOLD:
		case BOUND_DERIV_HOLD:
		case BOUND_MIRROR:
		default:
			ASSERT(false);
			return 2;
	}

	if(showProgress)
	{
		cerr << "|" << std::endl;
	}
	
	return 0;
}

template<class T>
T Voxels<T>::getData(unsigned long long x, unsigned long long y, unsigned long long z) const
{
	typedef unsigned long long ull;
	ASSERT(x < binCount[0] && y < binCount[1] && z < binCount[2]);
	ull off; //byte offset

	//Typecast everything to at least 64 bit uints.
	off=(ull)z*(ull)binCount[1]*(ull)binCount[0];
	off+=(ull)y*(ull)binCount[0] + (ull)x;

	ASSERT(off < voxels.size());
	return	voxels[off]; 
}

/*
template<class T>
const T &Voxels<T>::getDataRef(unsigned long long x, unsigned long long y, unsigned long long z) const
{
	typedef unsigned long long ull;
	ASSERT(x < binCount[0] && y < binCount[1] && z < binCount[2]);
	ull off;//byte offset

	//Typecast everything to at least 64 bit uints.
	off=(ull)z*(ull)binCount[1]*(ull)binCount[0];
	off+=(ull)y*(ull)binCount[0] + (ull)x;
	return	voxels[off]; 
}
*/

template<class T>
void Voxels<T>::setGaussianKernelCube(float stdDev, float bound, unsigned long long sideLen)
{
	T obj;

	//Equivariance gaussian.
	float product = 1.0/(stdDev*stdDev*stdDev*sqrt(2.0*M_PI));

	float scale;
	const unsigned long long halfLen = sideLen/2;

	resize(sideLen,sideLen,sideLen,Point3D(bound,bound,bound));

	const float twoVariance=2.0*stdDev*stdDev;

	scale=bound/sideLen;
	//Loop through the data
	//This could be sped up using decrementing whiles, 
	//at the cost of losing the openMP bit
#pragma omp parallel for shared(product)
	for(MP_INT_TYPE ui=0;ui<sideLen;ui++)
	{
		for(MP_INT_TYPE uj=0;uj<sideLen;uj++)
		{
			for(MP_INT_TYPE uk=0;uk<sideLen;uk++)
			{
				unsigned long long offset;
				offset = (ui-halfLen)*(ui-halfLen) + (uj-halfLen)*(uj-halfLen) + (uk-halfLen)*(uk-halfLen);

				obj = T(product*exp(-scale*((float)offset)/(twoVariance)));
				setData(ui,uj,uk,obj);
			}
		}
	}
}

template<class T>
void Voxels<T>::secondDifference(Voxels<T> &result, unsigned long long boundMode) const
{
	//Only clipping mode is supported at this time
	ASSERT(boundMode=BOUND_CLIP);
	//Note: Central difference loses two values from each edge
	ASSERT(binCount[0] > 2 && binCount[1] > 2 && binCount[2] > 2);
	float xSqr,ySqr,zSqr;

	xSqr = (maxBound[0]-minBound[0])/binCount[0];

	ySqr = (maxBound[1]-minBound[1])/binCount[1];

	zSqr = (maxBound[2]-minBound[2])/binCount[2];



	//Allocate the second difference result, with a bin wdith
	//Note: We are bastardising the definition of xSqr for convenience here
	//xSqr at this time is simply the grid pitch (same for ySqr,zSqr)

	result.resize(binCount[0]-2,binCount[1]-2,binCount[2]-2,
					Point3D((binCount[0]-2)*xSqr,
						(binCount[1]-2)*ySqr,
						(binCount[2]-2)*zSqr));

	
	xSqr*=xSqr;
	ySqr*=ySqr;
	zSqr*=zSqr;


	//Calculate del^2
	T d;
#pragma omp parallel for private(d)
	for(MP_INT_TYPE ui=1; ui<binCount[0]-1; ui++)
	{
		for(MP_INT_TYPE  uj=1; uj<binCount[1]-1; uj++)
		{
			for(MP_INT_TYPE uk=1; uk<binCount[2]-1; uk++)
			{
			
				//dx by first order central difference
				d=T((getData(ui+1,uj,uk) - 2.0*getData(ui,uj,uk) + getData(ui-1,uj,uk))/(xSqr));
				
				//dy
				d+=T((getData(ui,uj+1,uk) - 2.0*getData(ui,uj,uk) + getData(ui,uj-1,uk))/(ySqr));
				
				//dz
				d+=T((getData(ui,uj,uk+1) - 2.0*getData(ui,uj,uk) + getData(ui,uj,uk-1))/(zSqr));
				result.setData(ui-1,uj-1,uk-1,d);
			}
		}
	}

}

template<class T>
void Voxels<T>::thresholdForPosition(std::vector<Point3D> &p, const T &thresh, bool lowerEq)
{
	p.clear();

	if(lowerEq)
	{
#pragma omp parallel for 
		for(MP_INT_TYPE ui=0;ui<binCount[0]; ui++)
		{
			for(unsigned long long uj=0;uj<binCount[1]; uj++)
			{
				for(unsigned long long uk=0;uk<binCount[2]; uk++)
				{
					if( getData(ui,uj,uk) < thresh)
					{
#pragma omp critical
						p.push_back(getPoint(ui,uj,uk));
					}

				}
			}
		}
	}
	else
	{
#pragma omp parallel for 
		for(MP_INT_TYPE ui=0;ui<binCount[0]; ui++)
		{
			for(unsigned long long uj=0;uj<binCount[1]; uj++)
			{
				for(unsigned long long uk=0;uk<binCount[2]; uk++)
				{
					if( getData(ui,uj,uk) > thresh)
					{
#pragma omp critical
						p.push_back(getPoint(ui,uj,uk));
					}

				}
			}
		}
	}
}

template<class T>
void Voxels<T>::binarise(Voxels<T> &result, const T &thresh, 
				const T &onThresh, const T &offThresh) const
{

	result.resize(binCount[0],binCount[1],
			binCount[2],minBound,maxBound);
#pragma omp parallel for
	for(MP_INT_TYPE ui=0;ui<(MP_INT_TYPE)binCount[0]; ui++)
	{
		for(unsigned long long uj=0;uj<binCount[1]; uj++)
		{
			for(unsigned long long uk=0;uk<binCount[2]; uk++)
			{
				if( getData(ui,uj,uk) < thresh)
					result.setData(ui,uj,uk,offThresh);
				else
				{
					result.setData(ui,uj,uk,onThresh);
				}
			}
		}
	}
}

template<class T>
void Voxels<T>::rebin(Voxels<T> &result, unsigned long long rate, unsigned long long clipDir) const
{
	//Check that the binsize can be reduced by this factor
	//or clipping allowed
	ASSERT(clipDir || ( !(binCount[0] % rate) && !(binCount[1] % rate)
						&& !(binCount[2] % rate)));

	//Datasets must be bigger than  their clipping direction
	ASSERT(binCount[0] > rate && binCount[1] && rate && binCount[2] > rate);

	
	unsigned long long newBin[3];
	newBin[0]=binCount[0]/rate;
	newBin[1]=binCount[1]/rate;
	newBin[2]=binCount[2]/rate;


	//Clipping not implmented!
	ASSERT(clipDir == CLIP_NONE);

	result.resize(newBin[0],newBin[1],newBin[2],getMinBounds(), getMaxBounds());
	//Draw a progress bar
	cerr << std::endl << "|";
	for(unsigned int ui=0; ui<100; ui++)
		cerr << ".";
	cerr << "| 100%" << std::endl << "|";

	unsigned long long its=0;
	unsigned int progress =0;
	float itsToDo = binCount[0]/rate;
#pragma omp parallel for shared(progress,its) 
	for(MP_INT_TYPE ui=0; ui<binCount[0]-rate; ui+=rate)
	{ 
#pragma omp critical
		{
			its++;
			//Update progress bar
			if(progress < (unsigned long long) ((float)its/itsToDo*100) )
			{
				cerr <<  ".";
				progress = (unsigned long long) ((float)its/itsToDo*100);
			}
		}

		for(unsigned long long uj=0; uj<binCount[1]-rate; uj+=rate)
		{

			for(unsigned long long uk=0; uk<binCount[2]-rate; uk+=rate)
			{
				double sum;
				sum=0;

				//Forgive the indenting. This is deep.
				for(unsigned long long uir=0; uir<rate; uir++)
				{
					for(unsigned long long ujr=0; ujr<rate; ujr++)
					{
						for(unsigned long long ukr=0; ukr<rate; ukr++)
						{
							ASSERT(ui+uir < binCount[0]);
							ASSERT(uj+ujr < binCount[1]);
							ASSERT(uk+ukr < binCount[2]);
							sum+=getData(ui+uir,uj+ujr,uk +ukr);
						}
					}
				}
				
			
				sum/=(double)(rate*rate*rate);

				result.setData(ui/rate,uj/rate,uk/rate,T(sum));
			}
		}
	}

	//Fill out the progress bar
	while(progress++ <100)
		cerr << ".";
	
	cerr << "| done" << std::endl;
	
	
}

template<class T>
void Voxels<T>::makeSphericalKernel(unsigned long long sideLen, float bound, const T &val, unsigned int antialiasLevel) 
{

	const unsigned long long halfLen = sideLen/2;
	float sqrSideLen=(float)halfLen*(float)halfLen;
	resize(sideLen,sideLen,sideLen,Point3D(bound,bound,bound));

	if(!antialiasLevel)
	{
		//Loop through the data
		//This could be sped up using decrementing whiles, 
		//at the cost of losing the openMP bit
#pragma omp parallel for shared(sqrSideLen)
		for(MP_INT_TYPE ui=0;ui<sideLen;ui++)
		{
			for(unsigned long long uj=0;uj<sideLen;uj++)
			{
				for(unsigned long long uk=0;uk<sideLen;uk++)
				{
					float offset;
					//Calcuate r^2
					offset = (ui-halfLen)*(ui-halfLen) + (uj-halfLen)*(uj-halfLen) + (uk-halfLen)*(uk-halfLen);

					if(offset < (float)sqrSideLen)
						setData(ui,uj,uk,val);
					else
						setData(ui,uj,uk,0);
				}
			}
		}
	
	}
	else
	{
		//We want an antialiased sphere. This is a little more tricky
#pragma omp parallel for shared(sqrSideLen)
		for(MP_INT_TYPE ui=0;ui<sideLen;ui++)
		{
			for(unsigned long long uj=0;uj<sideLen;uj++)
			{
				for(unsigned long long uk=0;uk<sideLen;uk++)
				{
					float offset[8],x,y,z;
					bool insideSphere,fullCalc;
					//Calcuate r^2 from sphere centre for each corner
					for(unsigned int off=0;off<8; off++)
					{
						x= ui + 2*(off &1)-1;
						y= uj + (off &2)-1;
						z= uk + 0.5*(off &4)-1;
						offset[off] =  (x - halfLen)*(x-halfLen) + (y-halfLen)*(y-halfLen)
								+ (z-halfLen)*(z-halfLen);
					}

					insideSphere= offset[0] < (float)sqrSideLen;
					fullCalc=false;

					//Spin through each to check if this is a border-straddling voxel
					for(unsigned int off=1;off<8; off++)
					{
						if( (offset[off] < (float)sqrSideLen && !insideSphere) ||
							(offset[off]  > (float)sqrSideLen && insideSphere))
						{
							//We have to do a full volume fraction
							//calculation to compute the value of this voxel
							fullCalc=true;
							break;
						}
					}


					if(fullCalc)
					{
						//Compute volume fraction

						//The method we use is a recursive divide & measure approach
						//We chop the voxel into eight half size voxels, then check 
						//to see which lie in the sphere, and which dont.
						std::stack<std::pair<Point3D, unsigned int > > positionStack;
						Point3D thisCentre;
						unsigned int thisLevel;
						float thisLen,value,x,y,z;
						value=0;
						//Push this voxel's 8 sub-voxel's centres onto the stack
						//to kick things off
						for(int off=0;off<8; off++) //CRITICAL that off is an int for bitmask
						{
							//the right hand part of the addition should
							//flip between -0.5 and +0.5 alternately
							x= ui + 0.5*(2*(off &1)-1);
							y= uj + 0.5*((off &2)-1);
							z= uk + 0.5*(0.5*(off &4)-1);

							positionStack.push(std::make_pair(Point3D(x,y,z),0));
						}

						//Level 0 corresponds to 1/8th of the original voxel. Level n = 1/(2^3(n+1)) 
						//each voxel has side lenght L_v = originalLen/(2^(level+1))
						while(positionStack.size())
						{
							thisCentre = positionStack.top().first;
							thisLevel = positionStack.top().second;
							positionStack.pop();

							
							//Side length for this voxel
							thisLen = pow(2.0,-((int)thisLevel+1))*halfLen;
							//Calculate r^2 from sphere centre for each corner
							for(int off=0;off<8; off++) //critical that off is int for bitmasking
							{
								x= thisCentre[0] + (2*(off &1)-1)*thisLen;
								y= thisCentre[1] + ((off &2)-1)*thisLen;
								z= thisCentre[2] + (0.5*(off &4)-1)*thisLen;

								offset[off] =  (x - halfLen)*(x-halfLen) + (y-halfLen)*(y-halfLen)
										+ (z-halfLen)*(z-halfLen);
							}

							insideSphere= offset[0] < (float)sqrSideLen;
							fullCalc=false;

							//Spin through each to check if this is a border-straddling voxel
							for(int off=1;off<8; off++)
							{
								if( (offset[off] < (float)sqrSideLen && !insideSphere) ||
									(offset[off]  > (float)sqrSideLen && insideSphere))
								{
									//We have to do a full volume fraction
									//calculation to compute the value of this voxel
									fullCalc=true;
									break;
								}
							}

							//OK, the action to take now depends on whether we need to subdivide or not
							if(thisLevel < antialiasLevel && fullCalc)
							{
								for(int off=0;off<8; off++) //Critical that off is signed for bitmask
								{
									x= thisCentre[0] + (2*(off &1)-1)*thisLen;
									y= thisCentre[1] + ((off &2)-1)*thisLen;
									z= thisCentre[2] + (0.5*(off &4)-1)*thisLen;

									positionStack.push(std::make_pair(Point3D(x,y,z),thisLevel+1));
								}
							}
							else
							{
								if(fullCalc)
								{
									//Lets just call it a half shall we
									value+=pow(2.0,-(((int)thisLevel+1)*3+1));

								}
								else
								{
									//Voxel is either wholly within, or without
									//If inside sphere, add to internal volume
									//otherwise we "add zero" - ie do nothing
									if(insideSphere)
										value+=pow(2.0,-((int)thisLevel+1)*3);
								}
							}
							
						}
					
				
						setData(ui,uj,uk,value*val);
					}
					else
						setData(ui,uj,uk,((int)insideSphere)*val);
				}
			}
		}
	}
	
}

template<class T>
void Voxels<T>::moravecHarrisFeatures(Voxels<float> &eigA, Voxels<float> &eigB,Voxels<float> &eigC) const
{
	//3D implementation of the moravec plessy edge detector
	eigA.resize(binCount[0]-2,binCount[1]-2,
			binCount[2]-2,getMinBounds(), getMaxBounds());
	eigB.resize(binCount[0]-2,binCount[1]-2,
			binCount[2]-2,getMinBounds(), getMaxBounds());
	eigC.resize(binCount[0]-2,binCount[1]-2,
			binCount[2]-2,getMinBounds(), getMaxBounds());

	float xPitch,yPitch,zPitch;

	xPitch=(maxBound[0]-minBound[0])/binCount[0];
	yPitch=(maxBound[1]-minBound[1])/binCount[1];
	zPitch=(maxBound[2]-minBound[2])/binCount[2];

	//Moravec derivative matrix
	gsl_matrix *M ;
	//Workspace for computing eigenvalues
	gsl_eigen_symm_workspace *w;
	//Vector for storing the eigenvalues
	gsl_vector *v;
	
	M=gsl_matrix_alloc(3,3);
	v=gsl_vector_alloc(3);
	w= gsl_eigen_symm_alloc(3);


	//NOTE: I have trouble using openmp to parallelise this
	//as the M,v and w variables need ot have their own storage
	//allocated. Could use arrays of M[],v[] and w[]  but I will worry about it another day
	
	//Calculate the directional derivatives
	float deriv[3];
	//Work out boundary
	for(unsigned long long ui=1; ui<binCount[0]-1; ui++)
	{
		for(unsigned long long uj=1; uj<binCount[1]-1; uj++)
		{
			for(unsigned long long uk=1; uk<binCount[2]-1; uk++)
			{
				
				//dx by first order central difference
				deriv[0]=T((getData(ui+1,uj,uk) + getData(ui-1,uj,uk))/(xPitch));
				
				//dy
				deriv[1]=T((getData(ui,uj+1,uk) + getData(ui,uj-1,uk))/(yPitch));
				
				//dz
				deriv[2]=T((getData(ui,uj,uk+1) + getData(ui,uj,uk-1))/(zPitch));

				//NOTE: Harris' paper 
				//(Harris, C. & Stephens, M. A combined corner and 
				//edge detector Alvey vision conference, 1998, 15, 50 )
				//suggests a way to not compute the eigenvalues 
				//and still have a reasonable detector. This is not implemented here, but could
				//be done for a speedup

				//Construct the matrix
				//Note that the matrix is symmetric,
				//so really we only construct the lower triangluar component
				gsl_matrix_set(M,0,0,(float)(deriv[0]*deriv[0]));
				gsl_matrix_set(M,1,0,(float)(deriv[1]*deriv[0]));
				gsl_matrix_set(M,1,1,(float)(deriv[1]*deriv[1]));
				gsl_matrix_set(M,2,0,(float)(deriv[2]*deriv[0]));
				gsl_matrix_set(M,2,1,(float)(deriv[2]*deriv[1]));
				gsl_matrix_set(M,2,2,(float)(deriv[2]*deriv[2]));
				

				//Find the eigenvalues
				//GSL reference manual: 
				//The diagonal and lower triangular part of A 
				//are destroyed during the computation, but the strict 
				//upper triangular part is not referenced.
				gsl_eigen_symm(M, v, w);
				
				eigA.setData(ui-1,uj-1,uk-1,gsl_vector_get(v,0));
				eigB.setData(ui-1,uj-1,uk-1,gsl_vector_get(v,1));
				eigC.setData(ui-1,uj-1,uk-1,gsl_vector_get(v,2));

			}
		}
	}

	gsl_eigen_symm_free(w);
	gsl_vector_free(v);
	gsl_matrix_free(M);
}

template<class T>
T Voxels<T>::min() const
{
	ASSERT(voxels.size());
	unsigned long long numPts = binCount[0]*binCount[1]*binCount[2];
	T minVal;
	minVal = voxels[0];
#ifdef _OPENMP
	//Unfortunately openMP doesn't lend itself well here
	//But we can code around it.
	unsigned long long maxThr = omp_get_max_threads();	
	T minArr[maxThr];
	//Init all mins
	for(unsigned long long ui=0; ui<maxThr; ui++)
		minArr[ui] = voxels[0];

	//parallel min search	
	#pragma omp parallel for shared(minArr)
	for(MP_INT_TYPE ui=0;ui<(MP_INT_TYPE)numPts; ui++)
		minArr[omp_get_thread_num()] = std::min(minArr[omp_get_thread_num()],voxels[ui]);	

	//find min of mins
	for(unsigned long long ui=0;ui<maxThr; ui++)
		minVal=std::min(minArr[ui],minVal);
#else

	for(unsigned long long ui=0;ui<numPts; ui++)
		minVal=std::min(minVal,voxels[ui]);
#endif
	return minVal;
}

template<class T>
T Voxels<T>::max() const
{
	ASSERT(voxels.size());
	unsigned long long numPts = binCount[0]*binCount[1]*binCount[2];
	T maxVal;
	maxVal = voxels[0];
#ifdef _OPENMP
	//Unfortunately openMP doesn't lend itself well here
	//But we can code around it.
	unsigned long long maxThr = omp_get_max_threads();	
	T maxArr[maxThr];
	//Init all maxs
	for(unsigned long long ui=0; ui<maxThr; ui++)
		maxArr[ui] = voxels[0];

	//parallel max search	
	#pragma omp parallel for shared(maxArr)
	for(MP_INT_TYPE ui=0;ui<(MP_INT_TYPE)numPts; ui++)
		maxArr[omp_get_thread_num()] = std::max(maxArr[omp_get_thread_num()],voxels[ui]);	

	//find max of maxs
	for(unsigned long long ui=0;ui<maxThr; ui++)
		maxVal=std::max(maxArr[ui],maxVal);
#else

	for(unsigned long long ui=0;ui<numPts; ui++)
		maxVal=std::max(maxVal,voxels[ui]);
#endif
	return maxVal;
}

template<class T>
void Voxels<T>::fillSpheresByPosition( const std::vector<Point3D> &spherePos, float rad, const T &value, bool doErase)
{

	//I haven't though this through for non cubic datasets
	ASSERT((maxBound[0]-minBound[0]) == (maxBound[1]-minBound[1]));
	ASSERT((maxBound[1]-minBound[1]) == (maxBound[2]-minBound[2]));
	ASSERT(binCount[0] == binCount[1]);
	ASSERT(binCount[1] == binCount[2]);

	//Size of rectangular box that contains the sphere
	unsigned long long numBlocks[3];
	numBlocks[0] = (unsigned long long)(2.0*rad*binCount[0]/(maxBound[0]-minBound[0]));
	numBlocks[1] = (unsigned long long)(2.0*rad*binCount[1]/(maxBound[1]-minBound[1]));
	numBlocks[2] = (unsigned long long)(2.0*rad*binCount[2]/(maxBound[2]-minBound[2]));

	//Construct the sphere kernel
	std::vector<int> vX,vY,vZ;
	float sqrRad=(float)numBlocks[0]*(float)numBlocks[0]/4.0f;
#pragma omp parallel for
	for(MP_INT_TYPE ui=0;ui<numBlocks[0]; ui++)
	{
		for(unsigned long long uj=0;uj<numBlocks[1]; uj++)
		{
			for(unsigned long long uk=0;uk<numBlocks[2]; uk++)
			{
				long long delta[3];
				float sqrDist;
				
				delta[0]=ui-numBlocks[0]/2;
				delta[1]=uj-numBlocks[1]/2;
				delta[2]=uk-numBlocks[2]/2;

				sqrDist=((float)delta[0]*(float)delta[0]) + 
					((float)delta[1]*(float)delta[1]) + 
					((float)delta[2]*(float)delta[2]);

				if(sqrDist < sqrRad)
				{
#pragma omp critical
					{
					vX.push_back(delta[0]);
					vY.push_back(delta[1]);
					vZ.push_back(delta[2]);
					}
				}
				
				
			}
		}
	}


	//Clear the dataset
	if(doErase)
		fill(T(0.0));
#pragma omp parallel for	
	for(MP_INT_TYPE ui=0; ui<spherePos.size(); ui++)
	{
		unsigned long long sphereCentre[3];
	
		getIndex(sphereCentre[0],sphereCentre[1],sphereCentre[2],spherePos[ui]);

		//Calculate the points that lie within the sphere
		for(unsigned long long uj=0;uj<vX.size(); uj++)
		{
			long long p[3];
			
			p[0] = sphereCentre[0] + vX[uj];
			p[1] = sphereCentre[1] + vY[uj];
			p[2] = sphereCentre[2] + vZ[uj];

			if(p[0] < binCount[0] && p[0] > 0 &&
				p[1] < binCount[1] && p[1] > 0 &&
				p[2] < binCount[2] && p[2] > 0)
			{	
#pragma omp critical
				setData(p[0],p[1],p[2],value);
			}
			
		}

	}
}


template<class T>
int Voxels<T>::countPoints( const std::vector<Point3D> &points, bool noWrap, bool doErase)
{
	if(doErase)
	{
		fill(0);	
	}

	unsigned long long x,y,z;

	unsigned int downSample=MAX_CALLBACK;
	for(unsigned long long ui=0; ui<points.size(); ui++)
	{
		if(!downSample--)
		{
			if(!(*callback)())
				return VOXEL_ABORT_ERR;
			downSample=MAX_CALLBACK;
		}
		
		T value;
		getIndex(x,y,z,points[ui]);

		//Ensure it lies within the dataset	
		if(x < binCount[0] && y < binCount[1] && z< binCount[2]
			&& x >= 0 && y >= 0 && z >= 0)
		{
			{
				value=getData(x,y,z)+T(1);

				//Prevent wrap-around errors
				if (noWrap) {
					if(value > getData(x,y,z))
						setData(x,y,z,value);
				} else {
					setData(x,y,z,value);
				}
			}
		}	
	}

	return 0;
}

template<class T>
int Voxels<T>::countPoints( const std::vector<IonHit> &points, bool noWrap, bool doErase)
{
	if (doErase)
		fill(0);

	unsigned long long x,y,z;

	unsigned int downSample=MAX_CALLBACK;
	for (unsigned long long ui=0; ui<points.size(); ui++)
	{
		if(!downSample--)
		{
			if(!(*callback)())
				return VOXEL_ABORT_ERR;
			downSample=MAX_CALLBACK;
		}
		T value;
		getIndex(x,y,z,points[ui].getPos());

		//Ensure it lies within the dataset
		if (x < binCount[0] && y < binCount[1] && z< binCount[2]
		        && x >= 0 && y >= 0 && z >= 0)
		{
			{
				value=getData(x,y,z)+T(1);

				//Prevent wrap-around errors
				if (noWrap) {
					if (value > getData(x,y,z))
						setData(x,y,z,value);
				} else {
					setData(x,y,z,value);
				}
			}
		}
	}
	return 0;
}

template<class T>
int Voxels<T>::calculateDensity()
{
	Point3D size = maxBound - minBound;
	// calculate the volume of a voxel
	double volume = 1.0;
	for (int i = 0; i < 3; i++)
		volume *= size[i] / binCount[i];
	
	// normalise the voxel value based on volume
#pragma omp parallel for
	for(unsigned long long ui=0; ui<voxels.size(); ui++) 
		voxels[ui] /= volume;

	return 0;
}

template<class T>
void Voxels<T>::getIndex(unsigned long long &x, unsigned long long &y,
	       			unsigned long long &z, const Point3D &p) const
{
	//ASSERT(p[0] >=0 && p[1] >=0 && p[2] >=0);


	ASSERT(p[0] >=minBound[0] && p[1] >=minBound[1] && p[2] >=minBound[2] &&
		   p[0] <=maxBound[0] && p[1] <=maxBound[1] && p[2] <=maxBound[2]);
	x=(unsigned long long)((p[0]-minBound[0])/(maxBound[0]-minBound[0])*(float)binCount[0]);
	y=(unsigned long long)((p[1]-minBound[1])/(maxBound[1]-minBound[1])*(float)binCount[1]);
	z=(unsigned long long)((p[2]-minBound[2])/(maxBound[2]-minBound[2])*(float)binCount[2]);
}

template<class T>
void Voxels<T>::fill(const T &v)
{
	unsigned long long nBins = binCount[0]*binCount[1]*binCount[2];

#pragma omp parallel for
	for(MP_INT_TYPE ui=0;ui<(MP_INT_TYPE)nBins; ui++)
		voxels[ui]=v;
}


template<class T>
int Voxels<T>::histogram(std::vector<unsigned long long> &v, unsigned long long histBinCount) const
{

	T maxVal=max();
	T minVal=min();
	
#ifdef _OPENMP
	//We need an array for each thread to store results
	unsigned long long *vals;

	vals = new unsigned long long[omp_get_max_threads()*histBinCount];

	//Zero out the array
	for(unsigned long long ui=0;ui<omp_get_max_threads()*histBinCount; ui++)
		*(vals+ui)=0;
	

	bool spin=true;
	//Do it -- loop through, writing to each threads "scratch pad"	
	//to prevent threads interfering with one another
#pragma omp parallel for
	for(MP_INT_TYPE ui=0; ui<binCount[0]; ui++)
	{
		if(spin)
			continue;
		unsigned long long *basePtr;
		unsigned long long offset;
		basePtr=vals + omp_get_thread_num()*histBinCount;
		for(unsigned long long uj=0; uj <binCount[1]; uj++)
		{
			for(unsigned long long uk=0; uk <binCount[2]; uk++)
			{
				offset = (getData(ui,uj,uk)-minVal)/(maxVal -minVal)*(histBinCount-1);
				
				basePtr[offset]++;
			}
		}
#pragma omp critical
		{
		if(!(*callback)())
			spin=true;
		}

	}

	if(spin)
		return VOXEL_ABORT_ERR;
	//Merge the values
	v.resize(histBinCount);
	for(unsigned long long thr=0;thr<omp_get_max_threads(); thr++)
	{
		unsigned long long *basePtr;
		basePtr=vals + thr*histBinCount;
		for(unsigned long long ui=0;ui<histBinCount; ui++)
		{
			v[ui]+=basePtr[ui];
		}
	}

#else
	//We need an array for each thread to store results
	unsigned long long *vals=new unsigned long long[histBinCount];

	for(unsigned long long ui=0;ui<histBinCount; ui++)
		vals[ui]=0;
	unsigned long long bc;
	bc=binCount[0];
	bc*=binCount[1];
	bc*=binCount[2];
	for(unsigned long long ui=0; ui <bc; ui++)
	{
		unsigned long long offset;
		offset = (unsigned long long)((getData(ui)-minVal)/(maxVal -minVal)*histBinCount);
		vals[offset]++ ;
	}

	v.resize(histBinCount);
	for(unsigned long long ui=0;ui<histBinCount; ui++)
		v[ui]=vals[ui];
		
#endif
	delete[] vals;	


	ASSERT(v.size() == histBinCount);

	return 0;	
}

template<class T>
void Voxels<T>::findSorted(std::vector<unsigned long long> &x, std::vector<unsigned long long> &y, 
			std::vector<unsigned long long> &z, unsigned long long n, bool largest) const
{
	//Could be better if we didn't use indexed data aquisition (record opsition)
	std::deque<unsigned long long> bSx,bSy,bSz;

	if(!voxels.size())
		return;
	
	T curBest;
	curBest=getData(0);
	
	//It is theoretically possible to rewrite this without locking (critical sections).
	if(largest)
	{
		
		for(unsigned long long ui=0;ui<binCount[0]; ui++)
		{
			for(unsigned long long uj=0;uj<binCount[1]; uj++)
			{
				for(unsigned long long uk=0;uk<binCount[2]; uk++)
				{
					
					if ( getData(ui,uj,uk) > curBest)
					{
#pragma omp critical 
						{
						bSx.push_front(ui);
						bSy.push_front(uj);
						bSz.push_front(uk);
						curBest=getData(ui,uj,uk);
						}
					}
					
				}
			}
		}
		
	}
	else
	{
		for(unsigned long long ui=0;ui<binCount[0]; ui++)
		{
			for(unsigned long long uj=0;uj<binCount[1]; uj++)
			{
				for(unsigned long long uk=0;uk<binCount[2]; uk++)
				{
					
					if ( getData(ui,uj,uk) < curBest)
					{
#pragma omp critical 
						{
						
						bSx.push_front(ui);
						bSy.push_front(uj);
						bSz.push_front(uk);
						curBest=getData(ui,uj,uk);

						}
						
					}
					
				}
			}
		}
		
	}
	

	ASSERT(bSx.size() == bSy.size() == bSz.size());

	x.resize(n);
	y.resize(n);
	z.resize(n);
	while(n--)
	{
		x[n] =bSx.back(); 
		y[n] =bSy.back(); 
		z[n] =bSz.back(); 
	
		bSx.pop_back();	
		bSy.pop_back();	
		bSz.pop_back();	
	}
}

template<class T>
void Voxels<T>::operator/=(const Voxels<T> &v)
{
	ASSERT(v.voxels.size() == voxels.size());
	for (size_t i = 0; i < voxels.size(); i++)
	{
		if(v.voxels[i])
			voxels[i]/=(v.voxels[i]);
		else
			voxels[i] =0;
	}
}	
//===



#endif
