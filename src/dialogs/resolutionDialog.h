// -*- C++ -*- generated by wxGlade 0.6.3 on Mon May  7 00:46:06 2012

#include <wx/wx.h>
#include <wx/image.h>
// begin wxGlade: ::dependencies
#include <wx/spinctrl.h>
#include <wx/statline.h>
// end wxGlade


#ifndef RESOLUTIONDIALOG_H
#define RESOLUTIONDIALOG_H

//!resolution chooser dialog
class ResolutionDialog: public wxDialog {
public:
    // begin wxGlade: ResolutionDialog::ids
    // end wxGlade

    ResolutionDialog(wxWindow* parent, int id, const wxString& title, const wxPoint& pos=wxDefaultPosition, const wxSize& size=wxDefaultSize, long style=wxDEFAULT_DIALOG_STYLE);

private:
    //Resolution width and height or the final output
    unsigned int resWidth,resHeight;

    //!Reset value for resolution
    unsigned int resOrigWidth,resOrigHeight;

    //!Programmatic event counter
    // Non-zero if event is being generated programatically
    int programmaticEvent;  

    //!aspect ratio for when locking aspect. Zero if undefined
    float aspect;

    // begin wxGlade: ResolutionDialog::methods
    void set_properties();
    void do_layout();
    // end wxGlade
    //!Finish up the dialog
    void finishDialog();
    //!Update the drawn image representing the output shape
    void updateImage();

    //!Draw the image rectangle
    void drawImageRectangle(wxDC *dc); 
protected:
    // begin wxGlade: ResolutionDialog::attributes
    wxStaticText* labelWidth;
    wxTextCtrl* textWidth;
    wxStaticText* labelHeight;
    wxTextCtrl* textHeight;
    wxCheckBox* checkLockAspect;
    wxStaticLine* static_line_1;
    wxPanel* panelImage;
    wxStaticLine* static_line_2;
    wxButton* btnReset;
    wxButton* btnOK;
    wxButton* button_2;
    // end wxGlade

    DECLARE_EVENT_TABLE();


public:
    virtual void OnTextWidth(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnTextHeight(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnCheckLockAspect(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnBtnReset(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnBtnOK(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnBtnCancel(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnMouseWheelWidth(wxMouseEvent &event);
    virtual void OnMouseWheelHeight(wxMouseEvent &event);
    virtual void OnPaint(wxPaintEvent &event);

    virtual void OnKeypress(wxKeyEvent &evt);
    
    
    //!get the width as entered in the dialog
    unsigned int getWidth() const {return resWidth;};
    //!Get the height as entered in the dialog box
    unsigned int getHeight() const {return resHeight;};
    //!Set the resolution and update text boxes
    void setRes(unsigned int w, unsigned int h, bool asReset=false);
}; // wxGlade: end class


#endif // RES_H
