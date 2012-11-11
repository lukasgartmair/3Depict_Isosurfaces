/*
 * 	scene.h - Opengl interaction header. 
 *	Copyright (C) 2011, D Haley 

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
#ifndef SCENE_H
#define SCENE_H

#include <vector>
#include <algorithm>

class Scene;

//Custom includes
#include "drawables.h"
#include "select.h"
#include "basics.h"
#include "viscontrol.h"
//cameras.h uses libxml2. libxml2 conflicts with wx headers, and must go last
#include "cameras.h" 

#include "filter.h"
#include "textures.h"
#include "effect.h"

//OpenGL debugging macro
#if DEBUG
#define glError() { \
		GLenum err = glGetError(); \
		while (err != GL_NO_ERROR) { \
					fprintf(stderr, "glError: %s caught at %s:%u\n", (char *)gluErrorString(err), __FILE__, __LINE__); \
					err = glGetError(); \
				} \
		std::cerr << "glErr Clean " << __FILE__ << ":" << __LINE__ << std::endl; \
}
#else
#define glError()
#endif

#ifdef DEBUG
	#define glStackDepths() { \
		int gldepthdebug[3];glGetIntegerv (GL_MODELVIEW_STACK_DEPTH, gldepthdebug);\
	       	glGetIntegerv (GL_PROJECTION_STACK_DEPTH, gldepthdebug+1);\
	       	glGetIntegerv (GL_TEXTURE_STACK_DEPTH, gldepthdebug+2);\
		std::cerr << "OpenGL Stack Depths: ModelV:" << gldepthdebug[0] << " Pr: "\
		 << gldepthdebug[1] << " Tex:" << gldepthdebug[2] << std::endl;}
#else
	#define glStackDepths()
#endif

//!The scene class brings together elements such as objects, lights, and cameras
//to enable scene rendering
class Scene
{
	private:
		//!Viscontroller. Needed for notification of updates during selection binding
		VisController *visControl;
		//!Objects that will be used for drawing
		std::vector<DrawableObj const * > objects;
		
		//!Objects used for drawing that will not be destroyed
		std::vector<const DrawableObj * > refObjects;


		//!Bindings for interactive object properties
		std::vector<SelectionDevice<Filter> *> selectionDevices;

		//!Various OpenGL effects
		std::vector<const Effect *> effects;

		//!Vector of camera stats
		std::vector<Camera *> cameras;

		//!Temporary override camera
		Camera *tempCam;

		//!Texture pool
		TexturePool texPool;

		//!Size of window in px (needed if doing 2D drawing)
		unsigned int winX,winY;

		//!Which camera are we using
		unsigned int activeCam;
		//!Is there a camera set?
		bool cameraSet;
		//!Aspect ratio of output window (x/y) -- needed for cams
		float outWinAspect;

		//!Blank canvas colour
		float r,g,b;


		//!Camera id storage and handling
		UniqueIDHandler camIDs;

		//!Effect ID handler
		UniqueIDHandler effectIDs;

		//!Cube that holds the scene bounds
		BoundCube boundCube;


		//!True if user interaction (selection/hovering) is forbidden
		bool lockInteract;
		//!Tells the scene if we are in selection mode or not
		bool selectionMode;

		//!Tells us if we are in hover mode (should we draw hover overlays?)
		bool hoverMode;

		//!Is the camera to be restricted to only draw a particular portion of the viewport?
		bool viewRestrict;

		float viewRestrictStart[2], viewRestrictEnd[2];
			
		//!Last selected object from call to glSelect(). -1 if last call failed to identify an item
		unsigned int lastSelected;
	
		//!Last hoeverd object	
		unsigned int lastHovered;

		//!Should alpha belnding be used?
		bool useAlpha;
		//!Should lighting calculations be performed?
		bool useLighting;
		//!Should we be using effects?
		bool useEffects;

		//!Should the world axis be drawn?
		bool showAxis;

		//!Background colour
		float rBack,gBack,bBack;


		///!Draw the hover overlays
		void drawHoverOverlay();

		//!Draw the normal overlays
		void drawOverlays() const;

		//!initialise the drawing window
		unsigned int initDraw();

		void updateCam(const Camera *camToUse) const;


		//!Draw a specified vector of objects
		void drawObjectVector(const std::vector<const DrawableObj*> &objects, bool &lightsOn, bool drawOpaques=true) const;

				
	public:
		//!Constructor
		Scene();
		//!Destructor
		virtual ~Scene();

		//!Set the vis control
		void setViscontrol(VisController *v) { visControl=v;};
		//!Draw the objects in the active window. May adjust cameras and compute bounding as needed.
		void draw();

	
		//!clear rendering vectors
		void clearAll();
		//!Clear drawing objects vector
		void clearObjs();
		//! Clear the reference object vector
		void clearRefObjs();
		//!Clear object bindings vector
		void clearBindings();
		//!Clear camera vector
		void clearCams();
		//!Set the aspect ratio of the output window. Required.
		void setAspect(float newAspect);
		//!retreive aspect ratio (h/w) of output win
		float getAspect() const { return outWinAspect;};
		
		//!Add a drawable object 
		/*!Pointer must be set to a valid (allocated) object.
		 *!Scene will delete upon call to clearAll, clearObjs or
		 *!upon destruction
		 */
		void addDrawable(const DrawableObj *);
		
		//!Add a drawble to the refernce only section
		/* Objects refferred to will not be modified or destroyed
		 * by this class. It will onyl be used for drawing purposes
		 * It is up to the user to ensure that they are in a good state
		 */
		void addRefDrawable(const DrawableObj *);
		

		//!remove a drawable object
		void removeDrawable(unsigned int);

		//!Add a camera 
		/*!Pointer must be set to a valid (allocated) object.
		 *!Scene will delete upon call to clearAll, clearCameras or
		 *!upon destruction
		 */
		unsigned int addCam(Camera *);
		
		//!remove a camera object
		void removeCam(unsigned int uniqueCamID);


		//! set the active camera
		void setActiveCam(unsigned int uniqueCamID);

		//! get the active camera
		Camera *getActiveCam() { ASSERT(cameras.size()); return cameras[activeCam];};

		//! get the active camera's location
		Point3D getActiveCamLoc() const;

		//!Construct (or refresh) a temporary camera
		/*! this temporary camera is discarded  with 
		 * either killTempCam or reset to the active
		 * camera with another call to setTempCam().
		 * The temporary camera overrides the existing camera setup
		 */ 
		void setTempCam();

		//!Return pointer to active camera. Must init a temporary camera first!  (use setTempCam)
		Camera *getTempCam() { ASSERT(tempCam); return tempCam;};

		//!Make the temp camera permanent.
		void commitTempCam();
		
		//!Discard the temporary camera
		void discardTempCam();

		//!Are we using a temporary camera?
		bool haveTempCam() const { return tempCam!=0;};

		//!Clone the active camera
		Camera *cloneActiveCam() const { return cameras[activeCam]->clone(); };

		//!Get the number of cameras (excluding tmp cam)
		unsigned int getNumCams() const { return cameras.size(); } ;

		//!Get the camera properties for a given camera
		void getCamProperties(unsigned int uniqueID, CameraProperties &p) const;
		//!Set the camera properties for a given camera. returns true if property set is OK
		bool setCamProperty(unsigned int uniqueID, unsigned int key,
	       					const std::string &value);
		//!Return ALL the camera unique IDs
		void getCameraIDs(vector<std::pair<unsigned int, std::string> > &idVec) const;

		//!Modify the active camera position to ensure that scene is visible 
		void ensureVisible(unsigned int direction);

		//!Set the active camera to the first entry. Only to be called if getNumCams > 0
		void setDefaultCam();

		//!Call if user has stopped interacting with camera briefly.
		void finaliseCam();

		//!perform an openGL selection rendering pass. Return 
		//closest object in depth buffer under position 
		//if nothing, returns -1
		unsigned int glSelect(bool storeSelection=true);

		//!Add selection devices to the scene.
		void addSelectionDevices(const std::vector<SelectionDevice<Filter> *> &d);

		//!Clear the current selection devices 
		void clearDevices();

		//!Apply the device given the following start and end 
		//viewport coordinates.
		void applyDevice(float startX, float startY,
					float curX, float curY,unsigned int keyFlags, 
					unsigned int mouseflags,bool permanent=true);

		// is interaction currently locked?
		bool isInteractionLocked()  const { return lockInteract;}
		//!Prevent user interactoin
		bool lockInteraction(bool amLocking=true) { lockInteract=amLocking;};
		//!Set selection mode true=select on, false=select off.
		//All this does internally is modify how drawing works.
		void setSelectionMode(bool selMode) { selectionMode=selMode;};

		//!Set the hover mode to control drawing
		void setHoverMode(bool hMode) { hoverMode=hMode;};

		//!Return the last object over whichthe cursor was hovered	
		void setLastHover(unsigned int hover) { lastHovered=hover;};
		//!Get the last selected object from call to glSelect()
		unsigned int getLastSelected() const { return lastSelected;};
	
		//!Return the last object over whichthe cursor was hovered	
		unsigned int getLastHover() const { return lastHovered;};
		//!Duplicates the internal camera vector. return value is active camera
		//in returned vector
		unsigned int duplicateCameras(std::vector<Camera *> &cams) const; 
		//!Get a copy of the effects pointers
		void getEffects(std::vector<const Effect *> &effects) const; 

		//!Return the unique ID of the active camera
		unsigned int getActiveCamId() const;

		//!Return any devices that have been modified since their creation
		void getModifiedBindings(std::vector<std::pair<const Filter *,SelectionBinding > > &bindings) const;

		//!Restrict the openGL drawing view when using the camera
		void restrictView(float xS,float yS, float xFin, float yFin);
		//!Disable view restriction
		void unrestrictView() { viewRestrict=false;};

		//!True if the current camera is the default (0th) camera
		bool isDefaultCam() const; 

		//!Set whether to use alpha blending
		void setAlpha(bool newAlpha) { useAlpha=newAlpha;};

		//!Set whether to enable lighting
		void setLighting(bool newLight) { useLighting=newLight;};

		//!Set whether to enable the XYZ world axes
		void setWorldAxisVisible(bool newAxis) { showAxis=newAxis;};
		//!Get whether the XYZ world axes are enabled
		bool getWorldAxisVisible() const { return showAxis;};

		//!Set window size
		void setWinSize(unsigned int x, unsigned int y) {winX=x;winY=y;}

		//!Get the scene boundinng box
		BoundCube getBound() const { return boundCube;}

		//!Returns true if this camera name is already in use
		bool camNameExists(const std::string &s) const;

		//!Set the background colour
		void setBackgroundColour(float newR,float newG,float newB) { rBack=newR;gBack=newG;bBack=newB;};

		void getBackgroundColour(float &newR,float &newG,float &newB) const { newR=rBack;newG=gBack;newB=bBack;};
		
		//!Computes the bounding box for the scene. 
		//this is locked to a minimum of 0.1 unit box around the origin.
		//this avoids nasty camera situations, where lookat cameras are sitting
		//on their targets, and don't know where to look.
		void computeSceneLimits();
		
		//!Set whether to use effects or not
		void setEffects(bool enable) {useEffects=enable;} 

		//!Add an effect
		unsigned int addEffect(Effect *e);
		//!Remove a given effect
		void removeEffect(unsigned int uniqueEffectID);

		//!Clear effects vector
		void clearEffects();
};

#endif
