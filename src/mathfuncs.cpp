/*
 *	mathfuncs.h - Miscellaneous mathematical functions implementation. 
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
#include "mathfuncs.h"
#include <sys/time.h>
#include <cassert>
#define ASSERT(f) assert(f)

const int MBIG = std::numeric_limits<int>::max();

RandNumGen::RandNumGen()
{
	//Initialisation is NOT performed here, because we need a random seed
	//to generate our sequence....

	//we dont initially have gaussian
	//value to spare
	haveGaussian=false;	
}

void RandNumGen::initialise(int seed)
{
	long mj,mk;

	int ii;
	//initialise ma[55] with seed
	mj=labs((MBIG-labs(seed)));
	mj%=MBIG;
	ma[55]=mj;
	mk=1;


	//Initialise the rest of the table
	for(unsigned int i=1; i<55; i++)
	{
		ii=(21*i)%55;
	
		ma[ii]=mk;
		mk=mj-mk;
		
		if(mk<0)
			mk+=MBIG;
		mj=ma[ii];
	}

	//"warm up" the rng
	for(unsigned int j=1;j<=4;j++)
	{
		for(unsigned int i=1;i<=55;i++)
		{
			ma[i] -=ma[1+ (i+30)%55];
			if(ma[i] < 0)
				ma[i]+=MBIG;
		}
	}
	//the constant 31 is special
	inext=0;
	inextp=31;
}


float RandNumGen::genUniformDev()
{
	long mj;

	if(++inext==56)
		inext=1;
	if(++inextp == 56)
		inextp=1;

	mj=ma[inext]-ma[inextp];
	if(mj<0)
		mj+=MBIG;

	ma[inext]=mj;

	return mj*(1.0/MBIG);
}


int RandNumGen::genInt()
{
	long mj;

	if(++inext==56)
		inext=1;
	if(++inextp == 56)
		inextp=1;

	mj=ma[inext]-ma[inextp];
	if(mj<0)
		mj+=MBIG;

	ma[inext]=mj;

	return mj;
}


//This is known as the Box-Muller transform
//You can change it from being a uniform variance 
//by simply multiplying by the std deviation of your choice
//this will cause the random number returned to be stretched 
//in the x axis from unit variance to your number
float RandNumGen::genGaussDev()
{
	float v1,v2,rsq,fac;
	//This algorithm generates
	//two gaussian numbers from two Uniform Devs,
	//however we only want one. So to speed things up
	//remember the second and spit it out as required
	if(haveGaussian)
	{
		haveGaussian=false;
		return gaussSpare;
	}

	do
	{
		//grab two uniform Devs and 
		//move them into (-1,+1) domain
		v1=2.0f*genUniformDev()-1.0f;
		v2=2.0f*genUniformDev()-1.0f;
		rsq=v1*v1+v2*v2;
	//reject them if they dont lie in unit circle
	//or if rsq is at the origin of the unit circle
	//(as eqn below is undefined at origin)
	}while(rsq>=1.0f || rsq==0.0f);

	fac=sqrt(-2.0f*log(rsq)/rsq);
	gaussSpare=v1*fac;
	haveGaussian=true;		
	return v2*fac;
	
}

int RandNumGen::initTimer()
{
	timeval tp;

	gettimeofday(&tp,NULL);

	initialise(tp.tv_sec+ tp.tv_usec);
	
	return tp.tv_sec + tp.tv_usec;
}

