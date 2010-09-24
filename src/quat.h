/*
 * 	Quat.h - quaternion rotation and mathematics header.
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


#ifndef QUAT_H
#define QUAT_H

//needed for sincos
#ifndef __APPLE__
#ifdef __GNUC__
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif
#endif
//needed for sin and cos
#include <math.h>

typedef struct 
{
	float a;
	float b;
	float c;
	float d;
} Quaternion;

typedef struct
{
	float fx;
	float fy;
	float fz;
} Point3f;

//conjugates the argument
#ifdef __cplusplus
extern "C" {
#endif

void quat_conj(Quaternion *quat);
//calculates the square length of the quaternion
float quat_sqrlen(Quaternion *quat);
//multiplies two quaternions result=q1*q2
void quat_mult(Quaternion *result, Quaternion *q1, Quaternion *q2);

//this is a little optimisation that doesnt calculate the a component for
//the returned quaternion. Note that the result is specific to quaternion rotation 
void quat_pointmult(Point3f *result, Quaternion *q1, Quaternion *q2);

//Uses quaternion mathematics to perform a rotation around your favourite axis
//IMPORTANT: rotVec must be normalised before passing to this function 
//failure to do so will have wierd results
//Note result is stored in  point passsed as argument
void quat_rot(Point3f *point, Point3f *rotVec, float angle);

//Retrieve the two quaternions for repeated rotations. pass to the quat_rot_apply_quats
void quat_get_rot_quats(Point3f *rotVec, float angle, 
		Quaternion *rotQuat, Quaternion *conjQuat);

//Use previously generated quats from quat_get_rot_quats to rotate a point
void quat_rot_apply_quats(Point3f *point, Quaternion *rotQuat, Quaternion *conjQuat);

//this performs an optimised rotation of a point around 3 angles
//pitch, then yaw and then roll. Pitch is defined to be rotation around x
//yaw around z and roll around y. x is right, y is forwards, z is up
//The results are derived by special case quaternion rotation around basis vectors
void quat_rot_pitchyawroll(Point3f *point, float pitchAngle,float yawAngle, float rollAngle);

//As for the above but in reverse order. This allows you to undo the above transform by simply
//passing through the negative roll yaw and pitch angles
void quat_rot_rollyawpitch(Point3f *point, float rollAngle,float yawAngle,float pitchAngle);

#ifdef __cplusplus
}
#endif
#endif
