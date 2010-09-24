/*
 *	cameras.cpp - opengl camera implementations
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
#include "quat.h"
#include "common.h"
#include "cameras.h"
#include "XMLHelper.h"

//MacOS is "special" and puts it elsewhere
#ifdef __APPLE__
	#include <OpenGL/glu.h>
#else
	#include <GL/glu.h>
#endif
#include <limits>
#include <vector>
#include <utility>

using std::string;
using std::vector;
using std::pair;


//!Key types for property setting and getting via property grids
enum
{
	KEY_PERSP_ORIGIN,
	KEY_PERSP_VIEWDIR,
	KEY_PERSP_UPDIR,
	KEY_PERSP_FOV,
	KEY_PERSPLOOKAT_ORIGIN,
	KEY_PERSPLOOKAT_TARGET,
	KEY_PERSPLOOKAT_UPDIRECTION,
	KEY_PERSPLOOKAT_FOV,
};



Camera::Camera() : origin(0.0f,0.0f,0.0f), viewDirection(0.0f,0.0f,-1.0f), upDirection(0.0f,0.0f,1.0f)
{
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
	origin=pt;
}

void Camera::setViewDirection(const Point3D &pt)
{
	viewDirection=pt;
	viewDirection.normalise();
}

void Camera::setUpDirection(const Point3D &pt)
{
	upDirection = pt;
	upDirection.normalise();
}

void Camera::forwardsDolly(float moveRate)
{
	origin=origin+ viewDirection*moveRate;
}

void Camera::move(float moveLR, float moveUD)
{
	//Right is the cross product of up and 
	//view direction (check sign)
	//Up is simply the up vector
	origin+=upDirection*moveUD + (upDirection.crossProd(viewDirection))*moveLR;
}

void Camera::translate(float moveLR, float moveUD)
{
	//This camera has no target. Just do plain move
	move(moveLR,moveUD);
}

void Camera::pivot(float lrRad, float udRad)
{
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

CameraPerspective::CameraPerspective() : fovAngle(90.0f), nearPlane(1)
{
	typeNum=CAM_PERSP;
}

CameraPerspective::~CameraPerspective()
{
}

Camera *CameraPerspective::clone() const
{
	CameraPerspective *retCam = new CameraPerspective;
	retCam->origin =origin;
	retCam->viewDirection=viewDirection;
	retCam->upDirection =upDirection;
	retCam->userString=userString;

	retCam->fovAngle = fovAngle;
	retCam->nearPlane=nearPlane;

	return retCam;
}

void CameraPerspective::apply(float aspectRatio, const BoundCube &bc,bool loadIdentity) const
{
	doPerspCalcs(aspectRatio,bc,loadIdentity);
}

void CameraPerspective::doPerspCalcs(float aspectRatio, const BoundCube &bc,bool loadIdentity) const
{
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

void CameraPerspective::doPerspCalcs(float aspectRatio, const BoundCube &bc,bool loadIdentity, float left, float top) const
{
	ASSERT(false);
}


//Make a given bounding box visible, as much as possible
void CameraPerspective::ensureVisible(const BoundCube &boundCube, unsigned int face)
{
	//FIXME: Implement me
	// -- what does it mean to ensure visible for a free camera??
	

	ASSERT(false);

}

void CameraPerspective::recomputeUpDirection()
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

void CameraPerspective::getProperties(CameraProperties &p) const
{
	p.data.clear();
	p.types.clear();
	p.keys.clear();

	vector<pair<string,string> > s;
	vector<unsigned int> type,keys;

	string ptStr;
	stream_cast(ptStr,origin);
	s.push_back(std::make_pair("Origin", ptStr));
	type.push_back(PROPERTY_TYPE_POINT3D);
	keys.push_back(KEY_PERSP_ORIGIN);
	
	stream_cast(ptStr,viewDirection);
	s.push_back(std::make_pair("View Dir.", ptStr));
	type.push_back(PROPERTY_TYPE_POINT3D);
	keys.push_back(KEY_PERSP_VIEWDIR);
	
	stream_cast(ptStr,upDirection);
	s.push_back(std::make_pair("Up Dir.", ptStr));
	type.push_back(PROPERTY_TYPE_POINT3D);
	keys.push_back(KEY_PERSP_UPDIR);


	string tmp;
	stream_cast(tmp,fovAngle);
	s.push_back(std::make_pair("Field of View (deg)", tmp));
	type.push_back(PROPERTY_TYPE_REAL);
	keys.push_back(KEY_PERSP_FOV);

	p.data.push_back(s);
	p.keys.push_back(keys);
	p.types.push_back(type);
}

bool CameraPerspective::setProperty(unsigned int key, const string &value)
{
	//TODO: IMPLEMENT ME
	return false;
}

bool CameraPerspective::writeState(std::ostream &f, unsigned int format,
	       					unsigned int nTabs) const
{
	using std::endl;
	
	f << tabs(nTabs) << "<perspective>" << endl;
	ASSERT(userString.size());
	f << tabs(nTabs+1) << "<userstring value=\"" << userString << "\"/>" << endl;
			
	f << tabs(nTabs+1) << "<origin x=\"" << origin[0] << "\" y=\"" << origin[1] <<
	       	"\" z=\"" << origin[2] << "\"/>" << endl;
	f << tabs(nTabs+1) << "<viewdirecton x=\"" << viewDirection[0] << "\" y=\"" << origin[1] <<
	       	"\" z=\"" << viewDirection[2] << "\"/>" << endl;
	f << tabs(nTabs+1) << "<updirection x=\"" << origin[0] << "\" y=\"" << origin[1] <<
	       	"\" z=\"" << upDirection[2] << "\"/>" << endl;

	f<< tabs(nTabs+1) <<  "<fovangle value=\"" << fovAngle << "\"/>" << endl;
	f<< tabs(nTabs+1) <<  "<nearplane value=\"" << nearPlane << "\"/>" << endl;
	f << tabs(nTabs) << "</perspective>" << endl;

	return true;
}

bool CameraPerspective::readState(xmlNodePtr nodePtr)
{
	//Retrieve user string
	if(XMLHelpFwdToElem(nodePtr,"userstring"))
		return false;

	xmlChar *xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	userString=(char *)xmlString;
	xmlFree(xmlString);
	if(!userString.size())
		return false;

	std::string tmpStr;	
	//Retrieve origin
	//====
	if(XMLHelpFwdToElem(nodePtr,"origin"))
		return false;
	float x,y,z;
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
	if(XMLHelpFwdToElem(nodePtr,"fovangle"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(fovAngle,tmpStr))
		return false;

	xmlFree(xmlString);
	
	//====
	
	//Get the near plane
	//====
	if(XMLHelpFwdToElem(nodePtr,"nearplane"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(nearPlane,tmpStr))
		return false;

	xmlFree(xmlString);
	
	//====
	
	return true;
}	
//=====

CameraPerspLookAt::CameraPerspLookAt() 
{
	target=Point3D(0.0f,0.0f,0.0f);
	origin=Point3D(0.0f,0.0f,1.0f);
	viewDirection=Point3D(0.0f,0.0f,-1.0f);
	upDirection=Point3D(0.0f,1.0f,0.0f);
	typeNum=CAM_LOOKAT;
}

Camera *CameraPerspLookAt::clone() const
{
	CameraPerspLookAt *retCam = new CameraPerspLookAt;

	retCam->origin =origin;
	retCam->viewDirection=viewDirection;
	retCam->upDirection =upDirection;
	retCam->userString=userString;

	retCam->fovAngle = fovAngle;
	retCam->nearPlane=nearPlane;

	retCam->target = target;

	return retCam;
}

CameraPerspLookAt::~CameraPerspLookAt()
{
}

void CameraPerspLookAt::setTarget(const Point3D &pt)
{
	ASSERT(pt.sqrDist(origin)>10.0f*std::numeric_limits<float>::epsilon());
	target=pt;
	recomputeViewDirection();
}

Point3D CameraPerspLookAt::getTarget() const
{
	return target;
}


void CameraPerspLookAt::setOrigin(const Point3D &newOrigin)
{
	ASSERT(newOrigin.sqrDist(target)>std::numeric_limits<float>::epsilon());
	origin=newOrigin;
	recomputeViewDirection();
}

void CameraPerspLookAt::apply(float aspect, const BoundCube &bc, bool loadIdentity) const
{
	glMatrixMode (GL_PROJECTION);
	if(loadIdentity)
		glLoadIdentity();

	farPlane = 1.5*bc.getMaxDistanceToBox(origin);
	gluPerspective(fovAngle/2.0,aspect,nearPlane,farPlane);
	glMatrixMode(GL_MODELVIEW);

	ASSERT(origin.sqrDist(target)>std::numeric_limits<float>::epsilon());
	gluLookAt(origin[0],origin[1],origin[2],
				target[0],target[1],target[2],
				upDirection[0],upDirection[1],upDirection[2]);
}

void CameraPerspLookAt::apply(float aspect,const BoundCube &b,bool loadIdentity,
						float leftRestrict,float rightRestrict, 
						float bottomRestrict,float topRestrict) const
{
	ASSERT(leftRestrict < rightRestrict);
	ASSERT(bottomRestrict< topRestrict);

	glMatrixMode (GL_PROJECTION);
	if(loadIdentity)
		glLoadIdentity();

	float width,height;
	height = tan(fovAngle/2.0*M_PI/180)*nearPlane;
	width= height*aspect;
	
	farPlane = 1.5*b.getMaxDistanceToBox(origin);

	//Frustum uses eye coordinates.	
	glFrustum(leftRestrict*width,rightRestrict*width,bottomRestrict*height,
					topRestrict*height,nearPlane,farPlane);
	glMatrixMode(GL_MODELVIEW);

	ASSERT(origin.sqrDist(target)>std::numeric_limits<float>::epsilon());
	gluLookAt(origin[0],origin[1],origin[2],
				target[0],target[1],target[2],
				upDirection[0],upDirection[1],upDirection[2]);
}

void CameraPerspLookAt::translate(float moveLR, float moveUD)
{
	//Try to move such that the target sweeps our field of view
	//at a constant rate. Standard normaliser is view length at
	//a 90* camera
	//Use tan.. to normalise motion rate 
	float fovMultiplier;
	//Prevent numerical error near tan( 90*)
	if(fovAngle < 175.0f)
		fovMultiplier = tan(fovAngle/2.0*M_PI/180.0);
	else
		fovMultiplier = tan(175.0f/2.0*M_PI/180.0);
	

	moveLR=moveLR*sqrt(target.sqrDist(origin)*fovMultiplier);
	moveUD=moveUD*sqrt(target.sqrDist(origin)*fovMultiplier);

	origin+=upDirection*moveUD + (upDirection.crossProd(viewDirection))*moveLR;
	target+=upDirection*moveUD + (upDirection.crossProd(viewDirection))*moveLR;
}

void CameraPerspLookAt::forwardsDolly(float moveRate)
{
	Point3D newOrigin;

	//Prevent camera orientation inversion, which occurs when moving past the target
	if((viewDirection*moveRate).sqrMag() > target.sqrDist(origin))
	{
		if((target-origin).sqrMag() < sqrt(std::numeric_limits<float>::epsilon()))
				return;

		//Yes, this simplifies analytically. However i think the numerics come into play.
		float moveInv = 1.0/(fabs(moveRate) + sqrt(std::numeric_limits<float>::epsilon()));
		newOrigin=origin+viewDirection*moveInv/(1.0+moveInv);

	}
	else
	{
		//scale moverate by orbit distance
		moveRate = 0.05*moveRate*sqrt(target.sqrDist(origin));
		newOrigin=origin+viewDirection*moveRate;
	}

	//Only accept origin change if it is sufficiently far from the target
	if(newOrigin.sqrDist(target)>sqrt(std::numeric_limits<float>::epsilon()))
		origin=newOrigin;
}


//Clockwise roll looking from camera view by rollRad radians
void CameraPerspLookAt::roll(float rollRad)
{
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

void CameraPerspLookAt::pivot(float leftRightRad,float updownRad)
{

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
	target*=sqrt((target).sqrDist(origin));

	recomputeViewDirection();
	recomputeUpDirection();
}

//Make a given bounding box visible, as much as possible
void CameraPerspLookAt::ensureVisible(const BoundCube &boundCube, unsigned int face)
{
	//Face is defined by the following net
	//	1
	//  2   3   4
	//  	5
	//  	6
	//3 is the face directed to the +ve x axis,
	//with the "up"" vector on the 3 aligned to z,
	//so "1" is parallel to the Z axis and is "visible"
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
			tmpUpVec = Point3D(0,0,1);
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
	outDistance=boxToFrontDist+halfMaxFaceDim/tan((fovAngle*M_PI/180)/2.0f);
	//Minimal camera distance is given trigonometrically. Add aditional 1 to ensure that nearplane does not clip object
	//then multiply by 1.4 to give a bit of border.
	origin=boxCentroid+faceOutVector*1.4*(1.0+outDistance);


	//Set the default up direction
	upDirection=tmpUpVec;

	//Reset the view direction
	recomputeViewDirection();
	//Ensure up direction orthogonal
	recomputeUpDirection();
	nearPlane = 1;
}

void CameraPerspLookAt::recomputeViewDirection()
{
	viewDirection=origin-target;
	viewDirection.normalise();
}


void CameraPerspLookAt::move(float moveLRAngle, float moveUDAngle)
{

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

void CameraPerspLookAt::getProperties(CameraProperties &p) const
{
	p.data.clear();
	p.types.clear();
	p.keys.clear();

	vector<pair<string,string> > s;
	vector<unsigned int> type,keys;

	string ptStr;
	stream_cast(ptStr,origin);
	s.push_back(std::make_pair("Origin", ptStr));
	type.push_back(PROPERTY_TYPE_POINT3D);
	keys.push_back(KEY_PERSPLOOKAT_ORIGIN);
	
	stream_cast(ptStr,target);
	s.push_back(std::make_pair("Target", ptStr));
	type.push_back(PROPERTY_TYPE_POINT3D);
	keys.push_back(KEY_PERSPLOOKAT_TARGET);
	
	stream_cast(ptStr,upDirection);
	s.push_back(std::make_pair("Up Dir.", ptStr));
	type.push_back(PROPERTY_TYPE_POINT3D);
	keys.push_back(KEY_PERSPLOOKAT_UPDIRECTION);

	string tmp;
	stream_cast(tmp,fovAngle);
	s.push_back(std::make_pair("Field of View (deg)", tmp));
	type.push_back(PROPERTY_TYPE_REAL);
	keys.push_back(KEY_PERSPLOOKAT_FOV);


	p.data.push_back(s);
	p.keys.push_back(keys);
	p.types.push_back(type);
}

bool CameraPerspLookAt::setProperty(unsigned int key, const string &value)
{

	switch(key)
	{
		case KEY_PERSPLOOKAT_ORIGIN:
		{
			Point3D newPt;
			if(!newPt.parse(value))
				return false;

			//Disallow origin to be set to same as target
			if(newPt.sqrDist(target) < sqrt(std::numeric_limits<float>::epsilon()))
				return false;

			origin= newPt;

			break;
		}
		case KEY_PERSPLOOKAT_TARGET:
		{
			Point3D newPt;
			if(!newPt.parse(value))
				return false;

			//Disallow origin to be set to same as target
			if(newPt.sqrDist(origin) < sqrt(std::numeric_limits<float>::epsilon()))
				return false;
			target = newPt;

			break;
		}
		case KEY_PERSPLOOKAT_UPDIRECTION:
		{
			Point3D newDir;
			if(!newDir.parse(value))
				return false;

			//View direction and up direction may not be the same
			if(viewDirection.crossProd(newDir).sqrMag() < 
				sqrt(std::numeric_limits<float>::epsilon()))
				return false;

			upDirection=newDir;
			//Internal up direction should be perp. to view direction.
			//use double cross product method to restore
			recomputeUpDirection();
			break;
		}
		case KEY_PERSPLOOKAT_FOV:
		{
			float newFOV;
			if(stream_cast(newFOV,value))
				return false;

			fovAngle=newFOV;
			break;
		}
		default:
			ASSERT(false);
	}
	return true;
}

bool CameraPerspLookAt::writeState(std::ostream &f, unsigned int format,
	       					unsigned int nTabs) const
{
	using std::endl;

	f << tabs(nTabs) << "<persplookat>" << endl;
	ASSERT(userString.size());
	f << tabs(nTabs+1) << "<userstring value=\"" << userString << "\"/>" << endl;
			
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
		
bool CameraPerspLookAt::readState(xmlNodePtr nodePtr)
{
	//Retrieve user string
	if(XMLHelpFwdToElem(nodePtr,"userstring"))
		return false;

	xmlChar *xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	userString=(char *)xmlString;
	xmlFree(xmlString);
	if(!userString.size())
		return false;

	std::string tmpStr;	
	//Retrieve origin
	//====
	if(XMLHelpFwdToElem(nodePtr,"origin"))
		return false;
	float x,y,z;
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
	if(XMLHelpFwdToElem(nodePtr,"fovangle"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(fovAngle,tmpStr))
		return false;

	xmlFree(xmlString);
	
	//====
	
	//Get the near plane
	//====
	if(XMLHelpFwdToElem(nodePtr,"nearplane"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(nearPlane,tmpStr))
		return false;

	xmlFree(xmlString);
	
	//====

	recomputeViewDirection();	
	return true;
}	

std::ostream& operator<<(std::ostream &strm, const Camera &c)
{
	strm << "origin: " << c.origin << std::endl;
	strm << "View Direction: " << c.viewDirection << std::endl;	
	strm << "Up Direction: "<< c.upDirection << std::endl;	
	return strm;
}

std::ostream& operator<<(std::ostream &strm, const CameraPerspective &c)
{
	strm << "origin: " << c.origin << std::endl;
	strm << "View Direction: " << c.viewDirection << std::endl;	
	strm << "Up Direction: "<< c.upDirection << std::endl;

	strm << "FOV (deg) : " << c.fovAngle << std::endl;
	strm << "Clip plane: " << c.nearPlane << " (near) " << std::endl;
	return strm;
}

std::ostream& operator<<(std::ostream &strm, const CameraPerspLookAt &c)
{
	strm << "origin: " << c.origin << std::endl;
	strm << "Target : " << c.target << std::endl;

	strm << "View Direction: " << c.viewDirection << std::endl;	
	strm << "Up Direction: "<< c.upDirection << std::endl;


	strm << "FOV (deg) : " << c.fovAngle << std::endl;
	strm << "Clip planes: " << c.nearPlane << " (near) " << std::endl;
	return strm;
}


