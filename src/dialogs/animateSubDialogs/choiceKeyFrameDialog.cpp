// -*- C++ -*- generated by wxGlade 0.6.5 on Sun Sep 23 22:52:41 2012

#include "choiceKeyFrameDialog.h"

// begin wxGlade: ::extracode

// end wxGlade

#include "../../assertion.h"
#include "../../basics.h"
#include "../../wxcommon.h"


ChoiceKeyFrameDialog::ChoiceKeyFrameDialog(wxWindow* parent, int id, const wxString& title, const wxPoint& pos, const wxSize& size, long style):
	wxDialog(parent, id, title, pos, size, wxDEFAULT_DIALOG_STYLE)
{
	// begin wxGlade: ChoiceKeyFrameDialog::ChoiceKeyFrameDialog
	labelFrame = new wxStaticText(this, wxID_ANY, wxT("Frame"));
	textFrame = new wxTextCtrl(this, ID_TEXT_FRAME, wxEmptyString);
	labelSelection = new wxStaticText(this, wxID_ANY, wxT("Selection"));
	comboChoice = new wxComboBox(this, ID_COMBO_CHOICE, wxT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN|wxCB_SIMPLE|wxCB_READONLY);
	btnCancel = new wxButton(this, wxID_CANCEL, wxEmptyString);
	btnOK = new wxButton(this, wxID_OK, wxEmptyString);

	startFrameOK=false;
	

	set_properties();
	do_layout();
	// end wxGlade
}


BEGIN_EVENT_TABLE(ChoiceKeyFrameDialog, wxDialog)
	// begin wxGlade: ChoiceKeyFrameDialog::event_table
	EVT_COMBOBOX(ID_COMBO_CHOICE, ChoiceKeyFrameDialog::OnChoiceCombo)
	EVT_TEXT(ID_TEXT_FRAME, ChoiceKeyFrameDialog::OnFrameText)
	// end wxGlade
END_EVENT_TABLE();


void ChoiceKeyFrameDialog::OnChoiceCombo(wxCommandEvent &event)
{
	choice=comboChoice->GetSelection();
}

void ChoiceKeyFrameDialog::OnFrameText(wxCommandEvent &event)
{
	if(validateTextAsStream(textFrame,startFrame,intIsPositive))
		startFrameOK=true;

	updateOKButton();
}

// wxGlade: add ChoiceKeyFrameDialog event handlers

void ChoiceKeyFrameDialog::updateOKButton() 
{
	btnOK->Enable(startFrameOK);
}

void ChoiceKeyFrameDialog::buildCombo(size_t defaultChoice)
{
	ASSERT(choiceStrings.size());


	comboChoice->Clear();

	for(size_t ui=0;ui<choiceStrings.size();ui++)
		comboChoice->Append(wxStr(choiceStrings[ui]));

	comboChoice->SetSelection(defaultChoice);
}


void ChoiceKeyFrameDialog::setChoices(const std::vector<std::string> &choices,
					size_t defChoice)
{
	choiceStrings=choices;

	buildCombo(defChoice);
	choice=defChoice;
}

void ChoiceKeyFrameDialog::set_properties()
{
	// begin wxGlade: ChoiceKeyFrameDialog::set_properties
	SetTitle(wxT("Key Frame"));
	// end wxGlade
}


void ChoiceKeyFrameDialog::do_layout()
{
	// begin wxGlade: ChoiceKeyFrameDialog::do_layout
	wxBoxSizer* frameSizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* comboSizer = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* textSizer = new wxBoxSizer(wxHORIZONTAL);
	textSizer->Add(labelFrame, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 20);
	textSizer->Add(textFrame, 0, wxLEFT|wxALIGN_CENTER_VERTICAL, 5);
	frameSizer->Add(textSizer, 0, wxALL|wxEXPAND, 10);
	comboSizer->Add(labelSelection, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5);
	comboSizer->Add(comboChoice, 0, wxLEFT, 5);
	frameSizer->Add(comboSizer, 0, wxLEFT|wxRIGHT|wxTOP|wxEXPAND, 10);
	buttonSizer->Add(20, 20, 1, 0, 0);
	buttonSizer->Add(btnCancel, 0, wxRIGHT, 5);
	buttonSizer->Add(btnOK, 0, wxLEFT, 5);
	frameSizer->Add(buttonSizer, 0, wxALL|wxEXPAND, 5);
	SetSizer(frameSizer);
	frameSizer->Fit(this);
	Layout();
	// end wxGlade
}

