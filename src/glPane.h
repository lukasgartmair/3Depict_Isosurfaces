/*
 *	gLPane.h - WxWidgets opengl Pane. 
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
#ifndef _glpane_
#define _glpane_
 
#include "wxPreprec.h"

#define GL_GLEXT_PROTOTYPES 1
#include <wx/glcanvas.h>

//Local includes
#include "scene.h"

#include "basics.h"

class BasicGLPane : public wxGLCanvas
{
private:
	wxStatusBar *parentStatusBar;
	wxTimer *parentStatusTimer;
	unsigned int statusDelay;	
	//In some implementation of openGL in wx. 
	//calling GL funcs before Paint() will crash program
	bool paneInitialised;
	bool dragging;
	wxPoint draggingStart;
	bool lastMoveShiftDown;
	//True if an object has been mouse-overed for selection
	bool selectionMode;
	//The scene ID value for the currently selected object
	unsigned int curSelectedObject;
	//The scene ID value for object currently being "hovered" over
	unsigned int hoverObject;


	//!Last mouseflags/keyflags during selection event
	unsigned int lastMouseFlags,lastKeyFlags;

	//Test for a object selection. REturns -1 if no selection
	//or object ID if selection OK. Also sets lastSelected & scene
	unsigned int selectionTest(wxPoint &p,  bool &shouldRedraw);

	//Test for a object hover under cursor Returns -1 if no selection
	//or object ID if selection OK. Also sets last hover and scene
	unsigned int hoverTest(wxPoint &p,  bool &shouldRedraw);

	//!OpenGL clear colour, which controls background colour
	Colour glBackgroundColour;

	//!Are there updates to the camera Properties due to camera motion?
	bool haveCameraUpdates;

	//!Are we currently applying a device in the scene?
	bool applyingDevice;
public:
	//!Constructor
	BasicGLPane(wxWindow* parent, wxWindowID id); 

	//!The scene object, holds all info about 3D drawable components
	Scene currentScene;

	//!Must be cllaed before user has a chance to perform interaction
	void setParentStatus(wxStatusBar *statusBar,
			wxTimer *timer,unsigned int statDelay) 
		{ parentStatusBar=statusBar;parentStatusTimer=timer;statusDelay=statDelay;};

	bool hasCameraUpdates() const {return haveCameraUpdates;};
	
	void clearCameraUpdates() {haveCameraUpdates=false;};

	BasicGLPane(wxWindow* parent);
    
	void resized(wxSizeEvent& evt);
    
	int getWidth();
	int getHeight();
  
       	
	//!Set the background colour (openGL clear colour)
	void setGlClearColour(float r,float g,float b);	
	//!Set the background colour (openGL clear colour)
	Colour getGlClearColour() const { return glBackgroundColour;};
	//!Render the view using the scene
	void render(wxPaintEvent& evt);
	//!Construct a 3D viewport, ready for openGL output. returns false if intialisation failed
	bool prepare3DViewport(int topleft_x, int topleft_y, int bottomrigth_x, int bottomrigth_y);
   	//!Save an image to file, return false on failure
	bool saveImage(unsigned int width, unsigned int height,const char *filename);
 
	// events
	void mouseMoved(wxMouseEvent& event);
	void mouseDown(wxMouseEvent& event);
	void mouseWheelMoved(wxMouseEvent& event);
	void mouseReleased(wxMouseEvent& event);
	void rightClick(wxMouseEvent& event);
	void mouseLeftWindow(wxMouseEvent& event);
	void keyPressed(wxKeyEvent& event);
	void keyReleased(wxKeyEvent& event);
	void charEvent(wxKeyEvent& event);
	void OnEraseBackground(wxEraseEvent &);
	bool setFullscreen(bool fullscreen);
	bool setMouseVisible(bool visible);
   
	
	DECLARE_EVENT_TABLE()
};
 
#endif 
