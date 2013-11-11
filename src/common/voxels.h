 /*
 * common/voxels.h - Voxelised data manipulation class
 * Copyright (C) 2013  D. Haley
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


const unsigned int MAX_CALLBACK=500;

#include "common/basics.h"

#include <stack>

using namespace std;


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
	BOUND_MIRROR,
	BOUND_ZERO,
	BOUND_ENUM_END
};

//Interpolation mode for slice 
enum
{
	SLICE_INTERP_NONE,
	SLICE_INTERP_LINEAR,
	SLICE_INTERP_ENUM_END
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
	VOXELS_OUT_OF_MEMORY
};

//Must be power of two (buffer size when loading files, in sizeof(T)s)
const unsigned int ITEM_BUFFER_SIZE=65536;

//!Clipping direction constants
/*! Controls the clipping direction when performing clipping operations
 */
enum {
	CLIP_NONE=0,
       CLIP_LOWER_SOUTH_WEST,
       CLIP_UPPER_NORTH_EAST
};


enum {
	VOXEL_ABORT_ERR,
	VOXEL_MEMORY_ERR,
	VOXEL_BOUNDS_INVALID_ERR
};

#ifdef _OPENMP
//Auto-detect the openMP iterator type.
#if ( __GNUC__ > 4 && __GNUC_MINOR__ > 3 )
	#define MP_INT_TYPE size_t
#else
	#define MP_INT_TYPE long long 
#endif
#else
	#define MP_INT_TYPE size_t
#endif

#ifdef DEBUG
	bool runVoxelTests();
#endif
//!Template class that stores 3D voxel data
/*! To instantiate this class, objects must have
 * basic mathematical operators, such as * + - and =
 */
template<class T> class Voxels
{
	private:
		//!Number of bins in data set (X,Y,Z)
		size_t binCount[3];

		//!Voxel array 
		std::vector<T> voxels;
		
		//!Scaling value for furthest bound of the dataset. 
		//Dataset is assumed to sit in a rectilinear volume from minBound
		//to maxBound
		Point3D minBound, maxBound;
	
		bool (*callback)(bool);
			

		void localPaddedConvolve(long long ui,long long uj, long long uk, 
			const Voxels<T> &kernel,Voxels<T> &result, unsigned int mode) const;
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
		void getIndex(size_t &x, size_t &y,
				size_t &z, const Point3D &p) const;
		
		//!Retrieve the XYZ voxel location associated with a given position,
		// including upper borders
		void getIndexWithUpper(size_t &x, size_t &y,
				size_t &z, const Point3D &p) const;
		
		//!Get the position associated with an XYZ voxel
		Point3D getPoint(size_t x, 
				size_t y, size_t z) const;
		//!Retrieve the value of a specific voxel
		inline T getData(size_t x, size_t y, size_t z) const;
		//!Retrieve value of the nth voxel
		inline T getData(size_t i) const { return voxels[i];}
		//Return a padded version of the data. Valid pads are BOUND_*
		T getPaddedData(long long x, long long y, long long z, 
						unsigned int padMode) const;


		void setEntry(size_t n, const T &val) { voxels[n] = val;};
		//!Retrieve a reference to the data ata  given position
		//const T &getDataRef(size_t x, size_t y, size_t z) const;
		//!Set the value of a point in the dataset
		void setData(size_t x, size_t y, size_t z, const T &val);
		//!Set the value of nth point in the dataset
		void setData(size_t n, const T &val);


		//get an interpolated slice from a section of the data
		void getSlice(size_t normal, float offset, T *p, 
			size_t interpMode=SLICE_INTERP_NONE, size_t boundMode=BOUND_MIRROR) const;

		//Get a specific slice, from an integral offset in the data
		void getSlice(size_t normal, size_t offset, 
					T *p,size_t boundMode) const;
		//!Get the size of the data field
		void getSize(size_t &x, size_t &y, size_t &z) const;
		size_t getSize() const {return voxels.size();};

		//!Resize the data field
		/*! This will destroy any data that was already in place
		 * If the data needs to be preserved use "resizeKeepData"
		 * Data will *not* be initialised
		 */
		size_t resize(size_t newX, size_t newY, size_t newZ, const Point3D &newMinBound=Point3D(0.0f,0.0f,0.0f), const Point3D &newMaxBound=Point3D(1.0f,1.0f,1.0f));
	
		size_t resize(const Voxels<T> &v);

		//!Resize the data field, maintaining data as best as possible
		/*! This will preserve data by resizing as much as possible 
		 * about the origin. If the bounds are extended, the "fill" value is used
		 * by default iff doFill is set to true. 
		 */
		size_t resizeKeepData(size_t newX, size_t newY, size_t newZ, 
					unsigned int direction=CLIP_LOWER_SOUTH_WEST, const Point3D &newMinBound=Point3D(0.0f,0.0f,0.0f), const Point3D &newMaxBound=Point3D(1.0f,1.0f,1.0f), const T &fill=T(0),bool doFill=false);



		//!Get a unique integer that corresponds to the edge index for the voxel; where edges are shared between voxels
		/*! Each voxel has 12 edges. These are shared (except
		 * voxels that on zero or positive boundary). Return a
	 	 * unique index that corresponds to a specified edge (0->11)
		 */
		size_t getEdgeIndex(size_t x,size_t y, size_t z, unsigned int edge) const;

	
		//!Convert the edge index (as generated by getEdgeIndex) into a cenre position	
		// returns the axis value so you know edge vector too.
		// NOte that the value to pass as the edge index is (getEdgeIndex>>2)<<2 to
		// make the ownership of the voxel correct 
		void getEdgeEnds(size_t edgeIndex,Point3D &a, Point3D &b) const;



		//TODO: there is duplicate code between this and getEdgeEnds. Refactor.
		//!Return the values that are associated with the edge ends, as returned by getEdgeEnds
		void getEdgeEndApproxVals(size_t edgeUniqId, float &a, float &b ) const;


		//!Rebin the data by a given rate
		/*! This will perform a quick and dirty rebin operation, where groups of datablocks 
		 * are binned into a single cell. Number of blocks binned is rate^3. Field must be larger than rate
		 * in all directions. Currently only CLIP_NONE is supported.
		 */
		void rebin(Voxels<T> &dest, size_t rate, size_t clipMode=CLIP_NONE) const;
		
		//!Get the total value of the data field.
		/*! An "initial value" is provided to provide the definition of a zero value
		 */
		T getSum(const T &initialVal=T(0.0)) const;

		//!count the number of cells with at least this intensity
		size_t count(const T &minIntensity) const;

		//!Fill all voxels with a given value
		void fill(const T &val);
		//!Get the bounding size
		Point3D getMinBounds() const;
		Point3D getMaxBounds() const;
		//Obtain the ounds for a specified axis
		void getAxisBounds(size_t axis, float &minV, float &maxV) const;
		///! Get the spacing for a unit cell
		Point3D getPitch() const;
		//!Set the bounding size
		void setBounds(const Point3D &pMin, const Point3D &pMax);
		//!Get the bounding size
		void getBounds(Point3D &pMin, Point3D &pMax) const { pMin=minBound;pMax=maxBound;}

		//!Initialise the voxel storage
		size_t init(size_t nX,size_t nY,size_t nZ, const BoundCube &bound);
		//!Initialise the voxel storage
		size_t init(size_t nX,size_t nY, size_t nZ);
		//!Load the voxels from file
		/*! Format is flat size_ts in column major
		 * return codes:
		 * 1: File open error 
		 * 2: Data size error
		 */
		size_t loadFile(const char *cpFilename, size_t nX,
						size_t nY, size_t nZ, bool silent=false);
		//!Write the voxel objects in column major written out to file
		/*! Format is flat objects ("T"s) in column major format.
		 * Returns nonzero on failure
		 */
		size_t writeFile(const char *cpFilename) const;

		//!Run convolution over this data, placing the correlation data into "result"
		/*! 
		 * Datasets MUST have the same pitch (spacing) for the result to be defined
		 * template type must have a  T(0.0) constructor that intialises it to some "zero"
		 */
		size_t convolve(const Voxels<T> &templateVec, Voxels<T> &result,
							 size_t boundMode=BOUND_CLIP) const; 

		
		//!Similar to convolve, but faster -- only works with separable kernels.
		//Destroys original data in process.
		/*! 
		 * Datasets MUST have the same pitch (spacing) for the result to be defined
		 * template type must have a  T(0.0) constructor that intialises it to some "zero"
		 */
		size_t separableConvolve(const Voxels<T> &templateVec, Voxels<T> &result,
							 size_t boundMode=BOUND_CLIP); 
		
		//!Set this object to a normalised gaussian kernel, centered around the middle of the dataset
		void setGaussianKernelCube(float stdDev, float bound, size_t sideLen);
		
		//!Second derivative by difference approximation 
		/*! Returns the first order central difference approximation to the
		 * second derivative. Bound mode can (when implemented) bye used to compute
		 * appropriate differences.
		 */
		void secondDifference(Voxels<T> &result, size_t boundMode=BOUND_CLIP) const;


		//!Find the positions of the voxels that are above or below a given value
		/*! Returns the positions of the voxels' centroids for voxels that have, by default,
		 * a value greater than that of thresh. This behaviour can by reversed to "lesser than"
		 * by setting lowerEq to false
		 */
		void thresholdForPosition(std::vector<Point3D> &p, const T &thresh, bool lowerEq=false) const;



		//!Construct a spherical kernel
		/* The spherical result is located at the centre of the voxel set
		 * sideLen -The side length is the diameter of the sphere
		 * bound -the bounding value that the voxels contain themselves within
		 * val - the value that the voxels take on for a full sphere.
		 * antialiasingLevel - the number of times to subdivide the voxels into 8 parts 
		 *  and test these voxels for internal/external. The resultant fraction of  inside/outside voxels is summed.
		 */
		void makeSphericalKernel(size_t sideLen, float bound, const T &val, unsigned int antialiasLevel=0);

		//!Return the sizeof value for the T type
		/*! Maybe there is a better way to do this, I don't know
		 */
		static size_t sizeofType() { return sizeof(T);}; 


		//!Binarise the data into a result vector
		/* On thresholding (val > thresh), set the value to "onThresh". 
		 * Otherwise set to "offthresh"
		 */
		void binarise(Voxels<T> &result,const T &thresh, const T &onThresh, 
				const T &offThresh) const;
	

		//!Empty the data
		/*Remove the data from the voxel set
		 */
		void clear() { voxels.clear();};

		//!Find minimum in dataset
		T min() const;

		//!Find maximum in dataset
		T max() const;	
		
		//!Find both min and max in dataset in the same loop
		void minMax(T &min, T &max) const;	

		//!Given some coordinates and radii, generate a voxel dataset of solid spheres superimposed
		/*! The radius specified is in the bounding units
		 *  clear value optionally wipes the dataset before continuing 
		 */
		void fillSpheresByPosition( const std::vector<Point3D> &spherePos, float rad, const T &value, bool doErase=true);


		//!Generate a histogram of data values. / operator must be defined for target type
		/*! Data bins are evenly spaced between min and max. Formula for binning is (val-min())/(max()-min())*histBinCount
		 */
		int histogram(std::vector<size_t> &v, size_t histBinCount) const;

		//!Find the largest or smallest n objects
		void findNExtrema(std::vector<size_t> &x, std::vector<size_t> &y, std::vector<size_t> &z, size_t n, bool largest=true) const;

		//!Generate a dataset that consists of the counts of points in a given voxel
		/*! Ensure that the voxel scaling factors 
		 * are set by calling ::setBounds() with the 
		 * appropriate parameters for your data.
		 * Disabling nowrap allows you to "saturate" your
		 * data field in the case of dense regions, rather 
		 * than let wrap-around errors occur
		 */
		int countPoints( const std::vector<Point3D> &points, bool noWrap=true, bool doErase=false);

		//!Integrate the datataset via the trapezoidal method
		T trapezIntegral() const;	
		//! Convert voxel intensity into voxel density
		// this is done by dividing each voxel by its volume 
		void calculateDensity();

		float getBinVolume() const;
		//!increment the position specified
		inline void increment(size_t x, size_t y, size_t z){
			//Typecast everything to at least 64 bit uints.
			voxels[(size_t)z*(size_t)binCount[1]*(size_t)binCount[0]
				+(size_t)y*(size_t)binCount[0] + (size_t)x]++;}
	
		void setCallbackMethod(bool (*cb)(bool)) {callback = cb;}

		//!Element wise division	
		void operator/=(const Voxels<T> &v);

		void operator/=(const T &v);
		
		bool operator==(const Voxels<T> &v) const;

};

