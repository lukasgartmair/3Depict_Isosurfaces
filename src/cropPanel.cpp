/*
 *	wxCropPanel.cpp - cropping window  for user interaction
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


#include "cropPanel.h"

#include <wx/dcbuffer.h>

//DEBUG ONLY
//---
#include <iostream>
using std::cerr;
using std::endl;
//---

//Crop array indices
enum
{
	CROP_LEFT,
	CROP_TOP,
	CROP_RIGHT,
	CROP_BOTTOM,
	CROP_ENUM_END
};


BEGIN_EVENT_TABLE(CropPanel, wxPanel)
	EVT_PAINT(CropPanel::onPaint)
	EVT_MOTION(CropPanel::mouseMove)
	EVT_LEFT_DOWN(CropPanel::mouseDown)
	EVT_LEFT_UP(CropPanel::mouseReleased)
	EVT_LEAVE_WINDOW(CropPanel::mouseLeftWindow)
	EVT_LEFT_DCLICK(CropPanel::mouseDoubleLeftClick) 
	EVT_ERASE_BACKGROUND(CropPanel::OnEraseBackground)
	EVT_SIZE(CropPanel::onResize)
END_EVENT_TABLE()



CropPanel::CropPanel(wxWindow * parent, wxWindowID id,
	const wxPoint & pos,const wxSize & size,long style) 
		: wxPanel(parent, id, pos, size, style)
{
	programmaticEvent=false;
	crop[0]=crop[1]=crop[2]=crop[3]=0.2;
	selMode=SELECT_MODE_NONE;
	dragging=false;
	linkedPanel=0;
	linkMode=CROP_LINK_NONE;
	hasUpdates=false;
}

void CropPanel::OnEraseBackground(wxEraseEvent &event)
{
	//Intentionally do nothing, to suppress background erase
}

void CropPanel::mouseMove(wxMouseEvent &event)
{
	//set the snap position using a bitmask
	int w,h;
	w=0;h=0;

	GetClientSize(&w,&h);

	if(!w || !h)
		return;

	//Do our calculations in reduced coordinates (0->1);
	float xMouse,yMouse,x,y;
	wxPoint mousePos =event.GetPosition();
	//Add a 1px border around control
	xMouse=(float)(mousePos.x+1)/(float)(w-2);
	yMouse=(float)(mousePos.y+1)/(float)(h-2);
	
	if(!dragging)
	{


		float meanPx = 1.0/(1.0/(w-2) + 1.0/(h-2));
		unsigned int minIndex;
		float minDist,tmpDist;

		//work our way clockwise around the corners
		//finding the minimum distance
		for(unsigned int ui=0;ui<4;ui++)
		{
			//Check this corner
			switch(ui)
			{
				case 0:
					//Top left corner
					x=crop[CROP_LEFT];
					y=crop[CROP_TOP];
					break;
				case 1:
					//Top right corner
					x=1.0-crop[CROP_RIGHT];
					y=crop[CROP_TOP];
					break;

				case 2:
					//Bottom right corner
					x=1.0-crop[CROP_RIGHT];
					y=1.0-crop[CROP_BOTTOM];
					break;
				case 3:
					//Bottom left corner
					x=crop[CROP_LEFT];
					y=1.0-crop[CROP_BOTTOM];
					break;
			}

			tmpDist=(xMouse-x)*(xMouse-x) + (yMouse-y)*(yMouse-y);
			if(!ui || tmpDist < minDist) //first pass - no data
			{
				minIndex=ui;
				minDist=tmpDist;
			}
		}

		minDist=sqrtf(minDist);
		bool haveCorner;

		const float MIN_CUTOFF_DISTANCE= 3;

		//Do we have a corner minimum?
		haveCorner= ((int)(minDist*meanPx) < MIN_CUTOFF_DISTANCE);

		bool haveCentre;
		float meanX = (float)(crop[0] + (1.0-crop[2]))*0.5;
		float meanY = (float)(crop[1] + (1.0-crop[3]))*0.5;
		
		float centreDist;
		centreDist=sqrtf((xMouse-meanX)*(xMouse-meanX)
				+ (yMouse-meanY)*(yMouse-meanY));
		//Check the centre, which is allowed to trump the corners
		if(haveCorner)
			haveCentre=(centreDist< minDist);
		else
			haveCentre=(meanPx*centreDist) < MIN_CUTOFF_DISTANCE;

		

		unsigned int sideIndex;

		bool haveSide=false;
		//OK, well, we are allowed to have a side match, check that.
		if(fabs(crop[CROP_LEFT] - xMouse)*meanPx < MIN_CUTOFF_DISTANCE)
		{
			haveSide=true;
			sideIndex=CROP_LEFT;
		}
		//OK, well, we are allowed to have a side match, check that.
		else if(fabs((1.0-crop[CROP_RIGHT]) - xMouse)*meanPx < MIN_CUTOFF_DISTANCE)
		{
			haveSide=true;
			sideIndex=CROP_RIGHT;
		}
		else if(fabs(crop[CROP_TOP] - yMouse)*meanPx < MIN_CUTOFF_DISTANCE)
		{
			haveSide=true;
			sideIndex=CROP_TOP;
		}
		else if(fabs((1.0- crop[CROP_BOTTOM]) - yMouse)*meanPx < MIN_CUTOFF_DISTANCE)
		{
			haveSide=true;
			sideIndex=CROP_BOTTOM;
		}


		//!Prioritise selection mode
		if(haveCentre)
		{
			selMode=SELECT_MODE_CENTRE;
		}
		else if(haveCorner)
		{
			selMode=SELECT_MODE_CORNER;
			selIndex=minIndex;
		}
		else if(haveSide)
		{
			selMode=SELECT_MODE_SIDE;
			selIndex=sideIndex;
		}
		else
		{
			selMode=SELECT_MODE_NONE;
		}
	}
	else
	{
		float origCrop[4];
		for(unsigned int ui=0;ui<4;ui++)
			origCrop[ui]=crop[ui];
		switch(selMode)
		{
			case SELECT_MODE_NONE:
				ASSERT(false); // Can't be dragging nothing, can we.
				break;
			case SELECT_MODE_SIDE: 
			{
				// we are dragging one of the side crop walls
				switch(selIndex)
				{
					case 0:
						crop[selIndex ] =xMouse;
						break;
					case 1:
						crop[selIndex ] = yMouse;
						break;
					case 2:
						crop[selIndex]=1.0-xMouse;
						break;
					case 3:
						crop[selIndex]=1.0-yMouse;
						break;
				}

				break;
			}
			case SELECT_MODE_CORNER:
			{
				//we are dragging one of the corners
				switch(selIndex)
				{
					case 0:
						crop[0] =xMouse;
						crop[1]=yMouse;
						break;
					case 1:
						crop[1]=yMouse;
						crop[2] =1.0-xMouse;
						break;
					case 2:
						crop[2] =1.0-xMouse;
						crop[3]=1.0-yMouse;
						break;
					case 3:
						crop[3] =1.0-yMouse;
						crop[0]=xMouse;
						break;
				}
				break;
			}
			case SELECT_MODE_CENTRE:	
			{
				//OK, we have to move them based upon the original drag
				//coordinates
				float delta[2];
				delta[0]=xMouse-mouseAtDragStart[0];
				delta[1]=yMouse-mouseAtDragStart[1];
				for(unsigned int ui=0;ui<4;ui++)
				{
					float flip;
						
					if(ui<2)	
						flip=1.0;
					else
						flip=-1;
					crop[ui]=cropAtDragStart[ui]+delta[ui&1]*flip;
				}
			
				break;	

			}
		}


		//Check the result is still valid
		if(!validCoords())
		{
			//Try to only adjust the invalid coordinates,
			//to make the motion a little "smoother"
			for(unsigned int ui=0;ui<4;ui++)
			{
				if(crop[ui] > 1.0 || crop[ui] < 0.0)
					crop[ui]=origCrop[ui];
			}

			//See if our quick fix solved the coord validity
			if(!validCoords())
			{
				//restore the original coords
				for(unsigned int ui=0;ui<4;ui++)
					crop[ui]=origCrop[ui];
			}
		}

		if(linkedPanel)
			updateLinked();

		hasUpdates=true;
	}


	Refresh();
}

void CropPanel::mouseDoubleLeftClick(wxMouseEvent& event)
{
	for(unsigned int ui=0;ui<4;ui++)
		crop[ui]=0;

	Refresh();
	if(linkedPanel)
		updateLinked();

	hasUpdates=true;
	event.Skip();
}


void CropPanel::mouseLeftWindow(wxMouseEvent& event) 
{
}

void CropPanel::mouseDown(wxMouseEvent &event)
{
	//set the snap position using a bitmask
	int w,h;
	w=0;h=0;

	GetClientSize(&w,&h);

	if(!w || !h)
		return;


	//Do our calculations in reduced coordinates (0->1);
	wxPoint mousePos =event.GetPosition();
	mouseAtDragStart[0]=(float)(mousePos.x+1)/(float)(w-2);
	mouseAtDragStart[1]=(float)(mousePos.y+1)/(float)(h-2);
	
	ASSERT(validCoords());
	if(selMode != SELECT_MODE_NONE)
		dragging=true;

	for(unsigned int ui=0;ui<4;ui++)
		cropAtDragStart[ui] = crop[ui];

}


void CropPanel::mouseReleased(wxMouseEvent &event)
{
	dragging=false;
	selMode=SELECT_MODE_NONE;
	selIndex=0;

	Refresh();
}

bool CropPanel::validCoords() const
{
	float sum;
	sum=(crop[CROP_LEFT] + crop[CROP_RIGHT]);
	//Draw the four crop markers
	if(sum > 1.00f)
		return false;
	
	
	sum=(crop[CROP_TOP] + crop[CROP_BOTTOM]);
	if( sum> 1.00f)
		return false;


	for(unsigned int ui=0;ui<4;ui++)
	{
		if(crop[ui] < 0.0)
			return false;
	}

	return true;
}

void CropPanel::onPaint(wxPaintEvent &event)
{
	draw();
}

void CropPanel::draw()
{
	ASSERT(validCoords());

	wxAutoBufferedPaintDC   *dc=new wxAutoBufferedPaintDC(this);
	dc->SetBackground(*wxWHITE_BRUSH);
	dc->Clear();

	int w,h;
	w=0;h=0;

	GetClientSize(&w,&h);
	if(!w || !h)
	{
		delete dc;
		return;
	}


	//Drawing coords
	int lineX[8],lineY[8];

	//Draw lines
	lineX[0]=lineX[1]=(int)(crop[CROP_LEFT]*(float)w);
	lineX[2]=0;
	lineX[3]=w;
	lineX[4]=lineX[5]=(int)((1.0-crop[CROP_RIGHT])*(float)w);
	lineX[6]=w;
	lineX[7]=0;

	lineY[0]=0;
	lineY[1]=h;
	lineY[2]=lineY[3]=(int)(crop[CROP_TOP]*(float)h);
	lineY[4]=0;
	lineY[5]=h;
	lineY[6]=lineY[7]=(int)((1.0-crop[CROP_BOTTOM])*(float)h);

		

	//Draw greyed out section
	//--
	wxPen *noPen;
	noPen = new wxPen(*wxBLACK,1,wxTRANSPARENT);
	dc->SetBrush(*wxLIGHT_GREY);
	dc->SetPen(*noPen);

	dc->DrawRectangle(0,0,lineX[0],h);
	dc->DrawRectangle(0,0,w,lineY[2]);
	dc->DrawRectangle(0,lineY[6],w,h-lineY[6]);
	dc->DrawRectangle(lineX[4],0,w-lineX[4],h);
	delete noPen;
	//--
	
	
	wxPen *highPen,*normalPen;
	highPen=  new wxPen(*wxBLUE,2,wxSOLID);
	normalPen= new wxPen(*wxBLACK,2,wxSOLID);

	dc->SetPen(*normalPen);
	if(selMode!=SELECT_MODE_SIDE)	
	{

		dc->SetPen(*normalPen);
		for(unsigned int ui=0;ui<8;ui+=2)
			dc->DrawLine(lineX[ui],lineY[ui],lineX[ui+1],lineY[ui+1]);
	
	}
	else
	{
		for(unsigned int ui=0;ui<8;ui+=2)
		{
			if(selIndex== ui/2)
				dc->SetPen(*highPen);
			else
				dc->SetPen(*normalPen);
			
			dc->DrawLine(lineX[ui],lineY[ui],lineX[ui+1],lineY[ui+1]);
		}

		dc->SetPen(*normalPen);
	}


	if(selMode == SELECT_MODE_CORNER)
	{
		//Draw the corner markers
		float xC,yC;
		float sizeX,sizeY;

		sizeX=sizeY=8;
		switch(selIndex)
		{
			case 0:
				xC=crop[CROP_LEFT];
				yC=crop[CROP_TOP];
				sizeX=-sizeX;
				sizeY=-sizeY;
				break;
			case 1:
				xC=1.0-crop[CROP_RIGHT];
				yC=crop[CROP_TOP];
				sizeY=-sizeY;
				break;
			case 2:
				xC=1.0-crop[CROP_RIGHT];
				yC=1.0-crop[CROP_BOTTOM];
				break;
			case 3:
				sizeX=-sizeX;
				xC=crop[CROP_LEFT];
				yC=1.0-crop[CROP_BOTTOM];
				break;
		}

		xC=xC*(float)w;
		yC=yC*(float)h;


		//Draw the corner
		dc->SetPen(*highPen);
		dc->DrawLine(xC + 2.0*sizeX, yC+sizeY, xC+sizeX,yC+sizeY);
		dc->DrawLine(xC+sizeX, yC+sizeY, xC+sizeX,yC+2.0*sizeY);
		dc->SetPen(*normalPen);
	}	

	

	float meanX = (float)w*(crop[0] + (1.0-crop[2]))*0.5;
	float meanY = (float)h*(crop[1] + (1.0-crop[3]))*0.5;

	dc->DrawCircle(meanX,meanY,1);
	if(selMode==SELECT_MODE_CENTRE)
	{
		dc->SetPen(*highPen);
		dc->DrawCircle(meanX,meanY,4);
	}

	delete dc;

}


void CropPanel::updateLinked()
{
	ASSERT(linkedPanel);
	switch(linkMode)
	{
		case CROP_LINK_NONE:
			return;
		case CROP_LINK_LR:
			linkedPanel->crop[CROP_LEFT]=crop[CROP_LEFT];
			linkedPanel->crop[CROP_RIGHT]=crop[CROP_RIGHT];
			break;
		case CROP_LINK_LR_FLIP:
			linkedPanel->crop[CROP_BOTTOM]=crop[CROP_LEFT];
			linkedPanel->crop[CROP_TOP]=crop[CROP_RIGHT];
			break;
		case CROP_LINK_TB:
			linkedPanel->crop[CROP_BOTTOM]=crop[CROP_BOTTOM];
			linkedPanel->crop[CROP_TOP]=crop[CROP_TOP];
			break;
		case CROP_LINK_TB_FLIP:
			linkedPanel->crop[CROP_LEFT]=crop[CROP_BOTTOM];
			linkedPanel->crop[CROP_RIGHT]=crop[CROP_TOP];
			break;
		case CROP_LINK_BOTH:
			linkedPanel->crop[CROP_LEFT]=crop[CROP_LEFT];
			linkedPanel->crop[CROP_RIGHT]=crop[CROP_RIGHT];
			linkedPanel->crop[CROP_BOTTOM]=crop[CROP_BOTTOM];
			linkedPanel->crop[CROP_TOP]=crop[CROP_TOP];
			break;
		case CROP_LINK_BOTH_FLIP:
			linkedPanel->crop[CROP_BOTTOM]=crop[CROP_LEFT];
			linkedPanel->crop[CROP_TOP]=crop[CROP_RIGHT];
			linkedPanel->crop[CROP_LEFT]=crop[CROP_BOTTOM];
			linkedPanel->crop[CROP_RIGHT]=crop[CROP_TOP];
			break;
		default:
			ASSERT(false);

	}

	linkedPanel->Refresh();
	
}


void CropPanel::link(CropPanel *panel,unsigned int mode)
{
	linkMode=mode;
	if(linkMode== CROP_LINK_NONE)
		linkedPanel=0;
	else
	{
		linkedPanel=panel;

		for(unsigned int ui=0;ui<CROP_ENUM_END; ui++)
			linkedPanel->crop[ui]=crop[ui];
	}
}

void CropPanel::getCropValues(float *array) const
{
	array[0]=crop[CROP_LEFT];
	array[1]=crop[CROP_RIGHT];
	array[2]=crop[CROP_TOP];
	array[3]=crop[CROP_BOTTOM];

}

void CropPanel::setCropValue(unsigned int index, float v) 

{
	ASSERT(index<=CROP_BOTTOM);
	crop[index]=v;
}

void CropPanel::onResize(wxSizeEvent &evt)
{
	draw();
}