/*
 *	wxcomponents.h - Custom wxWidgets components header
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

#include "wxcomponents.h"
#include "wxcommon.h"
#include "common/stringFuncs.h"

#include "common/constants.h"
#include "common/translation.h"


#include <wx/clipbrd.h>

using std::ofstream;
using std::vector;

const float FONT_HEADING_SCALEFACTOR=1.25f;

void upWxTreeCtrl(const FilterTree &filterTree, wxTreeCtrl *t,
		std::map<size_t,Filter *> &filterMap,vector<const Filter *> &persistentFilters,
		const Filter *visibleFilt)
{
	//Remove any filters that don't exist any more
	for(unsigned int ui=persistentFilters.size();ui--;)
	{
		if(!filterTree.contains(persistentFilters[ui]))
		{
			std::swap(persistentFilters[ui],persistentFilters.back());
			persistentFilters.pop_back();
		}
	}

	stack<wxTreeItemId> treeIDs;
	t->Freeze();
	//Warning: this generates an event, 
	//most of the time (some windows versions do not according to documentation)
	t->DeleteAllItems();

	//Clear the mapping
	filterMap.clear();
	size_t nextID=0;
	
	size_t lastDepth=0;
	//Add dummy root node. This will be invisible to wxTR_HIDE_ROOT controls
	wxTreeItemId tid;
	tid=t->AddRoot(wxT("TreeBase"));
	t->SetItemData(tid,new wxTreeUint(nextID));

	// Push on stack to prevent underflow, but don't keep a copy, 
	// as we will never insert or delete this from the UI	
	treeIDs.push(tid);

	nextID++;
	std::map<const Filter*,wxTreeItemId> reverseFilterMap;
	//Depth first  add
	for(tree<Filter * >::pre_order_iterator filtIt=filterTree.depthBegin();
					filtIt!=filterTree.depthEnd(); ++filtIt)
	{	
		//Push or pop the stack to make it match the iterator position
		if( lastDepth > filterTree.depth(filtIt))
		{
			while(filterTree.depth(filtIt) +1 < treeIDs.size())
				treeIDs.pop();
		}
		else if( lastDepth < filterTree.depth(filtIt))
		{
			treeIDs.push(tid);
		}
		

		lastDepth=filterTree.depth(filtIt);
	
		//This will use the user label or the type string.	
		tid=t->AppendItem(treeIDs.top(),
			wxStr((*filtIt)->getUserString()));
		t->SetItemData(tid,new wxTreeUint(nextID));
	

		//Record mapping to filter for later reference
		filterMap[nextID]=*filtIt;
		//Remember the reverse mapping for later in
		// this function when we reset visibility
		reverseFilterMap[*filtIt] = tid;

		nextID++;
	}

	//Try to restore the  selection in a user friendly manner
	// - Try restoring all requested filter's visibility
	// - then restore either the first requested filter as the selection
	//   or the specified parameter filter as the selection.
	if(persistentFilters.size())
	{
		for(unsigned int ui=0;ui<persistentFilters.size();ui++)
			t->EnsureVisible(reverseFilterMap[persistentFilters[ui]]);

		if(!visibleFilt)
			t->SelectItem(reverseFilterMap[persistentFilters[0]]);
		else
			t->SelectItem(reverseFilterMap[visibleFilt]);

		persistentFilters.clear();
	}
	else if(visibleFilt)
	{
		ASSERT(reverseFilterMap.find(visibleFilt)!=reverseFilterMap.end())
		t->SelectItem(reverseFilterMap[visibleFilt]);
	}
	t->GetParent()->Layout();

	t->Thaw();
}

//Convert my internal choice string format to wx's
std::string wxChoiceParamString(std::string choiceString)
{
	std::string retStr;

	bool haveColon=false;
	bool haveBar=false;
	for(unsigned int ui=0;ui<choiceString.size();ui++)
	{
		if(haveColon && haveBar )
		{
			if(choiceString[ui] != ',')
				retStr+=choiceString[ui];
			else
			{
				haveBar=false;
				retStr+=",";
			}
		}
		else
		{
			if(choiceString[ui]==':')
				haveColon=true;
			else
			{
				if(choiceString[ui]=='|')
					haveBar=true;
			}
		}
	}

	return retStr;
}

BEGIN_EVENT_TABLE(wxCustomPropGrid, wxGrid)
	EVT_GRID_CELL_LEFT_CLICK(wxCustomPropGrid::OnCellLeftClick )
	EVT_GRID_LABEL_LEFT_DCLICK(wxCustomPropGrid::OnLabelDClick) 
	EVT_MOTION(wxCustomPropGrid::OnMouseMove) 
END_EVENT_TABLE()

wxCustomPropGrid::wxCustomPropGrid(wxWindow* parent, wxWindowID id, 
		const wxPoint& pos , const wxSize& size , long style ) :
					wxGrid(parent, id, pos, size , style)
{
	lastGridHoverCol=lastGridHoverRow=-1;
	//Perform wx event connecting/binding
#if wxCHECK_VERSION(2, 9, 0)
	GetGridWindow()->Bind(wxEVT_MOTION, &wxCustomPropGrid::OnMouseMove, this);
#else
    	GetGridWindow()->Connect(wxID_ANY,
                 wxEVT_MOTION, wxMouseEventHandler(wxCustomPropGrid::OnMouseMove), NULL, this);
#endif
}


void wxCustomPropGrid::setGroupName(unsigned int set, const std::string &name) 
{
	ASSERT(set < sectionNames.size());
	sectionNames[set]=name;
}

void wxCustomPropGrid::OnSize(wxSizeEvent &event)
{

}

void wxCustomPropGrid::OnLabelDClick(wxGridEvent &event)
{
#if wxCHECK_VERSION(2,9,0)
	if(!this->GetNumberCols())
#else
	if(!this->GetCols())
#endif
		return;
	wxSize s;
	s=this->GetSize();
	fitCols(s);
	SelectBlock(-1,-1,-1,-1); //Select empty block
}

void wxCustomPropGrid::fitCols(wxSize &size)
{
	int wc = size.GetWidth()-this->GetScrollThumb(wxVERTICAL)-15;
	this->SetColSize(1, wc-this->GetColSize(0)); 

	Refresh();
}

wxCustomPropGrid::~wxCustomPropGrid()
{
#if wxCHECK_VERSION(2, 9, 0)
   GetGridWindow()->Unbind(wxEVT_MOTION, &wxCustomPropGrid::OnMouseMove, this);
#else
   GetGridWindow()->Disconnect();
#endif
}

//Function adapted from nomadsync.sourceforge.net (GPL)
void wxGridCellChoiceRenderer::Draw(wxGrid& grid, wxGridCellAttr& attr, wxDC& dc,
	const wxRect& rectCell, int row, int col, bool isSelected)
{
    wxGridCellRenderer::Draw(grid, attr, dc, rectCell, row, col, isSelected);
	// first calculate button size
	// don't draw outside the cell
	int nButtonWidth = 17;
	if (rectCell.height < 2) return;	
	wxRect rectButton;
	rectButton.x = rectCell.x + rectCell.width - nButtonWidth;
	rectButton.y = rectCell.y + 1;
	int cell_rows, cell_cols;
	attr.GetSize(&cell_rows, &cell_cols);
	rectButton.width = nButtonWidth;
	if (cell_rows == 1)
		rectButton.height = rectCell.height-2;
	else
		rectButton.height = nButtonWidth;

	SetTextColoursAndFont(grid, attr, dc, isSelected);
	int hAlign, vAlign;
	attr.GetAlignment(&hAlign, &vAlign);
	// leave room for button
	wxRect rect = rectCell;
	rect.SetWidth(rectCell.GetWidth() - rectButton.GetWidth()-2);
	rect.Inflate(-1);
	grid.DrawTextRectangle(dc, grid.GetCellValue(row, col), rect, hAlign, vAlign);

	// don't bother drawing if the cell is too small
	if (rectButton.height < 4 || rectButton.width < 4) return;
	// draw 3-d button
	wxColour colourBackGround = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE);
	dc.SetBrush(wxBrush(colourBackGround, wxSOLID));
	dc.SetPen(wxPen(colourBackGround, 1, wxSOLID));
	dc.DrawRectangle(rectButton);
	dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT), 1, wxSOLID));
	dc.DrawLine(rectButton.GetLeft(), rectButton.GetBottom(), 
		rectButton.GetRight(), rectButton.GetBottom());
	dc.DrawLine(rectButton.GetRight(), rectButton.GetBottom(), 
		rectButton.GetRight(), rectButton.GetTop()-1);
	dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW), 
		1, wxSOLID));
	dc.DrawLine(rectButton.GetLeft()+1, rectButton.GetBottom()-1, 
		rectButton.GetRight()-1, rectButton.GetBottom()-1);
	dc.DrawLine(rectButton.GetRight()-1, rectButton.GetBottom()-1, 
		rectButton.GetRight()-1, rectButton.GetTop());
	dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNHIGHLIGHT), 
		1, wxSOLID));
	dc.DrawLine(rectButton.GetRight()-2, rectButton.GetTop()+1, 
		rectButton.GetLeft()+1, rectButton.GetTop()+1);
	dc.DrawLine(rectButton.GetLeft()+1, rectButton.GetTop()+1, 
		rectButton.GetLeft()+1, rectButton.GetBottom()-1);
	// Draw little triangle
	int nTriWidth = 7;
	int nTriHeight = 4;
	wxPoint point[3];
	point[0] = wxPoint(rectButton.GetLeft() + (rectButton.GetWidth()-nTriWidth)/2, 
		rectButton.GetTop()+(rectButton.GetHeight()-nTriHeight)/2);
	point[1] = wxPoint(point[0].x+nTriWidth-1, point[0].y);
	point[2] = wxPoint(point[0].x+3, point[0].y+nTriHeight-1);
	dc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT), wxSOLID));
	dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT), 1, wxSOLID));
	dc.DrawPolygon(3, point);
	if (m_border == wxLAYOUT_TOP)
	{
		dc.SetPen(wxPen(*wxBLACK, 1, wxDOT));
		dc.DrawLine(rectCell.GetRight(), rectCell.GetTop(), 
			rectCell.GetLeft(), rectCell.GetTop());
	}
}

bool wxCustomPropGrid::isSeparatorRow(int row) const
{
	//TODO: Consider moving to map, this does not scale well.
	// for now we have usually five or six items tho
	return std::find(sepRows.begin(),sepRows.end(),row)!=sepRows.end();
}

void wxCustomPropGrid::OnMouseMove(wxMouseEvent &event)
{

	int xPos,yPos;
	int xMouse,yMouse;
	event.GetPosition(&xMouse,&yMouse);

	CalcUnscrolledPosition(xMouse,yMouse,&xPos,&yPos);

	int row,col;
	row=YToRow(yPos);
	col=XToCol(xPos);

	//Check if the column or row was different
	if(col != lastGridHoverCol || row != lastGridHoverRow)
	{
		lastGridHoverRow = row;
		lastGridHoverCol = col;

		//If we are hovering over a "spacer" row,
		//the null row, or not on the prop
		//column, then bail out
		if(isSeparatorRow(row) || row == -1  || col == 1)
		{
			GetGridWindow()->SetToolTip(NULL); //Hide the tooltip
			event.Skip();
			return;
		}

		//Set the help text based upon the group property
		const GRID_PROPERTY *p;
		p=getProperty(getKeyFromRow(row));
	
		GetGridWindow()->SetToolTip(wxStr(p->helpText));
	}

	event.Skip();
}

//Function adapted from nomadsync.sourceforge.net (GPL)
void wxCustomPropGrid::OnCellLeftClick(wxGridEvent &ev)
{
	// This forces the cell to go into edit mode directly
	m_waitForSlowClick = true;
	SetGridCursor(ev.GetRow(), ev.GetCol());

	// hack to prevent selection from being lost when click combobox
	if (ev.GetCol() == 0 && IsInSelection(ev.GetRow(), ev.GetCol()))
	{
		m_selection = NULL;
	}
	ev.Skip();
}

void wxCustomPropGrid::clear()
{
	this->BeginBatch();
	//Empty the grid
	if(this->GetNumberCols())
		this->DeleteCols(0,this->GetNumberCols());
	if(this->GetNumberRows())
		this->DeleteRows(0,this->GetNumberRows());
	
	
	propertyKeys.clear();
	this->EndBatch();

	GetGridWindow()->SetToolTip(wxT(""));
	lastGridHoverRow=lastGridHoverCol=-1;
}

bool wxCustomPropGrid::isComboEditing() 
{
	bool retVal;
	retVal= IsCellEditControlEnabled();
#if wxCHECK_VERSION(2,9,0)
	retVal = retVal &&getTypeFromRow(this->GetGridCursorRow()) == PROPERTY_TYPE_CHOICE;
#else
	retVal = retVal && getTypeFromRow(this->GetCursorRow()) == PROPERTY_TYPE_CHOICE;
#endif
	return retVal;
};

unsigned int wxCustomPropGrid::getTypeFromRow(int row) const
{
	for(unsigned int ui=0;ui<propertyKeys.size();ui++)
	{

		for(unsigned int uj=0;uj<propertyKeys[ui].size();uj++)
		{
			if(propertyKeys[ui][uj].renderPosition == row)
				return propertyKeys[ui][uj].type;
		}
	}

	ASSERT(false);
	return 0; // Shut gcc up
}

void  wxCustomPropGrid::clearKeys()
{
	propertyKeys.clear();
	sectionNames.clear();
}

void wxCustomPropGrid::addKey(const std::string &name, unsigned int group,
		unsigned int newKey, unsigned int type, const std::string &data,
		const std::string &helpText)
{

	GRID_PROPERTY newProp;
	newProp.key=newKey;
	newProp.type=type;
	newProp.name=name;
	newProp.data=data;
	newProp.helpText = helpText;
	propertyKeys[group].push_back(newProp);
}

//Layout the property vector
void wxCustomPropGrid::propertyLayout()
{
	this->BeginBatch();

	//Empty the grid
	if(this->GetNumberCols())
		this->DeleteCols(0,this->GetNumberCols());
	if(this->GetNumberRows())
		this->DeleteRows(0,this->GetNumberRows());

	this->AppendCols(2);
	this->SetColLabelValue(0,wxTRANS("Param"));
	this->SetColLabelValue(1,wxTRANS("Value"));

	sepRows.clear();
	sepRows.resize(propertyKeys.size());
	int rows=0;
	for (unsigned int ui=0; ui<propertyKeys.size(); ui++)
	{
		//One row for each property
		for (unsigned uj=0; uj<propertyKeys[ui].size(); uj++)
			propertyKeys[ui][uj].renderPosition=rows+uj;

		rows+=propertyKeys[ui].size();

		//One row for separator
		sepRows[ui] = rows;
		rows++;
	}

	//Remove last separator row.
	if (rows)
		rows--;
	this->AppendRows(rows);

	for (unsigned int ui=0; ui<propertyKeys.size(); ui++)
	{
		for (unsigned int uj=0; uj<propertyKeys[ui].size(); uj++)
		{
			//set the title
			this->SetCellValue(propertyKeys[ui][uj].renderPosition,
			                   0,wxStr(propertyKeys[ui][uj].name));

			//Set the cell renderer
			wxGridCellAttr *attr = this->GetOrCreateCellAttr(
			                           propertyKeys[ui][uj].renderPosition, 1);

			//Attr will return 0 if the grid is not created.
			//you must create the grid before calling this function 
			//(see wxGrd::CreateGrid(i,j)
			ASSERT(attr);
			//The datatype determines the appearance of the combo box
			switch (propertyKeys[ui][uj].type)
			{
			case PROPERTY_TYPE_BOOL:
				attr->SetRenderer(new wxGridCellBoolRenderer());
				//set the data
				this->SetCellValue(propertyKeys[ui][uj].renderPosition,
				                   1,wxStr(propertyKeys[ui][uj].data));
				break;
			case PROPERTY_TYPE_INTEGER:
				attr->SetRenderer(new wxGridCellNumberRenderer());
				//set the data
				this->SetCellValue(propertyKeys[ui][uj].renderPosition,
				                   1,wxStr(propertyKeys[ui][uj].data));
				break;
			case PROPERTY_TYPE_REAL:
				attr->SetRenderer(new wxGridCellFloatRenderer());
				//set the data
				this->SetCellValue(propertyKeys[ui][uj].renderPosition,
				                   1,wxStr(propertyKeys[ui][uj].data));
				break;
			case PROPERTY_TYPE_COLOUR:
				//OK, this is totally inefficient, and hacky. but set the colour
				//based upon the colour string. Then when user edits, use a colour
				//dialog (handled elsewhere)
				unsigned char r,g,b,a;
				parseColString(propertyKeys[ui][uj].data,r,g,b,a);
				attr->SetBackgroundColour(wxColour(r,g,b,a));

				break;
			case PROPERTY_TYPE_CHOICE:
			{
				//set up the renderer
				std::string s;
				s=getActiveChoice(propertyKeys[ui][uj].data);
				attr->SetRenderer(new wxGridCellChoiceRenderer());
				this->SetCellValue(propertyKeys[ui][uj].renderPosition,
				                   1,wxStr(s));

				//construct a wxStringArray of possible choices.
				s=wxChoiceParamString(propertyKeys[ui][uj].data);
				vector<std::string> splitStrs;
				wxArrayString a;
				splitStrsRef(s.c_str(),',',splitStrs);
				for (unsigned int uk=0; uk<splitStrs.size(); uk++)
					a.Add(wxStr(splitStrs[uk]));
				//Set up the editor
				wxGridCellChoiceEditor *choiceEd=new wxFastComboEditor(a,false);
				this->SetCellEditor(propertyKeys[ui][uj].renderPosition,
				                    1,choiceEd);

				//Required to prevent wx from asserting,
				// "GetEventHandler == this" failed in ~wxWindowBase(), any pushed event handlers must be removed".
				// I posted on the wxwidgets forum and got the reply that I should try this, but they were not sure.
				// seems to work
				attr->DecRef();
				break;
			}
			case PROPERTY_TYPE_STRING:
			case PROPERTY_TYPE_POINT3D:
				//set the data
				this->SetCellValue(propertyKeys[ui][uj].renderPosition,
				                   1,wxStr(propertyKeys[ui][uj].data));
				break;
			}


		}

		//Add a blank, light grey, read only row, but not if it is the last
		//use to denote spacing between sets to user.
		if (ui+1 < propertyKeys.size())
		{
			//Optionally give it a description; if one has been provided
			if(sectionNames[ui+1].size())
			{
				this->SetCellValue(sepRows[ui],0,wxStr(sectionNames[ui+1]));
				wxFont f;
				f=*wxSMALL_FONT;
				f.SetStyle(wxFONTSTYLE_ITALIC);
				f.SetPointSize((int)(f.GetPointSize()*FONT_HEADING_SCALEFACTOR));
				this->SetCellFont(sepRows[ui],0,f);
			}

			wxGridCellAttr *readOnlyRowAttr=new wxGridCellAttr;
			readOnlyRowAttr->SetReadOnly(true);
			readOnlyRowAttr->SetBackgroundColour(wxColour(*wxLIGHT_GREY));
			this->SetRowAttr(sepRows[ui],readOnlyRowAttr);

		}
	}

	//First column is read only
	wxGridCellAttr *readOnlyGridAttr=new wxGridCellAttr;
	readOnlyGridAttr->SetReadOnly(true);
	SetColAttr(0,readOnlyGridAttr);
	//Remove labels for column lead-in
	SetRowLabelSize(0);
	SetMargins(0,0);

	AutoSizeColumn(0,true);
	EndBatch();

	//Expand the column contents to fit current size	
	wxSize s;
	s=GetSize();
	fitCols(s);
}

unsigned int  wxCustomPropGrid::getKeyFromRow(int row) const
{
	for(unsigned int ui=0;ui<propertyKeys.size();ui++)
	{

		for(unsigned int uj=0;uj<propertyKeys[ui].size();uj++)
		{
			if(propertyKeys[ui][uj].renderPosition == row)
				return propertyKeys[ui][uj].key;
		}
	}

	ASSERT(false);
	return 0; // Shut gcc up
}

const GRID_PROPERTY *wxCustomPropGrid::getProperty(unsigned int key) const
{
	for(unsigned int group=0;group<propertyKeys.size();group++)
	{
		for(unsigned int ui=0;ui<propertyKeys[group].size(); ui++)
		{
			if(propertyKeys[group][ui].key == key)
				return &(propertyKeys[group][ui]);
		}
	}
	return 0;
}

void wxFastComboEditor::BeginEdit(int row, int col, wxGrid* grid)
{
	wxGridCellChoiceEditor::BeginEdit(row, col, grid);
	// unfortunately I don't know how to send a left button down message to a 
	// control in GTK, if anyone can give me a hint it would be greatly appreciated :)
#ifdef __WINDOWS__
	DWORD style = ::GetWindowLong((HWND)Combo()->GetHandle(), GWL_STYLE);
	style &= ~WS_TABSTOP;
	::SetWindowLong((HWND)Combo()->GetHandle(), GWL_STYLE, style);
	if (m_pointActivate.x > -1 && m_pointActivate.y > -1)
	{
		m_pointActivate = Combo()->ScreenToClient(m_pointActivate);
		SendMessage((HWND)Combo()->GetHandle(), WM_LBUTTONDOWN, 0,
			MAKELPARAM(m_pointActivate.x, m_pointActivate.y));
	}
#endif
}

BEGIN_EVENT_TABLE(CopyGrid, wxGrid)
	EVT_KEY_DOWN(CopyGrid::OnKey) 
END_EVENT_TABLE()
CopyGrid::CopyGrid(wxWindow* parent, wxWindowID id, 
		const wxPoint& pos , 
		const wxSize& size , 
		long style, const wxString& name ) : wxGrid(parent,id,pos,size,style,name)
{

}


void CopyGrid::selectData()
{
}

void CopyGrid::saveData()
{
	wxFileDialog *wxF = new wxFileDialog(this,wxTRANS("Save Data..."), wxT(""),
		wxT(""),wxTRANS("Text File (*.txt)|*.txt|All Files (*)|*"),wxFD_SAVE);

	if( (wxF->ShowModal() == wxID_CANCEL))
		return;
	

	std::string dataFile = stlStr(wxF->GetPath());
	ofstream f(dataFile.c_str());

	if(!f)
	{
		wxMessageDialog *wxD  =new wxMessageDialog(this,
			wxTRANS("Error saving file. Check output dir is writable."),wxTRANS("Save error"),wxOK|wxICON_ERROR);

		wxD->ShowModal();
		wxD->Destroy();

		return;
	}	 

        // Number of rows and cols
        int rows,cols;
#if wxCHECK_VERSION(2,9,0)
	rows=GetNumberRows();
	cols=GetNumberCols();
#else
	rows=GetRows();
	cols=GetCols();
#endif
        // data variable contain text that must be set in the clipboard
        // For each cell in selected range append the cell value in the data
	//variable
        // Tabs '\\t' for cols and '\\r' for rows
	
	//print headers
	for(int  c=0; c<cols; c++)
	{
		f << stlStr(GetColLabelValue(c)) << "\t";
	}
	f<< std::endl;
	
	
        for(int  r=0; r<rows; r++)
	{
            for(int  c=0; c<cols; c++)
	    {
                f << stlStr(GetCellValue(r,c));
                if(c < cols - 1)
                    f <<  "\t";

	    }
	    f << std::endl; 
	}
	
}

void CopyGrid::OnKey(wxKeyEvent &event)
{



        if (event.CmdDown() && event.GetKeyCode() == 67)
	{
	
            copyData();
	    return;
	}
    event.Skip();
    return;

}



void CopyGrid::copyData() 
{
        // Number of rows and cols
        int rows,cols;
       
	
	//This is an undocumented class AFAIK. :(
	wxGridCellCoordsArray arrayTL(GetSelectionBlockTopLeft());
	wxGridCellCoordsArray arrayBR(GetSelectionBlockBottomRight());
	

	// data variable contain text that must be set in the clipboard
	std::string data;
	std::string endline;
#ifdef __WXMSW__
	endline="\r\n";
#else
	endline="\n";
#endif

	if(arrayTL.Count() && arrayBR.Count())
	{

		wxGridCellCoords coordTL = arrayTL.Item(0);
		wxGridCellCoords coordBR = arrayBR.Item(0);


		rows = coordBR.GetRow() - coordTL.GetRow() +1;
		cols = coordBR.GetCol() - coordTL.GetCol() +1;	

		//copy title from header
		if(cols)
		{
			for(int ui=0;ui<cols;ui++)
			{
				data+=stlStr(GetColLabelValue(ui));

				if(ui<cols-1)
					data+="\t";
			}
			data+=endline;
		}

		// For each cell in selected range append the cell value in the data
		//variable
		// Tabs '\\t' for cols and '\\n' for rows
		for(int  r=0; r<rows; r++)
		{
		    for(int  c=0; c<cols; c++)
		    {
			data+= stlStr(GetCellValue(coordTL.GetRow() + r,
							coordTL.GetCol()+ c));
			if(c < cols - 1)
			    data+= "\t";

		    }
		    data+=endline;
		}
	}
	else
	{
		const wxArrayInt& rows(GetSelectedRows());
		const wxArrayInt& cols(GetSelectedCols());
//		const wxGridCellCoordsArray& cells(GetSelectedCells());

		//Copy title from header
		if(cols.size())
		{
			for(int ui=0;ui<cols.size();ui++)
			{
				data+=stlStr(GetColLabelValue(ui));

				if(ui<cols.size()-1)
					data+="\t";
			}
			data+=endline;

			for(int ui=0;ui<GetNumberRows(); ui++)
			{
			    for(int  c=0; c<cols.size(); c++)
			    {
				data+= stlStr(GetCellValue(ui,cols[c]));
				if(c < cols.size()-1)
				    data+= "\t";

			    }
			    data +=endline;
			}


		}
		else if (rows.size())
		{
			for(int ui=0;ui<GetNumberCols();ui++)
			{
				data+=stlStr(GetColLabelValue(ui));

				if(ui<cols.size()-1)
					data+="\t";
			}
			data+=endline;

			for(int r=0;r<rows.size(); r++)
			{
			    for(int  c=0; c<GetNumberCols(); c++)
			    {
				data+= stlStr(GetCellValue(rows[r],c));
				if(c < GetNumberCols()-1)
				    data+= "\t";

			    }
			    data +=endline;
			}
		}
/*		else if(cells.size())
		{
			//FIXME: Needs more thought than I have time for right now. 
			// the problem is that cells[] doesn't necessarily sort tl->br,
			// so you have to do this first
			int lastRow=cells[0].GetRow();
			int lastCol=cells[0].GetCol();
			for(int cell=0; cell<cells.size();cell++)
			{

				if(cells[cell].GetRow() > lastRow)
				{
					lastRow=cells[cell].GetRow();
					data+=endline;
				}
				data+=stlStr(GetCellValue(cells[cell].GetRow(),
							cells[cell].GetCol()));

				if(lastCol < cells[cell].GetCol())
				{
					lastCol=cells[cell].GetCol();
					data+="\t";
				}
			}
		} */
		else 
			return;


	}

	// Put the data in the clipboard
	if (wxTheClipboard->Open())
	{
		wxTextDataObject* clipData= new wxTextDataObject;
		// Set data object value
		clipData->SetText(wxStr(data));
		wxTheClipboard->UsePrimarySelection(false);
		wxTheClipboard->SetData(clipData);
		wxTheClipboard->Close();
	}

}


