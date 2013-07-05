/*
 *	glPane.cpp - OpenGL panel implementation
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


#include <wx/progdlg.h>

#include "wxcommon.h"

#include "common/stringFuncs.h"
#include "gl/select.h"
#include "glPane.h"


// include OpenGL
#ifdef __WXMAC__
#include "OpenGL/glu.h"
#include "OpenGL/gl.h"

#else
#include <GL/glu.h>
#include <GL/gl.h>

#endif

#include "common/translation.h"

//Unclear why, but windows does not allow GL_BGR, 
//needs some other define BGR_EXT (NFI), which is 0x80E0
#if defined(WIN32) || defined(WIN64)
    #define GL_BGR 0x80E0
#endif


enum
{
	ID_KEYPRESS_TIMER=wxID_ANY+1,
};

//Double tap delay (ms), for axis reversal
const unsigned int DOUBLE_TAP_DELAY=500; 

BEGIN_EVENT_TABLE(BasicGLPane, wxGLCanvas)
EVT_MOTION(BasicGLPane::mouseMoved)
EVT_ERASE_BACKGROUND(BasicGLPane::OnEraseBackground)
EVT_LEFT_DOWN(BasicGLPane::mouseDown)
EVT_LEFT_UP(BasicGLPane::mouseReleased)
EVT_MIDDLE_UP(BasicGLPane::mouseReleased)
EVT_MIDDLE_DOWN(BasicGLPane::mouseDown)
EVT_RIGHT_UP(BasicGLPane::mouseReleased)
EVT_RIGHT_DOWN(BasicGLPane::mouseDown)
EVT_LEAVE_WINDOW(BasicGLPane::mouseLeftWindow)
EVT_SIZE(BasicGLPane::resized)
EVT_KEY_DOWN(BasicGLPane::keyPressed)
EVT_CHAR(BasicGLPane::charEvent) 
EVT_KEY_UP(BasicGLPane::keyReleased)
EVT_MOUSEWHEEL(BasicGLPane::mouseWheelMoved)
EVT_PAINT(BasicGLPane::render)
EVT_TIMER(ID_KEYPRESS_TIMER,BasicGLPane::OnAxisTapTimer)
END_EVENT_TABLE()

//Controls camera pan/translate/pivot speed; Radii per pixel or distance/pixel
const float CAMERA_MOVE_RATE=0.05;

// Controls zoom speed, in err, zoom units.. Ahem. 
const float CAMERA_SCROLL_RATE=0.05;
//Zoom speed for keyboard
const float CAMERA_KEYBOARD_SCROLL_RATE=1;

int attribList[] = {WX_GL_RGBA,
			WX_GL_DEPTH_SIZE,
			16,
			WX_GL_DOUBLEBUFFER,
			1,
			0,0};

BasicGLPane::BasicGLPane(wxWindow* parent) :
wxGLCanvas(parent, wxID_ANY,  wxDefaultPosition, wxDefaultSize, 0, wxT("GLCanvas"),attribList)
{
	haveCameraUpdates=false;
	applyingDevice=false;
	paneInitialised=false;

	keyDoubleTapTimer=new wxTimer(this,ID_KEYPRESS_TIMER);
	lastKeyDoubleTap=(unsigned int)-1;

	mouseMoveFactor=mouseZoomFactor=1.0f;	
	dragging=false;
	lastMoveShiftDown=false;
	selectionMode=false;
	lastKeyFlags=lastMouseFlags=0;
}

BasicGLPane::~BasicGLPane()
{
	keyDoubleTapTimer->Stop();
	delete keyDoubleTapTimer;
}

bool BasicGLPane::displaySupported() const
{
#if wxCHECK_VERSION(2,9,0)
	return IsDisplaySupported(attribList);
#else
	ASSERT(false);
	//Lets hope so. If its not, then its just going to fail anyway. 
	//If it is, then returning false would simply create a roadblock.
	//Either way, you shouldn't get here.
	return true; 
#endif
}

void BasicGLPane::setSceneInteractionAllowed(bool enabled)
{
	currentScene.lockInteraction(!enabled);
}

unsigned int  BasicGLPane::selectionTest(const wxPoint &p,bool &shouldRedraw)
{

	if(currentScene.isInteractionLocked())
	{
		shouldRedraw=false;
		return -1; 
	}

	//TODO: Refactor. Much of this could be pushed into the scene, 
	//and hence out of this wx panel.
	
	//Push on the matrix stack
	glPushMatrix();
	
	GLint oldViewport[4];
	glGetIntegerv(GL_VIEWPORT, oldViewport);
	//5x5px picking region. Picking is done by modifying the view
	//to enlarge the selected region.
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPickMatrix(p.x, oldViewport[3]-p.y,5, 5, oldViewport);
	glMatrixMode(GL_MODELVIEW);

	int lastSelected = currentScene.getLastSelected();
	int selectedObject=currentScene.glSelect();

	//If the object selection hasn't changed, we don't need to redraw
	//if it has changed, we should redraw
	shouldRedraw = (lastSelected !=selectedObject);

	//Restore the previous matirx
	glPopMatrix();

	//Restore the viewport
	int w, h;
	GetClientSize(&w, &h);
	glViewport(0, 0, (GLint) w, (GLint) h);

	return selectedObject;
}
 
unsigned int  BasicGLPane::hoverTest(const wxPoint &p,bool &shouldRedraw)
{

	if(currentScene.isInteractionLocked())
	{
		shouldRedraw=false;
		return -1;
	}
	//Push on the matrix stack
	glPushMatrix();
	
	GLint oldViewport[4];
	glGetIntegerv(GL_VIEWPORT, oldViewport);
	//5x5px picking region. Picking is done by modifying the view
	//to enlarge the selected region.
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPickMatrix(p.x, oldViewport[3]-p.y,5, 5, oldViewport);
	glMatrixMode(GL_MODELVIEW);

	unsigned int lastHover = currentScene.getLastHover();
	unsigned int hoverObject=currentScene.glSelect(false);

	//FIXME: Should be able to make this more efficient	
	shouldRedraw =  lastHover!=(unsigned int)-1;

	//Set the scene's hover value
	currentScene.setLastHover(hoverObject);
	currentScene.setHoverMode(hoverObject != (unsigned int)-1);

	//Restore the previous matirx
	glPopMatrix();

	//Restore the viewport
	int w, h;
	GetClientSize(&w, &h);
	glViewport(0, 0, (GLint) w, (GLint) h);

	return hoverObject;
}

// some useful events to use
void BasicGLPane::mouseMoved(wxMouseEvent& event) 
{
	if (applyingDevice) return;
	enum
	{
		CAM_MOVE, //Movement of some kind
		CAM_TRANSLATE, //translate camera 
		CAM_PIVOT, //Pivot around view and across directions
		CAM_ROLL //Roll around view direction
	};


	if(selectionMode )
	{
		if(currentScene.isInteractionLocked())
		{
			event.Skip();
			return;
		}


		wxPoint p=event.GetPosition();
		
		unsigned int mouseFlags=0;
		unsigned int keyFlags=0;
		wxMouseState wxm = wxGetMouseState();
	
		if(wxm.CmdDown())
			keyFlags|=FLAG_CMD;
		if(wxm.ShiftDown())
			keyFlags|=FLAG_SHIFT;

		if(wxm.LeftDown())
		       	mouseFlags|= SELECT_BUTTON_LEFT;
		if(wxm.RightDown())
		       	mouseFlags|= SELECT_BUTTON_RIGHT;
		if(wxm.MiddleDown())
		       	mouseFlags|= SELECT_BUTTON_MIDDLE;

		//We can get  a mouse move event which reports no buttons before a mouse-up event,
		//this occurs frequently under windows, but sometimes under GTK
		if(!mouseFlags)
		{
			event.Skip();
			return;
		}
				
		int w, h;
		GetClientSize(&w, &h);


		currentScene.applyDevice((float)draggingStart.x/(float)w,
					(float)draggingStart.y/(float)h,
					 p.x/(float)w,p.y/(float)h,
					 keyFlags,mouseFlags,
					 false);

		lastMouseFlags=mouseFlags;
		lastKeyFlags=keyFlags;
		Refresh();
		return;
	}

	if(!dragging)
	{
		wxPoint p=event.GetPosition();

		//Do a hover test
		bool doRedraw=false;
		hoverTest(p,doRedraw);

		if(doRedraw)
			Refresh();

		return;
	}

	wxPoint draggingCurrent = event.GetPosition();

	//left-right and up-down move values
	float lrMove,udMove;	

	//Movement rate multiplier -- initialise to user value
	float camMultRate=mouseMoveFactor;
	if(event.m_shiftDown)
	{
		//Commit the current temp cam using the last camera rate
		//and then restart the motion.
		if(!lastMoveShiftDown && currentScene.haveTempCam())
			currentScene.commitTempCam();

		camMultRate*=5.0f;

		lastMoveShiftDown=true;

	}
	else
	{
		//Commit the current temp cam using the last camera rate
		//and then restart the motion.
 		if(lastMoveShiftDown && currentScene.haveTempCam())
			currentScene.commitTempCam();

		lastMoveShiftDown=false;
	}

	lrMove=CAMERA_MOVE_RATE*camMultRate*(draggingCurrent.x - draggingStart.x);
	udMove=CAMERA_MOVE_RATE*camMultRate*(draggingCurrent.y - draggingStart.y);

	lrMove*=2.0f*M_PI/180.0;
	udMove*=2.0f*M_PI/180.0;
	unsigned int camMode=0;
	//Decide camera movement mode
	bool translateMode;

    translateMode=event.CmdDown();

	bool swingMode;
	#if defined(WIN32) || defined(WIN64) || defined(__APPLE__)
		swingMode=wxGetKeyState(WXK_ALT);
	#else
		swingMode=wxGetKeyState(WXK_TAB);
	#endif

	if(translateMode && !swingMode)
		camMode=CAM_TRANSLATE;
	else if(swingMode && !translateMode)
		camMode=CAM_PIVOT;
	else if(swingMode && translateMode)
		camMode=CAM_ROLL;
	else
		camMode=CAM_MOVE;
	
	switch(camMode)
	{
		case CAM_TRANSLATE:
			currentScene.discardTempCam();
			currentScene.setTempCam();
			currentScene.getTempCam()->translate(lrMove,-udMove);
			break;
		case CAM_PIVOT:
			currentScene.discardTempCam();
			currentScene.setTempCam();
			currentScene.getTempCam()->pivot(lrMove,udMove);
			break;
		case CAM_MOVE:
			currentScene.setTempCam();
			currentScene.getTempCam()->move(lrMove,udMove);
			break;
		case CAM_ROLL:
			currentScene.setTempCam();
			currentScene.getTempCam()->roll(atan2(udMove,lrMove));
						
			break;	
		default:
			ASSERT(false);
			break;
	}

	if(!event.m_leftDown)
	{
		dragging=false;
		currentScene.commitTempCam();
	}
	
	haveCameraUpdates=true;

	Refresh(false);
}

void BasicGLPane::mouseDown(wxMouseEvent& event) 
{

	wxPoint p=event.GetPosition();

	//Do not re-trigger if dragging or doing a scene update.
	//This can cause a selection test to occur whilst
	//a temp cam is activated in the scene, or a binding refresh is underway,
	//which is currently considered bad
	if(!dragging && !applyingDevice && !selectionMode 
			&& !currentScene.isInteractionLocked())
	{
		//Check to see if the user has clicked an object in the scene
		bool redraw;
		selectionTest(p,redraw);


		//If the selected object is valid, then
		//we did select an object. Treat this as a seletion event
		if(currentScene.getLastSelected() != (unsigned int)-1)
		{
			selectionMode=true;
			currentScene.setSelectionMode(true);
		}
		else
		{
			//we aren't setting, it  -- it shouldn't be the case
			ASSERT(selectionMode==false);

			//Prevent right button from triggering camera drag
			if(!event.LeftDown())
			{
				event.Skip();
				return;
			}

			//If not a valid selection, this is a camera drag.
			dragging=true;
		}

		draggingStart = event.GetPosition();
		//Set keyboard focus to self, to receive key events
		SetFocus();

		if(redraw)
			Refresh();
	}

	event.Skip();
}

void BasicGLPane::mouseWheelMoved(wxMouseEvent& event) 
{
	const float SHIFT_MULTIPLIER=5;

	float cameraMoveRate=-(float)event.GetWheelRotation()/(float)event.GetWheelDelta();

	cameraMoveRate*=mouseZoomFactor;

	if(event.ShiftDown())
		cameraMoveRate*=SHIFT_MULTIPLIER;

	cameraMoveRate*=CAMERA_SCROLL_RATE;
	//Move by specified delta
	currentScene.getActiveCam()->forwardsDolly(cameraMoveRate);

	//if we are using a temporary camera, update that too
	if(currentScene.haveTempCam())
		currentScene.getTempCam()->forwardsDolly(cameraMoveRate);

	haveCameraUpdates=true;
	Refresh();
	event.Skip();
}

void BasicGLPane::mouseReleased(wxMouseEvent& event) 
{
	if(currentScene.isInteractionLocked())
	{
		event.Skip();
		return;
	}
		
	if(selectionMode )
	{
		//If user releases all buttons, then allow the up
		if(!event.LeftIsDown() && 
				!event.RightIsDown() && !event.MiddleIsDown())
		{
			wxPoint p=event.GetPosition();
			
			int w, h;
			GetClientSize(&w, &h);
			applyingDevice=true;


			currentScene.applyDevice((float)draggingStart.x/(float)w,
						(float)draggingStart.y/(float)h,
						 p.x/(float)w,p.y/(float)h,
						 lastKeyFlags,lastMouseFlags,
						 true);
			
			applyingDevice=false;


			selectionMode=false;
			currentScene.setSelectionMode(selectionMode);

			Refresh();
		}
		event.Skip();
		return;
	}
	

	if(currentScene.haveTempCam())
		currentScene.commitTempCam();
	currentScene.finaliseCam();

	haveCameraUpdates=true;
	dragging=false;

	Refresh();
	event.Skip();
	
}

void BasicGLPane::rightClick(wxMouseEvent& event) 
{
}

void BasicGLPane::mouseLeftWindow(wxMouseEvent& event) 
{
	if(selectionMode)
	{
		wxPoint p=event.GetPosition();
		
		int w, h;
		GetClientSize(&w, &h);

		applyingDevice=true;
		currentScene.applyDevice((float)draggingStart.x/(float)w,
					(float)draggingStart.y/(float)h,
					 p.x/(float)w,p.y/(float)h,
					 lastKeyFlags,lastMouseFlags,
					 true);

		selectionMode=false;
		currentScene.setSelectionMode(selectionMode);
		Refresh();
		applyingDevice=false;

		event.Skip();
		return;

	}

	if(event.m_leftDown)
	{
		if(currentScene.haveTempCam())
		{
			currentScene.commitTempCam();
			dragging=false;
		}
	}
	event.Skip();
}

void BasicGLPane::keyPressed(wxKeyEvent& event) 
{
	
	switch(event.GetKeyCode())
	{
		case WXK_SPACE:
		{
			unsigned int visibleDir;
	
			//Use modifier keys to alter the direction of visibility
			//First compute the part of the keymask that does not
			//reflect the double tap
            // needs to be control in apple as cmd-space open spotlight
			unsigned int keyMask;
#ifdef __APPLE__
    #if wxCHECK_VERSION(2,9,0)
			keyMask = (event.RawControlDown() ? 1 : 0);
    #else
			keyMask = (event.ControlDown() ? 1 : 0);
    #endif
#else
			keyMask = (event.CmdDown() ? 1 : 0);
#endif
			keyMask |= (event.ShiftDown() ?  2 : 0);

			//Now determine if we are the same mask as last time
			bool isKeyDoubleTap=(lastKeyDoubleTap==keyMask);
			//double tapping allows for selection of reverse direction
			keyMask |= ( isKeyDoubleTap ?  4 : 0);
			
			visibleDir=-1;

			//Hardwire key combo->Mapping
			switch(keyMask)
			{
				//Space only
				case 0: 
					visibleDir=3; 
					break;
				//Command down +space 
				case 1:
					visibleDir=0;
					break;
				//Shift +space 
				case 2:
					visibleDir=2;
					break;
				//NO CASE 3	
				//Double+space
				case 4:
					visibleDir=5; 
					break;
				//Doublespace+Cmd
				case 5:
					visibleDir=4;
					break;

				//Space+Double+shift
				case 6:
					visibleDir=1;
					break;
				default:
					;
			}

			if(visibleDir!=(unsigned int)-1)
			{

				if(isKeyDoubleTap)
				{
					//It was a double tap. Reset the tapping and stop the timer
					lastKeyDoubleTap=(unsigned int)-1;
					keyDoubleTapTimer->Stop();
				}
				else
				{
					lastKeyDoubleTap=keyMask & (~(0x04));
					keyDoubleTapTimer->Start(DOUBLE_TAP_DELAY,wxTIMER_ONE_SHOT);
				}

				
				currentScene.ensureVisible(visibleDir);
				parentStatusBar->SetStatusText(wxTRANS("Use shift/ctrl-space or double tap to alter reset axis"));
				parentStatusBar->SetBackgroundColour(*wxCYAN)
					;
				parentStatusTimer->Start(statusDelay,wxTIMER_ONE_SHOT);
				Refresh();
				haveCameraUpdates=true;
			}
		}
		break;
	}
	event.Skip();
}

void BasicGLPane::setGlClearColour(float r, float g, float b)
{
	ASSERT(r >= 0.0f && r <= 1.0f);
	ASSERT(g >= 0.0f && g <= 1.0f);
	ASSERT(b >= 0.0f && b <= 1.0f);

	//Let openGL know that we have changed the colour.
	glClearColor( r, g, b, 0.0f);

	currentScene.setBackgroundColour(r,g,b);
	
	Refresh();
}

void BasicGLPane::keyReleased(wxKeyEvent& event) 
{

	float cameraMoveRate=CAMERA_KEYBOARD_SCROLL_RATE;

	if(event.ShiftDown())
		cameraMoveRate*=5;

	switch(event.GetKeyCode())
	{
		case '-':
		case WXK_NUMPAD_SUBTRACT:
		case WXK_SUBTRACT:
		{
			//Do a backwards dolly by fixed amount
			currentScene.getActiveCam()->forwardsDolly(cameraMoveRate);
			if(currentScene.haveTempCam())
				currentScene.getTempCam()->forwardsDolly(cameraMoveRate);
			break;
		}
		case '+':
		case '=':
		case WXK_NUMPAD_ADD:
		case WXK_ADD:
		{
			//Reverse direction of motion
			cameraMoveRate= -cameraMoveRate;
			
			//Do a forwards dolly by fixed amount
			currentScene.getActiveCam()->forwardsDolly(cameraMoveRate);
			if(currentScene.haveTempCam())
				currentScene.getTempCam()->forwardsDolly(cameraMoveRate);
			break;
		}
		default:
		;
	}

	Refresh();
	event.Skip();
}

void BasicGLPane::charEvent(wxKeyEvent& event) 
{

}
 
 
void BasicGLPane::resized(wxSizeEvent& evt)
{
	wxGLCanvas::OnSize(evt);

	prepare3DViewport(0,0,getWidth(),getHeight()); 
	wxClientDC *dc=new wxClientDC(this);
	Refresh();
	delete dc;
}
 
bool BasicGLPane::prepare3DViewport(int tlx, int tly, int brx, int bry)
{

	if(!paneInitialised)
		return false;

	//Prevent NaN.
	if(!(bry-tly))
		return false;
	GLint dims[2]; 
	glGetIntegerv(GL_MAX_VIEWPORT_DIMS, dims); 

	//Ensure that the opengGL function didn't tell us porkies
	//but double check for the non-debug bulds next line
	ASSERT(dims[0] && dims[1]);

	//check for exceeding max viewport and we have some space
	if(dims[0] <(brx-tlx) || dims[1] < bry-tly || 
			(!dims[0] || !dims[1] ))
		return false; 

	glViewport( tlx, tly, brx-tlx, bry-tly);

	currentScene.setWinSize(brx-tlx,bry-tly);

	//Assume no perspective transform
	//use scene camera to achieve this
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	
	float aspect = (float)(brx-tlx)/(float)(bry-tly);

	currentScene.setAspect(aspect);

	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();
	float r,g,b;
	currentScene.getBackgroundColour(r,g,b);
	glClearColor( r, g, b,1.0f );
	glClear(GL_COLOR_BUFFER_BIT |GL_DEPTH_BUFFER_BIT);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	    

	
	glEnable(GL_LIGHT0);

	
	glShadeModel(GL_SMOOTH);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	


	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial ( GL_FRONT, GL_AMBIENT_AND_DIFFUSE ) ;

	glEnable(GL_POINT_SMOOTH);
	glEnable(GL_LINE_SMOOTH);



//	  SetPosition( wxPoint(0,0) );

	return true;
}
 
int BasicGLPane::getWidth()
{
    return GetClientSize().x;
}
 
int BasicGLPane::getHeight()
{
    return GetClientSize().y;
}

void BasicGLPane::render( wxPaintEvent& evt )
{
	//Prevent calls to openGL if pane not visible
	if (!IsShown()) 
		return;
		
	wxGLCanvas::SetCurrent();
	if(!paneInitialised)
	{
		paneInitialised=true;
		prepare3DViewport(0,0,getWidth(),getHeight()); 
	}

	wxPaintDC(this); 
	currentScene.draw();
	glFlush();
	SwapBuffers();
}

void BasicGLPane::OnEraseBackground(wxEraseEvent &evt)
{
	//Do nothing. This is to help eliminate flicker apparently
}

void BasicGLPane::updateClearColour()
{
	float rClear,gClear,bClear;
	currentScene.getBackgroundColour(rClear,gClear,bClear);
	//Can't set the opengl window without a proper context
	ASSERT(paneInitialised);
	setGlClearColour(rClear,gClear,bClear);
	//Let openGL know that we have changed the colour.
	glClearColor( rClear, gClear, 
				bClear,1.0f);
}

bool BasicGLPane::saveImage(unsigned int width, unsigned int height,
		const char *filename, bool showProgress, bool needPostPaint)
{
	GLint dims[2]; 
	glGetIntegerv(GL_MAX_VIEWPORT_DIMS, dims); 

	//Opengl should not be giving us zero dimensions here.
	// if it does, just abandon saving the image as a fallback
	ASSERT(dims[0] && dims[1]);
	if(!dims[0] || !dims[1])
		return false;
	
	//create new image
	wxImage *image = new wxImage(width,height);

	//We cannot seem to draw outside the current viewport.
	//in a cross platform manner.
	//fall back to stitching the image together by hand
	char *pixels;
	

	pixels= new char[3*width*height];
	int panelWidth,panelHeight;
	GetClientSize(&panelWidth,&panelHeight);

	float oldAspect = currentScene.getAspect();
	currentScene.setAspect((float)panelHeight/(float)panelWidth);

	//Check
	if((unsigned int)width > panelWidth || (unsigned int)height> panelHeight)
	{
		unsigned int numTilesX,numTilesY;
		numTilesX = width/panelWidth;
		numTilesY = height/panelHeight;
		if(panelWidth % width)
			numTilesX++;
		
		if(panelHeight% height)
			numTilesY++;

		//Construct the image using "tiles": what we do is
		//use the existing viewport size (as we cannot reliably change it
		//without handling an OnSize event to resize the underlying 
		//system buffer (eg hwnd under windows.))
		//and then use this to reconstruct the image in a piece wise manner
		float tileStart[2];

		float fractionWidth=(float)panelWidth/(float)width;
		float fractionHeight=(float)panelHeight/(float)height;

		unsigned int thisTileNum=0;
	
		wxProgressDialog *wxD=0;	
		if(showProgress)
		{
			wxD = new wxProgressDialog(wxTRANS("Image progress"), 
						wxTRANS("Rendering tiles..."), numTilesX*numTilesY);

			wxD->Show();
		}

		std::string tmpStr,tmpStrTwo;
		stream_cast(tmpStrTwo,numTilesX*numTilesY);

		for(unsigned int tileX=0;tileX<numTilesX;tileX++)
		{
			tileStart[0]=(fractionWidth*(float)tileX-0.5);
			for(unsigned int tileY=0;tileY<numTilesY; tileY++)
			{
				thisTileNum++;
				stream_cast(tmpStr,thisTileNum);
				//tell user which image tile we are making from the total image
				tmpStr = std::string(TRANS("Tile ")) + tmpStr + std::string(TRANS(" of ")) + tmpStrTwo + "...";
				//Update progress bar, if required
				if(showProgress)
					wxD->Update(thisTileNum,wxStr(tmpStr));


				tileStart[1]=(fractionHeight*(float)tileY-0.5);
				//Adjust the viewport such that the render generates the tile for
				//this view. Coordinates are in normalised device coords (-1 to 1)
				currentScene.restrictView(tileStart[0],tileStart[1],
							  tileStart[0]+fractionWidth,tileStart[1]+fractionHeight);

				//Clear the buffers and draw the openGL scene
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				currentScene.draw();

				//Force openGL to block execution until draw complete
				glFlush();
				glFinish();

				//Grab the image generated for this tile
				glReadBuffer(GL_BACK);
				//Set the pixel alignment to one byte, so that openGL unpacks
				//correctly into our buffer.
				glPushAttrib(GL_PACK_ALIGNMENT);
				glPixelStorei(GL_PACK_ALIGNMENT,1);

				//Read image
				glReadPixels(0, 0, panelWidth,panelHeight,
					     GL_BGR, GL_UNSIGNED_BYTE, pixels);
				glPopAttrib();


				//Copy the data into its target location,
				char *pixel;
				unsigned int pixX,pixY;
				pixX=0;
				pixY=0;
				unsigned int cutoffX,cutoffY;
				cutoffX= std::min((tileX+1)*panelWidth,width);
				cutoffY= std::min((tileY+1)*panelHeight,height);
				for (unsigned int ui=tileX*panelWidth; ui<=cutoffX; ui++)
				{
					pixY=0;
					for (unsigned int uj=tileY*panelHeight; uj<=cutoffY; uj++)
					{
						pixel=pixels+(3*(pixY*panelWidth+ pixX));
						image->SetRGB(ui,(height-1)-uj,pixel[2],pixel[1],pixel[0]);
						pixY++;
					}
					pixX++;
				}
			}

		}
		//Disable the view restriction
		currentScene.unrestrictView();
		if(showProgress)
			wxD->Destroy();

	}
	else
	{
		//"Resize" the viewport. This wont cause the underlying buffer to resize
		//so the imae will reduce, but be surrounded by borders.
		//Its a bit of a hack, but it should work
		prepare3DViewport(0,0,width,height);
	
		//Clear the buffers and draw the openGL scene
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		currentScene.draw();
		glFinish();

		SwapBuffers();
		glReadBuffer(GL_BACK);
		
		//Set the pixel alignment to one byte, so that openGL unpacks 
		//correctly into our buffer.
		glPushAttrib(GL_PACK_ALIGNMENT);
		glPixelStorei(GL_PACK_ALIGNMENT,1);
		//Read the image
		glReadPixels(0, 0, width,height, 
			GL_BGR, GL_UNSIGNED_BYTE, pixels);


		glPopAttrib();
	
		char *pixel;
		
		//copy rows & columns into image
		for(unsigned int ui=0;ui<width;ui++)
		{
			for(unsigned int uj=0;uj<height;uj++)
			{
				pixel=pixels+(3*(uj*width + ui));
				image->SetRGB(ui,(height-1)-uj,pixel[2],pixel[1],pixel[0]);
			}
		}

		//Restore viewport
		prepare3DViewport(0,0,getWidth(),getHeight());
	}

	delete[] pixels;

	bool isOK=image->SaveFile(wxCStr(filename),wxBITMAP_TYPE_PNG);

	currentScene.setAspect(oldAspect);
	delete image;

    if (needPostPaint) {
        wxPaintEvent event;
        wxPostEvent(this,event);
    }

	return isOK;
}

void BasicGLPane::OnAxisTapTimer(wxTimerEvent &evt)
{
	lastKeyDoubleTap=(unsigned int)-1;
}


bool BasicGLPane::saveImageSequence(unsigned int resX, unsigned int resY, unsigned int nFrames,
		wxString &path,wxString &prefix, wxString &ext)
{
	//OK, lets animate!
	//


	ASSERT(!currentScene.haveTempCam());
	std::string outFile;
	wxProgressDialog *wxD = new wxProgressDialog(wxTRANS("Animation progress"), 
					wxTRANS("Rendering sequence..."), nFrames,this,wxPD_CAN_ABORT );

	wxD->Show();
	std::string tmpStr,tmpStrTwo;
	stream_cast(tmpStrTwo,nFrames);
	for(unsigned int ui=0;ui<nFrames;ui++)
	{
		Camera *c;
		float angle;
		std::string digitStr;

		//Create a string like 00001, such that there are always leading zeros
		digitStr=digitString(ui,nFrames);

		//Manipulate the camera such that it orbits around its current axis
		//FIXME: Why is this M_PI, not 2*M_PI???
		angle= (float)ui/(float)nFrames*M_PI;

		//create a new temp camera
		currentScene.setTempCam();
		c=currentScene.getTempCam();
		//Rotate the temporary camera
		c->move(angle,0);

		//Save the result
		outFile = string(stlStr(path))+ string("/") + 
				string(stlStr(prefix))+digitStr+ string(".") + string(stlStr(ext));
		if(!saveImage(resX,resY,outFile.c_str(),false, false))
			return false;

		//Update the progress bar
		stream_cast(tmpStr,ui+1);
		//Tell user which image from the animation we are saving
		tmpStr = std::string(TRANS("Saving Image ")) + tmpStr + std::string(TRANS(" of ")) + tmpStrTwo + "...";
		if(!wxD->Update(ui,wxStr(tmpStr)))
			break;

		Refresh();
	}

	//Discard the current temp. cam to return the scene back to normal
	currentScene.discardTempCam();
	wxD->Destroy();
	
	wxPaintEvent event;
	wxPostEvent(this,event);
	return true;
		
}
