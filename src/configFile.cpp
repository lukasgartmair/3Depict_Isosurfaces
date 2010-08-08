#include "configFile.h"
#include "XMLHelper.h"

#include "common.h"

const char *CONFIG_FILENAME="config.xml";

const unsigned int MAX_RECENT=5;

#include <wx/stdpaths.h>

#include <iostream>
#include <cstdlib>
#include <fstream>
#include <stack>

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
	deque<string>::iterator it;
	it=std::find(recentFiles.begin(),recentFiles.end(),str);

	if(it!=recentFiles.end())
		recentFiles.erase(it);
}

bool ConfigFile::read()
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
		return false;
	}

	//Open the XML file
	//TODO: Nowhere do i check which DTD I have validated against.
	doc = xmlCtxtReadFile(context, filename.c_str(), NULL, XML_PARSE_DTDVALID);

	if(!doc)
	{
		//Open the XML file again, but without DTD validation
		doc = xmlCtxtReadFile(context, filename.c_str(), NULL, 0);

		if(!doc)
			return false;
	}
	else
	{
		//Check for context validity
		if(!context->valid)
		{
			//TODO: Implement some kind of warn message?
		}
	}
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
			throw 1;

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
					throw 1;
				if(stream_cast(thisName,xmlString))
					throw 1;

				recentFiles.push_back(thisName);
			}
		}
	}
	catch (int)
	{
		//Code threw an error, just say "bad parse" and be done with it
		delete paths;
		xmlFreeDoc(doc);
		return false;
	}


	delete paths;
	xmlFreeDoc(doc);

	return true;

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

	f << "</threeDepictconfig>" << endl;


	delete paths;
	return true;
}
