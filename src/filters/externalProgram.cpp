#include "externalProgram.h"
#include "../wxcommon.h"

#include "../xmlHelper.h"

#include "../translation.h"

#include <wx/process.h>
#include <wx/filename.h>
#include <wx/dir.h>

enum
{
	KEY_COMMAND,
	KEY_WORKDIR,
	KEY_ALWAYSCACHE,
	KEY_CLEANUPINPUT
};

//!Error codes
enum
{
	COMMANDLINE_FAIL=1,
	SETWORKDIR_FAIL,
	WRITEPOS_FAIL,
	WRITEPLOT_FAIL,
	MAKEDIR_FAIL,
	PLOTCOLUMNS_FAIL,
	READPLOT_FAIL,
	READPOS_FAIL,
	SUBSTITUTE_FAIL,
	COMMAND_FAIL, 
};

//=== External program filter === 
ExternalProgramFilter::ExternalProgramFilter()
{
	alwaysCache=false;
	cleanInput=true;
	cacheOK=false;
	cache=false; 
}

Filter *ExternalProgramFilter::cloneUncached() const
{
	ExternalProgramFilter *p=new ExternalProgramFilter();

	//Copy the values
	p->workingDir=workingDir;
	p->commandLine=commandLine;
	p->alwaysCache=alwaysCache;
	p->cleanInput=cleanInput;

	//We are copying wether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

size_t ExternalProgramFilter::numBytesForCache(size_t nObjects) const
{
	if(alwaysCache)
		return 0;
	else
		return (size_t)-1; //Say we don't know, we are not going to cache anyway.
}

unsigned int ExternalProgramFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(bool))
{
	//use the cached copy if we have it.
	if(cacheOK)
	{
		for(unsigned int ui=0;ui<filterOutputs.size();ui++)
			getOut.push_back(filterOutputs[ui]);

		return 0;
	}

	vector<string> commandLineSplit;

	splitStrsRef(commandLine.c_str(),' ',commandLineSplit);
	//Nothing to do
	if(commandLineSplit.empty())
		return 0;

	vector<string> ionOutputNames,plotOutputNames;

	//Compute the bounding box of the incoming streams
	string s;
	wxString tempDir;
	if(workingDir.size())
		tempDir=(wxStr(workingDir) +wxT("/inputData"));
	else
		tempDir=(wxT("inputData"));


	//Create a temporary dir
	if(!wxDirExists(tempDir) )
	{
		//Audacity claims that this can return false even on
		//success (NoiseRemoval.cpp, line 148).
		//I was having problems with this function too;
		//so use their workaround
		wxMkdir(tempDir);

		if(!wxDirExists(tempDir) )	
			return MAKEDIR_FAIL;

	}
	
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS:
			{
				const IonStreamData *i;
				i = (const IonStreamData * )(dataIn[ui]);

				if(i->data.empty())
					break;
				//Save the data to a file
				wxString tmpStr;

				tmpStr=wxFileName::CreateTempFileName(tempDir+ wxT("/pointdata"));
				//wxwidgets has no suffix option... annoying.
				wxRemoveFile(tmpStr);

				s = stlStr(tmpStr);
				s+=".pos";
				if(IonVectorToPos(i->data,s))
				{
					//Uh-oh problem. Clean up and exit
					return WRITEPOS_FAIL;
				}

				ionOutputNames.push_back(s);
				break;
			}
			case STREAM_TYPE_PLOT:
			{
				const PlotStreamData *i;
				i = (const PlotStreamData * )(dataIn[ui]);

				if(i->xyData.empty())
					break;
				//Save the data to a file
				wxString tmpStr;

				tmpStr=wxFileName::CreateTempFileName(tempDir + wxT("/plot"));
				//wxwidgets has no suffix option... annoying.
				wxRemoveFile(tmpStr);
				s = stlStr(tmpStr);
			        s+= ".xy";
				if(!writeTextFile(s.c_str(),i->xyData))
				{
					//Uh-oh problem. Clean up and exit
					return WRITEPLOT_FAIL;
				}

				plotOutputNames.push_back(s);
				break;
			}
			default:
				break;
		}	
	}

	//Nothing to do.
	if(!(plotOutputNames.size() ||
		ionOutputNames.size()))
		return 0;


	//Construct the command, using substitution
	string command;
	unsigned int ionOutputPos,plotOutputPos;
	ionOutputPos=plotOutputPos=0;
	command = commandLineSplit[0];
	for(unsigned int ui=1;ui<commandLineSplit.size();ui++)
	{
		if(commandLineSplit[ui].find('%') == std::string::npos)
		{
			command+= string(" ") + commandLineSplit[ui];
		}
		else
		{
			size_t pos;
			pos=0;

			string thisCommandEntry;
			pos =commandLineSplit[ui].find("%",pos);
			while(pos != string::npos)
			{
				//OK, so we found a % symbol.
				//lets do some substitution
				
				//% must be followed by something otherwise this is an error
				if(pos == commandLineSplit[ui].size())
					return COMMANDLINE_FAIL;

				char code;
				code = commandLineSplit[ui][pos+1];

				switch(code)
				{
					case '%':
						//Escape '%%' to '%' symbol
						thisCommandEntry+="%";
						pos+=2;
						break;
					case 'i':
					{
						//Substitute '%i' with ion file name
						if(ionOutputPos == ionOutputNames.size())
						{
							//User error; not enough pos files to fill.
							return SUBSTITUTE_FAIL;
						}

						thisCommandEntry+=ionOutputNames[ionOutputPos];
						ionOutputPos++;
						pos++;
						break;
					}
					case 'I':
					{
						//Substitute '%I' with all ion file names, space separated
						if(ionOutputPos == ionOutputNames.size())
						{
							//User error. not enough pos files to fill
							return SUBSTITUTE_FAIL;
						}
						for(unsigned int ui=ionOutputPos; ui<ionOutputNames.size();ui++)
							thisCommandEntry+=ionOutputNames[ui] + " ";

						ionOutputPos=ionOutputNames.size();
						pos++;
						break;
					}
					case 'p':
					{
						//Substitute '%p' with all plot file names, space separated
						if(plotOutputPos == plotOutputNames.size())
						{
							//User error. not enough pos files to fill
							return SUBSTITUTE_FAIL;
						}
						for(unsigned int ui=plotOutputPos; ui<plotOutputNames.size();ui++)
							thisCommandEntry+=plotOutputNames[ui];

						plotOutputPos=plotOutputNames.size();
						pos++;
						break;
					}
					case 'P': 
					{
						//Substitute '%I' with all plot file names, space separated
						if(plotOutputPos == plotOutputNames.size())
						{
							//User error. not enough pos files to fill
							return SUBSTITUTE_FAIL;
						}
						for(unsigned int ui=plotOutputPos; ui<plotOutputNames.size();ui++)
							thisCommandEntry+=plotOutputNames[ui]+ " ";

						plotOutputPos=plotOutputNames.size();
						pos++;
						break;
					}
					default:
						//Invalid user input string. % must be escaped or recognised.
						return SUBSTITUTE_FAIL;
				}


				pos =commandLineSplit[ui].find("%",pos);
			}
			
			command+= string(" ") + thisCommandEntry;
		}

	}

	//If we have a specific working dir; use it. Otherwise just use curdir
	wxString origDir = wxGetCwd();
	if(workingDir.size())
	{
		//Set the working directory before launching
		if(!wxSetWorkingDirectory(wxStr(workingDir)))
			return SETWORKDIR_FAIL;
	}
	else
	{
		if(!wxSetWorkingDirectory(_(".")))
			return SETWORKDIR_FAIL;
	}

	bool result;
	//Execute the program
	result=wxShell(wxStr(command));

	if(cleanInput)
	{
		//If program returns error, this was a problem
		//delete the input files.
		for(unsigned int ui=0;ui<ionOutputNames.size();ui++)
		{
			//try to delete the file
			wxRemoveFile(wxStr(ionOutputNames[ui]));

			//call the update to be nice
			(*callback)(false);
		}
		for(unsigned int ui=0;ui<plotOutputNames.size();ui++)
		{
			//try to delete the file
			wxRemoveFile(wxStr(plotOutputNames[ui]));

			//call the update to be nice
			(*callback)(false);
		}
	}
	wxSetWorkingDirectory(origDir);	
	if(!result)
		return COMMAND_FAIL; 
	
	wxSetWorkingDirectory(origDir);	

	wxDir *dir = new wxDir;
	wxArrayString *a = new wxArrayString;
	if(workingDir.size())
		dir->GetAllFiles(wxStr(workingDir),a,_("*.pos"),wxDIR_FILES);
	else
		dir->GetAllFiles(wxGetCwd(),a,_("*.pos"),wxDIR_FILES);


	//read the output files, which is assumed to be any "pos" file
	//in the working dir
	for(unsigned int ui=0;ui<a->Count(); ui++)
	{
		wxULongLong size;
		size = wxFileName::GetSize((*a)[ui]);

		if( (size !=0) && size!=wxInvalidSize)
		{
			//Load up the pos file

			string sTmp;
			wxString wxTmpStr;
			wxTmpStr=(*a)[ui];
			sTmp = stlStr(wxTmpStr);
			unsigned int dummy;
			IonStreamData *d = new IonStreamData();
			d->parent=this;
			//TODO: some kind of secondary file for specification of
			//ion attribs?
			d->r = 1.0;
			d->g=0;
			d->b=0;
			d->a=1.0;
			d->ionSize = 2.0;

			unsigned int index2[] = {
					0, 1, 2, 3
					};
			if(GenericLoadFloatFile(4, 4, index2, d->data,sTmp.c_str(),dummy,0))
				return READPOS_FAIL;


			if(alwaysCache)
			{
				d->cached=1;
				filterOutputs.push_back(d);
			}
			else
				d->cached=0;
			getOut.push_back(d);
		}
	}

	a->Clear();
	if(workingDir.size())
		dir->GetAllFiles(wxStr(workingDir),a,_("*.xy"),wxDIR_FILES);
	else
		dir->GetAllFiles(wxGetCwd(),a,_("*.xy"),wxDIR_FILES);

	//read the output files, which is assumed to be any "pos" file
	//in the working dir
	for(unsigned int ui=0;ui<a->Count(); ui++)
	{
		wxULongLong size;
		size = wxFileName::GetSize((*a)[ui]);

		if( (size !=0) && size!=wxInvalidSize)
		{
			string sTmp;
			wxString wxTmpStr;
			wxTmpStr=(*a)[ui];
			sTmp = stlStr(wxTmpStr);

			vector<vector<float> > dataVec;

			vector<string> header;	

			//Possible delimiters to try when loading file
			//try each in turn
			const char *delimString ="\t, ";
			if(!loadTextData(sTmp.c_str(),dataVec,header,delimString))
				return READPLOT_FAIL;

			//Check that the input has the correct size
			for(unsigned int uj=0;uj<dataVec.size()-1;uj+=2)
			{
				//well the columns don't match
				if(dataVec[uj].size() != dataVec[uj+1].size())
					return PLOTCOLUMNS_FAIL;
			}

			//Check to see if the header might be able
			//to be matched to the data
			bool applyLabels=false;
			if(header.size() == dataVec.size())
				applyLabels=true;

			for(unsigned int uj=0;uj<dataVec.size()-1;uj+=2)
			{
				//TODO: some kind of secondary file for specification of
				//plot attribs?
				PlotStreamData *d = new PlotStreamData();
				d->parent=this;
				d->r = 0.0;
				d->g=1.0;
				d->b=0;
				d->a=1.0;


				//set the title to the filename (trim the .xy extension
				//and the working directory name)
				string tmpFilename;
				tmpFilename=sTmp.substr(workingDir.size(),sTmp.size()-
								workingDir.size()-3);

				d->dataLabel=getUserString() + string(":") + onlyFilename(tmpFilename);

				if(applyLabels)
				{

					//set the xy-labels to the column headers
					d->xLabel=header[uj];
					d->yLabel=header[uj+1];
				}
				else
				{
					d->xLabel="x";
					d->yLabel="y";
				}

				d->xyData.resize(dataVec[uj].size());

				ASSERT(dataVec[uj].size() == dataVec[uj+1].size());
				for(unsigned int uk=0;uk<dataVec[uj].size(); uk++)
				{
					d->xyData[uk]=make_pair(dataVec[uj][uk], 
								dataVec[uj+1][uk]);
				}
				
				if(alwaysCache)
				{
					d->cached=1;
					filterOutputs.push_back(d);
				}
				else
					d->cached=0;
				
				getOut.push_back(d);
			}
		}
	}

	if(alwaysCache)
		cacheOK=true;

	delete dir;
	delete a;

	return 0;
}


