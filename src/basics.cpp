/*
 *	basics.cpp  - basic functions header
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

#include "basics.h"

#include <iostream>
#include <limits>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>

#ifdef __APPLE__
	#include <sys/types.h>
	#include <sys/sysctl.h>
#elif defined __linux__
	//Needed for getting ram total usage under linux
	#include <sys/sysinfo.h>
#endif

using std::string;
using std::vector;
using std::list;

using std::cerr;
using std::endl;

//default font to use.
std::string defaultFontFile;

void setDefaultFontFile(const std::string &font)
{
	defaultFontFile=font;
}

std::string getDefaultFontFile()
{
	return defaultFontFile;
}

void nullifyMarker(char *buffer, char marker)
{
	while(*(buffer))
	{
		if(*buffer == marker)
		{
			*buffer='\0';
			break;
		}
		buffer++;
	}
}

void dh_assert(const char * const filename, const unsigned int lineNumber) 
{
    std::cerr << "ASSERTION ERROR!" << std::endl;
    std::cerr << "Filename: " << filename << std::endl;
    std::cerr << "Line number: " << lineNumber << std::endl;

//	std::cerr << "Do you wish to continue?(y/n)";
//	char y = 'a';
//	while (y != 'n' && y != 'y')
//		std::cin >> y;
//
//	if (y != 'y')
//		exit(1);
}

void ucharToHexStr(unsigned char c, std::string &s)
{
	s="  ";
	char h1,h2;

	h1 = c/16;
	h2 = c%16;

	if(h1 < 10)
	{
		s[0]=(h1+'0');
	}
	else
		s[0]=((h1-10)+'a');

	if(h2 < 10)
	{
		s[1]=(h2+'0');
	}
	else
		s[1]=((h2-10)+'a');
	
}

void hexStrToUChar(const std::string &s, unsigned char &c)
{
	ASSERT(s.size() ==2);

	ASSERT((s[0] >= '0' && s[0] <= '9') ||
	      		(s[0] >= 'a' && s[0] <= 'f'));
	ASSERT((s[1] >= '0' && s[1] <= '9') ||
			(s[1] >= 'a' && s[1] <= 'f'));

	int high,low;
	if(s[0] <= '9' && s[0] >='0')
		high = s[0]-(int)'0';
	else
		high = s[0] -(int)'a' + 10;	
	
	if(s[1] <= '9' && s[1] >='0')
		low = s[1]-(int)'0';
	else
		low = s[1] -(int)'a' + 10;	

	c = 16*high + low;
}


std::string digitString(unsigned int thisDigit, unsigned int maxDigit)
{
	std::string s,thisStr;
	stream_cast(thisStr,thisDigit);

	stream_cast(s,maxDigit);
	for(unsigned int ui=0;ui<s.size();ui++)
		s[ui]='0';


	s=s.substr(0,s.size()-thisStr.size());	

	return  s+thisStr;
}

unsigned int genUniqueID(const vector<unsigned int> &vec)
{
	//Look for each element number as a unique value in turn
	for(unsigned int ui=0;ui<vec.size(); ui++)
	{
		if(std::find(vec.begin(),vec.end(),ui) == vec.end())
			return ui;
	}

	//Note: This is guaranteed to be unique
	//via the pigeonhole principle (as we found every element, and the sizes
	//match, then the vector can only contain the elements we searched for)
	return vec.size();
}

std::string choiceString(std::vector<std::pair<unsigned int, std::string> > comboString, 
									unsigned int curChoice)
{
	ASSERT(curChoice < comboString.size())

	string s,sTmp;
	stream_cast(sTmp,curChoice);
	s=sTmp + string(":");
	for(unsigned int ui=0;ui<comboString.size();ui++)
	{
		//Should not contain a comma or vert. bar in user string
		ASSERT(comboString[ui].second.find(",") == string::npos);
		ASSERT(comboString[ui].second.find("|") == string::npos);

		stream_cast(sTmp,comboString[ui].first);
		if(ui < comboString.size()-1)
			s+=sTmp + string("|") + comboString[ui].second + string(",");
		else
			s+=sTmp + string("|") + comboString[ui].second;

	}

	return s;
}

//!Returns Choice ID from string (see choiceString(...) for string format)
std::string getActiveChoice(std::string choiceString)
{
	size_t colonPos;
	colonPos = choiceString.find(":");
	ASSERT(colonPos!=string::npos);

	//Extract active selection
	string tmpStr;	
	tmpStr=choiceString.substr(0,colonPos);
	unsigned int activeChoice;
	stream_cast(activeChoice,tmpStr);

	//Convert ID1|string 1, .... IDN|string n to vectors
	choiceString=choiceString.substr(colonPos,choiceString.size()-colonPos);
	vector<string> choices;
	splitStrsRef(choiceString.c_str(),',',choices);

	ASSERT(activeChoice < choices.size());
	tmpStr = choices[activeChoice];

	return tmpStr.substr(tmpStr.find("|")+1,tmpStr.size()-1);
}

//Convert my internal choice string format to wx's
std::string wxChoiceParamString(std::string choiceString)
{
	string retStr;

	bool haveColon=false;
	bool haveBar=false;
	for(unsigned int ui=0;ui<choiceString.size();ui++)
	{
		if(haveColon && haveBar )
		{
			if(choiceString[ui] != ',')
				retStr+=choiceString[ui];
			else
			{
				haveBar=false;
				retStr+=",";
			}
		}
		else
		{
			if(choiceString[ui]==':')
				haveColon=true;
			else
			{
				if(choiceString[ui]=='|')
					haveBar=true;
			}
		}
	}

	return retStr;
}

bool parseColString(const std::string &str,
	unsigned char &r, unsigned char &g, unsigned char &b, unsigned char &a)
{
	//Input string is in 2 char hex form, 3 or 4 colour, with # leading. RGB order
	//lowercase string.
	if(str.size() != 9 && str.size() != 7)
		return false;

	if(str[0] != '#')
		return false;

	string rS,gS,bS,aS;
	rS=str.substr(1,2);
	gS=str.substr(3,2);
	bS=str.substr(5,2);

	if(!isxdigit(rS[0]) || !isxdigit(rS[1]))
		return false;
	if(!isxdigit(gS[0]) || !isxdigit(gS[1]))
		return false;
	if(!isxdigit(bS[0]) || !isxdigit(bS[1]))
		return false;

	hexStrToUChar(str.substr(1,2),r);	
	hexStrToUChar(str.substr(3,2),g);	
	hexStrToUChar(str.substr(5,2),b);	
	//3 colour must have a=255.
	if(str.size() == 7)
		a = 255;
	else
	{
		aS=str.substr(7,2);
		if(!isxdigit(aS[0]) || !isxdigit(aS[1]))
			return false;
		hexStrToUChar(str.substr(7,2),a);	
	}
	return true;
}

void genColString(unsigned char r, unsigned char g, 
			unsigned char b, unsigned char a, std::string &s)
{
	s="#";
	string tmp;
	ucharToHexStr(r,tmp);
	s+=tmp;
	ucharToHexStr(g,tmp);
	s+=tmp;
	ucharToHexStr(b,tmp);
	s+=tmp;
	ucharToHexStr(a,tmp);
	s+=tmp;

}

void genColString(unsigned char r, unsigned char g, 
			unsigned char b, std::string &s)
{
	string tmp;
	s="#";
	ucharToHexStr(r,tmp);
	s+=tmp;
	ucharToHexStr(g,tmp);
	s+=tmp;
	ucharToHexStr(b,tmp);
	s+=tmp;
}
//Strip "whitespace"
std::string stripWhite(const std::string &str)
{
	using std::string;

	size_t start;
	size_t end;
	if(!str.size())
	      return str;

 	start = str.find_first_not_of("\f\n\r\t ");
	end = str.find_last_not_of("\f\n\r\t ");
	if(start == string::npos) 
		return string("");
	else
		return string(str, start, 
				end-start+1);
}

void stripZeroEntries(std::vector<std::string> &sVec)
{
	//Create a truncated vector and reserve mem.
	std::vector<std::string> tVec;
	tVec.reserve(sVec.size());
	std::string s;
	for(unsigned int ui=0;ui<sVec.size(); ui++)
	{
		if(sVec[ui].size())
		{
			tVec.push_back(s);
			tVec.back().swap(sVec[ui]);
		}
	}
	//Swap out the truncated vector with the source
	tVec.swap(sVec);
}	

std::string lowercase(std::string s)
{
	for(unsigned int ui=0;ui<s.size();ui++)
	{
		if(isascii(s[ui]) && isupper(s[ui]))
			s[ui] = tolower(s[ui]);
	}
	return s;
}

//Split strings around a delimiter
void splitStrsRef(const char *cpStr, const char delim,std::vector<string> &v )
{
	const char *thisMark, *lastMark;
	string str;
	v.clear();

	//check for null string
	if(!*cpStr)
		return;
	thisMark=cpStr; 
	lastMark=cpStr;
	while(*thisMark)
	{
		if(*thisMark==delim)
		{
			str.assign(lastMark,thisMark-lastMark);
			v.push_back(str);
		
			thisMark++;
			lastMark=thisMark;
		}
		else
			thisMark++;
	}

	if(thisMark!=lastMark)
	{
		str.assign(lastMark,thisMark-lastMark);
		v.push_back(str);
	}	
		
}

//Split strings around any of a string of delimiters
void splitStrsRef(const char *cpStr, const char *delim,std::vector<string> &v )
{
	const char *thisMark, *lastMark;
	string str;
	v.clear();

	//check for null string
	if(!(*cpStr))
		return;
	thisMark=cpStr; 
	lastMark=cpStr;
	while(*thisMark)
	{
		//Loop over possible delmiters to determine if thiis char is a delimeter
		bool isDelim;
		const char *tmp;
		tmp=delim;
		isDelim=false;
		while(*tmp)
		{
			if(*thisMark==*tmp)
			{
				isDelim=true;
				break;
			}
			tmp++;
		}
		
		if(isDelim)
		{
			str.assign(lastMark,thisMark-lastMark);
			v.push_back(str);
		
			thisMark++;
			lastMark=thisMark;
		}
		else
			thisMark++;
	}

	if(thisMark!=lastMark)
	{
		str.assign(lastMark,thisMark-lastMark);
		v.push_back(str);
	}	
		
}

std::string Colour::asString() const
{
	string s,sTmp;
	stream_cast(sTmp,r);
	s +=sTmp + ",";
	stream_cast(sTmp,g);
	s +=sTmp + ",";
	stream_cast(sTmp,b);
	s +=sTmp +","; 
	stream_cast(sTmp,a);
	s +=sTmp; 

	return s;
}

bool Colour::fromString(const std::string &str)
{
	if(!str.size())
		return false;
	float rTmp,gTmp,bTmp,aTmp;

	vector<string> splitColours;

	splitStrsRef(str.c_str(),',',splitColours);


	//Needs to be an r,g,b and a term
	if(splitColours.size() != 4)
		return false;

	if(stream_cast(rTmp,splitColours[0]))
		return false;

	if(stream_cast(gTmp,splitColours[1]))
		return false;

	if(stream_cast(bTmp,splitColours[2]))
		return false;

	if(stream_cast(aTmp,splitColours[3]))
		return false;

	r=rTmp;
	g=gTmp;
	b=bTmp;
	a=aTmp;
	return true;
}


void Point3D::copyValueArr(float *valArr) const
{
	ASSERT(valArr);
	//compiler should unroll this automatically
	for(unsigned int ui=0; ui<3; ui++)
	{
		*(valArr+ui) = *(value+ui);
	}
}

bool Point3D::operator==(const Point3D &pt) const
{
	return (value[0] == pt.value[0] && value[1] == pt.value[1] && value[2] == pt.value[2]);
}

Point3D Point3D::operator=(const Point3D &pt)
{
	value [0] = pt.value[0];
	value [1] = pt.value[1];
	value [2] = pt.value[2];
	return *this;
}

Point3D Point3D::operator+=(const Point3D &pt)
{
	for(unsigned int ui=0;ui<3; ui++)
		value[ui]+= pt.value[ui];
	
	return *this;
}

Point3D Point3D::operator+(const Point3D &pt) const
{
	Point3D ptTmp;
	
	for(unsigned int ui=0;ui<3; ui++)
		ptTmp.value[ui] = value[ui]  + pt.value[ui];
	
	return ptTmp;
}

Point3D Point3D::operator+(float f) 
{
	for(unsigned int ui=0;ui<3; ui++)
		value[ui] = value[ui]  + f;
	
	return *this;
}

Point3D Point3D::operator-(const Point3D &pt) const
{
	Point3D ptTmp;
	
	for(unsigned int ui=0;ui<3; ui++)
		ptTmp.value[ui] = value[ui]  - pt.value[ui];
	
	return ptTmp;
}

Point3D Point3D::operator-() const
{
	Point3D ptTmp;
	
	for(unsigned int ui=0;ui<3; ui++)
		ptTmp.value[ui] = -value[ui];
	
	return ptTmp;
}

Point3D Point3D::operator*=(const float scale)
{
	value[0] = value[0]*scale;
	value[1] = value[1]*scale;
	value[2] = value[2]*scale;
	
	return *this;
}

Point3D Point3D::operator*(float scale) const
{
	Point3D tmpPt;
	
	tmpPt.value[0] = value[0]*scale;
	tmpPt.value[1] = value[1]*scale;
	tmpPt.value[2] = value[2]*scale;
	
	return tmpPt;
}

Point3D Point3D::operator*(const Point3D &pt) const
{
	Point3D tmpPt;
	
	tmpPt.value[0] = value[0]*pt[0];
	tmpPt.value[1] = value[1]*pt[1];
	tmpPt.value[2] = value[2]*pt[2];
	
	return tmpPt;
}

Point3D Point3D::operator/(float scale) const
{
	Point3D tmpPt;

	scale = 1.0f/scale;
	tmpPt.value[0] = value[0]*scale;
	tmpPt.value[1] = value[1]*scale;
	tmpPt.value[2] = value[2]*scale;

	return tmpPt;
}

float Point3D::sqrDist(const Point3D &pt) const
{
	return (pt.value[0]-value[0])*(pt.value[0]-value[0])+
		(pt.value[1]-value[1])*(pt.value[1]-value[1])+
		(pt.value[2]-value[2])*(pt.value[2]-value[2]);
}
		
float Point3D::dotProd(const Point3D &pt) const
{
	//Return the inner product
	return value[0]*pt.value[0] + value[1]*pt.value[1] + value[2]*pt.value[2];
}

Point3D Point3D::crossProd(const Point3D &pt) const
{
	Point3D cross;

	cross.value[0] = (pt.value[2]*value[1] - pt.value[1]*value[2]);
	cross.value[1] = -(value[0]*pt.value[2] - pt.value[0]*value[2]);
	cross.value[2] = (value[0]*pt.value[1] - value[1]*pt.value[0]);

	return cross;
}

bool Point3D::insideBox(const Point3D &farPoint) const
{
	
	return (value[0] < farPoint.value[0] && value[0] >=0) &&
		(value[1] < farPoint.value[1] && value[1] >=0) &&
		(value[2] < farPoint.value[2] && value[2] >=0);
}

bool Point3D::insideBox(const Point3D &lowPt,const Point3D &highPt) const
{
	
	return (value[0] < highPt.value[0] && value[0] >=lowPt.value[0]) &&
		(value[1] < highPt.value[1] && value[1] >=lowPt.value[1]) &&
		(value[2] < highPt.value[2] && value[2] >=lowPt.value[2]);
}

//This is different to +=, because it generates no return value
void Point3D::add(const Point3D &obj)
{
	value[0] = obj.value[0] + value[0];
	value[1] = obj.value[1] + value[1];
	value[2] = obj.value[2] + value[2];
}

float Point3D::sqrMag() const
{
	return value[0]*value[0] + value[1]*value[1] + value[2]*value[2];
}

Point3D Point3D::normalise()
{
	float mag = sqrt(sqrMag());

	value[0]/=mag;
	value[1]/=mag;
	value[2]/=mag;

	return *this;
}

void Point3D::negate() 
{
	value[0] = -value[0];
	value[1] = -value[1];
	value[2] = -value[2];
}

float Point3D::angle(const Point3D &pt) const
{
	return acos(dotProd(pt)/(sqrt(sqrMag()*pt.sqrMag())));
}

#ifdef __LITTLE_ENDIAN__

void Point3D::switchEndian()
{
	floatSwapBytes(&value[0]);
	floatSwapBytes(&value[1]);
	floatSwapBytes(&value[2]);
}
#endif

std::ostream& operator<<(std::ostream &stream, const Point3D &pt)
{
	stream << "(" << pt.value[0] << "," << pt.value[1] << "," << pt.value[2] << ")";
	return stream;
}

bool Point3D::parse(const std::string &str)
{
	//Needs to be at minimum #,#,#
	if(str.size()< 5)
		return false;

	bool usingBrackets;
	string tmpStr;
	tmpStr=stripWhite(str);

	unsigned int nCommas=0;
	unsigned int commaPos[2];
	unsigned int nStart,nEnd;
	if( tmpStr[0] == '(' && tmpStr[tmpStr.size()-1] == ')')
	{
		//(#,#,#) format
		if(str.size()< 7)
			return false;
		nStart=1;
		nEnd=tmpStr.size()-1;
		usingBrackets=true;
	}
	else
	{
		//(-)#,(-)#,(-)# format : brackets( ) denote optional
		if(!(isdigit(tmpStr[0]) || tmpStr[0] == '-') || 
			!isdigit(tmpStr[tmpStr.size()-1]))
		       return false;

		nStart=0;
		nEnd=tmpStr.size();
		usingBrackets=false;
	}
	for(unsigned int ui=nStart; ui<nEnd; ui++)
	{
		if(tmpStr[ui] == ',')
		{
			commaPos[nCommas]=ui;
			nCommas++;
			if(nCommas > 2)
				return false;
		}
	}
	if(nCommas!=2)
		return false;

	unsigned int length;
	if(usingBrackets)
		length= commaPos[0]-1;
	else
		length= commaPos[0];
	string tmpStrTwo;
	tmpStrTwo =tmpStr.substr(nStart,length);
	
	float p[3];
	if(stream_cast(p[0],tmpStrTwo))
		return false;


	tmpStrTwo =tmpStr.substr(commaPos[0]+1,commaPos[1]-(commaPos[0]+1));
	if(stream_cast(p[1],tmpStrTwo))
		return false;
	
	tmpStrTwo =tmpStr.substr(commaPos[1]+1,nEnd-(commaPos[1]));
	if(stream_cast(p[2],tmpStrTwo))
		return false;

	value[0]=p[0];
	value[1]=p[1];
	value[2]=p[2];

	return true;
}

//========

void BoundCube::setBounds(const std::vector<Point3D> &points)
{
	
	setInverseLimits();	
	for(unsigned int ui=0; ui<points.size(); ui++)
	{
		for(unsigned int uj=0; uj<3; uj++)
		{
			if(points[ui].getValue(uj) < bounds[uj][0])
			{
				{
				bounds[uj][0] = points[ui].getValue(uj);
				valid[uj][0]=true;
				}
			}
			
			if(points[ui].getValue(uj) > bounds[uj][1])
			{
				{
				bounds[uj][1] = points[ui].getValue(uj);
				valid[uj][1]=true;
				}
			}
		}
	}

}


void BoundCube::setInverseLimits()
{
	bounds[0][0] = std::numeric_limits<float>::max();
	bounds[1][0] = std::numeric_limits<float>::max();
	bounds[2][0] = std::numeric_limits<float>::max();
	
	bounds[0][1] = -std::numeric_limits<float>::max();
	bounds[1][1] = -std::numeric_limits<float>::max();
	bounds[2][1] = -std::numeric_limits<float>::max();

	valid[0][0] =false;
	valid[1][0] =false;
	valid[2][0] =false;
	
	valid[0][1] =false;
	valid[1][1] =false;
	valid[2][1] =false;
}
bool BoundCube::isValid() const
{
	for(unsigned int ui=0;ui<3; ui++)
	{
		if(!valid[ui][0] || !valid[ui][1])
			return false;
	}
	return true;
}

void BoundCube::expand(const BoundCube &b) 
{
	//Check both lower and upper limit.
	//Moving to other cubes value as needed

	if(!b.isValid())
		return;

	//If self not valid, ensure that it will be after this run
	//if(!isValid())
	//	setInverseLimits();

	for(unsigned int ui=0; ui<3; ui++)
	{
		if(b.bounds[ui][0] < bounds[ui][0])
		{
		       bounds[ui][0] = b.bounds[ui][0];	
		       valid[ui][0] = true;
		}

		if(b.bounds[ui][1] > bounds[ui][1])
		{
		       bounds[ui][1] = b.bounds[ui][1];	
		       valid[ui][1] = true;
		}
	}
}

void BoundCube::expand(const Point3D &p) 
{
	//If self not valid, ensure that it will be after this run
	//ASSERT(isValid())
	for(unsigned int ui=0; ui<3; ui++)
	{
		//Check lower bound is lower to new pt
		if(bounds[ui][0] > p[ui])
		       bounds[ui][0] = p[ui];

		//Check upper bound is upper to new pt
		if(bounds[ui][1] < p[ui])
		       bounds[ui][1] = p[ui];
	}
}


void BoundCube::setBounds(const Point3D *p, unsigned int n)
{
	bounds[0][0] = std::numeric_limits<float>::max();
	bounds[1][0] = std::numeric_limits<float>::max();
	bounds[2][0] = std::numeric_limits<float>::max();
	
	bounds[0][1] = -std::numeric_limits<float>::max();
	bounds[1][1] = -std::numeric_limits<float>::max();
	bounds[2][1] = -std::numeric_limits<float>::max();
	
	for(unsigned int ui=0;ui<n; ui++)
	{
		for(unsigned int uj=0;uj<3;uj++)
		{
			if(bounds[uj][0] > p[ui][uj])
			{
			       bounds[uj][0] = p[ui][uj];
		       		valid[uj][0] = true;	      
			}	


			if(bounds[uj][1] < p[ui][uj])
			{
			       bounds[uj][1] = p[ui][uj];		
		       		valid[uj][1] = true;	      
			}	
		}	
	}
}

void BoundCube::setBounds( const Point3D &p1, const Point3D &p2)
{
	for(unsigned int ui=0; ui<3; ui++)
	{
		bounds[ui][0]=std::min(p1[ui],p2[ui]);
		bounds[ui][1]=std::max(p1[ui],p2[ui]);
		valid[ui][0]= true;
		valid[ui][1]= true;
	}

}
void BoundCube::getBounds(Point3D &low, Point3D &high) const 
{
	for(unsigned int ui=0; ui<3; ui++) 
	{
		ASSERT(valid[ui][0] && valid[ui][1]);
		low.setValue(ui,bounds[ui][0]);
		high.setValue(ui,bounds[ui][1]);
	}
}

float BoundCube::getSize(unsigned int dim) const
{
	ASSERT(dim < 3);
#ifdef DEBUG
	for(unsigned int ui=0;ui<3; ui++)
	{
		ASSERT(valid[0][1] && valid [0][0]);
	}
#endif
	return fabs(bounds[dim][1] - bounds[dim][0]);
}

bool BoundCube::containsPt(const Point3D &p) const
{
	for(unsigned int ui=0; ui<3; ui++) 
	{
		ASSERT(valid[ui][0] && valid[ui][1]);
		if( p[ui] > bounds[ui][0] && p[ui] < bounds[ui][1])
			return true;
	}

	return false;
}

//checks intersection with sphere [centre,centre+radius)
bool BoundCube::intersects(const Point3D &pt, float sqrRad)
{
	Point3D nearPt;
	
	//Find the closest point on the cube  to the sphere
	unsigned int ui=2;
	const float *val=pt.getValueArr()+ui;
	do
	{
		if(*val <= bounds[ui][0])
		{
			nearPt.setValue(ui,bounds[ui][0]);
			--val;
			continue;
		}
		
		if(*val >=bounds[ui][1])
		{
			nearPt.setValue(ui,bounds[ui][1]);
			--val;
			continue;
		}
	
		nearPt.setValue(ui,*val);
		--val;
	}while(ui--);

	//now test the distance from nrPt to pt
	return (nearPt.sqrDist(pt) < sqrRad);
}

Point3D BoundCube::getCentroid() const
{
#ifdef DEBUG
	for(unsigned int ui=0;ui<3; ui++)
	{
		ASSERT(valid[ui][1] && valid [ui][0]);
	}
#endif
	return Point3D(bounds[0][1] + bounds[0][0],
			bounds[1][1] + bounds[1][0],
			bounds[2][1] + bounds[2][0])/2.0f;
}

float BoundCube::getMaxDistanceToBox(const Point3D &queryPt) const
{
#ifdef DEBUG
	for(unsigned int ui=0;ui<3; ui++)
	{
		ASSERT(valid[ui][1] && valid [ui][0]);
	}
#endif

	float maxDistSqr=0.0f;

	//Could probably be a bit more compact.
	

	//Set lower and upper corners on the bounding rectangle
	Point3D p[2];
	p[0] = Point3D(bounds[0][0],bounds[1][0],bounds[2][0]);
	p[1] = Point3D(bounds[0][1],bounds[1][1],bounds[2][1]);

	//Count binary-wise selecting upper and lower limits, to enumerate all 8 verticies.
	for(unsigned int ui=0;ui<9; ui++)
	{
		maxDistSqr=std::max(maxDistSqr,
			queryPt.sqrDist(Point3D(p[ui&1][0],p[(ui&2) >> 1][1],p[(ui&4) >> 2][2])));
	}

	return sqrt(maxDistSqr);
}

BoundCube BoundCube::operator=(const BoundCube &b)
{
	for(unsigned int ui=0;ui<3; ui++)
	{
		for(unsigned int uj=0;uj<2; uj++)
		{
			valid[ui][uj] = b.valid[ui][uj];
			bounds[ui][uj] = b.bounds[ui][uj];

		}
	}

	return *this;
}

std::ostream &operator<<(std::ostream &stream, const BoundCube& b)
{
	stream << "Bounds :Low (";
	stream << b.bounds[0][0] << ",";
	stream << b.bounds[1][0] << ",";
	stream << b.bounds[2][0] << ") , High (";
	
	stream << b.bounds[0][1] << ",";
	stream << b.bounds[1][1] << ",";
	stream << b.bounds[2][1] << ")" << std::endl;
	
	
	stream << "Bounds Valid: Low (";
	stream << b.valid[0][0] << ",";
	stream << b.valid[1][0] << ",";
	stream << b.valid[2][0] << ") , High (";
	
	stream << b.valid[0][1] << ",";
	stream << b.valid[1][1] << ",";
	stream << b.valid[2][1] << ")" << std::endl;

	return stream;
}

bool getFilesize(const char *fname, size_t  &size)
{
	std::ifstream f(fname);

	if(!f)
		return false;

	f.seekg(0,std::ios::end);

	size = f.tellg();
	return true;
}

void UniqueIDHandler::clear()
{
	idList.clear();
}

unsigned int UniqueIDHandler::getPos(unsigned int id) const
{

	for(list<std::pair<unsigned int, unsigned int> >::const_iterator it=idList.begin();
			it!=idList.end(); it++)
	{
		if(id == it->second)
			return it->first;
	}
	ASSERT(false);
	return 0;
}

void UniqueIDHandler::killByPos(unsigned int pos)
{
	for(list<std::pair<unsigned int, unsigned int> >::iterator it=idList.begin();
			it!=idList.end(); it++)
	{
		if(pos  == it->first)
		{
			idList.erase(it);
			break;
		}
	}

	//Decreement the items, which were further along, in order to maintin the mapping	
	for(list<std::pair<unsigned int, unsigned int> >::iterator it=idList.begin();
			it!=idList.end(); it++)
	{
		if( it->first > pos)
			it->first--;
	}
}

unsigned int UniqueIDHandler::getId(unsigned int pos) const
{
	for(list<std::pair<unsigned int, unsigned int> >::const_iterator it=idList.begin();
			it!=idList.end(); it++)
	{
		if(pos == it->first)
			return it->second;
	}

	ASSERT(false);
	return 0;
}

unsigned int UniqueIDHandler::genId(unsigned int pos)
{
	
	//Look for each element number as a unique value in turn
	//This is guaranteed to return by the pigeonhole principle (we are testing 
	//a larget set (note <=)).
	for(unsigned int ui=0;ui<=idList.size(); ui++)
	{
		bool idTaken;
		idTaken=false;
		for(list<std::pair<unsigned int, unsigned int> >::iterator it=idList.begin();
				it!=idList.end(); it++)
		{
			if(ui == it->second)
			{
				idTaken=true;
				break;
			}
		}

		if(!idTaken)
		{
			idList.push_back(std::make_pair(pos,ui));
			return ui;
		}
	}

	ASSERT(false);
	return 0;
}

void UniqueIDHandler::getIds(std::vector<unsigned int> &idVec) const
{
	//most wordy way of saying "spin through list" ever.
	for(list<std::pair<unsigned int, unsigned int> >::const_iterator it=idList.begin();
			it!=idList.end(); it++)
		idVec.push_back(it->second);
}



#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	#include <windows.h>
	//Windows.h is a nasty name clashing horrible thing.
	//Put it last to avoid clashing with std:: stuff (eg max & min)

#endif

// Total ram in MB
int getTotalRAM()
{
    int ret = 0;
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	MEMORYSTATUS MemStat;

	// Zero structure
	memset(&MemStat, 0, sizeof(MemStat));

	// Get RAM snapshot
	::GlobalMemoryStatus(&MemStat);
	ret= MemStat.dwTotalPhys / (1024*1024);
#elif __APPLE__ || __FreeBSD__

	uint64_t mem;
	size_t len = sizeof(mem);

	sysctlbyname("hw.physmem", &mem, &len, NULL, 0);

	ret = (int)(mem/(1024*1024));
#elif __linux__
	struct sysinfo sysInf;
	sysinfo(&sysInf);
	return ((size_t)(sysInf.totalram)*(size_t)(sysInf.mem_unit)/(1024*1024));
#else
	#error Unknown platform, no getTotalRAM function defined.
#endif
    return ret;
}

int getAvailRAM()
{
    int ret = 0;
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)   
	MEMORYSTATUS MemStat;
	// Zero structure
	memset(&MemStat, 0, sizeof(MemStat));

	// Get RAM snapshot
	::GlobalMemoryStatus(&MemStat);
	ret= MemStat.dwAvailPhys / (1024*1024);

#elif __APPLE__ || __FreeBSD__
	uint64_t mem;
	size_t len = sizeof(mem);

	sysctlbyname("hw.usermem", &mem, &len, NULL, 0);

	ret = (int)(mem/(1024*1024));
#elif __linux__
	{
		struct sysinfo sysInf;
		sysinfo(&sysInf);

		return ((size_t)(sysInf.freeram + sysInf.bufferram)*(size_t)(sysInf.mem_unit)/(1024*1024));
	}
#else
	#error Unknown platform, no getAvailRAM function defined.
#endif
    return ret;
}

bool strhas(const char *cpTest, const char *cpPossible)
{
	const char *search;
	while(*cpTest)
	{
		search=cpPossible;
		while(*search)
		{
			if(*search == *cpTest)
				return true;
			search++;
		}
		cpTest++;
	}

	return false;
}

//A routine for loading numeric data from a text file
unsigned int loadTextData(const char *cpFilename, vector<vector<float> > &dataVec,vector<string> &headerVec, const char delim)
{
	const unsigned int BUFFER_SIZE=4096;
	char inBuffer[BUFFER_SIZE];
	
	unsigned int num_fields=0;

	dataVec.clear();
	//Open a file in text mode
	std::ifstream CFile(cpFilename);

	if(!CFile)
		return ERR_FILE_OPEN_FAIL;

	//DroR the headers, if any
	string str;
	vector<string> strVec;	
	bool atHeader=true;

	vector<string> prevStrs;
	while(!CFile.eof() && atHeader)
	{
		//Grab a line from the file
		CFile.getline(inBuffer,BUFFER_SIZE);

		prevStrs=strVec;
		//Split the strings around the tab char
		splitStrsRef(inBuffer,delim,strVec);		
		
		if(!strhas(inBuffer,"0123456789"))
			continue;

	
		//Skip blank lines or lines that are only spaces
		if(!strVec.size() ||
			(strVec.size() == 1 && strVec[0] == "") )
			continue;

		num_fields = strVec.size();
		dataVec.resize(num_fields);		
		//Check to see if we are in the header
		if(atHeader)
		{
			//If we have the right number of fields 
			//we might be not in the header anymore
			if(num_fields >= 1 && strVec[0].size())
			{
				float f;
				//Assume we are not reading the header
				atHeader=false;
				//Confirm by checking all values
				for(unsigned int ui=0; ui<num_fields; ui++)
				{
					//If any cast fails
					//we are in the header
					if(stream_cast(f,strVec[ui]))
					{
						atHeader=true;
						break;
					}

				}
			}
		}

	}

	stripZeroEntries(prevStrs);
	if(prevStrs.size() == num_fields)
		std::swap(headerVec,prevStrs);


	float f;
	std::stringstream ss;
	while(!CFile.eof())
	{
		if(strhas(inBuffer,"0123456789"))
		{
			//Split the strings around the tab char
			splitStrsRef(inBuffer,delim,strVec);	
			stripZeroEntries(strVec);
			
			//Check the number of fields	
			//=========
			if(strVec.size() != num_fields)
			{
				return ERR_FILE_FORMAT_FAIL;	
			}
	

			for(unsigned int ui=0; ui<num_fields; ui++)
			{	
				ss.clear();
				ss.str(strVec[ui]);
				ss >> f;
				if(ss.fail())
					return ERR_FILE_FORMAT_FAIL;
				dataVec[ui].push_back(f);
			
			}
			//=========
			
		}
		//Grab a line from the file
		CFile.getline(inBuffer,BUFFER_SIZE);
	}

	if(atHeader)
		return ERR_FILE_FORMAT_FAIL;

	return 0;
}
