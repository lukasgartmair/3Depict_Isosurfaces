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

#include "wxcommon.h"


#include "backend/filters/allFilter.h"
#include "backend/configFile.h"


#include "common/stringFuncs.h"
#include "common/xmlHelper.h"


const char *TESTING_RESOURCE_DIRS[] = {
		"../test/",
		"./test/"
};

//!Run each filter through its own unit test function
bool filterTests();
//!Try cloning the filter from itself, and checking the filter
// clone is identical
bool filterCloneTests();

//!Try loading each range file in the testing folder
bool rangeFileLoadTests();

//!Some elementary function testing
bool basicFunctionTests() ;

//!Run a few checks on our XML helper functions
bool XMLTests();

//!basic filter tree topology tests
bool filterTreeTests();


bool testFilterTree(const FilterTree &f)
{
	ASSERT(!f.hasHazardousContents());
	std::list<std::pair<Filter *, std::vector<const FilterStreamData * > > > outData;
	std::vector<SelectionDevice *> devices;
	std::vector<std::pair<const Filter *, string > > consoleMessages;

	ProgressData prog;
	if(f.refreshFilterTree(outData,devices,consoleMessages,prog,dummyCallback))
	{
		f.safeDeleteFilterList(outData);
		return false;
	}


	typedef std::pair<Filter *, std::vector<const FilterStreamData * > > FILTER_PAIR;

	for(list<FILTER_PAIR>::iterator it=outData.begin();
			it!=outData.end();++it)
	{
		cerr << it->first->getUserString() << ":" << endl;
		for(size_t ui=0;ui<it->second.size();ui++)
		{
			size_t streamType;
			streamType=ilog2(it->second[ui]->getStreamType());
			ASSERT(streamType<NUM_STREAM_TYPES);



			//Print out the stream name, and the number of objects it contains
			cerr << "\t" << STREAM_NAMES[streamType] << " " <<
				"\t" << it->second[ui]->getNumBasicObjects() << endl;
		}
	}


	//TODO: report on Xml contents
	
	
	f.safeDeleteFilterList(outData);

	return true;
}


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

	cerr << " OK" << endl << endl;

	return true;
}

bool filterTests()
{
	//Instantiate various filters, then run their unit tests
	for(unsigned int ui=0; ui<FILTER_TYPE_ENUM_END; ui++)
	{
		Filter *f;
	       	f= makeFilter(ui);
		if(!f->runUnitTests())
			return false;

		delete f;
	}

	if(!Filter::boolToggleTests())
		return false;

	if(!Filter::helpStringTests())
		return false;

	return true;
}