void ExternalProgramFilter::getProperties(FilterProperties &propertyList) const
{
	propertyList.data.clear();
	propertyList.keys.clear();
	propertyList.types.clear();

	vector<unsigned int> type,keys;
	vector<pair<string,string> > s;

	std::string tmpStr;
	
	s.push_back(make_pair(TRANS("Command"), commandLine));
	type.push_back(PROPERTY_TYPE_STRING);
	keys.push_back(KEY_COMMAND);		
	
	s.push_back(make_pair(TRANS("Work Dir"), workingDir));
	type.push_back(PROPERTY_TYPE_STRING);
	keys.push_back(KEY_WORKDIR);		
	
	propertyList.types.push_back(type);
	propertyList.data.push_back(s);
	propertyList.keys.push_back(keys);

	type.clear();s.clear();keys.clear();

	if(cleanInput)
		tmpStr="1";
	else
		tmpStr="0";

	s.push_back(make_pair(TRANS("Cleanup input"),tmpStr));
	type.push_back(PROPERTY_TYPE_BOOL);
	keys.push_back(KEY_CLEANUPINPUT);		
	if(alwaysCache)
		tmpStr="1";
	else
		tmpStr="0";
	
	s.push_back(make_pair(TRANS("Cache"),tmpStr));
	type.push_back(PROPERTY_TYPE_BOOL);
	keys.push_back(KEY_ALWAYSCACHE);		

	propertyList.types.push_back(type);
	propertyList.data.push_back(s);
	propertyList.keys.push_back(keys);
}