std::string TTFFinder::findFont(const char *fontFile)
{
	//Action is OS dependant
	
#ifdef __APPLE__
		return macFindFont(fontFile);
#elif defined __UNIX_LIKE__ || defined __linux__
		return nxFindFont(fontFile);
#elif defined  __WINDOWS__
		return winFindFont(fontFile);
#else
#error OS not detected in preprocessor series
#endif
}


#ifdef __APPLE__
std::string TTFFinder::macFindFont(const char *fontFile) 
{
	//This is a list of possible target dirs to search
	//(Oh look Ma, I'm autoconf!)
	const char *dirs[] = {	".",
				"/Library/Fonts",
				"" ,
				}; //MUST end with "".

	wxPathList *p = new wxPathList;

	unsigned int ui=0;
	//Try a few standard locations
	while(strlen(dirs[ui]))
	{
		p->Add(wxCStr(dirs[ui]));
		ui++;
	};

	wxString s;

	//execute the search for the file
	s= p->FindValidPath(wxCStr(fontFile));


	std::string res;
	if(s.size())
	{
		if(p->EnsureFileAccessible(s))
			res = stlStr(s);
	}

	delete p;
	return res;
}
#elif defined __UNIX_LIKE__ || defined __linux__
std::string TTFFinder::nxFindFont(const char *fontFile) 
{
	//This is a list of possible target dirs to search
	//(Oh look Ma, I'm autoconf!)

	const char *dirs[] = {	".",
				"/usr/share/fonts/truetype", //Old debian 
				"/usr/share/fonts/truetype/freefont", // New debian
				"/usr/share/fonts/truetype/ttf-dejavu", //New debian
				"/usr/local/share/fonts/truetype", // User fonts
				"/usr/X11R6/lib/X11/fonts/truetype",
				"/usr/X11R6/lib64/X11/fonts/truetype",
				"/usr/lib/X11/fonts/truetype",// Fedora 32
				"/usr/lib64/X11/fonts/truetype", //Fedora 64
				"/usr/local/lib/X11/fonts/truetype", // Fedora 32 new
				"/usr/local/lib64/X11/fonts/truetype",// Fedora 64 new
				"",
				}; //MUST end with "".

	wxPathList *p = new wxPathList;

	unsigned int ui=0;
	//Try a few standard locations
	while(strlen(dirs[ui]))
	{
		p->Add(wxCStr(dirs[ui]));
		ui++;
	};

	wxString s;

	//execute the search for the file
	s= p->FindValidPath(wxCStr(fontFile));


	std::string res;
	if(s.size())
	{
		if(p->EnsureFileAccessible(s))
			res = stlStr(s);
	}

	delete p;
	return res;
}
#elif defined  __WINDOWS__
std::string TTFFinder::winFindFont(const char *fontFile)
{
            //This is a list of possible target dirs to search
	//(Oh look Ma, I'm autoconf!)
	const char *dirs[] = {	".",
               "C:\\Windows\\Fonts",
				"",
				}; //MUST end with "".

	wxPathList *p = new wxPathList;

	unsigned int ui=0;
	//Try a few standard locations
	while(strlen(dirs[ui]))
	{
		p->Add(wxCStr(dirs[ui]));
		ui++;
	};

	wxString s;

  
 
	//execute the search for the file
	s= p->FindValidPath(wxCStr(fontFile));


	std::string res;
	if(s.size())
	{
		if(p->EnsureFileAccessible(s))
			res = stlStr(s);
	}

	delete p;
	return res;


}
#endif

