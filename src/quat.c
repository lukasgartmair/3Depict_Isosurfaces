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

float quat_sqrlen(Quaternion *quat)
{
	return  quat->a*quat->a + quat->b*quat->b + 
			quat->c*quat->c + quat->d*quat->d;
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
//the returned quaternion. Note that the result is specific to quaternion rotation 
void quat_pointmult(Point3f *result, Quaternion *q1, Quaternion *q2)
{
	//12 mutliplications, 9 additions
	result->fx = (q1->a*q2->b +q1->b*q2->a +q1->c*q2->d -q1->d*q2->c);
	result->fy = (q1->a*q2->c -q1->b*q2->d +q1->c*q2->a +q1->d*q2->b);
	result->fz = (q1->a*q2->d +q1->b*q2->c -q1->c*q2->b +q1->d*q2->a);

}
 
//Uses quaternion mathematics to perform a rotation around your favourite axis
//IMPORTANT: Rotvec must be normalised before passing to this function 
//failure to do so will have wierd results
//If neccesary, this function can be optimised, knowing quatPoint.a = 0 simplifies
//multiplying quaternions somewhat
//Note result is stored in returned point
void quat_rot(Point3f *point, Point3f *rotVec, float angle)
{
	double sinCoeff;
       	Quaternion rotQuat;
	Quaternion pointQuat;
	Quaternion conjQuat;
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
	//calculate conjugate of the quaternion
	conjQuat.a = rotQuat.a;
	conjQuat.b = -rotQuat.b;
	conjQuat.c = -rotQuat.c;
	conjQuat.d = -rotQuat.d;

	//perform  rotation
	quat_mult_no_second_a(&temp,&rotQuat,&pointQuat);
	quat_pointmult(point, &temp,&conjQuat);

}


//Retrieve the two quaternions for repeated rotations. pass to the quat_rot_apply_quats
void quat_get_rot_quats(Point3f *rotVec, float angle, 
		Quaternion *rotQuat, Quaternion *conjQuat)
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

	//calculate conjugate of the quaternion
	conjQuat->a = rotQuat->a;
	conjQuat->b = -rotQuat->b;
	conjQuat->c = -rotQuat->c;
	conjQuat->d = -rotQuat->d;
}

//Use previously generated quats from quat_get_rot_quats to rotate a point
void quat_rot_apply_quats(Point3f *point, Quaternion *rotQuat, Quaternion *conjQuat)
{
	Quaternion pointQuat,temp;
//	pointQuat.a =0.0f; No need to set this, as we do not use it in the multiplciation function
	pointQuat.b = point->fx;
	pointQuat.c = point->fy;
	pointQuat.d = point->fz;
	//perform  rotation
	quat_mult_no_second_a(&temp,rotQuat,&pointQuat);
	quat_pointmult(point, &temp,conjQuat);
}

