/*
 *	mathglPane.cpp - mathgl-wx interface panel control
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
#include <wx/sizer.h>
#include <wx/dcbuffer.h>
#include "mathglPane.h"
#include "wxcomponents.h"
 
 #include <mgl/mgl_eps.h>

#include <fstream>

//Uncomment me if using MGL >=1_10 to 
//enable nice drawing of rectangular bars
#define MGL_GTE_1_10

using std::ifstream;
using std::ios;

BEGIN_EVENT_TABLE(MathGLPane, wxPanel)
	EVT_MOTION(MathGLPane::mouseMoved)
	EVT_LEFT_DOWN(MathGLPane::mouseDown)
	EVT_LEFT_UP(MathGLPane::mouseReleased)
	EVT_RIGHT_DOWN(MathGLPane::rightClick)
	EVT_LEAVE_WINDOW(MathGLPane::mouseLeftWindow)
	EVT_LEFT_DCLICK(MathGLPane::mouseDoubleLeftClick) 
	EVT_SIZE(MathGLPane::resized)
	EVT_KEY_DOWN(MathGLPane::keyPressed)
	EVT_KEY_UP(MathGLPane::keyReleased)
	EVT_MOUSEWHEEL(MathGLPane::mouseWheelMoved)
	EVT_PAINT(MathGLPane::render)
END_EVENT_TABLE();


MathGLPane::MathGLPane(wxWindow* parent, int id) :
wxPanel(parent, id,  wxDefaultPosition, wxDefaultSize)
{
	hasResized=true;
	dragging=false;
	thePlot=0;	
	gr=0;
	SetBackgroundStyle(wxBG_STYLE_CUSTOM);// Only if using wxAutoBufferedPaintDC
}

void MathGLPane::setPlot(Multiplot *newPlot)
{
	if(thePlot)
		delete thePlot;

	thePlot=newPlot;

	Refresh();
}

MathGLPane::~MathGLPane()
{
	if(thePlot)
		delete thePlot;
	if(gr)
		delete gr;
}

void MathGLPane::render(wxPaintEvent &event)
{
	wxAutoBufferedPaintDC   *dc=new wxAutoBufferedPaintDC(this); //This does not work  under windows? (I had double paint events before in my event table -- this might work now)

	
	if(!thePlot || !plotSelList)
	{
		delete dc;
		return;
	}

	bool hasChanged;
	hasChanged=thePlot->hasChanged();
	int w,h;
	w=0;h=0;

	GetClientSize(&w,&h);
	
	if(!w || !h)
	{
		delete dc;
		return;
	}

	//Set the enabled and disabled plots
	wxArrayInt a;

	unsigned int nItems =plotSelList->GetSelections(a);

	if(!nItems)
	{
		dc->Clear();

		int clientW,clientH;
		GetClientSize(&clientW,&clientH);
		
		
		wxString str=_("No plots currently selected.");
		dc->GetMultiLineTextExtent(str,&w,&h);
		dc->DrawText(str,(clientW-w)/2, (clientH-h)/2);

		str = _("Use filter tree to generate new plots");
		dc->GetMultiLineTextExtent(str,&w,&h);
		dc->DrawText(str,(clientW-w)/2, (clientH-h)/2+h);
		
		str= _("then select from plot list.");
		dc->GetMultiLineTextExtent(str,&w,&h);
		dc->DrawText(str,(clientW-w)/2, (clientH-h)/2+2*h);


		delete dc;

		return;

	}

	thePlot->hideAll();
	std::vector<unsigned int> newVisible;
	for(unsigned int ui=0;ui<nItems; ui++)
	{
		unsigned int plotID;
		wxListUint *l =(wxListUint *) plotSelList->GetClientObject(a[ui]);
		plotID = l->value;
		newVisible.push_back(ui);
		thePlot->setVisible(plotID,true);
	}


	//If the plot has changed, we need to update it
	//likewise if we don't have a plot, we need one.
	if(!gr || hasChanged || hasResized)
	{
		if(gr)
			delete gr;

		gr = new mglGraphZB(w,h);
		//Draw the plot
		thePlot->drawPlot(gr);	
		hasResized=false;
	}

	//If the visiblity hasn't changed, then reset the 
	//plot to "no changes" state. Otherwise,
	//swap the visibility vectors
	if(lastVisible.size() == newVisible.size() &&
		std::equal(lastVisible.begin(),lastVisible.end(),newVisible.begin()))
		thePlot->resetChange();
	else
		lastVisible.swap(newVisible);
	



	//Copy the plot's memory buffer into a wxImage object, then draw it	
	wxImage img(w,h,const_cast<unsigned char*>(gr->GetBits()),true);
	dc->DrawBitmap(wxBitmap(img),0,0);

	//If we are engaged in a draggin operation
	//draw the nice little bits we need
	if(dragging)
	{
		
		//Draw a rectangle between the start and end positions
		wxCoord tlX,tlY,wRect,hRect;

		if(draggingStart.x < draggingCurrent.x)
		{
			tlX=draggingStart.x;
			wRect = draggingCurrent.x - tlX;
		}
		else
		{
			tlX=draggingCurrent.x;
			wRect = draggingStart.x - tlX;
		}

		if(draggingStart.y < draggingCurrent.y)
		{
			tlY=draggingStart.y;
			hRect = draggingCurrent.y - tlY;
		}
		else
		{
			tlY=draggingCurrent.y;
			hRect = draggingStart.y - tlY;
		}
	
		dc->SetBrush(wxBrush(*wxBLUE,wxTRANSPARENT));	

		const int END_MARKER_SIZE=5;

		bool useNiceAxis=0;

		//Compute the MGL coords
		int axisX,axisY;
		axisX=axisY=0;
#ifdef MGL_GTE_1_10
		gr->CalcScr(gr->Org,&axisX,&axisY);
		axisY=h-axisY;
		//Use a nicer looking axis drawing
		useNiceAxis=1;
#endif
		//If the cursor is wholly beloe
		//the axis, draw a line rather than abox

		if(draggingStart.x < axisX  && draggingCurrent.x < axisX )
		{
			if(draggingStart.y > axisY && draggingCurrent.y > axisY ) //y axis inverted
			{
				//corner event TODO: Nice feedback would be good.
				//	not sure what though
			}	
			else
			{	
				//left of X-Axis event
				//Draw a little I beam.
				dc->DrawLine(draggingStart.x,tlY,
						draggingStart.x,tlY+hRect);
				dc->DrawLine(draggingStart.x-END_MARKER_SIZE,tlY+hRect,
						draggingStart.x+END_MARKER_SIZE,tlY+hRect);
				dc->DrawLine(draggingStart.x-END_MARKER_SIZE,tlY,
						draggingStart.x+END_MARKER_SIZE,tlY);
			}
		}
		else if(draggingStart.y > axisY  && draggingCurrent.y > axisY ) //Y axis is inverted
		{
			//below Y axis event
			//Draw a little |-| beam.
			dc->DrawLine(tlX,draggingStart.y,
					tlX+wRect,draggingStart.y);
			dc->DrawLine(tlX+wRect,draggingStart.y-END_MARKER_SIZE,
					tlX+wRect,draggingStart.y+END_MARKER_SIZE);
			dc->DrawLine(tlX,draggingStart.y-END_MARKER_SIZE,
					tlX,draggingStart.y+END_MARKER_SIZE);

		}
		else
			dc->DrawRectangle(tlX,tlY,wRect,hRect);
					
	}    
	

	delete dc;
}

void MathGLPane::resized(wxSizeEvent& evt)
{
	hasResized=true;
    Refresh();
}

void MathGLPane::mouseMoved(wxMouseEvent& event)
{

	int w,h;
	GetClientSize(&w,&h);
	
	if(!w || !h || !thePlot)
		return;

	if(dragging)
	{
		if(!event.m_leftDown)
			dragging=false;
		else
			draggingCurrent=event.GetPosition();
	}

	if(!gr)
		return;
	

	wxPoint curMouse=event.GetPosition();	
	//Update mouse cursor
	//---------------
	//Draw a rectangle between the start and end positions
	bool useNiceAxis=0;

	//Compute the MGL coords
	int axisX,axisY;
	axisX=axisY=0;
#ifdef MGL_GTE_1_10
	gr->CalcScr(gr->Org,&axisX,&axisY);
	axisY=h-axisY;
	//Use a nicer looking axis drawing
	useNiceAxis=1;
#endif
	//If the cursor is wholly beloe
	//the axis, draw a line rather than abox

	if(useNiceAxis)
	{
		if(curMouse.x < axisX)
		{
			if(curMouse.y > axisY)
			{
				//Set cursor to normal
				SetCursor(wxNullCursor);
			}	
			else
			{	
				//left of X-Axis event, draw up-down arrow
				SetCursor(wxCURSOR_SIZENS);
			}
		}
		else if(curMouse.y > axisY )
			SetCursor(wxCURSOR_SIZEWE);
		else
			SetCursor(wxCURSOR_MAGNIFIER);
	}
	//---------------

	Refresh();

}

void MathGLPane::mouseDoubleLeftClick(wxMouseEvent& event)
{
	if(!thePlot)
		return;

	dragging=false;
	//reset plot bounds
	thePlot->disableUserBounds();	

	Refresh();
}


void MathGLPane::mouseDown(wxMouseEvent& event)
{
	if(gr)
	{
		dragging=true;
		draggingStart = event.GetPosition();
		SetFocus(); //Why am I calling setFocus?? can't remeber.. 
	}

}

void MathGLPane::mouseWheelMoved(wxMouseEvent& event)
{
}

void MathGLPane::mouseReleased(wxMouseEvent& event)
{

	if(!thePlot || !gr)
		return;
	wxPoint draggingEnd = event.GetPosition();


	unsigned int startX, endX,startY,endY;


	int w,h;
	GetSize(&w,&h);
	//Define the rectangle
	if(draggingEnd.x > draggingStart.x)
	{
		startX=draggingStart.x;
		endX=draggingEnd.x;
	}
	else
	{
		startX=draggingEnd.x;
		endX=draggingStart.x;
	}

	if(h-draggingEnd.y > h-draggingStart.y)
	{
		startY=draggingStart.y;
		endY=draggingEnd.y;
	}
	else
	{
		startY=draggingEnd.y;
		endY=draggingStart.y;
	}

	if(startX == endX || startY == endY )
		return ;


	//Compute the MGL coords
	mglPoint pStart,pEnd;
	pStart = gr->CalcXYZ(startX,startY);
	pEnd = gr->CalcXYZ(endX,endY);


	mglPoint cA=gr->Org;

	float currentAxisX,currentAxisY;
	currentAxisX=cA.x;
	currentAxisY=cA.y;
	
	if(pStart.x < currentAxisX  && pEnd.x < currentAxisX )
	{
		if(pStart.y < currentAxisY && pEnd.y < currentAxisY )
		{
			//corner event
			return ; // Do nothing
		}	
		else
		{	
			//left of X-Axis event
			//Reset the axes such that the
			//zoom is only along one dimension (y)
			pStart.x = gr->Min.x;
		        pEnd.x = gr->Max.x;	
		}
	}
	else if(pStart.y < currentAxisY  && pEnd.y < currentAxisY )
	{
		//below Y axis event
		//Reset the axes such that the
		//zoom is only along one dimension (x)
		pStart.y = gr->Min.y;
		pEnd.y = gr->Max.y;	

	}
	
	
	//now that we have the rectangle defined, 
	//Allow for the plot to be zoomed
	thePlot->setBounds(std::min(pStart.x,pEnd.x),std::max(pStart.x,pEnd.x),
				std::min(pStart.y,pEnd.y),std::max(pStart.y,pEnd.y));

	
	dragging=false;
	//Repaint
	Refresh();
}

void MathGLPane::rightClick(wxMouseEvent& event) 
{
}

void MathGLPane::mouseLeftWindow(wxMouseEvent& event) 
{
}

void MathGLPane::keyPressed(wxKeyEvent& event) 
{
}

void MathGLPane::keyReleased(wxKeyEvent& event) 
{
}


bool MathGLPane::savePNG(const std::string &filename, 
		unsigned int width, unsigned int height)
{

	if(gr)
		delete gr;

	ASSERT(filename.size());
	gr = new mglGraphZB(width,height);
	thePlot->drawPlot(gr);	

	gr->WritePNG(filename.c_str());

	gr=0;
	//Hack. mathgl does not return an error value from its writer
	//function :(. Check to see that the file is openable, and nonzero sized
	
	ifstream f(filename.c_str(),ios::binary);

	if(!f)
		return false;

	f.seekg(0,ios::end);


	if(!f.tellg())
		return false;

	return true;
}

bool MathGLPane::saveSVG(const std::string &filename)
{
	ASSERT(filename.size());


	mglGraphPS *grS;
	//Width and height are not *really* important, 
	//but it can be used to set the bounding size
	grS = new mglGraphPS(1024,768);


	thePlot->drawPlot(grS);
	
	grS->WriteSVG(filename.c_str());

	delete grS;

	//Hack. mathgl does not return an error value from its writer
	//function :(. Check to see that the file is openable, and nonzero sized
	
	ifstream f(filename.c_str(),ios::binary);

	if(!f)
		return false;

	f.seekg(0,ios::end);


	if(!f.tellg())
		return false;
	return true;

}


bool MathGLPane::saveJPG(const std::string &filename,
		unsigned int width, unsigned int height)
{

	if(gr)
		delete gr;

	ASSERT(filename.size());
	gr = new mglGraphZB(width,height);
	thePlot->drawPlot(gr);	

	gr->WriteJPEG(filename.c_str());

	gr=0;
	//Hack. mathgl does not return an error value from its writer
	//function :(. Check to see that the file is openable, and nonzero sized
	
	ifstream f(filename.c_str(),ios::binary);

	if(!f)
		return false;

	f.seekg(0,ios::end);


	if(!f.tellg())
		return false;

	return true;
}

void MathGLPane::setPlotVisible(unsigned int plotId, bool visible)
{
	thePlot->setVisible(plotId,visible);	
}