std::string TTFFinder::suggestFontName(unsigned int fontType, unsigned int index) 
{
	//Possible font names
	const char *sansFontNames[] = {
		//First fonts are fonts I have a preference for in my app
		//in my preference order
		"FreeSans.ttf",
		"DejaVuSans.ttf",
		"Arial.ttf",
		"ArialUnicodeMS.ttf",
		"NimbusSansL.ttf",
		"LiberationSans.ttf",
		"Courier.ttf",
		
		//These are simply in semi-alphabetical order
		//may not even be font names (font families) :)
		"AkzidenzGrotesk.ttf",
		"Avenir.ttf",
		"BankGothic.ttf",
		"Barmeno.ttf",
		"Bauhaus.ttf",
		"BellCentennial.ttf",
		"BellGothic.ttf",
		"BenguiatGothic.ttf",
		"Beteckna.ttf",
		"Calibri.ttf",
		"CenturyGothic.ttf",
		"Charcoal.ttf",
		"Chicago.ttf",
		"ClearfaceGothic.ttf",
		"Clearview.ttf",
		"Corbel.ttf",
		"Denmark.ttf",
		"Droid.ttf",
		"Eras.ttf",
		"EspySans.ttf",
		"Eurocrat.ttf",
		"Eurostile.ttf",
		"FFDax.ttf",
		"FFMeta.ttf",
		"FranklinGothic.ttf",
		"Frutiger.ttf",
		"Futura.ttf",
		"GillSans.ttf",
		"Gotham.ttf",
		"Haettenschweiler.ttf",
		"HandelGothic.ttf",
		"Helvetica.ttf",
		"HelveticaNeue.ttf",
		"HighwayGothic.ttf",
		"Hobo.ttf",
		"Impact.ttf",
		"Johnston.ttf"
		"NewJohnston.ttf",
		"Kabel.ttf",
		"LucidaGrande.ttf",
		"Macintosh.ttf",
		"Microgramma.ttf",
		"Motorway.ttf",
		"Myriad.ttf",
		"NewsGothic.ttf",
		"Optima.ttf",
		"Pricedown.ttf",
		"RailAlphabet.ttf",
		"ScalaSans.ttf",
		"SegoeUI.ttf",
		"Skia.ttf",
		"Syntax.ttf",
		"",
	};

	//FIXME: Suggest some font names
	const char *serifFontNames[] = {""};	
					
	//FIXME: Suggest some font names
	const char *monoFontNames[] = {""};	



	std::string s;
	switch(fontType)
	{
		case TTFFINDER_FONT_SANS:
			s = sansFontNames[index];
			break;
		case TTFFINDER_FONT_SERIF:
			s = serifFontNames[index];
			break;
		case TTFFINDER_FONT_MONO:
			s = monoFontNames[index];
			break;
	}

	return s;
}

std::string TTFFinder::getBestFontFile(unsigned int type) 
{
	unsigned int index=0;

	std::string s;

	do
	{
		s=suggestFontName(type,index);

		if(s.size())
		{
			index++;
			s=findFont(s.c_str());
			if(s.size())
			{
				return s;	
			}
		}
		else
			return s;
	}
	while(true);

	ASSERT(false);
	return s;
}
