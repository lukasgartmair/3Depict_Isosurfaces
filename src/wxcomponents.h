/*
 * 	wxcomponents.h - custom wxwidgets components
 *	Copyright (C) 2011, D. Haley

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


#ifndef WXCOMPONENTS_H
#define WXCOMPONENTS_H

#include <wx/dc.h>
#include <wx/settings.h>
#include <wx/grid.h>
#include <wx/treectrl.h>
#include <wx/laywin.h>
#include <wx/filedlg.h>

#include <vector>
#include <string>

//Shut wxwidgets assertion errors up by defining a "safe" cb_sort wrapper macro
#if defined(__WXMAC__) || defined(__WXGTK20__)
	       
    #include <wx/version.h>
    #if wxCHECK_VERSION(2,9,0)
    //Sorted combos not supported under gtk in 2.8 series 
    // http://trac.wxwidgets.org/ticket/4398
    //and not supported in mac.
    // http://trac.wxwidgets.org/ticket/12419
        #define SAFE_CB_SORT 0 
    #else
        #define SAFE_CB_SORT wxCB_SORT
	#endif
#else
	#define SAFE_CB_SORT wxCB_SORT
#endif

#include "assertion.h"
#include "commonConstants.h"

//!3D combo grid renderer, from
//http://nomadsync.cvs.sourceforge.net/nomadsync/nomadsync/src/EzGrid.cpp?view=markup (GPL)
class wxGridCellChoiceRenderer : public wxGridCellStringRenderer
{
public:
	wxGridCellChoiceRenderer(wxLayoutAlignment border = wxLAYOUT_NONE) :
			m_border(border) {}
	virtual void Draw(wxGrid& grid,
	                  wxGridCellAttr& attr,
	                  wxDC& dc,
	                  const wxRect& rect,
	                  int row, int col,
	                  bool isSelected);
	virtual wxGridCellRenderer *Clone() const
	{
		return new wxGridCellChoiceRenderer;
	}
private:
	wxLayoutAlignment m_border;

};

class wxFastComboEditor : public wxGridCellChoiceEditor
{
public:
	wxFastComboEditor(const wxArrayString choices,	bool allowOthers = FALSE) : 
		wxGridCellChoiceEditor(choices, allowOthers), 
			m_pointActivate(-1,-1)
		{
			SetClientData((void*)&m_pointActivate);
		}
	virtual void BeginEdit(int row, int col, wxGrid* grid);
protected:
	wxPoint m_pointActivate;
};


//!Data container for tree object data
class wxTreeUint : public wxTreeItemData
{
	public:
		wxTreeUint(unsigned int v) : value(v) {};
		unsigned int value;
};

//!Data container for wxWidgets list object data
class wxListUint : public wxClientData
{
	public:
		wxListUint(unsigned int v) { value=v;};
		unsigned int value;
};

struct GRID_PROPERTY
{
	unsigned int key;
	unsigned int type;
	int renderPosition;
	std::string name;
	std::string data; //String version of data stored
	std::string helpText; //Hover tool-tip help text
};

class wxPropertyGrid;

//!WxGrid derived class to hold and display property data
//So it turns out that someone has MADE a property grid.
//http://wxpropgrid.sourceforge.ne
//This code is not from there. Maybe I should 
//use their code instead, its bound to be better. 
//Also, this appears up in the wxwidgets SVN, 
//so maybe there will be a name clash in future
class wxPropertyGrid : public wxGrid
{
	protected:
		//First element is key number. Second element is key type
		std::vector<std::vector<GRID_PROPERTY> > propertyKeys;
		//Names of each of the grouped keys in propertyKeys
		std::vector<std::string> sectionNames;

		//Fit the columns to the specified size
		void fitCols(wxSize &size);

		//Rows used for separators
		std::vector<int> sepRows;
		//The last coordinates of mouse hovering 
		size_t lastGridHoverCol,lastGridHoverRow;

		//Is the nth row a separator row?
		bool isSeparatorRow(int row) const;
	public:
		wxPropertyGrid(wxWindow* parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, 
					const wxSize& size = wxDefaultSize, long style = wxWANTS_CHARS);
		~wxPropertyGrid();
		
		void OnSize(wxSizeEvent &size);
		void OnLabelDClick(wxGridEvent &size);
		void OnMouseMove(wxMouseEvent &mouse);
		void clearKeys();

		//!Set the number of separator groups to use 
		void setNumGroups(unsigned int newGroupCount){propertyKeys.resize(newGroupCount); sectionNames.resize(newGroupCount);};

		//Set the names for each group. This will appear as a small text in the blank rows
		void setGroupName(unsigned int set, const std::string &name) {ASSERT(set < sectionNames.size()); sectionNames[set]=name;};

		//!This adds the item to the property key vector of a specified group 
		//!	key must be unique 
		void addKey(const std::string &name, unsigned int group,
				unsigned int newKey, unsigned int type, 
				const std::string &data,const std::string &helpText);
		
		//!Click event
		virtual void OnCellLeftClick(wxGridEvent &e);

		//!Cause grid to layout its elements
		void propertyLayout();

		//!Get the key from the row
		unsigned int getKeyFromRow(int row) const;

		//!Get the type from the row
		unsigned int getTypeFromRow(int row) const;

		//!Get the property from key and set 
		const GRID_PROPERTY* getProperty(unsigned int key) const;

		//!Clear the grid
		void clear();

		//!Are we editing a combo box? 
		bool isComboEditing() ;

		DECLARE_EVENT_TABLE()
};

//A wx Grid with copy & paste support
class CopyGrid : public wxGrid
{

public:
	CopyGrid(wxWindow* parent, wxWindowID id, 
		const wxPoint& pos = wxDefaultPosition, 
		const wxSize& size = wxDefaultSize, 
		long style = wxWANTS_CHARS, const wxString& name = wxPanelNameStr);

	void currentCell();
	void selectData();
	//!Copy data to the clipboard (TODO: not working under GTK???)
	void copyData();
	//!Prompts user to save data to file, and then saves it. pops up error 
	// dialog box if there is a problem. Data is tab deliminated
	void saveData();

	virtual void OnKey(wxKeyEvent &evt);

	virtual ~CopyGrid(){};

};


//Type IDs for TTFFinder::suggestFontName
enum
{
	TTFFINDER_FONT_SANS,
	TTFFINDER_FONT_SERIF,
	TTFFINDER_FONT_MONO
};

//A class to determine ttf file locations, in a best effort fashion
class TTFFinder 
{
	private:
		//*n?x (FHS compliant) searching
		static std::string nxFindFont(const char *fontFile);
		//MS win. searching
		static std::string winFindFont(const char *fontFile);
		//Mac OS X searching
		static std::string macFindFont(const char *fontFile);
	public:
		//Given a ttf file name, search for it in several common paths
		static std::string findFont(const char *fontFile);

		//!Given an font type (Sans, serif etc) suggest a font name. 
		//As long as function does not return empty std::string,, then index+1 is a valid
		//query (which may return empty std::string). Font names returned are a suggestion
		//only. Pass to findFont to confirm that a font file exists.
		static std::string suggestFontName(unsigned int fontType, unsigned int index) ;

		//!Returns a valid file that points to an installed ttf file, or an empty string
		//NOTE: TTF file is not checked for content validity!
		std::string getBestFontFile(unsigned int type) const ;
};

#endif

