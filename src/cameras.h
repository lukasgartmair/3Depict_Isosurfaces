/*
 *	cameras.h - 3D cameras for opengl
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

#ifndef CAMERAS_H
#define CAMERAS_H

#include "basics.h"

//libxml2 headers
#undef ATTRIBUTE_PRINTF
#include <libxml/xmlreader.h>
#undef ATTRIBUTE_PRINTF

enum CAM_ENUM
{
	CAM_FREE=1,
	CAM_PERSP,
	CAM_LOOKAT,
};

class CameraProperties 
{
	public:
		//Filter property data, one per output, each is value then name
		std::vector<std::vector<std::pair< std::string, std::string > > > data;
		//Data types for each single element
		std::vector<std::vector<unsigned int>  > types;
		
		//!Key numbers for filter. Must be unique per set
		std::vector<std::vector<unsigned int> > keys;
	
};

//!An abstract base class for a camera
class Camera
{
	protected:
		//!Camera location
		Point3D origin;
		//!Direction camera is looking in
		Point3D viewDirection;
		//!Up direction for camera (required to work out "roll")
		Point3D upDirection;
		//!Type number
		unsigned int typeNum;
		//!user string, e.g. camera name
		std::string userString;
	public:
		//!constructor
		Camera();
		//!Destructor
		virtual ~Camera();
		//!Duplication routine. Must delete returned pointer manually. 
		/*!Base implementation OK for non-pointer containing objects. Otherwise define (1) copy constructor or (2) overload
		 */
		virtual Camera *clone() const=0;

		//!Streaming output operator, presents human readable text
                friend std::ostream &operator<<(std::ostream &stream, const Camera &);

		//!Return the origin of the camera
		Point3D getOrigin() const;
		//!Return the view direction for the camera
		Point3D getViewDirection() const;
		//!Return the up direction for the camera
		Point3D getUpDirection() const;

		//!Set the camera's position
		virtual void setOrigin(const Point3D &);
		//!set the direction that the camera looks towards
		void setViewDirection(const Point3D &);
		//!set the direction that the camera considers "up"
		void setUpDirection(const Point3D &);
		
		//!Set the user string
		void setUserString(const std::string &newString){ userString=newString;};
		//!Get the user string
		std::string getUserString() const { return userString;};

		//!Do a forwards "dolly",where the camera moves along its viewing axis
		virtual void forwardsDolly(float dollyAmount);

		//!Move the camera origin
		virtual void move(float leftRightAmount,float UpDownAmount);

		//!Move the camera origin
		virtual void translate(float leftRightAmount,float UpDownAmount);

		//!pivot the camera
		/* First pivots the camera around the across direction
		 * second pivot sthe camera around the up direction
		 */
		virtual void pivot(float rollAroundAcross, float rollaroundUp);

		//!Roll around the view direction
		virtual void roll(float roll) {} ;
		//!Applies the camera settings to openGL. Ensures the far planes
		//is set to make the whole scene visible
		virtual void apply(float outputRatio,const BoundCube &b,bool loadIdentity=true) const=0;
		//!Applies the camera settings to openGL, restricting the viewport (range (-1, 1))
		virtual void apply(float outputRatio,const BoundCube &b,bool loadIdentity,
						float leftRestrict,float rightRestrict, 
						float bottomRestrict, float topRestrict) const=0;
		//!Ensures that the given boundingbox should look nice, and be visible
		virtual void ensureVisible(const BoundCube &b, unsigned int face=3)=0;

		//!Obtain the properties specific to a camera
		virtual void getProperties(CameraProperties &p) const =0;
		//!Set the camera property from a key & string pair
		virtual bool setProperty(unsigned int key, const std::string &value) =0;

		unsigned int type() const {return typeNum;};

		//!Write the state of the camera
		virtual bool writeState(std::ostream &f, unsigned int format, unsigned int tabs) const =0;
		
		//!Read the state of the camera from XML document
		virtual bool readState(xmlNodePtr nodePtr)=0;

};

//!Orthognal transformation camera
class CameraOrthogonal : public Camera
{
	public:
		CameraOrthogonal() {typeNum=CAM_FREE;};
		~CameraOrthogonal() {};
		//!clone function
		Camera *clone() const;	
		//!Applies the camera settings to openGL, ensuring cube is not clipped by far plane 
		virtual void apply(float outputRatio,const BoundCube &b,bool loadIdentity=true) const {}; 
		//!Applies the camera settings to openGL, restricting the viewport (range (-1, 1))
		virtual void apply(float outputRatio,const BoundCube &b,bool loadIdentity,
						float leftRestrict,float rightRestrict, 
						float bottomRestrict, float topRestrict) const {ASSERT(false);};

		//!Return the user-settable properties of the camera
		void getProperties(CameraProperties &p) const;
		
		//!Write the state of the camera
		virtual bool writeState(std::ostream &f, unsigned int format, unsigned int tabs=0) const;
		//!Read the state of the camera
		virtual bool readState(xmlNodePtr nodePtr);
};

//!A class for a perspective "point and view" camera
/*!Class employes a standard viewing frustrum method
 * using glFrustrum
 */
class CameraPerspective : public Camera
{
	protected:
		//!Perspective FOV
		float fovAngle;

