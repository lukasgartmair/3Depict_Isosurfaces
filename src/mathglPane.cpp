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
#include <vector>

//Uncomment me if using MGL >=1_10 to 
//enable nice drawing of rectangular bars
#define MGL_GTE_1_10

//Panning speed modifier
const float MGL_PAN_SPEED=0.8f;

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
	limitInteract=haveUpdates=false;
	regionDragging=panning=dragging=false;
	leftWindow=true;
	thePlot=0;	
	gr=0;
	SetBackgroundStyle(wxBG_STYLE_CUSTOM);
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
	wxAutoBufferedPaintDC   *dc=new wxAutoBufferedPaintDC(this);

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
		
		
		wxString str=_("No plots selected.");
		dc->GetMultiLineTextExtent(str,&w,&h);
		dc->DrawText(str,(clientW-w)/2, (clientH-h)/2);

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
	if(!gr || hasChanged || hasResized || panning)
	{

		//clear the plot drawing entity
		if(gr)
			delete gr;

		gr = new mglGraphZB(w,h);

		//change the plot by panning it before we draw.
		//if we need to 
		if(panning)
		{

			//Panning and dragging are two different operations.
			ASSERT(!dragging); 
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
	//Compute the MGL coords
	int axisX,axisY;
	axisX=axisY=0;
	gr->CalcScr(gr->Org,&axisX,&axisY);
	axisY=h-axisY;
	if(dragging)
	{
		//Panning and dragging are two different operations.
		ASSERT(!panning); 
		
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
	else if(regionDragging)
	{
		//Well, we are dragging the region out some.
		//let us draw a line from the original X position to
		//the current mouse position/nearest region position

		mglPoint mglCurMouse= gr->CalcXYZ(curMouse.x,curMouse.y);

		//See where extending the region is allowed up to.
		float regionTest=thePlot->moveRegionTest(startMouseRegion,
						regionMoveType, mglCurMouse.x);
		
		int deltaDrag;
		mglPoint testPoint;
		testPoint.x=regionTest;
		testPoint.y=0;
		int testX,testY;
		gr->CalcScr(testPoint,&testX,&testY);
		deltaDrag = testX-draggingStart.x;

		//Draw some text above the cursor to indicate the current position
		std::string str;
		stream_cast(str,regionTest);
		wxString wxs;
		wxs=wxStr(str);
		wxCoord textW,textH;
		dc->GetTextExtent(wxs,&textW,&textH);


		wxPen *arrowPen;
		arrowPen=  new wxPen(*wxBLACK,2,wxSOLID);

		dc->SetPen(*arrowPen);
		const int ARROW_SIZE=8;
		
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

	
		//Draw "ghost" limits markers for move
		if(regionMoveType == REGION_MOVE)
		{
			PlotRegion reg=thePlot->getRegion(startMouseRegion);
		
			mglPoint mglDragStart = gr->CalcXYZ(draggingStart.x,draggingStart.y);

			float newLower,newUpper;

			newLower = reg.bounds.first + (mglCurMouse.x-mglDragStart.x);
			newUpper = reg.bounds.second + (mglCurMouse.x-mglDragStart.x);

			int newLowerX,newUpperX,dummy;
			gr->CalcScr(mglPoint(newLower,0.0f),&newLowerX,&dummy);
			gr->CalcScr(mglPoint(newUpper,0.0f),&newUpperX,&dummy);

			dc->DrawLine(newLowerX,h/2+2*ARROW_SIZE,newLowerX,h/2-2*ARROW_SIZE);
			dc->DrawLine(newUpperX,h/2+2*ARROW_SIZE,newUpperX,h/2-2*ARROW_SIZE);


		}	

		delete arrowPen;
				

	}
	else if(!leftWindow)
	{
		if(curMouse.y < axisY && curMouse.x > axisX ) //y axis inverted
		{
			unsigned int region,regionSide;
			if((region=getRegionUnderCursor(curMouse,regionSide)) != 
					std::numeric_limits<unsigned int>::max())
			{
				PlotRegion r;
				r=thePlot->getRegion(region);
				
				wxPen *arrowPen;
				if(limitInteract)
					arrowPen= new wxPen(*wxLIGHT_GREY,2,wxSOLID);
				else
					arrowPen= new wxPen(*wxBLACK,2,wxSOLID);
				dc->SetPen(*arrowPen);

				const int ARROW_SIZE=8;
					
				switch(regionSide)
				{
					//Left hand side of region
					case REGION_LEFT_EXTEND:
						dc->DrawLine(curMouse.x-ARROW_SIZE,h/2-ARROW_SIZE,
							     curMouse.x-2*ARROW_SIZE, h/2);
						dc->DrawLine(curMouse.x-2*ARROW_SIZE, h/2,
							     curMouse.x-ARROW_SIZE,h/2+ARROW_SIZE);
						break;
						//right hand side of region
					case REGION_RIGHT_EXTEND:
						dc->DrawLine(curMouse.x+ARROW_SIZE,h/2-ARROW_SIZE,
							     curMouse.x+2*ARROW_SIZE, h/2);
						dc->DrawLine(curMouse.x+2*ARROW_SIZE, h/2,
							     curMouse.x+ARROW_SIZE,h/2+ARROW_SIZE);
						break;

						//centre of region
					case REGION_MOVE:
						dc->DrawLine(curMouse.x-ARROW_SIZE,h/2-ARROW_SIZE,
							     curMouse.x-2*ARROW_SIZE, h/2);
						dc->DrawLine(curMouse.x-2*ARROW_SIZE, h/2,
							     curMouse.x-ARROW_SIZE,h/2+ARROW_SIZE);
						dc->DrawLine(curMouse.x+ARROW_SIZE,h/2-ARROW_SIZE,
							     curMouse.x+2*ARROW_SIZE, h/2);
						dc->DrawLine(curMouse.x+2*ARROW_SIZE, h/2,
							     curMouse.x+ARROW_SIZE,h/2+ARROW_SIZE);
						break;

				}

				delete arrowPen;
			}

		}
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
	leftWindow=false;
	if(!thePlot)
		return;

	if(limitInteract)
		regionDragging=false;


	if(dragging)
	{
		if(!event.m_leftDown)
			dragging=false;
		else
			draggingCurrent=event.GetPosition();


	}
	else if(panning)
	{
		if(!event.m_leftDown || !event.m_shiftDown)
			panning=false;
		else
			draggingCurrent=event.GetPosition();
	}

	if(!gr)
		return;

	curMouse=event.GetPosition();	
	updateMouseCursor();

	Refresh();

}

void MathGLPane::updateMouseCursor()
{
	int w,h;
	w=0;h=0;

	GetClientSize(&w,&h);
	
	if(!w || !h || !thePlot)
		return;
	
	//Update mouse cursor
	//---------------
	//Draw a rectangle between the start and end positions

	//Compute the MGL coords
	int axisX,axisY;
	axisX=axisY=0;
	gr->CalcScr(gr->Org,&axisX,&axisY);
	axisY=h-axisY;
	//If the cursor is wholly beloe
	//the axis, draw a line rather than abox

	//Set cursor to normal by default
	SetCursor(wxNullCursor);

	if(wxGetKeyState(WXK_SHIFT))
	{
		SetCursor(wxCURSOR_SIZEWE);
	}
	else
	{
		//Look at mouse position relative to the axis position
		//to determine the cursor style.
		if(curMouse.x < axisX)
		{
			if(curMouse.y < axisY)
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
}

void MathGLPane::mouseDoubleLeftClick(wxMouseEvent& event)
{
	if(!thePlot)
		return;

	panning=dragging=false;

	int w,h;
	w=0;h=0;
	GetClientSize(&w,&h);
	
	if(!w || !h )
		return;
	
	//Compute the coords for the axis position
	int axisX,axisY;
	axisX=axisY=0;
	gr->CalcScr(gr->Org,&axisX,&axisY);
	axisY=h-axisY;
	//Look at mouse position relative to the axis position
	if(curMouse.x < axisX)
	{
		if(curMouse.y < axisY)
		{	
			//left of X-Axis -- plot Y zoom
			thePlot->disableUserAxisBounds(false);	
		}
		else
		{
			//bottom corner
			thePlot->disableUserBounds();	
		}
	}
	else if(curMouse.y > axisY )
	{
		//Below Y axis; plot X Zoom

		thePlot->disableUserAxisBounds(true);	
	}
	else
	{
		//reset plot bounds
		thePlot->disableUserBounds();	
	}

	Refresh();
}


unsigned int MathGLPane::getRegionUnderCursor(const wxPoint  &mousePos,
						unsigned int &rangeSide) const
{
	ASSERT(gr);

	//pronounced "christmouse" (no, not really)
	float xMouse;


	mglPoint pMouse= gr->CalcXYZ(mousePos.x,mousePos.y);
	xMouse=pMouse.x;

	std::vector<PlotRegion> regions;
	thePlot->getRegions(regions);

	for(size_t ui=0;ui<regions.size();ui++)
	{
		if(regions[ui].bounds.first < xMouse &&
				regions[ui].bounds.second > xMouse)
		{
			//we are in this region

			//OK, so we are in the region, but wichich "third"
			//are we in? (This MUST match REGION_* enum)
			rangeSide= (unsigned int)(3.0f*((xMouse-regions[ui].bounds.first)/
				(regions[ui].bounds.second- regions[ui].bounds.first))) + 1;

			return regions[ui].uniqueID;
		}
	}


	//No range...
	return 	std::numeric_limits<unsigned int>::max();
}


void MathGLPane::mouseDown(wxMouseEvent& event)
{
	if(gr)
	{
		int w,h;
		w=0;h=0;

		GetClientSize(&w,&h);
		
		if(!w || !h)
			return;

		int axisX,axisY;
		axisX=axisY=0;
		gr->CalcScr(gr->Org,&axisX,&axisY);
		axisY=h-axisY;

		bool alternateDown;

		alternateDown=event.ShiftDown();
		draggingStart = event.GetPosition();

		//Set the interaction mode
		if(event.LeftDown() && !alternateDown )
		{
			//check to see if we have hit a region
			unsigned int region,regionSide;
			if(!limitInteract && (region=getRegionUnderCursor(curMouse,regionSide)) != 
					std::numeric_limits<unsigned int>::max() && 
					axisY > draggingStart.y)
			{
				PlotRegion r;
				startMouseRegion=region;

				regionMoveType=regionSide;
				regionDragging=true;
			}
			else
				dragging=true;
		}
		else if(event.LeftDown() && alternateDown)
		{
			panning=true;

			float xMin,xMax,yMin,yMax;
			thePlot->getBounds(xMin,xMax,yMin,yMax);
			origPanMinX=xMin;
			origPanMaxX=xMax;

		}
		
		SetFocus(); //Why am I calling setFocus?? can't remeber.. 
	}

	event.Skip();
}

void MathGLPane::mouseWheelMoved(wxMouseEvent& event)
{
}

void MathGLPane::mouseReleased(wxMouseEvent& event)
{

	if(!thePlot || !gr)
		return;

	if(dragging)
	{
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
	else if(regionDragging)
	{

		if(!limitInteract)
		{
			//we need to tell viscontrol that we have done a region
			//update
			mglPoint mglCurMouse= gr->CalcXYZ(curMouse.x,curMouse.y);
			
			thePlot->moveRegion(startMouseRegion,regionMoveType,mglCurMouse.x);	
			haveUpdates=true;	


			regionDragging=false;
		}
		Refresh();
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
	if(gr)
		updateMouseCursor();
}


void MathGLPane::keyReleased(wxKeyEvent& event) 
{
	if(gr)
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
		gr = new mglGraphZB(width,height);
	}
	catch(std::bad_alloc)
	{
		gr=0;
		return MGLPANE_ERR_BADALLOC;
	}
	char *mglWarnMsgBuf=new char[1024];
	gr->SetWarn(0);
	gr->Message=mglWarnMsgBuf;
	thePlot->drawPlot(gr);	

	gr->WritePNG(filename.c_str());
	if(gr->WarnCode)
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


	mglGraphPS *grS;

	//Width and height are not *really* important per se, 
	//since this is scale-less data.
	grS = new mglGraphPS(1024,768);


	thePlot->drawPlot(grS);
	
	
	grS->SetWarn(0);
	grS->WriteSVG(filename.c_str());

	if(grS->WarnCode)
	{
		delete grS;
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
			return std::string("Unable to allocate requested memory.\n Try a lower resolution, or save as vector (SVG).");
		case MGLPANE_ERR_MGLWARN:
			return std::string("Plotting functions returned an error:\n")+ lastMglErr;
		case MGLPANE_FILE_REOPEN_FAIL:
			return std::string("File readback check failed");
		case MGLPANE_FILE_UNSIZED_FAIL:
			return std::string("Filesize during readback appears to be zero.");
		default:
			ASSERT(false);
	}
}