bool filterCloneTests()
{
	//Run the clone/uncloned versions of filter write functions
	//against each other and ensure
	//that their XML output is the same. Then check against
	//the read function.
	//
	// Without a user config file (with altered defaults), this is not a
	// "strong" test, as nothing is being altered   inside the filter after 
	// instantiation in the default case -- stuff can still be missed 
	// in the cloneUncached, and won't be detected, but it does prevent cross-wiring. 
	//
	ConfigFile configFile;
    	configFile.read();
	
	bool fileWarn=false;
	for(unsigned int ui=0; ui<FILTER_TYPE_ENUM_END; ui++)
	{
		//Get the user's preferred, or the program
		//default filter.
		Filter *f = configFile.getDefaultFilter(ui);
		
		//now attempt to clone the filter, and write both XML outputs.
		Filter *g;
		g=f->cloneUncached();

		//create the files
		string sOrig,sClone;
		{

			wxString wxs,tmpStr;

			tmpStr=wxT("3Depict-unit-test-a");
			tmpStr=tmpStr+wxStr(f->getUserString());
			
			wxs= wxFileName::CreateTempFileName(tmpStr);
			sOrig=stlStr(wxs);
		
			//write out one file from original object	
			ofstream fileOut(sOrig.c_str());
			if(!fileOut)
			{
				//Run a warning, but only once.
				WARN(fileWarn,"unable to open output xml file for xml test");
				fileWarn=true;
			}

			f->writeState(fileOut,STATE_FORMAT_XML);
			fileOut.close();

			//write out file from cloned object
			tmpStr=wxT("3Depict-unit-test-b");
			tmpStr=tmpStr+wxStr(f->getUserString());
			wxs= wxFileName::CreateTempFileName(tmpStr);
			sClone=stlStr(wxs);
			fileOut.open(sClone.c_str());
			if(!fileOut)
			{
				//Run a warning, but only once.
				WARN(fileWarn,"unable to open output xml file for xml test");
				fileWarn=true;
			}

			g->writeState(fileOut,STATE_FORMAT_XML);
			fileOut.close();



		}

		//Now run diff
		//------------
		string command;
		command = string("diff \'") +  sOrig + "\' \'" + sClone + "\'";

		wxArrayString stdOut;
		long res;
#if wxCHECK_VERSION(2,9,0)
		res=wxExecute(wxStr(command),stdOut, wxEXEC_BLOCK);
#else
		res=wxExecute(wxStr(command),stdOut);
#endif


		string comment = f->getUserString() + string(" Orig: ")+ sOrig + string (" Clone:") +sClone+
			string("Cloned filter output was different... (or diff not around?)");
		TEST(res==0,comment.c_str());
		//-----------
	
		//Check both files are valid XML
		TEST(isValidXML(sOrig.c_str()) ==true,"XML output of filter not valid...");	

		//Now, try to re-read the XML, and get back the filter,
		//then write it out again.
		//---
		{
			xmlDocPtr doc;
			xmlParserCtxtPtr context;
			context =xmlNewParserCtxt();
			if(!context)
			{
				WARN(false,"Failed allocating XML context");
				return false;
			}

			//Open the XML file
			doc = xmlCtxtReadFile(context, sClone.c_str(), NULL,0);

			//release the context
			xmlFreeParserCtxt(context);

			//retrieve root node	
			xmlNodePtr nodePtr = xmlDocGetRootElement(doc);

			//Read the state file, then re-write it!
			g->readState(nodePtr);

			xmlFreeDoc(doc);

			ofstream fileOut(sClone.c_str());
			g->writeState(fileOut,STATE_FORMAT_XML);

			//Re-run diff
#if wxCHECK_VERSION(2,9,0)
			res=wxExecute(wxStr(command),stdOut, wxEXEC_BLOCK);
#else
			res=wxExecute(wxStr(command),stdOut);
#endif
			comment = f->getUserString() + string("Orig: ")+ sOrig + string (" Clone:") +sClone+
				string("Read-back filter output was different... (or diff not around?)");
			TEST(res==0,comment.c_str());
		}
		//----
		//clean up
		wxRemoveFile(wxStr(sOrig));
		wxRemoveFile(wxStr(sClone));

		delete f;
		delete g;
	}
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

bool filterTreeTests()
{
	FilterTree fTree;


	Filter *fA = new IonDownsampleFilter;
	Filter *fB = new IonDownsampleFilter;
	Filter *fC = new IonDownsampleFilter;
	Filter *fD = new IonDownsampleFilter;
	//Tree layout:
	//A
	//-> B
	//  -> D
	//-> C
	fTree.addFilter(fA,0);
	fTree.addFilter(fB,fA);
	fTree.addFilter(fC,fA);
	fTree.addFilter(fD,fB);
	TEST(fTree.size() == 4,"Tree construction");
	TEST(fTree.maxDepth() == 2, "Tree construction");

	
	//Copy B's child to B.
	//A
	//-> B
	//  -> D
	//  -> E
	//-> C
	size_t oldSize;
	oldSize=fTree.size();
	TEST(fTree.copyFilter(fD,fB),"copy test");
	TEST(oldSize+1 == fTree.size(), "copy test");
	TEST(fTree.maxDepth() == 2, " copy test");

	//Remove B from tree
	fTree.removeSubtree(fB);
	TEST(fTree.size() == 2, "subtree remove test");
	TEST(fTree.maxDepth() == 1, "subtree remove test");

	fTree.clear();

	Filter *f[4];
	f[0] = new IonDownsampleFilter;
	f[1] = new IonDownsampleFilter;
	f[2] = new IonDownsampleFilter;
	f[3] = new IonDownsampleFilter;

	for(unsigned int ui=0;ui<4;ui++)
	{
		std::string s;
		stream_cast(s,ui);
		f[ui]->setUserString(s);
	}

	//build a new tree arrangement
	//0
	// ->3
	//1
	// ->2
	fTree.addFilter(f[0],0);
	fTree.addFilter(f[1],0);
	fTree.addFilter(f[2],f[1]);
	fTree.addFilter(f[3],f[0]);

	fTree.reparentFilter(f[1],f[3]);

	//Now :
	// 0
	//   ->3
	//       ->1
	//          ->2
	TEST(fTree.size() == 4,"reparent test")
	TEST(fTree.maxDepth() == 3, "reparent test");
	for(unsigned int ui=0;ui<4;ui++)
	{
		TEST(fTree.contains(f[ui]),"reparent test");
	}

	FilterTree fSpareTree=fTree;
	fTree.addFilterTree(fSpareTree,f[2]);


	TEST(fTree.maxDepth() == 7,"reparent test");

	//Test the swap function
	FilterTree fTmp;
	fTmp=fTree;
	//swap spare with working
	fTree.swap(fSpareTree);
	//spare should now be the same size as the original working
	TEST(fSpareTree.maxDepth() == fTmp.maxDepth(),"filtertree swap");
	//swap working back
	fTree.swap(fSpareTree);
	TEST(fTree.maxDepth() == fTmp.maxDepth(),"filtertree swap");


	bool needUp;
	ASSERT(fTree.setFilterProperty(f[0],KEY_IONDOWNSAMPLE_FRACTION,"0.5",needUp)
	 || fTree.setFilterProperty(f[0],KEY_IONDOWNSAMPLE_COUNT,"10",needUp));

	std::string tmpName;
	if(!genRandomFilename(tmpName) )
	{
		//Build an XML file containing the filter tree
		// then try to load it
		try
		{
			//Create an XML file
			std::ofstream tmpFile(tmpName.c_str());

			if(!tmpFile)
				throw false;
			tmpFile << "<testXML>" << endl;
			//Dump tree contents into XML file
			std::map<string,string> dummyMap;
			if(fTree.saveXML(tmpFile,dummyMap,false,true))
			{
				tmpFile<< "</testXML>" << endl;
			}
			else
			{
			WARN(true,"Unable to write to random file in current folder. skipping test");
			}
		

			//Reparse tree
			//---
			xmlDocPtr doc;
			xmlParserCtxtPtr context;

			context =xmlNewParserCtxt();


			if(!context)
			{
				cout << "Failed to allocate parser" << std::endl;
				throw false;
			}

			//Open the XML file
			doc = xmlCtxtReadFile(context, tmpName.c_str(), NULL,0);

			if(!doc)
				throw false;
			
			//release the context
			xmlFreeParserCtxt(context);
			//retrieve root node	
			xmlNodePtr nodePtr = xmlDocGetRootElement(doc);
			if(!nodePtr)
				throw false;
			
			nodePtr=nodePtr->xmlChildrenNode;
			if(!nodePtr)
				throw false;

			//find filtertree data
			if(XMLHelpFwdToElem(nodePtr,"filtertree"))
				throw false;
			
			TEST(!fTree.loadXML(nodePtr,cerr,""),"Tree load test");
			//-----
		}
		catch(bool)
		{
			WARN(false,"Couldn't run XML reparse of output file");
			wxRemoveFile(wxStr(tmpName));
			return false;
		}
		wxRemoveFile(wxStr(tmpName));
	}
	else
	{
		WARN(true,"Unable to open random file in current folder. skipping a test");
	}

	return true;
}



#endif
