/*
 *	configFile.cpp  - User configuration loading/saving
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

#include "configFile.h"
#include "xmlHelper.h"

#include "filters/allFilter.h"

const char *CONFIG_FILENAME="config.xml";
const unsigned int MAX_RECENT=15;



#include <wx/stdpaths.h>

#include <iostream>
#include <cstdlib>
#include <fstream>
#include <stack>
#include <deque>

using std::endl;
using std::string;

unsigned int ConfigFile::getMaxHistory() const
{
	return MAX_RECENT;
}

void ConfigFile::addRecentFile(const  std::string &str)
{
	recentFiles.push_back(str);
	if(recentFiles.size() > MAX_RECENT)
		recentFiles.pop_front();
}

void ConfigFile::getRecentFiles(std::vector<std::string> &files) const
{
	files.resize(recentFiles.size());
	std::copy(recentFiles.begin(),recentFiles.end(),files.begin());
}


void ConfigFile::removeRecentFile(const std::string &str)
{
	std::deque<string>::iterator it;
	it=std::find(recentFiles.begin(),recentFiles.end(),str);

	if(it!=recentFiles.end())
		recentFiles.erase(it);
}


void ConfigFile::getFilterDefaults(vector<Filter *>  &defs)
{
	defs.resize(filterDefaults.size());
	std::copy(filterDefaults.begin(),filterDefaults.end(),defs.begin());
}
void ConfigFile::setFilterDefaults(const vector<Filter *>  &defs)
{
	for(unsigned int ui=0;ui<filterDefaults.size();ui++)
		delete filterDefaults[ui];
	filterDefaults.resize(defs.size());

	for(unsigned int ui=0;ui<filterDefaults.size();ui++)
	{
		//Disallow storing of potentially hazardous filters
		filterDefaults[ui]=defs[ui];
		ASSERT(!(filterDefaults[ui]->canBeHazardous()));
	}
}

Filter *ConfigFile::getDefaultFilter(unsigned int type) const
{
	for(unsigned int ui=0;ui<filterDefaults.size();ui++)
	{
		if(type == filterDefaults[ui]->getType())
		{
			ASSERT(!filterDefaults[ui]->canBeHazardous());
			return filterDefaults[ui]->cloneUncached();
		}
	}

	return makeFilter(type);	
}

unsigned int ConfigFile::read()
{

	string filename;
	wxStandardPaths *paths = new wxStandardPaths;

	wxString filePath = paths->GetDocumentsDir()+wxCStr("/.")+wxCStr(PROGRAM_NAME);
	filePath+=wxCStr("/") + wxCStr(CONFIG_FILENAME);
	filename = stlStr(filePath);

	//Load the state from an XML file
	//here we use libxml2's loading routines
	//http://xmlsoft.org/
	//Tutorial: http://xmlsoft.org/tutorial/xmltutorial.pdf
	xmlDocPtr doc;
	xmlParserCtxtPtr context;

	context =xmlNewParserCtxt();

	if(!context)
	{
		return CONFIG_ERR_NOPARSER;
	}

	//Open the XML file again, but without DTD validation
	doc = xmlCtxtReadFile(context, filename.c_str(), NULL, 0);

	if(!doc)
		return CONFIG_ERR_NOFILE;

	//release the context
	xmlFreeParserCtxt(context);

	//retrieve root node	
	xmlNodePtr nodePtr = xmlDocGetRootElement(doc);


	try
	{
		std::stack<xmlNodePtr>  nodeStack;

		//Umm where is our root node guys?
		if(!nodePtr)
			throw 1;
		
		//push root tag	
		nodeStack.push(nodePtr);
		
		//This *should* be an threeDepict state file
		if(xmlStrcmp(nodePtr->name, (const xmlChar *)"threeDepictconfig"))
		{
			errMessage="Config file present, but is not valid (root node test)";
			throw 1;
		}

		nodeStack.push(nodePtr);
		nodePtr=nodePtr->xmlChildrenNode;	
		//Clean up current configuration
		recentFiles.clear();
		
		if(!XMLHelpFwdToElem(nodePtr,"recent"))
		{
			nodeStack.push(nodePtr);
			nodePtr=nodePtr->xmlChildrenNode;


			std::string thisName;
			while(!XMLHelpFwdToElem(nodePtr,"file") && recentFiles.size() < MAX_RECENT)
			{
				xmlChar *xmlString;
				
				//read name of file
				xmlString=xmlGetProp(nodePtr,(const xmlChar *)"name");
				if(!xmlString)
				{
					errMessage="Unable to interpret recent file entry";
					throw 1;
				}
				thisName=(char *)xmlString;

				recentFiles.push_back(thisName);
			}
		}

		//restore old node	
		nodePtr=nodeStack.top();
		nodeStack.pop();
		
		//Advance and push
		if(nodePtr->next)
			goto nodeptrEndJump;

		nodePtr=nodePtr->next;
		nodeStack.push(nodePtr);
	   
		if(!XMLHelpFwdToElem(nodePtr,"filterdefaults"))
		{
			nodePtr=nodePtr->xmlChildrenNode;

			if(nodePtr)
			{

				
				while(!XMLHelpNextType(nodePtr,XML_ELEMENT_NODE))
				{
					string s;
					
					s=(char *)(nodePtr->name);
					Filter *f;
					f=makeFilter(s);

					if(!f)
					{
						errMessage="Unable to determine filter type in defaults listing.";
						throw 1;
					}
			

					//potentially hazardous filters cannot have their
					//default properties altered. Quietly drop them
					if(!f->canBeHazardous())
					{
						nodeStack.push(nodePtr);
						nodePtr=nodePtr->xmlChildrenNode;
						if(f->readState(nodePtr))
							filterDefaults.push_back(f);

						nodePtr=nodeStack.top();
						nodeStack.pop();
					}	
				}

			}
		}
		
		//restore old node	
		nodePtr=nodeStack.top();
		nodeStack.pop();
		
		//Advance and push
		if(nodePtr->next)
			goto nodeptrEndJump;
		nodePtr=nodePtr->next;
		nodeStack.push(nodePtr);
		if(!XMLHelpFwdToElem(nodePtr,"startuppanels"))
		{
			startupPanelView.resize(CONFIG_STARTUPPANEL_END_ENUM);

			std::string tmpStr;
			xmlChar *xmlString;
			xmlString=xmlGetProp(nodePtr,(xmlChar*)"mode");

			if(xmlString)
			{
				tmpStr=(char*)xmlString;
				stream_cast(panelMode,tmpStr);
			
				if(panelMode >=CONFIG_PANELMODE_END_ENUM)	
					panelMode=CONFIG_PANELMODE_NONE;
			}

			if(panelMode)
			{
				xmlString=xmlGetProp(nodePtr,(xmlChar*)"rawdata");
				if(xmlString)
				{
				
					tmpStr=(char *)xmlString;
					if(tmpStr == "1")
						startupPanelView[CONFIG_STARTUPPANEL_RAWDATA]=true;
					else
						startupPanelView[CONFIG_STARTUPPANEL_RAWDATA]=false;
				}
				
				xmlString=xmlGetProp(nodePtr,(xmlChar*)"control");
				if(xmlString)
				{
				
					tmpStr=(char *)xmlString;
					if(tmpStr == "1")
						startupPanelView[CONFIG_STARTUPPANEL_CONTROL]=true;
					else
						startupPanelView[CONFIG_STARTUPPANEL_CONTROL]=false;
				}

				xmlString=xmlGetProp(nodePtr,(xmlChar*)"plotlist");
				if(xmlString)
				{
				
					tmpStr=(char *)xmlString;
					if(tmpStr == "1")
						startupPanelView[CONFIG_STARTUPPANEL_PLOTLIST]=true;
					else
						startupPanelView[CONFIG_STARTUPPANEL_PLOTLIST]=false;
				}
		
			}
		}

nodeptrEndJump:
		;

	}
	catch (int)
	{
		//Code threw an error, just say "bad parse" and be done with it
		delete paths;
		xmlFreeDoc(doc);
		return CONFIG_ERR_BADFILE;
	}


	delete paths;
	xmlFreeDoc(doc);

	return 0;

}

bool ConfigFile::write()
{
	string filename;
	wxStandardPaths *paths = new wxStandardPaths;

	wxString filePath = paths->GetDocumentsDir()+wxCStr("/.")+wxCStr(PROGRAM_NAME);
	filePath+=wxCStr("/") + wxCStr(CONFIG_FILENAME);
	filename = stlStr(filePath);
	
	//Open file for output
	std::ofstream f(filename.c_str());

	if(!f)
		return false;

	//Write state open tag 
	f<< "<threeDepictconfig>" << endl;
	f<<tabs(1)<< "<writer version=\"" << PROGRAM_VERSION << "\"/>" << endl;

	f<<tabs(1) << "<recent>" << endl;

	for(unsigned int ui=0;ui<recentFiles.size();ui++)
		f<<tabs(2) << "<file name=\"" << recentFiles[ui] << "\"/>" << endl;

	f<< tabs(1) << "</recent>" << endl;
	
	f<< tabs(1) << "<filterdefaults>" << endl;

	for(unsigned int ui=0;ui<filterDefaults.size();ui++)
		filterDefaults[ui]->writeState(f,STATE_FORMAT_XML,2);
	f<< tabs(1) << "</filterdefaults>" << endl;

	if(startupPanelView.size())
	{
		ASSERT(startupPanelView.size() == CONFIG_STARTUPPANEL_END_ENUM);

		f << tabs(1) << "<startuppanels mode=\"" << panelMode << "\" rawdata=\"" << 
			boolStrEnc(startupPanelView[CONFIG_STARTUPPANEL_RAWDATA]) << 
			"\" control=\"" << boolStrEnc(startupPanelView[CONFIG_STARTUPPANEL_CONTROL])  << 
			"\" plotlist=\"" << boolStrEnc(startupPanelView[CONFIG_STARTUPPANEL_PLOTLIST]) << "\"/>" << endl;
	}

	f << "</threeDepictconfig>" << endl;


	delete paths;
	return true;
}

bool ConfigFile::getPanelEnabled(unsigned int panelID) const
{
	ASSERT(panelID < CONFIG_STARTUPPANEL_END_ENUM);

	switch(panelMode)
	{
		case CONFIG_PANELMODE_NONE:
			return true;	
		case CONFIG_PANELMODE_REMEMBER:
		case CONFIG_PANELMODE_SPECIFY:
			if(startupPanelView.size())
			{
				ASSERT(startupPanelView.size() == CONFIG_STARTUPPANEL_END_ENUM);
				return startupPanelView[panelID];
			}
			else
				return true;
		default:
			ASSERT(false);
	}
}

void ConfigFile::setPanelEnabled(unsigned int panelID, bool enabled, bool permanent) 
{
	ASSERT(panelID < CONFIG_STARTUPPANEL_END_ENUM);
	
	//Create the vector as neeeded, filling with default of "enbaled"
	if(!startupPanelView.size())
		startupPanelView.resize(CONFIG_STARTUPPANEL_END_ENUM,true);

	ASSERT(startupPanelView.size() == CONFIG_STARTUPPANEL_END_ENUM);

	if(panelMode != CONFIG_PANELMODE_SPECIFY || permanent)
		startupPanelView[panelID] = enabled;
}

void ConfigFile::setStartupPanelMode(unsigned int panelM)
{
	ASSERT(panelM < CONFIG_PANELMODE_END_ENUM);
	panelMode=panelM;
}
		
unsigned int ConfigFile::getStartupPanelMode() const
{
	return panelMode;
}



