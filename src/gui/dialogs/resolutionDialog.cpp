/*
 *	resolutionDialog.cpp - Resolution chooser dialog
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
// -*- C++ -*- generated by wxGlade 0.6.3 on Mon May  7 00:46:06 2012

#include "resolutionDialog.h"

#include "wx/wxcommon.h"
#include "common/translation.h"

#include <wx/dcbuffer.h>

// begin wxGlade: ::extracode

// end wxGlade


enum
{
  ID_RESET=wxID_ANY+1,
  ID_TEXT_WIDTH,
  ID_TEXT_HEIGHT,
};

const float MOUSEWHEEL_RATE_MULTIPLIER=1.0f;

ResolutionDialog::ResolutionDialog(wxWindow* parent, int id, const wxString& title, const wxPoint& pos, const wxSize& size, long style):
    wxDialog(parent, id, title, pos, size, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER)
{
    // begin wxGlade: ResolutionDialog::ResolutionDialog
    labelWidth = new wxStaticText(this, wxID_ANY, wxTRANS("Width :"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    textWidth = new wxTextCtrl(this, ID_TEXT_WIDTH, wxT(""));
    labelHeight = new wxStaticText(this, wxID_ANY, wxTRANS("Height :"));
    textHeight = new wxTextCtrl(this, ID_TEXT_HEIGHT, wxT(""));
    static_line_2 = new wxStaticLine(this, wxID_ANY);
    btnReset = new wxButton(this, ID_RESET, wxTRANS("Reset"));
    btnOK = new wxButton(this, wxID_OK, wxEmptyString);
    button_2 = new wxButton(this, wxID_CANCEL, wxEmptyString);

    textWidth->Bind(wxEVT_MOUSEWHEEL, &ResolutionDialog::OnMouseWheelWidth, this);
    textHeight->Bind(wxEVT_MOUSEWHEEL, &ResolutionDialog::OnMouseWheelHeight, this);

    set_properties();
    do_layout();
    // end wxGlade

    programmaticEvent=0;

    updateImage();

    btnOK->SetFocus();
}


BEGIN_EVENT_TABLE(ResolutionDialog, wxDialog)
    // begin wxGlade: ResolutionDialog::event_table
    EVT_TEXT(ID_TEXT_WIDTH, ResolutionDialog::OnTextWidth)
    EVT_TEXT(ID_TEXT_HEIGHT, ResolutionDialog::OnTextHeight)
    EVT_BUTTON(ID_RESET, ResolutionDialog::OnBtnReset)
    EVT_BUTTON(wxID_OK, ResolutionDialog::OnBtnOK)
    EVT_BUTTON(wxID_CANCEL, ResolutionDialog::OnBtnCancel)
    EVT_KEY_DOWN(ResolutionDialog::OnKeypress)
    // end wxGlade
END_EVENT_TABLE();

void ResolutionDialog::updateImage()
{
	wxPaintEvent paintEvt;
	wxPostEvent(this,paintEvt);
}

void ResolutionDialog::setRes(unsigned int w, unsigned int h, bool asReset)
{
	//Increment programmatic lock counter
	programmaticEvent++;

	std::string s;
	stream_cast(s,w);
	textWidth->SetValue(wxStr(s));

	stream_cast(s,h);
	textHeight->SetValue(wxStr(s));

	resWidth=w;
	resHeight=h;

	//do we want to use this as the reset value?
	if(asReset)
	{
		resOrigWidth=w;
		resOrigHeight=h;

		ASSERT(resOrigWidth);
		aspect=(float)resOrigHeight/(float)resOrigWidth;
	}

	programmaticEvent--;
}

void ResolutionDialog::OnTextWidth(wxCommandEvent &event)
{

	if(programmaticEvent)
		return;

	programmaticEvent++;	

	//Validate that string is numerical
	//---
	wxString s =event.GetString();
	std::string textStr;
	textStr=stlStr(s);
	if(textStr.find_first_not_of("0123456789")!=std::string::npos )
	{
		stream_cast(textStr,resWidth);
		textWidth->SetValue(wxStr(textStr));
		programmaticEvent--;
		return;
	}
	//---
	

	int width;
	textStr = stlStr(textWidth->GetValue());

	if(stream_cast(width,textStr))
	{
		programmaticEvent--;
		return;
	}
	resWidth=width;

	//if we are locking the aspect ratio, set the other text box to have the same ratio
	ASSERT(aspect > std::numeric_limits<float>::epsilon());

	resHeight=(unsigned int)(width*aspect);
	stream_cast(textStr,resHeight);
	textHeight->SetValue(wxStr(textStr));


	updateImage();

	programmaticEvent--;
}

void ResolutionDialog::OnTextHeight(wxCommandEvent &event)
{

	if(programmaticEvent)
		return;

	programmaticEvent++;	
	

	//Validate that string is numerical
	//---
	wxString s =event.GetString();
	std::string textStr;
	textStr=stlStr(s);
	if(textStr.find_first_not_of("0123456789")!=std::string::npos )
	{
		stream_cast(textStr,resHeight);
		textHeight->SetValue(wxStr(textStr));
		programmaticEvent--;
		return;
	}
	//---
	
	
	int height;
	textStr = stlStr(textHeight->GetValue());

	if(stream_cast(height,textStr))
	{
		programmaticEvent--;
		return;
	}
	
	resHeight=height;
	
	//if we are locking the aspect ratio, set the other text box to preserve the same ratio
	ASSERT(aspect > std::numeric_limits<float>::epsilon());

	resWidth=(unsigned int)(height/aspect);
	stream_cast(textStr,resWidth);
	textWidth->SetValue(wxStr(textStr));

	updateImage();

	programmaticEvent--;
}



void ResolutionDialog::OnBtnReset(wxCommandEvent &event)
{
	setRes(resOrigWidth,resOrigHeight);
}


void ResolutionDialog::OnBtnOK(wxCommandEvent &event)
{
	finishDialog();
}


void ResolutionDialog::OnBtnCancel(wxCommandEvent &event)
{
	EndModal(wxID_CANCEL);
}

void ResolutionDialog::OnMouseWheelWidth(wxMouseEvent &event)
{
	
	bool haveCtrl,haveShift;
	haveShift=event.ShiftDown();
	haveCtrl=event.CmdDown();

	//normal move rate
	float moveRate=(float)event.GetWheelRotation()/(float)event.GetWheelDelta()*MOUSEWHEEL_RATE_MULTIPLIER;

	//scroll rate multiplier for this scroll event
	{
	float multiplier;
	if(haveShift)
		multiplier=5.0f;
	else if(haveCtrl)
		multiplier=10.0f;
	else
		multiplier=1.0f;

	moveRate*=multiplier;
	}


	if(resWidth+moveRate <= 0)
		return;

	programmaticEvent++;
	setRes((unsigned int)(resWidth+moveRate),resHeight);

	//if we are locking the aspect ratio, set the other text box to preserve the same ratio
	ASSERT(aspect > std::numeric_limits<float>::epsilon());
	
	std::string textStr;
	resHeight=(unsigned int)(resWidth*aspect);
	stream_cast(textStr,resHeight);
	textHeight->SetValue(wxStr(textStr));
	
	updateImage();
	programmaticEvent--;
}

void ResolutionDialog::OnMouseWheelHeight(wxMouseEvent &event)
{


	bool haveCtrl,haveShift;
	haveShift=event.ShiftDown();
	haveCtrl=event.CmdDown();

	//normal move rate
	float moveRate=(float)event.GetWheelRotation()/(float)event.GetWheelDelta()*MOUSEWHEEL_RATE_MULTIPLIER;

	//scroll rate multiplier for this scroll event
	{
	float multiplier;
	if(haveShift)
		multiplier=5.0f;
	else if(haveCtrl)
		multiplier=10.0f;
	else
		multiplier=1.0f;

	moveRate*=multiplier;
	}



	if(resHeight+moveRate <= 0)
		return;

	programmaticEvent++;

	setRes(resWidth,(unsigned int)(resHeight+moveRate));

	//if we are locking the aspect ratio, set the other text box to preserve the same ratio
	ASSERT(aspect > std::numeric_limits<float>::epsilon());

	std::string textStr;
	resWidth=(unsigned int)(resHeight/aspect);
	stream_cast(textStr,resWidth);
	textWidth->SetValue(wxStr(textStr));

	updateImage();
	programmaticEvent--;
}


void ResolutionDialog::OnKeypress(wxKeyEvent &evt)
{
	if( evt.GetKeyCode() == WXK_RETURN)
		finishDialog();
	evt.Skip();
}

// wxGlade: add ResolutionDialog event handlers

void ResolutionDialog::finishDialog()
{
	//programmatic event counter should be decremented to zero
	ASSERT(!programmaticEvent);

	textWidth->Unbind(wxEVT_MOUSEWHEEL, &ResolutionDialog::OnMouseWheelWidth, this);
	textHeight->Unbind(wxEVT_MOUSEWHEEL, &ResolutionDialog::OnMouseWheelHeight, this);
	EndModal(wxID_OK);
}

void ResolutionDialog::set_properties()
{
    // begin wxGlade: ResolutionDialog::set_properties
    SetTitle(wxTRANS("Resolution Selection"));
    // end wxGlade
}


void ResolutionDialog::do_layout()
{
    // begin wxGlade: ResolutionDialog::do_layout
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* upperSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* leftSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* heightTextSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* widthTextSizer = new wxBoxSizer(wxHORIZONTAL);
    upperSizer->Add(10, 20, 0, 0, 0);
    leftSizer->Add(20, 20, 2, 0, 0);
    widthTextSizer->Add(labelWidth, 0, wxALIGN_CENTER_VERTICAL, 0);
    widthTextSizer->Add(textWidth, 0, wxALL|wxALIGN_CENTER_VERTICAL, 8);
    leftSizer->Add(widthTextSizer, 1, wxEXPAND, 0);
    heightTextSizer->Add(labelHeight, 0, wxALIGN_CENTER_VERTICAL, 0);
    heightTextSizer->Add(textHeight, 0, wxALL|wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 5);
    leftSizer->Add(heightTextSizer, 1, wxEXPAND, 0);
    leftSizer->Add(20, 20, 2, 0, 0);
    upperSizer->Add(leftSizer, 0, wxLEFT|wxEXPAND, 8);
    upperSizer->Add(10, 10, 0, 0, 0);
    mainSizer->Add(upperSizer, 1, wxEXPAND, 0);
    mainSizer->Add(static_line_2, 0, wxEXPAND, 0);
    buttonSizer->Add(btnReset, 0, wxALL, 5);
    buttonSizer->Add(20, 20, 1, 0, 0);
    buttonSizer->Add(btnOK, 0, wxALL, 5);
    buttonSizer->Add(button_2, 0, wxALL, 5);
    mainSizer->Add(buttonSizer, 0, wxTOP|wxBOTTOM|wxEXPAND, 5);
    SetSizer(mainSizer);
    mainSizer->Fit(this);
    Layout();
    // end wxGlade
}





