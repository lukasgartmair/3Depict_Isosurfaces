/*
 *	threeDepict.cpp - main program user interface and control implementation
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

//DEBUG NaN and INF
#ifdef DEBUG
#ifdef __linux__
#include <fenv.h>
#include <sys/cdefs.h>
void trapfpe () {
  feenableexcept(FE_INVALID|FE_DIVBYZERO|FE_OVERFLOW);
}
#endif
#endif

enum
{
	MESSAGE_ERROR=1,
	MESSAGE_INFO,
	MESSAGE_HINT,
	MESSAGE_NONE
};

#ifdef __WXMSW__
//Force a windows console to show for cerr/cout
#ifdef DEBUG
#include "winconsole.h"
winconsole winC;
#endif
#endif

#ifdef __APPLE__
#include "CoreFoundation/CoreFoundation.h"
#endif

#include "3Depict.h"
#include <utility>
#include "common.h"
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

//Custom program dialog windows
#include "StashDialog.h"
#include "ResDialog.h"
#include "ExportRngDialog.h"
#include "ExportPos.h"

//Program Icon
#include "art.h"

using std::pair;
using std::max;
// begin wxGlade: ::extracode

// end wxGlade

//milliseconds before clearing status bar (by invoking a status timer event)
const unsigned int STATUS_TIMER_DELAY=10000; 
//Milliseconds between querying viscontrol for needing update
const unsigned int UPDATE_TIMER_DELAY=200; 
//Seconds between autosaves
const unsigned int AUTOSAVE_DELAY=300; 

const unsigned int DEFAULT_IONS_VIEW=100;



const char *cameraIntroString= "New camera name...";
const char *stashIntroString="New stash name....";

//Name of autosave state file. MUST end in .xml
const char *AUTOSAVE_FILENAME = "autosave.xml";

//This is the dropdown matching list. This must match
//the order for comboFilters_choices, as declared in 
//MainFrame's constructor

//This enum MUST be arranged to match sorted list box contents
//comboFilters_choices[..]
enum
{
	COMBO_FILTER_BOUNDINGBOX,
	COMBO_FILTER_CLIPPING,
	COMBO_FILTER_COMPOSITION_PROFILE,
	COMBO_FILTER_DOWNSAMPLE,
	COMBO_FILTER_EXTERNALPROG,
	COMBO_FILTER_COLOUR,
	COMBO_FILTER_TRANSFORM,
	COMBO_FILTER_SPECTRUM,
	COMBO_FILTER_RANGEFILE,
	COMBO_FILTER_SPATIAL_ANALYSIS,
	COMBO_FILTER_VOXELS,
};

//Constant identifiers for binding events in wxwidgets "event table"
enum {
    
    //File menu
    ID_FILE_EXIT= wxID_ANY+1,
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

    //Options panel
    ID_CHECK_ALPHA,
    ID_CHECK_LIGHTING,
    ID_CHECK_CACHING,
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

	programmaticEvent=false;
	//Set up the program icon handler
	wxArtProvider::Push(new MyArtProvider);
	SetIcon(wxArtProvider::GetIcon(_("MY_ART_ID_ICON")));

	//Set up the drag and drop handler
	dropTarget = new FileDropTarget(this);
	SetDropTarget(dropTarget);
	
	//Set up the recently used files menu
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
	//Set up keyboard shortcuts "accellerators"
	wxAcceleratorEntry entries[1];
	entries[0].Set(0,WXK_ESCAPE,ID_PROGRESS_ABORT);
	wxAcceleratorTable accel(1, entries);
        SetAcceleratorTable(accel);

	// begin wxGlade: MainWindowFrame::MainWindowFrame
    splitLeftRight = new wxSplitterWindow(this, ID_SPLIT_LEFTRIGHT, wxDefaultPosition, wxDefaultSize, wxSP_3D|wxSP_BORDER);
    panelRight = new wxPanel(splitLeftRight, wxID_ANY);
    splitTopBottom = new wxSplitterWindow(panelRight, ID_SPLIT_TOP_BOTTOM, wxDefaultPosition, wxDefaultSize, wxSP_3D|wxSP_BORDER);
    noteDataView = new wxNotebook(splitTopBottom, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_LEFT);
    noteDataView_pane_3 = new wxPanel(noteDataView, wxID_ANY);
    noteRaw = new wxPanel(noteDataView, ID_NOTE_RAW);
    splitterSpectra = new wxSplitterWindow(noteDataView, ID_SPLIT_SPECTRA, wxDefaultPosition, wxDefaultSize, wxSP_3D|wxSP_BORDER);
    window_2_pane_2 = new wxPanel(splitterSpectra, wxID_ANY);
    panelTop = new BasicGLPane(splitTopBottom);
    panelLeft = new wxPanel(splitLeftRight, wxID_ANY);
    notebookControl = new wxNotebook(panelLeft, ID_NOTEBOOK_CONTROL, wxDefaultPosition, wxDefaultSize, wxNB_RIGHT);
    notePerformance = new wxScrolledWindow(notebookControl, ID_NOTE_PERFORMANCE, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    noteCamera = new wxScrolledWindow(notebookControl, ID_NOTE_CAMERA, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    noteData = new wxPanel(notebookControl, ID_NOTE_DATA, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    filterSplitter = new wxSplitterWindow(noteData,ID_SPLIT_FILTERPROP , wxDefaultPosition, wxDefaultSize, wxSP_3D|wxSP_BORDER);
    filterPropertyPane = new wxPanel(filterSplitter, wxID_ANY);
    //sizer_5_staticbox = new wxStaticBox(panelTop, -1, _(""));
    filterTreePane = new wxPanel(filterSplitter, wxID_ANY);
    MainFrame_Menu = new wxMenuBar();
    wxMenu* File = new wxMenu();
    File->Append(ID_FILE_OPEN, _("&Open...\tCtrl+O"), _("Open state file"), wxITEM_NORMAL);
    File->Append(ID_FILE_MERGE, _("&Merge...\tCtrl+Shift+O"), _("Merge other file"), wxITEM_NORMAL);
    
    recentFilesMenu=new wxMenu();
    recentHistory->UseMenu(recentFilesMenu);
    File->AppendSubMenu(recentFilesMenu,_("&Recent"));
    fileSave = File->Append(ID_FILE_SAVE, _("&Save\tCtrl+S"), _("Save state to file"), wxITEM_NORMAL);
    fileSave->Enable(false);
    File->Append(ID_FILE_SAVEAS, _("Save &As...\tCtrl+Shift+S"), _("Save current state to new file"), wxITEM_NORMAL);
    File->AppendSeparator();
    wxMenu* FileExport = new wxMenu();
    FileExport->Append(ID_FILE_EXPORT_PLOT, _("&Plot...\tCtrl+P"), _("Export Current Plot"), wxITEM_NORMAL);
    FileExport->Append(ID_FILE_EXPORT_IMAGE, _("&Image...\tCtrl+I"), _("Export Current 3D View"), wxITEM_NORMAL);
    FileExport->Append(ID_FILE_EXPORT_IONS, _("Ion&s...\tCtrl+N"), _("Export Ion Data"), wxITEM_NORMAL);
    FileExport->Append(ID_FILE_EXPORT_RANGE, _("Ran&ges...\tCtrl+G"), _("Export Range Data"), wxITEM_NORMAL);
    FileExport->Append(ID_FILE_EXPORT_ANIMATION, _("Ani&mation...\tCtrl+M"), _("Export Animation"), wxITEM_NORMAL);
    FileExport->Append(ID_FILE_EXPORT_PACKAGE, _("Pac&kage...\tCtrl+K"), _("Export analysis package"), wxITEM_NORMAL);

    File->AppendSubMenu(FileExport,_("&Export"));
    File->AppendSeparator();
#ifdef __APPLE__
    File->Append(ID_FILE_EXIT, _("E&xit\tCtrl+Q"), _("Exit Program"), wxITEM_NORMAL);
#else
    File->Append(ID_FILE_EXIT, _("E&xit"), _("Exit Program"), wxITEM_NORMAL);
#endif
    MainFrame_Menu->Append(File, _("&File"));
    wxMenu* wxglade_tmp_menu_1 = new wxMenu();
    wxglade_tmp_menu_1->Append(ID_VIEW_BACKGROUND, 
		    _("&Background Colour...\tCtrl+B"),_("Change background colour"));
    wxglade_tmp_menu_1->AppendSeparator(); //Separator
    checkMenuControlPane= wxglade_tmp_menu_1->Append(ID_VIEW_CONTROL_PANE, 
		    _("&Control Pane\tF3"), _("Toggle left control pane"), wxITEM_CHECK);
    checkMenuControlPane->Check();
    checkMenuRawDataPane= wxglade_tmp_menu_1->Append(ID_VIEW_RAW_DATA_PANE, 
		    _("&Raw Data Pane\tF4"), _("Toggle raw data  pane (bottom)"), wxITEM_CHECK);
    checkMenuRawDataPane->Check();
    checkMenuSpectraList=wxglade_tmp_menu_1->Append(ID_VIEW_SPECTRA, _("&Plot List\tF5"),_("Toggle plot list"), wxITEM_CHECK);
    checkMenuSpectraList->Check();

    wxglade_tmp_menu_1->AppendSeparator(); //Separator
    wxMenu* viewPlot= new wxMenu();
    checkViewLegend=viewPlot->Append(ID_VIEW_PLOT_LEGEND,_("&Legend\tCtrl+L"),_("Toggle Legend display"),wxITEM_CHECK);
    checkViewLegend->Check();
    wxglade_tmp_menu_1->AppendSubMenu(viewPlot,_("&Plot..."));
    checkViewWorldAxis=wxglade_tmp_menu_1->Append(ID_VIEW_WORLDAXIS,_("&Axis\tCtrl+Shift+I"),_("Toggle World Axis display"),wxITEM_CHECK);
    checkViewWorldAxis->Check();
    
    wxglade_tmp_menu_1->AppendSeparator(); //Separator
    checkViewFullscreen=wxglade_tmp_menu_1->Append(ID_VIEW_FULLSCREEN, _("&Fullscreen\tF11"),_("Toggle fullscreen"),wxITEM_CHECK);

    wxMenu *Edit = new wxMenu();
    editUndoMenuItem = Edit->Append(ID_EDIT_UNDO,_("&Undo\tCtrl+Z"));
    editUndoMenuItem->Enable(false);
    editRedoMenuItem = Edit->Append(ID_EDIT_REDO,_("&Redo\tCtrl+Y"));
    editRedoMenuItem->Enable(false);
    MainFrame_Menu->Append(Edit, _("&Edit"));


    MainFrame_Menu->Append(wxglade_tmp_menu_1, _("&View"));
    wxMenu* Help = new wxMenu();
    Help->Append(ID_HELP_HELP, _("&Help...\tCtrl+H"), _("Show help files and documentation"), wxITEM_NORMAL);
    Help->Append(ID_HELP_CONTACT, _("&Contact..."), _("Open contact page"), wxITEM_NORMAL);
    Help->AppendSeparator();
    Help->Append(ID_HELP_ABOUT, _("&About..."), _("Information about this program"), wxITEM_NORMAL);
    MainFrame_Menu->Append(Help, _("&Help"));
    SetMenuBar(MainFrame_Menu);
    lblSettings = new wxStaticText(noteData, wxID_ANY, _("Stashed Filters"));
    const wxString *comboStash_choices = NULL;
    comboStash = new wxComboBox(noteData, ID_COMBO_STASH, wxT(""), wxDefaultPosition, wxDefaultSize, 0, comboStash_choices, wxCB_DROPDOWN|wxCB_SORT|wxTE_PROCESS_ENTER );
    btnStashManage = new wxButton(noteData, ID_BTN_STASH_MANAGE, _("..."),wxDefaultPosition,wxSize(28,28));
    filteringLabel = new wxStaticText(noteData, wxID_ANY, _("Data Filtering"));
    const unsigned int FILTER_DROP_COUNT=11;
    const wxString comboFilters_choices[FILTER_DROP_COUNT] = {
        _("Bounding Box"),
        _("Clipping"),
        _("Compos. Profiles"),
        _("Downsampling"),
	_("Extern. Prog."),
        _("Ion Colour"),
        _("Ion Transform"),
	_("Mass Spectrum"),
        _("Range File"),
	_("Spat. Analysis"),
	_("Voxelisation"),
    };
    comboFilters = new wxComboBox(filterTreePane, ID_COMBO_FILTER, wxT(""), wxDefaultPosition, wxDefaultSize, FILTER_DROP_COUNT, comboFilters_choices, wxCB_DROPDOWN|wxCB_READONLY|wxCB_SORT);
    treeFilters = new wxTreeCtrl(filterTreePane, ID_TREE_FILTERS, wxDefaultPosition, wxDefaultSize, wxTR_HAS_BUTTONS|wxTR_NO_LINES|wxTR_HIDE_ROOT|wxTR_DEFAULT_STYLE|wxSUNKEN_BORDER|wxTR_EDIT_LABELS);
    lastRefreshLabel = new wxStaticText(filterTreePane, wxID_ANY, _("Last Outputs"));
    listLastRefresh = new wxListCtrl(filterTreePane, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT|wxSUNKEN_BORDER);
    checkAutoUpdate = new wxCheckBox(filterTreePane, ID_CHECK_AUTOUPDATE, _("Auto Refresh"));
    refreshButton = new wxButton(filterTreePane, wxID_REFRESH, wxEmptyString);
    btnFilterTreeExpand= new wxButton(filterTreePane, ID_BTN_EXPAND, _("▼"),wxDefaultPosition,wxSize(30,30));
    btnFilterTreeCollapse = new wxButton(filterTreePane, ID_BTN_COLLAPSE, _("▲"),wxDefaultPosition,wxSize(30,30));
    propGridLabel = new wxStaticText(filterPropertyPane, wxID_ANY, _("Filter settings"));
    gridFilterProperties = new wxPropertyGrid(filterPropertyPane, ID_GRID_FILTER_PROPERTY);
    label_2 = new wxStaticText(noteCamera, wxID_ANY, _("Camera Name"));
    const wxString *comboCamera_choices = NULL;
    comboCamera = new wxComboBox(noteCamera, ID_COMBO_CAMERA, wxT(""), wxDefaultPosition, wxDefaultSize, 0, comboCamera_choices, wxCB_DROPDOWN|wxTE_PROCESS_ENTER );
    buttonRemoveCam = new wxButton(noteCamera, wxID_REMOVE, wxEmptyString);
    static_line_1 = new wxStaticLine(noteCamera, wxID_ANY);
    gridCameraProperties = new wxPropertyGrid(noteCamera, ID_GRID_CAMERA_PROPERTY);
    checkAlphaBlend = new wxCheckBox(notePerformance,ID_CHECK_ALPHA , _("Smooth && translucent objects"));
    checkAlphaBlend->SetValue(true);
    checkLighting = new wxCheckBox(notePerformance, ID_CHECK_LIGHTING, _("Enable lighting"));
    checkLighting->SetValue(true);
    checkCaching = new wxCheckBox(notePerformance, ID_CHECK_CACHING, _("Enable filter caching"));
    checkCaching->SetValue(true);
    label_8 = new wxStaticText(notePerformance, wxID_ANY, _("Max. Ram usage (%)"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    spinCachePercent = new wxSpinCtrl(notePerformance, ID_SPIN_CACHEPERCENT, wxT("50"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 100);
    panelView = new wxPanel(panelTop, ID_PANEL_VIEW);
    panelSpectra = new MathGLPane(splitterSpectra, wxID_ANY);
    label_5 = new wxStaticText(window_2_pane_2, wxID_ANY, _("Plot List"));
    const wxString *list_box_1_choices = NULL;
    plotList = new wxListBox(window_2_pane_2, ID_LIST_PLOTS, wxDefaultPosition, wxDefaultSize, 0, list_box_1_choices, wxLB_MULTIPLE|wxLB_NEEDED_SB|wxLB_SORT);
    gridRawData = new CopyGrid(noteRaw, ID_GRID_RAW_DATA);
    btnRawDataSave = new wxButton(noteRaw, wxID_SAVE, wxEmptyString);
    btnRawDataClip = new wxButton(noteRaw, wxID_COPY, wxEmptyString);
    textConsoleOut = new wxTextCtrl(noteDataView_pane_3, wxID_ANY, wxEmptyString,wxDefaultPosition,
		    wxDefaultSize,wxTE_MULTILINE|wxTE_READONLY);


    //Tuned splitter parameters.
    filterSplitter->SetMinimumPaneSize(80);
    filterSplitter->SetSashGravity(0.8);
    splitLeftRight->SetSashGravity(0.15);
    splitTopBottom->SetSashGravity(0.85);
    splitterSpectra->SetSashGravity(0.82);

    listLastRefresh->InsertColumn(0,_("Type"));
    listLastRefresh->InsertColumn(1,_("Num"));

    MainFrame_statusbar = CreateStatusBar(3, 0);
 
    panelTop->setParentStatus(MainFrame_statusbar,statusTimer,STATUS_TIMER_DELAY);
    panelTop->clearCameraUpdates();
    set_properties();
    do_layout();
    // end wxGlade
    //


	//Try to load config file. If we can't no big deal.
    	if(configFile.read())
	{
		std::vector<std::string> strVec;

		configFile.getRecentFiles(strVec);

		for(unsigned int ui=0;ui<strVec.size();ui++)
			recentHistory->AddFileToHistory(wxStr(strVec[ui]));
	}
	
	//Attempt to load the auto-save file, if it exists
	//-----------------

	wxStandardPaths *paths = new wxStandardPaths;

	wxString filePath = paths->GetDocumentsDir()+wxCStr("/.")+wxCStr(PROGRAM_NAME);
	filePath+=wxCStr("/") + wxCStr(AUTOSAVE_FILENAME);


	if(wxFileExists(filePath))
	{
		//Well, looky here, looks like we got an autosave
		wxMessageDialog *wxD  =new wxMessageDialog(this,
					_("An auto-save file was found, would you like to restore it?.") 
					,wxT("Autosave"),wxYES_NO|wxICON_QUESTION|wxYES_DEFAULT );

		if(wxD->ShowModal()== wxID_YES)
		{
			if(!loadFile(filePath))
				statusMessage("Unable to load autosave file..",MESSAGE_ERROR);
			else
			{
				requireFirstUpdate=true;
				//Prevent the program from allowing save menu usage
				//into autosave file
				currentFile.clear();
				fileSave->Enable(false);		
			}
		}
	}

	delete paths;
	//-----------------

	   


	updateTimer->Start(UPDATE_TIMER_DELAY,wxTIMER_CONTINUOUS);
	autoSaveTimer->Start(AUTOSAVE_DELAY*1000,wxTIMER_CONTINUOUS);
}

MainWindowFrame::~MainWindowFrame()
{

	//Delete and stop all the timers.
	delete statusTimer;
	delete progressTimer;
	delete updateTimer;
	delete autoSaveTimer;
}

BEGIN_EVENT_TABLE(MainWindowFrame, wxFrame)
    EVT_GRID_CMD_EDITOR_SHOWN(ID_GRID_FILTER_PROPERTY,MainWindowFrame::OnFilterGridCellEditorShow)
    EVT_GRID_CMD_EDITOR_SHOWN(ID_GRID_CAMERA_PROPERTY,MainWindowFrame::OnCameraGridCellEditorShow)
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
    EVT_SPINCTRL(ID_SPIN_CACHEPERCENT, MainWindowFrame::OnCacheRamUsageSpin)
    EVT_COMBOBOX(ID_COMBO_CAMERA, MainWindowFrame::OnComboCamera)
    EVT_COMBOBOX(ID_COMBO_FILTER, MainWindowFrame::OnComboFilter)
    EVT_TEXT_ENTER(ID_COMBO_FILTER, MainWindowFrame::OnComboFilterEnter)
    EVT_BUTTON(ID_BTN_STASH_MANAGE, MainWindowFrame::OnButtonStashDialog)
    EVT_BUTTON(wxID_REMOVE, MainWindowFrame::OnButtonRemoveCam)
    EVT_LISTBOX(ID_LIST_PLOTS, MainWindowFrame::OnSpectraListbox)
    EVT_CLOSE(MainWindowFrame::OnClose)
    EVT_TREE_END_LABEL_EDIT(ID_TREE_FILTERS,MainWindowFrame::OnTreeEndLabelEdit)
   
    // end wxGlade
END_EVENT_TABLE();




void MainWindowFrame::OnFileOpen(wxCommandEvent &event)
{
	//Do not allow any action if a scene update is in progress
	if(currentlyUpdatingScene)
		return;

	//Load a file, either a state file, or a new pos file
	wxFileDialog *wxF = new wxFileDialog(this,wxT("Select Data or State File..."), wxT(""),
		wxT(""),wxT("3Depict file (*.xml, *.pos)|*.xml;*.pos|POS File (*.pos)|*.pos|XML State File (*.xml)|*.xml|All Files (*)|*"),wxOPEN|wxFILE_MUST_EXIST);

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
	
	doSceneUpdate();
	//If we are using the default camera,
	//move it to make sure that it is visible
	if(visControl.numCams() == 1)
		visControl.ensureSceneVisible(3);

	statusMessage("Loaded file.",MESSAGE_INFO);
	
	panelTop->Refresh();
	
	wxF->Destroy();
}

void MainWindowFrame::OnFileMerge(wxCommandEvent &event)
{
	//Do not allow any action if a scene update is in progress
	if(currentlyUpdatingScene)
		return;

	//Load a file, either a state file, or a new pos file
	wxFileDialog *wxF = new wxFileDialog(this,wxT("Select Data or State File..."), wxT(""),
		wxT(""),wxT("3Depict file (*.xml, *.pos)|*.xml;*.pos|POS File (*.pos)|*.pos|XML State File (*.xml)|*.xml|All Files (*)|*"),wxOPEN|wxFILE_MUST_EXIST);

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

void MainWindowFrame::OnDropFiles(const wxArrayString &files)
{	

	textConsoleOut->Clear();
	wxMouseState wxm = wxGetMouseState();

	for(unsigned int ui=0;ui<files.Count();ui++)
	{
		//If command down, load first file using this,
		//then merge the rest
		if(!ui)	
			loadFile(files[ui],!wxm.CmdDown());
		else
			loadFile(files[ui],true);
	}


	if(!wxm.CmdDown() && files.Count())
	{
#ifdef __APPLE__    
		statusMessage("Tip: You can use ⌘ (command) to merge",MESSAGE_HINT);
#else
		statusMessage("Tip: You can use ctrl to merge",MESSAGE_HINT);
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
	
	//Split the filename into chunks. path, volume, name and extention
	//the format of this is OS dependant, but wxWidgets can deal with this.
	wxFileName fname;
	wxString volume,path,name,ext;
	bool hasExt;
	fname.SplitPath(fileStr,&volume,
			&path,&name,&ext, &hasExt);

	//Test the extention to determine what we will do
	//FIXME: This is really lazy, and I should use something like libmagic.
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
						_("Error loading state file.\nSee console for more info.") 
						,wxT("Load error"),wxOK|wxICON_ERROR);
			wxD->ShowModal();
			wxD->Destroy();
			return false;
		}


		if(visControl.hasHazardousContents())
		{
			wxMessageDialog *wxD  =new wxMessageDialog(this,
						_("This state file contains filters that can be unsafe to run\nDo you wish to remove these before continuing?.") 
						,wxT("Security warning"),wxYES_NO|wxICON_WARNING|wxYES_DEFAULT );

			if(wxD->ShowModal()!= wxID_NO)
				visControl.makeFiltersSafe();

		}

		//Update the background colour
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
			//Do not delete as this will be deleted by wx..
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
			gridCameraProperties->clear();

	

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
			ASSERT(comboStash->GetClientObject(comboStash->GetCount()-1));
		}

	}
	else
	{
		//Load the file as if it were a pos file.
		unsigned int err;
		

		//No need to refresh scene, this is done internally	
		if((err = visControl.LoadIonSet(dataFile))) 
		{
			std::string errString;
			errString = posErrStrings[err];
			wxMessageDialog *wxD  =new wxMessageDialog(this,wxStr(errString)
							,wxT("Load error"),wxOK|wxICON_ERROR);
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
			//Didn't load ?  we don't want it.
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
		errString="Unable to save. Check output destination can be written to.";
		
		wxMessageDialog *wxD  =new wxMessageDialog(this,wxStr(errString)
						,wxT("Save error"),wxOK|wxICON_ERROR);
		wxD->ShowModal();
		wxD->Destroy();
	}
	else
	{
		fileSave->Enable(true);

		dataFile=std::string("Saved state: ") + dataFile;
		statusMessage(dataFile.c_str(),MESSAGE_INFO);

	}

}

void MainWindowFrame::OnFileExportPlot(wxCommandEvent &event)
{

	if(!panelSpectra->getNumVisible())
	{
		std::string errString;
		errString="No plot available. Please create a plot before exporting.";
		wxMessageDialog *wxD  =new wxMessageDialog(this,wxStr(errString)
						,wxT("Unable to save"),wxOK|wxICON_ERROR);
		wxD->ShowModal();

		wxD->Destroy();
		return;
	}

	wxFileDialog *wxF = new wxFileDialog(this,wxT("Save plot..."), wxT(""),
		wxT(""),wxT("By Ext. (svg,png)|*.svg;*.png|Scalable Vector Graphics File (*.svg)|*.svg|PNG File (*.png)|*.png|All Files (*)|*"),wxSAVE);

	if( (wxF->ShowModal() == wxID_CANCEL))
		return;

	std::string dataFile = stlStr(wxF->GetPath());
	
	//Split the filename into chunks. path, volume, name and extention
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

	//Try to save the file (if we recognise the extention)
	if(strExt == "svg")
		errCode=panelSpectra->saveSVG(dataFile);
	else if (strExt == "png")
	{
		//Show a resolution chooser dialog
		ResDialog d(this,wxID_ANY,_("Choose resolution"));

		if(d.ShowModal() == wxID_CANCEL)
			return;

		
		errCode=panelSpectra->savePNG(dataFile,d.getWidth(),d.getHeight());
	
	}	
	else
	{
		std::string errString;
		errString="Unknown file extention. Please use \"svg\" or \"png\"";
		wxMessageDialog *wxD  =new wxMessageDialog(this,wxStr(errString)
						,wxT("Unable to save"),wxOK|wxICON_ERROR);
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
						,wxT("Save error"),wxOK|wxICON_ERROR);
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
	wxFileDialog *wxF = new wxFileDialog(this,wxT("Save Image..."), wxT(""),
		wxT(""),wxT("PNG File (*.png)|*.png|All Files (*)|*"),wxSAVE);

	if( (wxF->ShowModal() == wxID_CANCEL))
		return;

	std::string dataFile = stlStr(wxF->GetPath());
	
	//Show a resolution chooser dialog
	ResDialog d(this,wxID_ANY,_("Choose resolution"));

	//Use the current res as the dialog default
	int w,h;
	panelTop->GetClientSize(&w,&h);
	d.setRes(w,h);

	//Show dialog, skip save if user cancels dialgo
	if(d.ShowModal() == wxID_CANCEL)
		return;

	if((d.getWidth() < w && d.getHeight() > h) ||
		(d.getWidth() > w && d.getHeight() < h))
	{
		wxMessageDialog *wxD  =new wxMessageDialog(this,
				wxT("Limitation on the screenshot dimension; please ensure that both width and height exceed the initial values,\n or that they are smaller than the intial values.\n If this bothers you, please submit a bug."),
				wxT("Program limitation"),wxOK|wxICON_ERROR);

		wxD->ShowModal();

		wxD->Destroy();

		return;
	}


	bool saveOK=panelTop->saveImage(d.getWidth(),d.getHeight(),dataFile.c_str());

	if(!saveOK)
	{
		std::string errString;
		errString="Unable to save. Check output destination can be written to.";
		
		wxMessageDialog *wxD  =new wxMessageDialog(this,wxStr(errString)
						,wxT("Save error"),wxOK|wxICON_ERROR);
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
	wxFileDialog *wxF = new wxFileDialog(this,wxT("Save Image..."), wxT(""),
		wxT(""),wxT("PNG File (*.png)|*.png|All Files (*)|*"),wxSAVE);

	if( (wxF->ShowModal() == wxID_CANCEL))
		return;

	
	//Show a resolution chooser dialog
	ResDialog d(this,wxID_ANY,_("Choose resolution"));

	//Use the current res as the dialog default
	int w,h;
	panelTop->GetClientSize(&w,&h);
	d.setRes(w,h);

	//Show dialog, skip save if user cancels dialgo
	if(d.ShowModal() == wxID_CANCEL)
	{
		wxF->Destroy();
		return;
	}

	if((d.getWidth() < w && d.getHeight() > h) ||
		(d.getWidth() > w && d.getHeight() < h) )
	{
		wxMessageDialog *wxD  =new wxMessageDialog(this,
				wxT("Limitation on the screenshot dimension; please ensure that both width and height exceed the initial values,\n or that they are smaller than the intial values.\n If this bothers, please submit a bug."),
				wxT("Program limitation"),wxOK|wxICON_ERROR);

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
		ext=_("png");

	///TODO: This is nasty and hackish. We should present a nice,
	//well layed out dialog for frame count (show angular increment) 
	wxTextEntryDialog *teD = new wxTextEntryDialog(this,_("Number of frames"),_("Frame count"),
						_("180"),(long int)wxOK|wxCANCEL);

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
		errString="Unable to save. Check output destination can be written to.";
		
		wxMessageDialog *wxD  =new wxMessageDialog(this,wxStr(errString)
						,wxT("Save error"),wxOK|wxICON_ERROR);
		wxD->ShowModal();
		wxD->Destroy();
	}
	else
	{
		std::string dataFile = stlStr(wxF->GetPath());
		dataFile=std::string("Saved 3D View :") + dataFile;
		statusMessage(dataFile.c_str(),MESSAGE_INFO);
	}
	wxF->Destroy();

}

void MainWindowFrame::OnFileExportPackage(wxCommandEvent &event)
{
	if(!treeFilters->GetCount())
	{
		statusMessage("No filters means no data to export",MESSAGE_ERROR);
		return;

	}

	//This could be nicer, or reordered
	wxTextEntryDialog *wxT = new wxTextEntryDialog(this,_("Package name"),
					_("Package directory name"),_(""),wxOK|wxCANCEL);

	wxT->SetValue(_("AnalysisPackage"));

	if(wxT->ShowModal() == wxID_CANCEL)
	{
		wxT->Destroy();
		return;
	}


	//Pop up a directory dialog, to choose the base path for the new folder
	wxDirDialog *wxD = new wxDirDialog(this);

	wxMessageDialog *wxMesD  =new wxMessageDialog(this,wxT("Package folder already exists, won't overwrite.")
					,wxT("Not available"),wxICON_ERROR);

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
		wxMessageDialog *wxMesD  =new wxMessageDialog(this,wxT("Package folder creation failed\ncheck writing to this location is possible.")
						,wxT("Folder creation failed"),wxICON_ERROR);

		wxMesD->ShowModal();
		wxMesD->Destroy();

		return;
	}


	//OK, so the folder eixsts, lets make the XML state file
	std::string dataFile = string(stlStr(folder)) + "state.xml";

	std::map<string,string> fileMapping;
	//Try to save the viscontrol state
	if(!visControl.saveState(dataFile.c_str(),fileMapping,true))
	{
		std::string errString;
		errString="Unable to save. Check output destination can be written to.";
		
		wxMessageDialog *wxD  =new wxMessageDialog(this,wxStr(errString)
						,wxT("Save error"),wxOK|wxICON_ERROR);
		wxD->ShowModal();
		wxD->Destroy();
	}
	else
	{
		//Copy the files in the mapping
		wxProgressDialog *wxP = new wxProgressDialog(_("Copying"),
					_("Copying referenced files"),fileMapping.size());

		wxP->Show();
		for(map<string,string>::iterator it=fileMapping.begin();
				it!=fileMapping.end();++it)
		{
			if(!wxCopyFile(wxStr(it->second),folder+wxStr(it->first)))
			{
				wxMessageDialog *wxD  =new wxMessageDialog(this,_("Error copying file")
								,wxT("Save error"),wxOK|wxICON_ERROR);
				wxD->ShowModal();
				wxD->Destroy();
				wxP->Destroy();

				return;
			}
			wxP->Update();
		}

		wxP->Destroy();


		wxString s;
		s=wxString(_("Saved package: ")) + folder;
		statusMessage(stlStr(s),MESSAGE_INFO);
	}
	
	wxD->Destroy();
}

void MainWindowFrame::OnFileExportIons(wxCommandEvent &event)
{
	if(!treeFilters->GetCount())
	{
		statusMessage("No filters means no data to export",MESSAGE_ERROR);
		return;

	}
	//Load up the export dialog
	ExportPosDialog *exportDialog=new ExportPosDialog(this,wxID_ANY,_("Export"));
	
	//create a file chooser for later.
	wxFileDialog *wxF = new wxFileDialog(this,wxT("Save pos..."), wxT(""),
		wxT(""),wxT("POS Data (*.pos)|*.pos|All Files (*)|*"),wxSAVE);


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
	
	std::string dataFile = stlStr(wxF->GetPath());

	//Retreive the ion streams that we need to save
	vector<const FilterStreamData *> exportVec;
	exportDialog->getExportVec(exportVec);
	

	//write the ion streams to disk
	if(visControl.exportIonStreams(exportVec,dataFile))
	{
		std::string errString;
		errString="Unable to save. Check output destination can be written to.";
		
		wxMessageDialog *wxD  =new wxMessageDialog(this,wxStr(errString)
						,wxT("Save error"),wxOK|wxICON_ERROR);
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
	//filtersream pointers it generated	
	exportDialog->Destroy();
	//Need this to reset the ID values
	visControl.updateWxTreeCtrl(treeFilters);
	visControl.setRefreshed();
	
}

void MainWindowFrame::OnFileExportRange(wxCommandEvent &event)
{

	if(!treeFilters->GetCount())
	{
		statusMessage("No filters means no data to export",
				MESSAGE_ERROR);
		return;
	}
	ExportRngDialog *rngDialog = new ExportRngDialog(this,wxID_ANY,_("Export Ranges"),
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
	wxFileDialog *wxF = new wxFileDialog(this,wxT("Save state..."), wxT(""),
		wxT(""),wxT("XML state file (*.xml)|*.xml|All Files (*)|*"),wxSAVE);

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
		wxMessageDialog *wxD  =new wxMessageDialog(this,wxT("File already exists, overwrite?")
						,wxT("Overwrite?"),wxOK|wxCANCEL|wxICON_QUESTION);
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
		

	std::map<string,string> dummyMap;
	//Try to save the viscontrol state
	if(!visControl.saveState(dataFile.c_str(),dummyMap))
	{
		std::string errString;
		errString="Unable to save. Check output destination can be written to.";
		
		wxMessageDialog *wxD  =new wxMessageDialog(this,wxStr(errString)
						,wxT("Save error"),wxOK|wxICON_ERROR);
		wxD->ShowModal();
		wxD->Destroy();
	}
	else
	{
		currentFile = wxF->GetPath();
		fileSave->Enable(true);

		dataFile=std::string("Saved state: ") + dataFile;
		statusMessage(dataFile.c_str(),MESSAGE_INFO);
	}
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
		visControl.updateFilterPropertyGrid(gridFilterProperties,
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
		visControl.updateFilterPropertyGrid(gridFilterProperties,
						((wxTreeUint *)parentData)->value);

	}
	else
	{
		gridFilterProperties->clear();
		updateLastRefreshBox();
	}



	doSceneUpdate();
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
	
		}
	}
	else
	{
		if(splitLeftRight->IsSplit())
			splitLeftRight->Unsplit(panelLeft);
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
	
		}
	}
	else
	{
		if(splitTopBottom->IsSplit())
			splitTopBottom->Unsplit();
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
	
		}
	}
	else
	{
		if(splitterSpectra->IsSplit())
			splitterSpectra->Unsplit();
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
	std::string helpFileLocation("http://threedepict.sourceforge.net/documentation.html");
	wxLaunchDefaultBrowser(wxStr(helpFileLocation),wxBROWSER_NEW_WINDOW);

	statusMessage("Opening help in external web browser",MESSAGE_INFO);
}

void MainWindowFrame::OnHelpContact(wxCommandEvent &event)
{
	std::string contactFileLocation("http://threedepict.sourceforge.net/contact.html");
	wxLaunchDefaultBrowser(wxStr(contactFileLocation),wxBROWSER_NEW_WINDOW);

	statusMessage("Opening contact page in external web browser",MESSAGE_INFO);
}

void MainWindowFrame::OnButtonStashDialog(wxCommandEvent &event)
{
	std::vector<std::pair<std::string,unsigned int > > stashList;
	visControl.getStashList(stashList);
#ifdef DEbUG

	ASSERT(comboStash->GetCount() == stashList.size())
#endif
	if(!stashList.size())
	{
		statusMessage("No filter stashes to edit.",MESSAGE_ERROR);
		return;
	}

	StashDialog *s = new StashDialog(this,wxID_ANY,_("Filter Stashes"));
	s->setVisController(&visControl);
	s->ready();
	s->ShowModal();

	s->Destroy();

	//Stash list may have changed. force update
	stashList.clear();
	visControl.getStashList(stashList);

	comboStash->Clear();
	for(unsigned int ui=0;ui<stashList.size(); ui++)
	{
		wxListUint *u;
		u = new wxListUint(stashList[ui].second);
		comboStash->Append(wxStr(stashList[ui].first),(wxClientData *)u);
		ASSERT(comboStash->GetClientObject(comboStash->GetCount()-1));
	}

	
}


void MainWindowFrame::OnHelpAbout(wxCommandEvent &event)
{
	wxAboutDialogInfo info;
	info.SetName(wxCStr(PROGRAM_NAME));
	info.SetVersion(wxCStr(PROGRAM_VERSION));
	info.SetDescription(_("Quick and dirty analysis for point data.")); 
	info.SetWebSite(_("https://sourceforge.net/apps/phpbb/threedepict/"));

	info.AddDeveloper(_("D. Haley"));	
	info.AddDeveloper(_("A. Ceguerra"));	
	//GNU GPL v3
	info.SetCopyright(_T("Copyright (C) 2010 3Depict team\n This software is licenced under the GPL Version 3.0 or later\n This program comes with ABSOLUTELY NO WARRANTY.\nThis is free software, and you are welcome to redistribute it\nunder certain conditions; Please see the file COPYING in the program directory for details"));	

	info.AddArtist(_T("Thanks go to all who have developed the libraries that I use, which make this program possible.\n This includes the wxWidgets team, Alexy Balakin (MathGL), the FTGL and freetype people, the GNU Scientific Library contributors, the tree.h guy (Kasper Peeters)  and more."));


	wxArrayString s;
	s.Add(wxString(_("Compiled with wx Version: " )) + 
			wxString(wxSTRINGIZE_T(wxVERSION_STRING)));
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
		statusMessage("Press enter to store new stash",MESSAGE_HINT);
	else
	{
		//The combo generates an ontext event when a string
		//is selected (yeah, I know, wierd..) Block this case.
		if(comboStash->GetSelection() != n)
			statusMessage("Press enter to restore stash",MESSAGE_HINT);
	}
}

void MainWindowFrame::OnComboStashEnter(wxCommandEvent &event)
{
	//The user has pressed enterk, in the combo box. If there is an existing stash of this name,
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
			statusMessage("Unable to create stash, selection invalid",MESSAGE_ERROR);
			return;
		}

		
		//Tree data contains unique identifier for vis control to do matching
		wxTreeItemData *tData=treeFilters->GetItemData(id);

		unsigned int n =visControl.stashFilters(((wxTreeUint *)tData)->value,userText.c_str());
		n=comboStash->Append(wxStr(userText),(wxClientData *)new wxListUint(n));
		ASSERT(comboStash->GetClientObject(n));
		
		statusMessage("Created new filter tree stash",MESSAGE_INFO);

	}
	else
	{
		//FOund it. Restore te existing stash
		//Find the stash associated with this item
		int index;
		index= comboStash->FindString(comboStash->GetValue());
		wxListUint *l;
		l =(wxListUint*)comboStash->GetClientObject(index);
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
		comboStash->SetValue(_(""));
	}

	//clear the text in the combo box
	comboStash->SetValue(_(""));
}


void MainWindowFrame::OnComboStash(wxCommandEvent &event)
{
	//Find the stash associated with this item
	wxListUint *l;
	l =(wxListUint*)comboStash->GetClientObject(comboStash->GetSelection());
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
	comboStash->SetValue(_(""));
}



void MainWindowFrame::OnTreeEndDrag(wxTreeEvent &event)
{
	//Should be enfoced by ::Allow() in start drag.
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
			//then copy. otherwise move
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

	visControl.updateFilterPropertyGrid(gridFilterProperties,
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
		//retreive the current active filter
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
				index=listLastRefresh->InsertItem(0,wxCStr(STREAM_NAMES[ui]));
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
		statusMessage("Moving - Hold ⌘ (command) to copy",MESSAGE_HINT);
#else
		statusMessage("Moving - Hold control to copy",MESSAGE_HINT);
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
				//wish to deferefence the treeItem, otherwise the treeitem
				//referred to by the pointer is destroyed, and invalid
				visControl.updateFilterPropertyGrid(gridFilterProperties,
								((wxTreeUint *)parentData)->value);
				visControl.updateWxTreeCtrl(treeFilters,parentFilter);
			}
			else
			{
				if(parent.IsOk())
					visControl.updateWxTreeCtrl(treeFilters);
			}
	
			//Force a scene update, independant of if autoUpdate is enabled. 
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

	if(tId.IsOk())
	{
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
		visControl.updateFilterPropertyGrid(gridFilterProperties,filterId);
	}
	programmaticEvent=false;
	editUndoMenuItem->Enable(visControl.getUndoSize());
	editRedoMenuItem->Enable(visControl.getRedoSize());
	Layout();
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
	l =(wxListUint*)  comboCamera->GetClientObject(n);
	if(!l)
	{
		programmaticEvent=false;
		return;
	}

	size_t cameraId;
	cameraId = l->value;

	int row=event.GetRow();
	if(visControl.setCamProperties(cameraId,gridCameraProperties->getSetFromRow(row),
					gridCameraProperties->getKeyFromRow(row),value))
		visControl.updateCamPropertyGrid(gridCameraProperties,cameraId);
	else
		event.Veto();

	panelTop->Refresh(true);
	programmaticEvent=false;
}

void MainWindowFrame::OnComboCameraText(wxCommandEvent &event)
{
	std::string s;
	s=stlStr(comboCamera->GetValue());
	if(!s.size())
		return;
	
	int n = comboCamera->FindString(comboCamera->GetValue());
	
	if ( n== wxNOT_FOUND ) 
		statusMessage("Press enter to store new camera",MESSAGE_HINT);
	else
		statusMessage("Press enter to restore camera",MESSAGE_HINT);
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
		l =(wxListUint*)  comboCamera->GetClientObject(comboCamera->GetSelection());
		visControl.setCam(l->value);
		
		std::string s = std::string("Restored camera: " ) +stlStr(comboCamera->GetValue());	
		
		statusMessage(s.c_str(),MESSAGE_INFO);
		
		//refresh the camera property grid
		visControl.updateCamPropertyGrid(gridCameraProperties ,l->value);

		//force redraw in 3D pane
		panelTop->Refresh(false);
		return ;
	}

	//Create a new camera for the scene.
	unsigned int u=visControl.addCam(camName);

	//Do not delete as this will be deleted by wx..
	comboCamera->Append(comboCamera->GetValue(),(wxClientData *)new wxListUint(u));	

	std::string s = std::string("Stored camera: " ) +stlStr(comboCamera->GetValue());	
	statusMessage(s.c_str(),MESSAGE_INFO);

	visControl.setCam(u);
	visControl.updateCamPropertyGrid(gridCameraProperties,u);
	panelTop->Refresh(false);
}

void MainWindowFrame::OnComboCamera(wxCommandEvent &event)
{
	//Set the active camera
	wxListUint *l;
	l =(wxListUint*)  comboCamera->GetClientObject(comboCamera->GetSelection());
	visControl.setCam(l->value);



	visControl.updateCamPropertyGrid(gridCameraProperties,l->value);

	std::string s = std::string("Restored camera: " ) +stlStr(comboCamera->GetValue());	
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
		l =(wxListUint*)  comboCamera->GetClientObject(n);
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
			statusMessage("Select an item from the filter tree before choosing a new filter");
		else
			statusMessage("Load data source (file->open) before choosing a new filter");
		return;
	}

	bool haveErr=false;
	switch(event.GetSelection())
	{
		case COMBO_FILTER_DOWNSAMPLE:
		{
			IonDownsampleFilter *f = new IonDownsampleFilter;
			f->setControlledOut(true);
			f->setFilterCount(DEFAULT_IONS_VIEW);
			//Tree data contains unique identifier for vis control to do matching
			wxTreeItemData *tData=treeFilters->GetItemData(id);

			ASSERT(tData);
			visControl.addFilter(f,((wxTreeUint *)tData)->value);

			//Rebuild tree control
			visControl.updateWxTreeCtrl(treeFilters,f);
			break;
		}
		case COMBO_FILTER_RANGEFILE:
		{
			///Prompt user for file
			wxFileDialog *wxF = new wxFileDialog(this,wxT("Select RNG File..."),wxT(""),wxT(""),
					wxT("Range Files (*rng; *env; *rrng)|*rng;*env;*rrng|RNG File (*.rng)|*.rng|Environment File (*.env)|*.env|RRNG Files (*.rrng)|*.rrng|All Files (*)|*"),wxOPEN|wxFILE_MUST_EXIST);
			

			if( (wxF->ShowModal() == wxID_CANCEL))
			{
				haveErr=true;
				break;
			}

			//Load rangefile &  construct filter
			RangeFileFilter *f = new RangeFileFilter;

			//Split the filename into chunks. path, volume, name and extention
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
				statusMessage("Unknown file extention",MESSAGE_ERROR);
				return;
			}
				

			std::string dataFile = stlStr(wxF->GetPath());
			f->setRangeFileName(dataFile);


			unsigned int errCode;
			errCode = f->updateRng();
			std::string  errString;
			switch(errCode)
			{
				case 0:
					break;
				default:
				{
					errString = "Failed reading range file, may not readable or may be invalid: ";
					errString += "\n";
					errString+=dataFile;
					
				}
			}

			if(errCode)
			{
				wxMessageDialog *wxD  =new wxMessageDialog(this,
					wxStr(errString),wxT("Error loading file"),wxOK|wxICON_ERROR);
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
		case COMBO_FILTER_SPECTRUM:
		{
			SpectrumPlotFilter *f = new SpectrumPlotFilter;
			//Tree data contains unique identifier for vis control to do matching
			wxTreeItemData *tData=treeFilters->GetItemData(id);

			ASSERT(tData);
			visControl.addFilter(f,((wxTreeUint *)tData)->value);

			//Rebuild tree control
			visControl.updateWxTreeCtrl(treeFilters,f);
			break;
		}
		case COMBO_FILTER_CLIPPING:
		{
			IonClipFilter *f = new IonClipFilter;
			//Tree data contains unique identifier for vis control to do matching
			wxTreeItemData *tData=treeFilters->GetItemData(id);

			ASSERT(tData);
			visControl.addFilter(f,((wxTreeUint *)tData)->value);

			//Rebuild tree control
			visControl.updateWxTreeCtrl(treeFilters,f);
			break;
		}
		case COMBO_FILTER_COLOUR:
		{
			IonColourFilter *f = new IonColourFilter;
			//Tree data contains unique identifier for vis control to do matching
			wxTreeItemData *tData=treeFilters->GetItemData(id);

			ASSERT(tData);
			visControl.addFilter(f,((wxTreeUint *)tData)->value);

			//Rebuild tree control
			visControl.updateWxTreeCtrl(treeFilters,f);
			break;
		}		
		case COMBO_FILTER_COMPOSITION_PROFILE:
		{
			CompositionProfileFilter *f = new CompositionProfileFilter;
			//Tree data contains unique identifier for vis control to do matching
			wxTreeItemData *tData=treeFilters->GetItemData(id);
			
			ASSERT(tData);
			visControl.addFilter(f,((wxTreeUint *)tData)->value);
			
			//Rebuild tree control
			visControl.updateWxTreeCtrl(treeFilters,f);
			break;
		}
		case COMBO_FILTER_VOXELS:
		{
			VoxeliseFilter *f = new VoxeliseFilter;
			//Tree data contains unique identifier for vis control to do matching
			wxTreeItemData *tData=treeFilters->GetItemData(id);
			
			ASSERT(tData);
			visControl.addFilter(f,((wxTreeUint *)tData)->value);
			
			//Rebuild tree control
			visControl.updateWxTreeCtrl(treeFilters,f);
			break;
		}
		case COMBO_FILTER_BOUNDINGBOX:
		{
			BoundingBoxFilter *f = new BoundingBoxFilter;
			//Tree data contains unique identifier for vis control to do matching
			wxTreeItemData *tData=treeFilters->GetItemData(id);

			ASSERT(tData);
			visControl.addFilter(f,((wxTreeUint *)tData)->value);

			//Rebuild tree control
			visControl.updateWxTreeCtrl(treeFilters,f);
			break;
		}
		case COMBO_FILTER_TRANSFORM:
		{
			TransformFilter *f = new TransformFilter;
			//Tree data contains unique identifier for vis control to do matching
			wxTreeItemData *tData=treeFilters->GetItemData(id);

			ASSERT(tData);
			visControl.addFilter(f,((wxTreeUint *)tData)->value);

			//Rebuild tree control
			visControl.updateWxTreeCtrl(treeFilters,f);
			break;
		}
		case COMBO_FILTER_EXTERNALPROG:
		{
			ExternalProgramFilter *f = new ExternalProgramFilter;
			//Tree data contains unique identifier for vis control to do matching
			wxTreeItemData *tData=treeFilters->GetItemData(id);

			ASSERT(tData);
			visControl.addFilter(f,((wxTreeUint *)tData)->value);

			//Rebuild tree control
			visControl.updateWxTreeCtrl(treeFilters,f);
			break;
		}
		case COMBO_FILTER_SPATIAL_ANALYSIS:
		{
			SpatialAnalysisFilter *f = new SpatialAnalysisFilter;
			//Tree data contains unique identifier for vis control to do matching
			wxTreeItemData *tData=treeFilters->GetItemData(id);

			ASSERT(tData);
			visControl.addFilter(f,((wxTreeUint *)tData)->value);

			//Rebuild tree control
			visControl.updateWxTreeCtrl(treeFilters,f);
			break;
		}
		default:
			ASSERT(false);
			haveErr=true;
	}

	if(haveErr)
	{
		//Clear thue combo box
		comboFilters->SetValue(_(""));
		return;
	}

	//Rebuild tree control
	refreshButton->Enable(visControl.numFilters());

	if(checkAutoUpdate->GetValue())
		doSceneUpdate();

	comboFilters->SetValue(_(""));
	
	editUndoMenuItem->Enable(visControl.getUndoSize());
	editRedoMenuItem->Enable(visControl.getRedoSize());
}

void MainWindowFrame::doSceneUpdate()
{
	//Update scene
	progressTimer->Start(150);		
	currentlyUpdatingScene=true;
	haveAborted=false;
	statusMessage("",MESSAGE_NONE);

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
	

	//Set focus on the main frame itself, so that we can catch escape key presses
	SetFocus();

	//Attempt a scene update
	unsigned int errCode=visControl.updateScene();

	progressTimer->Stop();
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
			errString = "Refresh Aborted.";
	
		statusMessage(errString.c_str(),MESSAGE_ERROR);	
	}

	if(!errCode)
	{
		//Call the progress one more time, in order to ensure that user sees "100%"
		updateProgressStatus();
	}
	
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
	
	statusTimer->Start(STATUS_TIMER_DELAY,wxTIMER_ONE_SHOT);
	panelTop->Refresh(false);
	panelSpectra->Refresh(false);	

	updateLastRefreshBox();
}

void MainWindowFrame::OnStatusBarTimer(wxTimerEvent &event)
{
	for(unsigned int ui=0; ui<3; ui++)
	{
		MainFrame_statusbar->SetStatusText(wxT(""),ui);
		MainFrame_statusbar->SetBackgroundColour(wxNullColour);
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
	wxStandardPaths *paths = new wxStandardPaths;

	wxString filePath = paths->GetDocumentsDir()+wxCStr("/.")+wxCStr(PROGRAM_NAME);


	//create the dir
	if(!wxDirExists(filePath))
		wxMkdir(filePath);

	filePath+=wxCStr("/") + wxCStr(AUTOSAVE_FILENAME);
	//Save to the autosave file
	std::string s;
	s=  stlStr(filePath);
	std::map<string,string> dummyMap;
	if(visControl.saveState(s.c_str(),dummyMap))
		statusMessage("Autosave complete.",MESSAGE_INFO);

}

void MainWindowFrame::OnUpdateTimer(wxTimerEvent &event)
{

	//FIXME: HACK AROUND: force tree filter to relayout under wxGTK and Mac
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
			l =(wxListUint*)  comboCamera->GetClientObject(n);

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
			visControl.updateFilterPropertyGrid(gridFilterProperties,
							((wxTreeUint *)tData)->value);
			editUndoMenuItem->Enable(visControl.getUndoSize());
			editRedoMenuItem->Enable(visControl.getRedoSize());
		}

	}
	
}

void MainWindowFrame::statusMessage(const char *message, unsigned int type)
{
	MainFrame_statusbar->SetStatusText(wxCStr(message),0);
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
		case MESSAGE_NONE:
			return;
		default:
			ASSERT(false);
	}

	statusTimer->Start(STATUS_TIMER_DELAY,wxTIMER_ONE_SHOT);
}

void MainWindowFrame::updateProgressStatus()
{

	std::string progressString,filterProg;


	if(!visControl.numFilters())
		return;

	if(haveAborted)
	{
		progressString="Aborted.";
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
		stream_cast(totalCount,visControl.numFilters());
		stream_cast(step,p.step);
		stream_cast(maxStep,p.maxStep);

		ASSERT(p.step <=p.maxStep);
		
		if(p.curFilter)
		{
			if(!p.maxStep)
				progressString = totalProg+" of " + totalCount +
						 " (" + p.curFilter->typeString() +")";
			else
			{
				progressString = totalProg+" of " + totalCount +
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
				progressString = "Updated.";
			else
				progressString = totalProg + " of " + totalCount;
		}

		if( p.filterProgress != 100)
			filterProg+="\% Done (Esc aborts)";
		else
			filterProg+="\% Done";

	}

	MainFrame_statusbar->SetStatusText(wxStr(progressString),1);
	MainFrame_statusbar->SetStatusText(wxStr(filterProg),2);
	MainFrame_statusbar->SetBackgroundColour(wxNullColour);
}

void MainWindowFrame::OnProgressAbort(wxCommandEvent &event)
{
	if(!haveAborted)
		visControl.abort();
	haveAborted=true;
}

void MainWindowFrame::OnViewFullscreen(wxCommandEvent &event)
{
	//Toggle fullscreen, leave the menubar  & statsbar visible
	checkViewFullscreen->Check(!IsFullScreen());	
	ShowFullScreen(!IsFullScreen(), wxFULLSCREEN_NOTOOLBAR| 
			 wxFULLSCREEN_NOBORDER| wxFULLSCREEN_NOCAPTION );}

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
			statusMessage("Use shift-click to force full refresh",MESSAGE_HINT);
	}
	doSceneUpdate();	
}

void MainWindowFrame::OnRawDataUnsplit(wxSplitterEvent &event)
{
    checkMenuRawDataPane->Check(false);
}

void MainWindowFrame::OnFilterPropDoubleClick(wxSplitterEvent &event)
{
	//Disallow unsplitting of filter property panel
	event.Veto();
}

void MainWindowFrame::OnControlUnsplit(wxSplitterEvent &event)
{
	splitLeftRight->Unsplit(panelLeft);
	checkMenuControlPane->Check(false);
}

void MainWindowFrame::OnSpectraUnsplit(wxSplitterEvent &event)
{
    checkMenuSpectraList->Check(false);
}

//This function modifies the properties before showing the cell content editor.T
//This is needed only for certain data types (colours, bools) other data types are edited
//using the default editor and modified using ::OnGridFilterPropertyChange
void MainWindowFrame::OnFilterGridCellEditorShow(wxGridEvent &event)
{
	//Find where the event occured (cell & property)
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
		case PROPERTY_TYPE_CHOICE:
			break;
		default:
		//we will handle this after the user has edited the cell contents
			break;
	}

	if(needUpdate)
	{
		visControl.updateFilterPropertyGrid(gridFilterProperties,
						((wxTreeUint *)tData)->value);

		editUndoMenuItem->Enable(visControl.getUndoSize());
		editRedoMenuItem->Enable(visControl.getRedoSize());
		if(checkAutoUpdate->GetValue())
			doSceneUpdate();
	}
}

void MainWindowFrame::OnCameraGridCellEditorShow(wxGridEvent &event)
{
	//Find where the event occured (cell & property)
	const GRID_PROPERTY *item=0;

	unsigned int key,set;
	key=gridCameraProperties->getKeyFromRow(event.GetRow());
	set=gridCameraProperties->getSetFromRow(event.GetRow());

	item=gridCameraProperties->getProperty(set,key);

	//Get the camera ID
	wxListUint *l;
	int n = comboCamera->FindString(comboCamera->GetValue());
	l =(wxListUint*)  comboCamera->GetClientObject(n);
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
			visControl.setCamProperties(camUniqueID,set,key,s);

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
				visControl.setCamProperties(camUniqueID,set,key,s);
			}

			//Set the filter property
			//Disallow direct editing of the grid cell
			event.Veto();

			break;
		}	
		default:
		//we will handle this after the user has edited the cell contents
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
	if(!gridRawData->GetRows()| !gridRawData->GetCols())
	{
		statusMessage("No data to save",MESSAGE_ERROR);
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


void MainWindowFrame::OnCacheRamUsageSpin(wxSpinEvent &event)
{
    event.Skip();
    wxLogDebug(wxT("Event handler (MainWindowFrame::OnCacheRamUsageSpin) not implemented yet")); //notify the user that he hasn't implemented the event handler yet
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
		l =(wxListUint*)  comboCamera->GetClientObject(n);
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
	//doSceneUpdate. prevent interaction.
	if(visControl.isRefreshing())
		return;
	//Get the currently selected item
	//Spin through the selected items
	for(unsigned int ui=0;ui<plotList->GetCount(); ui++)
	{
	 	wxListUint *l;
		unsigned int plotID;

		//Retreive the uniqueID
		l=(wxListUint*)plotList->GetClientObject(ui);
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

			statusMessage("Aborting...",MESSAGE_INFO);
			return;
		}
		else
		{
			wxMessageDialog *wxD  =new wxMessageDialog(this,
					wxT("Waiting for refresh to abort. Exiting could lead to the program backgrounding. Exit anyway? "),
					wxT("Confirmation request"),wxOK|wxCANCEL|wxICON_ERROR);

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
						wxT("Are you sure you wish to exit 3Depict?"),\
						wxT("Confirmation request"),wxOK|wxCANCEL|wxICON_ERROR);
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
	wxStandardPaths *paths = new wxStandardPaths;
	wxString filePath = paths->GetDocumentsDir()+wxCStr("/.")+wxCStr(PROGRAM_NAME);
	
	filePath+=wxCStr("/") + wxCStr(AUTOSAVE_FILENAME);

	if(wxFileExists(filePath))
		wxRemoveFile(filePath);


	delete paths;

	//Try to save the configuration
	configFile.write();

	Destroy();
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
		//
		requireFirstUpdate=true;
	}
}

void MainWindowFrame::set_properties()
{
    // begin wxGlade: MainWindowFrame::set_properties
    SetTitle(wxCStr(PROGRAM_NAME));
    comboFilters->SetToolTip(_("List of available filters"));
    comboFilters->SetSelection(-1);
    treeFilters->SetToolTip(_("Tree of data filters"));
    checkAutoUpdate->SetToolTip(_("Enable/Disable automatic updates of data when filter change takes effect"));
    checkAutoUpdate->SetValue(true);

    checkAlphaBlend->SetToolTip(_("Enable/Disable \"Alpha blending\" (transparency) in rendering system. This is used to smooth objects (avoids artefacts known as \"jaggies\") and to make transparent surfaces. Disabling will provide faster rendering but look more blocky")); 
    checkLighting->SetToolTip(_("Enable/Disable lighting calculations in rendering support, for objects that perform these calculations. Lighting provides important depth cues for objects comprised of 3D surfaces. Disabling may allow faster rendering in complex scenes"));
    checkCaching->SetToolTip(_("Enable/Disable caching of intermediate results during filter updates. This will use less system RAM, though changes to any filter property will cause the entire filter tree to be recomputed, greatly slowing computations"));

    gridFilterProperties->CreateGrid(0, 2);
    gridFilterProperties->EnableDragRowSize(false);
    gridFilterProperties->SetColLabelValue(0, _("Property"));
    gridFilterProperties->SetColLabelValue(1, _("Value"));
    gridCameraProperties->CreateGrid(4, 2);
    gridCameraProperties->EnableDragRowSize(false);
    gridCameraProperties->SetSelectionMode(wxGrid::wxGridSelectRows);
    gridCameraProperties->SetColLabelValue(0, _("Property"));
    gridCameraProperties->SetColLabelValue(1, _("Value"));
    gridCameraProperties->SetToolTip(_("Camera data information"));
    noteCamera->SetScrollRate(10, 10);
    notePerformance->SetScrollRate(10, 10);
    gridRawData->CreateGrid(10, 2);
    gridRawData->EnableEditing(false);
    gridRawData->EnableDragRowSize(false);
    gridRawData->SetColLabelValue(0, _("X"));
    gridRawData->SetColLabelValue(1, _("Y"));
    btnRawDataSave->SetToolTip(_("Save raw data to file"));
    btnRawDataClip->SetToolTip(_("Copy raw data to clipboard"));
    textConsoleOut->SetToolTip(_("Program text output"));
    // end wxGlade
    //

    //Set the controls that the viscontrol needs to interact with
    visControl.setScene(&panelTop->currentScene); //GL scene

    visControl.setRawGrid(gridRawData); //Raw data grid
    

    Multiplot *p=new Multiplot; //plotting area
    panelSpectra->setPlot(p);
    panelSpectra->setPlotList(plotList);
    visControl.setPlot(p);
    visControl.setPlotList(plotList);
    visControl.setConsole(textConsoleOut);

    refreshButton->Enable(false);
    comboCamera->Connect(wxID_ANY,
                 wxEVT_SET_FOCUS,
		   wxFocusEventHandler(MainWindowFrame::OnComboCameraSetFocus), NULL, this);
    comboStash->Connect(wxID_ANY,
                 wxEVT_SET_FOCUS,
		   wxFocusEventHandler(MainWindowFrame::OnComboStashSetFocus), NULL, this);
    gridCameraProperties->clear();
    int widths[] = {-4,-2,-1};
    MainFrame_statusbar->SetStatusWidths(3,widths);

}

void MainWindowFrame::do_layout()
{
    // begin wxGlade: MainWindowFrame::do_layout
    wxBoxSizer* topSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizer_3 = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizer_10 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizer_7 = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizer_8 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizer_19= new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizer_17= new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizer_19_copy = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizer_5 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizerLeft = new wxBoxSizer(wxVERTICAL);
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
    filterSplitter->SplitHorizontally(filterTreePane, filterPropertyPane);
    filterPaneSizer->Add(filterSplitter, 1, wxEXPAND, 0);
    noteData->SetSizer(filterPaneSizer);
    camPaneSizer->Add(label_2, 0, 0, 0);
    camTopRowSizer->Add(comboCamera, 3, 0, 0);
    camTopRowSizer->Add(buttonRemoveCam, 0, wxLEFT|wxRIGHT, 2);
    camPaneSizer->Add(camTopRowSizer, 0, wxTOP|wxBOTTOM|wxEXPAND, 4);
    camPaneSizer->Add(static_line_1, 0, wxEXPAND, 0);
    camPaneSizer->Add(gridCameraProperties, 1, wxEXPAND, 0);
    noteCamera->SetSizer(camPaneSizer);
    sizer_19->Add(checkAlphaBlend, 0, wxTOP|wxBOTTOM|wxADJUST_MINSIZE, 5);
    sizer_19->Add(checkLighting, 0, wxTOP|wxBOTTOM|wxADJUST_MINSIZE, 5);
    sizer_19->Add(checkCaching, 0, wxTOP|wxBOTTOM|wxADJUST_MINSIZE, 5);
    sizer_17->Add(10, 20, 0, wxADJUST_MINSIZE, 0);
    sizer_17->Add(label_8, 0, wxRIGHT|wxADJUST_MINSIZE, 5);
    sizer_17->Add(spinCachePercent, 0, wxADJUST_MINSIZE, 0);
    sizer_19->Add(sizer_17, 1, wxTOP|wxEXPAND, 5);
    notePerformance->SetSizer(sizer_19);
    notebookControl->AddPage(noteData, _("Data"));
    notebookControl->AddPage(noteCamera, _("Cam."));
    notebookControl->AddPage(notePerformance, _("Tools"));
    sizerLeft->Add(notebookControl, 1, wxLEFT|wxBOTTOM|wxEXPAND, 2);
    panelLeft->SetSizer(sizerLeft);
    sizer_5->Add(panelView, 1, wxEXPAND, 0);
    panelTop->SetSizer(sizer_5);
    sizer_19_copy->Add(label_5, 0, 0, 0);
    sizer_19_copy->Add(plotList, 1, wxEXPAND, 0);
    window_2_pane_2->SetSizer(sizer_19_copy);
    splitterSpectra->SplitVertically(panelSpectra, window_2_pane_2);
    sizer_7->Add(gridRawData, 3, wxEXPAND, 0);
    sizer_8->Add(20, 20, 1, 0, 0);
    sizer_8->Add(btnRawDataSave, 0, wxLEFT, 2);
    sizer_8->Add(btnRawDataClip, 0, wxLEFT, 2);
    sizer_7->Add(sizer_8, 0, wxTOP|wxEXPAND, 5);
    noteRaw->SetSizer(sizer_7);
    sizer_10->Add(textConsoleOut, 1, wxEXPAND, 0);
    noteDataView_pane_3->SetSizer(sizer_10);
    noteDataView->AddPage(splitterSpectra, _("Spec."));
    noteDataView->AddPage(noteRaw, _("Raw"));
    noteDataView->AddPage(noteDataView_pane_3, _("Cons."));
    splitTopBottom->SplitHorizontally(panelTop, noteDataView);
    sizer_3->Add(splitTopBottom, 1, wxEXPAND, 0);
    panelRight->SetSizer(sizer_3);
    splitLeftRight->SplitVertically(panelLeft, panelRight);
    topSizer->Add(splitLeftRight, 1, wxEXPAND, 0);
    SetSizer(topSizer);
    topSizer->Fit(this);
    Layout();
    // end wxGlade
    //
    // GTK fix hack thing. reparent window
	panelTop->Reparent(splitTopBottom);

	//Set the combo text
	haveSetComboCamText=false;
	comboCamera->SetValue(wxCStr(cameraIntroString));
	haveSetComboStashText=false;
	comboStash->SetValue(wxCStr(stashIntroString));
}

class threeDepictApp: public wxApp {
private:
	MainWindowFrame* MainFrame ;
	wxArrayString commandLineFiles;
public:
    bool OnInit();
    virtual void OnInitCmdLine(wxCmdLineParser& parser);
    virtual bool OnCmdLineParsed(wxCmdLineParser& parser);
		 
    int FilterEvent(wxEvent &event);

};


//Command line parameter table
static const wxCmdLineEntryDesc g_cmdLineDesc [] =
{
	{ wxCMD_LINE_SWITCH, wxT("h"), wxT("help"), wxT("displays this message"),
		wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
	{ wxCMD_LINE_PARAM,  NULL, NULL, _("inputfile"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_MULTIPLE |wxCMD_LINE_PARAM_OPTIONAL},
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
	if(  MainFrame->isCurrentlyUpdatingScene() &&
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
	name=name+ _(" ");
	version=wxCStr(PROGRAM_VERSION);
	version+=_("\n");

	preamble=_("Copyright (C) 2010  3Depict team\n");
	preamble+=_("This program comes with ABSOLUTELY NO WARRANTY; for details see LICENCE file.\n");
	preamble+=_("This is free software, and you are welcome to redistribute it under certain conditions.\n");
	preamble+=_("Source code is available under the terms of the GNU GPL v3.0 or any later version (http://www.gnu.org/licenses/gpl.txt)");
	parser.SetLogo(name+version+preamble);

	parser.SetDesc (g_cmdLineDesc);
	parser.SetSwitchChars (wxT("-"));
}

//Initialise wxwidgets parser
bool threeDepictApp::OnCmdLineParsed(wxCmdLineParser& parser)
{
	for(unsigned int ui=0;ui<parser.GetParamCount();ui++)
	{
		wxFileName f;
		f.Assign(parser.GetParam(ui));

		if( f.FileExists() )
			commandLineFiles.Add(f.GetFullPath());
		else
			std::cerr << "File : " << stlStr(f.GetFullPath()) << " does not exist. Skipping" << std::endl;

	}

	return true;
}



bool threeDepictApp::OnInit()
{

    //Register signal handler for backtraces
        if (!wxApp::OnInit())
    	return false; 

    //Need to seed random number generator for entire program
    srand (time(NULL));

    //Use a heuristic method (ie look around) to find a good sans-serif font
    TTFFinder fontFinder;
    setDefaultFontFile(fontFinder.getBestFontFile(TTFFINDER_FONT_SANS));
    
    wxInitAllImageHandlers();
    MainFrame = new MainWindowFrame(NULL, wxID_ANY, wxEmptyString);
    SetTopWindow(MainFrame);

#ifdef DEBUG
#ifdef __linux__
   trapfpe(); //Under linux, enable  segfault on invalid floating point operations
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
    return true;
}
