// -*- C++ -*- generated by wxGlade HG on Fri Dec  3 22:26:29 2010
/*
 * prefDialog.h  - program preferences management dialog
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
#ifndef PREFDIALOG_H
#define PREFDIALOG_H


#include <wx/wx.h>
// begin wxGlade: ::dependencies
#include <wx/notebook.h>
#include "wx/wxcomponents.h"
// end wxGlade

class Filter;

// begin wxGlade: ::extracode
// end wxGlade

//As a courtesy, we do not allow online update checking under linux
//its pointless, as linux systems usually have proper package management
#if defined( __linux__) 
	#define DISABLE_ONLINE_UPDATE
#endif

class PrefDialog: public wxDialog 
{

private:
	// begin wxGlade: PrefDialog::methods
	void set_properties();
	void do_layout();
	// end wxGlade

	//the user specified defaults.  This class must clean up this pointer
	std::vector<Filter *> filterDefaults;

	//!Current default filter setting (def. filter panel). Is null iff using hard-coded version
	Filter *curFilter;

	bool programmaticEvent;
	//!Generate the list of filters which can have their defaults set
	void createFilterListing();

	//!Enable/disable the check controls ont he startup panel as needed
	void setStartupCheckboxEnables(unsigned int comboSel);

	//!Update the filter property grid, as needed
	void updateFilterProp(const Filter *f);

	//!Percentile speeds for mouse zoom and move 
	unsigned int mouseZoomRatePercent,mouseMoveRatePercent;
protected:
    // begin wxGlade: PrefDialog::attributes
    wxStaticBox* sizerCamSpeed_staticbox;
    wxStaticBox* sizer_7_staticbox;
    wxStaticBox* updateSizer_staticbox;
    wxStaticBox* sizer_2_staticbox;
    wxStaticBox* filterPropSizer_staticbox;
    wxStaticText* lblFilters;
    wxListBox* listFilters;
    wxPropertyGrid* filterGridProperties;
    wxButton* filterBtnResetAllFilters;
    wxButton* filterResetDefaultFilter;
    wxPanel* panelFilters;
    wxComboBox* comboPanelStartMode;
    wxCheckBox* chkControl;
    wxCheckBox* chkRawData;
    wxCheckBox* chkPlotlist;
#ifndef DISABLE_ONLINE_UPDATE
    wxCheckBox* checkAllowOnlineUpdate;
#endif
    wxPanel* panelStartup;
    wxCheckBox* chkPreferOrtho;
    wxStaticText* lblMoveSpeed;
    wxStaticText* labelSlowCamMoveRate;
    wxSlider* sliderCamMoveRate;
    wxStaticText* labelFastCamMoveRate;
    wxStaticText* lblZoomSpeed;
    wxStaticText* labelSlowCamZoomRate;
    wxSlider* sliderCamZoomRate;
    wxStaticText* labelSlowFastZoomRate;
    wxPanel* notePrefPanels_pane_3;
    wxNotebook* notePrefPanels;
    wxButton* btnOK;
    wxButton* btnCancel;
    // end wxGlade

	DECLARE_EVENT_TABLE();

public:
	// begin wxGlade: PrefDialog::ids
	// end wxGlade
	PrefDialog(wxWindow* parent, int id=wxID_ANY, const wxString& title=wxT(""), const wxPoint& pos=wxDefaultPosition, const wxSize& size=wxDefaultSize, long style=wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER);
	virtual ~PrefDialog();
	virtual void OnFilterCellChange(wxGridEvent &event); // wxGlade: <event_handler>
	virtual void OnFilterListClick(wxCommandEvent &event); // wxGlade: <event_handler>
	virtual void OnFilterGridCellEditorShow(wxGridEvent &event); // wxGlade: <event_handler>
	virtual void OnResetFilterButton(wxCommandEvent &event); // wxGlade: <event_handler>
	virtual void OnResetFilterAllButton(wxCommandEvent &event); // wxGlade: <event_handler>

	//set the filter defaults. note that the incoming pointers are cloned, and control is NOT transferred to this class
	void setFilterDefaults(const std::vector<Filter * > &defs);
	//Get the filter defaults. Note clones are returned, not the originals
	void getFilterDefaults(std::vector<Filter * > &defs) const;

	void setPanelDefaults(unsigned int panelMode, bool panelControl,
					bool panelRaw,bool panelPlotlist);
	void getPanelDefaults(unsigned int &panelMode, bool &panelControl,
					bool &panelRaw,bool &panelPlotlist) const;

#ifndef DISABLE_ONLINE_UPDATE
	bool getAllowOnlineUpdate() const { return checkAllowOnlineUpdate->IsChecked();};
	void setAllowOnlineUpdate(bool allowed) { checkAllowOnlineUpdate->SetValue(allowed);};
#endif
	void setMouseZoomRate(unsigned int rate) { mouseZoomRatePercent=rate;};
	void setMouseMoveRate(unsigned int rate) { mouseMoveRatePercent=rate;};

	bool getPreferOrthoCam() const { return chkPreferOrtho->IsChecked();}
	void setPreferOrthoCam(bool prefer) const { return chkPreferOrtho->SetValue(prefer);}

	unsigned int getMouseZoomRate() const { return  mouseZoomRatePercent;};
	unsigned int getMouseMoveRate() const { return mouseMoveRatePercent;};

	virtual void OnStartupPanelCombo(wxCommandEvent &event); // wxGlade: <event_handler>
    	virtual void OnCheckPreferOrtho(wxCommandEvent &event); // wxGlade: <event_handler>
	void OnMouseMoveSlider(wxScrollEvent &event);
	void OnMouseZoomSlider(wxScrollEvent &event);

	void initialise();
	void cleanup();
}; // wxGlade: end class


#endif // PREFDIALOG_H
