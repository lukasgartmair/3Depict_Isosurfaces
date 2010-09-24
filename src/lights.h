/*
 *	lights.h - opengl lighting class header
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
#ifndef LIGHTS_H
#define LIGHTS_H

#include "basics.h"

//OpenGL includes
//MacOS is "special" and puts it elsewhere
#ifdef __APPLE__ 
	#include <OpenGL/glu.h>
#else
	#include <GL/glu.h>
#endif


//!Enumerate light vlaues for setColour
enum
{
	LIGHT_AMBIENT,
	LIGHT_DIFFUSE,
	LIGHT_SPECULAR
};

class Light
{
	protected:
		//!True if the light is on
		bool active;
		//!OpenGL light number
		unsigned int lightNum;
		//!Light's location
		Point3D origin;

		//!Light Ambient colour & intensity
		float ambientColour[3];
		//!Light diffuse colour & intensity
		float diffuseColour[3];
		//!Light specular color & intensity
		float specularColour[3];
	public:
		//!Constructor
		Light();
		//!Destructor
		virtual ~Light();
		//!Set the color of a component of the light
		/*! Light component can be LIGHT_AMBIENT, LIGHT_DIFFUSE or
		 * LIGHT_SPECULAR */
		void setColour(unsigned int, float r, 
								float g, float b);

		//!Switch light on or off
		void setActive(bool);

		//!Set the light's opengl number
		void setLightNum(unsigned int);
		//!Set the origin of the light
		void setOrigin(const Point3D&);	

		//!Is the light on or off?
		bool isActive() const;

		//!Apply  
		virtual void apply() const ;
};

#endif
