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


#include "plot.h"

#include <mgl/mgl_zb.h>
#undef is_nan

#include <vector>
#ifndef MATHGLPANE_H
#define MATHGLPANE_H

// begin wxGlade: ::extracode
// end wxGlade

//Error codes
enum
{
	MGLPANE_ERR_BADALLOC=1,
	MGLPANE_ERR_MGLWARN,
	MGLPANE_FILE_REOPEN_FAIL,
	MGLPANE_FILE_UNSIZED_FAIL,
	MGLPANE_ERRMAX,
};


class MathGLPane: public wxPanel {
private:
	//Current mouse position
	wxPoint curMouse;
	//!Has the mouse left the window?
	bool leftWindow;
	//!Last error reported by mathgl
	std::string lastMglErr;
	//!Is the user dragging with the mouse?
	bool dragging, panning, regionDragging;
	//!Has the window resized since the last draw?
	bool hasResized;
	//!Start and currentlocations for the drag
	wxPoint draggingStart,draggingCurrent;
	//!Original bounds during panning operations
	float origPanMinX, origPanMaxX;

	//!region used at mouse down
	unsigned int startMouseRegion,startMousePlot,regionMoveType;

	//!Do we have region updates?
	bool haveUpdates;

	//!Should we be limiting interaction to 
	//things that won't modify filters (eg region dragging)
	bool limitInteract;

	//!Pointer to the plot data holding class
	PlotWrapper *thePlot;
	//!Pointer to the listbox that is used for plot selection
	wxListBox *plotSelList;
	//!Pointer to the mathgl renderer
	mglGraphZB *gr;	
	//!Caching check vector for plot visibility
	std::vector<unsigned int> lastVisible;

	//!Update the mouse cursor style, based upon mouse position
	void updateMouseCursor();

	//!Compute the "aspect" for a given region; ie which grab handle type
	// does this position correspond to
	unsigned int computeRegionMoveType(float dataX,float dataY, const PlotRegion &r) const;
	
public:

	MathGLPane(wxWindow* parent, int id);
	virtual ~MathGLPane();

	//Set the plot pointer for this class to manipulate
	void setPlotWrapper(PlotWrapper *plots);
	//!Set the plot listbox for this class to use
	void setPlotList(wxListBox *box){plotSelList=box;};

	std::string getErrString(unsigned int code); 

	//save an SVG file
	unsigned int saveSVG(const std::string &filename);
	//Save a PNG file
	unsigned int savePNG(const std::string &filename,
			unsigned int width,unsigned int height);
	//Get the number of visible plots
	unsigned int getNumVisible() const {return thePlot->getNumVisible();}; 

	//!Get the region under the cursor. Returns region num [0->...) or
	//numeric_limits<unsigned int>::max() if nothing.
	bool getRegionUnderCursor(const wxPoint &mousePos, unsigned int &plotId, unsigned int &regionId) const;

	//!Do we have updates?
	bool hasUpdates() const { return haveUpdates;}
	void clearUpdates() { haveUpdates=false;}
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
	void setLegendVisible(bool visible){thePlot->setLegendVisible(visible);}
	void limitInteraction(bool doLimit=true){ limitInteract=doLimit;};
protected:
    DECLARE_EVENT_TABLE()
};



#endif
