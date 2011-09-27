
#include "wxcommon.h"

#include "basics.h"

#include <wx/xml/xml.h>
#include <wx/url.h>
#include <wx/event.h>
#include <vector>
#include <string>

#if defined(WIN32) || defined(WIN64)
#include <wx/msw/registry.h>
#endif

//Auto update checking RSS URL
const char *RSS_FEED_LOCATION="http://threedepict.sourceforge.net/rss.xml";

//Auto update event for posting back to main thread upon completion
wxEventType RemoteUpdateAvailEvent = wxNewEventType(); // You get to choose the name yourself
		

//Maximum amount of content in RSS header is 1MB.
const unsigned int MAX_RSS_CONTENT_SIZE=1024*1024;

std::string inputString;

std::string locateDataFile(const char *name)
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

#ifdef __linux__

	//POssible search paths. Must have trailing slash. will
	//be searched in sequence.
	const unsigned int NUM_SEARCH_DIRS=4;
	const char *possibleDirs[] = { "./",
					"/usr/local/share/3Depict/",
					"/usr/share/3Depict/",
					"/usr/share/3depict/", //Under debian, we have to use lowercase according to the debian guidelines, so handle this case.
					"",
					};

	std::string s;
	for(unsigned int ui=0; ui<NUM_SEARCH_DIRS; ui++)
	{
		s=std::string(possibleDirs[ui]) + name;

		if(wxFileExists(wxStr(s)))
				return s;
	}

	//Give up and force cur working dir.
	return std::string(name);
#else

	//E.g. Mac
	//	- Look in cwd
	return  std::string(name);
#endif
}

void *VersionCheckThread::Entry()
{
  	wxCommandEvent event( RemoteUpdateAvailEvent);
	ASSERT(targetWindow);

	wxInputStream* inputStream;
	versionStr.clear();

	//Try to download RSS feed
	wxURL url(wxCStr(RSS_FEED_LOCATION));

	//If the URL could not be downloaded, tough.
	if (url.GetError() != wxURL_NOERR)
	{
		retrieveOK=false;
		complete=true;
		wxPostEvent(targetWindow,event);
		return 0;
	}	

	inputStream = url.GetInputStream();

	wxXmlDocument *doc= new wxXmlDocument;
	if(!doc->Load(*inputStream))
	{
		delete doc;
		retrieveOK=false;
		complete=true;
		wxPostEvent(targetWindow,event);
		return 0;
	}
	

	//Check we grabbed an RSS feed
	if(doc->GetRoot()->GetName() != wxT("rss"))
	{
		delete doc;
		retrieveOK=false;
		complete=true;
		wxPostEvent(targetWindow,event);
		return 0;
	}

	//Find first channel
	wxXmlNode *child = doc->GetRoot()->GetChildren();

	bool foundChannel=false;
	while(child)
	{
		if(child->GetName() == wxT("channel"))
		{
			foundChannel=true;
			break;
		}
	    child = child->GetNext();
	}

	if(!foundChannel)
	{
		delete doc;
		retrieveOK=false;
		complete=true;
		wxPostEvent(targetWindow,event);
		return 0;
	}
	
	std::vector<std::string> itemStrs;

	//Spin through all the <item> nodes in the first <channel></channel>
	wxXmlNode *itemNode=child->GetChildren();
	while(itemNode)
	{
		//OK, we have an item node,lets check its children
		if(itemNode->GetName() == wxT("item"))
		{
			child=itemNode->GetChildren();

			while(child)
			{
				//OK, we found a child node; 
				if(child->GetName() == wxT("title"))
				{
					std::string stlContent;
					wxString content = child->GetNodeContent();

					stlContent=stlStr(content);
					if(stlContent.size() < MAX_RSS_CONTENT_SIZE &&
						isVersionNumberString(stlContent))
						itemStrs.push_back(stlContent);
					break;
				}
	    
				child = child->GetNext();
			}

		}
	    
		itemNode = itemNode->GetNext();
	}
	delete doc;

	if(!itemStrs.size())
	{
		//hmm. thats odd. no items. guess we failed :(
		retrieveOK=false;
		complete=true;
		wxPostEvent(targetWindow,event);
		return 0;
	}

	//Find the greatest version number
	versionStr=getMaxVerStr(itemStrs);
	retrieveOK=true;
	complete=true;
	wxPostEvent(targetWindow,event);

	return 0;
}


//Does a process with a given ID both (1) exist, and (2) match the process name?
bool processMatchesName(size_t processID, const std::string &procName)
{

//Really, any system with a working "ps" command (i.e. posix compliant)
#if defined(__LINUX__) || defined(__BSD__)
	//Execute the ps process, then filter the output by processID
	
	wxArrayString stdOut;
	long res;
	res=wxExecute(wxT("ps ax"),stdOut,wxEXEC_SYNC);

	if(res !=0 )
		return false;

	std::string pidStr;
	stream_cast(pidStr,processID);
	//Parse stdout..
	for(size_t ui=0;ui<stdOut.GetCount();ui++)
	{
		std::string s;
		s=stlStr(stdOut[ui]);

		//FIXME: This is a little lax. finding the proc name should
		//check the position of the found string more heavily
		if(s.find(pidStr) == 0 && s.find(procName) != std::string::npos)
			return true;
	}

#else
	#error __FUNCTION__ not implemented.
#endif

	return false;
}


