/*
 *	threeDepict.cpp - main program implementation
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


#include <wx/wx.h>
#include <wx/cmdline.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

#ifdef __APPLE__
#include "CoreFoundation/CoreFoundation.h"
#endif

#include "common/translation.h"
#include "gui/mainFrame.h"

//Unit testing code
#include "testing/testing.h"

enum
{
	ID_MAIN_WINDOW=wxID_ANY+1,
};

class threeDepictApp: public wxApp {
private:
	MainWindowFrame* MainFrame ;
	wxArrayString commandLineFiles;
	wxLocale* usrLocale;
	long language;

	void initLanguageSupport();


public:

    threeDepictApp() ;
    ~threeDepictApp() { if(usrLocale) delete usrLocale;}
    bool OnInit();
    virtual void OnInitCmdLine(wxCmdLineParser& parser);
    virtual bool OnCmdLineParsed(wxCmdLineParser& parser);
		 
    int FilterEvent(wxEvent &event);
#ifdef __APPLE__
    void MacOpenFile(const wxString & fileName);
    void MacReopenFile(const wxString & fileName);
#endif

#ifdef DEBUG
    void setEventloggerFile(const char *file);
#endif
};

//Check version is in place because wxT is deprecated for wx 2.9
//Command line parameter table
static const wxCmdLineEntryDesc g_cmdLineDesc [] =
{
	{ wxCMD_LINE_SWITCH, ("h"), ("help"), ("displays this message"),
		wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
	{ wxCMD_LINE_PARAM,  NULL, NULL, ("inputfile"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL | wxCMD_LINE_PARAM_MULTIPLE},
	//Unit testing system
#ifdef DEBUG
	{ wxCMD_LINE_SWITCH, ("t"), ("test"), ("Run debug unit tests, returns nonzero on test failure, zero on success.\n\t\t"
		       "XML files may be passed to run , instead of default tests"), wxCMD_LINE_VAL_NONE, wxCMD_LINE_SWITCH},
#endif
  { wxCMD_LINE_NONE,NULL,NULL,NULL,wxCMD_LINE_VAL_NONE,0 }

};

#ifdef __WXMSW__
//Force a windows console to show for cerr/cout
#ifdef DEBUG
#include "winconsole.h"
winconsole winC;
#endif
#endif

//DEBUG NaN and INF
#ifdef DEBUG
#ifdef __linux__
#include <fenv.h>
void trapfpe () {
//  feenableexcept(FE_INVALID|FE_DIVBYZERO|FE_OVERFLOW);
}
#endif
#endif

IMPLEMENT_APP(threeDepictApp)

threeDepictApp::threeDepictApp()
{
       	MainFrame=0;usrLocale=0;
#ifndef DEBUG
	//Wx 2.9 and up now has assertions auto-enabled. 
	//Disable for release builds
	wxSetAssertHandler(NULL);
#endif
}


void threeDepictApp::initLanguageSupport()
{
	language =  wxLANGUAGE_DEFAULT;

	// load language if possible, fall back to English otherwise
	if(wxLocale::IsAvailable(language))
	{
		//Wx 2.9 and above are now unicode, so locale encoding
		//conversion is deprecated.
#if !wxCHECK_VERSION(2, 9, 0)
		usrLocale = new wxLocale( language, wxLOCALE_CONV_ENCODING | wxLOCALE_LOAD_DEFAULT);
#else
		usrLocale = new wxLocale( language, wxLOCALE_LOAD_DEFAULT);
#endif

#if defined(__WXMAC__)
		wxStandardPaths* paths = (wxStandardPaths*) &wxStandardPaths::Get();
		usrLocale->AddCatalogLookupPathPrefix(paths->GetResourcesDir());
		
#elif defined(__WIN32__)
		wxStandardPaths* paths = (wxStandardPaths*) &wxStandardPaths::Get();
		usrLocale->AddCatalogLookupPathPrefix(paths->GetResourcesDir());
		usrLocale->AddCatalogLookupPathPrefix ( wxT ( "locales" ) );
#endif
		usrLocale->AddCatalog((PROGRAM_NAME));



		if(! usrLocale->IsOk() )
		{
			std::cerr << "Unable to initialise usrLocale, falling back to English" << std::endl;
			delete usrLocale;
			usrLocale = new wxLocale( wxLANGUAGE_ENGLISH );
			language = wxLANGUAGE_ENGLISH;
		}
		else
		{
			//Set the gettext language
			textdomain( PROGRAM_NAME );
			setlocale (LC_ALL, "");
#ifdef __WXMAC__
			bindtextdomain( PROGRAM_NAME, paths->GetResourcesDir().mb_str(wxConvUTF8) );
#elif defined(__WIN32) || defined(__WIN64)
			cerr << paths->GetResourcesDir().mb_str(wxConvUTF8) << endl;
			std::string s;
			s =  paths->GetResourcesDir().mb_str(wxConvUTF8);
			s+="/locales/";
			bindtextdomain( PROGRAM_NAME, s.c_str() );
			//The names for the codesets are in confg.charset in gettext-runtime/intl in
			// the gettext package. Tell gettext what codepage windows is using.
			//
			// The windows lookup codes are at
			// http://msdn.microsoft.com/en-us/library/dd317756%28v=VS.85%29.aspx
			unsigned int curPage;
			curPage=GetACP();
			switch(curPage)
			{
				case 1252:
					cerr << "Bound cp1252" << endl;
					bind_textdomain_codeset(PROGRAM_NAME, "CP1252");
					break;
				case 65001:
					cerr << "Bound utf8"<< endl;
					bind_textdomain_codeset(PROGRAM_NAME, "UTF-8");
					break;
				default:
					cerr << "Unknown codepage " << curPage << endl;
					break;
			}			
#else
			bindtextdomain( PROGRAM_NAME, "/usr/share/locale" );
			bind_textdomain_codeset(PROGRAM_NAME, "utf-8");
#endif
		}
	}
	else
	{
		std::cout << "Language not supported, falling back to English" << endl;
		usrLocale = new wxLocale( wxLANGUAGE_ENGLISH );
		language = wxLANGUAGE_ENGLISH;
	}
}

//Catching key events globally.
int threeDepictApp::FilterEvent(wxEvent& event)
{
	//Process global keyboard (non-accelerator) events
	if ( event.GetEventType()==wxEVT_KEY_DOWN )
	{
		bool mainActive;
#ifdef __APPLE__
		mainActive=true; //Any way to actually get this?? wxGetActiveWindow() apparently returns null here.
#else
		mainActive =( wxGetTopLevelParent((wxWindow*)(wxGetActiveWindow())) == (wxWindow*)MainFrame);
#endif
	
		if(MainFrame && mainActive)
		{
			wxKeyEvent& keyEvent = (wxKeyEvent&)event;
			//Under GTK, escape aborts refresh. under mac, 
			//set it to also abort fullscreen, if not refreshing
			if(keyEvent.GetKeyCode()==WXK_ESCAPE)
			{
				if( MainFrame->isCurrentlyUpdatingScene())
				{
					wxCommandEvent cmd;
					MainFrame->OnProgressAbort( cmd);
					return true;
				}
#ifdef __APPLE__
				else if(MainFrame->IsFullScreen())
				{
					wxCommandEvent cmd;
					MainFrame->OnViewFullscreen(cmd);
					MainFrame->ShowFullScreen(false);
					return true;
				}
#endif

			}

			//If the user presses F5, generate refresh
			if( keyEvent.GetKeyCode()==WXK_F5)
			{
				wxCommandEvent cmd;
				MainFrame->OnButtonRefresh(cmd);
			}

#ifdef DEBUG
			//Determine if control or meta key is down (dep. platform)
			bool commandDown;
			commandDown=keyEvent.ControlDown();
			//If the user presses ctrl+insert, or alt+f5 
	                //(ctrl+f5 doesn't work on mac, nor does it have an insert key) 
            		//this activates hidden
			//functionality to create autosave from filtertree
			if((keyEvent.GetKeyCode()==WXK_INSERT && commandDown)
              		  || (keyEvent.GetKeyCode()==WXK_F5 && keyEvent.AltDown()) )
			{
				wxTimerEvent evt;
				MainFrame->OnAutosaveTimer(evt);
			}
#endif
		}

	}

    return -1;
}

//Command line help table and setup
void threeDepictApp::OnInitCmdLine(wxCmdLineParser& parser)
{
	wxString name,version,preamble;

	name=(PROGRAM_NAME);
	name=name+ wxT(" ");
	version=(PROGRAM_VERSION);
	version+=wxT("\n");

	preamble=wxT("Copyright (C) 2013  3Depict team\n");
	preamble+=wxT("This program comes with ABSOLUTELY NO WARRANTY; for details see LICENCE file.\n");
	preamble+=wxT("This is free software, and you are welcome to redistribute it under certain conditions.\n");
	preamble+=wxT("Source code is available under the terms of the GNU GPL v3.0 or any later version (http://www.gnu.org/licenses/gpl.txt)\n");
	parser.SetLogo(name+version+preamble);

	parser.SetDesc (g_cmdLineDesc);
	parser.SetSwitchChars (wxT("-"));
}

//Initialise wxwidgets parser
bool threeDepictApp::OnCmdLineParsed(wxCmdLineParser& parser)
{
#ifdef DEBUG
	if( parser.Found(wxT("test"))) 
	{
		//If we were given arguments, try to load them
		//otherwise use the inbuilt test files
		if(parser.GetParamCount())
		{
			for(unsigned int ui=0;ui<parser.GetParamCount();ui++)
			{
				wxFileName f;
				f.Assign(parser.GetParam(ui));

				std::string strFile;
				strFile=stlStr(f.GetFullPath());
				if( !f.FileExists() )
				{
					cerr << "Unable to locate file:" << strFile << endl;
					continue;
				}

				cerr << "Loading :" << strFile << endl ;

				{
				VisController visControl;
				if(!visControl.loadState(strFile.c_str(),cerr,false,true))
				{
					cerr << "Error loading state file:" << endl;
					exit(1);
				}

				//Run a refresh over the filter tree as a test
				FilterTree f;
				visControl.cloneFilterTree(f);
				if(f.hasHazardousContents())
				{
					f.stripHazardousContents();
					cerr << "For security reasons, the tree was pruned prior to execution." << endl;
				}
				
				if(!testFilterTree(f))
				{
					cerr << "Failed loading :" << strFile << " , aborting" << endl;
					exit(1);
				}
				}

				cerr << "OK" << endl; 

			}
			
			 
			cerr << "Test XML File(s) Loaded OK" << endl;	
			exit(0);
		}
		else
		{
			//Unit tests failed
			if(!runUnitTests()) 
			{
				cerr << "Unit tests failed" <<endl;
				exit(1);
			}
			else
			{
				cerr << "Unit tests succeeded!" <<endl;
				exit(0);
			}
		}
	}
#endif

	for(unsigned int ui=0;ui<parser.GetParamCount();ui++)
	{
		wxFileName f;
		f.Assign(parser.GetParam(ui));

		if( f.FileExists() )
			commandLineFiles.Add(f.GetFullPath());
		else
			std::cerr << TRANS("File : ") << stlStr(f.GetFullPath()) << TRANS(" does not exist. Skipping") << std::endl;

	}

	return true;
}

#ifdef __APPLE__
//Mac OSX drag/drop file support
void threeDepictApp::MacOpenFile(const wxString &filename)
{
	ASSERT(MainFrame);
	wxArrayString array;
	array.Add(filename);

	MainFrame->OnDropFiles(array,0,0);
}

void threeDepictApp::MacReopenFile(const wxString &filename)
{
	ASSERT(MainFrame);
	MainFrame->Raise();

	MacOpenFile(filename);
}

#endif

bool threeDepictApp::OnInit()
{

    initLanguageSupport();
	

    //Set the gettext language
    //Register signal handler for backtraces
    if (!wxApp::OnInit())
    	return false; 

    //Need to seed random number generator for entire program
    srand (time(NULL));

    //Use a heuristic method (ie look around) to find a good sans-serif font
    TTFFinder fontFinder;
    setDefaultFontFile(fontFinder.getBestFontFile(TTFFINDER_FONT_SANS));
    
    wxInitAllImageHandlers();
    MainFrame = new MainWindowFrame(NULL, ID_MAIN_WINDOW, wxEmptyString,wxDefaultPosition,wxDefaultSize);

    SetTopWindow(MainFrame);

#if defined(DEBUG) && defined(__linux__)
   trapfpe(); //Under Linux, enable  segfault on invalid floating point operations
#endif

#ifdef __APPLE__    
   	//Switch the working directory into the .app bundle's resources
	//directory using the absolute path
	CFBundleRef mainBundle = CFBundleGetMainBundle();
	CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
	char path[PATH_MAX];
	if (!CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path, PATH_MAX))
	{
		// error!
	} else
	{
		CFRelease(resourcesURL);
		chdir(path);
	}
#endif


    MainFrame->Show();
   
    MainFrame->checkShowTips();
    MainFrame->checkReloadAutosave();


    if(commandLineFiles.GetCount())
    	MainFrame->SetCommandLineFiles(commandLineFiles);

    MainFrame->fixSplitterWindow();

    MainFrame->finaliseStartup();
    return true;
}


