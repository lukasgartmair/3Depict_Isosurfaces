
#include "wxcommon.h"

#if defined(WIN32) || defined(WIN64)
#include <wx/wx.h>
#include <wx/msw/registry.h>
#endif

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

	return std::string("/usr/share/3Depict/") + std::string(name);
#else

	//E.g. Mac
	//	- Look in cwd
	return  std::string(name);
#endif
}