bool ExternalProgramFilter::setProperty( unsigned int set, unsigned int key,
					const std::string &value, bool &needUpdate)
{
	needUpdate=false;
	switch(key)
	{
		case KEY_COMMAND:
		{
			if(commandLine!=value)
			{
				commandLine=value;
				needUpdate=true;
				clearCache();
			}
			break;
		}
		case KEY_WORKDIR:
		{
			if(workingDir!=value)
			{
				//Check the directory exists
				if(!wxDirExists(wxStr(value)))
					return false;

				workingDir=value;
				needUpdate=true;
				clearCache();
			}
			break;
		}
		case KEY_ALWAYSCACHE:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			if(stripped=="1")
				alwaysCache=true;
			else
			{
				alwaysCache=false;

				//If we need to generate a cache, do so
				//otherwise, trash it
				clearCache();
			}

			needUpdate=true;
			break;
		}
		case KEY_CLEANUPINPUT:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			if(stripped=="1")
				cleanInput=true;
			else
				cleanInput=false;
			
			break;
		}
		default:
			ASSERT(false);

	}

	return true;
}


std::string  ExternalProgramFilter::getErrString(unsigned int code) const
{

	switch(code)
	{
		case COMMANDLINE_FAIL:
			return std::string(TRANS("Error processing command line"));
		case SETWORKDIR_FAIL:
			return std::string(TRANS("Unable to set working directory"));
		case WRITEPOS_FAIL:
			return std::string(TRANS("Error saving posfile result for external program"));
		case WRITEPLOT_FAIL:
			return std::string(TRANS("Error saving plot result for externalprogram"));
		case MAKEDIR_FAIL:
			return std::string(TRANS("Error creating temporary directory"));
		case PLOTCOLUMNS_FAIL:
			return std::string(TRANS("Detected unusable number of columns in plot"));
		case READPLOT_FAIL:
			return std::string(TRANS("Unable to parse plot result from external program"));
		case READPOS_FAIL:
			return std::string(TRANS("Unable to load ions from external program")); 
		case SUBSTITUTE_FAIL:
			return std::string(TRANS("Unable to perform commandline substitution"));
		case COMMAND_FAIL: 
			return std::string(TRANS("Error executing external program"));
		default:
			//Currently the only error is aborting
			return std::string("Bug: write me (externalProgramfilter).");
	}
}

