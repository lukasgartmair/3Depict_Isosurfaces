/*
 *	effect.h - opengl 3D effects header
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

#ifndef EFFECT_H
#define EFFECT_H

#include "basics.h"

//OpenGL includes
//MacOS is "special" and puts it elsewhere
#ifdef __APPLE__ 
	#include <OpenGL/glu.h>
#else
	#include <GL/glu.h>
#endif

#include "cameras.h"

//opengl allows up to 8 clipping planes
const unsigned int MAX_OPENGL_CLIPPLANES=6;

//Effect6 IDs
enum
{
	EFFECT_PLANE_CROP=1,
	EFFECT_ENUM_END
};

class Effect
{
	protected:
		static const Camera *curCam;
		unsigned int effectType;
	public:
		virtual void enable() const=0;
		virtual void disable() const=0;
		virtual unsigned int getMax() const=0;

		virtual unsigned int getType() const { return effectType;};
		static void setCurCam(Camera *c) {curCam=c;}

};

class PlaneCropEffect : public Effect
{
	private:
		static unsigned int nextGLId;
		unsigned int openGLId;
		Point3D origin, normal;
	public:
		PlaneCropEffect(){effectType=EFFECT_PLANE_CROP;openGLId=nextGLId; 
			nextGLId++; ASSERT(nextGLId < MAX_OPENGL_CLIPPLANES);}

		void enable() const;

		void disable() const;
		virtual unsigned int getMax() const;
};

#endif
