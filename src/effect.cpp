#include "effect.h"

const Camera* Effect::curCam=0;
unsigned int PlaneCropEffect::nextGLId =0;

void PlaneCropEffect::enable() const
{
	//Convert point and normal to plane equation
	//form n.(r-r_0) = 0. r is [x;y;z]. r_0 is any
	//point on the plane
	double array[4]; //Ax + By + Cz + D =0
	array[0]=normal[0];
	array[1]=normal[1];
	array[2]=normal[2];

	array[3] = -normal.dotProd(origin);

	ASSERT(openGLId < MAX_OPENGL_CLIPPLANES); 
	//Set up the effect
	glClipPlane(GL_CLIP_PLANE0 + openGLId,
			array);

	glEnable(GL_CLIP_PLANE0+openGLId);
}


void PlaneCropEffect::disable() const
{
	glDisable(GL_CLIP_PLANE0+openGLId);
}

unsigned int PlaneCropEffect::getMax() const
{
	return MAX_OPENGL_CLIPPLANES;
}