//!Convert one type of voxel into another by assignment operator
template<class T, class U>
void castVoxels(const Voxels<T> &src, Voxels<U> &dest)
{
	size_t x,y,z;

	//Resize the dest
	src.getSize(x,y,z);
	
	dest.resize(x,y,z,src.getBounds());

	size_t numEntries=x*y*z;
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
	size_t nx,ny,nz;

	src.getSize(nx,ny,nz);
	
	counter=0;
	for(size_t ui=0; ui<nx; ui++)
	{
		for(size_t uj=0; uj<ny; uj++)	
		{
			for(size_t uk=0; uk<nz; uk++)
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
	newVox.minBound=minBound;
	newVox.maxBound=maxBound;

	std::copy(voxels.begin(),voxels.end(),newVox.voxels.begin());

}

template<class T>
void Voxels<T>::setPoint(const Point3D &point,const T &val)
{
	ASSERT(voxels.size());
	size_t pos[3];
	for(size_t ui=0;ui<3;ui++)
		pos[ui] = (size_t)round(point[ui]*(float)binCount[ui]);


	voxels[pos[2]*binCount[1]*binCount[0] + pos[1]*binCount[0] + pos[0]]=val; 
}

template<class T>
void Voxels<T>::setData(size_t x, size_t y, 
			size_t z, const T &val)
{
	ASSERT(voxels.size());

	ASSERT( x < binCount[0] && y < binCount[1] && z < binCount[2]);
	voxels[z*binCount[1]*binCount[0] + y*binCount[0] + x]=val; 
}

template<class T>
inline void Voxels<T>::setData(size_t n, const T &val)
{
	ASSERT(voxels.size());
	ASSERT(n<voxels.size());

	voxels[n] =val; 
}

template<class T>
T Voxels<T>::getPointData(const Point3D &point) const
{
	ASSERT(voxels.size());
	size_t pos[3];
	for(size_t ui=0;ui<3;ui++)
		pos[ui] = (size_t)round(point[ui]*(float)binCount[ui]);

	return voxels[pos[2]*binCount[1]*binCount[0] + pos[1]*binCount[0] + pos[0]]; 
}

template<class T>
Point3D Voxels<T>::getPoint(size_t x, size_t y, size_t z) const
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
void Voxels<T>::getSize(size_t &x, size_t &y, size_t &z) const
{
	ASSERT(voxels.size());
	x=binCount[0];
	y=binCount[1];
	z=binCount[2];
}


template<class T>
size_t Voxels<T>::getEdgeIndex(size_t x,size_t y, size_t z, unsigned int index) const
{
	//This provides a reversible mapping of x,y,z 
	//X aligned edges are first
	//Y second
	//Z third
	

	//Consider each parallel set of edges (eg all the X aligned edges)
	//to be the dual grid of the actual grid. From this you can visualise the
	//cell centres moving -1/2 -/12 units in the direction normal to the edge direction
	//to produce the centres of the edge. An additional vertex needs to be created at
	//the end of each dimension not equal to the alignement dim.


	//  		    ->ASCII ART TIME<-
	// In each individual cube, the offsets look like this:
        //		------------7-----------
        //		\		       |\ .
        //		|\ 		       | \ .
        //		| 10		       |  11
        //		|  \		       |   \ .
        //		|   \		       |    \ .
        //		|    \ --------6-------------|
        //		|     |                |     |
        //	      2 |                3     |     |
        //		|     |                |     |
        //		|     |                |     |
        //		|     |                |     |
        //		|     0                |     1
        //		\-----|----5-----------      |
        //	 	 \    |                 \    |
        //	 	  8   |                  9   |
        //	 	   \  |                   \  |
        //		    \ |---------4------------
	//
	//   	^x
	//  z|\	|
	//    \ |
	//     \-->y
	//

	switch(index)
	{
		//X axis aligned
		//--
		case 0:
			break;	
		case 1:
			y++; // one across in Y
			break;	
		case 2:
			z++;//One across in Z
			break;	
		case 3:
			y++;
			z++;
			break;	
		//--

		//Y Axis aligned
		//--
		case 4:
			break;	
		case 5:
			z++;
			break;	
		case 6:
			x++;
			break;	
		case 7:
			z++;
			x++;
			break;	
		//--
		
		//Z Axis aligned
		//--
		case 8:
			break;	
		case 9:
			y++;
			break;	
		case 10:
			x++;
			break;	
		case 11:
			x++;
			y++;
			break;	
		//--
	}


	size_t result = 12*(z + y*(binCount[2]+1) + x*(binCount[2]+1)*(binCount[1]+1)) +
	index;
	
	return result;

}

template<class T>
void Voxels<T>::getEdgeEnds(size_t edgeUniqId, Point3D &a, Point3D &b ) const
{
	//Invert the mapping generated by the edgeUniqId function
	//to retrieve the XYZ and axis values
	size_t x,y,z;

	int index; // Per voxel edge number. See ascii art in getEdgeidx
	index=edgeUniqId %12;

	//Drop the non-owner part of the voxel
	index/=4;
	index*=4;

	size_t tmp;
	tmp=edgeUniqId;
	tmp/=12; // shift out the index multiplier

	x = tmp/((binCount[2]+1)*(binCount[1]+1));
	tmp-=x*((binCount[2]+1)*(binCount[1]+1));

	y=tmp/(binCount[2]+1);
	tmp-=y*(binCount[2]+1);

	z=tmp;




	ASSERT(x< binCount[0]+1 && y<binCount[1]+1 && z<binCount[2]+1);

	Point3D delta=getPitch();
	Point3D cellCentre=getPoint(x,y,z);
	

	//Generate ends of edge, as seen in ascii diagram in uniqueID
	switch(index)
	{
		case 0:
			//|| x, lo y, lo z.	
			a=cellCentre;
			b=cellCentre + Point3D(delta[0],0,0);
			break;

		case 4:
			//|| y, low x, low z.	
			a=cellCentre;
			b=cellCentre + Point3D(0,delta[1],0);
			break;
		case 8:
			//|| z, lo x, lo y.	
			a=cellCentre; 
			b=cellCentre + Point3D(0,0,delta[2]);
			break;
		default:
			ASSERT(false);
	}


#ifdef DEBUG
	BoundCube bc;
	bc.setBounds(getMinBounds(), getMaxBounds());
	bc.expand(sqrt(std::numeric_limits<float>::epsilon()));
	ASSERT(bc.containsPt(a) && bc.containsPt(b));
#endif
}

template<class T>
void Voxels<T>::getEdgeEndApproxVals(size_t edgeUniqId, float &a, float &b ) const
{
	//Invert the mapping generated by the edgeUniqId function
	//to retrieve the XYZ and axis values
	size_t x,y,z;

	int index; // Per voxel edge number. See ascii art in getEdgeidx
	index=edgeUniqId %12;

	size_t tmp;
	tmp=edgeUniqId;
	tmp/=12; // shift out the index multiplier

	x = tmp/((binCount[2]+1)*(binCount[1]+1));
	tmp-=x*((binCount[2]+1)*(binCount[1]+1));

	y=tmp/(binCount[2]+1);
	tmp-=y*(binCount[2]+1);

	z=tmp;


	ASSERT(x< binCount[0]+1 && y<binCount[1]+1 && z<binCount[2]+1);


	//undo the index shuffle from getEdgeIndex
	switch(index)
	{
		//X axis aligned
		//--
		case 0:
			break;	
		case 1:
			y--; // one across in Y
			break;	
		case 2:
			z--;//One across in Z
			break;	
		case 3:
			y--;
			z--;
			break;	
		//--

		//Y Axis aligned
		//--
		case 4:
			break;	
		case 5:
			z--;
			break;	
		case 6:
			x--;
			break;	
		case 7:
			z--;
			x--;
			break;	
		//--
		
		//Z Axis aligned
		//--
		case 8:
			break;	
		case 9:
			y--;
			break;	
		case 10:
			x--;
			break;	
		case 11:
			x--;
			y--;
			break;	
		//--
	}
	//The values used here are for the "dual" grid 
	//of voxels centres. So there are at most 2
	//voxels sharing an edge	
	switch(index)
	{
		case 0:
		{
			//Up we go
			a= getData(x,y,z);
			b= getData(x+1,y,z);
			break;
		}
		case 1:
		{
			//
			a= getData(x,y+1,z);
			b= getData(x+1,y+1,z);
			
			break;
		}
		case 2:
		{
			a= getData(x,y,z+1);
			b= getData(x+1,y,z+1);
			
			break;
		}
		case 3:
		{
			//
			a= getData(x,y+1,z+1);
			b= getData(x+1,y+1,z+1);
			
			break;
		}
		case 4:
		{
			a= getData(x,y,z);
			b= getData(x,y+1,z);
			//
			
			break;
		}
		case 5:
		{
			a= getData(x,y,z+1);
			b= getData(x,y+1,z+1);
			
			break;
		}
		case 6:
		{
			a= getData(x+1,y,z);
			b= getData(x+1,y+1,z);
			
			break;
		}
		case 7:
		{
			a= getData(x+1,y,z+1);
			b= getData(x+1,y+1,z+1);
			
			break;
		}
		case 8:
		{
			//
			a= getData(x,y,z);
			b= getData(x,y,z+1);
			
			break;
		}
		case 9:
		{
			//
			a= getData(x,y+1,z);
			b= getData(x,y+1,z+1);
			
			break;
		}
		case 10:
		{
			a= getData(x+1,y,z);
			b= getData(x+1,y,z+1);
			//
			
			break;
		}
		case 11:
		{
			a= getData(x+1,y+1,z);
			b= getData(x+1,y+1,z+1);
			
			break;
		}
		default:
			ASSERT(false);
			
	}	

}

template<class T>
void Voxels<T>::getAxisBounds(size_t axis, float &minV, float &maxV ) const
{
	maxV=maxBound[axis];
	minV=minBound[axis];
}

template<class T>
size_t Voxels<T>::resize(size_t x, size_t y, size_t z, const Point3D &newMinBound, const Point3D &newMaxBound) 
{
	voxels.clear();

	binCount[0] = x;
	binCount[1] = y;
	binCount[2] = z;


	minBound=newMinBound;
	maxBound=newMaxBound;

	size_t binCountMax = binCount[0]*binCount[1]*binCount[2];
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
size_t Voxels<T>::resize(const Voxels<T> &oth)
{
	return resize(oth.binCount[0],oth.binCount[1],oth.binCount[2],oth.minBound,oth.maxBound);
}

template<class T>
size_t Voxels<T>::resizeKeepData(size_t newX, size_t newY, size_t newZ, 
			unsigned int direction, const Point3D &newMinBound, const Point3D &newMaxBound, const T &fill,bool doFill)
{

	ASSERT(direction==CLIP_LOWER_SOUTH_WEST);
	ASSERT(callback);

	Voxels<T> v;
	
	if(v.resize(newX,newY,newZ))
		return 1;

	switch(direction)
	{
		case CLIP_LOWER_SOUTH_WEST:
		{
			minBound=newMinBound;
			maxBound=newMaxBound;

			if(doFill)
			{
				size_t itStop[3];
				itStop[0]=std::min(newX,binCount[0]);
				itStop[1]=std::min(newY,binCount[1]);
				itStop[2]=std::min(newZ,binCount[2]);

				size_t itMax[3];
				itMax[0]=std::max(newX,binCount[0]);
				itMax[1]=std::max(newY,binCount[1]);
				itMax[2]=std::max(newZ,binCount[2]);
				//Duplicate into new value, if currently inside bounding box
				//This logic will be a bit slow, owing to repeated "if"ing, but
				//it is easy to understand. Other logics would have many more
				//corner cases
				bool spin=false;
#pragma omp parallel for
				for(size_t ui=0;ui<itMax[0];ui++)
				{
					if(spin)
						continue;

					for(size_t uj=0;uj<itMax[1];uj++)
					{
						for(size_t uk=0;uk<itMax[2];uk++)
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
					if(!(*callback)(false))
						spin=true;
					}
				}

				if(spin)
					return VOXEL_ABORT_ERR;
			}
			else
			{
				//Duplicate into new value, if currently inside bounding box
				bool spin=false;
#pragma omp parallel for
				for(size_t ui=0;ui<newX;ui++)
				{
					if(spin)
						continue;

					for(size_t uj=0;uj<newY;uj++)
					{
						for(size_t uk=0;uk<newZ;uk++)
							v.setData(ui,uj,uk,getData(ui,uj,uk));
					}

#pragma omp critical
					{
					if(!(*callback)(false))
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

	swap(v);
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
size_t Voxels<T>::init(size_t nX, size_t nY,
	       				size_t nZ, const BoundCube &bound)
{
	binCount[0]=nX;
	binCount[1]=nY;
	binCount[2]=nZ;

	typedef size_t ull;
	ull binCountMax;
       
	ASSERT(bound.isValid())
	bound.getBounds(minBound, maxBound);
	binCountMax= (ull)binCount[0]*(ull)binCount[1]*(ull)binCount[2];

	voxels.resize(binCountMax);

#pragma omp parallel for
	for(size_t ui=0; ui<binCountMax; ui++) 
		voxels[ui]=0;

	return 0;
}

template<class T>
size_t Voxels<T>::init(size_t nX, size_t nY, size_t nZ)

{
	Point3D pMin(0,0,0), pMax(nX,nY,nZ); 

	return init(nX,nY,nZ,pMin,pMax);
}

template<class T>
size_t Voxels<T>::loadFile(const char *cpFilename, size_t nX, size_t nY, size_t nZ , bool silent)
{
	std::ifstream CFile(cpFilename,std::ios::binary);

	if(!CFile)
		return VOXELS_BAD_FILE_OPEN;
	
	CFile.seekg(0,std::ios::end);
	
	
	size_t fileSize = CFile.tellg();
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
		while((size_t)CFile.tellg() <= fileSize-curBufferSize)
		{
			//Update progress bar
			if(!silent && ((unsigned int)(((float)CFile.tellg()*100.0f)/(float)fileSize) > lastFrac))
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
			for(size_t position=0; position<curBufferSize; position+=(sizeof(T)))
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
size_t Voxels<T>::writeFile(const char *filename) const
{

	ASSERT(voxels.size())

	std::ofstream file(filename, std::ios::binary);

	if(!file)
		return 1;

	
	for(size_t ui=0; ui<binCount[0]*binCount[1]*binCount[2]; ui++)
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
		for(size_t uj=0; uj<binCount[1]; uj++ )
		{
			for(size_t uk=0; uk<binCount[1]; uk++) 
			{
				val=getData(ui,uj,uk);	
				returnVal+=val; 
			}
		}
	}

	return returnVal;
}

template<class T>
T Voxels<T>::trapezIntegral() const
{
	//Compute volume prefactor - volume of cube of each voxel
	//--
	float prefactor=1.0;
	for(size_t ui=0;ui<3;ui++)
	{
		prefactor*=(maxBound[ui]-
			minBound[ui])/
			(float)binCount[ui];
	}

	//--


	double accumulation(0.0);
	//Loop across dataset integrating along z direction
#pragma omp parallel for reduction(+:accumulation)
	for(size_t ui=0;ui<voxels.size(); ui++)
		accumulation+=voxels[ui];

	return prefactor*accumulation;
}


template<class T>
size_t Voxels<T>::count(const T &minIntensity) const
{
	size_t bins;
	bins=binCount[0]*binCount[1]*binCount[2];

	size_t sum=0;
#pragma omp parallel for reduction(+:sum)
	for(size_t ui=0;ui<bins; ui++)
	{
		if(voxels[ui]>=minIntensity)
			sum++;
	}

	return sum;
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

//Helper function for performing convolution
template<class T>
void Voxels<T>::localPaddedConvolve(long long ui,long long uj, long long uk, 
			const Voxels<T> &kernel,Voxels<T> &result, unsigned int mode) const
{
	ASSERT(result.binCount[0] == binCount[0] && result.binCount[1]==binCount[1]
		&& result.binCount[2] == binCount[2]);

	long long halfX,halfY,halfZ;
#ifdef DEBUG
	for(size_t i=0;i<3;i++)
	{
		ASSERT(kernel.binCount[i] &1);
	}
#endif
	halfX=kernel.binCount[0]/2; halfY=kernel.binCount[1]/2;halfZ=kernel.binCount[2]/2;

	T tally(0.0);
	for(size_t ul=0; ul<kernel.binCount[0]; ul++)
	{
	for(size_t um=0; um<kernel.binCount[1]; um++)
	{
	for(size_t un=0; un<kernel.binCount[2]; un++)
	{
		tally+=getPaddedData(ui+ul-halfX,uj+um-halfY,uk+un-halfZ,mode)*
				kernel.getData(ul,um,un);
	}
	}
	}

	result.setData(ui,uj,uk,tally);	
}

template<typename T>
size_t Voxels<T>::convolve(const Voxels<T> &kernel, Voxels<T> &result,  size_t boundMode)  const
{

	//Check the kernel can fit within this est
	size_t x,y,z;
	kernel.getSize(x,y,z);
	if(binCount[0] <x || binCount[1]< y || binCount[2] < z)
		return 1;
	
	long long halfX,halfY,halfZ;
	halfX=kernel.binCount[0]/2; halfY=kernel.binCount[1]/2;halfZ=kernel.binCount[2]/2;

	//Loop through all of the voxel elements, setting the
	//result voxels to the convolution of the template
	//with the data


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
			size_t sX,sY,sZ;
			sX = binCount[0] -x+1;
			sY = binCount[1] -y+1;
			sZ = binCount[2] -z+1;

			if(result.resize(sX,sY,sZ,resultMinBound, resultMaxBound))
				return VOXELS_OUT_OF_MEMORY;

			//loop through each element
			//Forgive the poor indenting, but the looping is very deep
			//FIXME: there is a shift in the output when doing this?
			for (size_t ui=0;ui<(size_t)(binCount[0]-x+1); ui++)
			{
			for (size_t uj=0;uj<binCount[1]-y+1; uj++)
			{
			for (size_t uk=0;uk<binCount[2]-z+1; uk++)
			{
				T tally;
				tally=T(0.0);

				//Convolve this element with the kernel 
				for(size_t ul=0; ul<x; ul++)
				{
				for(size_t um=0; um<y; um++)
				{
				for(size_t un=0; un<z; un++)
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
			break;
		}
		case BOUND_MIRROR:
		case BOUND_HOLD:
		case BOUND_DERIV_HOLD:
		{
			//Calculate the bounding size of the resultant box
			//Set up the result box
			if(result.resize(binCount[0],binCount[1],binCount[2],minBound,maxBound))
				return VOXELS_OUT_OF_MEMORY;

			//loop through each element in the interior
			//Forgive the poor indenting, but the looping is very deep
			{
			for (long long ui=halfX;ui<(size_t)(binCount[0]-x+1); ui++)
			{
			for (long long uj=halfY;uj<binCount[1]-y+1; uj++)
			{
			for (long long uk=halfZ;uk<binCount[2]-z+1; uk++)
			{
				T tally;
				tally=T(0.0);

				//Convolve this element with the kernel 
				for(long long ul=0; ul<x; ul++)
				{
				for(long long um=0; um<y; um++)
				{
				for(long long un=0; un<z; un++)
				{
					tally+=(getData(ui+ ul-halfX,uj + um -halfY,uk + un -halfZ))*
							kernel.getData(ul,um,un);
				}
				}
				}

				result.setData(ui,uj,uk,tally);	
			}
			}
			}
			}

			//Lower Boundaries
			//----------
			//x- boundary
			for(long long ui=0;ui<halfX;ui++)
			{
			for(long long uj=0;uj<binCount[1];uj++)
			{
			for(long long uk=0;uk<binCount[2];uk++)
			{
				localPaddedConvolve(ui,uj,uk,kernel,result,boundMode);
			}
			}
			}
			
			//y- boundary
			for(long long ui=0;ui<binCount[0];ui++)
			{
			for(long long uj=0;uj<halfY;uj++)
			{
			for(long long uk=0;uk<binCount[2];uk++)
			{
				localPaddedConvolve(ui,uj,uk,kernel,result,boundMode);
			}
			}
			}
			
			//z- boundary
			for(long long ui=0;ui<binCount[0];ui++)
			{
			for(long long uj=0;uj<binCount[1];uj++)
			{
			for(long long uk=0;uk<halfZ;uk++)
			{
				localPaddedConvolve(ui,uj,uk,kernel,result,boundMode);
			}
			}
			}
			//----------
	
			//Upper boundary
			//----------
			//x+ boundary
			for(long long ui=binCount[0]-x+1;ui<binCount[0];ui++)
			{
			for(long long uj=halfY;uj<binCount[1];uj++)
			{
			for(long long uk=halfZ;uk<binCount[2];uk++)
			{	
				localPaddedConvolve(ui,uj,uk,kernel,result,boundMode);
			}
			}
			}

			//y+ boundary
			for(long long ui=halfX;ui<binCount[0];ui++)
			{
			for(long long uj=binCount[1]-y+1;uj<binCount[1];uj++)
			{
			for(long long uk=halfZ;uk<binCount[2];uk++)
			{
				localPaddedConvolve(ui,uj,uk,kernel,result,boundMode);
			}
			}
			}
			
			//z+ boundary
			for(long long ui=halfX;ui<binCount[0];ui++)
			{
			for(long long uj=halfY;uj<binCount[1];uj++)
			{
			for(long long uk=binCount[2]-z+1;uk<binCount[2];uk++)
			{
				localPaddedConvolve(ui,uj,uk,kernel,result,boundMode);
			}
			}
			}
			//----------
			


			break;
		}
		default:
			ASSERT(false);
			return 2;
	}

	return 0;
}

template<class T>
size_t Voxels<T>::separableConvolve(const Voxels<T> &kernel, Voxels<T> &result,  size_t boundMode) 
{
	ASSERT(voxels.size());

	//Loop through all of the voxel elements, setting the
	//result voxels to the convolution of the template
	//with the data 


	Point3D resultMinBound, resultMaxBound;
	size_t x,y,z;
	unsigned long long half;
	kernel.getSize(x,y,z);
	half=x/2;
	//Kernel needs to be cubic
	ASSERT(x==y && y == z && (z%2));

	if(boundMode!=BOUND_CLIP)
	{
		
		resultMinBound=minBound;
		resultMaxBound=maxBound;

		ASSERT(result.binCount[0] == binCount[0] &&
				result.binCount[1] == binCount[1] &&
				result.binCount[2] == binCount[2]);

		T tally;

		//Separable kernels can be repeatably convolved
		//ie, f*g can be expressed as (f*g(x) + 
		//


		if(x < binCount[0]/2)
		{	
			//Convolve in X direction only
			for(size_t uj=0;uj<binCount[1];uj++)
			{
			for(size_t uk=0;uk<binCount[2];uk++)
			{
				for(unsigned long long ui=0;ui<x; ui++)
				{
					tally=T(0.0);
					for(unsigned long long ul=-half;ul<half; ul++)
						tally+=(getPaddedData(ui+ul,uj,uk,boundMode))*
								kernel.getData(ul+half,half,half);

					result.setData(ui,uj,uk,tally);
				}
				
				for(unsigned long long ui=binCount[0]-x;ui<binCount[0]; ui++)
				{
					tally=T(0.0);
					for(unsigned long long ul=-half;ul<half; ul++)
						tally+=(getPaddedData(ui+ul,uj,uk,boundMode))*
								kernel.getData(ul+half,half,half);
					result.setData(ui,uj,uk,tally);
				}

				//Unpadded section
				for(size_t ui=x;ui<binCount[0]-x; ui++)
				{
					tally=T(0.0);
					for(unsigned long long ul=-half;ul<half; ul++)
						tally+=(getPaddedData(ui+ul,uj,uk,boundMode))*
								kernel.getData(ul+half,half,half);
					
					result.setData(ui,uj,uk,result.getData(ui,uj,uk)+tally);
				}
			}

			}
		}
		else
		{
			for(size_t uj=0;uj<binCount[1];uj++)
			{
			for(size_t uk=0;uk<binCount[2];uk++)
			{
			//Kernel is bigger than dataset
			for(unsigned long long ui=0;ui<binCount[0]; ui++)
			{
				tally=T(0.0);
				for(unsigned long long ul=-half;ul<half; ul++)
					tally+=(getPaddedData(ui+ul,uj,uk,boundMode))*
							kernel.getData(ul+half,half,half);

				result.setData(ui,uj,uk,tally);
			}
			}
			}
		}
		
		swap(result);

		if(y < binCount[1]/2)
		{	
			//Convolve in Y direction only
			for(size_t ui=0;ui<binCount[0];ui++)
			{
			for(size_t uk=0;uk<binCount[2];uk++)
			{
				for(unsigned long long uj=0;uj<y; uj++)
				{
					tally=T(0.0);
					for(unsigned long long ul=-half;ul<half; ul++)
						tally+=(getPaddedData(ui,uj+ul,uk,boundMode))*
								kernel.getData(half,half+ul,half);
					result.setData(ui,uj,uk,tally);
				}
				
				for(unsigned long long uj=binCount[1]-y;uj<binCount[1]; uj++)
				{
					tally=T(0.0);
					for(unsigned long long ul=-half;ul<half; ul++)
						tally+=(getPaddedData(ui,uj+ul,uk,boundMode))*
								kernel.getData(half,half+ul,half);
					result.setData(ui,uj,uk,tally);
				}

				//Unpadded section
				for(size_t uj=y;uj<binCount[1]-y; uj++)
				{
					tally=T(0.0);
					for(unsigned long long ul=-half;ul<half; ul++)
						tally+=(getPaddedData(ui,uj+ul,uk,boundMode))*
								kernel.getData(half,half+ul,half);

					result.setData(ui,uj,uk,result.getData(ui,uj,uk)+tally);
				}
			}

			}
		}
		else
		{
			for(size_t ui=0;ui<binCount[0];ui++)
			{
			for(size_t uk=0;uk<binCount[2];uk++)
			{
			//Kernel is bigger than dataset
			for(unsigned long long uj=0;uj<binCount[1]; uj++)
			{
				tally=T(0.0);
				for(unsigned long long ul=-half;ul<half; ul++)
					tally+=(getPaddedData(ui,uj+ul,uk,boundMode))*
							kernel.getData(half,half+ul,half);

				result.setData(ui,uj,uk,tally);
			}
			}
			}
		}
	
		swap(result);
		
		if(z < binCount[2]/2)
		{	
			//Convolve in Z direction onlbinCount[0]
			for(size_t ui=0;ui<binCount[0];ui++)
			{
			for(size_t uj=0;uj<binCount[1];uj++)
			{
				for(long long uk=0;uk<z; uk++)
				{
					tally=T(0.0);
					for(long long ul=-half;ul<half; ul++)
						tally+=(getPaddedData(ui,uj,uk+ul,boundMode))*
								kernel.getData(half,half,half+ul);
					result.setData(ui,uj,uk,tally);
				}
				
				for(long long uk=binCount[2]-z;uk<binCount[2]; uk++)
				{
					tally=T(0.0);
					for(long long ul=-half;ul<half; ul++)
						tally+=(getPaddedData(ui,uj,uk+ul,boundMode))*
								kernel.getData(half,half,half+ul);
					result.setData(ui,uj,uk,tally);
				}

				//Unpadded section
				for(size_t uk=z;uk<binCount[2]-z; uk++)
				{
					tally=T(0.0);
					for(long long ul=-half;ul<half; ul++)
						tally+=(getPaddedData(ui,uj,uk+ul,boundMode))*
								kernel.getData(half,half,half+ul);

					result.setData(ui,uj,uk,result.getData(ui,uj,uk)+tally);
				}
			}

			}
		}
		else
		{
			for(size_t ui=0;ui<binCount[0];ui++)
			{
			for(size_t uj=0;uj<binCount[1];uj++)
			{
			//Kernel is bigger than dataset
			for(unsigned long long uk=0;uk<binCount[2]; uk++)
			{
				tally=T(0.0);
				for(unsigned long long ul=-half;ul<half; ul++)
					tally+=(getPaddedData(ui,uj,uk+ul,boundMode))*
							kernel.getData(half,half,half+ul);

				result.setData(ui,uj,uk,tally);
			}
			}
			}
		}	
	}
	else
	{
	
		//Check the kernel can fit within this datasest
		if(binCount[0] <x || binCount[1]< y || binCount[2] < z)
			return 1;
		resultMinBound=minBound*Point3D((float)(binCount[0]-x)/(float)binCount[0],
			(float)(binCount[1]-y)/(float)binCount[1],(float)(binCount[2]-z)/(float)binCount[2] );
		resultMaxBound=maxBound*Point3D((float)(binCount[0]-x)/(float)binCount[0],
			(float)(binCount[1]-y)/(float)binCount[1],(float)(binCount[2]-z)/(float)binCount[2] );
		//FIXME: IMPLEMENT ME!
		ASSERT(false);
	}

	return 0;
}


template<class T>
T Voxels<T>::getData(size_t x, size_t y, size_t z) const
{
	typedef size_t ull;
	ASSERT(x < binCount[0] && y < binCount[1] && z < binCount[2]);
	ull off; //byte offset

	//Typecast everything to at least 64 bit uints.
	off=(ull)z*(ull)binCount[1]*(ull)binCount[0];
	off+=(ull)y*(ull)binCount[0] + (ull)x;

	ASSERT(off < voxels.size());
	return	voxels[off]; 
}

template<class T>
T Voxels<T>::getPaddedData(long long x, long long y, long long z, unsigned int padMode) const
{
	if(x >=0 && x<(long long)binCount[0] && y>=0 && y<(long long)binCount[1] && z>=0 && z <(long long)binCount[2])
	{
		return getData(x,y,z);
	}

	//OK, so we are not inside the dataset -- we have to guess
	
	switch(padMode)
	{
		case BOUND_ZERO:
			return T(0);
		case BOUND_MIRROR:
		{

			if(x<0)
				x=-x;
			
			if(y<0)
				y=-y;
			
			if(z<0)
				z=-z;
		
			if(x >=binCount[0])
				x=binCount[0]-(x-binCount[0]+1);
			if(y >=binCount[1])
				y=binCount[1]-(y-binCount[1]+1);
			if(z >=binCount[2])
				z=binCount[2]-(z-binCount[2]+1);

			
			return  getData(x,y,z);
		}
		default:
			//Should only get here if we have not implemented the bound mode
			ASSERT(false);
	}

	ASSERT(false);
}

template<class T>
void Voxels<T>::setGaussianKernelCube(float stdDev, float bound, size_t sideLen)
{
	T obj;

	//Equi-variance multivariate normal distribution.
	//https://en.wikipedia.org/wiki/Multivariate_normal_distribution#Density_function
	// with identity covariance. (non-degenerate case)
	// det|sigma| == (stddev^3)
	const float product = 1.0/(pow(2.0*M_PI*stdDev,3.0/2.0));

	float scale;
	const float halfLen = sideLen/2;

	resize(sideLen,sideLen,sideLen);

	setBounds(Point3D(-bound/2.0f,-bound/2.0f,-bound/2.0f),
		Point3D(bound/2.0f,bound/2.0f,bound/2.0f));


	//2*det(covariance matrix)
	const float twoDetCov=2.0*stdDev*stdDev*stdDev;

	scale=bound/sideLen;
	scale*=scale;
	//Loop through the data, setting 
	// using probability density function
	for(size_t ui=0;ui<sideLen;ui++)
	{
		for(size_t uj=0;uj<sideLen;uj++)
		{
			for(size_t uk=0;uk<sideLen;uk++)
			{
				size_t offset;
				offset = (size_t)(((float)ui+0.5f-halfLen)*((float)ui+0.5f-halfLen) + 
					((float)uj+0.5f-halfLen)*((float)uj+0.5f-halfLen) + 
					((float)uk+0.5f-halfLen)*((float)uk+0.5f-halfLen));

				obj = T(product*exp(-scale*((float)offset)/(twoDetCov)));
				setData(ui,uj,uk,obj);
			}
		}
	}
}

template<class T>
void Voxels<T>::secondDifference(Voxels<T> &result, size_t boundMode) const
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
void Voxels<T>::thresholdForPosition(std::vector<Point3D> &p, const T &thresh, bool lowerEq) const
{
	p.clear();

	if(lowerEq)
	{
#pragma omp parallel for 
		for(MP_INT_TYPE ui=0;ui<binCount[0]; ui++)
		{
			for(size_t uj=0;uj<binCount[1]; uj++)
			{
				for(size_t uk=0;uk<binCount[2]; uk++)
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
			for(size_t uj=0;uj<binCount[1]; uj++)
			{
				for(size_t uk=0;uk<binCount[2]; uk++)
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
		for(size_t uj=0;uj<binCount[1]; uj++)
		{
			for(size_t uk=0;uk<binCount[2]; uk++)
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
void Voxels<T>::rebin(Voxels<T> &result, size_t rate, size_t clipDir) const
{
	//Check that the binsize can be reduced by this factor
	//or clipping allowed
	ASSERT(clipDir || ( !(binCount[0] % rate) && !(binCount[1] % rate)
						&& !(binCount[2] % rate)));

	//Datasets must be bigger than  their clipping direction
	ASSERT(binCount[0] > rate && binCount[1] && rate && binCount[2] > rate);

	
	size_t newBin[3];
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

	size_t its=0;
	unsigned int progress =0;
	float itsToDo = binCount[0]/rate;
#pragma omp parallel for shared(progress,its) 
	for(MP_INT_TYPE ui=0; ui<binCount[0]-rate; ui+=rate)
	{ 
#pragma omp critical
		{
			its++;
			//Update progress bar
			if(progress < (size_t) ((float)its/itsToDo*100) )
			{
				cerr <<  ".";
				progress = (size_t) ((float)its/itsToDo*100);
			}
		}

		for(size_t uj=0; uj<binCount[1]-rate; uj+=rate)
		{

			for(size_t uk=0; uk<binCount[2]-rate; uk+=rate)
			{
				double sum;
				sum=0;

				//Forgive the indenting. This is deep.
				for(size_t uir=0; uir<rate; uir++)
				{
					for(size_t ujr=0; ujr<rate; ujr++)
					{
						for(size_t ukr=0; ukr<rate; ukr++)
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
void Voxels<T>::makeSphericalKernel(size_t sideLen, float bound, const T &val, unsigned int antialiasLevel) 
{

	const size_t halfLen = sideLen/2;
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
			for(size_t uj=0;uj<sideLen;uj++)
			{
				for(size_t uk=0;uk<sideLen;uk++)
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
			for(size_t uj=0;uj<sideLen;uj++)
			{
				for(size_t uk=0;uk<sideLen;uk++)
				{
					float offset[8];
					bool insideSphere,fullCalc;
					//Calcuate r^2 from sphere centre for each corner
					for(unsigned int off=0;off<8; off++)
					{
						float x,y,z;
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
						float value,x,y,z;
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
						//each voxel has side length L_v = originalLen/(2^(level+1))
						while(!positionStack.empty())
						{
							unsigned int thisLevel;
							float thisLen;
							Point3D thisCentre;

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
T Voxels<T>::min() const
{
	ASSERT(voxels.size());
	size_t numPts = binCount[0]*binCount[1]*binCount[2];
	T minVal;
	minVal = voxels[0];
#ifdef _OPENMP
	//Unfortunately openMP doesn't lend itself well here
	//But we can code around it.
	size_t maxThr = omp_get_max_threads();	
	T minArr[maxThr];
	//Init all mins
	for(size_t ui=0; ui<maxThr; ui++)
		minArr[ui] = voxels[0];

	//parallel min search	
	#pragma omp parallel for shared(minArr)
	for(MP_INT_TYPE ui=0;ui<(MP_INT_TYPE)numPts; ui++)
		minArr[omp_get_thread_num()] = std::min(minArr[omp_get_thread_num()],voxels[ui]);	

	//find min of mins
	for(size_t ui=0;ui<maxThr; ui++)
		minVal=std::min(minArr[ui],minVal);
#else

	for(size_t ui=0;ui<numPts; ui++)
		minVal=std::min(minVal,voxels[ui]);
#endif
	return minVal;
}

template<class T>
T Voxels<T>::max() const
{
	ASSERT(voxels.size());
	size_t numPts = binCount[0]*binCount[1]*binCount[2];
	T maxVal;
	maxVal = voxels[0];
#ifdef _OPENMP
	//Unfortunately openMP doesn't lend itself well here
	//But we can code around it.
	size_t maxThr = omp_get_max_threads();	
	T maxArr[maxThr];
	//Init all maxs
	for(size_t ui=0; ui<maxThr; ui++)
		maxArr[ui] = voxels[0];

	//parallel max search	
	#pragma omp parallel for shared(maxArr)
	for(MP_INT_TYPE ui=0;ui<(MP_INT_TYPE)numPts; ui++)
		maxArr[omp_get_thread_num()] = std::max(maxArr[omp_get_thread_num()],voxels[ui]);	

	//find max of maxs
	for(size_t ui=0;ui<maxThr; ui++)
		maxVal=std::max(maxArr[ui],maxVal);
#else

	for(size_t ui=0;ui<numPts; ui++)
		maxVal=std::max(maxVal,voxels[ui]);
#endif
	return maxVal;
}

template<class T>
void Voxels<T>::minMax(T &minVal,T &maxVal) const
{
	ASSERT(voxels.size());
	size_t numPts = binCount[0]*binCount[1]*binCount[2];
	maxVal=voxels[0];
	minVal=voxels[0];
#ifdef _OPENMP
	//Unfortunately openMP doesn't lend itself well here
	//But we can code around it.
	size_t maxThr = omp_get_max_threads();	
	T minArr[maxThr],maxArr[maxThr];
	//Init all maxs
	for(size_t ui=0; ui<maxThr; ui++)
	{
		maxArr[ui] = voxels[0];
		minArr[ui]=voxels[0];
	}

	//parallel max search	
	#pragma omp parallel for shared(maxArr)
	for(MP_INT_TYPE ui=0;ui<(MP_INT_TYPE)numPts; ui++)
	{
		maxArr[omp_get_thread_num()] = std::max(maxArr[omp_get_thread_num()],voxels[ui]);	
		minArr[omp_get_thread_num()] = std::min(minArr[omp_get_thread_num()],voxels[ui]);	
	}

	//find max of maxs
	for(size_t ui=0;ui<maxThr; ui++)
	{
		maxVal=std::max(maxArr[ui],maxVal);
		minVal=std::min(minArr[ui],minVal);
	}
#else

	for(size_t ui=0;ui<numPts; ui++)
	{
		maxVal=std::max(maxVal,voxels[ui]);
		minVal=std::min(minVal,voxels[ui]);
	}
#endif
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
	size_t numBlocks[3];
	numBlocks[0] = (size_t)(2.0*rad*binCount[0]/(maxBound[0]-minBound[0]));
	numBlocks[1] = (size_t)(2.0*rad*binCount[1]/(maxBound[1]-minBound[1]));
	numBlocks[2] = (size_t)(2.0*rad*binCount[2]/(maxBound[2]-minBound[2]));

	//Construct the sphere kernel
	std::vector<int> vX,vY,vZ;
	float sqrRad=(float)numBlocks[0]*(float)numBlocks[0]/4.0f;
#pragma omp parallel for
	for(MP_INT_TYPE ui=0;ui<numBlocks[0]; ui++)
	{
		for(size_t uj=0;uj<numBlocks[1]; uj++)
		{
			for(size_t uk=0;uk<numBlocks[2]; uk++)
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
		size_t sphereCentre[3];
	
		getIndex(sphereCentre[0],sphereCentre[1],sphereCentre[2],spherePos[ui]);

		//Calculate the points that lie within the sphere
		for(size_t uj=0;uj<vX.size(); uj++)
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
	ASSERT(callback);
	if(doErase)
	{
		fill(0);	
	}

	size_t x,y,z;

	unsigned int downSample=MAX_CALLBACK;
	for(size_t ui=0; ui<points.size(); ui++)
	{
		if(!downSample--)
		{
			if(!(*callback)(false))
				return VOXEL_ABORT_ERR;
			downSample=MAX_CALLBACK;
		}
		
		T value;
		getIndex(x,y,z,points[ui]);

		//Ensure it lies within the dataset	
		if(x < binCount[0] && y < binCount[1] && z< binCount[2])
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
void Voxels<T>::calculateDensity()
{
	Point3D size = maxBound - minBound;
	// calculate the volume of a voxel
	double volume = 1.0;
	for (int i = 0; i < 3; i++)
		volume *= size[i] / binCount[i];
	
	// normalise the voxel value based on volume
#pragma omp parallel for
	for(size_t ui=0; ui<voxels.size(); ui++) 
		voxels[ui] /= volume;

}

template<class T>
float Voxels<T>::getBinVolume() const
{
	Point3D size = maxBound - minBound;
	double volume = 1.0;
	for (int i = 0; i < 3; i++)
		volume *= size[i] / binCount[i];

	return volume;
}

template<class T>
void Voxels<T>::getIndex(size_t &x, size_t &y,
	       			size_t &z, const Point3D &p) const
{

	ASSERT(p[0] >=minBound[0] && p[1] >=minBound[1] && p[2] >=minBound[2] &&
		   p[0] <=maxBound[0] && p[1] <=maxBound[1] && p[2] <=maxBound[2]);
	x=(size_t)((p[0]-minBound[0])/(maxBound[0]-minBound[0])*(float)binCount[0]);
	y=(size_t)((p[1]-minBound[1])/(maxBound[1]-minBound[1])*(float)binCount[1]);
	z=(size_t)((p[2]-minBound[2])/(maxBound[2]-minBound[2])*(float)binCount[2]);
}

template<class T>
void Voxels<T>::getIndexWithUpper(size_t &x, size_t &y,
	       			size_t &z, const Point3D &p) const
{
	//Get the data index as per normal
	getIndex(x,y,z,p);

	//but, as a special case, if the index is the same as our bincount, check
	//to see if it is positioned on an edge
	if(x==binCount[0] &&  
		fabs(p[0] -maxBound[0]) < sqrt(std::numeric_limits<float>::epsilon()))
		x--;
	if(y==binCount[1] &&  
		fabs(p[1] -maxBound[1]) < sqrt(std::numeric_limits<float>::epsilon()))
		y--;
	if(z==binCount[2] &&  
		fabs(p[2] -maxBound[2]) < sqrt(std::numeric_limits<float>::epsilon()))
		z--;

}

template<class T>
void Voxels<T>::fill(const T &v)
{
	size_t nBins = binCount[0]*binCount[1]*binCount[2];

#pragma omp parallel for
	for(MP_INT_TYPE ui=0;ui<(MP_INT_TYPE)nBins; ui++)
		voxels[ui]=v;
}


template<class T>
int Voxels<T>::histogram(std::vector<size_t> &v, size_t histBinCount) const
{
	ASSERT(callback);

	T maxVal=max();
	T minVal=min();
	
#ifdef _OPENMP
	//We need an array for each thread to store results
	size_t *vals;

	vals = new size_t[omp_get_max_threads()*histBinCount];

	//Zero out the array
	for(size_t ui=0;ui<omp_get_max_threads()*histBinCount; ui++)
		*(vals+ui)=0;
	

	bool spin=true;
	//Do it -- loop through, writing to each threads "scratch pad"	
	//to prevent threads interfering with one another
#pragma omp parallel for
	for(MP_INT_TYPE ui=0; ui<binCount[0]; ui++)
	{
		if(spin)
			continue;
		size_t *basePtr;
		size_t offset;
		basePtr=vals + omp_get_thread_num()*histBinCount;
		for(size_t uj=0; uj <binCount[1]; uj++)
		{
			for(size_t uk=0; uk <binCount[2]; uk++)
			{
				offset = (getData(ui,uj,uk)-minVal)/(maxVal -minVal)*(histBinCount-1);
				
				basePtr[offset]++;
			}
		}
#pragma omp critical
		{
		if(!(*callback)(false))
			spin=true;
		}

	}

	if(spin)
		return VOXEL_ABORT_ERR;
	//Merge the values
	v.resize(histBinCount);
	for(size_t thr=0;thr<omp_get_max_threads(); thr++)
	{
		size_t *basePtr;
		basePtr=vals + thr*histBinCount;
		for(size_t ui=0;ui<histBinCount; ui++)
		{
			v[ui]+=basePtr[ui];
		}
	}

#else
	//We need an array for each thread to store results
	size_t *vals=new size_t[histBinCount];

	for(size_t ui=0;ui<histBinCount; ui++)
		vals[ui]=0;
	size_t bc;
	bc=binCount[0];
	bc*=binCount[1];
	bc*=binCount[2];
	for(size_t ui=0; ui <bc; ui++)
	{
		size_t offset;
		offset = (size_t)((getData(ui)-minVal)/(maxVal -minVal)*histBinCount);
		vals[offset]++ ;
	}

	v.resize(histBinCount);
	for(size_t ui=0;ui<histBinCount; ui++)
		v[ui]=vals[ui];
		
#endif
	delete[] vals;	


	ASSERT(v.size() == histBinCount);

	return 0;	
}

template<class T>
void Voxels<T>::findNExtrema(std::vector<size_t> &x, std::vector<size_t> &y, 
			std::vector<size_t> &z, size_t n, bool largest) const
{
	//Could be better if we didn't use indexed data acquisition (record position)
	std::deque<size_t> bSx,bSy,bSz;

	if(voxels.empty())
		return;
	
	T curBest;
	curBest=getData(0);
	
	//It is theoretically possible to rewrite this without locking (critical sections).
	if(largest)
	{
		
		for(size_t ui=0;ui<binCount[0]; ui++)
		{
			for(size_t uj=0;uj<binCount[1]; uj++)
			{
				for(size_t uk=0;uk<binCount[2]; uk++)
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
		for(size_t ui=0;ui<binCount[0]; ui++)
		{
			for(size_t uj=0;uj<binCount[1]; uj++)
			{
				for(size_t uk=0;uk<binCount[2]; uk++)
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


//Obtain a slice of the voxel data. Data output will be in column order
// p[posB*nA + posA]. Input slice must be sufficiently sized and allocated
// to hold the output data
template<class T>
void Voxels<T>::getSlice(size_t normalAxis, size_t offset, T *p,size_t boundMode) const
{
	ASSERT(normalAxis < 3);

	size_t dimA,dimB,nA;
	switch(normalAxis)
	{
		case 0:
		{
			dimA=1;
			dimB=2;
			nA=binCount[dimA];
			break;
		}
		case 1:
		{	
			dimA=0;
			dimB=2;
			nA=binCount[dimA];
			break;
		}
		case 2:
		{
			dimA=0;
			dimB=1;
			nA=binCount[dimA];
			break;
		}
		default:
			ASSERT(false); //WTF - how did you get here??
	}
		

	if(offset < binCount[normalAxis])
	{
		//We are within bounds, use normal access functions
		switch(normalAxis)
		{
			case 0:
			{
				for(size_t ui=0;ui<binCount[dimA];ui++)
				{
					for(size_t uj=0;uj<binCount[dimB];uj++)
						p[uj*nA + ui] =	getData(offset,ui,uj);
				}
				break;
			}
			case 1:
			{	
				for(size_t ui=0;ui<binCount[dimA];ui++)
				{
					for(size_t uj=0;uj<binCount[dimB];uj++)
						p[uj*nA + ui] =	getData(ui,offset,uj);
				}
				break;
			}
			case 2:
			{
				for(size_t ui=0;ui<binCount[dimA];ui++)
				{
					for(size_t uj=0;uj<binCount[dimB];uj++)
						p[uj*nA + ui] =	getData(ui,uj,offset);
				}
				break;
			}
			default:
				ASSERT(false);
		}
	}
	else
	{
		//We are outside the cube bounds, use the padded access functions
		switch(normalAxis)
		{
			case 0:
			{
				for(size_t ui=0;ui<binCount[dimA];ui++)
				{
					for(size_t uj=0;uj<binCount[dimB];uj++)
						p[uj*nA + ui] =	getPaddedData(offset,ui,uj,boundMode);
				}
				break;
			}
			case 1:
			{	
				for(size_t ui=0;ui<binCount[dimA];ui++)
				{
					for(size_t uj=0;uj<binCount[dimB];uj++)
						p[uj*nA + ui] =	getPaddedData(ui,offset,uj,boundMode);
				}
				break;
			}
			case 2:
			{
				for(size_t ui=0;ui<binCount[dimA];ui++)
				{
					for(size_t uj=0;uj<binCount[dimB];uj++)
						p[uj*nA + ui] =	getPaddedData(ui,uj,offset,boundMode);
				}
				break;
			}
			default:
				ASSERT(false);
		}
	}
}

template<class T>
void Voxels<T>::getSlice(size_t normal, float offset, 
		T *p, size_t interpMode,size_t boundMode) const
{
	ASSERT(offset <=1.0f && offset >=0.0f);

	//Obtain the appropriately interpolated slice
	switch(interpMode)
	{
		case SLICE_INTERP_NONE:
		{
			size_t slicePos;
			slicePos=roundf(offset*binCount[normal]);
			getSlice(normal,slicePos,p,boundMode);
			break;
		}
		case SLICE_INTERP_LINEAR:
		{
			//Find the upper and lower bounds
			size_t sliceUpper,sliceLower;;
			sliceUpper=ceilf(offset*binCount[normal]);
			sliceLower=offset*binCount[normal];

			{
				T *pLower;
				size_t numEntries=binCount[(normal+1)%3]*binCount[(normal+2)%3];
				
				pLower  = new T[numEntries];

				getSlice(normal,sliceLower,pLower,boundMode);
				getSlice(normal,sliceUpper,p,boundMode);

				//Get the decimal part of the float
				float integ;
				float delta=modff(offset*binCount[normal],&integ);
				for(size_t ui=0;ui<numEntries;ui++)
					p[ui] = delta*(p[ui]-pLower[ui]) + pLower[ui];

				delete[] pLower;
			}
			break;
		}
		default:
			ASSERT(false);
			
	}

}


template<class T>
void Voxels<T>::operator/=(const Voxels<T> &v)
{
	ASSERT(v.voxels.size() == voxels.size());
#pragma omp parallel for
	for (size_t i = 0; i < voxels.size(); i++)
	{
		if(v.voxels[i])
			voxels[i]/=(v.voxels[i]);
		else
			voxels[i] =0;
	}
}	

	
template<class T>
void Voxels<T>::operator/=(const T &v)
{
	ASSERT(v);
#pragma omp parallel for
	for (size_t i = 0; i < voxels.size(); i++)
		voxels[i]/=v;
}	

template<class T>
bool Voxels<T>::operator==(const Voxels<T> &v) const
{
	for(size_t ui=0;ui<3;ui++)
	{
		
		if(v.binCount[ui] != binCount[ui])
			return false;
	}

	for(size_t ui=0;ui<voxels.size();ui++)
	{
		if(v.getData(ui) != getData(ui))
			return false;
	}

	return true;
}


//===


#endif
