/*
 *	state.cpp - user session state handler
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

#include "state.h"

#include "common/translation.h"
#include "common/xmlHelper.h"
#include "common/stringFuncs.h"

const unsigned int MAX_UNDO_SIZE=10;

#include <unistd.h>


AnalysisState::AnalysisState()
{
	modifyLevel=STATE_MODIFIED_NONE;
	useRelativePathsForSave=false;
	activeCamera=0;

	rBack=gBack=bBack=0;
}


void AnalysisState::operator=(const AnalysisState &oth)
{
	clear();

	activeTree=oth.activeTree;
	
	stashedTrees=oth.stashedTrees;

	effects.resize(oth.effects.size());
	for(size_t ui=0;ui<effects.size();ui++)
		effects[ui]= oth.effects[ui]->clone();

	savedCameras.resize(oth.savedCameras.size());
	for(size_t ui=0;ui<savedCameras.size();ui++)
		savedCameras[ui]= oth.savedCameras[ui]->clone();


	undoTrees=oth.undoTrees; 
	redoTrees=oth.redoTrees;

	fileName=oth.fileName;
	workingDir=oth.workingDir;
	useRelativePathsForSave=oth.useRelativePathsForSave;

	rBack=oth.rBack;
	gBack=oth.gBack;
	bBack=oth.bBack;

	worldAxisMode=oth.worldAxisMode;
	activeCamera=oth.activeCamera;

	modifyLevel=oth.modifyLevel;


	redoFilterStack=oth.redoFilterStack;
	undoFilterStack=oth.undoFilterStack;

}

AnalysisState::~AnalysisState()
{
	clear();
}


void AnalysisState::clear()
{
	activeTree.clear();
	
	stashedTrees.clear();

	clearCams();

	clearEffects();

	undoTrees.clear(); 
	redoTrees.clear();

	fileName.clear();
	workingDir.clear();
}

void AnalysisState::clearCams()
{
	for(size_t ui=0;ui<savedCameras.size();ui++)
		delete savedCameras[ui];
	savedCameras.clear();
}

void AnalysisState::clearEffects()
{
	for(size_t ui=0;ui<effects.size();ui++)
		delete effects[ui];

	effects.clear();

}
bool AnalysisState::save(const char *cpFilename, std::map<string,string> &fileMapping,
		bool writePackage) const
{
	//Open file for output
	std::ofstream f(cpFilename);

	if(!f)
		return false;
	
	//Write header, also use local language if available
	const char *headerMessage = NTRANS("This file is a \"state\" file for the 3Depict program, and stores information about a particular analysis session. This file should be a valid \"XML\" file");

	f << "<!--" <<  headerMessage;
	if(TRANS(headerMessage) != headerMessage) 
		f << endl << TRANS(headerMessage); 
	
	f << "-->" <<endl;



	//Write state open tag 
	f<< "<threeDepictstate>" << endl;
	f<<tabs(1)<< "<writer version=\"" << PROGRAM_VERSION << "\"/>" << endl;
	//write general settings
	//---------------
	f << tabs(1) << "<backcolour r=\"" << rBack << "\" g=\"" << 
		 gBack  << "\" b=\"" << bBack << "\"/>" <<  endl;
	
	f << tabs(1) << "<showaxis value=\"" << worldAxisMode << "\"/>"  << endl;


	if(useRelativePathsForSave)
	{
		//Save path information
		//Note: When writing packages, 
		//- we don't want to leak path names to remote systems 
		//and
		//- we cannot assume that directory structures are preserved between systems
		//so don't keep working directory in this case.
		if(writePackage || workingDir.empty() )
		{
			//Are we saving the sate as a package, if so
			//make sure we let other 3depict loaders know
			//that we want to use relative paths
			f << tabs(1) << "<userelativepaths/>"<< endl;
		}
		else
		{
			//Not saving a package, however we could be, 
			//for example, be autosaving a load-from-package. 
			//We want to keep relative paths, but
			//want to be able to find files if something goes askew
			f << tabs(1) << "<userelativepaths origworkdir=\"" << workingDir << "\"/>"<< endl;
		}
	}

	//---------------


	//Write filter tree
	//---------------
	if(!activeTree.saveXML(f,fileMapping,writePackage,useRelativePathsForSave))
		return  false;
	//---------------

	//Save all cameras.
	f <<tabs(1) <<  "<cameras>" << endl;

	//First camera is the "working" camera, which is unnamed
	f << tabs(2) << "<active value=\"" << activeCamera << "\"/>" << endl;
	
	for(unsigned int ui=0;ui<savedCameras.size();ui++)
	{
		//ask each camera to write its own state, tab indent 2
		savedCameras[ui]->writeState(f,STATE_FORMAT_XML,2);
	}
	f <<tabs(1) <<  "</cameras>" << endl;
	
	if(stashedTrees.size())
	{
		f << tabs(1) << "<stashedfilters>" << endl;

		for(unsigned int ui=0;ui<stashedTrees.size();ui++)
		{
			f << tabs(2) << "<stash name=\"" << stashedTrees[ui].first
				<< "\">" << endl;
			stashedTrees[ui].second.saveXML(f,fileMapping,
					writePackage,useRelativePathsForSave,3);
			f << tabs(2) << "</stash>" << endl;
		}




		f << tabs(1) << "</stashedfilters>" << endl;
	}

	//Save any effects
	if(effects.size())
	{
		f <<tabs(1) <<  "<effects>" << endl;
		for(unsigned int ui=0;ui<effects.size();ui++)
			effects[ui]->writeState(f,STATE_FORMAT_XML,1);
		f <<tabs(1) <<  "</effects>" << endl;

	}



	//Close XMl tag.	
	f<< "</threeDepictstate>" << endl;

	//Debug check to ensure we have written a valid xml file
	ASSERT(isValidXML(cpFilename));

	modifyLevel=STATE_MODIFIED_NONE;

	return true;
}

bool AnalysisState::load(const char *cpFilename, std::ostream &errStream, bool merge) 
{
	clear();

	//Load the state from an XML file
	
	//here we use libxml2's loading routines
	//http://xmlsoft.org/
	//Tutorial: http://xmlsoft.org/tutorial/xmltutorial.pdf
	xmlDocPtr doc;
	xmlParserCtxtPtr context;

	context =xmlNewParserCtxt();


	if(!context)
	{
		errStream << TRANS("Failed to allocate parser") << std::endl;
		return false;
	}

	//Open the XML file
	doc = xmlCtxtReadFile(context, cpFilename, NULL,XML_PARSE_NOENT|XML_PARSE_NONET);

	if(!doc)
		return false;
	
	//release the context
	xmlFreeParserCtxt(context);
	

	//By default, lets not use relative paths
	if(!merge)
		useRelativePathsForSave=false;

	//Lets do some parsing goodness
	//ahh parsing - verbose and boring
	FilterTree newFilterTree;
	vector<Camera *> newCameraVec;
	vector<Effect *> newEffectVec;
	vector<pair<string,FilterTree > > newStashes;

	std::string stateDir=onlyDir(cpFilename);
	try
	{
		std::stack<xmlNodePtr>  nodeStack;
		//retrieve root node	
		xmlNodePtr nodePtr = xmlDocGetRootElement(doc);

		//Umm where is our root node guys?
		if(!nodePtr)
		{
			errStream << TRANS("Unable to retrieve root node in input state file... Is this really a non-empty XML file?") <<  endl;
			throw 1;
		}
		
		//This *should* be an threeDepict state file
		if(xmlStrcmp(nodePtr->name, (const xmlChar *)"threeDepictstate"))
		{
			errStream << TRANS("Base state node missing. Is this really a state XML file??") << endl;
			throw 1;
		}
		//push root tag	
		nodeStack.push(nodePtr);
		
		//Now in threeDepictstate tag
		nodePtr = nodePtr->xmlChildrenNode;
		xmlChar *xmlString;
		//check for version tag & number
		if(!XMLHelpFwdToElem(nodePtr,"writer"))
		{
			xmlString=xmlGetProp(nodePtr, (const xmlChar *)"version"); 

			if(xmlString)
			{
				string tmpVer;
				
				tmpVer =(char *)xmlString;
				//Check to see if only contains 0-9 period and "-" characters (valid version number)
				if(tmpVer.find_first_not_of("0123456789.-")== std::string::npos)
				{
					//Check between the writer reported version, and the current program version
					vector<string> vecStrs;
					vecStrs.push_back(tmpVer);
					vecStrs.push_back(PROGRAM_VERSION);

					if(getMaxVerStr(vecStrs)!=PROGRAM_VERSION)
					{
						errStream << TRANS("State was created by a newer version of this program.. ")
							<< TRANS("file reading will continue, but may fail.") << endl ;
					}
				}
				else
				{
					errStream<< TRANS("Warning, unparseable version number in state file. File reading will continue, but may fail") << endl;
				}
				xmlFree(xmlString);
			}
		}
		else
		{
			errStream<< TRANS("Unable to find the \"writer\" node") << endl;
			throw 1;
		}
	

		//Get the background colour
		//====
		float rTmp,gTmp,bTmp;
		if(XMLHelpFwdToElem(nodePtr,"backcolour"))
		{
			errStream<< TRANS("Unable to find the \"backcolour\" node.") << endl;
			throw 1;
		}

		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"r");
		if(!xmlString)
		{
			errStream<< TRANS("\"backcolour\" node missing \"r\" value.") << endl;
			throw 1;
		}
		if(stream_cast(rTmp,(char *)xmlString))
		{
			errStream<< TRANS("Unable to interpret \"backColour\" node's \"r\" value.") << endl;
			throw 1;
		}

		xmlFree(xmlString);
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"g");
		if(!xmlString)
		{
			errStream<< TRANS("\"backcolour\" node missing \"g\" value.") << endl;
			throw 1;
		}

		if(stream_cast(gTmp,(char *)xmlString))
		{
			errStream<< TRANS("Unable to interpret \"backColour\" node's \"g\" value.") << endl;
			throw 1;
		}

		xmlFree(xmlString);
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"b");
		if(!xmlString)
		{
			errStream<< TRANS("\"backcolour\" node missing \"b\" value.") << endl;
			throw 1;
		}

		if(stream_cast(bTmp,(char *)xmlString))
		{
			errStream<< TRANS("Unable to interpret \"backColour\" node's \"b\" value.") << endl;
			throw 1;
		}

		if(rTmp > 1.0 || gTmp>1.0 || bTmp > 1.0 || 
			rTmp < 0.0 || gTmp < 0.0 || bTmp < 0.0)
		{
			errStream<< TRANS("\"backcolour\"s rgb values must be in range [0,1]") << endl;
			throw 1;
		}
		rBack=rTmp;
		gBack=gTmp;
		bBack=bTmp;
		
		xmlFree(xmlString);

		nodeStack.push(nodePtr);


		if(!XMLHelpFwdToElem(nodePtr,"userelativepaths"))
		{
			useRelativePathsForSave=true;

			//Try to load the original working directory, if possible
			if(!XMLGetAttrib(nodePtr,workingDir,"origworkdir"))
				workingDir.clear();
		}
		
		nodePtr=nodeStack.top();

		//====
		
		//Get the axis visibility
		if(!XMLGetNextElemAttrib(nodePtr,worldAxisMode,"showaxis","value"))
		{
			errStream << TRANS("Unable to find or interpret \"showaxis\" node") << endl;
			throw 1;
		}

		//find filtertree data
		if(XMLHelpFwdToElem(nodePtr,"filtertree"))
		{
			errStream << TRANS("Unable to locate \"filtertree\" node.") << endl;
			throw 1;
		}

		//Load the filter tree
		if(newFilterTree.loadXML(nodePtr,errStream,stateDir))
			throw 1;

		//Read camera states, if present
		nodeStack.push(nodePtr);
		if(!XMLHelpFwdToElem(nodePtr,"cameras"))
		{
			//Move to camera active tag 
			nodePtr=nodePtr->xmlChildrenNode;
			if(XMLHelpFwdToElem(nodePtr,"active"))
			{
				errStream << TRANS("Cameras section missing \"active\" node.") << endl;
				throw 1;
			}

			//read ID of active cam
			xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
			if(!xmlString)
			{
				errStream<< TRANS("Unable to find property \"value\"  for \"cameras->active\" node.") << endl;
				throw 1;
			}

			if(stream_cast(activeCamera,xmlString))
			{
				errStream<< TRANS("Unable to interpret property \"value\"  for \"cameras->active\" node.") << endl;
				throw 1;
			}

		
			//Spin through the list of each camera	
			while(!XMLHelpNextType(nodePtr,XML_ELEMENT_NODE))
			{
				std::string tmpStr;
				tmpStr =(const char *)nodePtr->name;
				Camera *thisCam;
				thisCam=0;
				
				//work out the camera type
				if(tmpStr == "persplookat")
				{
					thisCam = new CameraLookAt;
					if(!thisCam->readState(nodePtr->xmlChildrenNode))
					{
						std::string s =TRANS("Failed to interpret camera state for camera : "); 

						errStream<< s <<  newCameraVec.size() << endl;
						throw 1;
					}
				}
				else
				{
					errStream << TRANS("Unable to interpret the camera type for camera : ") << newCameraVec.size() <<  endl;
					throw 1;
				}

				ASSERT(thisCam);
				newCameraVec.push_back(thisCam);	
			}

			//Enforce active cam value validity
			if(newCameraVec.size() < activeCamera)
				activeCamera=0;

		}
		
		nodePtr=nodeStack.top();
		nodeStack.pop();
		
		nodeStack.push(nodePtr);
		//Read stashes if present
		if(!XMLHelpFwdToElem(nodePtr,"stashedfilters"))
		{
			nodeStack.push(nodePtr);

			//Move to stashes 
			nodePtr=nodePtr->xmlChildrenNode;

			while(!XMLHelpFwdToElem(nodePtr,"stash"))
			{
				string stashName;
				FilterTree newStashTree;
				newStashTree.clear();

				//read name of stash
				xmlString=xmlGetProp(nodePtr,(const xmlChar *)"name");
				if(!xmlString)
				{
					errStream << TRANS("Unable to locate stash name for stash ") << newStashTree.size()+1 << endl;
					throw 1;
				}
				stashName=(char *)xmlString;

				if(!stashName.size())
				{
					errStream << TRANS("Empty stash name for stash ") << newStashTree.size()+1 << endl;
					throw 1;
				}

				xmlNodePtr tmpNode;
				tmpNode=nodePtr->xmlChildrenNode;
				
				if(XMLHelpFwdToElem(tmpNode,"filtertree"))
				{
					errStream << TRANS("No filter tree for stash:") << stashName << endl;
					throw 1;
				}

				if(newStashTree.loadXML(tmpNode,errStream,stateDir))
				{
					errStream << TRANS("For stash ") << newStashTree.size()+1 << endl;
					throw 1;
				}
				
				//if there were any valid elements loaded (could be empty, for exmapl)
				if(newStashTree.size())
					newStashes.push_back(make_pair(stashName,newStashTree));
			}

			nodePtr=nodeStack.top();
			nodeStack.pop();
		}
		nodePtr=nodeStack.top();
		nodeStack.pop();
	
		//Read effects, if present
		nodeStack.push(nodePtr);

		//Read effects if present
		if(!XMLHelpFwdToElem(nodePtr,"effects"))
		{
			std::string tmpStr;
			nodePtr=nodePtr->xmlChildrenNode;
			while(!XMLHelpNextType(nodePtr,XML_ELEMENT_NODE))
			{
				tmpStr =(const char *)nodePtr->name;

				Effect *e;
				e = makeEffect(tmpStr);
				if(!e)
				{
					errStream << TRANS("Unrecognised effect :") << tmpStr << endl;
					throw 1;
				}

				//Check the effects are unique
				for(unsigned int ui=0;ui<newEffectVec.size();ui++)
				{
					if(newEffectVec[ui]->getType()== e->getType())
					{
						delete e;
						errStream << TRANS("Duplicate effect found") << tmpStr << TRANS(" cannot use.") << endl;
						throw 1;
					}

				}

				nodeStack.push(nodePtr);
				//Parse the effect
				if(!e->readState(nodePtr))
				{
					errStream << TRANS("Error reading effect : ") << e->getName() << std::endl;
				
					throw 1;
				}
				nodePtr=nodeStack.top();
				nodeStack.pop();


				newEffectVec.push_back(e);				
			}
		}
		nodePtr=nodeStack.top();
		nodeStack.pop();



		nodeStack.push(nodePtr);
	}
	catch (int)
	{
		//Code threw an error, just say "bad parse" and be done with it
		xmlFreeDoc(doc);
		return false;
	}
	xmlFreeDoc(doc);	

	//Check that stashes are uniquely named
	// do brute force search, as there are unlikely to be many stashes	
	for(unsigned int ui=0;ui<newStashes.size();ui++)
	{
		for(unsigned int uj=0;uj<newStashes.size();uj++)
		{
			if(ui == uj)
				continue;

			//If these match, states not uniquely named,
			//and thus statefile is invalid.
			if(newStashes[ui].first == newStashes[uj].first)
				return false;

		}
	}

	if(!merge)
	{
		//Erase any viscontrol data, seeing as we got this far	
		clear(); 
		//Now replace it with the new data
		activeTree.swap(newFilterTree);
		std::swap(stashedTrees,newStashes);
	}
	else
	{
		//If we are merging, then there is a chance
		//of a name-clash. We avoid this by trying to append -merge continuously
		for(unsigned int ui=0;ui<newStashes.size();ui++)
		{
			//protect against overload (very unlikely)
			unsigned int maxCount;
			maxCount=100;
			while(hasFirstInPairVec(stashedTrees,newStashes[ui]) && --maxCount)
				newStashes[ui].first+=TRANS("-merge");

			if(maxCount)
				stashedTrees.push_back(newStashes[ui]);
			else
				errStream << TRANS(" Unable to merge stashes correctly. This is improbable, so please report this.") << endl;
		}

		activeTree.addFilterTree(newFilterTree,0);
		undoTrees.clear();
		redoTrees.clear();
	}

	activeTree.initFilterTree();
	
	//Wipe the existing cameras, and then put the new cameras in place
	if(!merge)
		savedCameras.clear();
	
	//Set a default camera as needed. We don't need to track its unique ID, as this is
	//"invisible" to the UI
	if(!savedCameras.size())
	{
		Camera *c=new CameraLookAt();
		savedCameras.push_back(c);
		activeCamera=0;
	}

	//spin through
	for(unsigned int ui=0;ui<newCameraVec.size();ui++)
	{
		if(merge)
		{
			//Don't merge the default camera (which has no name)
			if(newCameraVec[ui]->getUserString().empty())
				continue;

			//Keep trying new names appending "-merge" each time to obtain a new, and hopefully unique name
			// Abort after many times
			unsigned int maxCount;
			maxCount=100;
			while(camNameExists(newCameraVec[ui]->getUserString()) && --maxCount)
			{
				newCameraVec[ui]->setUserString(newCameraVec[ui]->getUserString()+"-merge");
			}

			//If we have any attempts left, then it worked
			if(maxCount)
				savedCameras.push_back(newCameraVec[ui]);
		}
		else
		{
			//If there is no userstring, then its a  "default"
			// camera (one that does not show up to the users,
			// and cannot be erased from the scene)
			// set it directly. Otherwise, its a user camera.
			if(newCameraVec[ui]->getUserString().size())
			{
				savedCameras.push_back(newCameraVec[ui]);
				activeCamera=ui;
			}
			else
			{
				ASSERT(savedCameras.size());
				delete savedCameras[0];
				savedCameras[0]=newCameraVec[ui];
			}
		}

	}

	fileName=cpFilename;


	if(workingDir.empty())
	{
		char *wd;
#if defined(__APPLE__)
		//Apple defines a special getcwd that just works
		wd = getcwd(NULL, 0);
#elif defined(WIN32) || defined(WIN64)
		//getcwd under POSIX is not clearly defined, it requires
		// an input number of bytes that are enough to hold the path,
		// however it does not define how one goes about obtaining the
		// number of bytes needed. 
		char *wdtemp = (char*)malloc(PATH_MAX*20);
		wd=getcwd(wdtemp,PATH_MAX*20);
		if(!wd)
		{
			free(wdtemp);
			return false;	
		}

#else
		//GNU extension, which just does it (tm).
		wd = get_current_dir_name();
#endif
		workingDir=wd;
		free(wd);
	}

	//If we are merging then the default state has been altered
	// if we are not merging, then it is overwritten
	if(merge)
		setModifyLevel(STATE_MODIFIED_DATA);
	else
		setModifyLevel(STATE_MODIFIED_NONE);

	//Perform sanitisation on results
	return true;
}

bool AnalysisState::camNameExists(const std::string &s) const
{
	for(size_t ui=0; ui<savedCameras.size(); ui++) 
	{
		if (savedCameras[ui]->getUserString() == s ) 
			return true;
	}
	return false;
}

void AnalysisState::copyFilterTree(FilterTree &f) const
{
	f = activeTree;
}

int AnalysisState::getWorldAxisMode() const 
{
	return worldAxisMode;
}

void AnalysisState::copyCams(vector<Camera *> &cams) const 
{
	ASSERT(!cams.size());

	cams.resize(savedCameras.size());
	for(size_t ui=0;ui<savedCameras.size();ui++)
		cams[ui] = savedCameras[ui]->clone();
}

void AnalysisState::copyCamsByRef(vector<const Camera *> &camRef) const
{
	camRef.resize(savedCameras.size());
	for(size_t ui=0;ui<camRef.size();ui++)
		camRef[ui]=savedCameras[ui];
}

const Camera *AnalysisState::getCam(size_t offset) const
{
	return savedCameras[offset];
}

void AnalysisState::removeCam(size_t offset)
{
	ASSERT(offset < savedCameras.size());
	savedCameras.erase(savedCameras.begin()+offset);
	if(activeCamera >=savedCameras.size())
		activeCamera=0;
}

void AnalysisState::addCamByClone(const Camera *c)
{
	setModifyLevel(STATE_MODIFIED_ANCILLARY);
	savedCameras.push_back(c->clone());
}

bool AnalysisState::setCamProperty(size_t offset, unsigned int key, const std::string &str)
{
	if(offset == activeCamera)
		setModifyLevel(STATE_MODIFIED_VIEW);
	else
		setModifyLevel(STATE_MODIFIED_ANCILLARY);
	return savedCameras[offset]->setProperty(key,str);
}

bool AnalysisState::getUseRelPaths() const 
{
	return useRelativePathsForSave;
}
		
void AnalysisState::getBackgroundColour(float &r, float &g, float &b) const
{
	r=rBack;
	g=gBack;
	b=bBack;
}

void AnalysisState::copyEffects(vector<Effect *> &e) const
{
	e.clear();
	for(size_t ui=0;ui<effects.size();ui++)
		e[ui]=effects[ui]->clone();
}


void AnalysisState::setBackgroundColour(float r, float g, float b)
{
	if(rBack != r || gBack!=g || bBack!=b)
		setModifyLevel(STATE_MODIFIED_VIEW);
	rBack=r;
	gBack=g;
	bBack=b;

}

void AnalysisState::setWorldAxisMode(unsigned int mode)
{
	if(mode)
		setModifyLevel(STATE_MODIFIED_VIEW);
	worldAxisMode=mode;
}

void AnalysisState::setCamerasByCopy(vector<Camera *> &c, unsigned int active)
{
	setModifyLevel(STATE_MODIFIED_DATA);
	clearCams();

	savedCameras.swap(c);
	activeCamera=active;
}

void AnalysisState::setCameraByClone(const Camera *c, unsigned int offset)
{
	ASSERT(offset < savedCameras.size()); 
	delete savedCameras[offset];
	savedCameras[offset]=c->clone(); 

	if(offset == activeCamera)
		setModifyLevel(STATE_MODIFIED_VIEW);
	else
		setModifyLevel(STATE_MODIFIED_ANCILLARY);
}

void AnalysisState::setEffectsByCopy(const vector<const Effect *> &e)
{
	setModifyLevel(STATE_MODIFIED_VIEW);
	clearEffects();

	effects.resize(e.size());
	for(size_t ui=0;ui<e.size();ui++)
		effects[ui] = e[ui]->clone();

}


void AnalysisState::setUseRelPaths(bool useRel)
{
	useRelativePathsForSave=useRel;
}
void AnalysisState::setWorkingDir(const std::string &work)
{
	if(work!=workingDir)
		setModifyLevel(STATE_MODIFIED_DATA);

	workingDir=work;
}

void AnalysisState::setFilterTreeByClone(const FilterTree &f)
{
	setModifyLevel(STATE_MODIFIED_DATA);
	activeTree=f;
}

void AnalysisState::setStashedTreesByClone(const vector<std::pair<string,FilterTree> > &s)
{
	setModifyLevel(STATE_MODIFIED_ANCILLARY);
	stashedTrees=s;
}

void AnalysisState::addStashedTree(const std::pair<string,FilterTree> &s)
{
	setModifyLevel(STATE_MODIFIED_ANCILLARY);
	stashedTrees.push_back(s);
}

void AnalysisState::copyStashedTrees(std::vector<std::pair<string,FilterTree > > &s) const
{
	s=stashedTrees;
}

void AnalysisState::copyStashedTree(size_t offset,std::pair<string,FilterTree> &s) const
{
	s.first=stashedTrees[offset].first;
	s.second=stashedTrees[offset].second;
}


void AnalysisState::pushUndoStack()
{
	if(undoFilterStack.size() > MAX_UNDO_SIZE)
		undoFilterStack.pop_front();

	undoFilterStack.push_back(activeTree);
	redoFilterStack.clear();
}

void AnalysisState::popUndoStack(bool restorePopped)
{
	ASSERT(undoFilterStack.size());

	//Save the current filters to the redo stack.
	// note that the copy constructor will generate a clone for us.
	redoFilterStack.push_back(activeTree);

	if(redoFilterStack.size() > MAX_UNDO_SIZE)
		redoFilterStack.pop_front();

	if(restorePopped)
	{
		//Swap the current filter cache out with the undo stack result
		std::swap(activeTree,undoFilterStack.back());
		
	}

	//Pop the undo stack
	undoFilterStack.pop_back();

	setModifyLevel(STATE_MODIFIED_DATA);
}

void AnalysisState::popRedoStack()
{
	ASSERT(undoFilterStack.size() <=MAX_UNDO_SIZE);
	undoFilterStack.push_back(activeTree);

	activeTree.clear();
	//Swap the current filter cache out with the redo stack result
	activeTree.swap(redoFilterStack.back());
	
	//Pop the redo stack
	redoFilterStack.pop_back();

	setModifyLevel(STATE_MODIFIED_DATA);
}


void AnalysisState::eraseStash(size_t offset)
{
	ASSERT(offset < stashedTrees.size());
	setModifyLevel(STATE_MODIFIED_ANCILLARY);
	stashedTrees.erase(stashedTrees.begin() + offset);
}



#ifdef DEBUG

#include "./filters/ionDownsample.h"
bool testStateReload();

bool runStateTests()
{
	return testStateReload();
}

bool testStateReload()
{

	AnalysisState someState;
	someState.setWorldAxisMode(0);
	someState.setBackgroundColour(0,0,0);

	FilterTree tree;
	IonDownsampleFilter *f = new IonDownsampleFilter;
	tree.addFilter(f,NULL);

	someState.setFilterTreeByClone(tree);
	someState.addStashedTree(make_pair("someStash",tree));

	std::string saveString;
	genRandomFilename(saveString);

	map<string,string> dummyMapping;
	someState.save(saveString.c_str(),dummyMapping,false);
	someState.clear();

	std::ofstream strm;
	someState.load(saveString.c_str(),strm,false);


	TEST(someState.getStashCount() == 1,"Stash save+load");
	std::pair<string,FilterTree> stashOut;
	someState.copyStashedTree(0,stashOut);
	TEST(stashOut.first == "someStash","Stash name conservation");

	TEST(stashOut.second.size() == tree.size(),"reloaded stash count");
	
	rmFile(saveString);

	return true;
}

#endif
