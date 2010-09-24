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
#include "wxPreprec.h"

// begin wxGlade: ::dependencies
#include <wx/splitter.h>
#include <wx/statline.h>
#include <wx/notebook.h>
#include <wx/spinctrl.h>
#include <wx/listctrl.h>
#include <wx/docview.h>
// end wxGlade

//Local stuff
#include "glPane.h"
#include "mathglPane.h"
#include "APTClasses.h"
#include "common.h"
#include "viscontrol.h"
#include "configFile.h"

#ifndef QUICK3D_H
#define QUICK3D_H


class MainWindowFrame: public wxFrame {
public:
    // begin wxGlade: MainWindowFrame::ids
    // end wxGlade

    MainWindowFrame(wxWindow* parent, int id, const wxString& title, const wxPoint& pos=wxDefaultPosition, const wxSize& size=wxDefaultSize, long style=wxDEFAULT_FRAME_STYLE);
    virtual ~MainWindowFrame();

private:
    // begin wxGlade: MainWindowFrame::methods
    void set_properties();
    void do_layout();
    // end wxGlade

	void updateProgressStatus();
	void doSceneUpdate();
	//!Load a file into the panel given the full path to the file
	bool loadFile(const wxString &dataFile,bool merge=false);
	//!Scene - user interaction interface "visualisation control"
	VisController visControl;

	//!Program on-disk configuration class
	ConfigFile configFile;

	//!Blocking bool to prevent functions from responding to programatically generated wx events
	bool programmaticEvent;
	//!A flag stating if the first update needs a refresh after GL window OK
	bool requireFirstUpdate;
	//!Have we set the combo cam/stash text in this session?
	bool haveSetComboCamText,haveSetComboStashText;
	//!Are we in the middle of updating the scene?
	bool currentlyUpdatingScene;
	//!Have we aborted an update
	bool haveAborted;

	//!Progress of filter (out of 100) for current filter
	unsigned int filterProgress;
	//!Number of filters that we have proccessed (n out of m filters)
	unsigned int totalProgress;
	//!Pointer to the current filter that is being updated. Only valid during an update callback
	const Filter *curProgFilter;
	//!source item when dragging a filter in the tree control
	wxTreeItemId *filterTreeDragSource;

	//!The current file if we are using an XML file
	wxString currentFile;
protected:
    wxTimer *statusTimer;
    wxTimer *progressTimer;
    wxTimer *updateTimer; //Periodically calls itself to check for updates from user interaction
    wxTimer *autoSaveTimer; //Periodically calls itself to create an autosave state file
    wxMenuItem *checkMenuControlPane;
    wxMenuItem *checkMenuRawDataPane;
    wxMenuItem *checkMenuSpectraList;
    wxMenuItem *checkViewFullscreen;
    wxMenuItem *checkViewLegend;

    wxMenuItem *editUndoMenuItem,*editRedoMenuItem;
    wxMenuItem *fileSave;
    wxMenu *recentFilesMenu;
    wxFileHistory *recentHistory;

    // begin wxGlade: MainWindowFrame::attributes
    wxStaticBox* sizer_4_staticbox;
    wxStaticBox* sizer_5_staticbox;
    wxMenuBar* MainFrame_Menu;
    wxStaticText* lblSettings;
    wxComboBox* comboStash;
    wxButton* btnStashManage;
    wxStaticText* filteringLabel;
    wxComboBox* comboFilters;
    wxTreeCtrl* treeFilters;
    wxStaticText* lastRefreshLabel;
    wxStaticText* label_3;
    wxListCtrl* listLastRefresh;
    wxCheckBox* checkAutoUpdate;
    wxButton* refreshButton;
    wxPanel* panel_1;
    wxButton* btnFilterTreeExpand;
    wxButton* btnFilterTreeCollapse;
    wxPanel* filterTreePane;
    wxStaticText* propGridLabel;
    wxPropertyGrid* gridFilterProperties;
    wxPanel* filterPropertyPane;
    wxSplitterWindow* filterSplitter;
    wxPanel* noteData;
    wxStaticText* label_2;
    wxComboBox* comboCamera;
    wxButton* buttonRemoveCam;
    wxStaticLine* static_line_1;
    wxPropertyGrid* gridCameraProperties;
    wxScrolledWindow* noteCamera;
    wxCheckBox* checkAlphaBlend;
    wxCheckBox* checkLighting;
    wxCheckBox* checkCaching;
    wxStaticText* label_8;
    wxSpinCtrl* spinCachePercent;
    wxScrolledWindow* notePerformance;
    wxNotebook* notebookControl;
    wxPanel* panelLeft;
    wxPanel* panelView;
    BasicGLPane* panelTop;
    MathGLPane* panelSpectra;
    wxStaticText* label_5;
    wxListBox* plotList;
    wxPanel* window_2_pane_2;
    wxSplitterWindow* splitterSpectra;
    CopyGrid* gridRawData;
    wxButton* btnRawDataSave;
    wxButton* btnRawDataClip;
    wxPanel* noteRaw;
    wxTextCtrl* textConsoleOut;
    wxPanel* noteDataView_pane_3;
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
   


    virtual void OnButtonRemoveCam(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnButtonStashDialog(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnCheckAlpha(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnCheckLighting(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnCheckCacheEnable(wxCommandEvent &event); // wxGlade: <event_handler>
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
    virtual void OnViewBackground(wxCommandEvent &event);
    virtual void OnClose(wxCloseEvent &evt);
    virtual void OnComboCameraSetFocus(wxFocusEvent &evt);
    virtual void OnComboStashSetFocus(wxFocusEvent &evt);
    virtual void OnGridCameraPropertyChange(wxGridEvent &event); // wxGlade: <event_handler>

    virtual void OnFileExportPlot(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnFileExportImage(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnFileExportIons(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnFileExportRange(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnFileExportVideo(wxCommandEvent &event);
    virtual void OnRecentFile(wxCommandEvent &event); // wxGlade: <event_handler>

    virtual void OnTreeEndLabelEdit(wxTreeEvent &evt);
    virtual void OnUpdateTimer(wxTimerEvent &evt);
    virtual void OnAutosaveTimer(wxTimerEvent &evt);

    virtual void SetCommandLineFiles(wxArrayString &files);
    virtual void updateLastRefreshBox();
}; // wxGlade: end class


#endif // QUICK3D_H
