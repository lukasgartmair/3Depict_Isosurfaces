/*
 * 	resdialog.h - Resolution selector dialog
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

#include <wx/wx.h>
#include <wx/image.h>

#include "assertion.h"
// begin wxGlade: ::dependencies
// end wxGlade


#ifndef RESDIALOG_H
#define RESDIALOG_H


// begin wxGlade: ::extracode
// end wxGlade


class ResDialog: public wxDialog {
public:
    // begin wxGlade: ResDialog::ids
    // end wxGlade

    ResDialog(wxWindow* parent, int id, const wxString& title, const wxPoint& pos=wxDefaultPosition, const wxSize& size=wxDefaultSize, long style=wxDEFAULT_DIALOG_STYLE);

    	//!Get the width as entered in the dialog box
	unsigned int getWidth() const {return resWidth;};
    	//!Get the height as entered in the dialog box
	unsigned int getHeight() const {return resHeight;};
	//!Set the resolution and update text boxes
	void setRes(unsigned int w, unsigned int h);
	//!Finish up the dialog
	void finishDialog();

private:
    // begin wxGlade: ResDialog::methods
    void set_properties();
    void do_layout();
    // end wxGlade
	unsigned int resWidth,resHeight;

protected:
    // begin wxGlade: ResDialog::attributes
    wxStaticText* label_1;
    wxTextCtrl* textWidth;
    wxStaticText* label_2;
    wxTextCtrl* textHeight;
    wxButton* btnOK;
    wxButton* btnCancel;
    // end wxGlade
    virtual void OnBtnOK(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnKeypress(wxKeyEvent &event); // wxGlade: <event_handler>
    virtual void OnBtnCancel(wxCommandEvent &event); // wxGlade: <event_handler>

protected:
    DECLARE_EVENT_TABLE()
}; // wxGlade: end class


#endif // RESDIALOG_H