		//!Near clipping plane distance. 
		float nearPlane;
		//!Far plane is computed on-the-fly. cannot be set directly. Oh no! mutable. gross!
		mutable float farPlane;

		//!Do the perspective calculations
		void doPerspCalcs(float aspect,const BoundCube &bc,bool loadIdentity) const;
	
		//Version with top left control. left and top are in world coordinates.
		void doPerspCalcs(float aspect,const BoundCube &bc,
				bool loadIdentity,float left, float top) const;
		
	public:
		//!Constructor
		CameraPerspective();
		//!Destructor
		virtual ~CameraPerspective();
		//!clone function
		Camera *clone() const;	

		//!Streaming output operator, presents human readable text
                friend std::ostream &operator<<(std::ostream &stream, const CameraPerspective &);
		//!Applies the camera transforms to world
		void apply(float outAspect, const BoundCube &boundCube,bool loadIdentity=true) const;
		
		//!Set the camera's near clipping plane
		void setNearPlane(float f) {nearPlane = f;}

		//!Set the camera's FOV angle. 
		inline void setFOV(float newFov) { ASSERT(newFov >0.0 && newFov<180.0); fovAngle =newFov;}

		//!get the camera's near clipping plane
		inline float getNearPlane() const {return nearPlane;}
		
		//!get the camera's near clipping plane
		inline float getFarPlane() const {return farPlane;}

		//!Get the camera's FOV angle (full angle across)
		inline float getFOV() const {return fovAngle;}
		
		//!Ensure that the box is visible
		/*! Face is set by cube net
					1
		 	 	    2   3   4
				  	5
				  	6
		3 is the face directed to the +ve x axis,
		with the "up"" vector on the 3 aligned to z,
		so "1" is perpendicular to the Z axis and is "visible"
		 */
		virtual void ensureVisible(const BoundCube &b, unsigned int face=3);

		//!Return the user-settable properties of the camera
		void getProperties(CameraProperties &p) const;
		
		//!Set the camera property from a key & string pair
		bool setProperty(unsigned int key, const std::string &value);
		
		//!Ensure that up direction is perpendicular to view direction
		void recomputeUpDirection();
		
		//!Write the state of the camera
		bool writeState(std::ostream &f, unsigned int format, unsigned int tabs=0) const;
		//!Read the state of the camera
		bool readState(xmlNodePtr nodePtr) ;
	
		//!Apply, restricting viewport to subresgion	
		virtual void apply(float outputRatio,const BoundCube &b,bool loadIdentity,
						float leftRestrict,float rightRestrict, 
						float bottomRestrict, float topRestrict) const {ASSERT(false);};
};

//!A perspective camera that looks at a specific location
class CameraPerspLookAt : public CameraPerspective
{
	protected:
		//!Location for camera to look at
		Point3D target;
		
		void recomputeViewDirection();
	public:
		//!Constructor
		CameraPerspLookAt();

		//!Streaming output operator, presents human readable text
                friend std::ostream &operator<<(std::ostream &stream, const CameraPerspLookAt &);
		//!clone function
		Camera *clone() const;	
		//!Destructor
		virtual ~CameraPerspLookAt();
		//!Set the look at target
		void setOrigin(const Point3D &);
		//!Set the look at target
		void setTarget(const Point3D &);
		//!Get the look at target
		Point3D getTarget() const;
		//!Applies the view transform 
		void apply(float outAspect, const BoundCube &boundCube,bool loadIdentity=true) const;
		
		//!Do a forwards "dolly",where the camera moves along its viewing axis
		void forwardsDolly(float dollyAmount);

		//!Move the camera origin
		void move(float leftRightAmount,float UpDownAmount);
		//!Simulate pivot of camera
		/* Actually I pivot by moving the target internally.
		*/
		void pivot(float lrRad,float udRad);

		void translate(float lrTrans, float udTrans);

		//Clockwise roll looking from camera view by rollRad radians
		void roll(float rollRad);
		
		//TODO: Move to parent class? Maybe not?
		//!Ensure that the box is visible
		/*! Face is set by cube net
					1
		 	 	    2   3   4
				  	5
				  	6
		3 is the face directed to the +ve x axis,
		with the "up"" vector on the 3 aligned to z,
		so "1" is perpendicular to the Z axis and is "visible"
		 */
		virtual void ensureVisible(const BoundCube &b, unsigned int face=3);
		
		//!Return the user-settable properties of the camera
		void getProperties(CameraProperties &p) const;
		//!Set the camera property from a key & string pair
		bool setProperty(unsigned int key, const std::string &value);

		//!Write the state of the camera
		bool writeState(std::ostream &f, unsigned int format, unsigned int tabs=0) const;

		//!Read the state of the camera
		bool readState(xmlNodePtr nodePtr) ;
		
		//!Apply, restricting viewport to subresgion	
		virtual void apply(float outputRatio,const BoundCube &b,bool loadIdentity,
						float leftRestrict,float rightRestrict, 
						float topRestrict, float bottomRestrict) const;
};

/*Follows a list of points
 *
 * Class contains some elementary constructions
class CameraTrack
{
	private:
		Camera *cam;
		vector<Point3D> cameraTrackPts;
		//Optional track of target points
		vector<Point3D> targetTrackPts;

	public:
	
};
*/
#endif
