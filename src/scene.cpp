/*
 *	scene.cpp  - OPengl scene implementation
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

#include "scene.h"

#include <string>


using std::vector;

Scene::Scene() : tempCam(0), activeCam(0), cameraSet(false), outWinAspect(1.0f), r(0.0f), g(0.0f), b(0.0f)
{
	lastHovered=lastSelected=(unsigned int)(-1);
	hoverMode=selectionMode=false;
	viewRestrict=false;
	useAlpha=true;
	useLighting=true;
	useEffects=false;
	showAxis=true;

	//default to black
	rBack=gBack=bBack=0.0f;

}

Scene::~Scene()
{
	clearAll();
}

unsigned int Scene::initDraw()
{
	glClear(GL_COLOR_BUFFER_BIT |GL_DEPTH_BUFFER_BIT);

	if(useAlpha)
		glEnable(GL_BLEND);
	else
		glDisable(GL_BLEND);

	glDisable(GL_LIGHTING);
	//Set up the scene lights
	//==
	//Set up default lighting
	const float light_ambient[] = { 0.0, 0.0, 0.0, 1.0 };
	const float light_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
	const float light_specular[] = { 0.0, 0.0, 0.0, 0.0 };
	float light_position[] = { 1.0, 1.0, 1.0, 0.0 };
	glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
	glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);




	glDisable(GL_LIGHTING);
	//==
	
	//Use the active if set 
	if(cameraSet)
	{
		if(!boundCube.isValid())
			computeSceneLimits();

	}



	//Let the effects objects know about the scene
	Effect::setBoundingCube(boundCube);

	unsigned int passes=1;

	if(useEffects)
	{
		for(unsigned int ui=0;ui<effects.size();ui++)
			passes=std::max(passes,effects[ui]->numPassesNeeded());
	}

	return passes;
}

void Scene::updateCam(const Camera *camToUse) const
{
	Point3D lightNormal;
	

	glLoadIdentity();
	//If viewport restriction is on, inform camera to
	//shrink viewport to specified region
	if(viewRestrict)
	{
		camToUse->apply(outWinAspect,boundCube,true,
				viewRestrictStart[0],viewRestrictEnd[0],
				viewRestrictStart[1],viewRestrictEnd[1]);
	}
	else
		camToUse->apply(outWinAspect,boundCube);

	lightNormal=camToUse->getViewDirection();
	glNormal3f(lightNormal[0],lightNormal[1],lightNormal[2]);	

}

void Scene::draw() 
{

	glPushMatrix();


	Camera *camToUse;
	if(tempCam)
		camToUse=tempCam;
	else
	{
		ASSERT(activeCam < cameras.size());
		camToUse=cameras[activeCam];
	}
	//Inform text about current camera, so it can billboard if needed
	DrawableObj::setCurCamera(camToUse);
	Effect::setCurCam(camToUse);


	bool lightsOn=false;
	unsigned int numberTotalPasses;
	numberTotalPasses=initDraw();

	unsigned int passNumber=0;

	if(cameraSet)
		updateCam(camToUse);



	bool needCamUpdate=false;
	while(passNumber < numberTotalPasses)
	{

		if(useEffects)
		{
			for(unsigned int ui=0;ui<effects.size();ui++)
			{
				effects[ui]->enable(passNumber);
				needCamUpdate|=effects[ui]->needCamUpdate();
			}

			if(cameraSet && needCamUpdate)
			{
				glLoadIdentity();	
				updateCam(camToUse);
			}
		}


		if(showAxis)
		{
			if(useLighting)
				glEnable(GL_LIGHTING);
			DrawAxis a;
			a.setStyle(AXIS_IN_SPACE);
			a.setSize(boundCube.getLargestDim());
			a.setPosition(boundCube.getCentroid());

			a.draw();
			if(useLighting)
				glDisable(GL_LIGHTING);
		}
		
	
		//First sub-pass with opaque objects
		//-----------	
		//Draw the referenced objects
		drawObjectVector(refObjects,lightsOn,true);
		//Draw normal objects
		drawObjectVector(objects,lightsOn,true);
		//-----------	
		
		//Second pass with transparent objects
		//-----------	
		//Draw the referenced objects
		drawObjectVector(refObjects,lightsOn,false);
		//Draw normal objects
		drawObjectVector(objects,lightsOn,false);
		//-----------	
		
		
		glFlush();
		passNumber++;
	}
	
	
	//Disable effects
	if(useEffects)
	{
		//Disable them in reverse order to simulate a stack-type
		//behaviour.
		for(unsigned int ui=effects.size();ui!=0;)
		{
			ui--;
			effects[ui]->disable();
		}
	}


	glPopMatrix();
		
	//Now draw 2D overlays
	if(lastHovered != (unsigned int)(-1))
		drawHoverOverlay();
	drawOverlays();

}

void Scene::drawObjectVector(const vector<const DrawableObj*> &drawObjs, bool &lightsOn, bool drawOpaques) const
{
	for(unsigned int ui=0; ui<drawObjs.size(); ui++)
	{
		//FIXME: Use logical operator to simplify this.
		// its late, and i'm tired and can't think. Its very easy
		// when you can think. 
		if(drawOpaques)
		{
			//Don't draw opaque drawObjs in this pass
			 if(drawObjs[ui]->needsDepthSorting())
				continue;
		}
		else
		{
			//Do draw opaque drawObjs in this pass
			 if(!drawObjs[ui]->needsDepthSorting())
				continue;
		}
	
		//overlays need to be drawn later
		if(drawObjs[ui]->isOverlay())
			continue;
		if(useLighting)
		{	
			if(!drawObjs[ui]->wantsLight && lightsOn )
			{
				//Object prefers doing its thing in the dark
				glDisable(GL_LIGHTING);
				lightsOn=false;
			}
			else if (drawObjs[ui]->wantsLight && !lightsOn)
			{
				glEnable(GL_LIGHTING);
				lightsOn=true;
			}
		}
		
		//If we are in selection mode, draw the bounding box
		//if the object is selected.
		if(ui == lastSelected && selectionMode)
		{
			//May be required for selection box drawing
			BoundCube bObject;
			DrawRectPrism p;
			//Get the bounding box for the object & draw it
			drawObjs[ui]->getBoundingBox(bObject);
			p.setAxisAligned(bObject);
			p.setColour(0,0.2,1,0.5); //blue-greenish
			if(lightsOn)
				glDisable(GL_LIGHTING);
			p.draw();
			if(lightsOn)
				glEnable(GL_LIGHTING);

		}
		
		drawObjs[ui]->draw();
	}
}

void Scene::drawOverlays() const
{

	//Custom projection matrix
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	glDisable(GL_LIGHTING);
	//Set the opengl camera state back into modelview mode
	if(viewRestrict)
	{

		//FIXME: How does the aspect ratio fit in here?
		gluOrtho2D(viewRestrictStart[0],
				viewRestrictEnd[0],
				viewRestrictStart[0],
				viewRestrictStart[1]);
	}
	else
		gluOrtho2D(0, outWinAspect, 1.0, 0);



	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);


	for(unsigned int ui=0;ui<refObjects.size();ui++)
	{
		if(refObjects[ui]->isOverlay())
			refObjects[ui]->draw();
	}
	
	for(unsigned int ui=0;ui<objects.size();ui++)
	{
		if(objects[ui]->isOverlay())
			objects[ui]->draw();
	}
	

	glEnable(GL_DEPTH_TEST);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);
}

void Scene::drawHoverOverlay()
{

	glEnable(GL_ALPHA_TEST);
	glDisable(GL_DEPTH_TEST);
	//Search for a binding
	bool haveBinding;
	haveBinding=false;

	//Prevent transparent areas from interactiing
	//with the depth buffer
	glAlphaFunc(GL_GREATER,0.01f);
	
	vector<const SelectionBinding *> binder;
	for(unsigned int uj=0;uj<selectionDevices.size();uj++)
	{
		if(selectionDevices[uj]->getAvailBindings(objects[lastHovered],binder))
		{
			haveBinding=true;
			break;
		}
	}


	if(haveBinding)
	{	
		glPushAttrib(GL_LIGHTING);
		glDisable(GL_LIGHTING);

		//Now draw some hints for the binding itself as a 2D overlay
		//
		//Draw the action type (translation, rotation etc)
		//and the button it is bound to
		DrawTexturedQuadOverlay binderIcons,mouseIcons,keyIcons;


		const float ICON_SIZE= 0.05;
		binderIcons.setTexturePool(&texPool);
		binderIcons.setWindowSize(winX,winY);	
		binderIcons.setSize(ICON_SIZE*winY);
		
		mouseIcons.setTexturePool(&texPool);
		mouseIcons.setWindowSize(winX,winY);	
		mouseIcons.setSize(ICON_SIZE*winY);
	
		keyIcons.setTexturePool(&texPool);
		keyIcons.setWindowSize(winX,winY);	
		keyIcons.setSize(ICON_SIZE*winY);

		unsigned int iconNum=0;
		for(unsigned int ui=0;ui<binder.size();ui++)
		{
			bool foundIconTex,foundMouseTex;
			foundIconTex=false;
			foundMouseTex=false;
			switch(binder[ui]->getInteractionMode())
			{
				case BIND_MODE_FLOAT_SCALE:
				case BIND_MODE_FLOAT_TRANSLATE:
				case BIND_MODE_POINT3D_SCALE:
					foundIconTex=binderIcons.setTexture(TEST_OVERLAY_PNG[TEXTURE_ENLARGE]);
					break;
				case BIND_MODE_POINT3D_TRANSLATE:
					foundIconTex=binderIcons.setTexture(TEST_OVERLAY_PNG[TEXTURE_TRANSLATE]);
					break;
				case BIND_MODE_POINT3D_ROTATE:
				case BIND_MODE_POINT3D_ROTATE_LOCK:
					foundIconTex=binderIcons.setTexture(TEST_OVERLAY_PNG[TEXTURE_ROTATE]);
				default:
					break;
			}

			//Draw the mouse action
			switch(binder[ui]->getMouseButtons())
			{
				case SELECT_BUTTON_LEFT:
					foundMouseTex=mouseIcons.setTexture(TEST_OVERLAY_PNG[TEXTURE_LEFT_CLICK]);
					break;
				case SELECT_BUTTON_MIDDLE:
					foundMouseTex=mouseIcons.setTexture(TEST_OVERLAY_PNG[TEXTURE_MIDDLE_CLICK]);
					break;
				case SELECT_BUTTON_RIGHT:
					foundMouseTex=mouseIcons.setTexture(TEST_OVERLAY_PNG[TEXTURE_RIGHT_CLICK]);
					break;
				default:
					//The flags are or'd together, so we can get other combinations
					break;
			}

			bool foundKeyTex;
			foundKeyTex=false;
			//Draw the keyboard action, if any
			switch(binder[ui]->getKeyFlags())
			{
				case FLAG_CMD:
#ifdef __APPLE__
					foundKeyTex=keyIcons.setTexture(TEST_OVERLAY_PNG[TEXTURE_COMMAND]);
#else
					foundKeyTex=keyIcons.setTexture(TEST_OVERLAY_PNG[TEXTURE_CTRL]);
#endif
					break;
				case FLAG_SHIFT:
					foundKeyTex=keyIcons.setTexture(TEST_OVERLAY_PNG[TEXTURE_SHIFT]);
					break;
				default:
					//The flags are or'd together, so we can get other combinations
					break;
			}

			if(foundIconTex && foundMouseTex )
			{
				const float SPACING=0.75*ICON_SIZE;
				if(foundKeyTex)
				{
					//Make room for keyTex
					binderIcons.setPos((0.93+SPACING)*winX,ICON_SIZE*winY*(1+(float)iconNum));
					keyIcons.setPos(0.93*winX,ICON_SIZE*winY*(1+(float)iconNum));
					mouseIcons.setPos((0.93-SPACING)*winX,ICON_SIZE*winY*(1+(float)iconNum));
				}
				else
				{
					binderIcons.setPos(0.95*winX,ICON_SIZE*winY*(1+(float)iconNum));
					mouseIcons.setPos(0.90*winX,ICON_SIZE*winY*(1+(float)iconNum));
				}

				binderIcons.draw();
				mouseIcons.draw();

				if(foundKeyTex)
					keyIcons.draw();

				iconNum++;
			}
			
		}

		glPopAttrib();

	}


	glDisable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);
}


void Scene::commitTempCam()
{
	ASSERT(tempCam);
	std::swap(cameras[activeCam], tempCam);
	delete tempCam;
	tempCam=0;
}

void Scene::discardTempCam()
{
	delete tempCam;
	tempCam=0;
}

void Scene::setTempCam()
{
	//If a temporary camera is not set, set one.
	//if it is set, update it from the active camera
	if(!tempCam)
		tempCam =cameras[activeCam]->clone();
	else
		*tempCam=*cameras[activeCam];
}

void Scene::addDrawable(DrawableObj const *obj )
{
	objects.push_back(obj);
	BoundCube bc;
	obj->getBoundingBox(bc);

	if(bc.isValid())
		boundCube.expand(bc);
}

void Scene::addRefDrawable(const DrawableObj *obj)
{
	refObjects.push_back(obj);
	BoundCube bc;
	obj->getBoundingBox(bc);

	ASSERT(bc.isValid());
	boundCube.expand(bc);
}	

void Scene::clearAll()
{
	//Invalidate the bounding cube
	boundCube.setInverseLimits();

	clearObjs();
	clearRefObjs();
	clearBindings();	
	clearCams();
}

void Scene::clearObjs()
{
	for(unsigned int ui=0; ui<objects.size(); ui++)
		delete objects[ui];
	objects.clear();
}

void Scene::clearBindings()
{
	for(unsigned int ui=0; ui<selectionDevices.size(); ui++)
		delete selectionDevices[ui];
	selectionDevices.clear();
}


void Scene::clearCams()
{
	for(unsigned int ui=0; ui<cameras.size(); ui++)
		delete cameras[ui];

	cameras.clear();
	camIDs.clear();
}

void Scene::clearRefObjs()
{
	refObjects.clear();
}


unsigned int Scene::addCam(Camera *c)
{
	ASSERT(c);
	ASSERT(cameras.size() == camIDs.size());
	cameras.push_back(c);
	unsigned int id = camIDs.genId(cameras.size()-1);

	if(cameras.size() == 1)
		setDefaultCam();
	return id;
}

void Scene::removeCam(unsigned int camIDVal)
{
	unsigned int position = camIDs.getPos(camIDVal);
	delete cameras[position];
	cameras.erase(cameras.begin()+position);
	camIDs.killByPos(position);

	if(cameras.size())
	{
		activeCam=0;
		cameraSet=true;
	}
	else
		cameraSet=false;
}


void Scene::setAspect(float newAspect)
{
	outWinAspect=newAspect;
}

void Scene::setActiveCam(unsigned int idValue)
{

	if(tempCam)
		discardTempCam();

	unsigned int position = camIDs.getPos(idValue);
	activeCam = position;
	cameraSet=true;
}

void Scene::setDefaultCam()
{
	ASSERT(cameras.size());
	if(tempCam)
		discardTempCam();
	activeCam=0;
	cameraSet=true;
}


void Scene::ensureVisible(unsigned int direction)
{
	computeSceneLimits();

	cameras[activeCam]->ensureVisible(boundCube,direction);
}

void Scene::computeSceneLimits()
{
	boundCube.setInverseLimits();

	BoundCube b;
	for(unsigned int ui=0; ui<objects.size(); ui++)
	{
		objects[ui]->getBoundingBox(b);
	
		if(b.isValid())	
			boundCube.expand(b);
	}

	for(unsigned int ui=0; ui<refObjects.size(); ui++)
	{
		refObjects[ui]->getBoundingBox(b);
		
		if(b.isValid())	
			boundCube.expand(b);
	}


	if(!boundCube.isValid())
	{
		//He's going to spend the rest of his life
		//in a one by one unit box.

		//If there are no objects, then set the bounds
		//to 1x1x1, centered around the origin
		boundCube.setBounds(-0.5,-0.5,-0.5,
					0.5,0.5,0.5);
	}
	//NOw that we have a scene level bounding box,
	//we need to set the camera to ensure that
	//this box is visible
	ASSERT(boundCube.isValid());


	//The scene bounds should be no less than 0.1 units
	BoundCube unitCube;
	Point3D centre;

	centre=boundCube.getCentroid();

	unitCube.setBounds(centre+Point3D(0.05,0.05,0.05),
				centre-Point3D(0.05,0.05,0.05));
	boundCube.expand(unitCube);
}

void Scene::getCamProperties(unsigned int uniqueID, CameraProperties &p) const
{
	unsigned int position=camIDs.getPos(uniqueID);
	cameras[position]->getProperties(p);
}

void Scene::getCameraIDs(vector<std::pair<unsigned int, std::string> > &idVec) const
{
	std::vector<unsigned int> ids;
	camIDs.getIds(ids);

	idVec.resize(ids.size());
	for(unsigned int ui=0;ui<ids.size(); ui++)
	{
		idVec[ui] = std::make_pair(ids[ui],
			cameras[camIDs.getPos(ids[ui])]->getUserString());
	}
}

bool Scene::setCamProperty(unsigned int uniqueID, unsigned int key,
	       					const std::string &value) 
{
	unsigned int position=camIDs.getPos(uniqueID);
	return cameras[position]->setProperty(key,value);
}

//Adapted from 
//http://chadweisshaar.com/robotics/docs/html/_v_canvas_8cpp-source.html
//GPLv3+ permission obtained by email inquiry.
unsigned int Scene::glSelect(bool storeSelected)
{
	glClear(  GL_DEPTH_BUFFER_BIT );
	//Shouldn't be using a temporary camera.
	//temporary cameras are only active during movement operations
	ASSERT(!tempCam);
	ASSERT(activeCam < cameras.size());
	
	
	// Need to load a base name so that the other calls can replace it
	GLuint *selectionBuffer = new GLuint[512];
	glSelectBuffer(512, selectionBuffer);
	glRenderMode(GL_SELECT);
	glInitNames();
	
	if(!boundCube.isValid())
		computeSceneLimits();

	glPushMatrix();
	//Apply the camera, but do NOT load the identity matrix, as
	//we have set the pick matrix
	cameras[activeCam]->apply(outWinAspect,boundCube,false);

	//Set up the objects. Only NON DISPLAYLIST items can be selected.
	for(unsigned int ui=0; ui<objects.size(); ui++)
	{
		glPushName(ui);
		if(objects[ui]->canSelect)
			objects[ui]->draw();
		glPopName();
	}

	//OpengGL Faq:
	//The number of hit records is returned by the call to
	//glRenderMode(GL_RENDER). Each hit record contains the following
	//information stored as unsigned ints:
	//
	// * Number of names in the name stack for this hit record
	// * Minimum depth value of primitives (range 0 to 2^32-1)
	// * Maximum depth value of primitives (range 0 to 2^32-1)
	// * Name stack contents (one name for each unsigned int).
	//
	//You can use the minimum and maximum Z values with the device
	//coordinate X and Y if known (perhaps from a mouse click)
	//to determine an object coordinate location of the picked
	//primitive. You can scale the Z values to the range 0.0 to 1.0,
	//for example, and use them in a call to gluUnProject().
	glFlush();
	GLint hits = glRenderMode(GL_RENDER);
	
	//The hit query records are stored in an odd manner
	//as the name stack is returned with it. This depends
	//upon how you have constructed your name stack during drawing
	//(clearly).  I didn't bother fully understanding this, as it does
	//what I want.
	GLuint *ptr = selectionBuffer;
	GLuint *closestNames = 0;
	GLuint minZ = 0xffffffff;
	GLuint numClosestNames = 0;
	for ( int i=0; i<hits; ++i )
	{
		if ( *(ptr+1) < minZ )
		{
			numClosestNames = *ptr;
			minZ = *(ptr+1);
			closestNames = ptr+3;
		}
		ptr+=*ptr+3;
	}


	//Record the nearest item
	// There should only be one item on the hit stack for the closest item.
	GLuint closest;
	if(numClosestNames==1)
		closest=closestNames[0]; 
	else 
		closest=(unsigned int)(-1);

	delete[] selectionBuffer;

	glPopMatrix();
	
	//Record the last item if required.
	if(storeSelected)
	{
		lastSelected=closest;
		return lastSelected;
	}
	else
		return closest;

}

void Scene::finaliseCam()
{
	switch(cameras[activeCam]->type())
	{
		case CAM_LOOKAT:
			((CameraLookAt *)cameras[activeCam])->recomputeUpDirection();
			break;
	}
}

void Scene::addSelectionDevices(const vector<SelectionDevice<Filter> *> &d)
{
	for(unsigned int ui=0;ui<d.size();ui++)
	{
#ifdef DEBUG
		//Ensure that pointers coming in are valid, by attempting to perform an operation on them
		vector<std::pair<const Filter *,SelectionBinding> > tmp;
		d[ui]->getModifiedBindings(tmp);
		tmp.clear();
#endif
		selectionDevices.push_back(d[ui]);
	}
}

//Values are in the range [0 1].
void Scene::applyDevice(float startX, float startY, float curX, float curY,
			unsigned int keyFlags, unsigned int mouseFlags,	bool permanent)
{

	if(lastSelected == (unsigned int) (-1))
		return;


	//Object should be in object array, and be selectable
	ASSERT(lastSelected < objects.size())
	ASSERT(objects[lastSelected]->canSelect);

	//Grab basis vectors. (up, fowards and 
	//across from camera view.)
	//---
	Point3D forwardsDir,upDir;
	
	forwardsDir=getActiveCam()->getViewDirection();
	upDir=getActiveCam()->getUpDirection();

	forwardsDir.normalise();
	upDir.normalise();
	Point3D acrossDir;
	acrossDir=forwardsDir.crossProd(upDir);

	acrossDir.normalise();
	//---

	//Compute the distance between the selected object's
	//centroid and the camera
	//---
	float depth;
	BoundCube b;
	objects[lastSelected]->getBoundingBox(b);
	//Camera-> object vector
	Camera *cam=getActiveCam();

	Point3D camToObject;
	//Get the vector to the object	
	camToObject = b.getCentroid() - cam->getOrigin();
	depth = camToObject.dotProd(forwardsDir);
	//---

	//Compute the width of the camera view for the object at 
	//the plane that intersects the object's centroid, and is 
	//normal to the camera direction
	float viewWidth;
	switch(cam->type())
	{
		case CAM_LOOKAT:
			viewWidth=((CameraLookAt*)cam)->getViewWidth(depth);
			break;
		default:
			ASSERT(false);
	}


	//We have the object number, but we don't know which binding
	//corresponds to this object. Search all bindings. It may be that more than one 
	//binding is enabled for this object
	SelectionBinding *binder;

	vector<SelectionBinding*> activeBindings;
	for(unsigned int ui=0;ui<selectionDevices.size();ui++)
	{
		if(selectionDevices[ui]->getBinding(
			objects[lastSelected],mouseFlags,keyFlags,binder))
			activeBindings.push_back(binder);
	}

	for(unsigned int ui=0;ui<activeBindings.size();ui++)
	{
		//Convert the mouse-XY coords into a world coordinate, depending upon mouse/
		//key cobinations
		Point3D worldVec;
		Point3D vectorCoeffs[2];
		activeBindings[ui]->computeWorldVectorCoeffs(mouseFlags,keyFlags,
					vectorCoeffs[0],vectorCoeffs[1]);	

		//Apply vector coeffs, dependant upon binding
		worldVec = acrossDir*vectorCoeffs[0][0]*(curX-startX)*outWinAspect
				+ upDir*vectorCoeffs[0][1]*(curX-startX)*outWinAspect
				+ forwardsDir*vectorCoeffs[0][2]*(curX-startX)*outWinAspect
				+ acrossDir*vectorCoeffs[1][0]*(curY-startY)
				+ upDir*vectorCoeffs[1][1]*(curY-startY)
				+ forwardsDir*vectorCoeffs[1][2]*(curY-startY);
		worldVec*=viewWidth;
		
		activeBindings[ui]->applyTransform(worldVec,permanent);
	}

	computeSceneLimits();
	//Inform viscontrol about updates, if we have applied any
	if(activeBindings.size() && permanent)
	{
		visControl->setUpdates();
		//If the viscontrol is in the midddle of an update,
		//tell it to abort.
		if(visControl->isRefreshing())
			visControl->abort();
	}

}

unsigned int Scene::duplicateCameras(vector<Camera *> &cams) const
{
	cams.resize(cameras.size());

	for(unsigned int ui=0;ui<cameras.size();ui++)	
		cams[ui]=cameras[ui]->clone();

	return activeCam;
}	

void Scene::getEffects(vector<const Effect *> &eff) const
{
	eff.resize(effects.size());

	for(unsigned int ui=0;ui<effects.size();ui++)	
		eff[ui]=effects[ui];
}	

unsigned int Scene::getActiveCamId() const
{
	return camIDs.getId(activeCam);
}


void Scene::getModifiedBindings(std::vector<std::pair<const Filter *, SelectionBinding> > &bindings) const
{
	for(unsigned int ui=0;ui<selectionDevices.size();ui++)
		selectionDevices[ui]->getModifiedBindings(bindings);
}

void Scene::restrictView(float xS, float yS, float xFin, float yFin)
{
	viewRestrictStart[0]=xS;	
	viewRestrictStart[1]=yS;	
	
	viewRestrictEnd[0]=xFin;	
	viewRestrictEnd[1]=yFin;

	viewRestrict=true;	
}

bool Scene::isDefaultCam() const
{
	return  activeCam==0;
}

bool Scene::camNameExists(const std::string &s) const
{
	for(unsigned int ui=0;ui<cameras.size() ;ui++)
	{
		if(cameras[ui]->getUserString() == s)
			return true;
	}

	return false;
}



unsigned int Scene::addEffect(Effect *e)
{
	ASSERT(e);
	ASSERT(effects.size() == effectIDs.size());
	effects.push_back(e);
	

	return effectIDs.genId(effects.size()-1);
}


void Scene::removeEffect(unsigned int uniqueID)
{
	unsigned int position = effectIDs.getPos(uniqueID);
	delete effects[position];
	effects.erase(effects.begin()+position);
	effectIDs.killByPos(position);
}

void Scene::clearEffects()
{
	for(size_t ui=0;ui<effects.size();ui++)
		delete effects[ui];

	effects.clear();
	effectIDs.clear();
}
