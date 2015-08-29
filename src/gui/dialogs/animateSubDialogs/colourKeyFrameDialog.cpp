/*
 *	colourKeyFrameDialog.cpp - Colour property keyframe selection dialog
 *	Copyright (C) 2015, D Haley 

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
// -*- C++ -*- generated by wxGlade 0.6.5 on Fri Sep 14 09:37:57 2012

#include "colourKeyFrameDialog.h"
#include "common/stringFuncs.h"
#include "wx/wxcommon.h"
#include "common/translation.h"

#include <wx/colordlg.h>


//TODO: Currently we change the foreground text in a button to set colour.
// better would be to use a "swatch"


using std::string;

// begin wxGlade: ::extracode
enum{
	ID_COMBO_TRANSITION,
	ID_BTN_START_VALUE,
	ID_BTN_FINAL_VALUE,
	ID_TEXT_INITIAL_VALUE,
	ID_TEXT_FRAME_START,
	ID_TEXT_FRAME_END
};

// end wxGlade

//FIXME: Is currently duplicated from realKeyframeDialog. Needs to be
// unified
enum
{
	TRANSITION_STEP,
	TRANSITION_INTERP,
	TRANSITION_END
};


ColourKeyFrameDialog::ColourKeyFrameDialog(wxWindow* parent, int id, const wxString& title, const wxPoint& pos, const wxSize& size, long style):
	wxDialog(parent, id, title, pos, size, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER|wxMAXIMIZE_BOX|wxMINIMIZE_BOX)
{
	// begin wxGlade: ColourKeyFrameDialog::ColourKeyFrameDialog
	sizerMainArea_staticbox = new wxStaticBox(this, -1, TRANS("Keyframe Data"));
	labelTransition = new wxStaticText(this, wxID_ANY, TRANS("Transition"));
	//FIXME: THis is declared in animator.h - use that def.
	const wxString comboTransition_choices[] = {
        TRANS("Step"),
        TRANS("Ramp")
    };
	comboTransition = new wxComboBox(this, ID_COMBO_TRANSITION, wxT(""), wxDefaultPosition, wxDefaultSize, 2, comboTransition_choices, wxCB_DROPDOWN|wxCB_READONLY);
	labelFrameStart = new wxStaticText(this, wxID_ANY, TRANS("Start Frame"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	textFrameStart = new wxTextCtrl(this, ID_TEXT_FRAME_START, wxEmptyString);
	labelFrameEnd = new wxStaticText(this, wxID_ANY, TRANS("End Frame"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	textFrameEnd = new wxTextCtrl(this, ID_TEXT_FRAME_END, wxEmptyString);
	verticalLine = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL);
	labelStartVal = new wxStaticText(this, wxID_ANY, TRANS("Initial Value"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	btnStartColour = new wxButton(this, ID_BTN_START_VALUE, TRANS("startColour"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
	labelFinalVal = new wxStaticText(this, wxID_ANY, TRANS("Final Value"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	btnEndColour = new wxButton(this, ID_BTN_FINAL_VALUE, TRANS("endColour"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
	buttonCancel = new wxButton(this, wxID_CANCEL, wxEmptyString);
	buttonOK = new wxButton(this, wxID_OK, wxEmptyString);

	comboTransition->SetSelection(1);
	transitionMode=TRANSITION_INTERP;
	endFrameOK=startFrameOK=false;
	startRed=startGreen=startBlue=0;		
	endRed=endGreen=endBlue=0;		


	updateOKButton();
	updateButtonColours();

	set_properties();
	do_layout();
	// end wxGlade
}


BEGIN_EVENT_TABLE(ColourKeyFrameDialog, wxDialog)
	// begin wxGlade: ColourKeyFrameDialog::event_table
	EVT_COMBOBOX(ID_COMBO_TRANSITION, ColourKeyFrameDialog::OnComboTransition)
	EVT_TEXT(ID_TEXT_FRAME_START, ColourKeyFrameDialog::OnTextStartFrame)
	EVT_TEXT(ID_TEXT_FRAME_END, ColourKeyFrameDialog::OnTextEndFrame)
	EVT_BUTTON(ID_BTN_START_VALUE, ColourKeyFrameDialog::OnBtnStartColour)
	EVT_BUTTON(ID_BTN_FINAL_VALUE, ColourKeyFrameDialog::OnBtnEndColour)
	// end wxGlade
END_EVENT_TABLE();

size_t ColourKeyFrameDialog::getStartFrame() const
{
	ASSERT(startFrameOK);
	return startFrame;
}

size_t ColourKeyFrameDialog::getEndFrame() const
{
	ASSERT(endFrameOK);
	return endFrame;
}

std::string ColourKeyFrameDialog::getEndValue() const 
{
	ASSERT(transitionMode!=TRANSITION_STEP);
	ColourRGBA rgb(endRed,endGreen,endBlue);
	return rgb.rgbString();
}

std::string ColourKeyFrameDialog::getStartValue() const
{
	ColourRGBA rgb(startRed,startGreen,startBlue);
	return rgb.rgbString();
}

void ColourKeyFrameDialog::OnComboTransition(wxCommandEvent &event)
{
	ASSERT(event.GetInt() < TRANSITION_END);

	transitionMode=event.GetInt();

	btnEndColour->Enable(transitionMode != TRANSITION_STEP);
	textFrameEnd->Enable(transitionMode!=TRANSITION_STEP);
}

void ColourKeyFrameDialog::OnTextStartFrame(wxCommandEvent &event)
{
	startFrameOK=validateTextAsStream(textFrameStart,startFrame);
	updateOKButton();
}


void ColourKeyFrameDialog::OnTextEndFrame(wxCommandEvent &event)
{
	endFrameOK=validateTextAsStream(textFrameEnd,endFrame);
	updateOKButton();
}


void ColourKeyFrameDialog::OnBtnStartColour(wxCommandEvent &event)
{
	wxColourData d;
	wxColourDialog *colDg=new wxColourDialog(this->GetParent(),&d);

	if( colDg->ShowModal() == wxID_OK)
	{
		wxColour c;
		//Change the colour
		c=colDg->GetColourData().GetColour();

		startRed=c.Red();
		startGreen=c.Green();
		startBlue=c.Blue();
		
		updateButtonColours();
	}

	delete colDg;
}


void ColourKeyFrameDialog::OnBtnEndColour(wxCommandEvent &event)
{
	wxColourData d;
	wxColourDialog *colDg=new wxColourDialog(this->GetParent(),&d);

	if( colDg->ShowModal() == wxID_OK)
	{
		wxColour c;
		//Change the colour
		c=colDg->GetColourData().GetColour();

		endRed=c.Red();
		endGreen=c.Green();
		endBlue=c.Blue();

		updateButtonColours();
	}

	colDg->Destroy();
}

void ColourKeyFrameDialog::updateOKButton()
{
	bool isOK=true;
	isOK&=startFrameOK;
	isOK&= (endFrameOK || transitionMode == TRANSITION_STEP) ;

	if(isOK)
	{
		if(transitionMode == TRANSITION_INTERP) 
		{
			//Ensure start frame is > end frame, 
			// if we are using interp mode
			isOK&=(startFrame<endFrame);
		}

		if(!isOK)
		{
			//If there is a problem, mark the problem with a colour background
			textFrameStart->SetBackgroundColour(*wxCYAN);
			if(transitionMode == TRANSITION_INTERP)
				textFrameEnd->SetBackgroundColour(*wxCYAN);
			else
				textFrameEnd->SetBackgroundColour(wxNullColour);
				
		}
		else
		{
			textFrameStart->SetBackgroundColour(wxNullColour);
			textFrameEnd->SetBackgroundColour(wxNullColour);
		}
	}

	//Enable the OK button if all properties are set appropriately
	buttonOK->Enable(isOK);
}

void ColourKeyFrameDialog::updateButtonColours()
{
	wxColour colD;
	colD.Set(startRed,startGreen,startBlue);
	btnStartColour->SetForegroundColour(colD);
	
	colD.Set(endRed,endGreen,endBlue);
	btnEndColour->SetForegroundColour(colD);

}
// wxGlade: add ColourKeyFrameDialog event handlers


void ColourKeyFrameDialog::set_properties()
{
	// begin wxGlade: ColourKeyFrameDialog::set_properties
	SetTitle(TRANS("Key Frame : Colour"));
	comboTransition->SetSelection(-1);
	btnStartColour->SetToolTip(TRANS("Colour at the start of the transtition"));
	btnEndColour->SetToolTip(TRANS("Colour at end of transition"));
	// end wxGlade
}


void ColourKeyFrameDialog::do_layout()
{
	// begin wxGlade: ColourKeyFrameDialog::do_layout
	wxBoxSizer* sizerTop = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* sizerBUttons = new wxBoxSizer(wxHORIZONTAL);
	wxStaticBoxSizer* sizerMainArea = new wxStaticBoxSizer(sizerMainArea_staticbox, wxHORIZONTAL);
	wxBoxSizer* sizerRight = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* sizerRightFinal = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* sizerRightInitial = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* sizerLeft = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* sizerEndFrame = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* sizerStartFrame = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* sizerTransition = new wxBoxSizer(wxHORIZONTAL);
	sizerTop->Add(20, 5, 0, 0, 0);
	sizerMainArea->Add(10, 20, 0, 0, 0);
	sizerLeft->Add(20, 10, 0, 0, 0);
	sizerTransition->Add(labelTransition, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 14);
	sizerTransition->Add(comboTransition, 1, wxLEFT|wxALIGN_CENTER_VERTICAL, 5);
	sizerLeft->Add(sizerTransition, 1, wxEXPAND, 0);
	sizerStartFrame->Add(labelFrameStart, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5);
	sizerStartFrame->Add(textFrameStart, 1, wxLEFT|wxALIGN_CENTER_VERTICAL, 4);
	sizerLeft->Add(sizerStartFrame, 1, wxEXPAND|wxALIGN_CENTER_VERTICAL, 0);
	sizerEndFrame->Add(labelFrameEnd, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 12);
	sizerEndFrame->Add(textFrameEnd, 1, wxLEFT|wxALIGN_CENTER_VERTICAL, 4);
	sizerLeft->Add(sizerEndFrame, 1, wxEXPAND|wxALIGN_CENTER_VERTICAL, 0);
	sizerLeft->Add(20, 10, 0, 0, 0);
	sizerMainArea->Add(sizerLeft, 2, wxEXPAND, 0);
	sizerMainArea->Add(verticalLine, 0, wxLEFT|wxRIGHT|wxEXPAND, 5);
	sizerRight->Add(20, 20, 1, 0, 0);
	sizerRightInitial->Add(20, 20, 1, 0, 0);
	sizerRightInitial->Add(labelStartVal, 2, wxRIGHT|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 0);
	sizerRightInitial->Add(btnStartColour, 0, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 3);
	sizerRightInitial->Add(20, 20, 1, 0, 0);
	sizerRight->Add(sizerRightInitial, 1, wxEXPAND|wxALIGN_CENTER_VERTICAL, 0);
	sizerRightFinal->Add(20, 20, 1, 0, 0);
	sizerRightFinal->Add(labelFinalVal, 2, wxRIGHT|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 9);
	sizerRightFinal->Add(btnEndColour, 0, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 3);
	sizerRightFinal->Add(20, 20, 1, 0, 0);
	sizerRight->Add(sizerRightFinal, 1, wxEXPAND|wxALIGN_CENTER_VERTICAL, 0);
	sizerRight->Add(20, 20, 1, 0, 0);
	sizerMainArea->Add(sizerRight, 2, wxEXPAND, 0);
	sizerMainArea->Add(10, 20, 0, 0, 0);
	sizerTop->Add(sizerMainArea, 1, wxEXPAND, 0);
	sizerBUttons->Add(20, 20, 1, 0, 0);
	sizerBUttons->Add(buttonCancel, 0, wxALL, 4);
	sizerBUttons->Add(buttonOK, 0, wxALL, 4);
	sizerTop->Add(sizerBUttons, 0, wxEXPAND, 0);
	SetSizer(sizerTop);
	sizerTop->Fit(this);
	Layout();
	// end wxGlade
}

