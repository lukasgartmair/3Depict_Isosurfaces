/*
 *	threeDepict.cpp - main program user interface and control implementation
 *	Copyright (C) 2011, D Haley 

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

//DEBUG NaN and INF
#ifdef DEBUG
#ifdef __linux__
#include <fenv.h>
#include <sys/cdefs.h>
void trapfpe () {
//  feenableexcept(FE_INVALID|FE_DIVBYZERO|FE_OVERFLOW);
}
#endif
#endif

#ifdef __APPLE__
//FIXME: workaround for UI layout under apple platform 
// wxMac appears to have problems with nested panels.
#define APPLE_EFFECTS_WORKAROUND  1
#endif

//Shut wxwidgets assertion errors up by defining a "safe" cb_sort wrapper macro
#if defined(__WXMAC__) || defined(__WXGTK20__)
	       
#include <wx/version.h>
#if (wxCHECK_VERSION(2,8,0) )
	#if (wxCHECK_VERSION(2,9,0))
	//Sorted combos not supported under gtk in 2.8 series 
	// http://trac.wxwidgets.org/ticket/4398
	//and not supported in mac.
	// http://trac.wxwidgets.org/ticket/12419
	#define SAFE_CB_SORT 0 
	#endif
#endif

#else
	#define SAFE_CB_SORT wxCB_SORT
#endif


enum
{
	MESSAGE_ERROR=1,
	MESSAGE_INFO,
	MESSAGE_HINT,
	MESSAGE_NONE_BUT_HINT,
	MESSAGE_NONE
};

#ifdef __WXMSW__
//Force a windows console to show for cerr/cout
#ifdef DEBUG
#include "winconsole.h"
winconsole winC;
#endif
#endif

//OS specific stuff
#ifdef __APPLE__
#include "CoreFoundation/CoreFoundation.h"
#endif
#if defined(__WIN32) || defined(__WIN64)
#include <windows.h>
#endif

#include "3Depict.h"
#include <utility>

//wxWidgets stuff
#include "wxcomponents.h"
#include <wx/colordlg.h>
#include <wx/aboutdlg.h> 
#include <wx/platinfo.h>
#include <wx/utils.h>
#include <wx/filename.h>
#include <wx/cmdline.h>
#include <wx/version.h>
#include <wx/filefn.h>
#include <wx/stdpaths.h>
#include <wx/progdlg.h>
#include <wx/display.h>
#include <wx/process.h>
#include <wx/dir.h>
#include <wx/choicdlg.h>

#if (wxMAJOR_VERSION >= 2) && (wxMINOR_VERSION > 8)
	#include <wx/utils.h>  // Needed for wxLaunchDefaultApplication
#else
	#include <wx/mimetype.h> //Needed for GetOpenCommand
#endif
//Custom program dialog windows
#include "StashDialog.h" //Stash editor
#include "resDialog.h" // resolution selection dialog
#include "ExportRngDialog.h" // Range export dialog
#include "ExportPos.h" // Ion export dialog
#include "prefDialog.h" // Preferences dialog

#include "translation.h"

//Program Icon
#include "art.h"

//Filter imports
#include "filters/rangeFile.h"
#include "filters/dataLoad.h"

using std::pair;
using std::max;

//milliseconds before clearing status bar (by invoking a status timer event)
const unsigned int STATUS_TIMER_DELAY=10000; 
//Milliseconds between querying viscontrol for needing update
const unsigned int UPDATE_TIMER_DELAY=50; 
//Milliseconds between progress bar updates 
const unsigned int PROGRESS_TIMER_DELAY=100; 
//Seconds between autosaves
const unsigned int AUTOSAVE_DELAY=300; 

//Default window size
const unsigned int DEFAULT_WIN_WIDTH=1024;
const unsigned int DEFAULT_WIN_HEIGHT=800;

//minimum startup window size
const unsigned int MIN_WIN_WIDTH=100;
const unsigned int MIN_WIN_HEIGHT=100;


//!Number of pages in the panel at the bottom
const unsigned int NOTE_CONSOLE_PAGE_OFFSET= 2;

//The conversion factor from the baseline shift slider to camera units
const float BASELINE_SHIFT_FACTOR=0.0002f;


const char *cameraIntroString=NTRANS("New camera name...");
const char *stashIntroString=NTRANS("New stash name....");

//Name of autosave state file. MUST end in .xml middle
const char *AUTOSAVE_PREFIX= "autosave.";
const char *AUTOSAVE_SUFFIX=".xml";




//This is the dropdown matching list. This must match
//the order for comboFilters_choices, as declared in 
//MainFrame's constructor

//--- These settings must be modified concomitantly.
const unsigned int FILTER_DROP_COUNT=14;


const char * comboFilters_choices[FILTER_DROP_COUNT] =
{
	NTRANS("Annotation"),
	NTRANS("Bounding Box"),
	NTRANS("Clipping"),
	NTRANS("Cluster Analysis"),
	NTRANS("Compos. Profiles"),
	NTRANS("Downsampling"),
	NTRANS("Extern. Prog."),
	NTRANS("Ion Colour"),
	NTRANS("Ion Info"),
	NTRANS("Ion Transform"),
	NTRANS("Mass Spectrum"),
	NTRANS("Range File"),
	NTRANS("Spat. Analysis"),
	NTRANS("Voxelisation")
};

//Mapping between filter ID and combo position
const unsigned int comboFiltersTypeMapping[FILTER_DROP_COUNT] = {
	FILTER_TYPE_ANNOTATION,
	FILTER_TYPE_BOUNDBOX,
	FILTER_TYPE_IONCLIP,
	FILTER_TYPE_CLUSTER_ANALYSIS,
	FILTER_TYPE_COMPOSITION,
	FILTER_TYPE_IONDOWNSAMPLE,
	FILTER_TYPE_EXTERNALPROC,
	FILTER_TYPE_IONCOLOURFILTER,
	FILTER_TYPE_IONINFO,
	FILTER_TYPE_TRANSFORM,
	FILTER_TYPE_SPECTRUMPLOT,
	FILTER_TYPE_RANGEFILE,
	FILTER_TYPE_SPATIAL_ANALYSIS,
	FILTER_TYPE_VOXELS
 };
//----

//Constant identifiers for binding events in wxwidgets "event table"
enum {
    
    //File menu
    ID_MAIN_WINDOW= wxID_ANY+1,

    ID_FILE_EXIT,
    ID_FILE_OPEN,
    ID_FILE_MERGE,
    ID_FILE_SAVE,
    ID_FILE_SAVEAS,
    ID_FILE_EXPORT_PLOT,
    ID_FILE_EXPORT_IMAGE,
    ID_FILE_EXPORT_IONS,
    ID_FILE_EXPORT_RANGE,
    ID_FILE_EXPORT_ANIMATION,
    ID_FILE_EXPORT_PACKAGE,

    //Edit menu
    ID_EDIT_UNDO,
    ID_EDIT_REDO,
    ID_EDIT_PREFERENCES,

    //Help menu
    ID_HELP_ABOUT,
    ID_HELP_HELP,
    ID_HELP_CONTACT,

    //View menu
    ID_VIEW_BACKGROUND,
    ID_VIEW_CONTROL_PANE,
    ID_VIEW_RAW_DATA_PANE,
    ID_VIEW_SPECTRA,
    ID_VIEW_PLOT_LEGEND,
    ID_VIEW_WORLDAXIS,
    ID_VIEW_FULLSCREEN,
    //Left hand note pane
    ID_NOTEBOOK_CONTROL,
    ID_NOTE_CAMERA,
    ID_NOTE_DATA,
    ID_NOTE_PERFORMANCE,
    ID_NOTE_TOOLS,
    ID_NOTE_VISUALISATION,
    //Lower pane
    ID_PANEL_DATA,
    ID_PANEL_VIEW,
    ID_NOTE_SPECTRA,
    ID_NOTE_RAW,
    ID_GRID_RAW_DATA,
    ID_BUTTON_GRIDCOPY,
    ID_LIST_PLOTS,

    //Splitter IDs
    ID_SPLIT_LEFTRIGHT,
    ID_SPLIT_FILTERPROP,
    ID_SPLIT_TOP_BOTTOM,
    ID_SPLIT_SPECTRA,
    ID_RAWDATAPANE_SPLIT,
    ID_CONTROLPANE_SPLIT,
   
    //Camera panel 
    ID_COMBO_CAMERA,
    ID_GRID_CAMERA_PROPERTY,
   
    //Filter panel 
    ID_COMBO_FILTER,
    ID_COMBO_STASH,
    ID_BTN_STASH_MANAGE,
    ID_CHECK_AUTOUPDATE,
    ID_FILTER_NAMES,
    ID_GRID_FILTER_PROPERTY,
    ID_LIST_FILTER,
    ID_TREE_FILTERS,
    ID_BUTTON_REFRESH,
    ID_BTN_EXPAND,
    ID_BTN_COLLAPSE,

    //Effects panel
    ID_EFFECT_ENABLE,
    ID_EFFECT_CROP_ENABLE,
    ID_EFFECT_CROP_AXISONE_COMBO,
    ID_EFFECT_CROP_PANELONE,
    ID_EFFECT_CROP_PANELTWO,
    ID_EFFECT_CROP_AXISTWO_COMBO,
    ID_EFFECT_CROP_CHECK_COORDS,
    ID_EFFECT_CROP_TEXT_DX,
    ID_EFFECT_CROP_TEXT_DY,
    ID_EFFECT_CROP_TEXT_DZ,
    ID_EFFECT_STEREO_ENABLE,
    ID_EFFECT_STEREO_COMBO,
    ID_EFFECT_STEREO_BASELINE_SLIDER,
    ID_EFFECT_STEREO_LENSFLIP,

    //Options panel
    ID_CHECK_ALPHA,
    ID_CHECK_LIGHTING,
    ID_CHECK_CACHING,
    ID_CHECK_WEAKRANDOM,
    ID_SPIN_CACHEPERCENT,
    	
    //Misc
    ID_PROGRESS_ABORT,
    ID_STATUS_TIMER,
    ID_PROGRESS_TIMER,
    ID_UPDATE_TIMER,
    ID_AUTOSAVE_TIMER,


};



 

MainWindowFrame::MainWindowFrame(wxWindow* parent, int id, const wxString& title, const wxPoint& pos, const wxSize& size, long style):
    wxFrame(parent, id, title, pos, size, style)
{
	initedOK=false;
	programmaticEvent=false;
	fullscreenState=0;
	verCheckThread=0;
	lastMessageType=MESSAGE_NONE;
	//Set up the program icon handler
	wxArtProvider::Push(new MyArtProvider);
	SetIcon(wxArtProvider::GetIcon(wxT("MY_ART_ID_ICON")));

	//Set up the drag and drop handler
	dropTarget = new FileDropTarget(this);
	SetDropTarget(dropTarget);
	
	//Set up the recently used files menu
	// Note that this cannot exceed 9. Items show up, but do not trigger events.
	// This is bug 12141 in wxWidgets : http://trac.wxwidgets.org/ticket/12141
	ASSERT(configFile.getMaxHistory() <=9);
	recentHistory= new wxFileHistory(configFile.getMaxHistory());


	programmaticEvent=false;
	currentlyUpdatingScene=false;
	statusTimer = new wxTimer(this,ID_STATUS_TIMER);
	progressTimer = new wxTimer(this,ID_PROGRESS_TIMER);
	updateTimer= new wxTimer(this,ID_UPDATE_TIMER);
	autoSaveTimer= new wxTimer(this,ID_AUTOSAVE_TIMER);
	requireFirstUpdate=true;


	//Tell the vis controller which window is to remain
	//semi-active during calls to wxSafeYield from callback system
	visControl.setYieldWindow(this);
	//Set up keyboard shortcuts "accelerators"
	wxAcceleratorEntry entries[1];
	entries[0].Set(0,WXK_ESCAPE,ID_PROGRESS_ABORT);
	wxAcceleratorTable accel(1, entries);
        SetAcceleratorTable(accel);

	// begin wxGlade: MainWindowFrame::MainWindowFrame
    splitLeftRight = new wxSplitterWindow(this, ID_SPLIT_LEFTRIGHT, wxDefaultPosition, wxDefaultSize, wxSP_3D|wxSP_BORDER);
    panelRight = new wxPanel(splitLeftRight, wxID_ANY);
    splitTopBottom = new wxSplitterWindow(panelRight, ID_SPLIT_TOP_BOTTOM, wxDefaultPosition, wxDefaultSize, wxSP_3D|wxSP_BORDER);
    noteDataView = new wxNotebook(splitTopBottom, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_LEFT);
    noteDataViewConsole = new wxPanel(noteDataView, wxID_ANY);
    noteRaw = new wxPanel(noteDataView, ID_NOTE_RAW);
    splitterSpectra = new wxSplitterWindow(noteDataView, ID_SPLIT_SPECTRA, wxDefaultPosition, wxDefaultSize, wxSP_3D|wxSP_BORDER);
    window_2_pane_2 = new wxPanel(splitterSpectra, wxID_ANY);
    panelTop = new BasicGLPane(splitTopBottom);

#if wxCHECK_VERSION(2,9,0)
    if(!panelTop->displaySupported())
    {
		wxMessageDialog *wxD  =new wxMessageDialog(this,
				wxTRANS("Unable to initialise the openGL (3D) panel. Program cannot start. Please check your video drivers.") 
			,wxTRANS("OpenGL Failed"),wxICON_ERROR|wxOK);
		
		wxD->ShowModal();

		cerr << wxTRANS("Unable to initialise the openGL (3D) panel. Program cannot start. Please check your video drivers.") << endl;
		delete wxD;
		return;
    }
#endif

#if defined(__WXGTK20__) && !wxCHECK_VERSION(2,9,0)
    //I had to work this out by studying the construtor, and then testing simultaneously
    //on a broken and working Gl install. booyah.
    if(!panelTop->m_glWidget)
    {
		wxMessageDialog *wxD  =new wxMessageDialog(this,
				wxTRANS("Unable to initialise the openGL (3D) panel. Program cannot start. Please check your video drivers.") 
			,wxTRANS("OpenGL Failed"),wxICON_ERROR|wxOK);
		
		wxD->ShowModal();

		cerr << wxTRANS("Unable to initialise the openGL (3D) panel. Program cannot start. Please check your video drivers.") << endl;
		delete wxD;
		return;
    }
#endif

    panelLeft = new wxPanel(splitLeftRight, wxID_ANY);
    notebookControl = new wxNotebook(panelLeft, ID_NOTEBOOK_CONTROL, wxDefaultPosition, wxDefaultSize, wxNB_RIGHT);
    noteTools = new wxPanel(notebookControl, ID_NOTE_PERFORMANCE, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    notePost = new wxPanel(notebookControl, wxID_ANY);
    noteEffects = new wxNotebook(notePost, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_LEFT);
    noteFxPanelStereo = new wxPanel(noteEffects, wxID_ANY);
    noteFxPanelCrop = new wxPanel(noteEffects, wxID_ANY);
    noteCamera = new wxScrolledWindow(notebookControl, ID_NOTE_CAMERA, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    noteData = new wxPanel(notebookControl, ID_NOTE_DATA, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    filterSplitter = new wxSplitterWindow(noteData,ID_SPLIT_FILTERPROP , wxDefaultPosition, wxDefaultSize, wxSP_3D|wxSP_BORDER);
    filterPropertyPane = new wxPanel(filterSplitter, wxID_ANY);
    //topPanelSizer_staticbox = new wxStaticBox(panelTop, -1, wxT(""));
    filterTreePane = new wxPanel(filterSplitter, wxID_ANY);
    MainFrame_Menu = new wxMenuBar();
    wxMenu* File = new wxMenu();
    File->Append(ID_FILE_OPEN, wxTRANS("&Open...\tCtrl+O"), wxTRANS("Open state file"), wxITEM_NORMAL);
    File->Append(ID_FILE_MERGE, wxTRANS("&Merge...\tCtrl+Shift+O"), wxTRANS("Merge other file"), wxITEM_NORMAL);
    
    recentFilesMenu=new wxMenu();
    recentHistory->UseMenu(recentFilesMenu);
    File->AppendSubMenu(recentFilesMenu,wxTRANS("&Recent"));
    fileSave = File->Append(ID_FILE_SAVE, wxTRANS("&Save\tCtrl+S"), wxTRANS("Save state to file"), wxITEM_NORMAL);
    fileSave->Enable(false);
    File->Append(ID_FILE_SAVEAS, wxTRANS("Save &As...\tCtrl+Shift+S"), wxTRANS("Save current state to new file"), wxITEM_NORMAL);
    File->AppendSeparator();
    wxMenu* FileExport = new wxMenu();
    FileExport->Append(ID_FILE_EXPORT_PLOT, wxTRANS("&Plot...\tCtrl+P"), wxTRANS("Export Current Plot"), wxITEM_NORMAL);
    FileExport->Append(ID_FILE_EXPORT_IMAGE, wxTRANS("&Image...\tCtrl+I"), wxTRANS("Export Current 3D View"), wxITEM_NORMAL);
    FileExport->Append(ID_FILE_EXPORT_IONS, wxTRANS("Ion&s...\tCtrl+N"), wxTRANS("Export Ion Data"), wxITEM_NORMAL);
    FileExport->Append(ID_FILE_EXPORT_RANGE, wxTRANS("Ran&ges...\tCtrl+G"), wxTRANS("Export Range Data"), wxITEM_NORMAL);
    FileExport->Append(ID_FILE_EXPORT_ANIMATION, wxTRANS("Ani&mation...\tCtrl+M"), wxTRANS("Export Animation"), wxITEM_NORMAL);
    FileExport->Append(ID_FILE_EXPORT_PACKAGE, wxTRANS("Pac&kage...\tCtrl+K"), wxTRANS("Export analysis package"), wxITEM_NORMAL);

    File->AppendSubMenu(FileExport,wxTRANS("&Export"));
    File->AppendSeparator();
#ifdef __APPLE__
    File->Append(ID_FILE_EXIT, wxTRANS("&Quit\tCtrl+Q"), wxTRANS("Exit Program"), wxITEM_NORMAL);
#else
    File->Append(ID_FILE_EXIT, wxTRANS("E&xit"), wxTRANS("Exit Program"), wxITEM_NORMAL);
#endif
    MainFrame_Menu->Append(File, wxTRANS("&File"));
    wxMenu* wxglade_tmp_menu_1 = new wxMenu();
    wxglade_tmp_menu_1->Append(ID_VIEW_BACKGROUND, 
		    wxTRANS("&Background Colour...\tCtrl+B"),wxTRANS("Change background colour"));
    wxglade_tmp_menu_1->AppendSeparator(); //Separator
#ifndef __APPLE__
    checkMenuControlPane= wxglade_tmp_menu_1->Append(ID_VIEW_CONTROL_PANE, 
		    wxTRANS("&Control Pane\tF3"), wxTRANS("Toggle left control pane"), wxITEM_CHECK);
#else
    checkMenuControlPane= wxglade_tmp_menu_1->Append(ID_VIEW_CONTROL_PANE, 
		    wxTRANS("&Control Pane\tAlt+C"), wxTRANS("Toggle left control pane"), wxITEM_CHECK);

#endif
    checkMenuControlPane->Check();
#ifndef __APPLE__
    checkMenuRawDataPane= wxglade_tmp_menu_1->Append(ID_VIEW_RAW_DATA_PANE, 
		    wxTRANS("&Raw Data Pane\tF4"), wxTRANS("Toggle raw data  pane (bottom)"), wxITEM_CHECK);
#else
    checkMenuRawDataPane= wxglade_tmp_menu_1->Append(ID_VIEW_RAW_DATA_PANE, 
		    wxTRANS("&Raw Data Pane\tAlt+R"), wxTRANS("Toggle raw data  pane (bottom)"), wxITEM_CHECK);
#endif
    checkMenuRawDataPane->Check();
#ifndef __APPLE__
    checkMenuSpectraList=wxglade_tmp_menu_1->Append(ID_VIEW_SPECTRA, wxTRANS("&Plot List\tF5"),wxTRANS("Toggle plot list"), wxITEM_CHECK);
#else
    checkMenuSpectraList=wxglade_tmp_menu_1->Append(ID_VIEW_SPECTRA, wxTRANS("&Plot List\tAlt+P"),wxTRANS("Toggle plot list"), wxITEM_CHECK);
#endif
    checkMenuSpectraList->Check();

    wxglade_tmp_menu_1->AppendSeparator(); //Separator
    wxMenu* viewPlot= new wxMenu();
    checkViewLegend=viewPlot->Append(ID_VIEW_PLOT_LEGEND,wxTRANS("&Legend\tCtrl+L"),wxTRANS("Toggle Legend display"),wxITEM_CHECK);
    checkViewLegend->Check();
    wxglade_tmp_menu_1->AppendSubMenu(viewPlot,wxTRANS("P&lot..."));
    checkViewWorldAxis=wxglade_tmp_menu_1->Append(ID_VIEW_WORLDAXIS,wxTRANS("&Axis\tCtrl+Shift+I"),wxTRANS("Toggle World Axis display"),wxITEM_CHECK);
    checkViewWorldAxis->Check();
    
    wxglade_tmp_menu_1->AppendSeparator(); //Separator
#ifndef __APPLE__
    menuViewFullscreen=wxglade_tmp_menu_1->Append(ID_VIEW_FULLSCREEN, wxTRANS("&Fullscreen mode\tF11"),wxTRANS("Next fullscreen mode: with toolbars"));
#else
    menuViewFullscreen=wxglade_tmp_menu_1->Append(ID_VIEW_FULLSCREEN, wxTRANS("&Fullscreen mode\tCtrl+Shift+F"),wxTRANS("Next fullscreen mode: with toolbars"));
#endif


    wxMenu *Edit = new wxMenu();
    editUndoMenuItem = Edit->Append(ID_EDIT_UNDO,wxTRANS("&Undo\tCtrl+Z"));
    editUndoMenuItem->Enable(false);
    editRedoMenuItem = Edit->Append(ID_EDIT_REDO,wxTRANS("&Redo\tCtrl+Y"));
   editRedoMenuItem->Enable(false);
    Edit->AppendSeparator();
    Edit->Append(ID_EDIT_PREFERENCES,wxTRANS("&Preferences"));

    MainFrame_Menu->Append(Edit, wxTRANS("&Edit"));


    MainFrame_Menu->Append(wxglade_tmp_menu_1, wxTRANS("&View"));
    wxMenu* Help = new wxMenu();
    Help->Append(ID_HELP_HELP, wxTRANS("&Help...\tCtrl+H"), wxTRANS("Show help files and documentation"), wxITEM_NORMAL);
    Help->Append(ID_HELP_CONTACT, wxTRANS("&Contact..."), wxTRANS("Open contact page"), wxITEM_NORMAL);
    Help->AppendSeparator();
    Help->Append(ID_HELP_ABOUT, wxTRANS("&About..."), wxTRANS("Information about this program"), wxITEM_NORMAL);
    MainFrame_Menu->Append(Help, wxTRANS("&Help"));
    SetMenuBar(MainFrame_Menu);
    lblSettings = new wxStaticText(noteData, wxID_ANY, wxTRANS("Stashed Filters"));


    comboStash = new wxComboBox(noteData, ID_COMBO_STASH, wxT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN|wxTE_PROCESS_ENTER|SAFE_CB_SORT);
    btnStashManage = new wxButton(noteData, ID_BTN_STASH_MANAGE, wxT("..."),wxDefaultPosition,wxSize(28,28));
    filteringLabel = new wxStaticText(noteData, wxID_ANY, wxTRANS("Data Filtering"));


    //Workaround for wx bug http://trac.wxwidgets.org/ticket/4398
    //Combo box wont sort even when asked under wxGTK<3.0
    // use sortedArrayString, rather than normal arraystring
    wxSortedArrayString filterNames;
    for(unsigned int ui=0;ui<FILTER_DROP_COUNT; ui++)
    {
	const char * str = comboFilters_choices[ui];

	//construct translation->comboFilters_choices offset.
	filterMap[TRANS(str)] = ui;
	//Add to filter name wxArray
	wxString wxStrTrans = wxTRANS(str);
	filterNames.Add(wxStrTrans);
    }



    comboFilters = new wxComboBox(filterTreePane, ID_COMBO_FILTER, wxT(""), wxDefaultPosition, wxDefaultSize, filterNames, wxCB_DROPDOWN|wxCB_READONLY|SAFE_CB_SORT);


    treeFilters = new wxTreeCtrl(filterTreePane, ID_TREE_FILTERS, wxDefaultPosition, wxDefaultSize, wxTR_HAS_BUTTONS|wxTR_NO_LINES|wxTR_HIDE_ROOT|wxTR_DEFAULT_STYLE|wxSUNKEN_BORDER|wxTR_EDIT_LABELS);
    lastRefreshLabel = new wxStaticText(filterTreePane, wxID_ANY, wxTRANS("Last Outputs"));
    listLastRefresh = new wxListCtrl(filterTreePane, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT|wxSUNKEN_BORDER);
    checkAutoUpdate = new wxCheckBox(filterTreePane, ID_CHECK_AUTOUPDATE, wxTRANS("Auto Refresh"));
    refreshButton = new wxButton(filterTreePane, wxID_REFRESH, wxEmptyString);
    btnFilterTreeExpand= new wxButton(filterTreePane, ID_BTN_EXPAND, wxT("▼"),wxDefaultPosition,wxSize(30,30));
    btnFilterTreeCollapse = new wxButton(filterTreePane, ID_BTN_COLLAPSE, wxT("▲"),wxDefaultPosition,wxSize(30,30));
    propGridLabel = new wxStaticText(filterPropertyPane, wxID_ANY, wxTRANS("Filter settings"));
    gridFilterProperties = new wxPropertyGrid(filterPropertyPane, ID_GRID_FILTER_PROPERTY);
    labelCameraName = new wxStaticText(noteCamera, wxID_ANY, wxTRANS("Camera Name"));
    comboCamera = new wxComboBox(noteCamera, ID_COMBO_CAMERA, wxT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN|wxTE_PROCESS_ENTER );
    buttonRemoveCam = new wxButton(noteCamera, wxID_REMOVE, wxEmptyString);
    cameraNamePropertySepStaticLine = new wxStaticLine(noteCamera, wxID_ANY);
    gridCameraProperties = new wxPropertyGrid(noteCamera, ID_GRID_CAMERA_PROPERTY);
#ifndef APPLE_EFFECTS_WORKAROUND
    checkPostProcessing = new wxCheckBox(notePost, ID_EFFECT_ENABLE, wxTRANS("3D Post-processing"));
#endif
    checkFxCrop = new wxCheckBox(noteFxPanelCrop, ID_EFFECT_CROP_ENABLE, wxTRANS("Enable Cropping"));
    const wxString comboFxCropAxisOne_choices[] = {
        wxTRANS("x-y"),
        wxTRANS("x-z"),
        wxTRANS("y-x"),
        wxTRANS("y-z"),
        wxTRANS("z-x"),
        wxTRANS("z-y")
    };
    comboFxCropAxisOne = new wxComboBox(noteFxPanelCrop, ID_EFFECT_CROP_AXISONE_COMBO, wxT(""), wxDefaultPosition, wxDefaultSize, 6, comboFxCropAxisOne_choices, wxCB_SIMPLE|wxCB_DROPDOWN|wxCB_READONLY);
    panelFxCropOne = new CropPanel(noteFxPanelCrop, ID_EFFECT_CROP_AXISONE_COMBO,
		   		 wxDefaultPosition,wxDefaultSize,wxEXPAND);
    const wxString comboFxCropAxisTwo_choices[] = {
        wxTRANS("x-y"),
        wxTRANS("x-z"),
        wxTRANS("y-x"),
        wxTRANS("y-z"),
        wxTRANS("z-x"),
        wxTRANS("z-y")
    };
    comboFxCropAxisTwo = new wxComboBox(noteFxPanelCrop, ID_EFFECT_CROP_AXISTWO_COMBO, wxT(""), wxDefaultPosition, wxDefaultSize, 6, comboFxCropAxisTwo_choices, wxCB_SIMPLE|wxCB_DROPDOWN|wxCB_READONLY);
    panelFxCropTwo = new CropPanel(noteFxPanelCrop, ID_EFFECT_CROP_AXISTWO_COMBO,wxDefaultPosition,wxDefaultSize,wxEXPAND);
    checkFxCropCameraFrame = new wxCheckBox(noteFxPanelCrop,ID_EFFECT_CROP_CHECK_COORDS,wxTRANS("Use camera coordinates"));
    labelFxCropDx = new wxStaticText(noteFxPanelCrop, wxID_ANY, wxTRANS("dX"));
    textFxCropDx = new wxTextCtrl(noteFxPanelCrop, ID_EFFECT_CROP_TEXT_DX, wxEmptyString);
    labelFxCropDy = new wxStaticText(noteFxPanelCrop, wxID_ANY, wxTRANS("dY"));
    textFxCropDy = new wxTextCtrl(noteFxPanelCrop, ID_EFFECT_CROP_TEXT_DY, wxEmptyString);
    labelFxCropDz = new wxStaticText(noteFxPanelCrop, wxID_ANY, wxTRANS("dZ"));
    textFxCropDz = new wxTextCtrl(noteFxPanelCrop, ID_EFFECT_CROP_TEXT_DZ, wxEmptyString);
    checkFxEnableStereo = new wxCheckBox(noteFxPanelStereo, ID_EFFECT_STEREO_ENABLE, wxTRANS("Enable Anaglyphic Stereo"));
    checkFxStereoLensFlip= new wxCheckBox(noteFxPanelStereo, ID_EFFECT_STEREO_LENSFLIP, wxTRANS("Flip Channels"));
    lblFxStereoMode = new wxStaticText(noteFxPanelStereo, wxID_ANY, wxTRANS("Anaglyph Mode"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
    const wxString comboFxStereoMode_choices[] = {
        wxTRANS("Red-Blue"),
        wxTRANS("Red-Green"),
        wxTRANS("Red-Cyan"),
        wxTRANS("Green-Magenta"),
    };
    comboFxStereoMode = new wxComboBox(noteFxPanelStereo, ID_EFFECT_STEREO_COMBO, wxT(""), wxDefaultPosition, wxDefaultSize, 4, comboFxStereoMode_choices, wxCB_DROPDOWN|wxCB_SIMPLE|wxCB_READONLY);
    bitmapFxStereoGlasses = new wxStaticBitmap(noteFxPanelStereo, wxID_ANY, wxNullBitmap);
    labelFxStereoBaseline = new wxStaticText(noteFxPanelStereo, wxID_ANY, wxTRANS("Baseline Separation"));
    sliderFxStereoBaseline = new wxSlider(noteFxPanelStereo,ID_EFFECT_STEREO_BASELINE_SLIDER, 20, 0, 100);
    checkAlphaBlend = new wxCheckBox(noteTools,ID_CHECK_ALPHA , wxTRANS("Smooth && translucent objects"));
    checkAlphaBlend->SetValue(true);
    checkLighting = new wxCheckBox(noteTools, ID_CHECK_LIGHTING, wxTRANS("3D lighting"));
    checkLighting->SetValue(true);
    checkWeakRandom = new wxCheckBox(noteTools, ID_CHECK_WEAKRANDOM, wxTRANS("Fast and weak randomisation."));
    checkWeakRandom->SetValue(true);
    checkCaching = new wxCheckBox(noteTools, ID_CHECK_CACHING, wxTRANS("Filter caching"));
    checkCaching->SetValue(true);
    labelMaxRamUsage = new wxStaticText(noteTools, wxID_ANY, wxTRANS("Max. Ram usage (%)"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    spinCachePercent = new wxSpinCtrl(noteTools, ID_SPIN_CACHEPERCENT, wxT("50"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 100);
    panelView = new wxPanel(panelTop, ID_PANEL_VIEW);
    panelSpectra = new MathGLPane(splitterSpectra, wxID_ANY);
    plotListLabel = new wxStaticText(window_2_pane_2, wxID_ANY, wxTRANS("Plot List"));
    plotList = new wxListBox(window_2_pane_2, ID_LIST_PLOTS, wxDefaultPosition, wxDefaultSize, 0, NULL, wxLB_MULTIPLE|wxLB_NEEDED_SB|wxLB_SORT);
    gridRawData = new CopyGrid(noteRaw, ID_GRID_RAW_DATA);
    btnRawDataSave = new wxButton(noteRaw, wxID_SAVE, wxEmptyString);
    btnRawDataClip = new wxButton(noteRaw, wxID_COPY, wxEmptyString);
    textConsoleOut = new wxTextCtrl(noteDataViewConsole, 
		    wxID_ANY, wxEmptyString,wxDefaultPosition,
		    wxDefaultSize,wxTE_MULTILINE|wxTE_READONLY);


    //Disable post-processing
#ifndef APPLE_EFFECTS_WORKAROUND
    checkPostProcessing->SetValue(false); 
    noteFxPanelCrop->Enable(false);
    noteFxPanelStereo->Enable(false);
#else
    //Disable Fx panel stereo controls explicitly
    comboFxStereoMode->Enable(false);
    sliderFxStereoBaseline->Enable(false);
    checkFxStereoLensFlip->Enable(false);

    //Disable Crop controls explicitly
    checkFxCropCameraFrame->Enable(false);
    comboFxCropAxisOne->Enable(false);
    panelFxCropOne->Enable(false);
    comboFxCropAxisTwo->Enable(false);
    panelFxCropTwo->Enable(false);
    textFxCropDx->Enable(false);
    textFxCropDy->Enable(false);
    textFxCropDz->Enable(false);
    labelFxCropDx->Enable(false);
    labelFxCropDy->Enable(false);
    labelFxCropDz->Enable(false);

#endif

    //Link the crop panels in the post section appropriately
    panelFxCropOne->link(panelFxCropTwo,CROP_LINK_BOTH); 
    panelFxCropTwo->link(panelFxCropOne,CROP_LINK_BOTH); 



    //Manually tuned splitter parameters.
    filterSplitter->SetMinimumPaneSize(80);
    filterSplitter->SetSashGravity(0.8);
    splitLeftRight->SetSashGravity(0.15);
    splitTopBottom->SetSashGravity(0.85);
    splitterSpectra->SetSashGravity(0.82);

    //Last Refresh box
    listLastRefresh->InsertColumn(0,wxTRANS("Type"));
    listLastRefresh->InsertColumn(1,wxTRANS("Num"));
    MainFrame_statusbar = CreateStatusBar(3, 0);

    //Inform top panel about timer and timeouts
    panelTop->setParentStatus(MainFrame_statusbar,statusTimer,STATUS_TIMER_DELAY);

    panelTop->clearCameraUpdates();

    set_properties();
    do_layout();
    // end wxGlade
    //


    	
	//Try to load config file. If we can't no big deal.
	unsigned int errCode;
    	errCode=configFile.read();
	if(!errCode)
	{
		std::vector<std::string> strVec;

		//Set the files that are listed in the recent files 
		//menu
		configFile.getRecentFiles(strVec);

		for(unsigned int ui=0;ui<strVec.size();ui++)
			recentHistory->AddFileToHistory(wxStr(strVec[ui]));

		//Set the panel defaults (hidden/shown)
		if(!configFile.getPanelEnabled(CONFIG_STARTUPPANEL_CONTROL))
		{
			splitLeftRight->Unsplit(panelLeft);
			checkMenuControlPane->Check(false);
		}
		if(!configFile.getPanelEnabled(CONFIG_STARTUPPANEL_RAWDATA))
		{
			splitTopBottom->Unsplit();
			checkMenuRawDataPane->Check(false);
		}
		if(!configFile.getPanelEnabled(CONFIG_STARTUPPANEL_PLOTLIST))
		{
			splitterSpectra->Unsplit();
			checkMenuSpectraList->Check(false);
		}


		//Set the mouse zoom speeds
		float zoomRate,moveRate;
		zoomRate=configFile.getMouseZoomRate();
		moveRate=configFile.getMouseMoveRate();


		panelTop->setMouseZoomFactor((float)zoomRate/100.0f);
		panelTop->setMouseMoveFactor((float)moveRate/100.0f);

		

		
	}
	else
	{
		switch(errCode)
		{
			case CONFIG_ERR_NOFILE:
				break;
			case CONFIG_ERR_BADFILE:
			{
				textConsoleOut->AppendText(wxTRANS("Warning: Your configuration file appears to be invalid:\n"));
				wxString wxS = wxTRANS("\tConfig Load: ");
				wxS+= wxStr( configFile.getErrMessage());
				textConsoleOut->AppendText(wxS);
				break;
			}
			default:
				ASSERT(false);
		}
	}
	
	//Try to set the window size to a nice size
	SetSize(getNiceWindowSize());
	
	//Restore the sash panel positions
	//----------------
	if(configFile.configLoadedOK())
	{
		wxSize winSize;
		winSize=getNiceWindowSize(); 
		float val,oldGravity;
		val=configFile.getLeftRightSashPos();
		if(val > std::numeric_limits<float>::epsilon())
		{
			oldGravity=splitLeftRight->GetSashGravity();
			splitLeftRight->SetSashGravity(1.0);
			splitLeftRight->SetSashPosition((int)(val*(float)winSize.GetWidth()));
			splitLeftRight->SetSashGravity(oldGravity);
			
		}
		val=configFile.getTopBottomSashPos();
		if(val > std::numeric_limits<float>::epsilon())
		{
			oldGravity=splitTopBottom->GetSashGravity();
			splitTopBottom->SetSashGravity(1.0);
			splitTopBottom->SetSashPosition((int)(val*(float)winSize.GetHeight()));
			splitTopBottom->SetSashGravity(oldGravity);
		}
		val=configFile.getFilterSashPos();
		winSize=filterPropertyPane->GetSize();
		if(val > std::numeric_limits<float>::epsilon())
		{
			oldGravity=filterSplitter->GetSashGravity();
			filterSplitter->SetSashGravity(1.0);
			filterSplitter->SetSashPosition((int)(val*(float)winSize.GetHeight()));
			filterSplitter->SetSashGravity(oldGravity);
		}
		winSize=noteDataView->GetSize();
		val=configFile.getPlotListSashPos();
		if(val > std::numeric_limits<float>::epsilon())
		{
			oldGravity=splitterSpectra->GetSashGravity();
			splitterSpectra->SetSashGravity(1.0);
			splitterSpectra->SetSashPosition((int)(val*(float)winSize.GetWidth()));
			splitterSpectra->SetSashGravity(oldGravity);
		}
	}
	//-----------
	
	
	//Attempt to load the auto-save file, if it exists
	//-----------------
	checkReloadAutosave();
	//-----------------


	initedOK=true;   


	updateTimer->Start(UPDATE_TIMER_DELAY,wxTIMER_CONTINUOUS);
	autoSaveTimer->Start(AUTOSAVE_DELAY*1000,wxTIMER_CONTINUOUS);

#ifndef DISABLE_ONLINE_UPDATE
        wxDateTime datetime = wxDateTime::Today();	
	
	//Annoy the user, on average, every (% blah) days. Note that you need to choose the modulo numbers
	//carefully, such that the GCD of the maximum day (7), maximum week (53 or 52) and modulo number
	//are all coprime. Otherwise the numbers might "sync" and either always be true, or never be true.
	//See Euclid's algorithm for details
	const int MODULO_NUMBER=5;
       	if( configFile.getAllowOnlineVersionCheck() && 
		datetime.GetWeekOfYear() % MODULO_NUMBER == (datetime.GetWeekDay()) % MODULO_NUMBER)
	{
		verCheckThread=new VersionCheckThread(this);
		verCheckThread->Create();
		verCheckThread->Run();
	}
#endif
}

MainWindowFrame::~MainWindowFrame()
{

	//Delete and stop all the timers.
	delete statusTimer;
	delete progressTimer;
	delete updateTimer;
	delete autoSaveTimer;

	//delete the file history  pointer
	delete recentHistory;
}


BEGIN_EVENT_TABLE(MainWindowFrame, wxFrame)
    EVT_GRID_CMD_EDITOR_SHOWN(ID_GRID_FILTER_PROPERTY,MainWindowFrame::OnFilterGridCellEditorShow)
    EVT_GRID_CMD_EDITOR_HIDDEN(ID_GRID_FILTER_PROPERTY,MainWindowFrame::OnFilterGridCellEditorHide)
    EVT_GRID_CMD_EDITOR_SHOWN(ID_GRID_CAMERA_PROPERTY,MainWindowFrame::OnCameraGridCellEditorShow)
    EVT_GRID_CMD_EDITOR_HIDDEN(ID_GRID_CAMERA_PROPERTY,MainWindowFrame::OnCameraGridCellEditorHide)
    EVT_TIMER(ID_STATUS_TIMER,MainWindowFrame::OnStatusBarTimer)
    EVT_TIMER(ID_PROGRESS_TIMER,MainWindowFrame::OnProgressTimer)
    EVT_TIMER(ID_UPDATE_TIMER,MainWindowFrame::OnUpdateTimer)
    EVT_TIMER(ID_AUTOSAVE_TIMER,MainWindowFrame::OnAutosaveTimer)
    EVT_SPLITTER_UNSPLIT(ID_SPLIT_TOP_BOTTOM, MainWindowFrame::OnRawDataUnsplit) 
    EVT_SPLITTER_UNSPLIT(ID_SPLIT_LEFTRIGHT, MainWindowFrame::OnControlUnsplit) 
    EVT_SPLITTER_UNSPLIT(ID_SPLIT_SPECTRA, MainWindowFrame::OnSpectraUnsplit) 
    EVT_SPLITTER_DCLICK(ID_SPLIT_FILTERPROP, MainWindowFrame::OnFilterPropDoubleClick) 
    EVT_SPLITTER_DCLICK(ID_SPLIT_LEFTRIGHT, MainWindowFrame::OnControlUnsplit) 
    // begin wxGlade: MainWindowFrame::event_table
    EVT_MENU(ID_FILE_OPEN, MainWindowFrame::OnFileOpen)
    EVT_MENU(ID_FILE_MERGE, MainWindowFrame::OnFileMerge)
    EVT_MENU(ID_FILE_SAVE, MainWindowFrame::OnFileSave)
    EVT_MENU(ID_FILE_SAVEAS, MainWindowFrame::OnFileSaveAs)
    EVT_MENU(ID_FILE_EXPORT_PLOT, MainWindowFrame::OnFileExportPlot)
    EVT_MENU(ID_FILE_EXPORT_IMAGE, MainWindowFrame::OnFileExportImage)
    EVT_MENU(ID_FILE_EXPORT_IONS, MainWindowFrame::OnFileExportIons)
    EVT_MENU(ID_FILE_EXPORT_RANGE, MainWindowFrame::OnFileExportRange)
    EVT_MENU(ID_FILE_EXPORT_ANIMATION, MainWindowFrame::OnFileExportVideo)
    EVT_MENU(ID_FILE_EXPORT_PACKAGE, MainWindowFrame::OnFileExportPackage)
    EVT_MENU(ID_FILE_EXIT, MainWindowFrame::OnFileExit)

    EVT_MENU(ID_EDIT_UNDO, MainWindowFrame::OnEditUndo)
    EVT_MENU(ID_EDIT_REDO, MainWindowFrame::OnEditRedo)
    EVT_MENU(ID_EDIT_PREFERENCES, MainWindowFrame::OnEditPreferences)
    
    EVT_MENU(ID_VIEW_BACKGROUND, MainWindowFrame::OnViewBackground)
    EVT_MENU(ID_VIEW_CONTROL_PANE, MainWindowFrame::OnViewControlPane)
    EVT_MENU(ID_VIEW_RAW_DATA_PANE, MainWindowFrame::OnViewRawDataPane)
    EVT_MENU(ID_VIEW_SPECTRA, MainWindowFrame::OnViewSpectraList)
    EVT_MENU(ID_VIEW_PLOT_LEGEND, MainWindowFrame::OnViewPlotLegend)
    EVT_MENU(ID_VIEW_WORLDAXIS, MainWindowFrame::OnViewWorldAxis)
    EVT_MENU(ID_VIEW_FULLSCREEN, MainWindowFrame::OnViewFullscreen)

    EVT_MENU(ID_HELP_HELP, MainWindowFrame::OnHelpHelp)
    EVT_MENU(ID_HELP_CONTACT, MainWindowFrame::OnHelpContact)
    EVT_MENU(ID_HELP_ABOUT, MainWindowFrame::OnHelpAbout)
    EVT_MENU_RANGE(wxID_FILE1, wxID_FILE9, MainWindowFrame::OnRecentFile)
    
    EVT_BUTTON(wxID_REFRESH,MainWindowFrame::OnButtonRefresh)
    EVT_BUTTON(wxID_COPY,MainWindowFrame::OnButtonGridCopy)
    EVT_BUTTON(wxID_SAVE,MainWindowFrame::OnButtonGridSave)
    EVT_TEXT(ID_COMBO_STASH, MainWindowFrame::OnComboStashText)
    EVT_TEXT_ENTER(ID_COMBO_STASH, MainWindowFrame::OnComboStashEnter)
    EVT_COMBOBOX(ID_COMBO_STASH, MainWindowFrame::OnComboStash)
    EVT_TREE_END_DRAG(ID_TREE_FILTERS, MainWindowFrame::OnTreeEndDrag)
    EVT_TREE_ITEM_GETTOOLTIP(ID_TREE_FILTERS, MainWindowFrame::OnTreeItemTooltip)
    EVT_TREE_KEY_DOWN(ID_TREE_FILTERS, MainWindowFrame::OnTreeKeyDown)
    EVT_TREE_SEL_CHANGED(ID_TREE_FILTERS, MainWindowFrame::OnTreeSelectionChange)
    EVT_TREE_DELETE_ITEM(ID_TREE_FILTERS, MainWindowFrame::OnTreeDeleteItem)
    EVT_TREE_BEGIN_DRAG(ID_TREE_FILTERS, MainWindowFrame::OnTreeBeginDrag)
    EVT_BUTTON(ID_BTN_EXPAND, MainWindowFrame::OnBtnExpandTree)
    EVT_BUTTON(ID_BTN_COLLAPSE, MainWindowFrame::OnBtnCollapseTree)
    EVT_GRID_CMD_CELL_CHANGE(ID_GRID_FILTER_PROPERTY, MainWindowFrame::OnGridFilterPropertyChange)
    EVT_GRID_CMD_CELL_CHANGE(ID_GRID_CAMERA_PROPERTY, MainWindowFrame::OnGridCameraPropertyChange)
    EVT_TEXT(ID_COMBO_CAMERA, MainWindowFrame::OnComboCameraText)
    EVT_TEXT_ENTER(ID_COMBO_CAMERA, MainWindowFrame::OnComboCameraEnter)
    EVT_CHECKBOX(ID_CHECK_ALPHA, MainWindowFrame::OnCheckAlpha)
    EVT_CHECKBOX(ID_CHECK_LIGHTING, MainWindowFrame::OnCheckLighting)
    EVT_CHECKBOX(ID_CHECK_CACHING, MainWindowFrame::OnCheckCacheEnable)
    EVT_CHECKBOX(ID_CHECK_WEAKRANDOM, MainWindowFrame::OnCheckWeakRandom)
    EVT_SPINCTRL(ID_SPIN_CACHEPERCENT, MainWindowFrame::OnCacheRamUsageSpin)
    EVT_COMBOBOX(ID_COMBO_CAMERA, MainWindowFrame::OnComboCamera)
    EVT_COMBOBOX(ID_COMBO_FILTER, MainWindowFrame::OnComboFilter)
    EVT_TEXT_ENTER(ID_COMBO_FILTER, MainWindowFrame::OnComboFilterEnter)
    EVT_BUTTON(ID_BTN_STASH_MANAGE, MainWindowFrame::OnButtonStashDialog)
    EVT_BUTTON(wxID_REMOVE, MainWindowFrame::OnButtonRemoveCam)
    EVT_LISTBOX(ID_LIST_PLOTS, MainWindowFrame::OnSpectraListbox)
    EVT_CLOSE(MainWindowFrame::OnClose)
    EVT_TREE_END_LABEL_EDIT(ID_TREE_FILTERS,MainWindowFrame::OnTreeEndLabelEdit)
   
    //Post-processing stuff	
    EVT_CHECKBOX(ID_EFFECT_ENABLE, MainWindowFrame::OnCheckPostProcess)
    EVT_CHECKBOX(ID_EFFECT_CROP_ENABLE, MainWindowFrame::OnFxCropCheck)
    EVT_CHECKBOX(ID_EFFECT_CROP_CHECK_COORDS, MainWindowFrame::OnFxCropCamFrameCheck)
    EVT_COMBOBOX(ID_EFFECT_CROP_AXISONE_COMBO, MainWindowFrame::OnFxCropAxisOne)
    EVT_COMBOBOX(ID_EFFECT_CROP_AXISTWO_COMBO, MainWindowFrame::OnFxCropAxisTwo)
    EVT_CHECKBOX(ID_EFFECT_STEREO_ENABLE, MainWindowFrame::OnFxStereoEnable)
    EVT_CHECKBOX(ID_EFFECT_STEREO_LENSFLIP, MainWindowFrame::OnFxStereoLensFlip)
    EVT_COMBOBOX(ID_EFFECT_STEREO_COMBO, MainWindowFrame::OnFxStereoCombo)
    EVT_COMMAND_SCROLL(ID_EFFECT_STEREO_BASELINE_SLIDER, MainWindowFrame::OnFxStereoBaseline)
  

    EVT_COMMAND(wxID_ANY, RemoteUpdateAvailEvent, MainWindowFrame::OnCheckUpdatesThread)
    // end wxGlade
END_EVENT_TABLE();




void MainWindowFrame::OnFileOpen(wxCommandEvent &event)
{
	//Do not allow any action if a scene update is in progress
	if(currentlyUpdatingScene)
		return;

	//Load a file, either a state file, or a new pos file
	wxFileDialog *wxF = new wxFileDialog(this,wxTRANS("Select Data or State File..."), wxT(""),
		wxT(""),wxTRANS("Readable files (*.xml, *.pos, *.txt,*.csv)|*.xml;*.pos;*.txt;*.csv|POS File (*.pos)|*.pos|XML State File (*.xml)|*.xml|Text Data Files (*.txt/csv)|*.txt;*.csv|All Files (*)|*"),wxFD_OPEN|wxFD_FILE_MUST_EXIST);

	//Show the file dialog	
	if( (wxF->ShowModal() == wxID_CANCEL))
		return;

	textConsoleOut->Clear();
	//Load the file
	if(!loadFile(wxF->GetPath()))
		return;


	std::string tmp;
	tmp = stlStr(wxF->GetPath());
	configFile.addRecentFile(tmp);
	//Update the "recent files" menu
	recentHistory->AddFileToHistory(wxF->GetPath());
	


	//Get vis controller to update tree control to match internal
	//structure
	visControl.updateWxTreeCtrl(treeFilters);
	refreshButton->Enable(visControl.numFilters());

	bool updateOK;	
	updateOK =doSceneUpdate();
	//If we are using the default camera,
	//move it to make sure that it is visible
	if(updateOK)
	{
		if(visControl.numCams() == 1)
			visControl.ensureSceneVisible(3);

			statusMessage(TRANS("Loaded file."),MESSAGE_INFO);
	}

	panelTop->Refresh();
	
	wxF->Destroy();
}

void MainWindowFrame::OnFileMerge(wxCommandEvent &event)
{
	//Do not allow any action if a scene update is in progress
	if(currentlyUpdatingScene)
		return;

	//Load a file, either a state file, or a new pos file, or text file
	wxFileDialog *wxF = new wxFileDialog(this,wxTRANS("Select Data or State File..."), wxT(""),
		wxT(""),wxTRANS("3Depict file (*.xml, *.pos,*.txt)|*.xml;*.pos;*.txt|POS File (*.pos)|*.pos|XML State File (*.xml)|*.xml|All Files (*)|*"),wxFD_OPEN|wxFD_FILE_MUST_EXIST);

	//Show the file dialog	
	if( (wxF->ShowModal() == wxID_CANCEL))
		return;

	textConsoleOut->Clear();
	//Load the file
	loadFile(wxF->GetPath(),true);

	//Get vis controller to update tree control to match internal
	//structure
	visControl.updateWxTreeCtrl(treeFilters);
	refreshButton->Enable(visControl.numFilters());
	
	doSceneUpdate();
	
	wxF->Destroy();
}

void MainWindowFrame::OnDropFiles(const wxArrayString &files, int x, int y)
{

	textConsoleOut->Clear();
	wxMouseState wxm = wxGetMouseState();

	//Try opening the files as range (if ext. agrees)
	// or as 
	bool loaded =false;
	for(unsigned int ui=0;ui<files.Count();ui++)
	{
		string ext;

		//Check to see if can be loaded as a range file,
		//but only if there is a node to hang it off in the tree
		//----
		
		bool rangeOK;
		rangeOK=false;

		if(treeFilters->GetCount())
		{
			//Check the extension to see if it should be a range file
			wxFileName fileName;
			fileName.Assign(files[ui]);
			ext=stlStr(fileName.GetExt());

			for(size_t uj=0;uj<ext.size();uj++)
				ext[uj]=tolower(ext[uj]); 
			
			if(RangeFile::extensionIsRange(ext.c_str()))
			{
				//Now we have opened the range file,
				//we are going to have to splice it into the tree
				//TODO:Better to use the XY coordinates,
				// rather than just dropping it on the selection
				// or the first item
				wxTreeItemId id=treeFilters->GetSelection();;

				if(id == treeFilters->GetRootItem())
				{
					wxTreeItemIdValue cookie;
					id = treeFilters->GetFirstChild(id,cookie);
				}

				if(id.IsOk() && id!=treeFilters->GetRootItem())
				{
					RangeFile rng;
					string s;
					s=stlStr(files[ui]);
					if(!rng.openGuessFormat(s.c_str()))
					{
						rangeOK=true;

							
						//Load rangefile &  construct filter
						RangeFileFilter *f=(RangeFileFilter*)
							configFile.getDefaultFilter(FILTER_TYPE_RANGEFILE);
						//Copy across the range data
						f->setRangeData(rng);

						//Get the tree data container
						wxTreeItemData *tData=treeFilters->GetItemData(id);
						//Get the parent filter pointer	
						visControl.addFilter(f,((wxTreeUint *)tData)->value);
						//Rebuild tree control
						visControl.updateWxTreeCtrl(treeFilters,f);
					}
					else
					{
						//OK, we need to let the user know something went wrong.
						//Less annoying than a dialog, but the statusbar is going
						//to be useless, as it will be overwritten during the subsequent
						//refresh (when we treat this as a pos file).
						//FIXME: Something needs to go here... A queue for messages?
					}


				}
			}
		}
		//---
	
		//If it is a pos file, just handle it by trying to load
		if(!rangeOK)
		{

			//If command down, load first file using this,
			//then merge the rest
			if(!loaded)
			{
				if(loadFile(files[ui],!wxm.CmdDown()))
					loaded=true;
			}
			else
				loadFile(files[ui],true);
		}
	}


	if(!wxm.CmdDown() && files.Count())
	{
#ifdef __APPLE__    
		statusMessage(TRANS("Tip: You can use ⌘ (command) to merge"),MESSAGE_HINT);
#else
		statusMessage(TRANS("Tip: You can use ctrl to merge"),MESSAGE_HINT);
#endif
	}

	if(files.Count())
	{
		visControl.updateWxTreeCtrl(treeFilters);
		refreshButton->Enable(visControl.numFilters());
		doSceneUpdate();	
		//If we are using the default camera,
		//move it to make sure that it is visible
		if(visControl.numCams() == 1)
			visControl.ensureSceneVisible(3);
	}
}

bool MainWindowFrame::loadFile(const wxString &fileStr, bool merge)
{
	std::string dataFile = stlStr(fileStr);
	
	//Split the filename into chunks: path, volume, name and extension
	//the format of this is OS dependant, but wxWidgets can deal with this.
	wxFileName fname;
	wxString volume,path,name,ext;
	bool hasExt;
	fname.SplitPath(fileStr,&volume,
			&path,&name,&ext, &hasExt);

	//Test the extension to determine what we will do
	//TODO: This is really lazy, and I should use something like libmagic.
	std::string extStr;
	extStr=stlStr(ext);
	if( extStr == std::string("xml"))
	{
		std::stringstream ss;
		
		//Load the file as if it were an XML file
		if(!visControl.loadState(dataFile.c_str(),ss,merge))
		{
			std::string str;
			str=ss.str();
			textConsoleOut->AppendText(wxStr(str));
			wxMessageDialog *wxD  =new wxMessageDialog(this,
						wxTRANS("Error loading state file.\nSee console for more info.") 
						,wxTRANS("Load error"),wxOK|wxICON_ERROR);
			wxD->ShowModal();
			wxD->Destroy();
			return false;
		}


		if(visControl.hasHazardousContents())
		{
			wxMessageDialog *wxD  =new wxMessageDialog(this,
						wxTRANS("This state file contains filters that can be unsafe to run\nDo you wish to remove these before continuing?.") 
						,wxTRANS("Security warning"),wxYES_NO|wxICON_WARNING|wxYES_DEFAULT );

			wxD->SetAffirmativeId(wxID_YES);
			wxD->SetEscapeId(wxID_NO);

			if(wxD->ShowModal()!= wxID_NO)
				visControl.makeFiltersSafe();

		}

		//Update the background colour
		if(panelTop->isInited())
			panelTop->updateClearColour();

		checkViewWorldAxis->Check(visControl.getAxisVisible());

		//Update the camera dropdown
		vector<std::pair<unsigned int, std::string> > camData;
		visControl.getCamData(camData);

		comboCamera->Clear();
		unsigned int uniqueID;
		//Skip the first element, as it is a hidden camera.
		for(unsigned int ui=1;ui<camData.size();ui++)
		{
			//Do not delete as this will be deleted by wx
			comboCamera->Append(wxStr(camData[ui].second),
					(wxClientData *)new wxListUint(camData[ui].first));	
			//If this is the active cam (1) set the selection and (2) remember
			//the ID
			if(camData[ui].first == visControl.getActiveCamId())
			{
				comboCamera->SetSelection(ui-1);
				uniqueID=camData[ui].first;
			}
		}

		//Only update the camera grid if we have a valid uniqueID
		if(camData.size() > 1)
		{
			//Use the remembered ID to update the grid.
			visControl.updateCamPropertyGrid(gridCameraProperties,uniqueID);
		}
		else
		{
			//Reset the camera property fields & combo box
			gridCameraProperties->clear();
			comboCamera->SetValue(wxCStr(TRANS(cameraIntroString)));
		}

		//reset the stash combo box
		comboStash->SetValue(wxCStr(TRANS(stashIntroString)));


		//Check to see if we have any effects that we need to enable
		vector<const Effect*> effs;
		panelTop->currentScene.getEffects(effs);
		if(!effs.empty())
		{
			//OK, we have some effects; we will need to update the UI
			updateFxUI(effs);
		}



		currentFile =fileStr;
		fileSave->Enable(true);

		
		
		//Update the stash combo box
		comboStash->Clear();
	
		std::vector<std::pair<std::string,unsigned int > > stashList;
		visControl.getStashList(stashList);
		for(unsigned int ui=0;ui<stashList.size(); ui++)
		{
			wxListUint *u;
			u = new wxListUint(stashList[ui].second);
			comboStash->Append(wxStr(stashList[ui].first),(wxClientData *)u);
			ASSERT(comboStash->wxItemContainer::GetClientObject(comboStash->GetCount()-1));
		}

	}
	else if (extStr == std::string("txt") || extStr == std::string("csv"))
	{
		//Load the file as if it were a text file.
		unsigned int err;
	
		Filter *posFilter,*downSampleFilter;
		posFilter= configFile.getDefaultFilter(FILTER_TYPE_POSLOAD);
		downSampleFilter= configFile.getDefaultFilter(FILTER_TYPE_IONDOWNSAMPLE);

		//Bastardise the default settings such that it knows to use the correct
		// file type, based upon file extension
		((DataLoadFilter*)posFilter)->setFileMode(DATALOAD_TEXT_FILE);


		//No need to refresh scene, this is done internally	
		if((err = visControl.LoadIonSet(dataFile,posFilter,
						downSampleFilter)))
		{
			wxMessageDialog *wxD  =new wxMessageDialog(this,wxTRANS("Unable to open file")
							,wxTRANS("Load error"),wxOK|wxICON_ERROR);
			wxD->ShowModal();
			wxD->Destroy();

			return false;
		}

		currentFile.clear();
	}	
	else
	{
		//Load the file as if it were a pos file.
		unsigned int err;
		

		//No need to refresh scene, this is done internally	
		if((err = visControl.LoadIonSet(dataFile,
				configFile.getDefaultFilter(FILTER_TYPE_POSLOAD),
				configFile.getDefaultFilter(FILTER_TYPE_IONDOWNSAMPLE)))) 
		{
			wxMessageDialog *wxD  =new wxMessageDialog(this,wxTRANS("Unable to open file")
							,wxTRANS("Load error"),wxOK|wxICON_ERROR);
			wxD->ShowModal();
			wxD->Destroy();

			return false;
		}

		currentFile.clear();


	}

	return true;
}	

void MainWindowFrame::OnRecentFile(wxCommandEvent &event)
{

	if(currentlyUpdatingScene)
		return;

	wxString f(recentHistory->GetHistoryFile(event.GetId() - wxID_FILE1));

	if (!f.empty())
	{
		textConsoleOut->Clear();
		if(loadFile(f))
		{
			//Get vis controller to update tree control to match internal
			//structure
			visControl.updateWxTreeCtrl(treeFilters);
			refreshButton->Enable(visControl.numFilters());
			doSceneUpdate();
			//If we are using the default camera,
			//move it to make sure that it is visible
			if(visControl.numCams() == 1)
				visControl.ensureSceneVisible(3);
			panelTop->Refresh();
		}
		else
		{
			//Didn't load?  We don't want it.
			recentHistory->RemoveFileFromHistory(event.GetId()-wxID_FILE1);
			configFile.removeRecentFile(stlStr(f));
		}
	}
}

void MainWindowFrame::OnFileSave(wxCommandEvent &event)
{
	if(!currentFile.length())
		return;

	//If the file does not exist, use saveas instead
	if(!wxFileExists(currentFile))
	{
		OnFileSaveAs(event);

		return;
	}

	
	std::map<string,string> dummyMap;
	std::string dataFile = stlStr(currentFile);

	//Try to save the viscontrol state
	if(!visControl.saveState(dataFile.c_str(),dummyMap))
	{
		std::string errString;
		errString=TRANS("Unable to save. Check output destination can be written to.");
		
		wxMessageDialog *wxD  =new wxMessageDialog(this,wxStr(errString)
						,wxTRANS("Save error"),wxOK|wxICON_ERROR);
		wxD->ShowModal();
		wxD->Destroy();
	}
	else
	{
		fileSave->Enable(true);

		//Update the recent files, and the menu.
		configFile.addRecentFile(dataFile);
		recentHistory->AddFileToHistory(wxStr(dataFile));
		
		dataFile=std::string("Saved state: ") + dataFile;
		statusMessage(dataFile.c_str(),MESSAGE_INFO);

	}

}

void MainWindowFrame::OnFileExportPlot(wxCommandEvent &event)
{

	if(!panelSpectra->getNumVisible())
	{
		std::string errString;
		errString=TRANS("No plot available. Please create a plot before exporting.");
		wxMessageDialog *wxD  =new wxMessageDialog(this,wxStr(errString)
						,wxTRANS("Unable to save"),wxOK|wxICON_ERROR);
		wxD->ShowModal();

		wxD->Destroy();
		return;
	}

	wxFileDialog *wxF = new wxFileDialog(this,wxTRANS("Save plot..."), wxT(""),
		wxT(""),wxTRANS("By Extension (svg,png)|*.svg;*.png|Scalable Vector Graphics File (*.svg)|*.svg|PNG File (*.png)|*.png|All Files (*)|*"),wxFD_SAVE);

	if( (wxF->ShowModal() == wxID_CANCEL))
		return;

	std::string dataFile = stlStr(wxF->GetPath());
	
	//Split the filename into chunks: path, volume, name and extension
	//the format of this is OS dependant, but wxWidgets can deal with this.
	wxFileName fname;
	wxString volume,path,name,ext;
	bool hasExt;
	fname.SplitPath(wxF->GetPath(),&volume,
			&path,&name,&ext, &hasExt);


	std::string strExt;
	strExt=stlStr(ext);
	strExt = lowercase(strExt);
	wxF->Destroy();

	unsigned int errCode;

	//Try to save the file (if we recognise the extension)
	if(strExt == "svg")
		errCode=panelSpectra->saveSVG(dataFile);
	else if (strExt == "png")
	{
		//Show a resolution chooser dialog
		ResDialog d(this,wxID_ANY,wxTRANS("Choose resolution"));

		int plotW,plotH;
		panelSpectra->GetClientSize(&plotW,&plotH);
		d.setRes(plotW,plotH);
		if(d.ShowModal() == wxID_CANCEL)
			return;

		
		errCode=panelSpectra->savePNG(dataFile,d.getWidth(),d.getHeight());
	
	}	
	else
	{
		std::string errString;
		errString=TRANS("Unknown file extension. Please use \"svg\" or \"png\"");
		wxMessageDialog *wxD  =new wxMessageDialog(this,wxStr(errString)
						,wxTRANS("Unable to save"),wxOK|wxICON_ERROR);
		wxD->ShowModal();

		wxD->Destroy();
		return;
	}

	//Did we save OK? If not, let the user know
	if(errCode)
	{
		std::string errString;
		errString=panelSpectra->getErrString(errCode);
		
		wxMessageDialog *wxD  =new wxMessageDialog(this,wxStr(errString)
						,wxTRANS("Save error"),wxOK|wxICON_ERROR);
		wxD->ShowModal();
		wxD->Destroy();
	}
	else
	{
		dataFile=std::string("Saved plot: ") + dataFile;
		statusMessage(dataFile.c_str(),MESSAGE_INFO);
	}
}

void MainWindowFrame::OnFileExportImage(wxCommandEvent &event)
{
	wxFileDialog *wxF = new wxFileDialog(this,wxTRANS("Save Image..."), wxT(""),
		wxT(""),wxTRANS("PNG File (*.png)|*.png|All Files (*)|*"),wxFD_SAVE);

	if( (wxF->ShowModal() == wxID_CANCEL))
		return;

	std::string dataFile = stlStr(wxF->GetPath());
	
	//Show a resolution chooser dialog
	ResDialog d(this,wxID_ANY,wxTRANS("Choose resolution"));

	//Use the current res as the dialog default
	int w,h;
	panelTop->GetClientSize(&w,&h);
	d.setRes(w,h);

	//Show dialog, skip save if user cancels dialog
	if(d.ShowModal() == wxID_CANCEL)
		return;

	if((d.getWidth() < w && d.getHeight() > h) ||
		(d.getWidth() > w && d.getHeight() < h))
	{
		wxMessageDialog *wxD  =new wxMessageDialog(this,
				wxTRANS("Limitation on the screenshot dimension; please ensure that both width and height exceed the initial values,\n or that they are smaller than the initial values.\n If this bothers you, please submit a bug."),
				wxTRANS("Program limitation"),wxOK|wxICON_ERROR);

		wxD->ShowModal();

		wxD->Destroy();

		return;
	}


	bool saveOK=panelTop->saveImage(d.getWidth(),d.getHeight(),dataFile.c_str());

	if(!saveOK)
	{
		std::string errString;
		errString=TRANS("Unable to save. Check output destination can be written to.");
		
		wxMessageDialog *wxD  =new wxMessageDialog(this,wxStr(errString)
						,wxTRANS("Save error"),wxOK|wxICON_ERROR);
		wxD->ShowModal();
		wxD->Destroy();
	}
	else
	{
		dataFile=std::string("Saved 3D View :") + dataFile;
		statusMessage(dataFile.c_str(),MESSAGE_INFO);
	}

}


void MainWindowFrame::OnFileExportVideo(wxCommandEvent &event)
{
	wxFileDialog *wxF = new wxFileDialog(this,wxTRANS("Save Image..."), wxT(""),
		wxT(""),wxTRANS("PNG File (*.png)|*.png|All Files (*)|*"),wxFD_SAVE);

	if( (wxF->ShowModal() == wxID_CANCEL))
		return;

	
	//Show a resolution chooser dialog
	ResDialog d(this,wxID_ANY,wxTRANS("Choose resolution"));

	//Use the current res as the dialog default
	int w,h;
	panelTop->GetClientSize(&w,&h);
	d.setRes(w,h);

	//Show dialog, skip save if user cancels dialo
	if(d.ShowModal() == wxID_CANCEL)
	{
		wxF->Destroy();
		return;
	}

	if((d.getWidth() < w && d.getHeight() > h) ||
		(d.getWidth() > w && d.getHeight() < h) )
	{
		wxMessageDialog *wxD  =new wxMessageDialog(this,
				wxTRANS("Limitation on the screenshot dimension; please ensure that both width and height exceed the initial values,\n or that they are smaller than the initial values.\n If this bothers, please submit a bug."),
				wxTRANS("Program limitation"),wxOK|wxICON_ERROR);

		wxD->ShowModal();

		wxD->Destroy();
		wxF->Destroy();

		return;
	}


	wxFileName fname;
	wxString volume,path,name,ext;
	bool hasExt;
	fname.SplitPath(wxF->GetPath(),&volume,
			&path,&name,&ext, &hasExt);

	if(!hasExt)
		ext=wxT("png");

	///TODO: This is nasty and hackish. We should present a nice,
	//well laid out dialog for frame count (show angular increment) 
	wxTextEntryDialog *teD = new wxTextEntryDialog(this,wxTRANS("Number of frames"),wxTRANS("Frame count"),
						wxT("180"),(long int)wxOK|wxCANCEL);

	unsigned int numFrames=0;
	std::string strTmp;
	do
	{
		if(teD->ShowModal() == wxID_CANCEL)
		{
			wxF->Destroy();
			teD->Destroy();
			return;
		}


		strTmp = stlStr(teD->GetValue());
	}while(stream_cast(numFrames,strTmp));


	bool saveOK=panelTop->saveImageSequence(d.getWidth(),d.getHeight(),
							numFrames,path,name,ext);
							

	if(!saveOK)
	{
		std::string errString;
		errString=TRANS("Unable to save. Check output destination can be written to.");
		
		wxMessageDialog *wxD  =new wxMessageDialog(this,wxStr(errString)
						,wxTRANS("Save error"),wxOK|wxICON_ERROR);
		wxD->ShowModal();
		wxD->Destroy();
	}
	else
	{
		std::string dataFile = stlStr(wxF->GetPath());
		dataFile=std::string(TRANS("Saved 3D View :")) + dataFile;
		statusMessage(dataFile.c_str(),MESSAGE_INFO);
	}
	wxF->Destroy();

}

void MainWindowFrame::OnFileExportPackage(wxCommandEvent &event)
{
	if(!treeFilters->GetCount())
	{
		statusMessage(TRANS("No filters means no data to export"),MESSAGE_ERROR);
		return;

	}

	//This could be nicer, or reordered
	wxTextEntryDialog *wxT = new wxTextEntryDialog(this,wxTRANS("Package name"),
					wxTRANS("Package directory name"),wxT(""),wxOK|wxCANCEL);

	wxT->SetValue(wxTRANS("AnalysisPackage"));

	if(wxT->ShowModal() == wxID_CANCEL)
	{
		wxT->Destroy();
		return;
	}


	//Pop up a directory dialog, to choose the base path for the new folder
	wxDirDialog *wxD = new wxDirDialog(this);

	wxMessageDialog *wxMesD  =new wxMessageDialog(this,wxTRANS("Package folder already exists, won't overwrite.")
					,wxTRANS("Not available"),wxOK|wxICON_ERROR);

	unsigned int res;
	res = wxD->ShowModal();
	while(res != wxID_CANCEL)
	{
		//Dir cannot exist yet, as we want to make it.
		if(wxDirExists(wxD->GetPath() + wxT->GetValue()))
		{
			wxMesD->ShowModal();
			res=wxD->ShowModal();
		}
		else
			break;
	}
	wxMesD->Destroy();

	//User aborted directory choice. 
	if(res==wxID_CANCEL)
	{
		wxD->Destroy();
		wxT->Destroy();
		return;
	}

	wxString folder;
	folder=wxD->GetPath() + wxFileName::GetPathSeparator() + wxT->GetValue() +
			wxFileName::GetPathSeparator();
	//Check to see that the folder actually exists
	if(!wxMkdir(folder))
	{
		wxMessageDialog *wxMesD  =new wxMessageDialog(this,wxTRANS("Package folder creation failed\ncheck writing to this location is possible.")
						,wxTRANS("Folder creation failed"),wxOK|wxICON_ERROR);

		wxMesD->ShowModal();
		wxMesD->Destroy();

		return;
	}


	//OK, so the folder exists, lets make the XML state file
	std::string dataFile = string(stlStr(folder)) + "state.xml";

	std::map<string,string> fileMapping;
	//Try to save the viscontrol state
	if(!visControl.saveState(dataFile.c_str(),fileMapping,true))
	{
		std::string errString;
		errString=TRANS("Unable to save. Check output destination can be written to.");
		
		wxMessageDialog *wxD  =new wxMessageDialog(this,wxStr(errString)
						,wxTRANS("Save error"),wxOK|wxICON_ERROR);
		wxD->ShowModal();
		wxD->Destroy();
	}
	else
	{
		//Copy the files in the mapping
		wxProgressDialog *wxP = new wxProgressDialog(wxTRANS("Copying"),
					wxTRANS("Copying referenced files"),fileMapping.size());

		wxP->Show();
		for(map<string,string>::iterator it=fileMapping.begin();
				it!=fileMapping.end();++it)
		{
			if(!wxCopyFile(wxStr(it->second),folder+wxStr(it->first)))
			{
				wxMessageDialog *wxD  =new wxMessageDialog(this,wxTRANS("Error copying file")
								,wxTRANS("Save error"),wxOK|wxICON_ERROR);
				wxD->ShowModal();
				wxD->Destroy();
				wxP->Destroy();

				return;
			}
			wxP->Update();
		}

		wxP->Destroy();


		wxString s;
		s=wxString(wxTRANS("Saved package: ")) + folder;
		statusMessage(stlStr(s).c_str(),MESSAGE_INFO);
	}
	
	wxD->Destroy();
}

void MainWindowFrame::OnFileExportIons(wxCommandEvent &event)
{
	if(!treeFilters->GetCount())
	{
		statusMessage(TRANS("No filters means no data to export"),MESSAGE_ERROR);
		return;

	}
	//Load up the export dialog
	ExportPosDialog *exportDialog=new ExportPosDialog(this,wxID_ANY,wxTRANS("Export"));
	
	//create a file chooser for later.
	wxFileDialog *wxF = new wxFileDialog(this,wxTRANS("Save pos..."), wxT(""),
		wxT(""),wxTRANS("POS Data (*.pos)|*.pos|All Files (*)|*"),wxFD_SAVE);


	exportDialog->initialiseData(&visControl);
	
	//If the user cancels the file chooser, 
	//drop them back into the export dialog.
	do
	{
		//Show, then check for user cancelling export dialog
		if(exportDialog->ShowModal() == wxID_CANCEL)
		{
			exportDialog->cleanup(&visControl);
			exportDialog->Destroy();
			//Need this to reset the ID values
			visControl.updateWxTreeCtrl(treeFilters);
			visControl.setRefreshed();
			return;	
		}
		
	}
	while( (wxF->ShowModal() == wxID_CANCEL)); //Check for user abort in file chooser
	
	//Check file already exists (no overwrite without asking)
	if(wxFileExists(wxF->GetPath()))
	{
		wxMessageDialog *wxD  =new wxMessageDialog(this,wxTRANS("File already exists, overwrite?")
												   ,wxTRANS("Overwrite?"),wxOK|wxCANCEL|wxICON_QUESTION);
		if(wxD->ShowModal() == wxID_CANCEL)
		{
			wxD->Destroy();
			exportDialog->cleanup(&visControl);
			exportDialog->Destroy();
			//Need this to reset the ID values
			visControl.updateWxTreeCtrl(treeFilters);
			visControl.setRefreshed();
			return;
		}
	}
	
	std::string dataFile = stlStr(wxF->GetPath());
	
	//Retrieve the ion streams that we need to save
	vector<const FilterStreamData *> exportVec;
	exportDialog->getExportVec(exportVec);
	

	//write the ion streams to disk
	if(visControl.exportIonStreams(exportVec,dataFile))
	{
		std::string errString;
		errString=TRANS("Unable to save. Check output destination can be written to.");
		
		wxMessageDialog *wxD  =new wxMessageDialog(this,wxStr(errString)
						,wxTRANS("Save error"),wxOK|wxICON_ERROR);
		wxD->ShowModal();
		wxD->Destroy();
	}
	else
	{
		dataFile=std::string("Saved ions: ") + dataFile;
		statusMessage(dataFile.c_str(),MESSAGE_INFO);
	}
	
	exportDialog->cleanup(&visControl);

	//Call ->Destroy to invoke destructor, which will safely delete the
	//filterstream pointers it generated	
	exportDialog->Destroy();
	//Need this to reset the ID values
	visControl.updateWxTreeCtrl(treeFilters);
	visControl.setRefreshed();
	
}

void MainWindowFrame::OnFileExportRange(wxCommandEvent &event)
{

	if(!treeFilters->GetCount())
	{
		statusMessage(TRANS("No filters means no data to export"),
				MESSAGE_ERROR);
		return;
	}
	ExportRngDialog *rngDialog = new ExportRngDialog(this,wxID_ANY,wxTRANS("Export Ranges"),
							wxDefaultPosition,wxSize(600,400));

	vector<const Filter *> rangeData;
	//Retrieve all the range filters in the viscontrol
	visControl.getFilterTypes(rangeData,FILTER_TYPE_RANGEFILE);
	//pass this to the range dialog
	rngDialog->addRangeData(rangeData);

	if(rngDialog->ShowModal() == wxID_CANCEL)
	{
		rngDialog->Destroy();
		return;
	}



}


void MainWindowFrame::OnFileSaveAs(wxCommandEvent &event)
{
	//Show a file save dialog
	wxFileDialog *wxF = new wxFileDialog(this,wxTRANS("Save state..."), wxT(""),
		wxT(""),wxT("XML state file (*.xml)|*.xml|All Files (*)|*"),wxFD_SAVE);

	//Show, then check for user cancelling dialog
	if( (wxF->ShowModal() == wxID_CANCEL))
		return;

	std::string dataFile = stlStr(wxF->GetPath());

	wxFileName fname;
	wxString volume,path,name,ext;
	bool hasExt;
	fname.SplitPath(wxF->GetPath(),&volume,
			&path,&name,&ext, &hasExt);

	//Check file already exists (no overwrite without asking)
	if(wxFileExists(wxF->GetPath()))
	{
		wxMessageDialog *wxD  =new wxMessageDialog(this,wxTRANS("File already exists, overwrite?")
						,wxTRANS("Overwrite?"),wxOK|wxCANCEL|wxICON_QUESTION);
		if(wxD->ShowModal() == wxID_CANCEL)
		{
			wxD->Destroy();
			return;
		}
	}
	if(hasExt)
	{
		//Force the string to end in ".xml"	
		std::string strExt;
		strExt=stlStr(ext);
		strExt = lowercase(strExt);
		if(strExt != "xml")
			dataFile+=".xml";
	}
	else
		dataFile+=".xml";
	

	bool oldRelPath=visControl.usingRelPaths();
	//Check to see if we have are using relative paths,
	//and if so, do any of our filters
	if(visControl.usingRelPaths() && visControl.hasStateOverrides())
	{
		wxMessageDialog *wxD  =new wxMessageDialog(this,wxTRANS("Files have been referred to using relative paths. Keep relative paths?")
						,wxTRANS("Overwrite?"),wxYES|wxNO|wxICON_QUESTION);
	
		wxD->SetEscapeId(wxID_NO);
		wxD->SetAffirmativeId(wxID_YES);
		//Just for the moment, set relative paths to false, if the user asks.
		//we will restore this later
		if(wxD->ShowModal() == wxID_NO)
		{
			oldRelPath=true;
			visControl.setUseRelPaths(false);
		}

	}


	std::map<string,string> dummyMap;
	//Try to save the viscontrol state
	if(!visControl.saveState(dataFile.c_str(),dummyMap))
	{
		std::string errString;
		errString=TRANS("Unable to save. Check output destination can be written to.");
		
		wxMessageDialog *wxD  =new wxMessageDialog(this,wxStr(errString)
						,wxTRANS("Save error"),wxOK|wxICON_ERROR);
		wxD->ShowModal();
		wxD->Destroy();
	}
	else
	{
		currentFile = wxF->GetPath();
		fileSave->Enable(true);

		//Update the recent files, and the menu.
		configFile.addRecentFile(dataFile);
		recentHistory->AddFileToHistory(wxStr(dataFile));
	
		dataFile=std::string(TRANS("Saved state: ")) + dataFile;
		statusMessage(dataFile.c_str(),MESSAGE_INFO);
	}

	//Restore the relative path behaviour
	visControl.setUseRelPaths(oldRelPath);
}


void MainWindowFrame::OnFileExit(wxCommandEvent &event)
{
	//Close query is handled by OnClose()
	Close();
}

void MainWindowFrame::OnEditUndo(wxCommandEvent &event)
{
	visControl.popUndoStack();
	
	//Update tree control
	visControl.updateWxTreeCtrl(treeFilters);

	//Get the parent filter from the tree selection
	wxTreeItemId id=treeFilters->GetSelection();;
	if(id !=treeFilters->GetRootItem() && id.IsOk())
	{
		//Get the parent filter pointer	
		wxTreeItemData *parentData=treeFilters->GetItemData(id);
		//Update property grid	
		visControl.updateFilterPropGrid(gridFilterProperties,
						((wxTreeUint *)parentData)->value);

	}
	else
	{
		gridFilterProperties->clear();
		updateLastRefreshBox();
	}



	doSceneUpdate();
}

void MainWindowFrame::OnEditRedo(wxCommandEvent &event)
{
	visControl.popRedoStack();
	
	//Update tree control
	visControl.updateWxTreeCtrl(treeFilters);

	//Get the parent filter from the tree selection
	wxTreeItemId id=treeFilters->GetSelection();;
	if(id !=treeFilters->GetRootItem() && id.IsOk())
	{
		//Get the parent filter pointer	
		wxTreeItemData *parentData=treeFilters->GetItemData(id);
		//Update property grid	
		visControl.updateFilterPropGrid(gridFilterProperties,
						((wxTreeUint *)parentData)->value);

	}
	else
	{
		gridFilterProperties->clear();
		updateLastRefreshBox();
	}



	doSceneUpdate();
}

void MainWindowFrame::OnEditPreferences(wxCommandEvent &event)
{
	//Create  a new preference dialog
	PrefDialog *p = new PrefDialog(this);

	vector<Filter *> filterDefaults;

	//obtain direct copies of the cloned Filter pointers
	configFile.getFilterDefaults(filterDefaults);
	p->setFilterDefaults(filterDefaults);

	//Get the default mouse parameters
	unsigned int mouseZoomRate,mouseMoveRate;
	mouseZoomRate=configFile.getMouseZoomRate();
	mouseMoveRate=configFile.getMouseMoveRate();
	
	
	unsigned int panelMode;

	//Set Panel startup flags
	bool rawStartup,controlStartup,plotStartup;
	controlStartup=configFile.getPanelEnabled(CONFIG_STARTUPPANEL_CONTROL);
	rawStartup=configFile.getPanelEnabled(CONFIG_STARTUPPANEL_RAWDATA);
	plotStartup=configFile.getPanelEnabled(CONFIG_STARTUPPANEL_PLOTLIST);

	panelMode=configFile.getStartupPanelMode();

	p->setPanelDefaults(panelMode,controlStartup,rawStartup,plotStartup);

#ifndef DISABLE_ONLINE_UPDATE
	p->setAllowOnlineUpdate(configFile.getAllowOnlineVersionCheck());
#endif


	p->setMouseZoomRate(mouseZoomRate);
	p->setMouseMoveRate(mouseMoveRate);

	//Initialise panel
	p->initialise();
	//show panel
	if(p->ShowModal() !=wxID_OK)
	{
		p->cleanup();
		p->Destroy();
		return;
	}

	filterDefaults.clear();

	//obtain cloned copies of the pointers
	p->getFilterDefaults(filterDefaults);

	mouseZoomRate=p->getMouseZoomRate();
	mouseMoveRate=p->getMouseMoveRate();

	panelTop->setMouseZoomFactor((float)mouseZoomRate/100.0f);
	panelTop->setMouseMoveFactor((float)mouseMoveRate/100.0f);

	configFile.setMouseZoomRate(mouseZoomRate);
	configFile.setMouseMoveRate(mouseMoveRate);

	//Note that this transfers control of pointer to the config file 
	configFile.setFilterDefaults(filterDefaults);

	//Retrieve pane settings, and pass to config manager
	p->getPanelDefaults(panelMode,controlStartup,rawStartup,plotStartup);
	
	configFile.setPanelEnabled(CONFIG_STARTUPPANEL_CONTROL,controlStartup,true);
	configFile.setPanelEnabled(CONFIG_STARTUPPANEL_RAWDATA,rawStartup,true);
	configFile.setPanelEnabled(CONFIG_STARTUPPANEL_PLOTLIST,plotStartup,true);

	configFile.setStartupPanelMode(panelMode);

#ifndef DISABLE_ONLINE_UPDATE
	configFile.setAllowOnline(p->getAllowOnlineUpdate());
	configFile.setAllowOnlineVersionCheck(p->getAllowOnlineUpdate());
#endif
	p->cleanup();
	p->Destroy();
}

void MainWindowFrame::OnViewBackground(wxCommandEvent &event)
{

	//retrieve the current colour from the openGL panel	
	float r,g,b;
	panelTop->getGlClearColour(r,g,b);	
	//Show a wxColour choose dialog. 
	wxColourData d;
	d.SetColour(wxColour((unsigned char)(r*255),(unsigned char)(g*255),
	(unsigned char)(b*255),(unsigned char)(255)));
	wxColourDialog *colDg=new wxColourDialog(this->GetParent(),&d);

	if( colDg->ShowModal() == wxID_OK)
	{
		wxColour c;
		//Change the colour
		c=colDg->GetColourData().GetColour();
	
		//Scale colour ranges to 0-> 1 and set in the gl pane	
		panelTop->setGlClearColour(c.Red()/255.0f,c.Green()/255.0f,c.Blue()/255.0f);
	}
}

void MainWindowFrame::OnViewControlPane(wxCommandEvent &event)
{
	if(event.IsChecked())
	{
		if(!splitLeftRight->IsSplit())
		{
			const float SPLIT_FACTOR=0.3;
			int x,y;
			GetClientSize(&x,&y);
			splitLeftRight->SplitVertically(panelLeft,
						panelRight,(int)(SPLIT_FACTOR*x));
			configFile.setPanelEnabled(CONFIG_STARTUPPANEL_CONTROL,true);
	
		}
	}
	else
	{
		if(splitLeftRight->IsSplit())
		{
			splitLeftRight->Unsplit(panelLeft);
			configFile.setPanelEnabled(CONFIG_STARTUPPANEL_CONTROL,false);
		}
	}
}


void MainWindowFrame::OnViewRawDataPane(wxCommandEvent &event)
{
	if(event.IsChecked())
	{
		if(!splitTopBottom->IsSplit())
		{
			const float SPLIT_FACTOR=0.3;

			int x,y;
			GetClientSize(&x,&y);
			splitTopBottom->SplitHorizontally(panelTop,
						noteDataView,(int)(SPLIT_FACTOR*x));
	
			configFile.setPanelEnabled(CONFIG_STARTUPPANEL_RAWDATA,true);
		}
	}
	else
	{
		if(splitTopBottom->IsSplit())
		{
			splitTopBottom->Unsplit();
			configFile.setPanelEnabled(CONFIG_STARTUPPANEL_RAWDATA,false);
		}
	}
}

void MainWindowFrame::OnViewSpectraList(wxCommandEvent &event)
{
	if(event.IsChecked())
	{
		if(!splitterSpectra->IsSplit())
		{
			const float SPLIT_FACTOR=0.6;

			int x,y;
			splitterSpectra->GetClientSize(&x,&y);
			splitterSpectra->SplitVertically(panelSpectra,
						window_2_pane_2,(int)(SPLIT_FACTOR*x));
	
			configFile.setPanelEnabled(CONFIG_STARTUPPANEL_PLOTLIST,true);
		}
	}
	else
	{
		if(splitterSpectra->IsSplit())
		{
			splitterSpectra->Unsplit();
			configFile.setPanelEnabled(CONFIG_STARTUPPANEL_PLOTLIST,false);
		}
	}
}

void MainWindowFrame::OnViewPlotLegend(wxCommandEvent &event)
{
	panelSpectra->setLegendVisible(event.IsChecked());
	panelSpectra->Refresh();
}

void MainWindowFrame::OnViewWorldAxis(wxCommandEvent &event)
{
	panelTop->currentScene.setWorldAxisVisible(event.IsChecked());
	panelTop->Refresh();
}

void MainWindowFrame::OnHelpHelp(wxCommandEvent &event)
{
	//First attempt to locate the local copy of the manual.
	string s;
	s=locateDataFile("3Depict-manual.pdf");

	//Also Debian makes us use the lowercase "D", so check there too.
	if(!s.size())
		s=locateDataFile("3depict-manual.pdf");


	//If we found it, use the default program associated with that data file
	bool launchedOK=false;
	if( wxFileExists(wxStr(s))  && s.size())
	{
		//we found the manual. Launch the default handler.
#if (wxMAJOR_VERSION >= 2) && (wxMINOR_VERSION > 8)
		launchedOK=wxLaunchDefaultApplication(wxStr(s));
#else
		//its a bit more convoluted for earlier versions of wx.
		//we have to try xdg-open or open for Linux and mac respectively
		//for windows, we need to use the wxWidgets GetOpenCommand
	
		long appPID;

	#if defined(__linux__)
		//Try xdg-open first
		wxString str;
		str= wxT("xdg-open ");
		str+=wxStr(s);
		appPID=wxExecute(str,wxEXEC_ASYNC);
		launchedOK=(appPID!=0);
	#elif defined(__APPLE__)
		//Try open first
		wxString str;
		str= wxT("open ");
		str+=wxStr(s);
		appPID=wxExecute(str,wxEXEC_ASYNC);
		launchedOK=(appPID!=0);
	#endif

		//No luck still? Try wx's quirky GetOpenCommand		
		if(!launchedOK)
		{
			wxString command;
			//Sigh. In version < 2.9; wx uses the mime-type
			//manager to wrap up the construction
			//of wxFileType object (private constructor).
			//so we can't just *make* a wxFileType
			//we have to derive one from a "mime-type manager".
			//the only way to do this is from the file extension
			//or by passing the mime-string.
			wxMimeTypesManager m;
			wxFileType *t;
				
			t=m.GetFileTypeFromExtension(wxT("pdf"));
			command=t->GetOpenCommand(wxStr(s));
			appPID=wxExecute(command,wxEXEC_ASYNC);
			launchedOK=(appPID!=0);
		}
#endif
	}

	//Still no go? Give up and launch a browser.
	if(!launchedOK)
	{
		std::string helpFileLocation("http://threedepict.sourceforge.net/documentation.html");
		wxLaunchDefaultBrowser(wxStr(helpFileLocation),wxBROWSER_NEW_WINDOW);

		statusMessage(TRANS("Manual not found locally. Launching web browser"),MESSAGE_INFO);
	}
}

void MainWindowFrame::OnHelpContact(wxCommandEvent &event)
{
	std::string contactFileLocation("http://threedepict.sourceforge.net/contact.html");
	wxLaunchDefaultBrowser(wxStr(contactFileLocation),wxBROWSER_NEW_WINDOW);

	statusMessage(TRANS("Opening contact page in external web browser"),MESSAGE_INFO);
}

void MainWindowFrame::OnButtonStashDialog(wxCommandEvent &event)
{
	std::vector<std::pair<std::string,unsigned int > > stashList;
	visControl.getStashList(stashList);

	ASSERT(comboStash->GetCount() == stashList.size())

	if(stashList.empty())
	{
		statusMessage(TRANS("No filter stashes to edit."),MESSAGE_ERROR);
		return;
	}

	StashDialog *s = new StashDialog(this,wxID_ANY,wxTRANS("Filter Stashes"));
	s->setVisController(&visControl);
	s->ready();
	s->ShowModal();

	s->Destroy();

	//Stash list may have changed. Force update
	stashList.clear();
	visControl.getStashList(stashList);

	comboStash->Clear();
	for(unsigned int ui=0;ui<stashList.size(); ui++)
	{
		wxListUint *u;
		u = new wxListUint(stashList[ui].second);
		comboStash->Append(wxStr(stashList[ui].first),(wxClientData *)u);
		ASSERT(comboStash->wxItemContainer::GetClientObject(comboStash->GetCount()-1));
	}

	
}


void MainWindowFrame::OnHelpAbout(wxCommandEvent &event)
{
	wxAboutDialogInfo info;
	info.SetName(wxCStr(PROGRAM_NAME));
	info.SetVersion(wxCStr(PROGRAM_VERSION));
	info.SetDescription(wxTRANS("Quick and dirty analysis for point data.")); 
	info.SetWebSite(wxT("https://sourceforge.net/apps/phpbb/threedepict/"));

	info.AddDeveloper(wxT("D. Haley"));	
	info.AddDeveloper(wxT("A. Ceguerra"));	
	//GNU GPL v3
	info.SetCopyright(_T("Copyright (C) 2011 3Depict team\n This software is licenced under the GPL Version 3.0 or later\n This program comes with ABSOLUTELY NO WARRANTY.\nThis is free software, and you are welcome to redistribute it\nunder certain conditions; Please see the file COPYING in the program directory for details"));	

	info.AddArtist(_T("Thanks go to all who have developed the libraries that I use, which make this program possible.\n This includes the wxWidgets team, Alexy Balakin (MathGL), the FTGL and freetype people, the GNU Scientific Library contributors, the tree.h guy (Kasper Peeters)  and more."));

	info.AddArtist(wxString(wxTRANS("Compiled with wx Version: " )) + 
			wxString(wxSTRINGIZE_T(wxVERSION_STRING)));

	wxArrayString s;
	s.Add(_T("Deutsch (German) : Erich (de)"));
	info.SetTranslators(s);

	wxAboutBox(info);
}


void MainWindowFrame::OnComboStashText(wxCommandEvent &event)
{
	std::string s;
	s=stlStr(comboStash->GetValue());
	if(!s.size())
		return;
	
	int n = comboStash->FindString(comboStash->GetValue());
	
	if ( n== wxNOT_FOUND ) 
		statusMessage(TRANS("Press enter to store new stash"),MESSAGE_HINT);
	else
	{
		//The combo generates an ontext event when a string
		//is selected (yeah, I know, weird..) Block this case.
		if(comboStash->GetSelection() != n)
			statusMessage(TRANS("Press enter to restore stash"),MESSAGE_HINT);
	}
}

void MainWindowFrame::OnComboStashEnter(wxCommandEvent &event)
{
	//The user has pressed enter, in the combo box. If there is an existing stash of this name,
	//use it. Otherwise store the current tree control as part of the new stash
	std::string userText;

	userText=stlStr(comboStash->GetValue());

	//Forbid names with no text content
	userText=stripWhite(userText);
	if(!userText.size())
		return;

	std::vector<std::pair<std::string,unsigned int > > stashList;
	visControl.getStashList(stashList);

	unsigned int stashPos = (unsigned int ) -1;
	for(unsigned int ui=0;ui<stashList.size(); ui++)
	{
		if(stashList[ui].first == userText)
		{
			stashPos=ui;
			break;
		}
	}

	if(stashPos == (unsigned int) -1)
	{
		//We didn't find it!. This must be a new stash
		wxTreeItemId id;
		id=treeFilters->GetSelection();
		//If there is a problem with the selection, abort
		if(!id.IsOk() || (id == treeFilters->GetRootItem()))
		{
			statusMessage(TRANS("Unable to create stash, selection invalid"),MESSAGE_ERROR);
			return;
		}

		
		//Tree data contains unique identifier for vis control to do matching
		wxTreeItemData *tData=treeFilters->GetItemData(id);

		unsigned int n =visControl.stashFilters(((wxTreeUint *)tData)->value,userText.c_str());
		n=comboStash->Append(wxStr(userText),(wxClientData *)new wxListUint(n));
		ASSERT(comboStash->wxItemContainer::GetClientObject(n));
		
		statusMessage(TRANS("Created new filter tree stash"),MESSAGE_INFO);

	}
	else
	{
		//Found it. Restore the existing stash
		//Find the stash associated with this item
		int index;
		index= comboStash->FindString(comboStash->GetValue());
		wxListUint *l;
		l =(wxListUint*)comboStash->wxItemContainer::GetClientObject(index);
		//Get the parent filter from the tree selection
		wxTreeItemId id=treeFilters->GetSelection();;
		if(id !=treeFilters->GetRootItem() && id.IsOk())
		{
			//Get the parent filter pointer	
			wxTreeItemData *parentData=treeFilters->GetItemData(id);
			const Filter *parentFilter=(const Filter *)visControl.getFilterById(
						((wxTreeUint *)parentData)->value);
		
			visControl.addStashedToFilters(parentFilter,l->value);
			
			visControl.updateWxTreeCtrl(treeFilters,
							parentFilter);

			statusMessage("",MESSAGE_NONE);
			if(checkAutoUpdate->GetValue())
				doSceneUpdate();	

		}
	
		//clear the text in the combo box
		comboStash->SetValue(wxT(""));
	}

	//clear the text in the combo box
	comboStash->SetValue(wxT(""));
}


void MainWindowFrame::OnComboStash(wxCommandEvent &event)
{
	//Find the stash associated with this item
	wxListUint *l;
	l =(wxListUint*)comboStash->wxItemContainer::GetClientObject(comboStash->GetSelection());
	//Get the parent filter from the tree selection
	wxTreeItemId id=treeFilters->GetSelection();;
	if(id !=treeFilters->GetRootItem() && id.IsOk())
	{
		//Get the parent filter pointer	
		wxTreeItemData *parentData=treeFilters->GetItemData(id);
		const Filter *parentFilter=(const Filter *)visControl.getFilterById(
					((wxTreeUint *)parentData)->value);
	
		visControl.addStashedToFilters(parentFilter,l->value);
		
		visControl.updateWxTreeCtrl(treeFilters,
						parentFilter);

		if(checkAutoUpdate->GetValue())
			doSceneUpdate();	

	}
	
	//clear the text in the combo box
	comboStash->SetValue(wxT(""));
}



void MainWindowFrame::OnTreeEndDrag(wxTreeEvent &event)
{
	//Should be enforced by ::Allow() in start drag.
	ASSERT(filterTreeDragSource && filterTreeDragSource->IsOk()); 
	//Allow tree to be manhandled, so you can move filters around
	int testType=wxTREE_HITTEST_ONITEMLABEL;
	wxTreeItemId newParent = treeFilters->HitTest(event.GetPoint(),
						testType);

	bool needRefresh=false;
	size_t sId;
	wxTreeItemData *sourceDat=treeFilters->GetItemData(*filterTreeDragSource);
	sId = ((wxTreeUint *)sourceDat)->value;

	wxMouseState wxm = wxGetMouseState();

	//if we have a parent node to reparent this to	
	if(newParent.IsOk())
	{
		size_t pId;
		wxTreeItemData *parentDat=treeFilters->GetItemData(newParent);
		pId = ((wxTreeUint *)parentDat)->value;
		//Copy elements from a to b, if a and b are not the same
		if(pId != sId)
		{
			//If command button down (ctrl or clover on mac),
			//then copy, otherwise move
			if(wxm.CmdDown())
				needRefresh=visControl.copyFilter(sId,pId);
			else
				needRefresh=visControl.reparentFilter(sId,pId);
		}	
	}
	else
	{
		wxTreeItemId parent = treeFilters->GetItemParent(*filterTreeDragSource);
		if(parent ==treeFilters->GetRootItem() && wxm.CmdDown())
		{
			//OK, so this is a base item, and is therefore a data source
			//allow duplication
			needRefresh=visControl.copyFilter(sId,0,true);
		}
	}

	if(needRefresh )
	{
		//Refresh the treecontrol
		visControl.updateWxTreeCtrl(treeFilters,
				visControl.getFilterById(sId));

		//We have finished the drag	
		statusMessage("",MESSAGE_NONE);
		if(checkAutoUpdate->GetValue())
			doSceneUpdate();	
	}
	delete filterTreeDragSource;
	filterTreeDragSource=0;	
}


void MainWindowFrame::OnTreeItemTooltip(wxTreeEvent &event)
{
    event.Skip();
    wxLogDebug(wxT("Event handler (MainWindowFrame::OnTreeItemTooltip) not implemented yet")); //notify the user that he hasn't implemented the event handler yet
}



void MainWindowFrame::OnTreeSelectionChange(wxTreeEvent &event)
{
	if(currentlyUpdatingScene)
	{
		event.Veto();
		return;
	}
	wxTreeItemId id;
	id=treeFilters->GetSelection();
	//Tree data contains unique identifier for vis control to do matching
	wxTreeItemData *tData=treeFilters->GetItemData(id);

	visControl.updateFilterPropGrid(gridFilterProperties,
					((wxTreeUint *)tData)->value);


	updateLastRefreshBox();
	

	treeFilters->Fit();	
	panelTop->Refresh();

}


void MainWindowFrame::updateLastRefreshBox()
{
	wxTreeItemId id;
	id=treeFilters->GetSelection();
	if(id.IsOk() && id != treeFilters->GetRootItem())
	{
		//Tree data contains unique identifier for vis control to do matching
		wxTreeItemData *tData=treeFilters->GetItemData(id);
		//retrieve the current active filter
		const Filter *f= visControl.getFilterById(((wxTreeUint *)tData)->value);
		
		//Prevent update flicker by disabling interaction
		listLastRefresh->Freeze();

		listLastRefresh->DeleteAllItems();
		for(unsigned int ui=0;ui<NUM_STREAM_TYPES; ui++)
		{
			//Add items to the listbox in the form "type" "count"
			//if there is a nonzero number of items for that type
			std::string n;
			unsigned int numOut;
			numOut=f->getNumOutput(ui);
			if(numOut)
			{
				long index;
				stream_cast(n,numOut);
				index=listLastRefresh->InsertItem(0,wxCStr(TRANS(STREAM_NAMES[ui])));
				listLastRefresh->SetItem(index,1,wxStr(n));
			}
		}
		listLastRefresh->Thaw();
	}
}

void MainWindowFrame::OnTreeDeleteItem(wxTreeEvent &event)
{
	//This event is only generated programatically, 
	//Currently it *purposely* does nothing
}

void MainWindowFrame::OnTreeEndLabelEdit(wxTreeEvent &event)
{
	if(event.IsEditCancelled())
	{
		treeFilters->Fit();
		return;
	}

	wxTreeItemId id;
	id=treeFilters->GetSelection();

	wxTreeItemData *selItemDat=treeFilters->GetItemData(id);


	//There is a case where the tree doesn't quite clear
	//when there is an editor involved.
	if(visControl.numFilters())
	{
		std::string s;
		s=stlStr(event.GetLabel());
		if(s.size())
		{
	
			//If the string has been changed, then we need to update	
			if(visControl.setFilterString(((wxTreeUint *)selItemDat)->value,s))
			{
				//We need to reupdate the scene, in order to re-fill the 
				//spectra list box
				doSceneUpdate();
			}
		}
		else
		{
			event.Veto(); // Disallow blank strings.
		}
	}

	treeFilters->Fit();
}

void MainWindowFrame::OnTreeBeginDrag(wxTreeEvent &event)
{
	if(treeFilters->GetEditControl() )
	{
		//No dragging if editing
		return;
	}
	int testType=wxTREE_HITTEST_ONITEMLABEL;
	//Record the drag source
	wxTreeItemId t = treeFilters->HitTest(event.GetPoint(),testType);

	if(t.IsOk())
	{
		filterTreeDragSource = new wxTreeItemId;
		*filterTreeDragSource =t;
		event.Allow();

#ifdef __APPLE__    
		statusMessage(TRANS("Moving - Hold ⌘ (command) to copy"),MESSAGE_HINT);
#else
		statusMessage(TRANS("Moving - Hold control to copy"),MESSAGE_HINT);
#endif
	}

}

void MainWindowFrame::OnBtnExpandTree(wxCommandEvent &event)
{
	treeFilters->ExpandAll();
}


void MainWindowFrame::OnBtnCollapseTree(wxCommandEvent &event)
{
	treeFilters->CollapseAll();
}

void MainWindowFrame::OnTreeKeyDown(wxTreeEvent &event)
{
	if(currentlyUpdatingScene)
	{
		event.Veto();
		return;
	}
	const wxKeyEvent k = event.GetKeyEvent();
	switch(k.GetKeyCode())
	{
		case WXK_BACK:
		case WXK_DELETE:
		{
			wxTreeItemId id;

			if(!treeFilters->GetCount())
				return;

			id=treeFilters->GetSelection();

			if(!id.IsOk() || id == treeFilters->GetRootItem())
				return;

			//Tree data contains unique identifier for vis control to do matching
			wxTreeItemData *tData=treeFilters->GetItemData(id);


			//Remove the item from the Tree 
			visControl.removeTreeFilter(((wxTreeUint *)tData)->value);
			//Clear property grid
			gridFilterProperties->clear();
		
			//Rebuild the tree control, ensuring that the parent is visible,
			//if it has a parent (recall root node  of wx control is hidden)
			
			//Get the parent
			wxTreeItemId parent = treeFilters->GetItemParent(id);
			if(parent !=treeFilters->GetRootItem())
			{
				//Get the parent filter pointer	
				wxTreeItemData *parentData=treeFilters->GetItemData(parent);
				const Filter *parentFilter=visControl.getFilterById(
							((wxTreeUint *)parentData)->value);

				//Note, you *must* update the tree after the grid if you
				//wish to dereference the treeItem, otherwise the treeitem
				//referred to by the pointer is destroyed, and invalid
				visControl.updateFilterPropGrid(gridFilterProperties,
								((wxTreeUint *)parentData)->value);
				visControl.updateWxTreeCtrl(treeFilters,parentFilter);
			}
			else
			{
				if(parent.IsOk())
					visControl.updateWxTreeCtrl(treeFilters);
			}
	
			//Force a scene update, independent of if autoUpdate is enabled. 
			doSceneUpdate();	

			break;
		}

		default:
			event.Skip();
	}

	
	refreshButton->Enable(visControl.numFilters());

	editUndoMenuItem->Enable(visControl.getUndoSize());
	editRedoMenuItem->Enable(visControl.getRedoSize());
}

void MainWindowFrame::OnGridFilterPropertyChange(wxGridEvent &event)
{

	if(programmaticEvent || currentlyUpdatingScene)
	{
		event.Veto();
		return;
	}

	programmaticEvent=true;
	//Should only be in the second col
	ASSERT(event.GetCol()==1);

	std::string value; 
	value = stlStr(gridFilterProperties->GetCellValue(
					event.GetRow(),1));

	//Get the filter ID value (long song and dance that it is)
	wxTreeItemId tId = treeFilters->GetSelection();

	if(!tId.IsOk())
	{
		programmaticEvent=false;
		return;
	}

	size_t filterId;
	wxTreeItemData *tData=treeFilters->GetItemData(tId);
	filterId = ((wxTreeUint *)tData)->value;

	bool needUpdate;
	int row=event.GetRow();
	if(!visControl.setFilterProperties(filterId,gridFilterProperties->getSetFromRow(row),
					gridFilterProperties->getKeyFromRow(row),value,needUpdate))
	{
		event.Veto();
		programmaticEvent=false;
		return;
	}


	if(needUpdate && checkAutoUpdate->GetValue())
		doSceneUpdate();
	visControl.updateFilterPropGrid(gridFilterProperties,filterId);
	
	editUndoMenuItem->Enable(visControl.getUndoSize());
	editRedoMenuItem->Enable(visControl.getRedoSize());
	Layout();
	programmaticEvent=false;
}

void MainWindowFrame::OnGridCameraPropertyChange(wxGridEvent &event)
{

	if(programmaticEvent)
	{
		event.Veto();
		return;
	}

	programmaticEvent=true;
	//Should only be in the second col
	ASSERT(event.GetCol()==1);

	std::string value; 
	value = stlStr(gridCameraProperties->GetCellValue(
					event.GetRow(),1));

	//Get the camera ID value (long song and dance that it is)
	wxListUint *l;
	int n = comboCamera->FindString(comboCamera->GetValue());
	l =(wxListUint*)  comboCamera->wxItemContainer::GetClientObject(n);
	if(!l)
	{
		programmaticEvent=false;
		return;
	}

	size_t cameraId;
	cameraId = l->value;

	int row=event.GetRow();
	if(visControl.setCamProperties(cameraId,gridCameraProperties->getKeyFromRow(row),value))
		visControl.updateCamPropertyGrid(gridCameraProperties,cameraId);
	else
		event.Veto();

	panelTop->Refresh(true);
	programmaticEvent=false;


}

void MainWindowFrame::OnCameraGridCellEditorHide(wxGridEvent &e)
{
	//Unlock the camera combo, now that we have finished editing
	comboCamera->Enable(true);
}

void MainWindowFrame::OnComboCameraText(wxCommandEvent &event)
{
	std::string s;
	s=stlStr(comboCamera->GetValue());
	if(!s.size())
		return;
	
	int n = comboCamera->FindString(comboCamera->GetValue());
	
	if ( n== wxNOT_FOUND ) 
		statusMessage(TRANS("Press enter to store new camera"),MESSAGE_HINT);
	else
		statusMessage(TRANS("Press enter to restore camera"),MESSAGE_HINT);
}

void MainWindowFrame::OnComboCameraEnter(wxCommandEvent &event)
{
	std::string camName;
	camName=stlStr(comboCamera->GetValue());

	//Disallow cameras with no name
	if (!camName.size())
		return;

	//Search for the camera's position in the combo box
	int n = comboCamera->FindString(comboCamera->GetValue());

	//If we have found the camera...
	if ( n!= wxNOT_FOUND ) 
	{
		//Select the combo box item
		comboCamera->Select(n);
		//Set this camera as thew new camera
		wxListUint *l;
		l =(wxListUint*)  comboCamera->wxItemContainer::GetClientObject(comboCamera->GetSelection());
		visControl.setCam(l->value);
		
		std::string s = std::string(TRANS("Restored camera: ") ) +stlStr(comboCamera->GetValue());	
		
		statusMessage(s.c_str(),MESSAGE_INFO);
		
		//refresh the camera property grid
		visControl.updateCamPropertyGrid(gridCameraProperties ,l->value);

		//force redraw in 3D pane
		panelTop->Refresh(false);
		return ;
	}

	//Create a new camera for the scene.
	unsigned int u=visControl.addCam(camName);

	//Do not delete as this will be deleted by wx.
	comboCamera->Append(comboCamera->GetValue(),(wxClientData *)new wxListUint(u));	

	std::string s = std::string(TRANS("Stored camera: " )) +stlStr(comboCamera->GetValue());	
	statusMessage(s.c_str(),MESSAGE_INFO);

	visControl.setCam(u);
	visControl.updateCamPropertyGrid(gridCameraProperties,u);
	panelTop->Refresh(false);
}

void MainWindowFrame::OnComboCamera(wxCommandEvent &event)
{
	//Set the active camera
	wxListUint *l;
	l =(wxListUint*)  comboCamera->wxItemContainer::GetClientObject(comboCamera->GetSelection());
	visControl.setCam(l->value);



	visControl.updateCamPropertyGrid(gridCameraProperties,l->value);

	std::string s = std::string(TRANS("Restored camera: ") ) +stlStr(comboCamera->GetValue());	
	statusMessage(s.c_str(),MESSAGE_INFO);
	
	panelTop->Refresh(false);
	return ;
}

void MainWindowFrame::OnComboCameraSetFocus(wxFocusEvent &event)
{
	
	if(!haveSetComboCamText)
	{
		comboCamera->SetValue(wxT(""));
		haveSetComboCamText=true;
		event.Skip();
		return;
	}
	
	if(comboCamera->GetCount())
	{

		wxListUint *l;
		int n = comboCamera->FindString(comboCamera->GetValue());
		l =(wxListUint*)  comboCamera->wxItemContainer::GetClientObject(n);
		if(l)
			visControl.updateCamPropertyGrid(gridCameraProperties,l->value);
	}
	event.Skip();
}

void MainWindowFrame::OnComboStashSetFocus(wxFocusEvent &event)
{
	if(!haveSetComboStashText)
	{
		comboStash->SetValue(wxT(""));
		haveSetComboStashText=true;
		event.Skip();
		return;
	}
	event.Skip();
}

void MainWindowFrame::OnComboFilterEnter(wxCommandEvent &event)
{
	if(currentlyUpdatingScene)
		return;
	
	OnComboFilter(event);
}

void MainWindowFrame::OnComboFilter(wxCommandEvent &event)
{
	if(currentlyUpdatingScene)
		return;
	
	wxTreeItemId id;
	
	id=treeFilters->GetSelection();

	//Check that there actually was a selection (a bit backwards, but thats the way of it)
	if(!id.IsOk() || id == treeFilters->GetRootItem())
	{
		if(treeFilters->GetCount())
			statusMessage(TRANS("Select an item from the filter tree before choosing a new filter"));
		else
			statusMessage(TRANS("Load data source (file->open) before choosing a new filter"));
		return;
	}

	//Perform the appropriate action for the particular filter,
	//or use the default action for every other filter
	bool haveErr=false;


	//Convert the string into a filter ID based upon our mapping
	wxString s;
	s=comboFilters->GetString(event.GetSelection());
	size_t filterType;
	filterType=filterMap[stlStr(s)];

	ASSERT(stlStr(s) == TRANS(comboFilters_choices[filterType]));
	switch(comboFiltersTypeMapping[filterType])
	{
		case FILTER_TYPE_RANGEFILE:
		{
			///Prompt user for file
			wxFileDialog *wxF = new wxFileDialog(this,wxTRANS("Select RNG File..."),wxT(""),wxT(""),
					wxTRANS("Range Files (*rng; *env; *rrng)|*rng;*env;*rrng|RNG File (*.rng)|*.rng|Environment File (*.env)|*.env|RRNG Files (*.rrng)|*.rrng|All Files (*)|*"),wxFD_OPEN|wxFD_FILE_MUST_EXIST);
			

			if( (wxF->ShowModal() == wxID_CANCEL))
			{
				haveErr=true;
				break;
			}

			//Load rangefile &  construct filter
			RangeFileFilter *f=(RangeFileFilter*)configFile.getDefaultFilter(FILTER_TYPE_RANGEFILE);

			//TODO: Delete me, the logic is already in aptclasses.h
			//-------
			//Split the filename into chunks: path, volume, name and extension
			//the format of this is OS dependant, but wxWidgets can deal with this.
			wxFileName fname;
			wxString volume,path,name,ext;
			bool hasExt;
			fname.SplitPath(wxF->GetPath(),&volume,
					&path,&name,&ext, &hasExt);
			
			std::string strExt;
			strExt=stlStr(ext);
			strExt = lowercase(strExt);
			if(strExt == "env")
				f->setFormat(RANGE_FORMAT_ENV);
			else if(strExt == "rng")
			       f->setFormat(RANGE_FORMAT_ORNL);	
			else if(strExt == "rrng")
			       f->setFormat(RANGE_FORMAT_RRNG);	
			else
			{
				statusMessage(TRANS("Unknown file extension"),MESSAGE_ERROR);
				return;
			}
				

			std::string dataFile = stlStr(wxF->GetPath());
			f->setRangeFileName(dataFile);
			//-------


			unsigned int errCode;
			errCode = f->updateRng();
			std::string  errString;
			switch(errCode)
			{
				case 0:
					break;
				default:
				{
					errString = TRANS("Failed reading range file, may not readable or may be invalid: ");
					errString += "\n";
					errString+=dataFile;
					
				}
			}

			if(errCode)
			{
				wxMessageDialog *wxD  =new wxMessageDialog(this,
					wxStr(errString),wxTRANS("Error loading file"),wxOK|wxICON_ERROR);
				wxD->ShowModal();
				wxD->Destroy();
				delete f;
				haveErr=true;;
				break;
			}
			
			//Tree data contains unique identifier for vis control to do matching
			wxTreeItemData *tData=treeFilters->GetItemData(id);

			ASSERT(tData);
			visControl.addFilter(f,((wxTreeUint *)tData)->value);

			//Rebuild tree control
			visControl.updateWxTreeCtrl(treeFilters,f);
			break;
		}
		default:
		{
			Filter *t;
		
			ASSERT(filterType < FILTER_TYPE_ENUM_END);
			//Generate the appropriate filter
			t=configFile.getDefaultFilter(comboFiltersTypeMapping[filterType]);
			//Tree data contains unique identifier for vis control to do matching
			wxTreeItemData *tData=treeFilters->GetItemData(id);
			
			ASSERT(tData);
			//Add the filter to viscontrol
			visControl.addFilter(t,((wxTreeUint *)tData)->value);

			//Rebuild tree control
			visControl.updateWxTreeCtrl(treeFilters,t);
		}
	
	}

	if(haveErr)
	{
		//Clear the combo box
		comboFilters->SetValue(wxT(""));
		return;
	}

	//Rebuild tree control
	refreshButton->Enable(visControl.numFilters());

	if(checkAutoUpdate->GetValue())
		doSceneUpdate();

	comboFilters->SetValue(wxT(""));
	
	editUndoMenuItem->Enable(visControl.getUndoSize());
	editRedoMenuItem->Enable(visControl.getRedoSize());
}

bool MainWindowFrame::doSceneUpdate()
{
	//Update scene
	
	//Suspend the update timer, and start the progress timer
	updateTimer->Stop();
	progressTimer->Start(PROGRESS_TIMER_DELAY);		
	currentlyUpdatingScene=true;
	haveAborted=false;

		
	statusMessage("",MESSAGE_NONE);
	noteDataView->SetPageText(NOTE_CONSOLE_PAGE_OFFSET,wxTRANS("Cons."));

	//Disable tree filters,refresh button and undo
	treeFilters->Enable(false);
	comboFilters->Enable(false);
	refreshButton->Enable(false);
	editUndoMenuItem->Enable(false);
	editRedoMenuItem->Enable(false);
	gridFilterProperties->Enable(false);
	comboStash->Enable(false);

	panelSpectra->limitInteraction();
	panelSpectra->Refresh();


	if(!requireFirstUpdate)
		textConsoleOut->Clear();	

	//Set focus on the main frame itself, so that we can catch escape key presses
	SetFocus();

	unsigned int errCode=visControl.updateScene();

	progressTimer->Stop();
	updateTimer->Start(UPDATE_TIMER_DELAY);

	//If there was an error, then
	//display it	
	if(errCode)
	{
		ProgressData p;
		p=visControl.getProgress();

		statusTimer->Start(STATUS_TIMER_DELAY,wxTIMER_ONE_SHOT);
		std::string errString;
		if(p.curFilter)
			errString = p.curFilter->getErrString(errCode);
		else
			errString = TRANS("Refresh Aborted.");
	
		statusMessage(errString.c_str(),MESSAGE_ERROR);	
	}

	//Call the progress one more time, in order to ensure that user sees "100%"
	if(!errCode)
		updateProgressStatus();
	
	currentlyUpdatingScene=false;
	visControl.resetProgress();

	//Restore the UI elements to their interactive state
	panelSpectra->limitInteraction(false);
	treeFilters->Enable(true);
	refreshButton->Enable(true);
	gridFilterProperties->Enable(true);
	comboFilters->Enable(true);
	comboStash->Enable(true);
	editUndoMenuItem->Enable(visControl.getUndoSize());
	editRedoMenuItem->Enable(visControl.getRedoSize());
	
	panelTop->Refresh(false);
	panelSpectra->Refresh(false);	

	updateLastRefreshBox();


	//Add (or hide) a little "Star" to inform the user there is some info available
	if(textConsoleOut->IsEmpty() || noteDataView->GetSelection()==NOTE_CONSOLE_PAGE_OFFSET)
		noteDataView->SetPageText(NOTE_CONSOLE_PAGE_OFFSET,wxTRANS("Cons."));
	else
		noteDataView->SetPageText(NOTE_CONSOLE_PAGE_OFFSET,wxTRANS("§Cons."));


	//Return a value dependant upon whether we successfully loaded 
	//the data or not
	return errCode == 0;
}

void MainWindowFrame::OnStatusBarTimer(wxTimerEvent &event)
{
	for(unsigned int ui=0; ui<3; ui++)
	{
		MainFrame_statusbar->SetBackgroundColour(wxNullColour);
		MainFrame_statusbar->SetStatusText(wxT(""),ui);
	}
	
	
	//Stop the status timer, just in case
	statusTimer->Stop();
}

void MainWindowFrame::OnProgressTimer(wxTimerEvent &event)
{
	updateProgressStatus();
}

void MainWindowFrame::OnAutosaveTimer(wxTimerEvent &event)
{
	//Save a state file to the configuration dir
	//with the title "autosave.xml"
	//

	wxString filePath = wxStandardPaths::Get().GetDocumentsDir()
					+wxCStr("/.")+wxCStr(PROGRAM_NAME);


	//create the dir
	if(!wxDirExists(filePath))
	{
		if(wxMkdir(filePath))
		{
			//Under windows, lets hide this folder
#if defined(__WIN32) || defined(__WIN64)
			string s;
			s=stlStr(filePath);
			SetFileAttributes(s.c_str(),FILE_ATTRIBUTE_HIDDEN);
#endif
		}
		else
		{
			//Creating the folder failed, for whatever reason.
			// just give up.
			return;
		}

	}

	unsigned int pid;
	pid = wxGetProcessId();;

	std::string pidStr;
	stream_cast(pidStr,pid);

	filePath+=wxCStr("/") + wxCStr(AUTOSAVE_PREFIX) + wxStr(pidStr) +
				wxCStr(AUTOSAVE_SUFFIX);
	//Save to the autosave file
	std::string s;
	s=  stlStr(filePath);
	std::map<string,string> dummyMap;
	if(visControl.saveState(s.c_str(),dummyMap))
		statusMessage(TRANS("Autosave complete."),MESSAGE_INFO);


}

void MainWindowFrame::OnUpdateTimer(wxTimerEvent &event)
{
	timerEvent=true;

	//TODO: HACK AROUND: force tree filter to relayout under wxGTK and Mac
	#ifndef __WXMSW__
	//Note: Calling this under windows causes the dropdown box that hovers over the top of this to
	//be closed, rendering the dropdown useless. That took ages to work out.
	treeFilters->GetParent()->Layout();
	#endif

	if(requireFirstUpdate)
	{
		//Get vis controller to update tree control to match internal
		//structure
		visControl.updateWxTreeCtrl(treeFilters);
		refreshButton->Enable(visControl.numFilters());
		
		doSceneUpdate();

		//If we are using the default camera,
		//move it to make sure that it is visible
		if(visControl.numCams() == 1)
			visControl.ensureSceneVisible(3);

		panelTop->Refresh();
		requireFirstUpdate=false;
	}


	//see if we need to update the post effects due to user interaction
	//with the crop panels
	if(panelFxCropOne->hasUpdate() || panelFxCropTwo->hasUpdate())
	{
		updatePostEffects();
		panelFxCropOne->clearUpdate();
		panelFxCropOne->clearUpdate();
	}

	//Check viscontrol to see if it needs an update, such as
	//when the user interacts with an object when it is not 
	//in the process of refreshing.
	//Don't attempt to update if already updating, or last
	//update aborted
	bool visUpdates=visControl.hasUpdates();
	bool plotUpdates=panelSpectra->hasUpdates();

	//I can has updates?
	if((visUpdates || plotUpdates) && !visControl.isRefreshing())
	{
		//FIXME: This is a massive hack. Use proper feedback to determine
		//the correct thing to update, rather than nuking everything
		//from orbit
		if(plotUpdates)
			visControl.invalidateRangeCaches();

		doSceneUpdate();
	}

	//Check the openGL pane to see if the camera property grid needs refreshing
	if(panelTop->hasCameraUpdates())
	{
		//Use the current combobox value to determine which camera is the 
		//current camera in the property grid
		

		int n = comboCamera->FindString(comboCamera->GetValue());

		if(n != wxNOT_FOUND)
		{
			wxListUint *l;
			l =(wxListUint*)  comboCamera->wxItemContainer::GetClientObject(n);

			visControl.updateCamPropertyGrid(gridCameraProperties,l->value);
		}

		panelTop->clearCameraUpdates();
	}

	if(plotUpdates)
		panelSpectra->clearUpdates();

	if(visUpdates)
	{
		wxTreeItemId id;
		id=treeFilters->GetSelection();

		if(id.IsOk() && (id != treeFilters->GetRootItem()))
		{
			//Tree data contains unique identifier for vis control to do matching
			wxTreeItemData *tData=treeFilters->GetItemData(id);
			visControl.updateFilterPropGrid(gridFilterProperties,
							((wxTreeUint *)tData)->value);
			editUndoMenuItem->Enable(visControl.getUndoSize());
			editRedoMenuItem->Enable(visControl.getRedoSize());
		}

	}




	timerEvent=false;	
}

void MainWindowFrame::statusMessage(const char *message, unsigned int type)
{
	bool sendMessage=true;
	switch(type)
	{
		case MESSAGE_ERROR:
			MainFrame_statusbar->SetBackgroundColour(*wxGREEN);
			break;
		case MESSAGE_INFO:
			MainFrame_statusbar->SetBackgroundColour(*wxCYAN);
			break;
		case MESSAGE_HINT:
			break;
		//Pseudo-messages
		case MESSAGE_NONE: // No actions needed, just supply the message
			ASSERT( string(message)== string(""));
			break;
		case MESSAGE_NONE_BUT_HINT:
			ASSERT( string(message)== string(""));
			//we need to clear any messages other than "hintMessage"
			sendMessage=(lastMessageType==MESSAGE_HINT);
			break;
		default:
			ASSERT(false);
	}

	lastMessageType=type;
	
	if(sendMessage)
		MainFrame_statusbar->SetStatusText(wxCStr(message),0);
	statusTimer->Start(STATUS_TIMER_DELAY,wxTIMER_ONE_SHOT);
}

void MainWindowFrame::updateProgressStatus()
{

	std::string progressString,filterProg;


	if(!visControl.numFilters())
		return;

	if(haveAborted)
	{
		progressString=TRANS("Aborted.");
	}
	else
	{
		ProgressData p;
		p=visControl.getProgress();
		ASSERT(p.totalProgress <= visControl.numFilters());
		
		if(p.filterProgress > 100)
			p.filterProgress=100;
	
		//Create a string from the total and percentile progresses
		std::string totalProg,totalCount,step,maxStep;
		stream_cast(totalProg,p.totalProgress);
		stream_cast(filterProg,p.filterProgress);
		stream_cast(totalCount,p.totalNumFilters);


		stream_cast(step,p.step);
		stream_cast(maxStep,p.maxStep);

		ASSERT(p.step <=p.maxStep);
		
		if(p.curFilter)
		{
			if(!p.maxStep)
				progressString = totalProg+TRANS(" of ") + totalCount +
						 " (" + p.curFilter->typeString() +")";
			else
			{
				progressString = totalProg+TRANS(" of ") + totalCount +
						 " (" + p.curFilter->typeString() + ", "
							+ step + "/" + maxStep + ": " + 
						       p.stepName+")";
			}
		}
		else
		{
			//If we have no filter, then we must be done if the totalProgress is
			//equal to the total count.
			if(totalProg == totalCount)
				progressString = TRANS("Updated.");
			else
				progressString = totalProg + " of " + totalCount;
		}

		if( p.filterProgress != 100)
			filterProg+=TRANS("\% Done (Esc aborts)");
		else
			filterProg+=TRANS("\% Done");

	}

	MainFrame_statusbar->SetBackgroundColour(wxNullColour);
    MainFrame_statusbar->SetStatusText(wxT(""),0);
	MainFrame_statusbar->SetStatusText(wxStr(progressString),1);
	MainFrame_statusbar->SetStatusText(wxStr(filterProg),2);
}

void MainWindowFrame::updatePostEffects()
{
	panelTop->currentScene.clearEffects();

	//Do we need post-processing?
#ifndef APPLE_EFFECTS_WORKAROUND
	if(!checkPostProcessing->IsChecked())
		return;
#endif
	if( checkFxCrop->IsChecked())
	{

		wxString ws;
		string s;
		ws=comboFxCropAxisOne->GetValue();
		s =stlStr(ws);

		//String encodes permutation (eg "x-y").
		unsigned int axisPerm[4];
		axisPerm[0] =(unsigned int)(s[0] -'x')*2;
		axisPerm[1] = (unsigned int)(s[0] -'x')*2+1;
		axisPerm[2] =(unsigned int)(s[2] -'x')*2;
		axisPerm[3] = (unsigned int)(s[2] -'x')*2+1;
		
		//Get the crop data, and generate an effect
		BoxCropEffect *b = new BoxCropEffect;

		//Assume, that unless otherwise specified
		//the default crop value is zero
		float array[6];
		float tmpArray[4];
		for(unsigned int ui=0;ui<6;ui++)
			array[ui]=0;
		
		//Permute the indices for the crop fractions, then assign
		panelFxCropOne->getCropValues(tmpArray);
		for(unsigned int ui=0;ui<4;ui++)
			array[axisPerm[ui]] = tmpArray[ui];
		
		
		ws=comboFxCropAxisTwo->GetValue();
		s =stlStr(ws);
		
		axisPerm[0] =(unsigned int)(s[0] -'x')*2;
		axisPerm[1] = (unsigned int)(s[0] -'x')*2+1;
		axisPerm[2] =(unsigned int)(s[2] -'x')*2;
		axisPerm[3] = (unsigned int)(s[2] -'x')*2+1;
		panelFxCropTwo->getCropValues(tmpArray);
		
		for(unsigned int ui=0;ui<4;ui++)
			array[axisPerm[ui]] = tmpArray[ui];

		b->setFractions(array);

		//Should we be using the camera frame?
		b->useCamCoords(checkFxCropCameraFrame->IsChecked());

		//Send the effect to the scene
		if(b->willDoSomething())
		{
			panelTop->currentScene.addEffect(b);
			panelTop->currentScene.setEffects(true);


			//Update the dx,dy and dz boxes
			BoundCube bcTmp;
			bcTmp=panelTop->currentScene.getBound();

			b->getCroppedBounds(bcTmp);	

			if(!checkFxCropCameraFrame->IsChecked())
			{
				float delta;
				delta=bcTmp.getBound(0,1)-bcTmp.getBound(0,0);
				stream_cast(s,delta);
				textFxCropDx->SetValue(wxStr(s));
				
				delta=bcTmp.getBound(1,1)-bcTmp.getBound(1,0);
				stream_cast(s,delta);
				textFxCropDy->SetValue(wxStr(s));
				
				delta=bcTmp.getBound(2,1)-bcTmp.getBound(2,0);
				stream_cast(s,delta);
				textFxCropDz->SetValue(wxStr(s));
			}
			else
			{
				textFxCropDx->SetValue(wxT(""));
				textFxCropDy->SetValue(wxT(""));
				textFxCropDz->SetValue(wxT(""));
			}
			
			//well, we dealt with this update.
			panelFxCropOne->clearUpdate();
			panelFxCropTwo->clearUpdate();
		}
		else
		{
			textFxCropDx->SetValue(wxT(""));
			textFxCropDy->SetValue(wxT(""));
			textFxCropDz->SetValue(wxT(""));
			delete b;

			//we should let this return true, 
			//so that an update takes hold
		}

	}
	

	if(checkFxEnableStereo->IsChecked())
	{
		AnaglyphEffect *anaglyph = new AnaglyphEffect;

		unsigned int sel;
		sel=comboFxStereoMode->GetSelection();
		anaglyph->setMode(sel);
		int v=sliderFxStereoBaseline->GetValue();

		float shift=((float)v)*BASELINE_SHIFT_FACTOR;

		anaglyph->setBaseShift(shift);
		anaglyph->setFlip(checkFxStereoLensFlip->IsChecked());
		panelTop->currentScene.addEffect(anaglyph);
	}

	panelTop->Refresh();
}

void MainWindowFrame::updateFxUI(const vector<const Effect*> &effs)
{
	//Here we pull information out from the effects and then
	//update the  ui controls accordingly

	Freeze();

	for(unsigned int ui=0;ui<effs.size();ui++)
	{
		switch(effs[ui]->getType())
		{
			case EFFECT_BOX_CROP:
			{
				const BoxCropEffect *e=(const BoxCropEffect*)effs[ui];

				//Enable the checkbox
				checkFxCrop->SetValue(true);
				//set the combos back to x-y y-z
				comboFxCropAxisOne->SetSelection(0);
				comboFxCropAxisTwo->SetSelection(1);
			
				//Temporarily de-link the panels
				panelFxCropOne->link(0,CROP_LINK_NONE);
				panelFxCropTwo->link(0,CROP_LINK_NONE);

				//Set the crop values 
				for(unsigned int ui=0;ui<6;ui++)
				{
					if(ui<4)
						panelFxCropOne->setCropValue(ui,
							e->getCropValue(ui));
					else if(ui > 2)
						panelFxCropTwo->setCropValue(ui-2,
							e->getCropValue(ui));
				}

				//Ensure that the values that went in were valid
				panelFxCropOne->makeCropValuesValid();
				panelFxCropTwo->makeCropValuesValid();


				//Restore the panel linkage
				panelFxCropOne->link(panelFxCropTwo,CROP_LINK_BOTH); 
				panelFxCropTwo->link(panelFxCropOne,CROP_LINK_BOTH); 


				break;
			}
			case EFFECT_ANAGLYPH:
			{
				const AnaglyphEffect *e=(const AnaglyphEffect*)effs[ui];
				//Set the slider from the base-shift value
				float shift;
				shift=e->getBaseShift();
				sliderFxStereoBaseline->SetValue(
					(unsigned int)(shift/BASELINE_SHIFT_FACTOR));


				//Set the stereo drop down colour
				unsigned int mode;
				mode = e->getMode();
				ASSERT(mode < comboFxStereoMode->GetCount());

				comboFxStereoMode->SetSelection(mode);
				//Enable the stereo mode
				checkFxEnableStereo->SetValue(true);
				break;
			}
			default:
				ASSERT(false);
		}
	
	}

	//Re-enable the effects UI as needed
	if(!effs.empty())
	{
#ifndef APPLE_EFFECTS_WORKAROUND
		checkPostProcessing->SetValue(true);
		noteFxPanelCrop->Enable();
		noteFxPanelStereo->Enable();
#endif
		visControl.setEffects(true);
	}
	

	Thaw();
}

void MainWindowFrame::OnProgressAbort(wxCommandEvent &event)
{
	if(!haveAborted)
		visControl.abort();
	haveAborted=true;
}

void MainWindowFrame::OnViewFullscreen(wxCommandEvent &event)
{
	if(programmaticEvent)
		return;

	programmaticEvent=true;

	//Toggle fullscreen, leave the menubar  & statusbar visible

	unsigned int flags;
#ifdef __APPLE__
	switch(fullscreenState)
	{
		case 0:
			flags= wxFULLSCREEN_NOTOOLBAR;
			ShowFullScreen(true,flags);
			menuViewFullscreen->SetHelp(wxTRANS("Next Fullscreen mode: none"));	
			break;
		case 1:
			menuViewFullscreen->SetHelp(wxTRANS("Next Fullscreen mode: complete"));	
			ShowFullScreen(false);
			break;
		case 2:
			menuViewFullscreen->SetHelp(wxTRANS("Next Fullscreen mode: none"));	
			ShowFullScreen(true);
			break;
		case 3:
			menuViewFullscreen->SetHelp(wxTRANS("Next Fullscreen mode: with toolbars"));	
			ShowFullScreen(false);
			break;

		default:
			ASSERT(false);

	}
	fullscreenState++;
	fullscreenState%=4;
#else
	//This does not work under non-apple platforms. :(
	switch(fullscreenState)
	{
		case 0:
			flags= wxFULLSCREEN_NOTOOLBAR;
			ShowFullScreen(true,flags);
			statusMessage(TRANS("Next Mode: No fullscreen"),MESSAGE_HINT);
			break;
		case 1:
			ShowFullScreen(false);
			statusMessage(TRANS("Next Mode: fullscreen w/o toolbar"),MESSAGE_HINT);
			break;
		case 2:
			ShowFullScreen(true);
			statusMessage(TRANS("Next Mode: fullscreen with toolbar"),MESSAGE_HINT);
			break;
		default:
			ASSERT(false);
	}
	fullscreenState++;
	fullscreenState%=3;
#endif
	programmaticEvent=false;
}

void MainWindowFrame::OnButtonRefresh(wxCommandEvent &event)
{

	//dirty hack to get keyboard state.
	wxMouseState wxm = wxGetMouseState();
	if(wxm.ShiftDown())
	{
		visControl.purgeFilterCache();
		statusMessage("",MESSAGE_NONE);
	}
	else
	{
		if(checkCaching->IsChecked())
			statusMessage(TRANS("Use shift-click to force full refresh"),MESSAGE_HINT);
	}
	doSceneUpdate();	
}

void MainWindowFrame::OnRawDataUnsplit(wxSplitterEvent &event)
{
	checkMenuRawDataPane->Check(false);
	configFile.setPanelEnabled(CONFIG_STARTUPPANEL_RAWDATA,false);
}

void MainWindowFrame::OnFilterPropDoubleClick(wxSplitterEvent &event)
{
	//Disallow unsplitting of filter property panel
	event.Veto();
}

void MainWindowFrame::OnControlUnsplit(wxSplitterEvent &event)
{
	//Make sure that the LHS panel is removed, rather than the default (right)
	splitLeftRight->Unsplit(panelLeft);  

	checkMenuControlPane->Check(false);
	configFile.setPanelEnabled(CONFIG_STARTUPPANEL_CONTROL,false);
}

void MainWindowFrame::OnSpectraUnsplit(wxSplitterEvent &event)
{
	checkMenuSpectraList->Check(false);
	configFile.setPanelEnabled(CONFIG_STARTUPPANEL_PLOTLIST,false);
}

//This function modifies the properties before showing the cell content editor.
//This is needed only for certain data types (colours, bools) other data types are edited
//using the default editor and modified using ::OnGridFilterPropertyChange
void MainWindowFrame::OnFilterGridCellEditorShow(wxGridEvent &event)
{

	if(programmaticEvent || timerEvent)
	{
		event.Skip();
		return;
	}
	//Find where the event occurred (cell & property)
	const GRID_PROPERTY *item=0;

	unsigned int key,set;
	key=gridFilterProperties->getKeyFromRow(event.GetRow());
	set=gridFilterProperties->getSetFromRow(event.GetRow());

	item=gridFilterProperties->getProperty(set,key);

	bool needUpdate=false;

	//Get the filter ID value (long song and dance that it is)
	wxTreeItemId tId = treeFilters->GetSelection();
	if(!tId.IsOk())
		return;
	
	size_t filterId;
	wxTreeItemData *tData=treeFilters->GetItemData(tId);
	filterId = ((wxTreeUint *)tData)->value;


	switch(item->type)
	{
		case PROPERTY_TYPE_BOOL:
		{
			std::string s;
			//Toggle the property in the grid
			if(item->data == "0")
				s= "1";
			else
				s="0";
			visControl.setFilterProperties(filterId,set,key,s,needUpdate);

			event.Veto();
			break;
		}
		case PROPERTY_TYPE_COLOUR:
		{
			//Show a wxColour choose dialog. 
			wxColourData d;

			unsigned char r,g,b,a;
			parseColString(item->data,r,g,b,a);

			d.SetColour(wxColour(r,g,b,a));
			wxColourDialog *colDg=new wxColourDialog(this->GetParent(),&d);
						

			if( colDg->ShowModal() == wxID_OK)
			{
				wxColour c;
				//Change the colour
				c=colDg->GetColourData().GetColour();
				
				std::string s;
				genColString(c.Red(),c.Green(),c.Blue(),s);
			
				//Pass the new colour to the viscontrol system, which updates
				//the filters	
				visControl.setFilterProperties(filterId,set,key,s,needUpdate);
			}

			//Set the filter property
			//Disallow direct editing of the grid cell
			event.Veto();

			break;
		}	
		default:
			//we will handle this after the user has edited the cell contents
			//but we must lock controls that can alter the active filter, and thereby changing the 
			// filter grid in the meantime
			refreshButton->Enable(false);
			comboFilters->Enable(false);	
			comboStash->Enable(false);	
			treeFilters->Enable(false);	
			break;
	}

	if(needUpdate)
	{
		visControl.updateFilterPropGrid(gridFilterProperties,
						((wxTreeUint *)tData)->value);

		editUndoMenuItem->Enable(visControl.getUndoSize());
		editRedoMenuItem->Enable(visControl.getRedoSize());
		if(checkAutoUpdate->GetValue())
			doSceneUpdate();
	}
}

void MainWindowFrame::OnFilterGridCellEditorHide(wxGridEvent &event)
{
	//re-enable the controls that were locked during OnFilterGridCellEditorShow
	refreshButton->Enable();
	comboFilters->Enable();	
	comboStash->Enable();	
	treeFilters->Enable();	
}

void MainWindowFrame::OnCameraGridCellEditorShow(wxGridEvent &event)
{
	if(programmaticEvent||timerEvent)
	{
		event.Skip();
		return;
	}
	//Find where the event occurred (cell & property)
	const GRID_PROPERTY *item=0;

	unsigned int key,set;
	key=gridCameraProperties->getKeyFromRow(event.GetRow());
	set=gridCameraProperties->getSetFromRow(event.GetRow());

	item=gridCameraProperties->getProperty(set,key);

	//Get the camera ID
	wxListUint *l;
	int n = comboCamera->FindString(comboCamera->GetValue());
	l =(wxListUint*)  comboCamera->wxItemContainer::GetClientObject(n);
	if(!l)
		return;
	
	size_t camUniqueID=l->value;


	switch(item->type)
	{
		case PROPERTY_TYPE_BOOL:
		{
			std::string s;
			//Toggle the property in the grid
			if(item->data == "0")
				s= "1";
			else
				s="0";
			visControl.setCamProperties(camUniqueID,key,s);
		
			//For some reason this does not  redraw neatly. Force a redraw
			visControl.updateCamPropertyGrid(gridCameraProperties,camUniqueID);

			event.Veto();
			break;
		}
		case PROPERTY_TYPE_COLOUR:
		{
			//Show a wxColour choose dialog. 
			wxColourData d;

			unsigned char r,g,b,a;
			parseColString(item->data,r,g,b,a);

			d.SetColour(wxColour(r,g,b,a));
			wxColourDialog *colDg=new wxColourDialog(this->GetParent(),&d);
						

			if( colDg->ShowModal() == wxID_OK)
			{
				wxColour c;
				//Change the colour
				c=colDg->GetColourData().GetColour();
				
				std::string s;
				genColString(c.Red(),c.Green(),c.Blue(),s);
			
				//Pass the new colour to the viscontrol system, which updates
				//the filters	
				visControl.setCamProperties(camUniqueID,key,s);
			}

			//Set the filter property
			//Disallow direct editing of the grid cell
			event.Veto();

			break;
		}	
		default:
			//we will handle this after the user has edited the cell contents
			//Lock the camera combo, so the user can't alter the camera data while we are using the editor
			comboCamera->Enable(false);
			break;
	}

	panelTop->Refresh(false);
	
}

void MainWindowFrame::OnButtonGridCopy(wxCommandEvent &event)
{
	gridRawData->copyData();	
}

void MainWindowFrame::OnButtonGridSave(wxCommandEvent &event)
{
	if(!gridRawData->GetRows()||!gridRawData->GetCols())
	{
		statusMessage(TRANS("No data to save"),MESSAGE_ERROR);
		return;
	}
	gridRawData->saveData();	
}

void MainWindowFrame::OnCheckAlpha(wxCommandEvent &event)
{
	panelTop->currentScene.setAlpha(event.IsChecked());

	panelTop->Refresh();
}

void MainWindowFrame::OnCheckLighting(wxCommandEvent &event)
{
	panelTop->currentScene.setLighting(event.IsChecked());
	
	panelTop->Refresh();
}

void MainWindowFrame::OnCheckCacheEnable(wxCommandEvent &event)
{
	if(event.IsChecked())
		visControl.setCachePercent((unsigned int)spinCachePercent->GetValue());
	else
	{
		visControl.setCachePercent(0);
		visControl.purgeFilterCache();

		doSceneUpdate();
	}
}

void MainWindowFrame::OnCheckWeakRandom(wxCommandEvent &event)
{
	visControl.setStrongRandom(!event.IsChecked());

	doSceneUpdate();
}

void MainWindowFrame::OnCacheRamUsageSpin(wxSpinEvent &event)
{
	ASSERT(event.GetPosition() >= 0 &&event.GetPosition()<=100);

	visControl.setCachePercent(event.GetPosition());
	
}
void MainWindowFrame::OnButtonRemoveCam(wxCommandEvent &event)
{

	std::string camName;


	camName=stlStr(comboCamera->GetValue());

	if (!camName.size())
		return;

	int n = comboCamera->FindString(comboCamera->GetValue());

	if ( n!= wxNOT_FOUND ) 
	{
		wxListUint *l;
		l =(wxListUint*)  comboCamera->wxItemContainer::GetClientObject(n);
		visControl.removeCam(l->value);
		comboCamera->Delete(n);
		
		programmaticEvent=true;
		comboCamera->SetValue(wxT(""));
		gridCameraProperties->clear();
		programmaticEvent=false;
	}
}

void MainWindowFrame::OnSpectraListbox(wxCommandEvent &event)
{
	//This function gets called programatically by 
	//doSceneUpdate. Prevent interaction.
	if(visControl.isRefreshing())
		return;
	//Get the currently selected item
	//Spin through the selected items
	for(unsigned int ui=0;ui<plotList->GetCount(); ui++)
	{
	 	wxListUint *l;
		unsigned int plotID;

		//Retrieve the uniqueID
		l=(wxListUint*)plotList->wxItemContainer::GetClientObject(ui);
		plotID = l->value;

		panelSpectra->setPlotVisible(plotID,plotList->IsSelected(ui));

	}
	
	panelSpectra->Refresh();
	//The raw grid contents may change due to the list selection
	//change. Update the grid
	visControl.updateRawGrid();
}

void MainWindowFrame::OnClose(wxCloseEvent &event)
{

	if(visControl.isRefreshing())
	{
		if(!haveAborted)
		{
			visControl.abort();
			haveAborted=true;

			statusMessage(TRANS("Aborting..."),MESSAGE_INFO);
			return;
		}
		else
		{
			wxMessageDialog *wxD  =new wxMessageDialog(this,
					wxTRANS("Waiting for refresh to abort. Exiting could lead to the program backgrounding. Exit anyway? "),
					wxTRANS("Confirmation request"),wxOK|wxCANCEL|wxICON_ERROR);

			if(wxD->ShowModal() != wxID_OK)
			{
				event.Veto();
				wxD->Destroy();
				return;
			}
			wxD->Destroy();
		}
	}
	else
	{
		if(event.CanVeto())
		{
			if(visControl.numFilters() || visControl.numCams() > 1)
			{
				//Prompt for close
				wxMessageDialog *wxD  =new wxMessageDialog(this,
						wxTRANS("Are you sure you wish to exit 3Depict?"),\
						wxTRANS("Confirmation request"),wxOK|wxCANCEL|wxICON_ERROR);
				if(wxD->ShowModal() != wxID_OK)
				{
					event.Veto();
					wxD->Destroy();
					return;
				}
				wxD->Destroy();
			
			}
		}
	}
	
	//Remove the autosave file if it exists
	wxString filePath = wxStandardPaths::Get().GetDocumentsDir()+wxCStr("/.")+wxCStr(PROGRAM_NAME);

	//Get self PID
	std::string pidStr;
	unsigned int pid;
	pid=wxGetProcessId();
	stream_cast(pidStr,pid);
	
	
	filePath+=wxCStr("/") + wxCStr(AUTOSAVE_PREFIX) + wxStr(pidStr)+ wxCStr(AUTOSAVE_SUFFIX);

	if(wxFileExists(filePath))
		wxRemoveFile(filePath);

	//Remember current window size for next time
	wxSize winSize;
	winSize=GetSize();
	configFile.setInitialAppSize(winSize.GetWidth(),winSize.GetHeight());

	//Remember the sash positions for next time, as fractional values fo
	// the window size
	float frac;
	frac =(float) splitLeftRight->GetSashPosition()/winSize.GetWidth();
	configFile.setLeftRightSashPos(frac);
	frac = (float) splitTopBottom->GetSashPosition()/winSize.GetHeight();
	configFile.setTopBottomSashPos(frac);
	frac= (float)filterSplitter->GetSashPosition()/winSize.GetHeight();
	configFile.setFilterSashPos(frac);

	winSize=noteDataView->GetSize();
	frac = (float)splitterSpectra->GetSashPosition()/winSize.GetWidth();

	//Try to save the configuration
	configFile.write();

	 if(verCheckThread)
	 {
		 if(!verCheckThread->isComplete())
		 {
			 //Kill it.
			 verCheckThread->Kill();
		 }
		 delete verCheckThread;
	}
	 

	 //Terminate the program
 	 Destroy();
}



void MainWindowFrame::OnCheckPostProcess(wxCommandEvent &event)
{
#ifdef APPLE_EFFECTS_WORKAROUND
	//FIXME: I have disabled this under apple
	ASSERT(false);
#endif
	//Disable the entire UI panel
	noteFxPanelCrop->Enable(event.IsChecked());
	noteFxPanelStereo->Enable(event.IsChecked());
	visControl.setEffects(event.IsChecked());
	updatePostEffects();
	
	panelTop->Refresh();
}


void MainWindowFrame::OnFxCropCheck(wxCommandEvent &event)
{
	//Disable/enable the other UI controls on the crop effects page
	//Include the text labels to give them that "greyed-out" look
	checkFxCropCameraFrame->Enable(event.IsChecked());
	comboFxCropAxisOne->Enable(event.IsChecked());
	panelFxCropOne->Enable(event.IsChecked());
	comboFxCropAxisTwo->Enable(event.IsChecked());
	panelFxCropTwo->Enable(event.IsChecked());
	textFxCropDx->Enable(event.IsChecked());
	textFxCropDy->Enable(event.IsChecked());
	textFxCropDz->Enable(event.IsChecked());
	labelFxCropDx->Enable(event.IsChecked());
	labelFxCropDy->Enable(event.IsChecked());
	labelFxCropDz->Enable(event.IsChecked());

	updatePostEffects();
}


void MainWindowFrame::OnFxCropCamFrameCheck(wxCommandEvent &event)
{
	updatePostEffects();
}



void MainWindowFrame::OnFxCropAxisOne(wxCommandEvent &event)
{
	linkCropWidgets();
	updatePostEffects();
}

void MainWindowFrame::OnFxCropAxisTwo(wxCommandEvent &event)
{
	linkCropWidgets();
	updatePostEffects();
}

void MainWindowFrame::linkCropWidgets()
{
	//Adjust the link mode as needed
	//Lets cheat a little and parse the combo box contents
	
	unsigned int linkMode;

	string first[2],second[2];

	wxString s;
	string tmp;
	
	s=comboFxCropAxisOne->GetValue();
	tmp=stlStr(s);
	first[0]=tmp[0];
	second[0]=tmp[2];

	s=comboFxCropAxisTwo->GetValue();
	tmp=stlStr(s);
	first[1]=tmp[0];
	second[1]=tmp[2];


	linkMode=0;
	//First and second axis match?
	if(first[0] == first[1] && second[0] == second[1])
	{
		linkMode=CROP_LINK_BOTH;
	}
	else if(first[0] == second[1] && second[0] == first[1])
		linkMode=CROP_LINK_BOTH_FLIP;
	else if(first[0] == first[1])
		linkMode=CROP_LINK_LR;
	else if(second[0] == second[1])
		linkMode=CROP_LINK_TB;
	else if(second[0] == first[1])
	{
		panelFxCropOne->link(panelFxCropTwo,CROP_LINK_TB_FLIP);
		panelFxCropTwo->link(panelFxCropOne,CROP_LINK_LR_FLIP);
	}
	else if(second[1]== first[0])
	{
		panelFxCropOne->link(panelFxCropTwo,CROP_LINK_LR_FLIP);
		panelFxCropTwo->link(panelFxCropOne,CROP_LINK_TB_FLIP);
	}
	else
	{
		//Pigeonhole principle says we can't get here.
		ASSERT(false);
	}
		

	if(linkMode)
	{
		panelFxCropOne->link(panelFxCropTwo,linkMode);
		panelFxCropTwo->link(panelFxCropOne,linkMode);
	}

}




void MainWindowFrame::OnFxStereoEnable(wxCommandEvent &event)
{
	comboFxStereoMode->Enable(event.IsChecked());
	sliderFxStereoBaseline->Enable(event.IsChecked());
	checkFxStereoLensFlip->Enable(event.IsChecked());

	updatePostEffects();
}

void MainWindowFrame::OnFxStereoLensFlip(wxCommandEvent &event)
{
	updatePostEffects();
}


void MainWindowFrame::OnFxStereoCombo(wxCommandEvent &event)
{
	updatePostEffects();
}


void MainWindowFrame::OnFxStereoBaseline(wxScrollEvent &event)
{
	updatePostEffects();
}

// wxGlade: add MainWindowFrame event handlers

void MainWindowFrame::SetCommandLineFiles(wxArrayString &files)
{

	textConsoleOut->Clear();
	//Load them up as data.
	for(unsigned int ui=0;ui<files.size();ui++)
		loadFile(files[ui],true);


	if(files.GetCount())
	{
		//OK, we got here, so it must be loaded.
		//so we need to update.
		requireFirstUpdate=true;
	}
}

void MainWindowFrame::OnNoteDataView(wxNotebookEvent &evt)
{
	//Get rid of the console page
	if(evt.GetSelection() == NOTE_CONSOLE_PAGE_OFFSET)
		noteDataView->SetPageText(NOTE_CONSOLE_PAGE_OFFSET,wxTRANS("Cons."));

	//Keep processing
	evt.Skip();
}

void MainWindowFrame::OnCheckUpdatesThread(wxCommandEvent &evt)
{
	//Check to see if we have a new version or not, and
	//what that version number is
	
	ASSERT(verCheckThread->isComplete());

	//Check to see if we got the version number OK. 
	// this might have failed, e.g. if the user has no net connection,
	// or the remote RSS is not parseable
	if(verCheckThread->isRetrieveOK())
	{
		string remoteMax=verCheckThread->getVerStr().c_str();

		string s;
		if(remoteMax != PROGRAM_VERSION)
		{
			//Use status bar message to notify user about update
			s = string(TRANS("Update Notice: New version ")) + remoteMax + TRANS(" found online.");
		}
		else
		{
			s=string(TRANS("Online Check: " ))+string(PROGRAM_NAME) + TRANS(" is up-to-date."); 
		}
		statusMessage(s.c_str(),MESSAGE_INFO);
	}

	//Wait for, then delete the other thread, as we are done with it
	verCheckThread->Wait();

	delete verCheckThread;
	verCheckThread=0;

}

void MainWindowFrame::checkReloadAutosave()
{

	wxString filePath = wxStandardPaths::Get().GetDocumentsDir()+wxCStr("/.")+wxCStr(PROGRAM_NAME);
	filePath+=wxCStr("/") ;


	//obtain a list of autosave xml files
	wxArrayString *dirListing= new wxArrayString;
	std::string s;
	s=std::string(AUTOSAVE_PREFIX) +
			std::string("*") + std::string(AUTOSAVE_SUFFIX);;
	wxString fileMask = wxStr(s);

	wxDir::GetAllFiles(filePath,dirListing,fileMask,wxDIR_FILES);

	if(!dirListing->GetCount())
	{
		delete dirListing;
		return;
	}


	unsigned int prefixLen;
	prefixLen = stlStr(filePath).size() + strlen(AUTOSAVE_PREFIX) + 1;

	map<string,unsigned int> autosaveNamePIDMap;
	for(unsigned int ui=0;ui<dirListing->GetCount(); ui++)
	{
		std::string tmp;
		tmp = stlStr(dirListing->Item(ui));
		//File name should match specified glob.
		ASSERT(tmp.size() >=(strlen(AUTOSAVE_PREFIX) + strlen(AUTOSAVE_SUFFIX)));

		//Strip the non-glob bit out of the string
		tmp = tmp.substr(prefixLen-1,tmp.size()-(strlen(AUTOSAVE_SUFFIX) + prefixLen-1));

		unsigned int pid;
		if(stream_cast(pid,tmp))
			continue;
		autosaveNamePIDMap[stlStr(dirListing->Item(ui))] = pid;
	}

	delete dirListing;

	for(map<string,unsigned int>::iterator it=autosaveNamePIDMap.begin();
		it!=autosaveNamePIDMap.end();)
	{
		//Filter on process existence and name match.
		//Note that map does not have a return value for erase in C++<C++11.
		// so use the unusual postfix incrementor method
		if(wxProcess::Exists(it->second) && processMatchesName(it->second,PROGRAM_NAME) )
			autosaveNamePIDMap.erase(it++); //Note postfix!
		else
			++it;
	}

	if(autosaveNamePIDMap.size() == 1)
	{
		filePath=wxStr(autosaveNamePIDMap.begin()->first);
		//Well, looky here, looks like we got an autosave
		wxMessageDialog *wxD  =new wxMessageDialog(this,
			wxTRANS("An auto-save state was found, would you like to restore it?.") 
			,wxTRANS("Autosave"),wxCANCEL|wxOK|wxICON_QUESTION|wxYES_DEFAULT );

		if(wxD->ShowModal()!= wxID_CANCEL)
		{
			if(!loadFile(filePath))
				statusMessage(TRANS("Unable to load autosave file.."),MESSAGE_ERROR);
			else
			{
				requireFirstUpdate=true;
				//Prevent the program from allowing save menu usage
				//into autosave file
				currentFile.clear();
				fileSave->Enable(false);		
			}
		}
		else
		{
			//No file to remove
			filePath=wxT("");
		}
	}
	else if(autosaveNamePIDMap.size() > 1)
	{	
		wxArrayString autoSaveChoices;
		vector<string> filenames;
		for(map<string,unsigned int>::iterator it=autosaveNamePIDMap.begin();
			it!=autosaveNamePIDMap.end();++it)
		{

			//Get the timestamp for the file
		
			time_t timeStamp=wxFileModificationTime(wxStr(it->first));
			time_t now = wxDateTime::Now().GetTicks();

			string s;
			s=veryFuzzyTimeSince(timeStamp,now);
		
			s=it->first + string(", ") + s;  //format like "filename.xml, a few seconds ago"
			autoSaveChoices.Add(wxStr(s));
			filenames.push_back(it->first);
		}
		//OK, looks like we have multiple selection options.
		//populate a list to ask the user to choose from.
		//User may only pick a single thing to restore.
		wxSingleChoiceDialog *dlg= new wxSingleChoiceDialog(this,
			wxTRANS("Multiple auto-save states were found; would you like to restore one?"),
			wxTRANS("Restore auto-save?"),autoSaveChoices,NULL, wxCANCEL|wxOK|wxICON_QUESTION);

		//Show the dialog to get a choice from the user
		if(dlg->ShowModal()==wxID_OK)
		{
			requireFirstUpdate=true;

			filePath=wxStr(filenames[dlg->GetSelection()]);

			if(loadFile(filePath))
			{
				//Prevent the program from allowing save menu usage
				//into autosave file
				currentFile.clear();
				fileSave->Enable(false);

			}

		}
		else
		{
			//No file to delete
			filePath=wxT("");
		}

	}
	

	//
	if(filePath != wxT(""))	
		wxRemoveFile(filePath);	
	
}
wxSize MainWindowFrame::getNiceWindowSize() const
{
	wxDisplay *disp=new wxDisplay;
	wxRect r = disp->GetClientArea();

	bool haveDisplaySizePref;
	unsigned int xPref,yPref;

	haveDisplaySizePref=configFile.getInitialAppSize(xPref,yPref);

	//So Min size trumps all
	// - then client area 
	// - then saved setting
	// - then default size 
	wxSize winSize;
	if(haveDisplaySizePref)
		winSize.Set(xPref,yPref);
	else
	{
		winSize.Set(DEFAULT_WIN_WIDTH,DEFAULT_WIN_HEIGHT);
	}

	//Override using minimal window sizes
	winSize.Set(std::max(winSize.GetWidth(),(int)MIN_WIN_WIDTH),
			std::max(winSize.GetHeight(),(int)MIN_WIN_HEIGHT));

	//Shrink to display size, as needed	
	winSize.Set(std::min(winSize.GetWidth(),r.GetWidth()),
			std::min(winSize.GetHeight(),r.GetHeight()));


	delete disp;

	return winSize;

}

void MainWindowFrame::set_properties()
{
    // begin wxGlade: MainWindowFrame::set_properties
    SetTitle(wxCStr(PROGRAM_NAME));
    comboFilters->SetToolTip(wxTRANS("List of available filters"));
    comboFilters->SetSelection(-1);
    treeFilters->SetToolTip(wxTRANS("Tree of data filters"));
    checkAutoUpdate->SetToolTip(wxTRANS("Enable/Disable automatic updates of data when filter change takes effect"));
    checkAutoUpdate->SetValue(true);

    checkAlphaBlend->SetToolTip(wxTRANS("Enable/Disable \"Alpha blending\" (transparency) in rendering system. This is used to smooth objects (avoids artefacts known as \"jaggies\") and to make transparent surfaces. Disabling will provide faster rendering but look more blocky")); 
    checkLighting->SetToolTip(wxTRANS("Enable/Disable lighting calculations in rendering, for objects that request this. Lighting provides important depth cues for objects comprised of 3D surfaces. Disabling may allow faster rendering in complex scenes"));
    checkWeakRandom->SetToolTip(wxTRANS("Enable/Disable weak randomisation (Galois linear feedback shift register). Strong randomisation uses a much slower random selection method, but provides better protection against inadvertent correlations, and is recommended for final analyses"));
    checkCaching->SetToolTip(wxTRANS("Enable/Disable caching of intermediate results during filter updates. This will use less system RAM, though changes to any filter property will cause the entire filter tree to be recomputed, greatly slowing computations"));

    gridFilterProperties->CreateGrid(0, 2);
    gridFilterProperties->EnableDragRowSize(false);
    gridFilterProperties->SetColLabelValue(0, wxTRANS("Property"));
    gridFilterProperties->SetColLabelValue(1, wxTRANS("Value"));
    gridCameraProperties->CreateGrid(4, 2);
    gridCameraProperties->EnableDragRowSize(false);
    gridCameraProperties->SetSelectionMode(wxGrid::wxGridSelectRows);
    gridCameraProperties->SetColLabelValue(0, wxTRANS("Property"));
    gridCameraProperties->SetColLabelValue(1, wxTRANS("Value"));
    gridCameraProperties->SetToolTip(wxTRANS("Camera data information"));
    noteCamera->SetScrollRate(10, 10);
#ifndef APPLE_EFFECTS_WORKAROUND
    checkPostProcessing->SetToolTip(wxTRANS("Enable/disable visual effects on final 3D output"));
#endif
    checkFxCrop->SetToolTip(wxTRANS("Enable cropping post-process effect"));
    comboFxCropAxisOne->SetSelection(0);
    comboFxCropAxisTwo->SetSelection(0);
    checkFxEnableStereo->SetToolTip(wxTRANS("Colour based 3D effect enable/disable - requires appropriate colour filter 3D glasses."));
    comboFxStereoMode->SetToolTip(wxTRANS("Glasses colour mode"));
    comboFxStereoMode->SetSelection(0);
    sliderFxStereoBaseline->SetToolTip(wxTRANS("Level of separation between left and right images, which sets 3D depth to visual distortion tradeoff"));
    gridRawData->CreateGrid(10, 2);
    gridRawData->EnableEditing(false);
    gridRawData->EnableDragRowSize(false);
    gridRawData->SetColLabelValue(0, wxTRANS("X"));
    gridRawData->SetColLabelValue(1, wxTRANS("Y"));
    btnRawDataSave->SetToolTip(wxTRANS("Save raw data to file"));
    btnRawDataClip->SetToolTip(wxTRANS("Copy raw data to clipboard"));
    textConsoleOut->SetToolTip(wxTRANS("Program text output"));
    // end wxGlade
    //

    PlotWrapper *p=new PlotWrapper; //plotting area
    
    panelSpectra->setPlotWrapper(p);
    panelSpectra->setPlotList(plotList);
    
    //Set the controls that the viscontrol needs to interact with
    visControl.setScene(&panelTop->currentScene); //GL scene
    visControl.setRawGrid(gridRawData); //Raw data grid
    visControl.setPlotWrapper(p);
    visControl.setPlotList(plotList);
    visControl.setConsole(textConsoleOut);


    refreshButton->Enable(false);
    comboCamera->Connect(wxID_ANY,
                 wxEVT_SET_FOCUS,
		   wxFocusEventHandler(MainWindowFrame::OnComboCameraSetFocus), NULL, this);
    comboStash->Connect(wxID_ANY,
                 wxEVT_SET_FOCUS,
		   wxFocusEventHandler(MainWindowFrame::OnComboStashSetFocus), NULL, this);
    noteDataView->Connect(wxID_ANY, wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGED,
		    wxNotebookEventHandler(MainWindowFrame::OnNoteDataView),NULL,this);

    gridCameraProperties->clear();
    int widths[] = {-4,-2,-1};
    MainFrame_statusbar->SetStatusWidths(3,widths);

}

void MainWindowFrame::do_layout()
{
    // begin wxGlade: MainWindowFrame::do_layout
    wxBoxSizer* topSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizerLeft = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizerTools = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizerToolsRamUsage = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* postProcessSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizerFxStereo = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizerSetereoBaseline = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizerStereoCombo = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* cropFxSizer = new wxBoxSizer(wxVERTICAL);
    wxFlexGridSizer* sizerFxCropGridLow = new wxFlexGridSizer(3, 2, 2, 2);
    wxBoxSizer* cropFxBodyCentreSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* rightPanelSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* textConsoleSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* rawDataGridSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* rawDataSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* plotListSizery = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* topPanelSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizerFxCropRHS = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizerFxCropLHS = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* camPaneSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* camTopRowSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* filterPaneSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* filterPropGridSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* filterTreeLeftRightSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* filterRightOfTreeSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* filterMainCtrlSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* stashRowSizer = new wxBoxSizer(wxHORIZONTAL);
    filterPaneSizer->Add(lblSettings, 0, 0, 0);
    stashRowSizer->Add(comboStash, 1, wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND, 3);
    stashRowSizer->Add(btnStashManage, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 0);
    filterPaneSizer->Add(stashRowSizer, 0, wxEXPAND, 0);
    filterPaneSizer->Add(filteringLabel, 0, 0, 0);
    filterMainCtrlSizer->Add(comboFilters, 0, wxLEFT|wxRIGHT|wxEXPAND, 4);
    filterMainCtrlSizer->Add(treeFilters, 3, wxLEFT|wxBOTTOM|wxEXPAND, 3);
    filterMainCtrlSizer->Add(lastRefreshLabel, 0, wxTOP, 8);
    filterMainCtrlSizer->Add(listLastRefresh, 1, wxBOTTOM|wxEXPAND, 5);
    filterTreeLeftRightSizer->Add(filterMainCtrlSizer, 3, wxEXPAND, 0);
    filterRightOfTreeSizer->Add(checkAutoUpdate, 0, 0, 0);
    filterRightOfTreeSizer->Add(10, 10, 0, 0, 0);
    filterRightOfTreeSizer->Add(refreshButton, 0, wxALL, 2);
    filterRightOfTreeSizer->Add(20, 20, 0, 0, 0);
    filterRightOfTreeSizer->Add(btnFilterTreeCollapse, 0, wxLEFT, 6);
    filterRightOfTreeSizer->Add(btnFilterTreeExpand, 0, wxLEFT, 6);
    filterTreeLeftRightSizer->Add(filterRightOfTreeSizer, 2, wxEXPAND, 0);
    filterTreePane->SetSizer(filterTreeLeftRightSizer);
    filterPropGridSizer->Add(propGridLabel, 0, 0, 0);
    filterPropGridSizer->Add(gridFilterProperties, 1, wxLEFT|wxEXPAND, 4);
    filterPropertyPane->SetSizer(filterPropGridSizer);
//    filterSplitter->SplitHorizontally(filterTreePane, filterPropertyPane);//DISABLED This has to be done later to get the window to work.
    filterPaneSizer->Add(filterSplitter, 1, wxEXPAND, 0);
    noteData->SetSizer(filterPaneSizer);
    camPaneSizer->Add(labelCameraName, 0, 0, 0);
    camTopRowSizer->Add(comboCamera, 3, 0, 0);
    camTopRowSizer->Add(buttonRemoveCam, 0, wxLEFT|wxRIGHT, 2);
    camPaneSizer->Add(camTopRowSizer, 0, wxTOP|wxBOTTOM|wxEXPAND, 4);
    camPaneSizer->Add(cameraNamePropertySepStaticLine, 0, wxEXPAND, 0);
    camPaneSizer->Add(gridCameraProperties, 1, wxEXPAND, 0);
    noteCamera->SetSizer(camPaneSizer);
#ifndef APPLE_EFFECTS_WORKAROUND
    postProcessSizer->Add(checkPostProcessing, 0, wxALL, 5);
#endif
    cropFxSizer->Add(checkFxCrop, 0, wxALL, 6);
    cropFxSizer->Add(checkFxCropCameraFrame, 0, wxLEFT, 15);
    sizerFxCropLHS->Add(comboFxCropAxisOne, 0, wxRIGHT|wxBOTTOM|wxEXPAND|wxALIGN_CENTER_HORIZONTAL, 5);
    sizerFxCropLHS->Add(panelFxCropOne, 1, wxRIGHT|wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    cropFxBodyCentreSizer->Add(sizerFxCropLHS, 1, wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 0);
    sizerFxCropRHS->Add(comboFxCropAxisTwo, 0, wxLEFT|wxBOTTOM|wxEXPAND, 5);
    sizerFxCropRHS->Add(panelFxCropTwo, 1, wxLEFT|wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    cropFxBodyCentreSizer->Add(sizerFxCropRHS, 1, wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 0);
    cropFxSizer->Add(cropFxBodyCentreSizer, 1, wxLEFT|wxRIGHT|wxTOP|wxEXPAND, 5);
    sizerFxCropGridLow->Add(labelFxCropDx, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 0);
    sizerFxCropGridLow->Add(textFxCropDx, 0, 0, 0);
    sizerFxCropGridLow->Add(labelFxCropDy, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 0);
    sizerFxCropGridLow->Add(textFxCropDy, 0, 0, 0);
    sizerFxCropGridLow->Add(labelFxCropDz, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 0);
    sizerFxCropGridLow->Add(textFxCropDz, 0, 0, 0);
    sizerFxCropGridLow->AddGrowableRow(0);
    sizerFxCropGridLow->AddGrowableRow(1);
    sizerFxCropGridLow->AddGrowableRow(2);
    sizerFxCropGridLow->AddGrowableCol(0);
    sizerFxCropGridLow->AddGrowableCol(1);
    cropFxSizer->Add(sizerFxCropGridLow, 0, wxBOTTOM|wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    noteFxPanelCrop->SetSizer(cropFxSizer);
    sizerFxStereo->Add(checkFxEnableStereo, 0, wxLEFT|wxTOP, 6);
    sizerFxStereo->Add(20, 20, 0, 0, 0);
    sizerStereoCombo->Add(lblFxStereoMode, 0, wxLEFT|wxRIGHT|wxALIGN_CENTER_VERTICAL, 5);
    sizerStereoCombo->Add(comboFxStereoMode, 0, wxLEFT, 5);
    sizerStereoCombo->Add(bitmapFxStereoGlasses, 0, 0, 0);
    sizerFxStereo->Add(sizerStereoCombo, 0, wxBOTTOM|wxEXPAND, 15);
    sizerSetereoBaseline->Add(labelFxStereoBaseline, 0, wxLEFT|wxTOP, 5);
    sizerSetereoBaseline->Add(sliderFxStereoBaseline, 1, wxLEFT|wxRIGHT|wxTOP|wxEXPAND, 5);
    sizerFxStereo->Add(sizerSetereoBaseline, 0, wxEXPAND, 0);
    sizerFxStereo->Add(checkFxStereoLensFlip, 0, wxLEFT, 5);
    noteFxPanelStereo->SetSizer(sizerFxStereo);
    noteEffects->AddPage(noteFxPanelCrop, wxTRANS("Crop"));
    noteEffects->AddPage(noteFxPanelStereo, wxTRANS("Stereo"));
    postProcessSizer->Add(noteEffects, 1, wxEXPAND, 0);
    notePost->SetSizer(postProcessSizer);
    sizerTools->Add(checkAlphaBlend, 0, wxLEFT|wxTOP|wxBOTTOM, 5);
    sizerTools->Add(checkLighting, 0, wxLEFT|wxTOP|wxBOTTOM, 6);
    sizerTools->Add(checkWeakRandom, 0, wxLEFT|wxTOP|wxBOTTOM, 5);
    sizerTools->Add(checkCaching, 0, wxLEFT|wxTOP|wxBOTTOM, 5);
    sizerToolsRamUsage->Add(labelMaxRamUsage, 0, wxRIGHT|wxALIGN_RIGHT, 5);
    sizerToolsRamUsage->Add(spinCachePercent, 0, 0, 5);
    sizerTools->Add(sizerToolsRamUsage, 1, wxTOP|wxEXPAND, 5);
    noteTools->SetSizer(sizerTools);
    notebookControl->AddPage(noteData, wxTRANS("Data"));
    notebookControl->AddPage(noteCamera, wxTRANS("Cam"));
    notebookControl->AddPage(notePost, wxTRANS("Post"));
    notebookControl->AddPage(noteTools, wxTRANS("Tools"));
    sizerLeft->Add(notebookControl, 1, wxLEFT|wxBOTTOM|wxEXPAND, 2);
    panelLeft->SetSizer(sizerLeft);
    topPanelSizer->Add(panelView, 1, wxEXPAND, 0);
    panelTop->SetSizer(topPanelSizer);
    plotListSizery->Add(plotListLabel, 0, 0, 0);
    plotListSizery->Add(plotList, 1, wxEXPAND, 0);
    window_2_pane_2->SetSizer(plotListSizery);
    splitterSpectra->SplitVertically(panelSpectra, window_2_pane_2);
    rawDataGridSizer->Add(gridRawData, 3, wxEXPAND, 0);
    rawDataSizer->Add(20, 20, 1, 0, 0);
    rawDataSizer->Add(btnRawDataSave, 0, wxLEFT, 2);
    rawDataSizer->Add(btnRawDataClip, 0, wxLEFT, 2);
    rawDataGridSizer->Add(rawDataSizer, 0, wxTOP|wxEXPAND, 5);
    noteRaw->SetSizer(rawDataGridSizer);
    textConsoleSizer->Add(textConsoleOut, 1, wxEXPAND, 0);
    noteDataViewConsole->SetSizer(textConsoleSizer);
    noteDataView->AddPage(splitterSpectra, wxTRANS("Spec."));
    noteDataView->AddPage(noteRaw, wxTRANS("Raw"));
    noteDataView->AddPage(noteDataViewConsole, wxTRANS("Cons."));
    splitTopBottom->SplitHorizontally(panelTop, noteDataView);
    rightPanelSizer->Add(splitTopBottom, 1, wxEXPAND, 0);
    panelRight->SetSizer(rightPanelSizer);
    splitLeftRight->SplitVertically(panelLeft, panelRight);
    topSizer->Add(splitLeftRight, 1, wxEXPAND, 0);
    SetSizer(topSizer);
    Layout();
    // end wxGlade
    //
    // GTK fix hack thing. reparent window

	

	panelTop->Reparent(splitTopBottom);

	//Set the combo text
	haveSetComboCamText=false;
	comboCamera->SetValue(wxCStr(TRANS(cameraIntroString)));
	haveSetComboStashText=false;
	comboStash->SetValue(wxCStr(TRANS(stashIntroString)));

}

class threeDepictApp: public wxApp {
private:
	MainWindowFrame* MainFrame ;
	wxArrayString commandLineFiles;
	wxLocale* usrLocale;
	long language;

	void initLanguageSupport();

#ifdef DEBUG
	bool runUnitTests();
#endif


public:

    threeDepictApp() { MainFrame=0;usrLocale=0;};
    ~threeDepictApp() { if(usrLocale) delete usrLocale;}
    bool OnInit();
    virtual void OnInitCmdLine(wxCmdLineParser& parser);
    virtual bool OnCmdLineParsed(wxCmdLineParser& parser);
		 
    int FilterEvent(wxEvent &event);
#ifdef __APPLE__
    void MacOpenFile(const wxString & fileName);
    void MacReopenFile(const wxString & fileName);
#endif
};

//Check version is in place because wxT is deprecated for wx 2.9
//Command line parameter table
static const wxCmdLineEntryDesc g_cmdLineDesc [] =
{
#if wxCHECK_VERSION(2,9,0) 
	{ wxCMD_LINE_SWITCH, ("h"), ("help"), ("displays this message"),
		wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
	{ wxCMD_LINE_PARAM,  NULL, NULL, ("inputfile"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL},
#else
	{ wxCMD_LINE_SWITCH, wxT("h"), wxT("help"), wxNTRANS("displays this message"),
		wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
	{ wxCMD_LINE_PARAM,  NULL, NULL, wxNTRANS("inputfile"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL},
#endif
	//Unit testing system
#ifdef DEBUG
#if wxCHECK_VERSION(2,9,0) 
	{ wxCMD_LINE_SWITCH, ("t"), ("test"), ("run debug unit tests, returns nonzero on test failure, zero on success"), wxCMD_LINE_VAL_NONE, wxCMD_LINE_SWITCH},
#else
	{ wxCMD_LINE_SWITCH, wxT("t"), wxT("test"), wxNTRANS("run debug unit tests, returns nonzero on test failure, zero on success"), wxCMD_LINE_VAL_NONE, wxCMD_LINE_SWITCH},
#endif
#endif
	{ wxCMD_LINE_NONE }
};

IMPLEMENT_APP(threeDepictApp)

//Catching key events globally.
int threeDepictApp::FilterEvent(wxEvent& event)
{

    if ( event.GetEventType()==wxEVT_KEY_DOWN )
    {
	//Re-task the escape key globally as aborting
	//during filter tree updates
	if(  MainFrame && MainFrame->isCurrentlyUpdatingScene() &&
	    ((wxKeyEvent&)event).GetKeyCode()==WXK_ESCAPE)
	{
		wxCommandEvent cmd;
		MainFrame->OnProgressAbort( cmd);
		return true;
	}

    }

    return -1;
}

//Command line help table and setup
void threeDepictApp::OnInitCmdLine(wxCmdLineParser& parser)
{
	wxString name,version,preamble;

	name=wxCStr(PROGRAM_NAME);
	name=name+ wxT(" ");
	version=wxCStr(PROGRAM_VERSION);
	version+=wxT("\n");

	preamble=wxT("Copyright (C) 2011  3Depict team\n");
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

    if(!MainFrame->initOK())
	    return false;
  
 
    SetTopWindow(MainFrame);

#ifdef DEBUG
#ifdef __linux__
   trapfpe(); //Under Linux, enable  segfault on invalid floating point operations
#endif
#endif

#ifdef __APPLE__    
   	//Switch the working directory into the .app bundle's resources
	//directory using the absolute path
	CFBundleRef mainBundle = CFBundleGetMainBundle();
	CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
	char path[PATH_MAX], fullpath[PATH_MAX];
	if (!CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path, PATH_MAX))
	{
		// error!
	} else
	{
		CFRelease(resourcesURL);
		chdir(path);
	}
#endif


    if(commandLineFiles.GetCount())
    	MainFrame->SetCommandLineFiles(commandLineFiles);



    MainFrame->Show();
    MainFrame->fixSplitterWindow();
    return true;
}

void threeDepictApp::initLanguageSupport()
{
	language =  wxLANGUAGE_DEFAULT;

	// load language if possible, fall back to English otherwise
	if(wxLocale::IsAvailable(language))
	{
		//Wx 2.9 and above are now unicode, so locale encoding
		//conversion is deprecated.
#if (wxMAJOR_VERSION <= 2) && (wxMINOR_VERSION <= 8)
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
#endif
		usrLocale->AddCatalog(wxCStr(PROGRAM_NAME));
//		usrLocale->AddCatalog(wxT("wxstd"));

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
#elif defined(WIN32) || defined(WIN64)
			bindtextdomain( PROGRAM_NAME, paths->GetResourcesDir().mb_str(wxConvUTF8) );
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
					bind_textdomain_codeset(PROGRAM_NAME, "CP1252");
					break;
				case 65001:
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
#ifdef DEBUG
#include "filters/allFilter.h"
bool threeDepictApp::runUnitTests()
{
	bool fileWarn=false;
	//Instaniate various filters, then run their unit tests
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
				WARN(false,"Failed allocating XmL context");
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
#endif