bool ExternalProgramFilter::writeState(std::ofstream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{
			f << tabs(depth) << "<" << trueName() << ">" << endl;

			f << tabs(depth+1) << "<userstring value=\""<< escapeXML(userString) << "\"/>"  << endl;
			f << tabs(depth+1) << "<commandline name=\"" << escapeXML(commandLine )<< "\"/>" << endl;
			f << tabs(depth+1) << "<workingdir name=\"" << escapeXML(convertFileStringToCanonical(workingDir)) << "\"/>" << endl;
			f << tabs(depth+1) << "<alwayscache value=\"" << alwaysCache << "\"/>" << endl;
			f << tabs(depth+1) << "<cleaninput value=\"" << cleanInput << "\"/>" << endl;
			f << tabs(depth) << "</" << trueName() << ">" << endl;
			break;

		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool ExternalProgramFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
	//Retrieve user string
	if(XMLHelpFwdToElem(nodePtr,"userstring"))
		return false;

	xmlChar *xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	userString=(char *)xmlString;
	xmlFree(xmlString);

	//Retrieve command
	if(XMLHelpFwdToElem(nodePtr,"commandline"))
		return false;
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"name");
	if(!xmlString)
		return false;
	commandLine=(char *)xmlString;
	xmlFree(xmlString);
	
	//Retrieve working dir
	if(XMLHelpFwdToElem(nodePtr,"workingdir"))
		return false;
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"name");
	if(!xmlString)
		return false;
	workingDir=(char *)xmlString;
	xmlFree(xmlString);


	//get should cache
	string tmpStr;
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"alwayscache","value"))
		return false;

	if(tmpStr == "1") 
		alwaysCache=true;
	else if(tmpStr== "0")
		alwaysCache=false;
	else
		return false;

	//check readable 
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"cleaninput","value"))
		return false;

	if(tmpStr == "1") 
		cleanInput=true;
	else if(tmpStr== "0")
		cleanInput=false;
	else
		return false;

	return true;
}

