/*
 *	mathfuncs.h - General mathematic functions header
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
#ifndef MATHFUNCS_H
#define MATHFUNCS_H

#include <cstdlib>
#include <cmath>
#include <limits>

//This should be the largest value a signed integer can hold
//(or one less...)

//This have been verified to produce correct output
//using histogram analysis and mean value analysis.



//IMPORTANT!!!
//===============
//Do NOT use mutliple instances of this in your code
//with the same initialisation technique (EG intialising from system clock)
//this would be BAD, correlations might well be introduced into your results
//that are simply a result of using correlated random sequences!!! (think about it)
//use ONE random number generator in the project, initialise it and then "register"
//it with any objects that need a random generator. 
//==============
class RandNumGen
{
	private:
		int ma[56];
		int inext,inextp;
		float gaussSpare;
		bool haveGaussian;

	public:
		RandNumGen();
		void initialise(int seedVal);
		int initTimer();

		int genInt();
		float genUniformDev();

		//This generates a number chosen from
		//a gaussian distribution range is (-inf, inf)
		float genGaussDev();
};

//As per ISO 31-11 : theta is zenith, phi is azimuth
//Point3D pointFromSpherical(float r, float theta, float phi);

#endif

