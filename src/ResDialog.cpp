/*
 *	ResDialog.cpp - Resolution dialog implementation
 *	Copyright (C) 2010, D Haley 

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

#include "ResDialog.h"


// begin wxGlade: ::extracode
// end wxGlade


BEGIN_EVENT_TABLE(ResDialog, wxDialog)
    EVT_BUTTON(wxID_OK,ResDialog::OnBtnOK)
    EVT_BUTTON(wxID_CANCEL,ResDialog::OnBtnCancel)
    EVT_KEY_DOWN(ResDialog::OnKeypress)
END_EVENT_TABLE();
ResDialog::ResDialog(wxWindow* parent, int id, const wxString& title, const wxPoint& pos, const wxSize& size, long style):
    wxDialog(parent, id, title, pos, size, wxDEFAULT_DIALOG_STYLE)
{
    // begin wxGlade: ResDialog::ResDialog
    label_1 = new wxStaticText(this, wxID_ANY, wxT("Width"));
    textWidth = new wxTextCtrl(this, wxID_ANY, _("1024"), wxDefaultPosition,wxDefaultSize,0,wxTextValidator(wxFILTER_NUMERIC));
    label_2 = new wxStaticText(this, wxID_ANY, wxT("Height"));
    textHeight = new wxTextCtrl(this, wxID_ANY,_("768"), wxDefaultPosition,wxDefaultSize,0,wxTextValidator(wxFILTER_NUMERIC));
    btnOK = new wxButton(this, wxID_OK, wxEmptyString);
    btnCancel = new wxButton(this, wxID_CANCEL, wxEmptyString);

    btnOK->SetFocus();

    resWidth=0;
    resHeight=0;
    set_properties();
    do_layout();
    // end wxGlade
    
}

void ResDialog::setRes(unsigned int w, unsigned int h)
{
	std::string s;
	stream_cast(s,w);
	textWidth->SetValue(wxStr(s));
	stream_cast(s,h);
	textHeight->SetValue(wxStr(s));

	resWidth=w;
	resHeight=h;
}


void ResDialog::OnBtnOK(wxCommandEvent &evt)
{
	finishDialog();
}

void ResDialog::finishDialog()
{
	unsigned int tmpW,tmpH;
	std::string s;

	s=stlStr(textWidth->GetValue());
	if(stream_cast(tmpW,s))
		return;
	if(!tmpW)
		return;

	s=stlStr(textHeight->GetValue());
	if(stream_cast(tmpH,s))
		return;
	if(!tmpH)
		return;

	resWidth=tmpW;
	resHeight=tmpH;
	EndModal(wxID_OK);
}


void ResDialog::OnBtnCancel(wxCommandEvent &evt)
{
	EndModal(wxID_CANCEL);
}


void ResDialog::OnKeypress(wxKeyEvent &evt)
{
	if( evt.GetKeyCode() == WXK_RETURN)
		finishDialog();
	evt.Skip();
}


void ResDialog::set_properties()
{
    // begin wxGlade: ResDialog::set_properties
    SetTitle(wxT("Enter Resolution"));
    // end wxGlade
}


void ResDialog::do_layout()
{
    // begin wxGlade: ResDialog::do_layout
    wxBoxSizer* sizer_5 = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizer_6 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizer_7 = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizer_8 = new wxBoxSizer(wxHORIZONTAL);
    sizer_7->Add(20, 20, 0, 0, 0);
    sizer_8->Add(label_1, 0, wxLEFT|wxRIGHT, 5);
    sizer_8->Add(textWidth, 1, 0, 0);
    sizer_8->Add(20, 20, 0, 0, 0);
    sizer_8->Add(label_2, 0, wxRIGHT, 0);
    sizer_8->Add(textHeight, 1, 0, 0);
    sizer_8->Add(20, 20, 0, 0, 0);
    sizer_7->Add(sizer_8, 0, wxEXPAND, 0);
    sizer_7->Add(20, 20, 0, 0, 0);
    sizer_5->Add(sizer_7, 1, wxEXPAND, 0);
    sizer_6->Add(20, 20, 1, 0, 0);
    sizer_6->Add(btnOK, 0, wxRIGHT|wxBOTTOM, 5);
    sizer_6->Add(btnCancel, 0, wxLEFT|wxRIGHT|wxBOTTOM, 6);
    sizer_5->Add(sizer_6, 0, wxEXPAND, 0);
    SetSizer(sizer_5);
    sizer_5->Fit(this);
    Layout();
    // end wxGlade
}