//this performs an optimised rotation of a point around 3 angles
//pitch, yaw and roll. Pitch is defined to be rotation around x
//yaw around z and roll around y. x is right, y is forwards, z is up
//The results are derived by special case rotation around basis vectors
//each call performs six sin/cos ops 3 divisions 21 multiplications
//So conceptually use this dunction to rotate a point/measruement from local tangent frame
//to body frame
void quat_rot_pitchyawroll(Point3f *point, float pitchAngle,float yawAngle, float rollAngle)
{
	//apply pitch rotation
	float twoSinCos;
	float Cos2MinSin2;
	float sinVal;
	float cosVal;
	float temp;
	//calculate sin/cos
	pitchAngle=pitchAngle*0.5f;//work out half angle
	sinVal = sin(pitchAngle);
	cosVal = cos(pitchAngle);

	//use identity c^2-s^2 = (c+s)(c-s) to speed up calc
	Cos2MinSin2 = (cosVal+sinVal)*(cosVal-sinVal);
	twoSinCos = 2.0f*cosVal*sinVal;

	//apply pitch rotation (rotate around x axis)
	//point->fx = point->fx;
	temp = Cos2MinSin2*point->fy - twoSinCos*point->fz;
	point->fz = Cos2MinSin2*point->fz + twoSinCos*point->fy;
	point->fy = temp;
	
	//calculate sin/cos
	yawAngle=yawAngle*0.5f; //work out half angle
	sinVal = sin(yawAngle);
	cosVal = cos(yawAngle);

	//use identity c^2-s^2 = (c+s)(c-s) to speed up calc
	Cos2MinSin2 = (cosVal+sinVal)*(cosVal-sinVal);
	twoSinCos = 2.0f*cosVal*sinVal;
	
	//apply yaw rotation (rotate around z axis)
	temp = Cos2MinSin2*point->fx - twoSinCos*point->fy;
      	point->fy = Cos2MinSin2*point->fy + twoSinCos*point->fx;
	//point->fz = point->fz;	
	point->fx=temp;
	
	//calculate sin/cos
	rollAngle = rollAngle*0.5f; //work out half angle
	sinVal = sin(rollAngle);
	cosVal = cos(rollAngle);

	//use identity c^2-s^2 = (c+s)(c-s) to speed up calc
	Cos2MinSin2 = (cosVal+sinVal)*(cosVal-sinVal);
	twoSinCos = 2.0f*cosVal*sinVal;

	//apply roll rotation (rotate around y axis)
	temp  = Cos2MinSin2*point->fx + twoSinCos*point->fz;
	//point->fy =  point->fy;
	point->fz = Cos2MinSin2*point->fz - twoSinCos*point->fx;

	point->fx = temp;
}
	

//As for the above but in reverse order. This allows you to undo the above transform by simply
//passing through the negative roll yaw and pitch angles
//so conceptually use this functiont o convert from body frame to local tangent frame
void quat_rot_rollyawpitch(Point3f *point, float rollAngle,float yawAngle, float pitchAngle)
{
	//apply pitch rotation
	float twoSinCos;
	float Cos2MinSin2;
	float sinVal;
	float cosVal;
	float temp;
	
	//ROLL
	//=====
	//calculate sin/cos
	rollAngle = rollAngle*0.5f; //work out half angle
	sinVal = sin(rollAngle);
	cosVal = cos(rollAngle);

	//use identity c^2-s^2 = (c+s)(c-s) to speed up calc
	Cos2MinSin2 = (cosVal+sinVal)*(cosVal-sinVal);
	twoSinCos = 2.0f*cosVal*sinVal;


	//apply roll rotation (rotate around y axis)
	temp  = Cos2MinSin2*point->fx + twoSinCos*point->fz;
	//point->fy =  point->fy;
	point->fz = Cos2MinSin2*point->fz - twoSinCos*point->fx;
	point->fx = temp;
	//====
	
	//YAW
	//====
	//calculate sin/cos
	yawAngle=yawAngle*0.5f; //work out half angle
	sinVal = sin(yawAngle);
	cosVal = cos(yawAngle);

	//use identity c^2-s^2 = (c+s)(c-s) to speed up calc
	Cos2MinSin2 = (cosVal+sinVal)*(cosVal-sinVal);
	twoSinCos = 2.0f*cosVal*sinVal;
	
	//apply yaw rotation (rotate around z axis)
	temp = Cos2MinSin2*point->fx - twoSinCos*point->fy;
      	point->fy = Cos2MinSin2*point->fy + twoSinCos*point->fx;
	//point->fz = point->fz;	
	point->fx=temp;
	//====
	
	//PITCH
	//=====
	//calculate sin/cos
	pitchAngle=pitchAngle*0.5f;//work out half angle
	sinVal = sin(pitchAngle);
	cosVal = cos(pitchAngle);

	//use identity c^2-s^2 = (c+s)(c-s) to speed up calc
	Cos2MinSin2 = (cosVal+sinVal)*(cosVal-sinVal);
	twoSinCos = 2.0f*cosVal*sinVal;

	//apply pitch rotation (rotate around x axis)
	//point->fx = point->fx;
	temp = Cos2MinSin2*point->fy - twoSinCos*point->fz;
	point->fz = Cos2MinSin2*point->fz + twoSinCos*point->fy;
	point->fy = temp;
	//====
		
}
	
