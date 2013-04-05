/*
 *	stringKeyFrameDialog.cpp - String value keyframe selection dialog
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
// -*- C++ -*- generated by wxGlade 0.6.5 on Sat Sep 22 21:13:08 2012

#include "stringKeyFrameDialog.h"

#include "common/translation.h"

#include "wxcommon.h"

using std::string;
using std::vector;

// begin wxGlade: ::extracode
enum
{
	ID_TEXT_START_FRAME,
	ID_RADIO_FROM_FILE,
	ID_TEXT_FROM_FILE,
	ID_RADIO_FROM_TABLE,
	ID_GRID_STRINGS
};
// end wxGlade


StringKeyFrameDialog::StringKeyFrameDialog(wxWindow* parent, int id, const wxString& title, const wxPoint& pos, const wxSize& size, long style):
	wxDialog(parent, id, title, pos, size, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER)
{
	// begin wxGlade: StringKeyFrameDialog::StringKeyFrameDialog
	labelStartFrame = new wxStaticText(this, wxID_ANY, wxTRANS("Start Frame: "));
	textStartFrame = new wxTextCtrl(this, ID_TEXT_START_FRAME, wxEmptyString);
	radioFromFile = new wxRadioButton(this, ID_RADIO_FROM_FILE, wxTRANS("From File"));
	textFilename = new wxTextCtrl(this, ID_TEXT_FROM_FILE, wxEmptyString);
	btnChooseFile = new wxButton(this, wxID_OPEN, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
	radioFromTable = new wxRadioButton(this, ID_RADIO_FROM_TABLE, wxTRANS("From Table"));
	gridStrings = new wxGrid(this, ID_GRID_STRINGS);
	btnStringAdd = new wxButton(this, wxID_ADD, wxEmptyString);
	btnRemove = new wxButton(this, wxID_REMOVE, wxEmptyString);
	btnCancel = new wxButton(this, wxID_CANCEL, wxEmptyString);
	btnOK = new wxButton(this, wxID_OK, wxEmptyString);



	set_properties();
	do_layout();
	// end wxGlade
	startFrame=0;
	startFrameOK=filenameOK=false;
	string s;
	stream_cast(s,startFrame);
	textStartFrame->SetValue(wxStr(s));

	radioFromFile->SetValue(true);

	dataSourceFromFile=radioFromFile->GetValue();
}


BEGIN_EVENT_TABLE(StringKeyFrameDialog, wxDialog)
	// begin wxGlade: StringKeyFrameDialog::event_table
	EVT_TEXT(ID_TEXT_START_FRAME, StringKeyFrameDialog::OnTextStart)
	EVT_RADIOBUTTON(ID_RADIO_FROM_FILE, StringKeyFrameDialog::OnFileRadio)
	EVT_TEXT(ID_TEXT_FROM_FILE, StringKeyFrameDialog::OnTextFilename)
	EVT_BUTTON(wxID_OPEN, StringKeyFrameDialog::OnBtnChooseFile)
	EVT_RADIOBUTTON(ID_RADIO_FROM_TABLE, StringKeyFrameDialog::OnTableRadio)
	EVT_GRID_CMD_CELL_CHANGE(ID_GRID_STRINGS, StringKeyFrameDialog::OnGridCellChange)
	EVT_GRID_CMD_EDITOR_SHOWN(ID_GRID_STRINGS, StringKeyFrameDialog::OnGridEditorShown)
	EVT_BUTTON(wxID_ADD, StringKeyFrameDialog::OnBtnAdd)
	EVT_BUTTON(wxID_REMOVE, StringKeyFrameDialog::OnBtnRemove)
	// end wxGlade
END_EVENT_TABLE();

size_t StringKeyFrameDialog::getStartFrame() const
{
	ASSERT(startFrameOK);
	return startFrame;
}

void StringKeyFrameDialog::buildGrid()
{
	gridStrings->BeginBatch();
	//Empty the grid
	if(gridStrings->GetNumberCols())
		gridStrings->DeleteCols(0,gridStrings->GetNumberCols());
	if(gridStrings->GetNumberRows())
		gridStrings->DeleteRows(0,gridStrings->GetNumberRows());

	gridStrings->AppendCols(2);
	gridStrings->SetColLabelValue(0,wxTRANS("Frame"));
	gridStrings->SetColLabelValue(1,wxTRANS("Value"));

	gridStrings->AppendRows(valueStrings.size());
	
	wxGridCellAttr *readOnlyColAttr=new wxGridCellAttr;
	readOnlyColAttr->SetReadOnly(true);
	gridStrings->SetColAttr(0,readOnlyColAttr);

	for(size_t ui=0;ui<valueStrings.size();ui++)
	{
		std::string pos;
		stream_cast(pos,ui+startFrame);

		//set the frame-value pair
		gridStrings->SetCellValue(ui,0,wxStr(pos));
		gridStrings->SetCellValue(ui,1,wxStr(valueStrings[ui]));
	}
	gridStrings->EndBatch();
}

bool StringKeyFrameDialog::getStrings(vector<string> &res) const
{
	if(dataSourceFromFile)
	{
		vector<vector<string> > dataVec;
		if(loadTextStringData(dataFilename.c_str(), dataVec,"\t"))
				return false;

		res.reserve(dataVec.size());
		for(size_t ui=0;ui<dataVec.size();ui++)
		{
			if(dataVec[ui].size())
				res.push_back(dataVec[ui][0]);
		}

	}
	else
	{
		res=valueStrings;
	}

	return true;
}

void StringKeyFrameDialog::OnTextStart(wxCommandEvent &event)
{
	startFrameOK=validateTextAsStream(textStartFrame,startFrame);
	updateOKButton();
	buildGrid();
}

void StringKeyFrameDialog::OnFileRadio(wxCommandEvent &event)
{
	dataSourceFromFile=radioFromFile->GetValue();
	if(dataSourceFromFile)
	{
		//Disable grid & enable text box
		gridStrings->Enable(false);
		textFilename->Enable(true);
		btnChooseFile->Enable(true);
	}
	else
	{
		//Enable grid & disable text box
		gridStrings->Enable(true);
		textFilename->Enable(false);
		btnChooseFile->Enable(false);
	}
}

void StringKeyFrameDialog::OnTableRadio(wxCommandEvent &event)
{
	dataSourceFromFile=!radioFromTable->GetValue();
	if(dataSourceFromFile)
	{
		//Disable grid & enable text box
		gridStrings->Enable(false);
		textFilename->Enable(true);
		btnChooseFile->Enable(true);
	}
	else
	{
		//Enable grid & disable text box
		gridStrings->Enable(true);
		textFilename->Enable(false);
		btnChooseFile->Enable(false);
	}
}
void StringKeyFrameDialog::OnTextFilename(wxCommandEvent &event)
{
	if(!wxFileExists(textFilename->GetValue()))
	{
		textFilename->SetBackgroundColour(*wxCYAN);
		filenameOK=false;
		dataFilename.clear();
	}
	else 
	{
		textFilename->SetBackgroundColour(wxNullColour);
		filenameOK=true;
		dataFilename=stlStr(textFilename->GetValue());
	}

	updateOKButton();
}

void StringKeyFrameDialog::updateOKButton()
{
	bool isOK=true;
	isOK&=startFrameOK;

	if(dataSourceFromFile)
		isOK&=filenameOK;
	else
		isOK&=(bool)(valueStrings.size());

	btnOK->Enable(isOK);
}

void StringKeyFrameDialog::OnGridCellChange(wxGridEvent &event)
{
	std::string s;
	s=stlStr(gridStrings->GetCellValue(event.GetRow(),event.GetCol()));
	valueStrings[event.GetRow()] = s;
}

void StringKeyFrameDialog::OnGridEditorShown(wxGridEvent &event)
{
	event.Skip();
	wxLogDebug(wxT("Event handler (StringKeyFrameDialog::OnGridEditorShown) not implemented yet")); //notify the user that he hasn't implemented the event handler yet
}

void StringKeyFrameDialog::OnBtnChooseFile(wxCommandEvent &event)
{
	//Pop up a directory dialog, to choose the base path for the new folder
	wxFileDialog *wxD = new wxFileDialog(this,wxTRANS("Select text file..."), wxT(""),
		wxT(""),wxTRANS("Text files (*.txt)|*.txt;|All Files (*)|*"),wxFD_OPEN|wxFD_FILE_MUST_EXIST);

	unsigned int res;
	res = wxD->ShowModal();

	while(res != wxID_CANCEL)
	{
		//If dir exists, exit
		if(wxFileExists(wxD->GetPath()))
			break;

		res=wxD->ShowModal();
	}

	//User aborted directory choice.
	if(res==wxID_CANCEL)
	{
		wxD->Destroy();
		return;
	}

	textFilename->SetValue(wxD->GetPath());
	wxD->Destroy();
}


void StringKeyFrameDialog::OnBtnAdd(wxCommandEvent &event)
{
	wxMouseState wxm = wxGetMouseState();
	if(wxm.ShiftDown())
		valueStrings.resize(valueStrings.size() + 5,"");
	else if (wxm.CmdDown())
		valueStrings.resize(valueStrings.size() + 10,"");
	else
		valueStrings.push_back("");
	buildGrid();
	updateOKButton();
}

void StringKeyFrameDialog::OnBtnRemove(wxCommandEvent &event)
{
	if(!gridStrings->GetNumberRows())
		return;
	// Whole selected rows.
	const wxArrayInt& rows(gridStrings->GetSelectedRows());
	for (size_t i = rows.size(); i--; )
	{
		valueStrings.erase(valueStrings.begin() + rows[i],
				valueStrings.begin()+rows[i]+1);
	}


	if(!rows.size())
	{
	
		//Obtain the IDs of the selected rows, or partially selected rows
		wxGridCellCoordsArray selectedRows = gridStrings->GetSelectedCells();
	    
		wxGridCellCoordsArray arrayTL(gridStrings->GetSelectionBlockTopLeft());
		wxGridCellCoordsArray arrayBR(gridStrings->GetSelectionBlockBottomRight());
		
		//This is an undocumented class AFAIK. :(
		if(arrayTL.Count() && arrayBR.Count())
		{
			//Grab the coordinates f the selection block,
			// top left (TL) and bottom right (BR)
			wxGridCellCoords coordTL = arrayTL.Item(0);
			wxGridCellCoords coordBR = arrayBR.Item(0);

			size_t rows = coordBR.GetRow() - coordTL.GetRow()+1;

			ASSERT(coordTL.GetRow()+rows-1<valueStrings.size());
			//work out which rows we want to delete
			valueStrings.erase(valueStrings.begin() + coordTL.GetRow(),
					valueStrings.begin()+coordTL.GetRow() + rows);
		}
		else
		{
			size_t rowToKill;
			rowToKill=gridStrings->GetGridCursorRow();
			ASSERT(rowToKill<valueStrings.size());
			valueStrings.erase(valueStrings.begin()+rowToKill,
						valueStrings.begin()+rowToKill+1);
		
		}
	}


	//update user interface
	buildGrid();
	updateOKButton();

}


// wxGlade: add StringKeyFrameDialog event handlers


void StringKeyFrameDialog::set_properties()
{
	// begin wxGlade: StringKeyFrameDialog::set_properties
	SetTitle(wxTRANS("String Keyframes"));
	SetSize(wxSize(542, 412));
	SetToolTip(wxTRANS("Frame at which to start string sequence"));
	textStartFrame->SetToolTip(wxTRANS("Frame offset for data start"));
	textFilename->SetToolTip(wxTRANS("File to use as string data source, one value per row"));
	btnChooseFile->SetToolTip(wxTRANS("Select file to use as data source"));
	radioFromTable->SetToolTip(wxTRANS("Use table below for data source"));
	gridStrings->CreateGrid(0, 2);
	gridStrings->SetColLabelValue(0, wxTRANS("Frame"));
	gridStrings->SetColSize(0, 1);
	gridStrings->SetColLabelValue(1, wxTRANS("Value"));
	gridStrings->SetColSize(1, 3);
	btnStringAdd->SetToolTip(wxTRANS("Add new data rows to table, hold shift/cmd to insert multiple rows"));
	btnRemove->SetToolTip(wxTRANS("Remove selected strings from table"));
	btnCancel->SetToolTip(wxTRANS("Abort value selection and return to previous window"));
	btnOK->SetToolTip(wxTRANS("Accept data values"));
	// end wxGlade
}


void StringKeyFrameDialog::do_layout()
{
	// begin wxGlade: StringKeyFrameDialog::do_layout
	wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* footerSizer = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* gridAreaSizer = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* gridButtonSizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* fromFileSizer = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* startFrameSizer = new wxBoxSizer(wxHORIZONTAL);
	startFrameSizer->Add(labelStartFrame, 0, wxALIGN_CENTER_VERTICAL, 0);
	startFrameSizer->Add(textStartFrame, 1, wxLEFT|wxALIGN_CENTER_VERTICAL, 6);
	startFrameSizer->Add(20, 20, 1, 0, 0);
	mainSizer->Add(startFrameSizer, 0, wxALL|wxEXPAND, 6);
	fromFileSizer->Add(radioFromFile, 0, wxALIGN_CENTER_VERTICAL, 0);
	fromFileSizer->Add(textFilename, 1, wxLEFT|wxRIGHT|wxALIGN_CENTER_VERTICAL, 6);
	fromFileSizer->Add(btnChooseFile, 0, wxLEFT|wxRIGHT, 6);
	mainSizer->Add(fromFileSizer, 0, wxALL|wxEXPAND, 6);
	mainSizer->Add(radioFromTable, 0, wxLEFT, 6);
	gridAreaSizer->Add(gridStrings, 1, wxALL|wxEXPAND, 5);
	gridButtonSizer->Add(btnStringAdd, 0, wxBOTTOM, 5);
	gridButtonSizer->Add(btnRemove, 0, 0, 0);
	gridAreaSizer->Add(gridButtonSizer, 0, wxRIGHT|wxEXPAND, 5);
	mainSizer->Add(gridAreaSizer, 1, wxEXPAND, 0);
	footerSizer->Add(20, 20, 1, 0, 0);
	footerSizer->Add(btnCancel, 0, wxALL, 5);
	footerSizer->Add(btnOK, 0, wxALL, 5);
	mainSizer->Add(footerSizer, 0, wxEXPAND, 0);
	SetSizer(mainSizer);
	Layout();
	// end wxGlade
}
