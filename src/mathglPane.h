/*
 *	mathglPanel.h - WxWidgets plotting panelf or interaction with mathgl
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


#include "wxPreprec.h"
#include <wx/image.h>

#include "plot.h"

#include <mgl/mgl_zb.h>

#include <vector>
#ifndef MATHGLPANE_H
#define MATHGLPANE_H

// begin wxGlade: ::extracode
// end wxGlade


class MathGLPane: public wxPanel {
private:
	//!Is the user dragging with the mouse?
	bool dragging;
	//!Has the window resized since the last draw?
	bool hasResized;
	//!Start and currentlocations for the drag
	wxPoint draggingStart,draggingCurrent;

	Multiplot *thePlot;
	wxListBox *plotSelList;
	mglGraphZB *gr;	
	//!Caching check vector.
	std::vector<unsigned int> lastVisible;
public:

	MathGLPane(wxWindow* parent, int id);
	virtual ~MathGLPane();

	//Set the plot pointer for this class to manipulate
	void setPlot(Multiplot *plot);
	//!Set the plot listbox for this class to use
	void setPlotList(wxListBox *box){plotSelList=box;};

	//save an SVG file
	bool saveSVG(const std::string &filename);
	//Save a PNG file
	bool savePNG(const std::string &filename,
			unsigned int width,unsigned int height);
	//Save a JPG file
	bool saveJPG(const std::string &filename,
			unsigned int width,unsigned int height);

	unsigned int getNumVisible() const {return thePlot->getNumVisible();}; 

	void resized(wxSizeEvent& evt);
	void render(wxPaintEvent& evt);
	//!wx Event that triggers on mouse movement on grah
	void mouseMoved(wxMouseEvent& event);
	void mouseDown(wxMouseEvent& event);
	void mouseDoubleLeftClick(wxMouseEvent& event);
	void mouseWheelMoved(wxMouseEvent& event);
	void mouseReleased(wxMouseEvent& event);
	void rightClick(wxMouseEvent& event);
	void mouseLeftWindow(wxMouseEvent& event);
	void keyPressed(wxKeyEvent& event);
	void keyReleased(wxKeyEvent& event);
	void setPlotVisible(unsigned int plotID, bool visible);
protected:
    DECLARE_EVENT_TABLE()
};



#endif
