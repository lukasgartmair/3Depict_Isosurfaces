/*
 *	basics.h - Basic functionality header 
 *	Copyright (C) 2010, D Haley 

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

#include "endianTest.h"

#include <iostream>
#include <cmath>
#include <vector>
#include <cstdlib>
#include <sstream>
#include <list>
#include <utility>

#if defined(WIN32) || defined(WIN64)
#include <wx/wx.h>
#include <wx/msw/registry.h>
#endif

class Point3D;
class K3DTree;

#ifdef DEBUG

void dh_assert(const char * const filename, const unsigned int lineNumber); 

#ifndef ASSERT
	#define ASSERT(f) if(!(f)) {dh_assert(__FILE__,__LINE__);}
#endif

	//I believe pretty_function is defined only by GCC
	#ifdef __PRETTY_FUNCTION__
		#define DBG_PRINTME std::cerr << "Function: " <<  __PRETTY_FUNCTION__ <<  endl; cerr << "File: " << __FILE__ << " Line:" << __LINE__ << endl
	#endif

#else
	#define ASSERT(f)
#endif

//!Text file loader errors
enum
{
	ERR_FILE_INPUT_OPEN_FAIL=1,
	ERR_FILE_OPEN_FAIL,
	ERR_FILE_OUT_FAIL,
	ERR_FILE_FORMAT_FAIL,
};

inline std::string locateDataFile(const char *name)
{
	//Possible strategies:
	//Linux:
	//TODO: Implement me. Currently we just return the name
	//which is equivalent to using current working dir (cwd).
	//	- Look in cwd.
	//	- Look in $PREFIX from config.h
	//	- Look in .config
	//Windows
	//	- Locate a registry key that has the install path, which is preset by
	//	  application installer
	//	- Look in cwd

#if defined(WIN32) || defined(WIN64)

	//This must match the key used in the installer
	wxRegKey *pRegKey = new wxRegKey(_("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\3Depict.exe"));

	if( pRegKey->Exists() )
	{
		//Now we are talkin. Regkey exists
		//OK, lets see if this dir actually exists or if we are being lied to (old dead regkey, for example)
		wxString keyVal;
		//Get the default key
		pRegKey->QueryValue(_(""),keyVal);
		//Strip the "3Depict.exe" from the key string
		std::string s;
		s = (const char *)keyVal.mb_str();
		
		if(s.size() > 11)
		{
			s=s.substr(0,s.size()-11);			
			return s + std::string(name);
		}
	}

#endif	

	//Mac
	//	- Look in cwd
	return std::string(name);
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


//!A routine for loading numeric data from a text file
unsigned int loadTextData(const char *cpFilename, 
		std::vector<std::vector<float> > &dataVec,
	       	std::vector<std::string> &header,const char delim);

//!Return the default font file to use. Must precede (first) call to getDefaultFontFile
void setDefaultFontFile(const std::string &font);

//!Return the default font file to use. 
//Not valid until you have set it with setDefaultFontFile
std::string getDefaultFontFile();

//!Generate string that can be parsed by wxPropertyGrid for combo control
//String format is CHOICEID:string 1, string 2, string 3,..... string_N
std::string choiceString(std::vector<std::pair<unsigned int, std::string> > comboString, 
								unsigned int curChoice);


//!Generate a string with leading digits up to maxDigit (eg, if maxDigit is 424, and thisDigit is 1
//leading digit will generate the string 001
std::string digitString(unsigned int thisDigit, unsigned int maxDigit);

//!Returns Choice from string (see choiceString(...) for string format)
std::string getActiveChoice(std::string choiceString);

//!Convert a choiceString() into something that a wxGridCellChoiceEditor will accept
std::string wxChoiceParamString(std::string choiceString);
//performs a string stream cast
//Returns true on failure
template<class T1, class T2> bool stream_cast(T1 &result,const T1 &obj);

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
inline unsigned int getBitNum(unsigned int u)
{
	ASSERT(u);
	unsigned int j=0;
	while(!(u &1) )
	{
		u=u>>1;
		j++;
	}

	return j;
}

//Strip whitespace from a string
std::string stripWhite(const std::string &str);

//!Return a lowercas version for a given string
std::string lowercase(std::string s);

void stripZeroEntries(std::vector<std::string> &s);


bool parseColString(const std::string &str,
	unsigned char &r, unsigned char &g, unsigned char &b, unsigned char &a);

void genColString(unsigned char r, unsigned char g, 
			unsigned char b, unsigned char a, std::string &s);
void genColString(unsigned char r, unsigned char g, 
			unsigned char b, std::string &s);

//!Strip whitespace
std::string stripWhite(const std::string &str);

//!Split string references using a single delimeter.
void splitStrsRef(const char *cpStr, const char delim,std::vector<std::string> &v );

//!Split string references using any of a given string of delimters
void splitStrsRef(const char *cpStr, const char *delim,std::vector<std::string> &v );

//TODO: Deprecate and use UniqueIDHandler, which does this and more
//!generate the first available unique ID number given a vector of IDs
unsigned int genUniqueID(const std::vector<unsigned int> &vec);

//!A class to manage "tear-off" ID values, to allow for indexing without knowing position. 
//You simply ask for a new unique ID. and it maintains the position->ID mapping
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
int getAvailRAM();

inline std::string tabs(unsigned int nTabs)
{
	std::string s;
	s.resize(nTabs);
	std::fill(s.begin(),s.end(),'\t');
	return s;
}

//Pair functor
class ComparePairSecond
{
	public:
	template<class T1, class T2>
	bool operator()(const std::pair<  T1, T2 > &p1, const std::pair<T1,T2> &p2)
	{
		return p1.second< p2.second;
	}
};

class ComparePairFirst
{
	public:
	template<class T1, class T2>
	bool operator()(const std::pair<  T1, T2 > &p1, const std::pair<T1,T2> &p2)
	{
		return p1.first< p2.first;
	}
};

class ComparePairFirstReverse
{
	public:
	template<class T1, class T2>
	bool operator()(const std::pair<  T1, T2 > &p1, const std::pair<T1,T2> &p2)
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
        bounds[0][1]=xMax; bounds[1][1]=yMax; bounds[2][1]=yMax;
        valid[0][0]=true; valid[1][0]=true; valid[2][0]=true;
        valid[0][1]=true; valid[1][1]=true; valid[2][1]=true;
    }

    void setBounds(const BoundCube &b)
    {
		for(unsigned int ui=0;ui<2;ui++)
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
    void setInverseLimits();

    void setBound(unsigned int bound, unsigned int minMax, float value) {
        ASSERT(bound <3 && minMax < 2);
        bounds[bound][minMax]=value;
        valid[bound][minMax]=true;
    };

    //!Return the centroid 
    Point3D getCentroid() const;

    //!Get the bounds
    void getBounds(Point3D &low, Point3D &high) const ;

    //!Return the size
    float getSize(unsigned int dim) const;

    //! Returns true if all bounds are valid
    bool isValid() const;
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


    //!Returns maximum distnace to box corners (which is an upper bound on max box distance). 
    //Bounding box must be valid.
    float getMaxDistanceToBox(const Point3D &pt) const;

    void limits();
    BoundCube operator=(const BoundCube &);
    //!Expand (as needed) volume such that the argument bounding cube is enclosed by this one
    void expand(const BoundCube &b);
    //!Expand such that point is contained in this volume. Existing volume must be valid
    void expand(const Point3D &p);

    friend  std::ostream &operator<<(std::ostream &stream, const BoundCube& b);

    //FIXME: Hack!
    friend class K3DTree;
};

//!A 3D point data class storage
/*! A  3D point data class
 * contains operator overloads and some basic
 * mathematical functions
 */
