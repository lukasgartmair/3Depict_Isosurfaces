/*
 *	mathglPane.cpp - mathgl-wx interface panel control
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

#include <wx/wx.h>
#include <wx/dcbuffer.h>
#include "wxcomponents.h"

#include "mathglPane.h"

#include "wxcommon.h"
#include "common/translation.h"

#ifdef USE_MGL2
	#include <mgl2/canvas_wnd.h>
	#include <mgl2/canvas_wnd.h>
#else
	#include <mgl/mgl_eps.h>
#endif

//Panning speed modifier
const float MGL_PAN_SPEED=0.8f;
//Mathgl uses floating point loop computation, and can get stuck. Limit zoom precision
const float MGL_ZOOM_LIMIT=10.0f*sqrt(std::numeric_limits<float>::epsilon());


//Mouse action types
enum
{
	MOUSE_MODE_DRAG, //Free mouse drag on plot
	MOUSE_MODE_DRAG_PAN, //dragging mouse using a "panning" action
	MOUSE_MODE_DRAG_REGION, //Dragging a region
	MOUSE_MODE_ENUM_END
};

//Do the particular enums require a redraw?
const bool MOUSE_ACTION_NEEDS_REDRAW[] = { false,true,true};



using std::ifstream;
using std::ios;

BEGIN_EVENT_TABLE(MathGLPane, wxPanel)
	EVT_MOTION(MathGLPane::mouseMoved)
	EVT_LEFT_DOWN(MathGLPane::leftMouseDown)
	EVT_LEFT_UP(MathGLPane::leftMouseReleased)
	EVT_MIDDLE_DOWN(MathGLPane::middleMouseDown)
	EVT_MIDDLE_UP(MathGLPane::middleMouseReleased)
	EVT_RIGHT_DOWN(MathGLPane::rightClick)
	EVT_LEAVE_WINDOW(MathGLPane::mouseLeftWindow)
	EVT_LEFT_DCLICK(MathGLPane::mouseDoubleLeftClick) 
	EVT_SIZE(MathGLPane::resized)
	EVT_KEY_DOWN(MathGLPane::keyPressed)
	EVT_KEY_UP(MathGLPane::keyReleased)
	EVT_MOUSEWHEEL(MathGLPane::mouseWheelMoved)
	EVT_PAINT(MathGLPane::render)
END_EVENT_TABLE();


enum
{
	AXIS_POSITION_INTERIOR=1,
	AXIS_POSITION_LOW_X=2,
	AXIS_POSITION_LOW_Y=4,
};


MathGLPane::MathGLPane(wxWindow* parent, int id) :
wxPanel(parent, id,  wxDefaultPosition, wxDefaultSize)
{
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(MOUSE_ACTION_NEEDS_REDRAW) == MOUSE_MODE_ENUM_END);

	hasResized=true;
	limitInteract=haveUpdates=false;
	mouseDragMode=MOUSE_MODE_ENUM_END;	
	leftWindow=true;
	thePlot=0;	
	gr=0;

	SetBackgroundStyle(wxBG_STYLE_CUSTOM);

}

MathGLPane::~MathGLPane()
{
	if(thePlot)
		delete thePlot;
	if(gr)
		delete gr;
}


unsigned int MathGLPane::getAxisMask(int x, int y) const
{
	
	ASSERT(thePlot && gr);
	int w,h;
	w=0;h=0;

	//Retrieve wx coordinates for window rectangle size
	GetClientSize(&w,&h);

	//Retrieve XY position in graph coordinate space
	// from XY coordinate in window space.
	mglPoint mglCurMouse= gr->CalcXYZ(x,y);

	unsigned int retVal=0;

#ifdef USE_MGL2
	if(mglCurMouse.x < gr->Self()->GetOrgX('x'))
		retVal |=AXIS_POSITION_LOW_X;

	if(mglCurMouse.y < gr->Self()->GetOrgY('y'))
		retVal |=AXIS_POSITION_LOW_Y;
#else
	if(mglCurMouse.x < gr->Org.x)
		retVal |=AXIS_POSITION_LOW_X;

	if(mglCurMouse.y < gr->Org.y)
		retVal |=AXIS_POSITION_LOW_Y;
#endif

	if(!retVal)
		retVal=AXIS_POSITION_INTERIOR;

	return retVal;
}

void MathGLPane::setPlotWrapper(PlotWrapper *newPlot)
{
	if(thePlot)
		delete thePlot;

	thePlot=newPlot;

	Refresh();
}



void MathGLPane::render(wxPaintEvent &event)
{
	
    wxAutoBufferedPaintDC   *dc=new wxAutoBufferedPaintDC(this);

	if(!thePlot || !plotSelList || thePlot->isInteractionLocked() )
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
#ifdef __WXGTK__
		wxBrush *b = new wxBrush;
		b->SetColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BACKGROUND));
		dc->SetBackground(*b);

		dc->SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
#endif
#if !defined(__APPLE__) 
		dc->Clear();
#endif

		int clientW,clientH;
		GetClientSize(&clientW,&clientH);
		
		
		wxString str=wxTRANS("No plots selected.");
		dc->GetMultiLineTextExtent(str,&w,&h);
		dc->DrawText(str,(clientW-w)/2, (clientH-h)/2);

#ifdef __WXGTK__
		delete b;
#endif
		delete dc;

		return;

	}

	//Set which plots should be visible
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




	//If the plot has changed, been resized or is performing
	// a mouse action that requires updating, we need to update it
	//likewise if we don't have a plot, we need one.
	if(!gr || hasChanged || hasResized || 
		MOUSE_ACTION_NEEDS_REDRAW[mouseDragMode])
	{

		//clear the plot drawing entity
		if(gr)
			delete gr;

#ifdef USE_MGL2
		gr = new mglGraph(0,w,h);
#else
		gr = new mglGraphZB(w,h);
#endif
		//change the plot by panningOneD it before we draw.
		//if we need to 
		if(mouseDragMode==MOUSE_MODE_DRAG_PAN)
		{
			float xMin,xMax,yMin,yMax;
			thePlot->getBounds(xMin,xMax,yMin,yMax);
		
			mglPoint pEnd,pStart;
			pEnd = gr->CalcXYZ(draggingCurrent.x,draggingCurrent.y);
			pStart = gr->CalcXYZ(draggingStart.x,draggingStart.y);
			
			float offX = pEnd.x-pStart.x;

			//I cannot for the life of me work out why
			//this extra transformation is needed.
			//but without this the code produces a scale-dependant pan speed.
			offX*=xMax-xMin;

			//Modify for speed
			offX*=MGL_PAN_SPEED;

			thePlot->setBounds(origPanMinX+offX/2,+origPanMaxX + offX/2.0,
						yMin,yMax);
		}

		//Draw the plot
		thePlot->drawPlot(gr);	
		hasResized=false;
	}

	//If the visibility hasn't changed, then reset the 
	//plot to "no changes" state. Otherwise,
	//swap the visibility vectors
	if(lastVisible.size() == newVisible.size() &&
		std::equal(lastVisible.begin(),lastVisible.end(),newVisible.begin()))
		thePlot->resetChange();
	else
		lastVisible.swap(newVisible);

	//Copy the plot's memory buffer into a wxImage object, then draw it	
#ifdef USE_MGL2
	char *rgbdata = (char*)malloc(w*h*3);
	gr->GetRGB((char*)rgbdata,w*h*3);
	
	wxImage img(w,h,(unsigned char*)rgbdata,true);
	free(rgbdata);
#else
	wxImage img(w,h,const_cast<unsigned char*>(gr->GetBits()),true);
#endif
	dc->DrawBitmap(wxBitmap(img),0,0);
	//If we are engaged in a dragging operation
	//draw the nice little bits we need
	switch(mouseDragMode)
	{
		case MOUSE_MODE_DRAG:
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

		//If the cursor is wholly below
			//the axis, draw a line rather than a box

			unsigned int startMask, endMask;
			startMask=getAxisMask(draggingStart.x, draggingStart.y);
			endMask=getAxisMask(draggingCurrent.x, draggingCurrent.y);

			if( (startMask & AXIS_POSITION_LOW_X)
				&& (endMask & AXIS_POSITION_LOW_X) )
			{
				if( !((startMask &AXIS_POSITION_LOW_Y) && (endMask & AXIS_POSITION_LOW_Y)))
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
			else if( (startMask & AXIS_POSITION_LOW_Y)
				&& (endMask & AXIS_POSITION_LOW_Y) )
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
		
			break;
		}    
		case MOUSE_MODE_DRAG_REGION:	
		{
			drawRegionDraggingOverlay(dc);
			break;
		}
		case MOUSE_MODE_DRAG_PAN:

			break;
		case MOUSE_MODE_ENUM_END:
		{
			unsigned int axisMask;
			axisMask=getAxisMask(curMouse.x,curMouse.y);

			//Draw any overlays
			if(axisMask == AXIS_POSITION_INTERIOR)
				drawInteractOverlay(dc);
			break;
		}
		default:
			ASSERT(false);
	}
	
	delete dc;
}

void MathGLPane::resized(wxSizeEvent& evt)
{
	hasResized=true;
	Refresh();
}


void MathGLPane::updateMouseCursor()
{
	int w,h;
	w=0;h=0;

	GetClientSize(&w,&h);
	
	if(!w || !h || !thePlot )
		return;

	//Set cursor to normal by default
		SetCursor(wxNullCursor);
	if(!thePlot->getNumVisible())
		return;

	//Update mouse cursor
	//---------------
	//Draw a rectangle between the start and end positions

	//If we are using shift, we slide along X axis anyway
	if(wxGetKeyState(WXK_SHIFT))
		SetCursor(wxCURSOR_SIZEWE);
	else
	{

		//If the cursor is wholly beloe
		//the axis, draw a line rather than abox
		unsigned int axisMask=getAxisMask(curMouse.x,curMouse.y);

		float xMin,xMax,yMin,yMax;

		thePlot->getBounds(xMin,xMax,yMin,yMax);
		//Look at mouse position relative to the axis position
		//to determine the cursor style.
		switch(axisMask)
			{	
			case AXIS_POSITION_LOW_X:
				//left of X-Axis event, draw up-down arrow
				SetCursor(wxCURSOR_SIZENS);
				break;
			case AXIS_POSITION_LOW_Y:
				//Below Y axis, draw line // to x axis
			SetCursor(wxCURSOR_SIZEWE);
				break;
			case AXIS_POSITION_INTERIOR:
				SetCursor(wxCURSOR_MAGNIFIER);
				break;
			default:
				;
		}
	}
	//---------------
}

bool MathGLPane::getRegionUnderCursor(const wxPoint  &mousePos, unsigned int &plotId,
								unsigned int &regionId) const
{
	ASSERT(gr);

	//Convert the mouse coordinates to data coordinates.
	mglPoint pMouse= gr->CalcXYZ(mousePos.x,mousePos.y);


	//Only allow  range interaction within the plot bb
#ifdef USE_MGL2
	if(pMouse.x > gr->Self()->Max.x || pMouse.x < gr->Self()->Min.x)
		return false;
#else
	if(pMouse.x > gr->Max.x || pMouse.x < gr->Min.x)
		return false;
#endif
	//check if we actually have a region
	if(!thePlot->getRegionIdAtPosition(pMouse.x,pMouse.y,plotId,regionId))
		return false;
	
	return true;
}


void MathGLPane::mouseMoved(wxMouseEvent& event)
{
	leftWindow=false;
	if(!thePlot || !gr || thePlot->isInteractionLocked() )
	{
		mouseDragMode=MOUSE_MODE_ENUM_END;
		return;
	}

	curMouse=event.GetPosition();	
	
	switch(mouseDragMode)
	{
		case MOUSE_MODE_DRAG:
			if(!event.m_leftDown)
				mouseDragMode=MOUSE_MODE_ENUM_END;
			else
				draggingCurrent=event.GetPosition();
			break;
		case MOUSE_MODE_DRAG_PAN:
			//Can only be dragging with shift/left or middle down
			// we might not receive an left up if the user
			// exits the window and then releases the mouse
			if(!((event.m_leftDown && event.m_shiftDown) || event.m_middleDown))
				mouseDragMode=MOUSE_MODE_ENUM_END;
			else
				draggingCurrent=event.GetPosition();
			break;
		default:
			;
	}
	//Check if we are still dragging
	if(!(event.m_leftDown || event.m_middleDown) || limitInteract)
		mouseDragMode=MOUSE_MODE_ENUM_END;
	else
		draggingCurrent=event.GetPosition();


	updateMouseCursor();

	Refresh();

}

void MathGLPane::mouseDoubleLeftClick(wxMouseEvent& event)
{
	if(!thePlot || !gr || thePlot->isInteractionLocked())
		return;

	//Cancel any mouse drag mode
	mouseDragMode=MOUSE_MODE_ENUM_END;
	
	int w,h;
	w=0;h=0;
	GetClientSize(&w,&h);
	
	if(!w || !h )
		return;
	
	unsigned int axisMask=getAxisMask(curMouse.x,curMouse.y);

	switch(axisMask)
	{	
		case AXIS_POSITION_LOW_X:
			//left of X-Axis -- plot Y zoom
			thePlot->disableUserAxisBounds(false);	
			break;
		case AXIS_POSITION_LOW_Y:
			//Below Y axis; plot X Zoom
			thePlot->disableUserAxisBounds(true);	
			break;
		case AXIS_POSITION_INTERIOR:
			//reset plot bounds
			thePlot->disableUserBounds();	
			break;
		default:
			//bottom corner
			thePlot->disableUserBounds();	
	}

	Refresh();
}


void MathGLPane::oneDMouseDownAction(bool leftDown,bool middleDown,
		 bool alternateDown, int dragX,int dragY)
{
	ASSERT(thePlot->getNumVisible());
	
	int w,h;
	w=0;h=0;

	GetClientSize(&w,&h);

	float xMin,xMax,yMin,yMax;
	thePlot->getBounds(xMin,xMax,yMin,yMax);

	//Set the interaction mode
	if(leftDown && !alternateDown )
	{
		int axisMask;
		axisMask = getAxisMask(curMouse.x,curMouse.y);
		
		draggingStart = wxPoint(dragX,dragY);
		//check to see if we have hit a region
		unsigned int plotId,regionId;
		if(!limitInteract && !(axisMask &(AXIS_POSITION_LOW_X | AXIS_POSITION_LOW_Y))
			&& getRegionUnderCursor(curMouse,plotId,regionId))
		{
			PlotRegion r;
			thePlot->getRegion(plotId,regionId,r);

			//TODO: Implement a more generic region handler?
			ASSERT(thePlot->plotType(plotId) == PLOT_TYPE_ONED);

			mglPoint mglDragStart = gr->CalcXYZ(draggingStart.x,draggingStart.y);
			//Get the type of move, and the region
			//that is being moved, as well as the plot that this
			//region belongs to.
			regionMoveType=computeRegionMoveType(mglDragStart.x,mglDragStart.y, r);
			startMouseRegion=regionId;
			startMousePlot=plotId;
			mouseDragMode=MOUSE_MODE_DRAG_REGION;
		}
		else
			mouseDragMode=MOUSE_MODE_DRAG;
	
	}
	
	
	if( (leftDown && alternateDown) || middleDown)
	{
		mouseDragMode=MOUSE_MODE_DRAG_PAN;
		draggingStart = wxPoint(dragX,dragY);
		
		origPanMinX=xMin;
		origPanMaxX=xMax;
	}

}

void MathGLPane::leftMouseDown(wxMouseEvent& event)
{
	if(!gr || !thePlot->getNumVisible() || thePlot->isInteractionLocked())
		return;

	int w,h;
	w=h=0;
	GetClientSize(&w,&h);
	if(!w || !h)
		return;

	switch(thePlot->getVisibleType())
	{
		case PLOT_TYPE_ONED:
			oneDMouseDownAction(event.LeftDown(),false,
						event.ShiftDown(),
						event.GetPosition().x,
						event.GetPosition().y);
			break;
		case PLOT_TYPE_ENUM_END:
			//Do nothing
			break;
		default:
			ASSERT(false);
	}

	event.Skip();
}

void MathGLPane::middleMouseDown(wxMouseEvent &event)
{
	if(!gr || !thePlot->getNumVisible() || thePlot->isInteractionLocked())
		return;
	
	int w,h;
	w=0;h=0;
	GetClientSize(&w,&h);
	
	if(!w || !h)
		return;
	
	switch(thePlot->getVisibleType())
	{
		case PLOT_TYPE_ONED:
			oneDMouseDownAction(false,event.MiddleDown(),
						event.ShiftDown(),
						event.GetPosition().x,
						event.GetPosition().y);
			break;
		case PLOT_TYPE_ENUM_END:
			//Do nothing
			break;
		default:
			ASSERT(false);
	}
	
	event.Skip();
}

void MathGLPane::mouseWheelMoved(wxMouseEvent& event)
{
	//If no valid plot, don't do anything
	if(!thePlot || !gr || thePlot->isInteractionLocked() )
		return;
	
	//no action if currently dragging
	if(!(mouseDragMode==MOUSE_MODE_ENUM_END))
		return;

	unsigned int axisMask=getAxisMask(curMouse.x,curMouse.y);



	//Bigger numbers mean faster
	const float SCROLL_WHEEL_ZOOM_RATE=0.75;
	float zoomRate=(float)event.GetWheelRotation()/(float)event.GetWheelDelta();
	zoomRate=zoomRate*SCROLL_WHEEL_ZOOM_RATE;

	//Convert from additive space to multiplicative
	float zoomFactor;
	if(zoomRate < 0.0f)
		zoomFactor=-1.0f/zoomRate;
	else
		zoomFactor=zoomRate;



	ASSERT(zoomFactor >0.0f);


	mglPoint mousePos;
	mousePos=gr->CalcXYZ(curMouse.x,curMouse.y);

	float xPlotMin,xPlotMax,yPlotMin,yPlotMax;
	float xMin,xMax,yMin,yMax;
	//Get the current bounds for the plot
	thePlot->getBounds(xMin,xMax,yMin,yMax);
	//Get the absolute bounds for the plot
	thePlot->scanBounds(xPlotMin,xPlotMax,yPlotMin,yPlotMax);

	
	//Zoom around the point
	switch(axisMask)
	{
		//below x axis -> y zoom only
		case AXIS_POSITION_LOW_X:
		{
			float newYMax,newYMin;
			//Zoom along Y
			newYMin= mousePos.y + (yMin-mousePos.y)*zoomFactor ;
			newYMax= mousePos.y + (yMax-mousePos.y)*zoomFactor ;
	
			newYMin=std::max(yPlotMin,newYMin);
			newYMax=std::min(yPlotMax,newYMax);
		
			if(newYMax- newYMin> MGL_ZOOM_LIMIT)
				thePlot->setBounds(xMin,xMax,newYMin,newYMax);
			break;
		}
		//Below y axis -> x zoom only
		case AXIS_POSITION_LOW_Y:
		{
			float newXMax,newXMin;
			//Zoom along X
			newXMin= mousePos.x + (xMin-mousePos.x)*zoomFactor ;
			newXMax= mousePos.x + (xMax-mousePos.x)*zoomFactor ;
	
			newXMin=std::max(xPlotMin,newXMin);
			newXMax=std::min(xPlotMax,newXMax);
		
			if(newXMax - newXMin > MGL_ZOOM_LIMIT)	
				thePlot->setBounds(newXMin,newXMax,yMin,yMax);
			break;
		}
		//Zoom both axes
		case AXIS_POSITION_INTERIOR:
		{
			float newXMax,newXMin;
			float newYMax,newYMin;
			
			//Zoom along X
			newXMin= mousePos.x + (xMin-mousePos.x)*zoomFactor ;
			newXMax= mousePos.x + (xMax-mousePos.x)*zoomFactor ;
			newYMin= mousePos.y + (yMin-mousePos.y)*zoomFactor ;
			newYMax= mousePos.y + (yMax-mousePos.y)*zoomFactor ;
	
				
			newXMin=std::max(xPlotMin,newXMin);
			newXMax=std::min(xPlotMax,newXMax);
			newYMin=std::max(yPlotMin,newYMin);
			newYMax=std::min(yPlotMax,newYMax);
			
			if(newXMax - newXMin > MGL_ZOOM_LIMIT &&
				newYMax - newYMin > MGL_ZOOM_LIMIT)	
				thePlot->setBounds(newXMin,newXMax,newYMin,newYMax);
			break;
		}
		default:
			;
	}

	Refresh();
}

void MathGLPane::leftMouseReleased(wxMouseEvent& event)
{

	if(!thePlot || !gr || thePlot->isInteractionLocked() )
		return;

	switch(mouseDragMode)
	{
		case MOUSE_MODE_DRAG:
		{
			wxPoint draggingEnd = event.GetPosition();
			updateDragPos(draggingEnd);
			Refresh();
			break;
		}
		case MOUSE_MODE_DRAG_REGION:
		{
			if(!limitInteract)
			{
				//we need to tell viscontrol that we have done a region
				//update
				mglPoint mglCurMouse= gr->CalcXYZ(curMouse.x,curMouse.y);
			
				//Send the movement to the parent filter
				thePlot->moveRegion(startMousePlot,
					startMouseRegion,regionMoveType,mglCurMouse.x,mglCurMouse.y);	
				haveUpdates=true;	

			}
			Refresh();
			break;
		}
		default:
		;
	}

	mouseDragMode=MOUSE_MODE_ENUM_END;
	Refresh();
}

void MathGLPane::middleMouseReleased(wxMouseEvent& event)
{

	if(!gr || thePlot->isInteractionLocked())
		return;

	if(mouseDragMode == MOUSE_MODE_DRAG_PAN)
	{
		mouseDragMode=MOUSE_MODE_ENUM_END;
		//Repaint
		Refresh();
	}
}

void MathGLPane::updateDragPos(const wxPoint &draggingEnd) const
{
	ASSERT(mouseDragMode== MOUSE_MODE_DRAG);

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

	//Check that the start and end were not the same (i.e. null zoom in all cases)
	if(startX == endX && startY == endY )
		return ;


	//Compute the MGL coords
	mglPoint pStart,pEnd;
	pStart = gr->CalcXYZ(startX,startY);
	pEnd = gr->CalcXYZ(endX,endY);


	mglPoint cA;
#ifdef USE_MGL2
	cA.x=gr->Self()->GetOrgX('x');
	cA.y=gr->Self()->GetOrgY('y');
#else
	cA=gr->Org;
#endif
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
			//Check if can't do anything with this, as it is a null zoom
			if(startY == endY)
				return;

			//left of X-Axis event
			//Reset the axes such that the
			//zoom is only along one dimension (y)
#ifdef USE_MGL2
			pStart.x = gr->Self()->Min.x;
			pEnd.x = gr->Self()->Max.x;
#else
			pStart.x = gr->Min.x;
			pEnd.x = gr->Max.x;
#endif
		}
	}
	else if(pStart.y < currentAxisY  && pEnd.y < currentAxisY )
	{
		//Check if can't do anything with this, as it is a null zoom
		if(startX == endX)
			return;
		//below Y axis event
		//Reset the axes such that the
		//zoom is only along one dimension (x)
#ifdef USE_MGL2
		pStart.y = gr->Self()->Min.y;
		pEnd.y = gr->Self()->Max.y;
#else				
		pStart.y = gr->Min.y;
		pEnd.y = gr->Max.y;
#endif
	}


	//now that we have the rectangle defined,
	//Allow for the plot to be zoomed
	//

	float minXZoom,maxXZoom,minYZoom,maxYZoom;

	minXZoom=std::min(pStart.x,pEnd.x);
	maxXZoom=std::max(pStart.x,pEnd.x);

	minYZoom=std::min(pStart.y,pEnd.y);
	maxYZoom=std::max(pStart.y,pEnd.y);


	//Enforce zoom limit to avoid FP aliasing
	if(maxXZoom - minXZoom > MGL_ZOOM_LIMIT &&
	        maxYZoom - minYZoom > MGL_ZOOM_LIMIT)
	{
		thePlot->setBounds(minXZoom,maxXZoom,
		                   minYZoom,maxYZoom);
	}

}

void MathGLPane::rightClick(wxMouseEvent& event) 
{
}

void MathGLPane::mouseLeftWindow(wxMouseEvent& event) 
{
	leftWindow=true;
	Refresh();
}

void MathGLPane::keyPressed(wxKeyEvent& event) 
{
	if(!gr || thePlot->isInteractionLocked() )
		return;
	updateMouseCursor();
}

void MathGLPane::keyReleased(wxKeyEvent& event) 
{
	if(!gr || thePlot->isInteractionLocked() )
		return;
	updateMouseCursor();
}


unsigned int MathGLPane::savePNG(const std::string &filename, 
		unsigned int width, unsigned int height)
{

	if(gr)
		delete gr;

	ASSERT(filename.size());
	try
	{
#ifdef USE_MGL2
		gr = new mglGraph(0, width,height);
#else
		gr = new mglGraphZB(width,height);
#endif
	}
	catch(std::bad_alloc)
	{
		gr=0;
		return MGLPANE_ERR_BADALLOC;
	}
	char *mglWarnMsgBuf=new char[1024];
	*mglWarnMsgBuf=0;
#ifdef USE_MGL2
	gr->Self()->SetWarn(0,mglWarnMsgBuf);
#else
	gr->SetWarn(0);
	gr->Message=mglWarnMsgBuf;
#endif
	thePlot->drawPlot(gr);	

	gr->WritePNG(filename.c_str());

	bool doWarn;
#ifdef USE_MGL2
	doWarn=gr->Self()->GetWarn();
#else
	doWarn=gr->WarnCode;
#endif
	if(doWarn)
	{
		lastMglErr=mglWarnMsgBuf;
		delete[] mglWarnMsgBuf;
		delete gr;
		gr=0;
		return MGLPANE_ERR_MGLWARN;
	}

	delete[] mglWarnMsgBuf;
	delete gr;
	gr=0;
	//Hack. mathgl does not return an error value from its writer
	//function :(. Check to see that the file is openable, and nonzero sized
	
	ifstream f(filename.c_str(),ios::binary);

	if(!f)
		return MGLPANE_FILE_REOPEN_FAIL;

	f.seekg(0,ios::end);


	if(!f.tellg())
		return MGLPANE_FILE_UNSIZED_FAIL;

	return 0;
}

unsigned int MathGLPane::saveSVG(const std::string &filename)
{
	ASSERT(filename.size());


#ifdef USE_MGL2
	mglGraph *grS;
	grS = new mglGraph();
#else
	mglGraphPS *grS;

	//Width and height are not *really* important per se, 
	//since this is scale-less data.
	grS = new mglGraphPS(1024,768);
#endif

	thePlot->drawPlot(grS);

	char *mglWarnMsgBuf=new char[1024];
	*mglWarnMsgBuf=0;
#ifdef USE_MGL2
	grS->Self()->SetWarn(0,mglWarnMsgBuf);
#else
	grS->SetWarn(0);
	grS->Message=mglWarnMsgBuf;
#endif	
	grS->WriteSVG(filename.c_str());

	bool doWarn;
#ifdef USE_MGL2
	doWarn=grS->Self()->GetWarn();
#else
	doWarn=grS->WarnCode;
#endif

	if(doWarn)
	{
		lastMglErr=mglWarnMsgBuf;
		delete[] mglWarnMsgBuf;
		delete grS;
		grS=0;
		return MGLPANE_ERR_MGLWARN;
	}
	delete grS;

	//Hack. mathgl does not return an error value from its writer
	//function :(. Check to see that the file is openable, and nonzero sized
	
	ifstream f(filename.c_str(),ios::binary);

	if(!f)
		return MGLPANE_FILE_REOPEN_FAIL;

	f.seekg(0,ios::end);


	if(!f.tellg())
		return MGLPANE_FILE_UNSIZED_FAIL;

	return 0;
}

void MathGLPane::setPlotVisible(unsigned int plotId, bool visible)
{
	thePlot->setVisible(plotId,visible);	
}


std::string MathGLPane::getErrString(unsigned int errCode)
{
	switch(errCode)
	{
		case MGLPANE_ERR_BADALLOC:
			return std::string(TRANS("Unable to allocate requested memory.\n Try a lower resolution, or save as vector (SVG)."));
		case MGLPANE_ERR_MGLWARN:
			return std::string(TRANS("Plotting functions returned an error:\n"))+ lastMglErr;
		case MGLPANE_FILE_REOPEN_FAIL:
			return std::string(TRANS("File readback check failed"));
		case MGLPANE_FILE_UNSIZED_FAIL:
			return std::string(TRANS("Filesize during readback appears to be zero."));
		default:
			ASSERT(false);
	}
	ASSERT(false);
}

unsigned int MathGLPane::computeRegionMoveType(float dataX,float dataY,const PlotRegion &r) const
{

	switch(r.bounds.size())
	{
		case 1:
			ASSERT(dataX >= r.bounds[0].first && dataX <=r.bounds[0].second);
			//Can have 3 different aspects. Left, Centre and Right
			return REGION_MOVE_EXTEND_XMINUS+(unsigned int)(3.0f*((dataX-r.bounds[0].first)/
					(r.bounds[0].second- r.bounds[0].first)));
		default:
			ASSERT(false);
	}

	ASSERT(false);	
}

void MathGLPane::drawInteractOverlay(wxDC *dc) const
{

	int w,h;
	w=0;h=0;
	GetClientSize(&w,&h);

	ASSERT(w && h);

	unsigned int regionId,plotId;
	if(getRegionUnderCursor(curMouse,plotId,regionId))
	{
		PlotRegion r;
		thePlot->getRegion(plotId,regionId,r);
		
		wxPen *arrowPen;
		if(limitInteract)
			arrowPen= new wxPen(*wxLIGHT_GREY,2,wxSOLID);
		else
			arrowPen= new wxPen(*wxBLACK,2,wxSOLID);
		dc->SetPen(*arrowPen);
		//Use inverse drawing function so that we don't get 
		//black-on-black type drawing.
		//Other option is to use inverse outlines.
		dc->SetLogicalFunction(wxINVERT);

		const int ARROW_SIZE=8;
		

		//Convert the mouse coordinates to data coordinates.
		mglPoint pMouse= gr->CalcXYZ(curMouse.x,curMouse.y);
		unsigned int regionMoveType=computeRegionMoveType(pMouse.x,pMouse.y,r);

		switch(regionMoveType)
		{
			//Left hand side of region
			case REGION_MOVE_EXTEND_XMINUS:
				dc->DrawLine(curMouse.x-ARROW_SIZE,h/2-ARROW_SIZE,
					     curMouse.x-2*ARROW_SIZE, h/2);
				dc->DrawLine(curMouse.x-2*ARROW_SIZE, h/2,
					     curMouse.x-ARROW_SIZE,h/2+ARROW_SIZE);
				break;
			//right hand side of region
			case REGION_MOVE_EXTEND_XPLUS:
				dc->DrawLine(curMouse.x+ARROW_SIZE,h/2-ARROW_SIZE,
					     curMouse.x+2*ARROW_SIZE, h/2);
				dc->DrawLine(curMouse.x+2*ARROW_SIZE, h/2,
					     curMouse.x+ARROW_SIZE,h/2+ARROW_SIZE);
				break;

			//centre of region
			case REGION_MOVE_TRANSLATE_X:
				dc->DrawLine(curMouse.x-ARROW_SIZE,h/2-ARROW_SIZE,
					     curMouse.x-2*ARROW_SIZE, h/2);
				dc->DrawLine(curMouse.x-2*ARROW_SIZE, h/2,
					     curMouse.x-ARROW_SIZE,h/2+ARROW_SIZE);
				dc->DrawLine(curMouse.x+ARROW_SIZE,h/2-ARROW_SIZE,
					     curMouse.x+2*ARROW_SIZE, h/2);
				dc->DrawLine(curMouse.x+2*ARROW_SIZE, h/2,
					     curMouse.x+ARROW_SIZE,h/2+ARROW_SIZE);
				break;
			default:
				ASSERT(false);

		}

		dc->SetLogicalFunction(wxCOPY);
		delete arrowPen;
	}


}

void MathGLPane::drawRegionDraggingOverlay(wxDC *dc) const
{
	int w,h;
	w=0;h=0;
	GetClientSize(&w,&h);
	ASSERT(w && h);
	//Well, we are dragging the region out some.
	//let us draw a line from the original X position to
	//the current mouse position/nearest region position
	mglPoint mglCurMouse= gr->CalcXYZ(curMouse.x,curMouse.y);

	ASSERT(thePlot->plotType(startMousePlot) == PLOT_TYPE_ONED);
	float regionLimitX,regionLimitY;
	regionLimitX=mglCurMouse.x;
	regionLimitY=mglCurMouse.y;
	//See where extending the region is allowed up to.
	thePlot->moveRegionLimit(startMousePlot,startMouseRegion,
					regionMoveType, regionLimitX,regionLimitY);
	
	int deltaDrag;
	mglPoint testPoint;
	testPoint.x=regionLimitX;
	testPoint.y=0;
	int testX,testY;
#ifdef USE_MGL2
	mglPoint testOut;
	testOut=gr->CalcScr(testPoint);
	testX=testOut.x; testY=testOut.y;
#else

	gr->CalcScr(testPoint,&testX,&testY);
#endif
	deltaDrag = testX-draggingStart.x;

	//Draw some text above the cursor to indicate the current position
	std::string str;
	stream_cast(str,regionLimitX);
	wxString wxs;
	wxs=wxStr(str);
	wxCoord textW,textH;
	dc->GetTextExtent(wxs,&textW,&textH);


	wxPen *arrowPen;
	arrowPen=  new wxPen(*wxBLACK,2,wxSOLID);

	dc->SetPen(*arrowPen);
	const int ARROW_SIZE=8;
	
	dc->SetLogicalFunction(wxINVERT);
	//draw horiz line
	dc->DrawLine(testX,h/2,
		     draggingStart.x,h/2);
	if(deltaDrag > 0)
	{

		dc->DrawText(wxs,testX-textW,h/2-textH*2);
		//Draw arrow head to face right
		dc->DrawLine(testX,h/2,
			     testX-ARROW_SIZE, h/2-ARROW_SIZE);
		dc->DrawLine(testX, h/2,
			     testX-ARROW_SIZE,h/2+ARROW_SIZE);

	}
	else
	{
		dc->DrawText(wxs,testX,h/2-textH*2);
		//Draw arrow head to face left
		dc->DrawLine(testX,h/2,
			     testX+ARROW_SIZE, h/2-ARROW_SIZE);
		dc->DrawLine(testX, h/2,
			     testX+ARROW_SIZE,h/2+ARROW_SIZE);
	}


	switch(regionMoveType)
	{
		case REGION_MOVE_EXTEND_XMINUS:
		case REGION_MOVE_EXTEND_XPLUS:
			//No extra markers; we are cool as is
			break;
		case REGION_MOVE_TRANSLATE_X:
		{
			//This needs to be extended to support more
			//plot types.
			ASSERT(thePlot->plotType(startMousePlot) == PLOT_TYPE_ONED);
			
			//Draw "ghost" limits markers for move,
			//these appear as moving vertical bars to outline
			//where the translation result will be for both
			//upper and lower
			PlotRegion reg;
			thePlot->getRegion(startMousePlot,startMouseRegion,reg);
			mglPoint mglDragStart = gr->CalcXYZ(draggingStart.x,draggingStart.y);

			float newLower,newUpper;
			newLower = reg.bounds[0].first + (mglCurMouse.x-mglDragStart.x);
			newUpper = reg.bounds[0].second + (mglCurMouse.x-mglDragStart.x);

			int newLowerX,newUpperX,dummy;
#ifdef USE_MGL2
			mglPoint tmp;
			tmp=gr->CalcScr(mglPoint(newLower,0.0f));
			newLowerX=tmp.x;
			tmp=gr->CalcScr(mglPoint(newUpper,0.0f));
			newUpperX=tmp.x;
#else
			gr->CalcScr(mglPoint(newLower,0.0f),&newLowerX,&dummy);
			gr->CalcScr(mglPoint(newUpper,0.0f),&newUpperX,&dummy);
#endif

			dc->DrawLine(newLowerX,h/2+2*ARROW_SIZE,newLowerX,h/2-2*ARROW_SIZE);
			dc->DrawLine(newUpperX,h/2+2*ARROW_SIZE,newUpperX,h/2-2*ARROW_SIZE);
			break;
		}
		default:
			ASSERT(false);
			break;
	}	

	dc->SetLogicalFunction(wxCOPY);
	delete arrowPen;

}
