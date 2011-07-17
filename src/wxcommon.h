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
std::string locateDataFile(const char *name);

extern wxEventType RemoteUpdateAvailEvent; 

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
