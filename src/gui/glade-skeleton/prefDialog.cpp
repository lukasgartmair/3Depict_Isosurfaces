// -*- C++ -*-
//
// generated by wxGlade 0.6.8 on Sun Nov 10 18:05:15 2013
//
// Example for compiling a single file project under Linux using g++:
//  g++ MyApp.cpp $(wx-config --libs) $(wx-config --cxxflags) -o MyApp
//
// Example for compiling a multi file project under Linux using g++:
//  g++ main.cpp $(wx-config --libs) $(wx-config --cxxflags) -o MyApp Dialog1.cpp Frame1.cpp
//

#include "prefDialog.h"

// begin wxGlade: ::extracode
// end wxGlade



PrefDialog::PrefDialog(wxWindow* parent, int id, const wxString& title, const wxPoint& pos, const wxSize& size, long style):
    wxDialog(parent, id, title, pos, size, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER|wxTHICK_FRAME)
{
    // begin wxGlade: PrefDialog::PrefDialog
    notePrefPanels = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0);
    notePrefPanels_pane_3 = new wxPanel(notePrefPanels, wxID_ANY);
    panelStartup = new wxPanel(notePrefPanels, wxID_ANY);
    panelFilters = new wxPanel(notePrefPanels, wxID_ANY);
    sizer_2_staticbox = new wxStaticBox(panelStartup, wxID_ANY, _("Panel Display"));
    updateSizer_staticbox = new wxStaticBox(panelStartup, wxID_ANY, _("Online Updates"));
    sizer_7_staticbox = new wxStaticBox(notePrefPanels_pane_3, wxID_ANY, _("Startup"));
    sizerCamSpeed_staticbox = new wxStaticBox(notePrefPanels_pane_3, wxID_ANY, _("Speed"));
    filterPropSizer_staticbox = new wxStaticBox(panelFilters, wxID_ANY, _("Filter Defaults"));
    lblFilters = new wxStaticText(panelFilters, wxID_ANY, _("Available Filters"));
    const wxString *listFilters_choices = NULL;
    listFilters = new wxListBox(panelFilters, ID_LIST_FILTERS, wxDefaultPosition, wxDefaultSize, 0, listFilters_choices, wxLB_SINGLE|wxLB_SORT);
    filterGridProperties = new wxGrid(panelFilters, ID_GRID_PROPERTIES);
    filterBtnResetAllFilters = new wxButton(panelFilters, wxID_ANY, _("Reset All"));
    filterResetDefaultFilter = new wxButton(panelFilters, wxID_ANY, _("Reset"));
    const wxString comboPanelStartMode_choices[] = {
        _("Always show"),
        _("Remember"),
        _("Specify")
    };
    comboPanelStartMode = new wxComboBox(panelStartup, ID_START_COMBO_PANEL, wxT(""), wxDefaultPosition, wxDefaultSize, 3, comboPanelStartMode_choices, wxCB_DROPDOWN|wxCB_SIMPLE|wxCB_DROPDOWN|wxCB_READONLY);
    chkControl = new wxCheckBox(panelStartup, ID_START_CHECK_CONTROL, _("Control Pane"));
    chkRawData = new wxCheckBox(panelStartup, ID_START_CHECK_RAWDATA, _("Raw Data Panel"));
    chkPlotlist = new wxCheckBox(panelStartup, ID_START_CHECK_PLOTLIST, _("Plot List"));
    checkAllowOnlineUpdate = new wxCheckBox(panelStartup, wxID_ANY, _("Notify periodically about available updates"));
    chkPreferOrtho = new wxCheckBox(notePrefPanels_pane_3, wxID_ANY, _("Prefer orthographic at startup"));
    lblMoveSpeed = new wxStaticText(notePrefPanels_pane_3, wxID_ANY, _("Move Rate"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
    labelSlowCamMoveRate = new wxStaticText(notePrefPanels_pane_3, wxID_ANY, _("(slow)"));
    sliderCamMoveRate = new wxSlider(notePrefPanels_pane_3, ID_MOUSE_MOVE_SLIDER, 1, 1, 100);
    labelFastCamMoveRate = new wxStaticText(notePrefPanels_pane_3, wxID_ANY, _("(fast)"));
    lblZoomSpeed = new wxStaticText(notePrefPanels_pane_3, wxID_ANY, _("Zoom Rate"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
    labelSlowCamZoomRate = new wxStaticText(notePrefPanels_pane_3, wxID_ANY, _("(slow)"));
    sliderCamZoomRate = new wxSlider(notePrefPanels_pane_3, ID_MOUSE_ZOOM_SLIDER, 1, 1, 100);
    labelSlowFastZoomRate = new wxStaticText(notePrefPanels_pane_3, ID_MOUSE_ZOOM_SLIDER, _("(fast)"));
    btnOK = new wxButton(this, wxID_OK, wxEmptyString);
    btnCancel = new wxButton(this, wxID_CANCEL, wxEmptyString);

    set_properties();
    do_layout();
    // end wxGlade
}


void PrefDialog::set_properties()
{
    // begin wxGlade: PrefDialog::set_properties
    SetTitle(_("Preferences"));
    SetSize(wxSize(640, 487));
    filterBtnResetAllFilters->SetToolTip(_("Reset all filter initial values back to program defaults"));
    filterResetDefaultFilter->SetToolTip(_("Reset the filter initial values back to program defaults"));
    comboPanelStartMode->SetToolTip(_("Set the method of panel layout when starting the program"));
    comboPanelStartMode->SetSelection(0);
    checkAllowOnlineUpdate->SetToolTip(_("Lets the program check the internet to see if updates to the program version are available, then notifies you about updates now and again."));
    chkPreferOrtho->SetToolTip(_("By default, use an orthographic camera at startup. State files will override this preference."));
    sliderCamMoveRate->SetToolTip(_("Camera translation, orbit and swivel rates. "));
    sliderCamZoomRate->SetToolTip(_("Camera zooming rate."));
    // end wxGlade
}


void PrefDialog::do_layout()
{
    // begin wxGlade: PrefDialog::do_layout
    wxBoxSizer* panelSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* exitButtonSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizer_5 = new wxBoxSizer(wxVERTICAL);
    sizerCamSpeed_staticbox->Lower();
    wxStaticBoxSizer* sizerCamSpeed = new wxStaticBoxSizer(sizerCamSpeed_staticbox, wxVERTICAL);
    wxBoxSizer* sizer_6_copy = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizer_6 = new wxBoxSizer(wxHORIZONTAL);
    sizer_7_staticbox->Lower();
    wxStaticBoxSizer* sizer_7 = new wxStaticBoxSizer(sizer_7_staticbox, wxHORIZONTAL);
    wxBoxSizer* sizer_1 = new wxBoxSizer(wxVERTICAL);
    updateSizer_staticbox->Lower();
    wxStaticBoxSizer* updateSizer = new wxStaticBoxSizer(updateSizer_staticbox, wxVERTICAL);
    sizer_2_staticbox->Lower();
    wxStaticBoxSizer* sizer_2 = new wxStaticBoxSizer(sizer_2_staticbox, wxVERTICAL);
    wxBoxSizer* sizer_3 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizer_4 = new wxBoxSizer(wxVERTICAL);
    filterPropSizer_staticbox->Lower();
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
    updateSizer->Add(checkAllowOnlineUpdate, 0, 0, 0);
    sizer_1->Add(updateSizer, 0, wxALL|wxEXPAND, 5);
    panelStartup->SetSizer(sizer_1);
    sizer_7->Add(chkPreferOrtho, 0, wxALL, 5);
    sizer_5->Add(sizer_7, 0, wxEXPAND, 0);
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
    sizer_5->Add(sizerCamSpeed, 1, wxEXPAND, 0);
    notePrefPanels_pane_3->SetSizer(sizer_5);
    notePrefPanels->AddPage(panelFilters, _("Pref"));
    notePrefPanels->AddPage(panelStartup, _("Startup"));
    notePrefPanels->AddPage(notePrefPanels_pane_3, _("Camera"));
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


BEGIN_EVENT_TABLE(PrefDialog, wxDialog)
    // begin wxGlade: PrefDialog::event_table
    EVT_LISTBOX(ID_LIST_FILTERS, PrefDialog::OnListClick)
    EVT_GRID_CMD_CELL_CHANGE(ID_GRID_PROPERTIES, PrefDialog::OnFilterCellChange)
    EVT_GRID_CMD_CELL_LEFT_CLICK(ID_GRID_PROPERTIES, PrefDialog::OnFilterCellClick)
    EVT_COMBOBOX(ID_START_COMBO_PANEL, PrefDialog::OnStartupPanelCombo)
    EVT_CHECKBOX(ID_START_CHECK_CONTROL, PrefDialog::OnStartupCheckControl)
    EVT_CHECKBOX(ID_START_CHECK_RAWDATA, PrefDialog::OnStartupCheckRawData)
    EVT_CHECKBOX(ID_START_CHECK_PLOTLIST, PrefDialog::OnStartupCheckPlotList)
    EVT_CHECKBOX(wxID_ANY, PrefDialog::OnCheckPreferOrtho)
    EVT_COMMAND_SCROLL(ID_MOUSE_MOVE_SLIDER, PrefDialog::OnMouseMoveSlider)
    EVT_COMMAND_SCROLL(ID_MOUSE_ZOOM_SLIDER, PrefDialog::OnMouseZoomSlider)
    // end wxGlade
END_EVENT_TABLE();


void PrefDialog::OnListClick(wxCommandEvent &event)
{
    event.Skip();
    // notify the user that he hasn't implemented the event handler yet
    wxLogDebug(wxT("Event handler (PrefDialog::OnListClick) not implemented yet"));
}

void PrefDialog::OnFilterCellChange(wxGridEvent &event)
{
    event.Skip();
    // notify the user that he hasn't implemented the event handler yet
    wxLogDebug(wxT("Event handler (PrefDialog::OnFilterCellChange) not implemented yet"));
}

void PrefDialog::OnFilterCellClick(wxGridEvent &event)
{
    event.Skip();
    // notify the user that he hasn't implemented the event handler yet
    wxLogDebug(wxT("Event handler (PrefDialog::OnFilterCellClick) not implemented yet"));
}

void PrefDialog::OnStartupPanelCombo(wxCommandEvent &event)
{
    event.Skip();
    // notify the user that he hasn't implemented the event handler yet
    wxLogDebug(wxT("Event handler (PrefDialog::OnStartupPanelCombo) not implemented yet"));
}

void PrefDialog::OnStartupCheckControl(wxCommandEvent &event)
{
    event.Skip();
    // notify the user that he hasn't implemented the event handler yet
    wxLogDebug(wxT("Event handler (PrefDialog::OnStartupCheckControl) not implemented yet"));
}

void PrefDialog::OnStartupCheckRawData(wxCommandEvent &event)
{
    event.Skip();
    // notify the user that he hasn't implemented the event handler yet
    wxLogDebug(wxT("Event handler (PrefDialog::OnStartupCheckRawData) not implemented yet"));
}

void PrefDialog::OnStartupCheckPlotList(wxCommandEvent &event)
{
    event.Skip();
    // notify the user that he hasn't implemented the event handler yet
    wxLogDebug(wxT("Event handler (PrefDialog::OnStartupCheckPlotList) not implemented yet"));
}

void PrefDialog::OnCheckPreferOrtho(wxCommandEvent &event)
{
    event.Skip();
    // notify the user that he hasn't implemented the event handler yet
    wxLogDebug(wxT("Event handler (PrefDialog::OnCheckPreferOrtho) not implemented yet"));
}

void PrefDialog::OnMouseMoveSlider(wxScrollEvent &event)
{
    event.Skip();
    // notify the user that he hasn't implemented the event handler yet
    wxLogDebug(wxT("Event handler (PrefDialog::OnMouseMoveSlider) not implemented yet"));
}

void PrefDialog::OnMouseZoomSlider(wxScrollEvent &event)
{
    event.Skip();
    // notify the user that he hasn't implemented the event handler yet
    wxLogDebug(wxT("Event handler (PrefDialog::OnMouseZoomSlider) not implemented yet"));
}


// wxGlade: add PrefDialog event handlers


class MyApp: public wxApp {
public:
    bool OnInit();
protected:
    wxLocale m_locale;  // locale we'll be using
};

IMPLEMENT_APP(MyApp)

bool MyApp::OnInit()
{
    m_locale.Init();
#ifdef APP_LOCALE_DIR
    m_locale.AddCatalogLookupPathPrefix(wxT(APP_LOCALE_DIR));
#endif
    m_locale.AddCatalog(wxT(APP_CATALOG));

    wxInitAllImageHandlers();
    PrefDialog* dlgPreference = new PrefDialog(NULL, wxID_ANY, wxEmptyString);
    SetTopWindow(dlgPreference);
    dlgPreference->Show();
    return true;
}