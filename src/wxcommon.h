/*
 * wxcommon.h  - Common wxwidgets header stuff
 * Copyright (C) 2011  D Haley
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef WXCOMMON_H
#define WXCOMMON_H
#include <wx/wx.h>
#include <wx/treectrl.h>
#include <wx/thread.h>
#include <string>

#define wxCStr(a) wxString(a,*wxConvCurrent)
#define wxStr(a) wxString(a.c_str(),*wxConvCurrent)


//This function is adapted from
//http://www.creatis.insa-lyon.fr/software/public/creatools/bbtk/v0_9_2/doc/doxygen/bbtk/bbtkWx_8h-source.html
//Authors : Eduardo Davila, Laurent Guigues, Jean-Pierre Roux
//Used under CeCILLB licence, limited liability and no warranty by authors. 
//http://www.cecill.info/licences/Licence_CeCILL-B_V1-en.html 
//This licence is compatible with GPL licence used in 3Depict.
inline std::string stlStr(const wxString& s){
	std::string s2;
	if(s.wxString::IsAscii()) {
		s2=s.wxString::ToAscii();
	} else {
		const wxWX2MBbuf tmp_buf = wxConvCurrent->cWX2MB(s);
		const char *tmp_str = (const char*) tmp_buf;
		s2=std::string(tmp_str, strlen(tmp_str));
	}
	return s2;
}

#include <string>
//locate the file we are looking for in OS specfic paths
std::string locateDataFile(const char *name);

extern wxEventType RemoteUpdateAvailEvent; 


//Return true IFF process ID and process name match running process
bool processMatchesName(size_t processID, const std::string &procName);


class VersionCheckThread : public wxThread
{
	private:
		bool complete;
		bool retrieveOK;
		std::string versionStr;
		//Window to post event to upon completion
		wxWindow *targetWindow;
	public:
		VersionCheckThread(wxWindow *target) : wxThread(wxTHREAD_JOINABLE){targetWindow=0; complete=false;retrieveOK=false;targetWindow=target;}

		//!Set the target window to send completion event to 
		//before running !MUST BE DONE!
		void setTargetWindow(wxWindow *w) {targetWindow=w;};

		//!Used internally by wxwidgets to launch thread
		void *Entry();

		//Returns true upon completion of thread.
		bool isComplete()  const { return complete; }

		//Returns true if version string was retreived succesfully
		bool isRetrieveOK() const { return retrieveOK; }

		//Return the maximal version string obtained from the remote rss feed
		std::string getVerStr() {  return versionStr; }
};
#endif
