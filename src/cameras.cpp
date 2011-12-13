/*
 *	cameras.cpp - opengl camera implementations
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

#include "cameras.h"
#include "mathfuncs.h"
#include "xmlHelper.h"

#include "commonConstants.h"

//MacOS is "special" and puts it elsewhere
#ifdef __APPLE__
	#include <OpenGL/glu.h>
#else
	#include <GL/glu.h>
#endif
#include "translation.h"

#include <limits>
#include <vector>
#include <utility>

using std::string;
using std::vector;
using std::pair;

//TODO: FIXME: Orthogonal camera zooming is very slow, compared to 
// perspective camera dolly. Check equations of motion for equivalence
const float ORTHO_SPEED_HACK=1.05;


//!Key types for property setting and getting via property grids
enum
{
	KEY_LOOKAT_LOCK,
	KEY_LOOKAT_ORIGIN,
	KEY_LOOKAT_TARGET,
	KEY_LOOKAT_UPDIRECTION,
	KEY_LOOKAT_FOV,
	KEY_LOOKAT_PROJECTIONMODE,
	KEY_LOOKAT_ORTHOSCALE
};



Camera::Camera() : origin(0.0f,0.0f,0.0f), viewDirection(0.0f,0.0f,-1.0f), upDirection(0.0f,0.0f,1.0f)
{
	lock=false;
}

Camera::~Camera()
{
}


Point3D Camera::getOrigin() const
{
	return origin;
}

Point3D Camera::getViewDirection() const
{
	return viewDirection;
}

Point3D Camera::getUpDirection() const
{
	return upDirection;
}

void Camera::setOrigin(const Point3D &pt)
{
	if(lock)
		return;
	origin=pt;
}

void Camera::setViewDirection(const Point3D &pt)
{
	if(lock)
		return;
	viewDirection=pt;
	viewDirection.normalise();
}

void Camera::setUpDirection(const Point3D &pt)
{
	if(lock)
		return;
	upDirection = pt;
	upDirection.normalise();
}

void Camera::forwardsDolly(float moveRate)
{
	if(lock)
		return;
	origin=origin+ viewDirection*moveRate;
}

void Camera::move(float moveLR, float moveUD)
{
	if(lock)
		return;
	//Right is the cross product of up and 
	//view direction (check sign)
	//Up is simply the up vector
	origin+=upDirection*moveUD + (upDirection.crossProd(viewDirection))*moveLR;
}

void Camera::translate(float moveLR, float moveUD)
{
	if(lock)
		return;
	//This camera has no target. Just do plain move
	move(moveLR,moveUD);
}

void Camera::pivot(float lrRad, float udRad)
{
	if(lock)
		return;
	Point3f viewNew, rotateAxis;

	//rotate normalised rOrig around axis one then two
	viewNew.fx=viewDirection[0]; 
	viewNew.fy=viewDirection[1]; 
	viewNew.fz=viewDirection[2]; 

	//rotate around "right" axis
	Point3D tmp = upDirection.crossProd(viewDirection);
	rotateAxis.fx=tmp[0];
	rotateAxis.fy=tmp[1];
	rotateAxis.fz=tmp[2];
	quat_rot(&viewNew,&rotateAxis,udRad);

	//rotate around original "up" axis
	rotateAxis.fx=upDirection[0];
	rotateAxis.fy=upDirection[1];
	rotateAxis.fz=upDirection[2];
	quat_rot(&viewNew,&rotateAxis,lrRad);

	viewDirection[0] = rotateAxis.fx;
	viewDirection[1] = rotateAxis.fy;
	viewDirection[2] = rotateAxis.fz;

}

//=====


//=====

CameraLookAt::CameraLookAt() 
{
	target=Point3D(0.0f,0.0f,0.0f);
	origin=Point3D(0.0f,0.0f,1.0f);
	viewDirection=Point3D(0.0f,0.0f,-1.0f);
	upDirection=Point3D(0.0f,1.0f,0.0f);
	typeNum=CAM_LOOKAT;
	projectionMode=PROJECTION_MODE_PERSPECTIVE;
	fovAngle=90.0f;
	nearPlane=1.0f;
	frustumDistortion=0.0f;

	orthoScale=1.0f;
}

Camera *CameraLookAt::clone() const
{
	CameraLookAt *retCam = new CameraLookAt;

	retCam->origin =origin;
	retCam->viewDirection=viewDirection;
	retCam->upDirection =upDirection;
	retCam->projectionMode=projectionMode;
	retCam->orthoScale=orthoScale;
	retCam->typeNum=typeNum;
	retCam->userString=userString;
	retCam->lock=lock;

	retCam->target = target;
	retCam->fovAngle = fovAngle;
	retCam->nearPlane=nearPlane;
	retCam->farPlane=farPlane;


	return retCam;
}

CameraLookAt::~CameraLookAt()
{
}

void CameraLookAt::setTarget(const Point3D &pt)
{
	ASSERT(pt.sqrDist(origin)>10.0f*std::numeric_limits<float>::epsilon());
	target=pt;
	recomputeViewDirection();
}

Point3D CameraLookAt::getTarget() const
{
	return target;
}

void CameraLookAt::doPerspCalcs(float aspectRatio, const BoundCube &bc,bool loadIdentity) const
{
	ASSERT(projectionMode == PROJECTION_MODE_PERSPECTIVE);

	glMatrixMode (GL_PROJECTION);
	if(loadIdentity)
		glLoadIdentity();

	
	farPlane = 1.5*bc.getMaxDistanceToBox(origin);
	gluPerspective(fovAngle/2.0,aspectRatio,nearPlane,farPlane);
	glMatrixMode(GL_MODELVIEW);

	glTranslatef(origin[0],origin[1],origin[2]);

	//As gluperspective defaults to viewing down the 0,0,-1 axis
	//we must rotate the scene such that the view direction
	//is brought into line with the default view vector
	Point3D rotVec,defaultDir(0,0,-1.0f);
	float rotAngle;


	rotVec = viewDirection.crossProd(defaultDir);


	rotAngle = 180.0f/M_PI * viewDirection.angle(defaultDir);

	glRotatef(-rotAngle,rotVec[0],rotVec[1],rotVec[2]);

	//Camera roll
	//Default "up" direction
	defaultDir[2] = 1.0f;
	rotVec = upDirection.crossProd(defaultDir);
	rotAngle = 180.0f/M_PI * upDirection.angle(defaultDir);
	glRotatef(-rotAngle, rotVec[0], rotVec[1],rotVec[2]);

}

void CameraLookAt::setOrigin(const Point3D &newOrigin)
{
	if(lock)
		return;
	ASSERT(newOrigin.sqrDist(target)>std::numeric_limits<float>::epsilon());
	origin=newOrigin;
	recomputeViewDirection();
}

void CameraLookAt::apply(float aspect, const BoundCube &bc, bool loadIdentity) const
{

	glMatrixMode (GL_PROJECTION);
	if(loadIdentity)
		glLoadIdentity();
	
	farPlane = 1.5*bc.getMaxDistanceToBox(origin);
	switch(projectionMode)
	{

		case PROJECTION_MODE_PERSPECTIVE:
		{

			gluPerspective(fovAngle/2.0,aspect,nearPlane,farPlane);
			glMatrixMode(GL_MODELVIEW);
			break;

		}
		case PROJECTION_MODE_ORTHOGONAL:
		{
			glOrtho(-orthoScale*aspect,orthoScale*aspect,-orthoScale,orthoScale,nearPlane,farPlane);
			glMatrixMode(GL_MODELVIEW);
			break;
		}
		default:
			ASSERT(false);

	}

	ASSERT(origin.sqrDist(target)>std::numeric_limits<float>::epsilon());
	gluLookAt(origin[0],origin[1],origin[2],
				target[0],target[1],target[2],
				upDirection[0],upDirection[1],upDirection[2]);
}

void CameraLookAt::apply(float aspect,const BoundCube &b,bool loadIdentity,
						float leftRestrict,float rightRestrict, 
						float bottomRestrict,float topRestrict) const
{
	ASSERT(leftRestrict < rightRestrict);
	ASSERT(bottomRestrict< topRestrict);

	glMatrixMode (GL_PROJECTION);
	if(loadIdentity)
		glLoadIdentity();

	farPlane = 1.5*b.getMaxDistanceToBox(origin);
	switch(projectionMode)
	{

		case PROJECTION_MODE_PERSPECTIVE:
		{
			float width,height;
			height = tan(fovAngle/2.0*M_PI/180)*nearPlane;
			width= height*aspect;
			

			//Frustum uses eye coordinates.	
			if(fabs(frustumDistortion) < std::numeric_limits<float>::epsilon())
				glFrustum(leftRestrict*width,rightRestrict*width,bottomRestrict*height,
								topRestrict*height,nearPlane,farPlane);
			else
			{
				float workingDist=farPlane;//sqrtf((target-origin).sqrMag());
				glFrustum(leftRestrict*width+frustumDistortion*nearPlane/workingDist,
						rightRestrict*width+frustumDistortion*nearPlane/workingDist,
						bottomRestrict*height,	topRestrict*height,nearPlane,farPlane);
			}
			break;
		}
		case PROJECTION_MODE_ORTHOGONAL:
		{
			float l,r,b,t;

			//FIXME:I have no idea why it is 2x...AFAIK it should just be ONE.
			//but this works, and one does not.
			l = 2*leftRestrict*orthoScale*aspect;
			r= 2*rightRestrict*orthoScale*aspect;
			b= 2*bottomRestrict*orthoScale;
			t = 2*topRestrict*orthoScale;

			glOrtho(l,r,b,t,nearPlane,farPlane);
			break;
		}
		default:
			ASSERT(false);
	}
	
	glMatrixMode(GL_MODELVIEW);

	ASSERT(origin.sqrDist(target)>std::numeric_limits<float>::epsilon());
	gluLookAt(origin[0],origin[1],origin[2],
				target[0],target[1],target[2],
				upDirection[0],upDirection[1],upDirection[2]);
	
}

void CameraLookAt::translate(float moveLR, float moveUD)
{
	if(lock)
		return;
	float fovMultiplier=1.0f;
	if(projectionMode== PROJECTION_MODE_PERSPECTIVE)
	{

		//Try to move such that the target sweeps our field of view
		//at a constant rate. Standard normaliser is view length at
		//a 90* camera
		//Use tan.. to normalise motion rate 
		//Prevent numerical error near tan( 90*)
		if(fovAngle < 175.0f)
			fovMultiplier = tan(fovAngle/2.0*M_PI/180.0);
		else
			fovMultiplier = tan(175.0f/2.0*M_PI/180.0);
	}
	

	moveLR=moveLR*sqrtf(target.sqrDist(origin)*fovMultiplier);
	moveUD=moveUD*sqrtf(target.sqrDist(origin)*fovMultiplier);

	origin+=upDirection*moveUD + (upDirection.crossProd(viewDirection))*moveLR;
	target+=upDirection*moveUD + (upDirection.crossProd(viewDirection))*moveLR;
}

void CameraLookAt::forwardsDolly(float moveRate)
{
	if(lock)
		return;

	if(projectionMode == PROJECTION_MODE_PERSPECTIVE)
	{
		Point3D newOrigin;

		//Prevent camera orientation inversion, which occurs when moving past the target
		if(moveRate > sqrt(target.sqrDist(origin)))
		{
			if((target-origin).sqrMag() < sqrtf(std::numeric_limits<float>::epsilon()))
					return;

			//Yes, this simplifies analytically. However i think the numerics come into play.
			float moveInv = 1.0/(fabs(moveRate) + std::numeric_limits<float>::epsilon());
			newOrigin=origin+viewDirection*moveInv/(1.0+moveInv);

		}
		else
		{
			//scale moverate by orbit distance
			moveRate = moveRate*sqrtf(target.sqrDist(origin));
			newOrigin=origin+viewDirection*moveRate;
		}

		//Only accept origin change if it is sufficiently far from the target
		if(newOrigin.sqrDist(target)>sqrtf(std::numeric_limits<float>::epsilon()))
			origin=newOrigin;
	}
	else
	{
		float deltaSqr;
		deltaSqr = (target-origin).sqrMag();
		if(deltaSqr< sqrtf(std::numeric_limits<float>::epsilon()))
			return;

		Point3D virtualOrigin;
		virtualOrigin = origin+viewDirection*moveRate;

		float factor;
		factor = virtualOrigin.sqrDist(target)/deltaSqr;
		if( factor > 1.0)
			factor*=ORTHO_SPEED_HACK;
		else
			factor/=ORTHO_SPEED_HACK;

		orthoScale*=factor;
	}
}


//Clockwise roll looking from camera view by rollRad radians
void CameraLookAt::roll(float rollRad)
{
	if(lock)
		return;
	Point3f rNew,rotateAxis;

	rotateAxis.fx=viewDirection[0];
	rotateAxis.fy=viewDirection[1];
	rotateAxis.fz=viewDirection[2];

	rNew.fx=upDirection[0];
	rNew.fy=upDirection[1];
	rNew.fz=upDirection[2];
	quat_rot(&rNew,&rotateAxis,rollRad);

	upDirection=Point3D(rNew.fx,rNew.fy,rNew.fz);
	recomputeUpDirection();
}

void CameraLookAt::pivot(float leftRightRad,float updownRad)
{
	if(lock)
		return;

	Point3f rNew,rotateAxis;
	Point3D tmp;
	//rotate normalised rOrig around axis one then two
	tmp=target-origin;
	rNew.fx=tmp[0]; 
	rNew.fy=tmp[1]; 
	rNew.fz=tmp[2]; 

	//rotate around "right" axis
	tmp = upDirection.crossProd(viewDirection);
	tmp.normalise();
	rotateAxis.fx=tmp[0];
	rotateAxis.fy=tmp[1];
	rotateAxis.fz=tmp[2];
	quat_rot(&rNew,&rotateAxis,updownRad);

	Point3D newDir;
	newDir=Point3D(rNew.fx,rNew.fy,rNew.fz)+origin;

	//rotate around original "up" axis
	rotateAxis.fx=upDirection[0];
	rotateAxis.fy=upDirection[1];
	rotateAxis.fz=upDirection[2];
	quat_rot(&rNew,&rotateAxis,leftRightRad);

	newDir+= Point3D(rNew.fx,rNew.fy,rNew.fz);
	target = target+newDir;
	target.normalise(); 
	target*=sqrtf((target).sqrDist(origin));

	recomputeViewDirection();
	recomputeUpDirection();
}

//Make a given bounding box visible, as much as possible
void CameraLookAt::ensureVisible(const BoundCube &boundCube, unsigned int face)
{
	if(lock)
		return;
	//Face is defined by the following net
	//	0
	//  1   2   3
	//  	4
	//  	5
	//2 is the face directed to the +ve x axis,
	//with the "up"" vector on the 3 aligned to z,
	//so "0" is parallel to the Z axis and is "visible"
	//from the top +ve side of the z axis (at sufficient distance)
	
	//To make the camera visible, we must place the camera
	//outside the box, on the correct face,
	//at sufficient distance to ensure that the face closest
	//to the box is visible at the current FOV.
	
       	//Box centroid
	Point3D boxCentroid = boundCube.getCentroid();

	//Vector from box face to camera
	Point3D faceOutVector, tmpUpVec;

	//I labelled a physical box to work this table out.
	float boxToFrontDist,faceSize[2];
	switch(face)
	{
		case 0:
			faceOutVector = Point3D(0,0,1); 
			boxToFrontDist=boundCube.getSize(2);
			tmpUpVec = Point3D(0,1,0);
			faceSize[0]=boundCube.getSize(0);
			faceSize[1]=boundCube.getSize(1);
			break;
		case 1:
			faceOutVector = Point3D(0,-1,0); 
			boxToFrontDist=boundCube.getSize(1);
			tmpUpVec = Point3D(1,0,0);
			faceSize[0]=boundCube.getSize(1);
			faceSize[1]=boundCube.getSize(0);
			break;
		case 2:
			faceOutVector = Point3D(0,1,0); 
			boxToFrontDist=boundCube.getSize(1);
			tmpUpVec =Point3D(1,0,0);
			faceSize[0]=boundCube.getSize(0);
			faceSize[1]=boundCube.getSize(2);
			break;
		case 3:
			faceOutVector = Point3D(1,0,0); 
			boxToFrontDist=boundCube.getSize(0);
			tmpUpVec = Point3D(0,0,1);
			faceSize[0]=boundCube.getSize(1);
			faceSize[1]=boundCube.getSize(2);
			break;
		case 4:
			faceOutVector = Point3D(0,0,-1); 
			boxToFrontDist=boundCube.getSize(2);
			tmpUpVec = Point3D(0,1,0);
			faceSize[0]=boundCube.getSize(0);
			faceSize[1]=boundCube.getSize(1);
			break;
		case 5:
			faceOutVector = Point3D(-1,0,0); 
			boxToFrontDist=boundCube.getSize(0);
			tmpUpVec = Point3D(0,0,1);
			faceSize[0]=boundCube.getSize(1);
			faceSize[1]=boundCube.getSize(2);
			break;
		default:
			ASSERT(false);
	}	


	//Convert box to front distance to vector from
	//centroid to front face.
	boxToFrontDist/=2.0f;
	float halfMaxFaceDim=std::max(faceSize[0],faceSize[1])/2.0;


	ASSERT(fovAngle > 0);

	//Set camera target to inside box
	target=boxCentroid;

	float outDistance;
	if(projectionMode == PROJECTION_MODE_PERSPECTIVE)
	{
		//Minimal camera distance is given trigonometrically. 
		//Add aditional 1 to ensure that nearplane does not clip object
		outDistance=1.0+boxToFrontDist+halfMaxFaceDim/tan((fovAngle*M_PI/180)/2.0f);
	}
	else
	{
		outDistance=boxToFrontDist+halfMaxFaceDim;
	}

	//Multiply by 1.4 to give a bit of border.
	origin=boxCentroid+faceOutVector*1.4*outDistance;

	orthoScale = sqrtf(target.sqrDist(origin))/2.0;


	//Set the default up direction
	upDirection=tmpUpVec;

	//Reset the view direction
	recomputeViewDirection();
	//Ensure up direction orthogonal
	recomputeUpDirection();
	nearPlane = 1;
}

void CameraLookAt::recomputeViewDirection()
{
	viewDirection=origin-target;
	viewDirection.normalise();
}
void CameraLookAt::recomputeUpDirection()
{
	//Use cross product of view and up to generate an across vector.
	Point3D across;
	upDirection.normalise();
	across = viewDirection.crossProd(upDirection);
	across.normalise();

	//Regenerate up vector by reversing the cross with a normalised across vector 
	upDirection = across.crossProd(viewDirection);

	upDirection.normalise();
}


void CameraLookAt::move(float moveLRAngle, float moveUDAngle)
{
	if(lock)
		return;

	//Think of the camera as moving around the surface of a sphere
	Point3f curOrig;
	curOrig.fx = origin[0] - target[0];
	curOrig.fy = origin[1] - target[1];
	curOrig.fz = origin[2] - target[2];

	//Perform "up" rotation
	Point3D rotateAxis;

	rotateAxis=upDirection;
	Point3f r,u;
	r.fx = rotateAxis[0];
	r.fy = rotateAxis[1];
	r.fz = rotateAxis[2];

	u.fx = upDirection[0];
	u.fy = upDirection[1];
	u.fz = 	upDirection[2];

	//Perform quaternion rotation around this aixs
	quat_rot(&curOrig,&r, moveLRAngle);
	quat_rot(&curOrig,&u, moveLRAngle);

	recomputeViewDirection();
	//Perform across rotation
	rotateAxis  =upDirection.crossProd(viewDirection).normalise();
	r.fx = rotateAxis[0];
	r.fy = rotateAxis[1];
	r.fz = rotateAxis[2];
	quat_rot(&curOrig,&r, moveUDAngle);
	//Get transformed coordinates
	origin[0] = target[0] + curOrig.fx;
	origin[1] = target[1] + curOrig.fy;
	origin[2] = target[2] + curOrig.fz;
	recomputeViewDirection();
}

void CameraLookAt::getProperties(CameraProperties &p) const
{
	p.data.clear();
	p.types.clear();
	p.keys.clear();

	vector<pair<string,string> > s;
	vector<unsigned int> type,keys;

	if(lock)
		s.push_back(std::make_pair(TRANS("Lock"),"1"));
	else
		s.push_back(std::make_pair(TRANS("Lock"),"0"));

	type.push_back(PROPERTY_TYPE_BOOL);
	keys.push_back(KEY_LOOKAT_LOCK);

	string ptStr;
	stream_cast(ptStr,origin);
	s.push_back(std::make_pair(TRANS("Origin"), ptStr));
	type.push_back(PROPERTY_TYPE_POINT3D);
	keys.push_back(KEY_LOOKAT_ORIGIN);
	
	stream_cast(ptStr,target);
	s.push_back(std::make_pair(TRANS("Target"), ptStr));
	type.push_back(PROPERTY_TYPE_POINT3D);
	keys.push_back(KEY_LOOKAT_TARGET);
	
	stream_cast(ptStr,upDirection);
	s.push_back(std::make_pair(TRANS("Up Dir."), ptStr));
	type.push_back(PROPERTY_TYPE_POINT3D);
	keys.push_back(KEY_LOOKAT_UPDIRECTION);

	vector<pair<unsigned int,string> > choices;
	string tmp;
	

	tmp=TRANS("Perspective");
	choices.push_back(make_pair((unsigned int)PROJECTION_MODE_PERSPECTIVE,tmp));
	tmp=TRANS("Orthogonal");
	choices.push_back(make_pair((unsigned int)PROJECTION_MODE_ORTHOGONAL,tmp));
	tmp= choiceString(choices,projectionMode);
	
	s.push_back(std::make_pair("Projection", tmp));
	type.push_back(PROPERTY_TYPE_CHOICE);
	keys.push_back(KEY_LOOKAT_PROJECTIONMODE);

	switch(projectionMode)
	{
		case PROJECTION_MODE_PERSPECTIVE:
			stream_cast(tmp,fovAngle);
			s.push_back(std::make_pair(TRANS("Field of View (deg)"), tmp));
			type.push_back(PROPERTY_TYPE_REAL);
			keys.push_back(KEY_LOOKAT_FOV);
			break;
		case PROJECTION_MODE_ORTHOGONAL:
			stream_cast(tmp,orthoScale);
			s.push_back(std::make_pair(TRANS("View size"), tmp));
			type.push_back(PROPERTY_TYPE_REAL);
			keys.push_back(KEY_LOOKAT_ORTHOSCALE);
			break;

	}

	p.data.push_back(s);
	p.keys.push_back(keys);
	p.types.push_back(type);
}

bool CameraLookAt::setProperty(unsigned int key, const string &value)
{

	switch(key)
	{
		case KEY_LOOKAT_LOCK:
		{
			if(value == "1")
				lock=true;
			else if (value == "0")
				lock=false;
			else
				return false;

			break;
		}
		case KEY_LOOKAT_ORIGIN:
		{
			Point3D newPt;
			if(!parsePointStr(value,newPt))
				return false;

			//Disallow origin to be set to same as target
			if(newPt.sqrDist(target) < sqrtf(std::numeric_limits<float>::epsilon()))
				return false;

			origin= newPt;

			break;
		}
		case KEY_LOOKAT_TARGET:
		{
			Point3D newPt;
			if(!parsePointStr(value,newPt))
				return false;

			//Disallow origin to be set to same as target
			if(newPt.sqrDist(origin) < sqrtf(std::numeric_limits<float>::epsilon()))
				return false;
			target = newPt;

			break;
		}
		case KEY_LOOKAT_UPDIRECTION:
		{
			Point3D newDir;
			if(!parsePointStr(value,newDir))
				return false;

			//View direction and up direction may not be the same
			if(viewDirection.crossProd(newDir).sqrMag() < 
				sqrtf(std::numeric_limits<float>::epsilon()))
				return false;

			upDirection=newDir;
			//Internal up direction should be perp. to view direction.
			//use double cross product method to restore
			recomputeUpDirection();
			break;
		}
		case KEY_LOOKAT_FOV:
		{
			float newFOV;
			if(stream_cast(newFOV,value))
				return false;

			fovAngle=newFOV;
			break;
		}
		case KEY_LOOKAT_PROJECTIONMODE:
		{
			size_t ltmp;
			if(value == TRANS("Perspective"))
				ltmp=PROJECTION_MODE_PERSPECTIVE;
			else if( value == TRANS("Orthogonal"))
			{
				if(projectionMode!=PROJECTION_MODE_ORTHOGONAL)
				{
					//use the distance to the target as the orthographic 
					//scaling size (size of parallel frustrum)
					orthoScale=sqrtf(target.sqrDist(origin));
				}

				ltmp=PROJECTION_MODE_ORTHOGONAL;

			}
			else
			{
				ASSERT(false);
				return false;
			}
			
			if(ltmp>=PROJECTION_MODE_ENUM_END)
				return false;
			
			projectionMode=ltmp;
			
			break;
		}
		case KEY_LOOKAT_ORTHOSCALE:
		{
			float newOrthoScale;
			if(stream_cast(newOrthoScale,value))
				return false;

			orthoScale=newOrthoScale;
			break;
		}


		default:
			ASSERT(false);
	}
	return true;
}

bool CameraLookAt::writeState(std::ostream &f, unsigned int format,
	       					unsigned int nTabs) const
{
	switch(format)
	{
		case STATE_FORMAT_XML:
		{
			using std::endl;

			f << tabs(nTabs) << "<persplookat>" << endl;
			ASSERT(userString.size());
			f << tabs(nTabs+1) << "<userstring value=\"" << userString << "\"/>" << endl;
			f << tabs(nTabs+1) << "<projectionmode value=\""<< projectionMode << "\"/>" << endl;
			f << tabs(nTabs+1) << "<orthoscale value=\""<< orthoScale << "\"/>" << endl;
			
			if(lock)	
				f<< tabs(nTabs+1) <<  "<lock value=\"1\"/>" << endl;
			else
				f<< tabs(nTabs+1) <<  "<lock value=\"0\"/>" << endl;
			f << tabs(nTabs+1) << "<origin x=\"" << origin[0] << "\" y=\"" << origin[1] <<
				"\" z=\"" << origin[2] << "\"/>" << endl;
			f << tabs(nTabs+1) << "<target x=\"" << target[0] << "\" y=\"" << target[1] <<
				"\" z=\"" << target[2] << "\"/>" << endl;
			f << tabs(nTabs+1) << "<updirection x=\"" << upDirection[0] << "\" y=\"" << upDirection[1] <<
				"\" z=\"" << upDirection[2] << "\"/>" << endl;

			f<< tabs(nTabs+1) <<  "<fovangle value=\"" << fovAngle << "\"/>" << endl;
			f<< tabs(nTabs+1) <<  "<nearplane value=\"" << nearPlane << "\"/>" << endl;
			f << tabs(nTabs) << "</persplookat>" << endl;

			return true;
		}
		default:
			ASSERT(false);
	}
}
		
bool CameraLookAt::readState(xmlNodePtr nodePtr)
{
	//Retrieve user string
	if(!XMLGetNextElemAttrib(nodePtr,userString,"userstring","value"))
		return false;
	if(!userString.size())
		return false;

	std::string tmpStr;

	//Retrieve projection mode
	if(!XMLGetNextElemAttrib(nodePtr,projectionMode,"projectionmode","value"))
		return false;
	if(projectionMode > PROJECTION_MODE_ENUM_END)
		return false;

	//Retrieve orthographic scaling
	if(!XMLGetNextElemAttrib(nodePtr,orthoScale,"orthoscale","value"))
		return false;
	if(orthoScale <=0 || std::isnan(orthoScale))
		return false;

	
	float x,y,z;
	xmlChar *xmlString;


	//retrieve lock state
	if(XMLHelpFwdToElem(nodePtr,"lock"))
		return false;
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");

	std::string strTmp;
	strTmp=(char*)xmlString;
	if(strTmp == "1")
		lock=true;
	else if(strTmp == "0")
		lock=false;
	else
		return false;

	//Retrieve origin
	//====
	if(XMLHelpFwdToElem(nodePtr,"origin"))
		return false;
	//--Get X value--
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"x");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;
	xmlFree(xmlString);

	//Check it is streamable
	if(stream_cast(x,tmpStr))
		return false;

	//--Get Z value--
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"y");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;
	xmlFree(xmlString);

	//Check it is streamable
	if(stream_cast(y,tmpStr))
		return false;

	//--Get Y value--
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"z");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;
	xmlFree(xmlString);

	//Check it is streamable
	if(stream_cast(z,tmpStr))
		return false;

	origin=Point3D(x,y,z);
	//====

	//Retrieve target 
	//====
	if(XMLHelpFwdToElem(nodePtr,"target"))
		return false;
	//--Get X value--
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"x");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;
	xmlFree(xmlString);

	//Check it is streamable
	if(stream_cast(x,tmpStr))
		return false;

	//--Get Z value--
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"y");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;
	xmlFree(xmlString);

	//Check it is streamable
	if(stream_cast(y,tmpStr))
		return false;

	//--Get Y value--
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"z");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;
	xmlFree(xmlString);

	//Check it is streamable
	if(stream_cast(z,tmpStr))
		return false;
	target=Point3D(x,y,z);
	//====
	
	//Retrieve up direction
	//====
	if(XMLHelpFwdToElem(nodePtr,"updirection"))
		return false;
	//--Get X value--
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"x");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;
	xmlFree(xmlString);

	//Check it is streamable
	if(stream_cast(x,tmpStr))
		return false;

	//--Get Z value--
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"y");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;
	xmlFree(xmlString);

	//Check it is streamable
	if(stream_cast(y,tmpStr))
		return false;

	//--Get Y value--
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"z");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;
	xmlFree(xmlString);

	//Check it is streamable
	if(stream_cast(z,tmpStr))
		return false;
	upDirection=Point3D(x,y,z);
	//====
	

	//Get the FOV angle
	//====
	if(!XMLGetNextElemAttrib(nodePtr,fovAngle,"fovangle","value"))
		return false;
	if(fovAngle<=0)
		return false;
	//====
	
	//Get the near plane
	//====
	if(!XMLGetNextElemAttrib(nodePtr,nearPlane,"nearplane","value"))
		return false;
	//====

	recomputeViewDirection();	
	return true;
}	

float CameraLookAt::getViewWidth(float depth) const
{
	if(projectionMode == PROJECTION_MODE_PERSPECTIVE)
		return depth*tan(fovAngle/2.0f*M_PI/180.0);
	else if(projectionMode == PROJECTION_MODE_ORTHOGONAL)
		return -orthoScale*2.0f; //FIXME: Why is this negative??!

	ASSERT(false);
}

std::ostream& operator<<(std::ostream &strm, const Camera &c)
{
	strm << "origin: " << c.origin << std::endl;
	strm << "View Direction: " << c.viewDirection << std::endl;	
	strm << "Up Direction: "<< c.upDirection << std::endl;	
	return strm;
}

std::ostream& operator<<(std::ostream &strm, const CameraLookAt &c)
{
	strm << "origin: " << c.origin << std::endl;
	strm << "Target : " << c.target << std::endl;

	strm << "View Direction: " << c.viewDirection << std::endl;	
	strm << "Up Direction: "<< c.upDirection << std::endl;


	strm << "FOV (deg) : " << c.fovAngle << std::endl;
	strm << "Clip planes: " << c.nearPlane << " (near) " << std::endl;
	return strm;
}



