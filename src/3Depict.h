/*
 * 	threeDepict.h - main program header
 * 	Copyright (C) 2010 D Haley
 *
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


// begin wxGlade: ::dependencies
#include <wx/wx.h>
#include <wx/splitter.h>
#include <wx/statline.h>
#include <wx/notebook.h>
#include <wx/spinctrl.h>
#include <wx/listctrl.h>
#include <wx/docview.h>
#include <wx/dnd.h>
// end wxGlade

//Local stuff
#include "glPane.h"
#include "mathglPane.h"
#include "cropPanel.h" // cropping tools

#include "APTClasses.h"
#include "viscontrol.h"
#include "configFile.h"

#ifndef THREEDEPICT_H 
#define THREEDEPICT_H

class FileDropTarget;

class MainWindowFrame: public wxFrame {
public:
    // begin wxGlade: MainWindowFrame::ids
    // end wxGlade

    MainWindowFrame(wxWindow* parent, int id, const wxString& title, const wxPoint& pos=wxDefaultPosition, const wxSize& size=wxDefaultSize, long style=wxDEFAULT_FRAME_STYLE);
    virtual ~MainWindowFrame();

    void OnDropFiles(const wxArrayString &files);

    bool isCurrentlyUpdatingScene() const { return currentlyUpdatingScene;};

    void linkCropWidgets();

    wxSize getNiceWindowSize() const ;

private:
    // begin wxGlade: MainWindowFrame::methods
    void set_properties();
    void do_layout();
    // end wxGlade
   
   	//!Give  a  
    	void statusMessage(const char *message, unsigned int messageType=MESSAGE_ERROR); 

	//!Update the progress information in the status bar
	void updateProgressStatus();
	//!Perform an update to the 3D Scene. Returns false if refresh failed
	bool doSceneUpdate();

	//!Update the post-processing effects in the 3D scene. 
	void updatePostEffects(); 
	//!Load a file into the panel given the full path to the file
	bool loadFile(const wxString &dataFile,bool merge=false);


	//!Update the effects UI from some effects vector
	void updateFxUI(const vector<const Effect *> &fx);

	//!Scene - user interaction interface "visualisation control"
	VisController visControl;

	//!Program on-disk configuration class
	ConfigFile configFile;

	//!Blocking bool to prevent functions from responding to programatically generated wx events
	bool programmaticEvent,timerEvent;
	//!A flag stating if the first update needs a refresh after GL window OK
	bool requireFirstUpdate;
	//!Have we set the combo cam/stash text in this session?
	bool haveSetComboCamText,haveSetComboStashText;
	//!Are we in the middle of updating the scene?
	bool currentlyUpdatingScene;
	//!Have we aborted an update
	bool haveAborted;

	//!source item when dragging a filter in the tree control
	wxTreeItemId *filterTreeDragSource;

	//!The current file if we are using an XML file
	wxString currentFile;

	//!Drag and drop functionality
	FileDropTarget *dropTarget;
	
	//!Current fullscreen status
	unsigned int fullscreenState;

	//!Did the main frame's constructor complete OK?
	bool initedOK;

	//The type of status message last sent to user
	unsigned int lastMessageType;

	//Pointer to version check thread, occasionally initialised at startup to
	// check online for new program updates
	VersionCheckThread *verCheckThread;

	//Map to convert filter drop down choices to IDs
	map<std::string,size_t> filterMap;

protected:
    wxTimer *statusTimer;
    wxTimer *progressTimer;
    wxTimer *updateTimer; //Periodically calls itself to check for updates from user interaction
    wxTimer *autoSaveTimer; //Periodically calls itself to create an autosave state file
    wxMenuItem *checkMenuControlPane;
    wxMenuItem *checkMenuRawDataPane;
    wxMenuItem *checkMenuSpectraList;
    wxMenuItem *menuViewFullscreen;
    wxMenuItem *checkViewLegend;
    wxMenuItem *checkViewWorldAxis;

    wxMenuItem *editUndoMenuItem,*editRedoMenuItem;
    wxMenuItem *fileSave;
    wxMenu *recentFilesMenu;
    wxFileHistory *recentHistory;


    // begin wxGlade: MainWindowFrame::attributes
    wxMenuBar* MainFrame_Menu;
    wxStaticText* lblSettings;
    wxComboBox* comboStash;
    wxButton* btnStashManage;
    wxStaticText* filteringLabel;
    wxComboBox* comboFilters;
    wxTreeCtrl* treeFilters;
    wxStaticText* lastRefreshLabel;
    wxListCtrl* listLastRefresh;
    wxCheckBox* checkAutoUpdate;
    wxButton* refreshButton;
    wxButton* btnFilterTreeExpand;
    wxButton* btnFilterTreeCollapse;
    wxPanel* filterTreePane;
    wxStaticText* propGridLabel;
    wxPropertyGrid* gridFilterProperties;
    wxPanel* filterPropertyPane;
    wxSplitterWindow* filterSplitter;
    wxPanel* noteData;
    wxStaticText* labelCameraName;
    wxComboBox* comboCamera;
    wxButton* buttonRemoveCam;
    wxStaticLine* cameraNamePropertySepStaticLine;
    wxPropertyGrid* gridCameraProperties;
    wxScrolledWindow* noteCamera;
    wxCheckBox* checkPostProcessing;
    wxCheckBox* checkFxCrop;
    wxComboBox* comboFxCropAxisOne;
    CropPanel* panelFxCropOne;
    wxComboBox* comboFxCropAxisTwo;
    CropPanel* panelFxCropTwo;
    wxCheckBox* checkFxCropCameraFrame;
    wxStaticText* labelFxCropDx;
    wxTextCtrl* textFxCropDx;
    wxStaticText* labelFxCropDy;
    wxTextCtrl* textFxCropDy;
    wxStaticText* labelFxCropDz;
    wxTextCtrl* textFxCropDz;
    wxPanel* noteFxPanelCrop;
    wxCheckBox* checkFxEnableStereo;
    wxCheckBox* checkFxStereoLensFlip;
    wxStaticText* lblFxStereoMode;
    wxComboBox* comboFxStereoMode;
    wxStaticBitmap* bitmapFxStereoGlasses;
    wxStaticText* labelFxStereoBaseline;
    wxSlider* sliderFxStereoBaseline;
    wxPanel* noteFxPanelStereo;
    wxNotebook* noteEffects;
    wxPanel* notePost;
    wxCheckBox* checkAlphaBlend;
    wxCheckBox* checkLighting;
    wxCheckBox* checkWeakRandom;
    wxCheckBox* checkCaching;
    wxStaticText* labelMaxRamUsage;
    wxSpinCtrl* spinCachePercent;
    wxPanel* noteTools;
    wxNotebook* notebookControl;
    wxPanel* panelLeft;
    wxPanel* panelView;
    BasicGLPane* panelTop;
    MathGLPane* panelSpectra;
    wxStaticText* plotListLabel;
    wxListBox* plotList;
    wxPanel* window_2_pane_2;
    wxSplitterWindow* splitterSpectra;
    CopyGrid* gridRawData;
    wxButton* btnRawDataSave;
    wxButton* btnRawDataClip;
    wxPanel* noteRaw;
    wxTextCtrl* textConsoleOut;
    wxPanel* noteDataViewConsole;
    wxNotebook* noteDataView;
    wxPanel* panelBottom;
    wxSplitterWindow* splitTopBottom;
    wxPanel* panelRight;
    wxSplitterWindow* splitLeftRight;
    wxStatusBar* MainFrame_statusbar;
    // end wxGlade

    DECLARE_EVENT_TABLE();

public:
    virtual void OnFileOpen(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnFileMerge(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnFileSave(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnFileSaveAs(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnFileExit(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnViewControlPane(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnViewRawDataPane(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnHelpHelp(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnHelpContact(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnHelpAbout(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnComboStashText(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnComboStashEnter(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnComboStash(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnTreeEndDrag(wxTreeEvent &event); // wxGlade: <event_handler>
    virtual void OnTreeItemTooltip(wxTreeEvent &event); // wxGlade: <event_handler>
    virtual void OnTreeKeyDown(wxTreeEvent &event); // wxGlade: <event_handler>
    virtual void OnTreeSelectionChange(wxTreeEvent &event); // wxGlade: <event_handler>
    virtual void OnTreeDeleteItem(wxTreeEvent &event); // wxGlade: <event_handler>
    virtual void OnTreeBeginDrag(wxTreeEvent &event); // wxGlade: <event_handler>
    virtual void OnBtnExpandTree(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnComboCameraText(wxCommandEvent &event); // wxGlade: <event_handler>

    virtual void OnBtnCollapseTree(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnGridFilterPropertyChange(wxGridEvent &event); // wxGlade: <event_handler>
    virtual void OnComboCameraEnter(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnComboCamera(wxCommandEvent &event); // wxGlade: <event_handler>
    

    virtual void OnEditUndo(wxCommandEvent &event);    
    virtual void OnEditRedo(wxCommandEvent &event);    
    virtual void OnEditPreferences(wxCommandEvent &event);    
   


    virtual void OnButtonRemoveCam(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnCheckPostProcess(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnFxCropCheck(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnFxCropCamFrameCheck(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnFxCropAxisOne(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnFxCropAxisTwo(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnFxStereoEnable(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnFxStereoCombo(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnFxStereoBaseline(wxScrollEvent &event); // wxGlade: <event_handler>
    virtual void OnFxStereoLensFlip(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnButtonStashDialog(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnCheckAlpha(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnCheckLighting(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnCheckCacheEnable(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnCheckWeakRandom(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnCacheRamUsageSpin(wxSpinEvent &event); // wxGlade: <event_handler>
    virtual void OnSpectraListbox(wxCommandEvent &event); // wxGlade: <event_handler>

    virtual void OnComboFilterEnter(wxCommandEvent &event); // 
    virtual void OnComboFilter(wxCommandEvent &event); // 
    
    virtual void OnStatusBarTimer(wxTimerEvent &event); // 
    virtual void OnFilterGridCellEditorShow(wxGridEvent &event); // 
    virtual void OnCameraGridCellEditorShow(wxGridEvent &event); // 
    virtual void OnProgressTimer(wxTimerEvent &event);
    virtual void OnProgressAbort(wxCommandEvent &event);
    virtual void OnViewFullscreen(wxCommandEvent &event);
    virtual void OnButtonRefresh(wxCommandEvent &event);
    virtual void OnButtonGridCopy(wxCommandEvent &event);
    virtual void OnButtonGridSave(wxCommandEvent &event);
    virtual void OnRawDataUnsplit(wxSplitterEvent &event);
    virtual void OnFilterPropDoubleClick(wxSplitterEvent &event);
    virtual void OnControlUnsplit(wxSplitterEvent &event);
    virtual void OnSpectraUnsplit(wxSplitterEvent &event);
    virtual void OnViewSpectraList(wxCommandEvent &event); 
    virtual void OnViewPlotLegend(wxCommandEvent &event); 
    virtual void OnViewWorldAxis(wxCommandEvent &event); 
    virtual void OnViewBackground(wxCommandEvent &event);
    virtual void OnClose(wxCloseEvent &evt);
    virtual void OnComboCameraSetFocus(wxFocusEvent &evt);
    virtual void OnComboStashSetFocus(wxFocusEvent &evt);
    virtual void OnNoteDataView(wxNotebookEvent &evt);
    virtual void OnGridCameraPropertyChange(wxGridEvent &event); // wxGlade: <event_handler>

    virtual void OnFileExportPlot(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnFileExportImage(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnFileExportIons(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnFileExportRange(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnFileExportVideo(wxCommandEvent &event);
    virtual void OnFileExportPackage(wxCommandEvent &event);
    virtual void OnRecentFile(wxCommandEvent &event); // wxGlade: <event_handler>

    virtual void OnTreeEndLabelEdit(wxTreeEvent &evt);
    virtual void OnUpdateTimer(wxTimerEvent &evt);
    virtual void OnAutosaveTimer(wxTimerEvent &evt);

    virtual void OnCheckUpdatesThread(wxCommandEvent &evt);

    virtual void SetCommandLineFiles(wxArrayString &files);
    virtual void updateLastRefreshBox();


    bool initOK() const {return initedOK;}

    //This is isolated from the layout code, due to "bug" 4815 in wx. The splitter window
    //does not know how to choose a good size until the window is shown
    void fixSplitterWindow() { 
	    filterSplitter->SplitHorizontally(filterTreePane,filterPropertyPane);
	    splitTopBottom->SplitHorizontally(panelTop, noteDataView);
	    splitTopBottom->SetSashPosition(GetSize().GetHeight()/10*7);
	   	};

}; // wxGlade: end class


class FileDropTarget : public wxFileDropTarget
{
private:
	MainWindowFrame *frame;
public:
	FileDropTarget(MainWindowFrame *f) {
		frame = f;
	}

	virtual bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& files)
	{
		frame->OnDropFiles(files);

		return true;
	};

};

#endif // QUICK3D_H
