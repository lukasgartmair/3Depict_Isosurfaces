/*
 * 	Quat.c - quaternion rotation and mathematics functions.
 * 	Copyright (C) 2010 D Haley
 * 	THanks to R. Minehan for mathematical discussions.
 *
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

#include "quat.h"

void quat_conj(Quaternion *quat)
{
	//negate all the quaternion values
	quat->b = -quat->b;
	quat->c = -quat->c;
	quat->d = -quat->d;
}

//quaternion multiplication
void quat_mult(Quaternion *result, Quaternion *q1, Quaternion *q2)
{

	//16 mutliplications, 12 additions
	result->a = (q1->a*q2->a -q1->b*q2->b -q1->c*q2->c -q1->d*q2->d);
	result->b = (q1->a*q2->b +q1->b*q2->a +q1->c*q2->d -q1->d*q2->c);
	result->c = (q1->a*q2->c -q1->b*q2->d +q1->c*q2->a +q1->d*q2->b);
	result->d = (q1->a*q2->d +q1->b*q2->c -q1->c*q2->b +q1->d*q2->a);

}

//quaternion multiplication
void quat_mult_no_second_a(Quaternion *result, Quaternion *q1, Quaternion *q2)
{

	//12 mutliplications, 8 additions, 1 negation
	result->a = (-q1->b*q2->b-q1->c*q2->c -q1->d*q2->d);
	result->b = (q1->a*q2->b +q1->c*q2->d -q1->d*q2->c);
	result->c = (q1->a*q2->c -q1->b*q2->d +q1->d*q2->b);
	result->d = (q1->a*q2->d +q1->b*q2->c -q1->c*q2->b );

}

//this is a little optimisation that doesnt calculate the a component for
//the returned quaternion, and implicitly performs conjugation. 
//Note that the result is specific to quaternion rotation 
void quat_pointmult(Point3f *result, Quaternion *q1, Quaternion *q2)
{
	//12 mutliplications, 9 additions
	result->fx = (-q1->a*q2->b +q1->b*q2->a -q1->c*q2->d +q1->d*q2->c);
	result->fy = (-q1->a*q2->c +q1->b*q2->d +q1->c*q2->a -q1->d*q2->b);
	result->fz = (-q1->a*q2->d -q1->b*q2->c +q1->c*q2->b +q1->d*q2->a);

}
 
//Uses quaternion mathematics to perform a rotation around your favourite axis
//IMPORTANT: Rotvec must be normalised before passing to this function 
//failure to do so will have wierd results. 
//For better performance on multiple rotations, use other functio
//Note result is stored in returned point
void quat_rot(Point3f *point, Point3f *rotVec, float angle)
{
	double sinCoeff;
       	Quaternion rotQuat;
	Quaternion pointQuat;
	Quaternion temp;
	
	//remember this value so we dont recompute it
#ifdef _GNU_SOURCE
	double cosCoeff;
	//GNU provides sincos which is about twice the speed of sin/cos separately
	sincos(angle*0.5f,&sinCoeff,&cosCoeff);
	rotQuat.a=cosCoeff;
#else
	angle*=0.5f;
	sinCoeff=sin(angle);
	
	rotQuat.a = cos(angle);
#endif	
	rotQuat.b=sinCoeff*rotVec->fx;
	rotQuat.c=sinCoeff*rotVec->fy;
	rotQuat.d=sinCoeff*rotVec->fz;

//	pointQuat.a =0.0f; This is implied in the pointQuat multiplcation function
	pointQuat.b = point->fx;
	pointQuat.c = point->fy;
	pointQuat.d = point->fz;


	//perform  rotation
	quat_mult_no_second_a(&temp,&rotQuat,&pointQuat);
	quat_pointmult(point, &temp,&rotQuat);

}


//Retrieve the quaternion for repeated rotation. pass to the quat_rot_apply_quats
void quat_get_rot_quat(Point3f *rotVec, float angle,Quaternion *rotQuat) 
{
	double sinCoeff;
#ifdef _GNU_SOURCE
	double cosCoeff;
	//GNU provides sincos which is about twice the speed of sin/cos separately
	sincos(angle*0.5f,&sinCoeff,&cosCoeff);
	rotQuat->a=cosCoeff;
#else
	angle*=0.5f;
	sinCoeff=sin(angle);
	rotQuat->a = cos(angle);
#endif	
	
	rotQuat->b=sinCoeff*rotVec->fx;
	rotQuat->c=sinCoeff*rotVec->fy;
	rotQuat->d=sinCoeff*rotVec->fz;
}

//Use previously generated quats from quat_get_rot_quats to rotate a point
void quat_rot_apply_quat(Point3f *point, Quaternion *rotQuat)
{
	Quaternion pointQuat,temp;
//	pointQuat.a =0.0f; No need to set this, as we do not use it in the multiplciation function
	pointQuat.b = point->fx;
	pointQuat.c = point->fy;
	pointQuat.d = point->fz;
	//perform  rotation
	quat_mult_no_second_a(&temp,rotQuat,&pointQuat);
	quat_pointmult(point, &temp,rotQuat);
}
	