unsigned int ExternalProgramFilter::getRefreshBlockMask() const
{
	//Absolutely nothing can go through this filter.
	return 0;
}

unsigned int ExternalProgramFilter::getRefreshEmitMask() const
{
	//Can only generate ion streams and plot streams
	return STREAM_TYPE_IONS | STREAM_TYPE_PLOT;
}

#ifdef DEBUG

bool echoTest()
{
	ExternalProgramFilter* f = new ExternalProgramFilter;
	f->setCaching(false);

	int errCode;
#if !defined(__WIN32__) && !defined(__WIN64__)
	errCode=system("echo testing... > /dev/null");
#else
	errCode=system("echo testing... > NUL");
#endif
	if(errCode)
	{
		WARN(false,"Unable to perform echo test on this system -- echo missing?");
		return true;
	}
	
	bool needUp;
	string s;
				
	wxString tmpFilename;
	tmpFilename=wxFileName::CreateTempFileName(wxT(""));
	s = string(" echo test > ") + stlStr(tmpFilename);
	f->setProperty(0,KEY_COMMAND,s,needUp);

	//Simulate some data to send to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	ProgressData p;
	f->refresh(streamIn,streamOut,p,dummyCallback);


	s=stlStr(tmpFilename);
	ifstream file(s.c_str());
	
	TEST(file,"echo retrieval");


	wxRemoveFile(tmpFilename);

	delete f;

	return true;
}

IonStreamData* createTestPosData(unsigned int numPts)
{
	IonStreamData* d= new IonStreamData;

	d->data.resize(numPts);
	for(unsigned int ui=0;ui<numPts;ui++)
	{
		d->data[ui].setPos(ui,ui,ui);
		d->data[ui].setMassToCharge(ui);
	}

	return d;
}

bool posTest()
{
	const unsigned int NUM_PTS=100;
	IonStreamData *someData;
	someData=createTestPosData(NUM_PTS);

	ExternalProgramFilter* f = new ExternalProgramFilter;
	f->setCaching(false);

	bool needUp;
	string s;

	wxString tmpFilename,tmpDir;
	tmpDir=wxFileName::GetTempDir();


#if defined(__WIN32__) || defined(__WIN64__)
	tmpDir=tmpDir + wxT("\\3Depict\\");

#else
	tmpDir=tmpDir + wxT("/3Depict/");
#endif
	wxMkdir(tmpDir);

	tmpFilename=wxFileName::CreateTempFileName(tmpDir+ wxT("unittest-"));
	wxRemoveFile(tmpFilename);
	tmpFilename+=wxT(".pos");
	s ="mv \%i " + stlStr(tmpFilename);

	ASSERT(tmpFilename.size());
	
	f->setProperty(0,KEY_COMMAND,s,needUp);
	f->setProperty(0,KEY_WORKDIR,stlStr(tmpDir),needUp);
	//Simulate some data to send to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	streamIn.push_back(someData);
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p,dummyCallback),"refresh error code");

	//Should have exactly one stream, which is an ion stream
	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS,"stream type");

	TEST(streamOut[0]->getNumBasicObjects() ==NUM_PTS,"Number of ions");
	const IonStreamData* out =(IonStreamData*) streamOut[0]; 

	for(unsigned int ui=0;ui<out->data.size();ui++)
	{
		TEST(out->data[ui].getPos() == someData->data[ui].getPos(),"position");
		TEST(out->data[ui].getMassToCharge() == 
			someData->data[ui].getMassToCharge(),"position");
	}



	wxRemoveFile(tmpFilename);
	wxRmdir(tmpDir+wxT("inputData"));
	wxRmdir(tmpDir);

	delete streamOut[0];

	delete someData;
	delete f;

	return true;
}


bool ExternalProgramFilter::runUnitTests() 
{
	if(!echoTest())
		return false;
	
	if(!posTest())
		return false;

	return true;
}


#endif
