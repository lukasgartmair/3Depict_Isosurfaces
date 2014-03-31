/*
 *	common/basics.h - Basic functionality header 
 *	Copyright (C) 2013, D Haley 

 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.

 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.

 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef BASICS_H
#define BASICS_H
//!Basic objects header file

#include "mathfuncs.h"
#include "common/assertion.h"


#include <vector>
#include <sstream>
#include <list>
#include <fstream>
#include <algorithm>

class K3DTree;


bool dummyCallback(bool);

extern const char *DTD_NAME;
extern const char *PROGRAM_NAME;
extern const char *PROGRAM_VERSION;
extern const char *FONT_FILE;


//Set new locale code. Must be followed by a popLocale call before completion
// Only one locale type can be pushed at a time this way
void pushLocale(const char *newLocale, int type);

//Restore old locale code
void popLocale();



//C file peek function
inline int fpeek(FILE *stream)
{
	int c;

	c = fgetc(stream);
	ungetc(c, stream);

	return c;
}

template<class T>
bool rangesOverlap( const T &minA, const T &maxA,
			const T &minB, const T &maxB)
{

	ASSERT(minA <= maxA);
	ASSERT(minB<=maxB);

	// A-  B- A+ 
	// A- B+ A+
	if( (minA <= minB && maxA >=minB )
		|| (minA<=maxB && maxA >=maxB) )
		return true;

	// B-  A- B+ 
	// B- A+ B+
	if(( minB <= minA && maxB >=minA )
		|| (minB<=maxA && maxB >=maxA) )
		return true;

	return false;

}

//!Text file loader errors
enum
{
	ERR_FILE_OPEN=1,
	ERR_FILE_FORMAT,
	ERR_FILE_NUM_FIELDS,
	ERR_FILE_ENUM_END // not an error, just end of enum
};

extern const char *TEXT_LOAD_ERR_STRINGS[];

//Perform a a<-b<-c<-a rotation of data
template<class T>
void rotate3(T &a, T &b, T &c)
{
	T tmp;
	tmp=a;
	a=b;
	b=c;
	c=tmp;
}

//Find the min/max value within an array
//array must be nonzero sized
template<class T>
T maxValue(const T *t, size_t n)
{
	ASSERT(n);

	T maxV=t[0];
	for(size_t ui=1;ui<n;ui++)
	{
		maxV=std::max(t[ui],maxV);
	}
	return maxV;

}

template<class T>
T minValue(const T *t, size_t n)
{
	ASSERT(n);

	T minV=t[0];
	for(size_t ui=1;ui<n;ui++)
	{
		minV=std::min(t[ui],minV);
	}
	return minV;

}

template<class T1, class T2>
bool hasFirstInPairVec(const std::vector<std::pair<T1,T2> > &v, const std::pair<T1,T2> &r)
{
	for(size_t ui=0;ui<v.size();ui++)
	{
		if(v[ui].first == r.first)
			return true;
	}
	return false;
}

//!Convert a normal string to a latex one, using character replacement
void tickSpacingsFromInterspace(float start, float end, 
		float interSpacing, std::vector<float> &spacings);

void tickSpacingsFromFixedNum(float start, float end, 
		unsigned int nTicks, std::vector<float> &spacings);

//!Get a "human-like" version of the time elapsed between new and original time period
std::string veryFuzzyTimeSince( time_t origTime, time_t newTime);


//!A routine for loading numeric data from a text file. Returns 0 on success
unsigned int loadTextData(const char *cpFilename, 
		std::vector<std::vector<float> > &dataVec,
	       	std::vector<std::string> &header,const char *delim);

//!Load non-numeric data from a text file into ragged array, using specified delimiters
unsigned int loadTextStringData(const char *cpFilename, 
		std::vector<std::vector<std::string> > &dataVec,
	       	const char *delim);


template<class T>
bool writeTextFile(const char *cpFilename, 
		const std::vector<std::pair<T, T> > &dataVec, const char delim='\t')
{
	std::ofstream f(cpFilename);

	if(!f)
		return false;

	for(unsigned int ui=0;ui<dataVec.size();ui++)
		f << dataVec[ui].first << delim << dataVec[ui].second << std::endl;
	
	return true;
}

//!Return the default font file to use. Must precede (first) call to getDefaultFontFile
void setDefaultFontFile(const std::string &font);

//!Return the default font file to use. 
//Not valid until you have set it with setDefaultFontFile
std::string getDefaultFontFile();


//!Template function to cast and object to another by the stringstream
//IO operator
template<class T1, class T2> bool stream_cast(T1 &result, const T2 &obj)
{
    std::stringstream ss;
    ss << obj;
    ss >> result;
    return ss.fail();
}

//!Replace first instance of marker with null terminator
void nullifyMarker(char *buffer, char marker);

//retrieve the active bit in a power of two sequence
unsigned int getBitNum(unsigned int u);

//!A class to manage "tear-off" ID values, to allow for indexing without knowing position. 
//You simply ask for a new unique ID. and it maintains the position->ID mapping
// as position could change if an element was removed, but ID cannot
//TODO: Extend to any unique type, rather than just int (think iterators..., pointers)
class UniqueIDHandler
{
	private:
		//!position-ID pairings
		std::list<std::pair<unsigned int, unsigned int > > idList;

	public:
		//!Generate  a unique ID value, storing the position ID pair
		unsigned int genId(unsigned int position);
		//!Remove a uniqueID using its position
		void killByPos(unsigned int position);
		//!Get the position from its unique ID
		unsigned int getPos(unsigned int id) const;
		//!Get the uniqueID from the position
		unsigned int getId(unsigned int pos) const;

		//!Get all unique IDs
		void getIds(std::vector<unsigned int> &idvec) const;
		//!Clear the mapping
		void clear();
		//!Get the number of elements stored
		unsigned int size() const {return idList.size();};
};

//!Get total filesize in bytes
bool getFilesize(const char *fname, size_t  &size);

//!get total ram in MB
int getTotalRAM();

//!Get available ram in MB
size_t getAvailRAM();

//!Determine if a given path is a not a directory, 
bool isNotDirectory(const char *filename);

bool rmFile(const std::string & filename);

#ifdef DEBUG
bool isValidXML(const char *filename);
#endif


class ComparePairFirst
{
	public:
	template<class T1, class T2>
	bool operator()(const std::pair<  T1, T2 > &p1, const std::pair<T1,T2> &p2) const
	{
		return p1.first< p2.first;
	}
};

class ComparePairSecond
{
	public:
	template<class T1, class T2>
	bool operator()(const std::pair<  T1, T2 > &p1, const std::pair<T1,T2> &p2) const
	{
		return p1.second< p2.second;
	}
};


class ComparePairFirstReverse
{
	public:
	template<class T1, class T2>
	bool operator()(const std::pair<  T1, T2 > &p1, const std::pair<T1,T2> &p2) const
	{
		return p1.first> p2.first;
	}
};

//! A helper class to define a bounding cube
class BoundCube
{
    //!bounding values (x,y,z) (lower,upper)
    float bounds[3][2];
    //!Is the cube set?
    bool valid[3][2];
public:

    BoundCube() {
        setInvalid();
    }

    void setBounds(float xMin,float yMin,float zMin,
                   float xMax,float yMax,float zMax) {
        bounds[0][0]=xMin; bounds[1][0]=yMin; bounds[2][0]=zMin;
        bounds[0][1]=xMax; bounds[1][1]=yMax; bounds[2][1]=zMax;
        valid[0][0]=true; valid[1][0]=true; valid[2][0]=true;
        valid[0][1]=true; valid[1][1]=true; valid[2][1]=true;
    }

    void setBounds(const BoundCube &b)
    {
		for(unsigned int ui=0;ui<3;ui++)
		{
			bounds[ui][0] = b.bounds[ui][0];
			valid[ui][0] = b.valid[ui][0];
			bounds[ui][1] = b.bounds[ui][1];
			valid[ui][1] = b.valid[ui][1];
		}
    }
    void setInvalid()
    {
        valid[0][0]=false; valid[1][0]=false; valid[2][0]=false;
        valid[0][1]=false; valid[1][1]=false; valid[2][1]=false;
    }

    //Set the cube to be "inside out" at the limits of numeric results;
    void setInverseLimits(bool setAsValid=false);

    void setBound(unsigned int bound, unsigned int minMax, float value) ;

    //Retrieve a specified bound, minMax=0 for min, =1 for max
    float getBound(unsigned int bound, unsigned int minMax) const ;
    
    void getBound(Point3D &bound, unsigned int minMax) const ;
    //!Return the centroid 
    Point3D getCentroid() const;

    //!Get the bounds
    void getBounds(Point3D &low, Point3D &high) const ;

    //!Return the size
    float getSize(unsigned int dim) const;

    //! Returns true if all bounds are valid
    bool isValid() const;

    //! Returns true if any bound is of null thickness
    bool isFlat() const;

    //!Returns true if any bound of datacube is considered to be "large" in magnitude compared to 
    // floating pt data type.
    bool isNumericallyBig() const;

    //!Obtain bounds from an array of Point3Ds
    void setBounds( const Point3D *ptArray, unsigned int nPoints);
    //!Use two points to set bounds -- does not need to be high,low. this is worked out/
    void setBounds( const Point3D &p, const Point3D &q);
    //!Obtain bounds from an array of Point3Ds
    void setBounds(const std::vector<Point3D> &ptArray);
    //!Checks if a point intersects a sphere of centre Pt, radius^2 sqrRad
    bool intersects(const Point3D &pt, float sqrRad);
    //Check to see if the point is contained in, or part of the walls
    //of the cube
    bool containsPt(const Point3D &pt) const;

    bool contains(const BoundCube &b) const;

    //!Is this bounding cube completely contained within a sphere centred on pt of sqr size sqrRad?
    bool containedInSphere(const Point3D &pt, float sqrRad) const;

    //!Returns maximum distnace to box corners (which is an upper bound on max box distance). 
    //Bounding box must be valid.
    float getMaxDistanceToBox(const Point3D &pt) const;

    //Get the largest dimension of the bound cube
    float getLargestDim() const;

    //Return the rectilinear volume represented by this prism.
    float volume() const { return (bounds[0][1] - bounds[0][0])*
	    	(bounds[1][1] - bounds[1][0])*(bounds[2][1] - bounds[2][0]);}
    void limits();
    const BoundCube &operator=(const BoundCube &);
    //!Expand (as needed) volume such that the argument bounding cube is enclosed by this one
    void expand(const BoundCube &b);
    //!Expand such that point is contained in this volume. Existing volume must be valid
    void expand(const Point3D &p);
    //!Expand by a specified thickness 
    void expand(float v);
    //!Obtain the corner points of the cube
    void getVertices(std::vector<Point3D> &p,bool centre=false) const;
    friend  std::ostream &operator<<(std::ostream &stream, const BoundCube& b);

    //FIXME: Hack!
    friend class K3DTree;
    friend class K3DTreeMk2;
};




//OK, this is a bit tricky. We override the operators to call
//a callback, so the UI updates keep happening, even inside the STL function
//----
template<class T>
class GreaterWithCallback 
{
	private:
		bool (*callback)(bool);
		//!Reduction frequency (use callback every k its)
		unsigned int redMax;
		//!Current reduction counter
		unsigned int reduction;
		//!pointer to progress value
		unsigned int *prgPtr;
	public:
		//!Second argument is a "reduction" value to set the number of calls
		//to the random functor before initiating a callback
		GreaterWithCallback( bool (*ptr)(bool),unsigned int red) : callback(ptr), redMax(red), reduction(red)
	{};

		bool operator()(const T &a, const T &b) 
		{
			if(!reduction--)
			{
				reduction=redMax;
				//Execute callback
				(*callback)(false);
			}

			return a < b;
		}
};


template<class T>
class EqualWithCallback 
{
	private:
		bool (*callback)(bool);
		//!Reduction frequency (use callback every k its)
		unsigned int redMax;
		//!Current reduction counter
		unsigned int reduction;
	public:
		//!Second argument is a "reduction" value to set the number of calls
		//to the random functor before initiating a callback
		EqualWithCallback( bool (*ptr)(bool),unsigned int red) : callback(ptr), redMax(red), reduction(red)
			{ };

		bool operator()(const T &a, const T &b) 
		{
			if(!reduction--)
			{
				reduction=redMax;
				//Execute callback
				(*callback)(false);
			}

			return a ==b;
		}
};
//----


//Randomly select subset. Subset will be (somewhat) sorted on output
template<class T> size_t randomSelect(std::vector<T> &result, const std::vector<T> &source, 
							RandNumGen &rng, size_t num,unsigned int &progress,bool (*callback)(bool), bool strongRandom=false)
{
	const unsigned int NUM_CALLBACK=50000;
	//If there are not enough points, just copy it across in whole
	if(source.size() <= num)
	{
		num=source.size();
		result.resize(source.size());
		for(size_t ui=0; ui<num; ui++)
			result[ui] = source[ui]; 
	
		return num;
	}

	result.resize(num);

	if(strongRandom || source.size() < 4)
	{

		size_t numTicksNeeded;
		//If the number of items is larger than half the source size,
		//switch to tracking vacancies, rather than data
		if(num < source.size()/2)
			numTicksNeeded=num;
		else
			numTicksNeeded=source.size()-num;

		//Randomly selected items 
		//---------
		std::vector<size_t> ticks;
		ticks.resize(numTicksNeeded);

		//Create an array of numTicksNeededbers and fill 
		for(size_t ui=0; ui<numTicksNeeded; ui++)
			ticks[ui]=(size_t)(rng.genUniformDev()*(source.size()-1));

		//Remove duplicates. Intersperse some callbacks to be nice
		GreaterWithCallback<size_t> gFunctor(callback,50000);
		std::sort(ticks.begin(),ticks.end(),gFunctor);
		EqualWithCallback<size_t> eqFunctor(callback,50000);

		std::vector<size_t>::iterator newLast;
		newLast=std::unique(ticks.begin(),ticks.end());	
		ticks.erase(newLast,ticks.end());

		//Top up with unique entries
		//TODO: Overcommit & Discard implementation?
		// - Should be possible to predict how many we need after collisions, and then overcommit and discard randomly. Removes need to loop-sort like this
		while(ticks.size() < numTicksNeeded)
		{
			size_t moreTicks=numTicksNeeded-ticks.size();
			for(size_t uk=0;uk<moreTicks;uk++)
			{
				//This is actually not too bad. the collision probability is at most 50%
				//due the switching behaviour above, for any large number of items 
				//So this is at worst case nlog(n) (I think)
				ticks.push_back((size_t)(rng.genUniformDev()*(float)(source.size()-1)));
	
			}

			std::sort(ticks.begin(),ticks.end(),gFunctor);
			newLast=std::unique(ticks.begin(),ticks.end(),eqFunctor);	
			ticks.erase(newLast,ticks.end());
		}

		ASSERT(ticks.size() == numTicksNeeded);
		//---------
		
		//Transfer the output
		unsigned int curProg=NUM_CALLBACK;

		if(num < source.size()/2)
		{
			size_t pos=0;
			for(std::vector<size_t>::iterator it=ticks.begin();it!=ticks.end();++it)
			{

				result[pos]=source[*it];
				pos++;
				if(!curProg--)
				{
					progress= (unsigned int)((float)(pos)/((float)num)*100.0f);
					(*callback)(false);
					curProg=NUM_CALLBACK;
				}
			}
		}
		else
		{
			//Sort the ticks properly (mostly sorted anyway..)
			std::sort(ticks.begin(),ticks.end(),gFunctor);

			unsigned int curTick=0;
			for(size_t ui=0;ui<source.size(); ui++)
			{
				//Don't copy if this is marked
				if(ui == ticks[curTick])
					curTick++;
				else
				{
					ASSERT(result.size() > (ui-curTick));
					result[ui-curTick]=source[ui];
				}
				
				if(!curProg--)
				{
					progress= (unsigned int)(((float)(ui)/(float)source.size())*100.0f);
					(*callback)(false);
					curProg=NUM_CALLBACK;
				}
			}
		}

		ticks.clear();
	}	
	else
	{
		//Use a weak randomisation
		LinearFeedbackShiftReg l;

		//work out the mask level we need to use
		size_t i=1;
		unsigned int j=0;
		while(i < (source.size()<<1))
		{
			i=i<<1;
			j++;
		}

		//linear shift table starts at 3.
		if(j<3) {
			j=3;
			i = 1 << j;
		}

		size_t start;
		//start at a random position  in the linear state
		start =(size_t)(rng.genUniformDev()*i);
		l.setMaskPeriod(j);
		l.setState(start);

		size_t ui=0;	
		unsigned int curProg=NUM_CALLBACK;
		//generate unique weak random numbers.
		while(ui<num)
		{
			size_t res;
			res= l.clock();
			
			//use the source if it lies within range.
			//drop it silently if it is out of range
			if(res< source.size())
			{
				result[ui] =source[res];
				ui++;
			}
			if(!curProg--)
			{
				progress= (unsigned int)((float)(ui)/((float)source.size())*100.0f);
				(*callback)(false);
				curProg=NUM_CALLBACK;
			}
		}

	}

	return num;
}

//Randomly select subset [0,max). Subset will be (somewhat) sorted on output
template<class T> size_t randomDigitSelection(std::vector<T> &result, const size_t max,
			RandNumGen &rng, size_t num,unsigned int &progress,bool (*callback)(bool),
			bool strongRandom=false)
{
	//If there are not enough points, just copy it across in whole
	if(max <=num)
	{
		num=max;
		result.resize(max);
		for(size_t ui=0; ui<num; ui++)
			result[ui] = ui; 
	
		return num;
	}

	result.resize(num);

	//If we have strong randomisation, or we have too few items to use the LFSR,
	//use proper random generation
	if(strongRandom || max < 3 )
	{

		size_t numTicksNeeded;
		//If the number of items is larger than half the source size,
		//switch to tracking vacancies, rather than data
		if(num < max/2)
			numTicksNeeded=num;
		else
			numTicksNeeded=max-num;

		//Randomly selected items 
		//TODO: Benchmark against set<> implementation
		//---------
		std::vector<size_t> ticks;
		ticks.resize(numTicksNeeded);

		//Create an array of numbers and fill 
		for(size_t ui=0; ui<numTicksNeeded; ui++)
			ticks[ui]=(size_t)(rng.genUniformDev()*(max-1));

		//Remove duplicates. Intersperse some callbacks to be nice
		GreaterWithCallback<size_t> gFunctor(callback,50000);
		std::sort(ticks.begin(),ticks.end(),gFunctor);
		EqualWithCallback<size_t> eqFunctor(callback,50000);
		
		std::vector<size_t>::iterator itLast;
		itLast=std::unique(ticks.begin(),ticks.end(),eqFunctor);	
		ticks.erase(itLast,ticks.end());
		
		//Top up with unique entries
		while(ticks.size() < numTicksNeeded)
		{
			size_t moreTicks=numTicksNeeded-ticks.size();
			for(size_t uk=0;uk<moreTicks;uk++)
			{
				//This is actually not too bad. the collision probability is at most 50%
				//due the switching behaviour above, for any large number of items 
				//So this is at worst case nlog(n) (I think)
				ticks.push_back((size_t)(rng.genUniformDev()*(float)(max-1)));
	
			}

			std::sort(ticks.begin(),ticks.end(),gFunctor);
			itLast=std::unique(ticks.begin(),ticks.end(),eqFunctor);	
			ticks.erase(itLast,ticks.end());
		}


		ASSERT(ticks.size() == numTicksNeeded);
		//---------
		
		//Transfer the output
		const unsigned int CURPROG=70000;
		unsigned int curProg=CURPROG;

		if(num < max/2)
		{
			size_t pos=0;
			for(std::vector<size_t>::iterator it=ticks.begin();it!=ticks.end();++it)
			{

				result[pos]=*it;
				pos++;
				if(!curProg--)
				{
					progress= (unsigned int)((float)(curProg)/((float)num)*100.0f);
					(*callback)(false);
					curProg=CURPROG;
				}
			}
		}
		else
		{
			//Sort the ticks properly (mostly sorted anyway..)
			std::sort(ticks.begin(),ticks.end(),gFunctor);
			
			unsigned int curTick=0;
			for(size_t ui=0;ui<numTicksNeeded; ui++)
			{
				//Don't copy if this is marked
				if(ui == ticks[curTick])
					curTick++;
				else
					result[ui-curTick]=ui;
				
				if(!curProg--)
				{
					progress= (unsigned int)((float)(curProg)/((float)num)*100.0f);
					(*callback)(false);
					curProg=CURPROG;
				}
			}
		}

		ticks.clear();
	}
	else	
	{
		//Use a weak randomisation
		LinearFeedbackShiftReg l;

		//work out the mask level we need to use
		size_t i=1;
		unsigned int j=0;
		while(i < (max<<1))
		{
			i=i<<1;
			j++;
		}
		
		size_t start;
		//start at a random position  in the linear state
		start =(size_t)(rng.genUniformDev()*i);
		l.setMaskPeriod(j);
		l.setState(start);

		size_t ui=0;	
		//generate unique weak random numbers.
		while(ui<num)
		{
			size_t res;
			res= l.clock();
			
			//use the source if it lies within range.
			//drop it silently if it is out of range
			if(res<max) 
			{
				result[ui] =res;
				ui++;
			}
		}
	}
	return num;
}

//Remove elements from the vector, without preserving order
// the pattern of removal will be unique with a given kill pattern
//Remove selected elements from vector, preserving order
template<class T>
void vectorMultiErase(std::vector<T> &vec, const std::vector<bool> &wantKill)
{
	ASSERT(vec.size() == wantKill.size());

	if(!vec.size())
		return;
	size_t shift=0;	
	for(size_t ui=0;ui<vec.size();ui++)
	{
		if(wantKill[ui])
			shift++;
		else if(shift)
			vec[ui-shift] = vec[ui];
	}
	vec.resize(vec.size()-shift);
}

#endif
