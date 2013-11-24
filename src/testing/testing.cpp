/*
 *	testing.cpp - unit testing implementation
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


#include "testing.h"
#ifdef DEBUG
#include <wx/filename.h>
#include <wx/dir.h>

#include "wx/wxcommon.h"


#include "backend/filters/allFilter.h"
#include "backend/state.h"
#include "backend/configFile.h"
#include "backend/filters/algorithms/binomial.h"
#include "backend/APT/ionhit.h"

#include "common/stringFuncs.h"
#include "common/xmlHelper.h"


const char *TESTING_RESOURCE_DIRS[] = {
		"../test/",
		"./test/"
};

#include "filtertesting.cpp"


//!Try loading each range file in the testing folder
bool rangeFileLoadTests();

//!Some elementary function testing
bool basicFunctionTests() ;

//!Run a few checks on our XML helper functions
bool XMLTests();


bool basicFunctionTests()
{
	testStringFuncs();

	//Test point parsing routines
	{
	std::string testStr;
	testStr="0.0,1.0,1";
	Point3D p;
	bool res=p.parse(testStr);
	ASSERT(res);
	ASSERT(p.sqrDist(Point3D(0,1,1)) < 0.1f);


	//test case causes segfault : found 30/9/12
	testStr="0,0,,";
	res=p.parse(testStr);
	ASSERT(!res);

	testStr="(0,0,0)";
	res=p.parse(testStr);
	ASSERT(res);
	ASSERT(p.sqrDist(Point3D(0,0,0))<0.01f);

	}

	//Test some basics routines
	{
	TEST(rangesOverlap(0,3,1,2),"Overlap test a contain b");
	TEST(rangesOverlap(1,2,0,3),"Overlap test b contain a");
	TEST(rangesOverlap(0,2,1,3),"Overlap test a partial b (low)");
	TEST(rangesOverlap(1,3,0,2),"Overlap test b partial a (high)");
	TEST(rangesOverlap(2,3,1,4),"Overlap test a partial b (high)");
	TEST(rangesOverlap(1,3,2,4),"Overlap test b partial a (low)");
	TEST(!rangesOverlap(1,2,3,4),"Overlap test");
	TEST(!rangesOverlap(3,4,1,2),"Overlap test");
	}

	//Test the LFSR to a small extent (first 16 table entries)
	// -test is brute-force so we can't test much more without being slow
	LinearFeedbackShiftReg reg;
	TEST(reg.verifyTable(16),"Check LFSR table integrity");

	
	return true;
}

bool runUnitTests()
{

	cerr << "Running unit tests..." ;


	if(!testIonHit())
		return false;

	if(!filterTests())
		return false;

	if(!filterCloneTests())
		return false;

	if(!rangeFileLoadTests())
		return false;


	if(!basicFunctionTests())
		return false;
	
	if(!XMLTests())
		return false;

	if(!filterTreeTests())
		return false;

	if(!runVoxelTests())
		return false;

	if(!testBinomial())
		return false;

	if(!runStateTests())
		return false;

	cerr << " OK" << endl << endl;

	return true;
}

bool rangeFileLoadTests()
{
	//try to load all rng, rrng, and env files in ../tests or ./tests/
	//whichever is first. 
	wxString testDir;
	bool haveDir=false;
	for(unsigned int ui=0;ui<THREEDEP_ARRAYSIZE(TESTING_RESOURCE_DIRS);ui++)
	{
		testDir=wxCStr(TESTING_RESOURCE_DIRS[ui]);
		if(wxDirExists(testDir))
		{
			haveDir=true;
			break;
		}
	}

	if(!haveDir)
	{
		WARN(false,"Unable to locate testing resource dir, unable to perform some tests");
		return true;
	}

	testDir=testDir+wxT("rangefiles/");

	wxArrayString arrayStr,tmpArr;
	//Get all the files matching rng extensions
	vector<string> rangeExts;
	RangeFile::getAllExts(rangeExts);
	for(unsigned int ui=0;ui<rangeExts.size();ui++)
	{
		std::string tmp;
		tmp = std::string("*.") + rangeExts[ui];
		
		wxDir::GetAllFiles(testDir,&tmpArr,wxStr(tmp));
		for(unsigned int uj=0;uj<tmpArr.GetCount();uj++)
			arrayStr.Add(tmpArr[uj]);
		tmpArr.clear();
	}

	if(!arrayStr.GetCount())
	{
		WARN(false,"Unable to locate test range files, unable to perform some tests");
		return true;
	}


	//Map names of file (without dir) to number of ions/range
	map<string,unsigned int> ionCountMap;
	map<string,unsigned int> rangeCountMap;
	//set that contains list of entries that should fail
	set<string> failSet;

	ionCountMap["test1.rng"]=10; rangeCountMap["test1.rng"]=6;
	ionCountMap["test2.rng"]=7; rangeCountMap["test2.rng"]=9; 
	ionCountMap["test3.rng"]=19; rangeCountMap["test3.rng"]=59;
	failSet.insert("test4.rng");
	ionCountMap["test5.rng"]=4; rangeCountMap["test5.rng"]=2;
	//After discussion with a sub-author of 
	// "Atom Probe Microscopy". ISBN 1461434351
	// and author of the RNG entry in the book,
	// it was agreed that the file shown in the book is invalid.
	// Multiple ions cannot be assigned in this fashion,
	// as there is no naming or colour data to match to
	failSet.insert("test6.rng");
	ionCountMap["test7.rng"]=2; rangeCountMap["test7.rng"]=2;
	ionCountMap["test8.rng"]=2; rangeCountMap["test8.rng"]=2;
	ionCountMap["test9.rng"]=3; rangeCountMap["test9.rng"]=3;
	ionCountMap["test10.rng"]=3; rangeCountMap["test10.rng"]=3;
	ionCountMap["test11.rng"]=5; rangeCountMap["test11.rng"]=10;
	ionCountMap["test12.rng"]=5; rangeCountMap["test12.rng"]=10;

	ionCountMap["test1.rrng"]=1; rangeCountMap["test1.rrng"]=1;
	ionCountMap["test2.rrng"]=3; rangeCountMap["test2.rrng"]=6; 
	ionCountMap["test3.rrng"]=8; rangeCountMap["test3.rrng"]=42; 
	ionCountMap["test4.rrng"]=14; rangeCountMap["test4.rrng"]=15; 
	ionCountMap["test5.rrng"]=1; rangeCountMap["test5.rrng"]=1; 
	ionCountMap["test6.rrng"]=2; rangeCountMap["test6.rrng"]=4; 
	
	ionCountMap["test1.env"]=1; rangeCountMap["test1.env"]=1; 

	//Sort the array before we go any further, so that the output
	//each time is the same, regardless of how the files were
	//loaded into the dir. F.ex this makes diffing easier
	arrayStr.Sort();
	//Now, check to see if each file is in fact a valid, loadable range file
	for(unsigned int ui=0;ui<arrayStr.GetCount();ui++)
	{
		std::string fileLongname, fileShortname;
		fileLongname=stlStr(arrayStr[ui]);

		wxFileName filename;
		filename=wxStr(fileLongname);
		//This returns the short name of the file. Yes, its badly named.
		fileShortname=stlStr(filename.GetFullName());
		{
			RangeFile f;

			bool shouldSucceed;

			//check to see if we have a failure entry for this rangefile
			// if its not in the set, it should load successfully
			shouldSucceed=(failSet.find(fileShortname)==failSet.end());
				
			if(!(f.openGuessFormat(fileLongname.c_str()) == shouldSucceed))
			{
				cerr << "\t" << fileShortname.c_str() << "...";
				cerr << f.getErrString() << endl;
				TEST(false,"range file load test"); 
			}


			if(!shouldSucceed)
				continue;

			//Check against the hand-made map of ion and range counts
			if(ionCountMap.find(fileShortname)!=ionCountMap.end())
			{
				std::string errMsg;
				errMsg=string("ion count test : ") + fileShortname;
				TEST(ionCountMap[fileShortname] == f.getNumIons(),errMsg.c_str());
			}
			else
			{
				cerr << "\t" << fileShortname.c_str() << "...";
				WARN(false,"Did not know how many ions file was supposed to have. Test inconclusive");
			}
			
			if(rangeCountMap.find(fileShortname)!=rangeCountMap.end())
			{
				std::string errMsg;
				errMsg=string("range count test : ") + fileShortname;
				TEST(rangeCountMap[fileShortname] == f.getNumRanges(),errMsg.c_str());
			}
			else
			{
				cerr << "\t" << fileShortname.c_str() << "...";
				WARN(false,"Did not know how many ranges file was supposed to have. Test inconclusive");
			}
		}
	}



	map<string,int> typeMapping;

	typeMapping["test1.rng"]=RANGE_FORMAT_ORNL;
	typeMapping["test2.rng"]=RANGE_FORMAT_ORNL; 
	typeMapping["test3.rng"]=RANGE_FORMAT_ORNL;
	typeMapping["test5.rng"]=RANGE_FORMAT_ORNL; 
	typeMapping["test7.rng"]=RANGE_FORMAT_ORNL; 
	typeMapping["test8.rng"]=RANGE_FORMAT_ORNL; 
	typeMapping["test9.rng"]=RANGE_FORMAT_ORNL; 
	typeMapping["test10.rng"]=RANGE_FORMAT_ORNL;
	typeMapping["test11.rng"]=RANGE_FORMAT_ORNL;
	typeMapping["test12.rng"]=RANGE_FORMAT_DBL_ORNL; 
	typeMapping["test1.rrng"]=RANGE_FORMAT_RRNG;
	typeMapping["test2.rrng"]=RANGE_FORMAT_RRNG;
	typeMapping["test3.rrng"]=RANGE_FORMAT_RRNG;
	typeMapping["test4.rrng"]=RANGE_FORMAT_RRNG;
	typeMapping["test5.rrng"]=RANGE_FORMAT_RRNG;
	typeMapping["test6.rrng"]=RANGE_FORMAT_RRNG;
	typeMapping["test1.env"]=RANGE_FORMAT_ENV; 


	for(unsigned int ui=0;ui<arrayStr.GetCount();ui++)
	{
		std::string fileLongname, fileShortname;
		wxFileName filename;
		
		fileLongname=stlStr(arrayStr[ui]);
		filename=wxStr(fileLongname);
		fileShortname=stlStr(filename.GetFullName());

		//Check to see that the auto-parser correctly identifies the type
		if(typeMapping.find(fileShortname) != typeMapping.end())
		{
			std::string errString;
			errString="Range type detection : ";
			errString+=fileLongname;

			if(!wxFileExists(wxStr(fileLongname)))
			{
				cerr << "File expected, but not found during test:" <<
					fileLongname << endl;
				continue;
			}
			

			TEST(RangeFile::detectFileType(fileLongname.c_str()) == 
					typeMapping[fileShortname], errString);
		}
	}
	return true;
}

bool XMLTests()
{
	vector<std::string> v;
	v.push_back("<A & B>");
	v.push_back(" \"\'&<>;");
	v.push_back("&amp;");


	for(unsigned int ui=0;ui<v.size();ui++)
	{
		TEST(unescapeXML(escapeXML(v[ui])) == v[ui],"XML unescape round-trip test");
	}

	return true;
}



#endif