class Point3D
{
        private:
				//!Value data
                float value[3];
        public:
				//!Constructor
                inline Point3D() {};
				//!Constructor with initialising values
                inline Point3D(float x,float y,float z) 
					{ value[0] = x, value[1] = y, value[2] = z;}
                //!Set by value (ith dim 0, 1 2)
                inline void setValue(unsigned int ui, float val){value[ui]=val;};
				//!Set all values
                inline void setValue(float fX,float fY, float fZ)
                        {value[0]=fX; value[1]=fY; value[2]=fZ;}

                //!Set by pointer
                inline void setValueArr(const float *val)
                        {
                                value[0]=*val;
                                value[1]=*(val+1);
                                value[2]=*(val+2);
                        };

                //!Get value of ith dim (0, 1, 2)
                inline float getValue(unsigned int ui) const {return value[ui];};
		//Retrieve the internal pointer. Only use if you know why.
                inline const float *getValueArr() const { return value;};

                //!get into an array (note array must hold sizeof(float)*3 bytes of valid mem
                void copyValueArr(float *value) const;

                //!Add a point to this, without generating a return value
                void add(const Point3D &obj);

		//!Equality operator
                bool operator==(const Point3D &pt) const;
		//!assignemnt operator
                Point3D operator=(const Point3D &pt);
		//!+= operator
                Point3D operator+=(const Point3D &pt);
                Point3D operator+(float f);
		//!multiplication= operator
                Point3D operator*=(const float scale);
		//!Addition operator
                Point3D operator+(const Point3D &pt) const;
		//!multiplication
                Point3D operator*(float scale) const;
		//!multiplication
				Point3D operator*(const Point3D &pt) const;
		//!Division. 
                Point3D operator/(float scale) const;
		//!Subtraction
                Point3D operator-(const Point3D &pt) const;
		//!returns a negative of the existing value
                Point3D operator-() const;
		//!Output streaming operator. Users (x,y,z) as format for output
                friend std::ostream &operator<<(std::ostream &stream, const Point3D &);
                //!make point unit magnitude, maintaining direction
		Point3D normalise();
                //!returns the square of distance another pt
                float sqrDist(const Point3D &pt) const;

                //!Calculate the dot product of this and another pint
                float dotProd(const Point3D &pt) const;
                //!Calculate the cross product of this and another point
                Point3D crossProd(const Point3D &pt) const;

				//!Calculate the angle between two position vectors in radiians
				float angle(const Point3D &pt) const;

                //!overload for array indexing returns |pt|^2
                float sqrMag() const;

				//!Retrieve by value
                inline float operator[](unsigned int ui) const { ASSERT(ui < 3); return value[ui];}
				//!Retrieve element by referene
                inline float &operator[](unsigned int ui) { ASSERT(ui < 3); return value[ui];}

                //!Is a given point stored inside a box bounded by orign and this pt?
                /*!returns true if this point is located inside (0,0,0) -> Farpoint
                * assuming box shape (non zero edges return false)
                * farPoint must be positive in all dim
                */
                bool insideBox(const Point3D &farPoint) const;


				//!Tests if this point lies inside the rectangular prism 
				/*!Returns true if this point lies inside the box bounded
				 * by lowPoint and highPoint
				 */
                bool insideBox(const Point3D &lowPoint, const Point3D &highPoint) const;

				//!Makes each value negative of old value
				void negate();
#ifdef __LITTLE_ENDIAN__
                //!Flip the endian state for data stored in this point
                void switchEndian();
#endif
		bool parse(const std::string &str);
};

//!A colour data storage structure 
class Colour
{
	public:
		//!colours, range [0,1.0f]
		float r,g,b,a;
		std::string asString() const; 
		bool fromString(const std::string &str);
};

#endif
