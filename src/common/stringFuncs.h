/*
 *	common/stringFuncs.h - String manipulation header 
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

#ifndef STRINGFUNCS_H
#define STRINGFUNCS_H

#include <string>
#include <vector>

//generate a semi-random string (not strong random), returns true
// if a file that could be opened was found
// this is useful for creating temp files
bool genRandomFilename(std::string &s,bool timerInitRand=true);

//Convert a boolean to "1" or "0"
std::string boolStrEnc(bool b);

//Convert an input string "0" or "1" into its boolean value. 
//	Whitespace is stripped from either end. If string cannot be understood, returns false
bool boolStrDec(const std::string &s,bool &result);

//!Generate string that can be parsed by wxPropertyGrid for combo control
//String format is CHOICEID:id1|string 1,id2|string 2,id3|string 3,.....,idN|string_N
// where id1->idN are integers
std::string choiceString(std::vector<std::pair<unsigned int, std::string> > comboString, 
								unsigned int curChoice);

//Convert a choice string, such as generated by "choiceString", into a vector of strings
void choiceStringToVector(const std::string &str,
		std::vector<std::string> &choices, unsigned int &selected);

#ifdef DEBUG
//!Obtain whether or not the string is a choice string
bool isMaybeChoiceString(const std::string &s);
#endif
//!Generate a string with leading digits up to maxDigit (eg, if maxDigit is 424, and thisDigit is 1
//leading digit will generate the string 001
std::string digitString(unsigned int thisDigit, unsigned int maxDigit);

//!Returns Choice from string (see choiceString(...) for string format)
std::string getActiveChoice(const std::string &choiceString);

//Strip given whitespace (\f,\n,\r,\t,\ )from a string
std::string stripWhite(const std::string &str);
//Strip specified chars from a string
std::string stripChars(const std::string &Str, const char *chars);
//!Return a lowercase version for a given string
std::string lowercase(std::string s);
//!Return a uppercase version for a given string
std::string uppercase(std::string s);

//Drop empty entries from a string of vector
void stripZeroEntries(std::vector<std::string> &s);


//Parse a colour string, such as #aabbccdd into its RGBA 8-bit components
bool parseColString(const std::string &str,
	unsigned char &r, unsigned char &g, unsigned char &b, unsigned char &a);

//Convert an RGBA 8-bit/channel quadruplet into its hexadecimal colour string
// format is "#rrggbbaa" such as "#11ee00aa"
void genColString(unsigned char r, unsigned char g, 
			unsigned char b, unsigned char a, std::string &s);
//Convert an RGB 8-bit/channel quadruplet into its hexadecimal colour string
// format is "#rrggbb" such as "#11ee00"
void genColString(unsigned char r, unsigned char g, 
			unsigned char b, std::string &s);

//Check to see if a given string is a valid version number string,
// consisting of decmials and ints (eg 0.1.2.3.4)
bool isVersionNumberString(const std::string &s);

//!Retrieve the maximum version string from a list of version strings
// version strings are digit and decimal point (.) only
std::string getMaxVerStr(const std::vector<std::string> &verStrings);

//!Strip whitespace, (eg tab,space) from either side of a string
std::string stripWhite(const std::string &str);

//!Split string references using a single delimiter.
void splitStrsRef(const char *cpStr, const char delim,std::vector<std::string> &v );

//!Split string references using any of a given string of delimiters
void splitStrsRef(const char *cpStr, const char *delim,std::vector<std::string> &v );

//!Return only the filename component
std::string onlyFilename( const std::string& path );
//!Return only  the directory name component of the full path 
// - do not use with UNC windows paths
std::string onlyDir( const std::string& path );

//!Convert a path format into a native path from unix format
// - do not use with UNC windows paths
std::string convertFileStringToNative(const std::string &s);

//!Convert a path format into a unix path from native format
std::string convertFileStringToCanonical(const std::string &s);

//Print N tabs to a string
inline std::string tabs(unsigned int nTabs)
{
	std::string s;
	s.resize(nTabs);
	std::fill(s.begin(),s.end(),'\t');
	return s;
}

//Brute force convert a wide STL str to a normal stl str
inline std::string stlWStrToStlStr(const std::wstring& s)
{
	std::string temp(s.length(),' ');
	std::copy(s.begin(), s.end(), temp.begin());
	return temp; 
}
//Brute force convert an stlStr to an stl wide str
inline std::wstring stlStrToStlWStr(const std::string& s)
{
	std::wstring temp(s.length(),L' ');
	std::copy(s.begin(), s.end(), temp.begin());
	return temp;
}

#ifdef DEBUG
//Run unit tests for string funcs 
bool testStringFuncs();
#endif

#endif
