/*
 *	autosaveDialog.cpp - Selection of autosaves for hard program restarts
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
// -*- C++ -*- generated by wxGlade 0.6.5 on Thu Jun  7 12:47:44 2012

#include "autosaveDialog.h"
#include "wx/wxcommon.h"
#include "common/translation.h"
// begin wxGlade: ::extracode

// end wxGlade



AutosaveDialog::AutosaveDialog(wxWindow* parent, int id, const wxString& title, const wxPoint& pos, const wxSize& size, long style):
    wxDialog(parent, id, title, pos, size, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER)
{

	selectedItem=(size_t)-1;
	haveRemovedItems=false;

    // begin wxGlade: AutosaveDialog::AutosaveDialog
    const wxString *listStates_choices = NULL;
    listStates = new wxListBox(this, ID_LIST_STATES, wxDefaultPosition, wxDefaultSize, 0, listStates_choices, wxLB_SINGLE|wxLB_NEEDED_SB);
    btnRemoveAll = new wxButton(this, ID_REMOVE_ALL, wxTRANS("Remove &All"));
    btnCancel = new wxButton(this, wxID_CANCEL, wxEmptyString);
    btnOK = new wxButton(this, wxID_OK, wxEmptyString);


    btnOK->Enable(false);
    set_properties();
    do_layout();
    // end wxGlade

    //Force setting focus on the list control, because otherwise, pressing ESCAPE doesn't work for the user
    listStates->SetFocus();
}


BEGIN_EVENT_TABLE(AutosaveDialog, wxDialog)
    // begin wxGlade: AutosaveDialog::event_table
    EVT_LISTBOX_DCLICK(ID_LIST_STATES, AutosaveDialog::OnListStatesDClick)
    EVT_LISTBOX(ID_LIST_STATES, AutosaveDialog::OnListStates)
    EVT_BUTTON(ID_REMOVE_ALL, AutosaveDialog::OnRemoveAll)
    EVT_BUTTON(wxID_CANCEL, AutosaveDialog::OnCancel)
    EVT_BUTTON(wxID_OK, AutosaveDialog::OnOK)
    // end wxGlade
END_EVENT_TABLE();

void AutosaveDialog::OnListStates(wxCommandEvent &event)
{
	//Get the first selected item
	selectedItem= listStates->GetSelection();

	//Enable the OK button if and only if there is a valid
	// selection
	btnOK->Enable(selectedItem != -1);

}

void AutosaveDialog::OnListStatesDClick(wxCommandEvent &event)
{
	EndModal(wxID_OK);
}


void AutosaveDialog::OnRemoveAll(wxCommandEvent &event)
{
	selectedItem=-1;
	btnOK->Enable(false);

	haveRemovedItems=true;
	btnRemoveAll->Enable(false);
	listStates->Clear();
	listStates->Enable(false);
	//Force setting focus now on the cancel button ,
	//because otherwise, pressing ESCAPE doesn't work
	// on the list control
	btnCancel->SetFocus();
}


void AutosaveDialog::OnCancel(wxCommandEvent &event)
{
	EndModal(wxID_CANCEL);
}


void AutosaveDialog::OnOK(wxCommandEvent &event)
{
	//Require an item to be selected
	ASSERT(selectedItem != -1)
	
	EndModal(wxID_OK);
}


// wxGlade: add AutosaveDialog event handlers

void AutosaveDialog::setItems( const std::vector<std::string> &newItems)
{
	for(size_t ui=0;ui<newItems.size();ui++)
		listStates->Insert(wxStr(newItems[ui]),ui);
}

void AutosaveDialog::set_properties()
{
    // begin wxGlade: AutosaveDialog::set_properties
    SetTitle(wxTRANS("Restore state?"));
    // end wxGlade
}


void AutosaveDialog::do_layout()
{
    // begin wxGlade: AutosaveDialog::do_layout
    wxBoxSizer* sizer_1 = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizer_2 = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* labelHeader = new wxStaticText(this, wxID_ANY, wxTRANS("Multiple autosave states were found; would you like to restore one?"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
    sizer_1->Add(labelHeader, 0, wxALL|wxALIGN_CENTER_HORIZONTAL, 8);
    sizer_1->Add(listStates, 1, wxALL|wxEXPAND, 8);
    sizer_2->Add(btnRemoveAll, 0, wxLEFT|wxBOTTOM, 8);
    sizer_2->Add(20, 20, 1, 0, 0);
    sizer_2->Add(btnCancel, 0, wxLEFT|wxRIGHT|wxBOTTOM, 8);
    sizer_2->Add(btnOK, 0, wxRIGHT|wxBOTTOM, 8);
    sizer_1->Add(sizer_2, 0, wxEXPAND, 0);
    SetSizer(sizer_1);
    sizer_1->Fit(this);
    Layout();
    // end wxGlade
}

