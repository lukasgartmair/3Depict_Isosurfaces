/*
 *	lights.cpp - OpenGL Lighting implementation
 *	Copyright (C) 2007, D Haley 

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

#include "lights.h"

Light::Light() : active(false) 
{
	/*
	for(unsigned int ui=0; ui<3; ui++)
	{
		ambientColour[ui] = 1.0f;
		diffuseColour[ui] = 1.0f;
		specularColour[ui] = 1.0f;
	}*/
	origin = Point3D(0.0f,0.0f,0.0f);
}

Light::~Light()
{
}

void Light::setActive(bool b)
{
	active=b;
}

bool Light::isActive() const
{
	return active;
}

void Light::setLightNum(unsigned int ln)
{
	lightNum =ln;
}

void Light::setOrigin(const Point3D &p)
{
	origin=p;
}

void Light::setColour(unsigned int mode, float r, float g, float b)
{
	switch(mode)
	{
		case LIGHT_AMBIENT:
			ambientColour[0] = r;
			ambientColour[1] = g;
			ambientColour[2] = b;
			break;
		case LIGHT_DIFFUSE:
			diffuseColour[0] = r;
			diffuseColour[1] = g;
			diffuseColour[2] = b;
			break;
		case LIGHT_SPECULAR:
			specularColour[0] = r;
			specularColour[1] = g;
			specularColour[2] = b;
			break;
		default:
			ASSERT(false);
	}

}

void Light::apply() const
{
	if(active)
	{
		float f[3];
		origin.copyValueArr(f);
		glEnable(lightNum);
		glLightfv(lightNum, GL_AMBIENT, ambientColour);
		glLightfv(lightNum, GL_DIFFUSE, diffuseColour);
		glLightfv(lightNum, GL_SPECULAR, specularColour);
		glLightfv(lightNum, GL_POSITION, f);
	}
	else
		glDisable(lightNum);
}

