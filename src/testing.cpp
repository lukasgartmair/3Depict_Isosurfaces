#include "testing.h"
#ifdef DEBUG
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/dir.h>
#include <wx/arrstr.h>

#include "wxcommon.h"

#include <string>

#include "filters/allFilter.h"
#include "configFile.h"

#include "xmlHelper.h"

#include "filtertree.h"

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

//!Run a few checks on our XML helper functions
bool XMLTests();

//!basic filter tree topology tests
bool filterTreeTests();

bool runUnitTests()
{
	if(!filterTests())
		return false;

	if(!filterCloneTests())
		return false;

	if(!rangeFileLoadTests())
		return false;

	if(!XMLTests())
		return false;

	if(!filterTreeTests())
		return false;
	return true;
}

bool filterTests()
{
	//Instantiate various filters, then run their unit tests
	cerr << "Running per-filter unit tests...";
	for(unsigned int ui=0; ui<FILTER_TYPE_ENUM_END; ui++)
	{
		Filter *f;
	       	f= makeFilter(ui);
		if(!f->runUnitTests())
			return false;

		delete f;
	}
	cerr << "OK" <<endl;

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
	cerr << "Running clone tests...";
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
	cerr << "OK" << endl;
	return true;
}

bool rangeFileLoadTests()
{
	//try to load all rng, rrng, and env files in ../tests or ./tests/
	//whichever is first. 
	wxString testDir;
	bool haveDir=false;
	for(unsigned int ui=0;ui<ARRAYSIZE(TESTING_RESOURCE_DIRS);ui++)
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
	//Get all the files matching rng extentions
	vector<string> rangeExts;
	RangeFile::getAllExts(rangeExts);
	for(unsigned int ui=0;ui<rangeExts.size();ui++)
	{
		wxDir::GetAllFiles(testDir,&tmpArr,wxStr(rangeExts[ui]));
		for(unsigned int uj=0;uj<tmpArr.GetCount();uj++)
			arrayStr.Add(tmpArr[uj]);
	}

	if(!arrayStr.GetCount())
	{
		WARN(false,"Unable to locate test range files, unable to perform some tests");
		return true;
	}

	cerr << "Range file loading...";

	//Map names of file (without dir) to number of ions/range
	map<string,unsigned int> ionCountMap;
	map<string,unsigned int> rangeCountMap;

	ionCountMap["test1.rng"]=10; rangeCountMap["test1.rng"]=6;
	ionCountMap["test2.rng"]=7; rangeCountMap["test2.rng"]=9;

	ionCountMap["test1.rrng"]=1; rangeCountMap["test1.rrng"]=1;
	ionCountMap["test2.rrng"]=3; rangeCountMap["test2.rrng"]=6;
	ionCountMap["test3.rrng"]=8; rangeCountMap["test3.rrng"]=42;
	ionCountMap["test4.rrng"]=14; rangeCountMap["test4.rrng"]=15;
	
	ionCountMap["test1.env"]=1; rangeCountMap["test1.env"]=1;

	cerr << endl;
	//Sort the array before we go any further, so that the output
	//each time is the same, regardless of how the files were
	//loaded into the dir. F.ex this makes diffing easier
	arrayStr.Sort();
	//Now, check to see if each file is in fact a valid, loadable range file
	for(unsigned int ui=0;ui<arrayStr.GetCount();ui++)
	{
		std::string s;
		s=stlStr(arrayStr[ui]);
		{
			RangeFile f;
			cerr << "\t" << s.c_str() << "...";
			if(!f.openGuessFormat(s.c_str()))
			{
				cerr << f.getErrString() << endl;
				TEST(false,"range file load test"); 
			}

			//Check against the hand-made map of ion and range counts
			wxFileName filename;
			filename=wxStr(s);
			s=stlStr(filename.GetFullName());
			if(ionCountMap.find(s)!=ionCountMap.end())
			{
				TEST(ionCountMap[s] == f.getNumIons(),"ion count test");
			}
			else
			{
				WARN(false,"Did not know how many ions file was supposed to have. Test inconclusive");
			}
			
			if(rangeCountMap.find(s)!=rangeCountMap.end())
			{
				TEST(rangeCountMap[s] == f.getNumRanges(),"range count test");
			}
			else
			{
				WARN(false,"Did not know how many ranges file was supposed to have. Test inconclusive");
			}

			cerr << "OK" << endl;
		}
	}

	cerr << "OK" << endl;
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
	size_t oldSize,newSIze;
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

	//build a new tree arrangemnet
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
	return true;
}

#endif
