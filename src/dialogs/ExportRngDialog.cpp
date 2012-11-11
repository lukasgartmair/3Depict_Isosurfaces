/*
 *	ExportRngDialog.cpp  - "Range" data export dialog
 *	Copyright (C) 2012, D Haley 

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

#include "ExportRngDialog.h"
#include "../wxcommon.h"
#include "../translation.h"


#include "../filters/rangeFile.h"

#include <fstream>
// begin wxGlade: ::extracode

// end wxGlade

enum
{
    ID_LIST_ACTIVATE=wxID_ANY+1,
};

ExportRngDialog::ExportRngDialog(wxWindow* parent, int id, const wxString& title, const wxPoint& pos, const wxSize& size, long style):
    wxDialog(parent, id, title, pos, size, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER)
{
    // begin wxGlade: ExportRngDialog::ExportRngDialog
    lblRanges = new wxStaticText(this, wxID_ANY, wxTRANS("Range Sources"));
    listRanges = new wxListCtrl(this, ID_LIST_ACTIVATE, wxDefaultPosition, wxDefaultSize, wxLC_REPORT|wxSUNKEN_BORDER);
    label_3 = new wxStaticText(this, wxID_ANY, wxTRANS("Details"));
    gridDetails = new wxGrid(this, wxID_ANY);
    btnOK = new wxButton(this, wxID_SAVE, wxEmptyString);
    btnCancel = new wxButton(this, wxID_CANCEL, wxEmptyString);
    btnOK->SetFocus();

    set_properties();
    do_layout();
    // end wxGlade

    //Add columns to report listviews
    listRanges->InsertColumn(0,wxTRANS("Source Filter"));
    listRanges->InsertColumn(1,wxTRANS("Ions"));
    listRanges->InsertColumn(2,wxTRANS("Ranges"));

}


BEGIN_EVENT_TABLE(ExportRngDialog, wxDialog)
    // begin wxGlade: ExportRngDialog::event_table
    EVT_LIST_ITEM_ACTIVATED(ID_LIST_ACTIVATE, ExportRngDialog::OnListRangeItemActivate)
    EVT_BUTTON(wxID_SAVE, ExportRngDialog::OnSave)
    EVT_BUTTON(wxID_CANCEL, ExportRngDialog::OnCancel)
    // end wxGlade
END_EVENT_TABLE();


void ExportRngDialog::OnListRangeItemActivate(wxListEvent &event)
{
	updateGrid(event.GetIndex());

	selectedRange=event.GetIndex();
}

void ExportRngDialog::updateGrid(unsigned int index)
{
	const RangeFileFilter *rangeData;
	rangeData=(RangeFileFilter *)rngFilters[index];

	gridDetails->BeginBatch();
    	if (gridDetails->GetNumberCols())
        	gridDetails->DeleteCols(0,gridDetails->GetNumberCols());
    	if (gridDetails->GetNumberRows())
        	gridDetails->DeleteRows(0,gridDetails->GetNumberRows());

	gridDetails->AppendCols(3);
	gridDetails->SetColLabelValue(0,wxTRANS("Param"));
	gridDetails->SetColLabelValue(1,wxTRANS("Value"));
	gridDetails->SetColLabelValue(2,wxTRANS("Value2"));

	unsigned int nRows;
	nRows=rangeData->getRange().getNumIons()+rangeData->getRange().getNumRanges() + 4;
	gridDetails->AppendRows(nRows);


	gridDetails->SetCellValue(0,0,wxTRANS("Ion Name"));
	gridDetails->SetCellValue(0,1,wxTRANS("Num Ranges"));
	unsigned int row=1;
	std::string tmpStr;

	unsigned int maxNum;
	maxNum=rangeData->getRange().getNumIons();
	//Add ion data, then range data
	for(unsigned int ui=0;ui<maxNum; ui++)
	{	
		//Use format 
		// ION NAME  | NUMBER OF RANGES
		gridDetails->SetCellValue(row,0,wxStr(rangeData->getRange().getName(ui)));
		stream_cast(tmpStr,rangeData->getRange().getNumRanges(ui));
		gridDetails->SetCellValue(row,1,wxStr(tmpStr));
		row++;
	}

	row++;	
	gridDetails->SetCellValue(row,0,wxTRANS("Ion"));
	gridDetails->SetCellValue(row,1,wxTRANS("Range Start"));
	gridDetails->SetCellValue(row,2,wxTRANS("Range end"));
	row++;	

	maxNum=rangeData->getRange().getNumRanges();
	for(unsigned int ui=0;ui<maxNum; ui++)
	{
		std::pair<float,float> rngPair;
		unsigned int ionID;

		rngPair=rangeData->getRange().getRange(ui);
		ionID=rangeData->getRange().getIonID(ui);
		gridDetails->SetCellValue(row,0,
			wxStr(rangeData->getRange().getName(ionID)));

		stream_cast(tmpStr,rngPair.first);
		gridDetails->SetCellValue(row,1,wxStr(tmpStr));

		stream_cast(tmpStr,rngPair.second);
		gridDetails->SetCellValue(row,2,wxStr(tmpStr));
			
		row++;	
	}	

	gridDetails->EndBatch();
}

void ExportRngDialog::OnSave(wxCommandEvent &event)
{

	if(rngFilters.empty())
		EndModal(wxID_CANCEL);

	//create a file chooser for later.
	wxFileDialog *wxF = new wxFileDialog(this,wxTRANS("Save pos..."), wxT(""),
		wxT(""),wxTRANS("ORNL format RNG (*.rng)|*.rng|All Files (*)|*"),wxFD_SAVE);
	//Show, then check for user cancelling export dialog
	if(wxF->ShowModal() == wxID_CANCEL)
	{
		wxF->Destroy();
		return;	
	}
	
	std::string dataFile = stlStr(wxF->GetPath());


	if(((RangeFileFilter *)(rngFilters[selectedRange]))->
				getRange().write(dataFile.c_str()))
	{
		std::string errString;
		errString=TRANS("Unable to save. Check output destination can be written to.");
		
		wxMessageDialog *wxD  =new wxMessageDialog(this,wxStr(errString)
						,wxTRANS("Save error"),wxOK|wxICON_ERROR);
		wxD->ShowModal();
		wxD->Destroy();
		return;
	}

	EndModal(wxID_OK);
}

void ExportRngDialog::OnCancel(wxCommandEvent &event)
{
	EndModal(wxID_CANCEL);
}
// wxGlade: add ExportRngDialog event handlers


void ExportRngDialog::addRangeData(std::vector<const Filter *> rangeData)
{
#ifdef DEBUG
	//This function should only recieve rangefile filters
	for(unsigned int ui=0;ui<rangeData.size(); ui++)
		ASSERT(rangeData[ui]->getType() == FILTER_TYPE_RANGEFILE);
#endif

	rngFilters.resize(rangeData.size());
	std::copy(rangeData.begin(),rangeData.end(),rngFilters.begin());

	updateRangeList();

	if(rangeData.size())
	{
		//Use the first item to populate the grid
		updateGrid(0);
		//select the first item
		listRanges->SetItemState(0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);

		selectedRange=0;
	}
}


void ExportRngDialog::updateRangeList()
{
	listRanges->DeleteAllItems();
	for(unsigned int ui=0;ui<rngFilters.size(); ui++)
	{
		const RangeFileFilter *rangeData;
		rangeData=(RangeFileFilter *)rngFilters[ui];
		std::string tmpStr;
		long itemIndex;
	       	itemIndex=listRanges->InsertItem(0, wxStr(rangeData->getUserString())); 
		unsigned int nIons,nRngs; 
		nIons = rangeData->getRange().getNumIons();
		nRngs = rangeData->getRange().getNumIons();

		stream_cast(tmpStr,nIons);
		listRanges->SetItem(itemIndex, 1, wxStr(tmpStr)); 
		stream_cast(tmpStr,nRngs);
		listRanges->SetItem(itemIndex, 2, wxStr(tmpStr)); 
		
	}
}

void ExportRngDialog::set_properties()
{
    // begin wxGlade: ExportRngDialog::set_properties
    SetTitle(wxTRANS("Export Range"));
    gridDetails->CreateGrid(0, 0);
	gridDetails->SetRowLabelSize(0);
	gridDetails->SetColLabelSize(0);

    listRanges->SetToolTip(wxTRANS("List of rangefiles in filter tree"));
    gridDetails->EnableEditing(false);
    gridDetails->SetToolTip(wxTRANS("Detailed view of selected range"));
    // end wxGlade
}


void ExportRngDialog::do_layout()
{
    // begin wxGlade: ExportRngDialog::do_layout
    wxBoxSizer* sizer_2 = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizer_3 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizer_14 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizer_15 = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizer_16 = new wxBoxSizer(wxVERTICAL);
    sizer_16->Add(lblRanges, 0, wxLEFT|wxTOP, 5);
    sizer_16->Add(listRanges, 1, wxALL|wxEXPAND, 5);
    sizer_14->Add(sizer_16, 1, wxEXPAND, 0);
    sizer_14->Add(10, 20, 0, 0, 0);
    sizer_15->Add(label_3, 0, wxLEFT|wxTOP, 5);
    sizer_15->Add(gridDetails, 1, wxALL|wxEXPAND, 5);
    sizer_14->Add(sizer_15, 1, wxEXPAND, 0);
    sizer_2->Add(sizer_14, 1, wxEXPAND, 0);
    sizer_3->Add(20, 20, 1, 0, 0);
    sizer_3->Add(btnOK, 0, wxALL, 5);
    sizer_3->Add(btnCancel, 0, wxALL, 5);
    sizer_2->Add(sizer_3, 0, wxEXPAND, 0);
    SetSizer(sizer_2);
    sizer_2->Fit(this);
    Layout();
    // end wxGlade
}

