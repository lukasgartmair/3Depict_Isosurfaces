/*
 *	prefDialog.cpp - mathgl plot wrapper class
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

// -*- C++ -*- generated by wxGlade HG on Fri Dec  3 22:26:29 2010

#include "prefDialog.h"

#include "filters/allFilter.h"
#include "translation.h"

#include <wx/colordlg.h>

using std::vector;



// begin wxGlade: ::extracode
// end wxGlade
enum
{
	ID_LIST_FILTERS = wxID_ANY+1,
	ID_GRID_PROPERTIES,
	ID_BTN_RESET_FILTER,
	ID_BTN_RESET_FILTER_ALL,
	ID_START_CHECK_CONTROL,
	ID_START_CHECK_PLOTLIST,
	ID_START_CHECK_RAWDATA,
	ID_START_COMBO_PANEL,
	ID_MOUSE_MOVE_SLIDER,
	ID_MOUSE_ZOOM_SLIDER,
};


//Number that 
enum
{
	STARTUP_COMBO_SELECT_SHOW_ALL,
	STARTUP_COMBO_SELECT_REMEMBER,
	STARTUP_COMBO_SELECT_SPECIFY
};


PrefDialog::PrefDialog(wxWindow* parent, int id, const wxString& title, const wxPoint& pos, const wxSize& size, long style):
    wxDialog(parent, id, title, pos, size, style)
{
    // begin wxGlade: prefDialog::prefDialog
	notePrefPanels = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0);
    notePrefPanels_pane_3 = new wxPanel(notePrefPanels, wxID_ANY);
	panelStartup = new wxPanel(notePrefPanels, wxID_ANY);
	panelFilters = new wxPanel(notePrefPanels, wxID_ANY);
#ifndef DISABLE_ONLINE_UPDATE
	updateSizer_staticbox = new wxStaticBox(panelStartup, -1, wxTRANS("Online Updates"));
#endif
	sizer_2_staticbox = new wxStaticBox(panelStartup, -1, wxTRANS("Panel Display"));
    sizerCamSpeed_staticbox = new wxStaticBox(notePrefPanels_pane_3, -1, wxTRANS("Camera Speed"));
	filterPropSizer_staticbox = new wxStaticBox(panelFilters, -1, wxTRANS("Filter Defaults"));
	lblFilters = new wxStaticText(panelFilters, wxID_ANY, wxTRANS("Available Filters"));
	const wxString *listFilters_choices = NULL;
	listFilters = new wxListBox(panelFilters, ID_LIST_FILTERS, wxDefaultPosition, wxDefaultSize, 0, listFilters_choices, wxLB_SINGLE|wxLB_SORT);
	filterGridProperties = new wxPropertyGrid(panelFilters, ID_GRID_PROPERTIES);
	filterBtnResetAllFilters = new wxButton(panelFilters, ID_BTN_RESET_FILTER_ALL, wxTRANS("Reset All"));
	filterResetDefaultFilter = new wxButton(panelFilters, ID_BTN_RESET_FILTER, wxTRANS("Reset"));
	const wxString comboPanelStartMode_choices[] = {
		wxNTRANS("Show all panels"),
		wxNTRANS("Remember last"),
		wxNTRANS("Show Selected")
	};
	comboPanelStartMode = new wxComboBox(panelStartup, ID_START_COMBO_PANEL, wxT(""), wxDefaultPosition, wxDefaultSize, 3, comboPanelStartMode_choices, wxCB_DROPDOWN|wxCB_SIMPLE|wxCB_DROPDOWN|wxCB_READONLY);
	chkControl = new wxCheckBox(panelStartup, ID_START_CHECK_CONTROL, wxTRANS("Control Pane"));
	chkRawData = new wxCheckBox(panelStartup, ID_START_CHECK_RAWDATA, wxTRANS("Raw Data Panel"));
	chkPlotlist = new wxCheckBox(panelStartup, ID_START_CHECK_PLOTLIST, wxTRANS("Plot List"));
#ifndef DISABLE_ONLINE_UPDATE
	checkAllowOnlineUpdate = new wxCheckBox(panelStartup, wxID_ANY, _("Periodically notify about available updates"));
#endif
	lblMoveSpeed = new wxStaticText(notePrefPanels_pane_3, wxID_ANY, wxTRANS("Move Rate"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
	labelSlowCamMoveRate = new wxStaticText(notePrefPanels_pane_3,wxID_ANY, wxTRANS("(slow)"));
	sliderCamMoveRate = new wxSlider(notePrefPanels_pane_3, ID_MOUSE_MOVE_SLIDER, 25, 1, 400,wxDefaultPosition,wxDefaultSize,wxSL_HORIZONTAL|wxSL_LABELS);
	labelFastCamMoveRate = new wxStaticText(notePrefPanels_pane_3, wxID_ANY, wxTRANS("(fast)"));
	lblZoomSpeed = new wxStaticText(notePrefPanels_pane_3, wxID_ANY, wxTRANS("Zoom Rate"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
	labelSlowCamZoomRate = new wxStaticText(notePrefPanels_pane_3, wxID_ANY, wxTRANS("(slow)"));
	sliderCamZoomRate = new wxSlider(notePrefPanels_pane_3, ID_MOUSE_ZOOM_SLIDER, 25, 1, 400,wxDefaultPosition,wxDefaultSize,wxSL_HORIZONTAL|wxSL_LABELS);
	labelSlowFastZoomRate = new wxStaticText(notePrefPanels_pane_3, wxID_ANY, wxTRANS("(fast)"));
	btnOK = new wxButton(this, wxID_OK, wxEmptyString);
	btnCancel = new wxButton(this, wxID_CANCEL, wxEmptyString);

	filterGridProperties->CreateGrid(0, 2);
	filterGridProperties->EnableDragRowSize(false);
	filterGridProperties->SetColLabelValue(0, wxTRANS("Param"));
	filterGridProperties->SetColLabelValue(1, wxTRANS("Value"));

	bool enable=(comboPanelStartMode->GetSelection()  == STARTUP_COMBO_SELECT_SPECIFY);

	chkRawData->Enable(enable);
	chkControl->Enable(enable);
	chkPlotlist->Enable(enable);

	comboPanelStartMode->SetSelection(0);
	programmaticEvent=false;
	curFilter=0;

	set_properties();
	do_layout();
	// end wxGlade
}


BEGIN_EVENT_TABLE(PrefDialog, wxDialog)
    // begin wxGlade: PrefDialog::event_table
    EVT_LISTBOX(ID_LIST_FILTERS, PrefDialog::OnFilterListClick)
    EVT_GRID_CMD_CELL_CHANGE(ID_GRID_PROPERTIES, PrefDialog::OnFilterCellChange)
    EVT_GRID_CMD_EDITOR_SHOWN(ID_GRID_PROPERTIES,PrefDialog::OnFilterGridCellEditorShow)
    EVT_BUTTON(ID_BTN_RESET_FILTER,PrefDialog::OnResetFilterButton)
    EVT_BUTTON(ID_BTN_RESET_FILTER_ALL,PrefDialog::OnResetFilterAllButton)
    EVT_COMBOBOX(ID_START_COMBO_PANEL, PrefDialog::OnStartupPanelCombo)
    EVT_COMMAND_SCROLL(ID_MOUSE_ZOOM_SLIDER, PrefDialog::OnMouseZoomSlider)
    EVT_COMMAND_SCROLL(ID_MOUSE_MOVE_SLIDER, PrefDialog::OnMouseMoveSlider)
    // end wxGlade
END_EVENT_TABLE();


void PrefDialog::createFilterListing()
{
	listFilters->Clear();

	wxString s;
	vector<size_t> v;
	for(size_t ui=0;ui<filterDefaults.size();ui++)
	{
		s=wxStr(filterDefaults[ui]->typeString());
		v.push_back(filterDefaults[ui]->getType());
		listFilters->Append(s);
	}

	std::sort(v.begin(),v.end());
	unsigned int pos=0;
	for(unsigned int ui=0;ui<FILTER_TYPE_ENUM_END;ui++)
	{
		//If we have a default, dont create a new filter entry
		if(pos >=v.size() || v[pos] != ui)
		{
			//This is a bit of a hack, but it works.
			Filter *t;
			t=makeFilter(ui);
			s = wxStr(t->typeString());
			listFilters->Append(s);
			delete t;
		}
		else
			pos++;
	}
	
}

void PrefDialog::initialise()
{
	createFilterListing();

	//Transfer the movement rates from class  to the slider
	ASSERT(mouseZoomRatePercent  >=sliderCamZoomRate->GetMin() && 
			mouseZoomRatePercent < sliderCamZoomRate->GetMax());
	ASSERT(mouseMoveRatePercent  >=sliderCamMoveRate->GetMin() && 
			mouseMoveRatePercent < sliderCamMoveRate->GetMax());


	sliderCamZoomRate->SetValue(mouseZoomRatePercent);
	sliderCamMoveRate->SetValue(mouseMoveRatePercent);

}

void PrefDialog::cleanup()
{
	for(unsigned int ui=0;ui<filterDefaults.size();ui++)
		delete filterDefaults[ui];
}


//-------------- Filter page-----------------------
void PrefDialog::getFilterDefaults(vector<Filter *> &defs) const
{
	defs.resize(filterDefaults.size());

	for(unsigned int ui=0;ui<defs.size();ui++)
		defs[ui]=filterDefaults[ui]->cloneUncached();
}

void PrefDialog::setFilterDefaults(const vector<Filter *> &defs)
{
	filterDefaults.resize(defs.size());

	for(unsigned int ui=0;ui<defs.size();ui++)
		filterDefaults[ui]=defs[ui]->cloneUncached();
}

void PrefDialog::OnFilterListClick(wxCommandEvent &event)
{

	//get the string associated with the current click
	string s;

	//Check to see if we need to delete the current filter
	//or if we have it as a default
	if(find(filterDefaults.begin(),filterDefaults.end(),
				curFilter) == filterDefaults.end())
		delete curFilter;


	Filter *f=0;
	s=stlStr(listFilters->GetString(event.GetSelection()));

	for(size_t ui=0;ui<filterDefaults.size();ui++)
	{
		if(filterDefaults[ui]->getUserString() == s)
		{
			f=filterDefaults[ui];
			break;
		}
	}

	if(!f)
		f=makeFilterFromDefUserString(s);

	ASSERT(f);
	updateFilterProp(f);

	curFilter=f;
}


void PrefDialog::OnFilterCellChange(wxGridEvent &event)
{
	//Disallow programmatic event from causing filter update (stack loop->overflow)
	if(programmaticEvent)
	{
		event.Veto();
		return;
	}
	

	//Grab the changed value	
	std::string value; 
	int row=event.GetRow();
	value = stlStr(filterGridProperties->GetCellValue(
					row,1));
	programmaticEvent=true;

	bool needUpdate;
	curFilter->setProperty(filterGridProperties->getSetFromRow(row),
			filterGridProperties->getKeyFromRow(row),value,needUpdate);

	if(find(filterDefaults.begin(),filterDefaults.end(),
				curFilter) == filterDefaults.end())
		filterDefaults.push_back(curFilter);
	
	updateFilterProp(curFilter);
	programmaticEvent=false;
}



//This function modifies the properties before showing the cell content editor.T
//This is needed only for certain data types (colours, bools) other data types are edited
//using the default editor and modified using ::OnGridFilterPropertyChange
void PrefDialog::OnFilterGridCellEditorShow(wxGridEvent &event)
{
	//Find where the event occured (cell & property)
	const GRID_PROPERTY *item=0;

	unsigned int key,set;
	key=filterGridProperties->getKeyFromRow(event.GetRow());
	set=filterGridProperties->getSetFromRow(event.GetRow());

	item=filterGridProperties->getProperty(set,key);

	bool needUpdate;
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
			curFilter->setProperty(set,key,s,needUpdate);

			event.Veto();

			updateFilterProp(curFilter);
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
				curFilter->setProperty(set,key,s,needUpdate);
			}

			//Set the filter property
			//Disallow direct editing of the grid cell
			event.Veto();


			updateFilterProp(curFilter);
			break;
		}	
		case PROPERTY_TYPE_CHOICE:
			break;
		default:
		//we will handle this after the user has edited the cell contents
			break;
	}

	//Add to the modified filter defaults as needed
	if(find(filterDefaults.begin(),filterDefaults.end(),
				curFilter) == filterDefaults.end())
		filterDefaults.push_back(curFilter);

}

void PrefDialog::OnResetFilterButton(wxCommandEvent &evt)
{

	if(!curFilter)
	{
		evt.Skip();
		return;
	}

	//Erase the filter from the modified defaults list
	for(size_t ui=0;ui<filterDefaults.size();ui++)
	{
		//If we need to wipe the modified default,
		//do so. Otherwise, do nothing.
		if(filterDefaults[ui] == curFilter)
		{
			delete filterDefaults[ui];
			filterDefaults[ui]=filterDefaults.back();
			filterDefaults.pop_back();

			//Create a new "Cur" filter
			string s;
			s=stlStr(listFilters->GetString(
					listFilters->GetSelection()));
			curFilter=makeFilterFromDefUserString(s);
			
			//Update the prop grid 
			updateFilterProp(curFilter);
			break;
		}
	}

}

void PrefDialog::OnResetFilterAllButton(wxCommandEvent &evt)
{
	if(!curFilter)
	{
		evt.Skip();
		return;
	}

	//Check to see if we have it in the non-"true" default list
	for(size_t ui=0;ui<filterDefaults.size();ui++)
	{
		//Delete the modified default
		delete filterDefaults[ui];
	}
	filterDefaults.clear();

	string s;
	s=stlStr(listFilters->GetString(
			listFilters->GetSelection()));

	curFilter=makeFilterFromDefUserString(s);
	updateFilterProp(curFilter);
}


void PrefDialog::updateFilterProp(const Filter *f)
{
	if(!(f->canBeHazardous()))
	{
		filterGridProperties->Enable(true);
		updateFilterPropertyGrid(filterGridProperties,f);
	}
	else
	{
		//If the filter is potentially hazardous,
		//then we will disallow editing of the properties.
		//and give a notice to that effect	
		filterGridProperties->BeginBatch();
		filterGridProperties->Enable(false);
		wxGridCellAttr *readOnlyColAttr=new wxGridCellAttr;

		//Empty the grid
		//then fill it up with a note.
		filterGridProperties->DeleteCols(0,filterGridProperties->GetNumberCols());
		filterGridProperties->DeleteRows(0,filterGridProperties->GetNumberRows());
		
		filterGridProperties->AppendRows(1);
		filterGridProperties->AppendCols(1);
		filterGridProperties->AutoSizeColumn(0,true);
		filterGridProperties->SetColAttr(0,readOnlyColAttr);
		filterGridProperties->SetColLabelValue(0,wxTRANS("Notice"));

		filterGridProperties->SetCellValue(0,0,
			wxTRANS("For security reasons, defaults are not modifiable for this filter"));

		filterGridProperties->EndBatch();

		//Set the column size so you can see the message		
		filterGridProperties->SetColSize(0, filterGridProperties->GetSize().GetWidth()
					- filterGridProperties->GetScrollThumb(wxVERTICAL) - 15);
		
	}
}

//-------------- End Filter page-----------------------

//-------------- Startup panel page-----------------------

void PrefDialog::OnStartupPanelCombo(wxCommandEvent &event)
{
	setStartupCheckboxEnables(event.GetSelection());
}

void PrefDialog::setStartupCheckboxEnables(unsigned int value)
{
	bool enable=( value  == STARTUP_COMBO_SELECT_SPECIFY);

	chkRawData->Enable(enable);
	chkControl->Enable(enable);
	chkPlotlist->Enable(enable);
	switch(value)
	{
		case STARTUP_COMBO_SELECT_SHOW_ALL:
			comboPanelStartMode->SetToolTip(wxTRANS("Show all panels when starting program"));
			break;
		case STARTUP_COMBO_SELECT_REMEMBER:
			comboPanelStartMode->SetToolTip(wxTRANS("Show panels visible at last shutdown when starting program"));
			
			chkRawData->SetValue(true);
			chkControl->SetValue(true);
			chkPlotlist->SetValue(true);
			break;
		case STARTUP_COMBO_SELECT_SPECIFY:
			comboPanelStartMode->SetToolTip(wxTRANS("Show selected panels when starting program"));
			chkRawData->SetValue(true);
			chkControl->SetValue(true);
			chkPlotlist->SetValue(true);
			break;
		default:
			ASSERT(false);
	}
}
void PrefDialog::setPanelDefaults(unsigned int panelMode, bool panelControl,
				bool panelRaw,bool panelPlotlist)
{
	ASSERT(panelMode < comboPanelStartMode->GetCount());

	chkRawData->SetValue(panelRaw);
	chkControl->SetValue(panelControl);
	chkPlotlist->SetValue(panelPlotlist);

	comboPanelStartMode->SetSelection(panelMode);
	setStartupCheckboxEnables(panelMode);

}
void PrefDialog::getPanelDefaults(unsigned int &panelMode, bool &panelControl,
				bool &panelRaw,bool &panelPlotlist) const
{
	panelControl=chkControl->IsChecked();
	panelRaw=chkRawData->IsChecked();
	panelPlotlist=chkPlotlist->IsChecked();

	panelMode=comboPanelStartMode->GetSelection();
}


//-------------- End Startup panel page-----------------------


//------------Mouse page -----------
void PrefDialog::OnMouseZoomSlider(wxScrollEvent &event)
{
	mouseZoomRatePercent=sliderCamZoomRate->GetValue();
}
void PrefDialog::OnMouseMoveSlider(wxScrollEvent &event)
{
	mouseMoveRatePercent=sliderCamMoveRate->GetValue();
}
//------------End mouse page-------------
// wxGlade: add PrefDialog event handlers


void PrefDialog::set_properties()
{
    // begin wxGlade: PrefDialog::set_properties
    SetTitle(wxTRANS("Preferences"));
    SetSize(wxSize(640, 487));
    comboPanelStartMode->SetToolTip(wxTRANS("Set the method of panel layout when starting the program"));
    comboPanelStartMode->SetSelection(0);
#ifndef DISABLE_ONLINE_UPDATE
    checkAllowOnlineUpdate->SetToolTip(wxTRANS("Lets the program check the internet to see if updates to the program version are available, then notifies you about updates now and again."));
#endif
    sliderCamMoveRate->SetToolTip(wxTRANS("Camera translation, orbit and swivel rates. "));
    sliderCamZoomRate->SetToolTip(wxTRANS("Camera zooming rate."));
    // end wxGlade
}


void PrefDialog::do_layout()
{
	// begin wxGlade: PrefDialog::do_layout
	wxBoxSizer* panelSizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* exitButtonSizer = new wxBoxSizer(wxHORIZONTAL);
	wxStaticBoxSizer* sizerCamSpeed = new wxStaticBoxSizer(sizerCamSpeed_staticbox, wxVERTICAL);
	wxBoxSizer* sizer_6_copy = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* sizer_6 = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* sizer_1 = new wxBoxSizer(wxVERTICAL);
#ifndef DISABLE_ONLINE_UPDATE
	wxStaticBoxSizer* updateSizer = new wxStaticBoxSizer(updateSizer_staticbox, wxVERTICAL);
#endif
	wxStaticBoxSizer* sizer_2 = new wxStaticBoxSizer(sizer_2_staticbox, wxVERTICAL);
	wxBoxSizer* sizer_3 = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* sizer_4 = new wxBoxSizer(wxVERTICAL);
	wxStaticBoxSizer* filterPropSizer = new wxStaticBoxSizer(filterPropSizer_staticbox, wxHORIZONTAL);
	wxBoxSizer* filterRightSideSizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* resetButtonSizer = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* filterLeftSizer = new wxBoxSizer(wxVERTICAL);
	filterLeftSizer->Add(lblFilters, 0, 0, 0);
	filterLeftSizer->Add(listFilters, 1, wxEXPAND, 0);
	filterPropSizer->Add(filterLeftSizer, 1, wxEXPAND, 0);
	filterPropSizer->Add(20, 20, 0, 0, 0);
	filterRightSideSizer->Add(filterGridProperties, 1, wxEXPAND, 0);
	resetButtonSizer->Add(filterBtnResetAllFilters, 0, 0, 0);
	resetButtonSizer->Add(filterResetDefaultFilter, 0, 0, 0);
	resetButtonSizer->Add(20, 20, 1, 0, 0);
	filterRightSideSizer->Add(resetButtonSizer, 0, wxEXPAND, 0);
	filterPropSizer->Add(filterRightSideSizer, 2, wxEXPAND, 0);
	panelFilters->SetSizer(filterPropSizer);
	sizer_2->Add(comboPanelStartMode, 0, 0, 0);
	sizer_3->Add(20, 20, 0, 0, 0);
	sizer_4->Add(chkControl, 0, 0, 0);
	sizer_4->Add(chkRawData, 0, 0, 0);
	sizer_4->Add(chkPlotlist, 0, 0, 0);
	sizer_3->Add(sizer_4, 1, wxEXPAND, 0);
	sizer_2->Add(sizer_3, 1, wxEXPAND, 0);
	sizer_1->Add(sizer_2, 0, wxALL|wxEXPAND, 5);
#ifndef DISABLE_ONLINE_UPDATE
	updateSizer->Add(checkAllowOnlineUpdate, 0, 0, 0);
	sizer_1->Add(updateSizer, 0, wxALL|wxEXPAND, 5);
#endif
	panelStartup->SetSizer(sizer_1);
	sizer_6->Add(lblMoveSpeed, 0, wxALIGN_CENTER_VERTICAL, 0);
	sizer_6->Add(20, 20, 0, 0, 0);
	sizer_6->Add(labelSlowCamMoveRate, 0, wxALIGN_CENTER_VERTICAL, 0);
	sizer_6->Add(sliderCamMoveRate, 1, wxEXPAND|wxALIGN_CENTER_VERTICAL, 0);
	sizer_6->Add(labelFastCamMoveRate, 0, wxALIGN_CENTER_VERTICAL, 0);
	sizerCamSpeed->Add(sizer_6, 1, wxEXPAND, 0);
	sizer_6_copy->Add(lblZoomSpeed, 0, wxALIGN_CENTER_VERTICAL, 0);
	sizer_6_copy->Add(20, 20, 0, 0, 0);
	sizer_6_copy->Add(labelSlowCamZoomRate, 0, wxALIGN_CENTER_VERTICAL, 0);
	sizer_6_copy->Add(sliderCamZoomRate, 1, wxEXPAND|wxALIGN_CENTER_VERTICAL, 0);
	sizer_6_copy->Add(labelSlowFastZoomRate, 0, wxALIGN_CENTER_VERTICAL, 0);
	sizerCamSpeed->Add(sizer_6_copy, 1, wxEXPAND, 0);
	notePrefPanels_pane_3->SetSizer(sizerCamSpeed);
	notePrefPanels->AddPage(panelFilters, wxTRANS("Pref"));
	notePrefPanels->AddPage(panelStartup, wxTRANS("Startup"));
	notePrefPanels->AddPage(notePrefPanels_pane_3, wxTRANS("Camera"));
	panelSizer->Add(notePrefPanels, 2, wxEXPAND, 0);
	exitButtonSizer->Add(20, 20, 1, wxEXPAND, 0);
	exitButtonSizer->Add(btnOK, 0, wxTOP, 8);
	exitButtonSizer->Add(btnCancel, 0, wxLEFT|wxTOP|wxBOTTOM, 8);
	exitButtonSizer->Add(10, 20, 0, 0, 0);
	panelSizer->Add(exitButtonSizer, 0, wxEXPAND, 0);
	SetSizer(panelSizer);
	Layout();
	// end wxGlade
}

