/*
 * voxels.cpp - Voxelised data manipulation class 
 * Copyright (C) 2013  D. Haley
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "voxels.h"

using std::numeric_limits;
const float FLOAT_SMALL=
	sqrt(numeric_limits<float>::epsilon());

bool testConvolve()
{
	{
	Voxels<unsigned int> data,kernel,result;
	data.setCallbackMethod(dummyCallback);
	kernel.setCallbackMethod(dummyCallback);

	const size_t NUM_SIZES=4;
	const size_t TEST_SIZES[]= { 1, 3, 4, 7};

	for(size_t ui=0;ui<BOUND_ENUM_END; ui++)
	{
		//Convolve several kernel sizes
		//---
		for(unsigned int ui=0;ui<NUM_SIZES;ui++)
		{

			size_t curSize;
			curSize=TEST_SIZES[ui];

			data.resize(curSize,curSize,curSize);
			kernel.resize(curSize,curSize,curSize);

			//Test with a whole bunch of different fill values
			for(size_t fillVal=0;fillVal<5;fillVal++)
			{
				data.fill(fillVal);
				kernel.fill(fillVal);

				data.convolve(kernel,result,BOUND_CLIP);

				size_t nX,nY,nZ;
				result.getSize(nX,nY,nZ);

				TEST(nX == 1,"convolve dimensions");
				TEST(nY == 1,"convolve dimensions");
				TEST(nZ == 1,"convolve dimensions");

				TEST(result.max() == curSize*curSize*curSize*(fillVal*fillVal),
						"Convolve maxima");
			}
		}
		//--
	
		kernel.resize(1,1,1);
		//Convolve several kernel sizes
		for(unsigned int ui=0;ui<NUM_SIZES;ui++)
		{

			size_t curSize;
			curSize=TEST_SIZES[ui];

			data.resize(curSize,curSize,curSize);
			data.fill(1);
			kernel.fill(1);

			data.convolve(kernel,result,BOUND_CLIP);
			TEST(result == data, "convolve identity");
		}
		kernel.resize(3,3,3);
	}
	}

	//Test integral stuff
	{
		Voxels<float> data;
		data.resize(3,3,3);
		data.fill(1);
		TEST(fabs(data.trapezIntegral() - 1.0f )< FLOAT_SMALL,"Trapezoid test");

		data.resize(5,5,5);
		data.fill(1);
		data.setBounds(Point3D(0,0,0),Point3D(1,1,1));
		TEST(fabs(data.trapezIntegral() - 1.0f) < FLOAT_SMALL,"Trapezoid test");
		data.setBounds(Point3D(0,0,0),Point3D(5,5,5));
		TEST(fabs(data.trapezIntegral() - 125.0f) < FLOAT_SMALL,"Trapezoid test");
	}

	//Test convolution stuff
	{
	Voxels<float> data,kernel,result;
	data.setCallbackMethod(dummyCallback);
	kernel.setCallbackMethod(dummyCallback);
	//Check that convolving with an impulse with 
	//a Gaussian  gives us a Gaussian 
	//back, roughly speaking
	kernel.setGaussianKernelCube(1.0f,10.0f,10);

	float trapz = kernel.trapezIntegral();
	TEST(trapz < 1.5f && trapz > 0.5f, "Trapezoidal kernel integral test");
	

	data.resize(20,20,20);
	data.fill(0.0f);
	data.setData(10,10,10,1.0f);
	data.convolve(kernel,result,BOUND_CLIP);

	TEST(result.max() >  0 && result.max() < 1.0f,"result should be nonzero, and less than the original input (convolve only squeezes maxima/minima)");
	//Gaussian @ x=0, stdev 1
	TEST(fabs(result.max() - kernel.max())  < FLOAT_SMALL
		,"Gaussian kernel test- maxima of convolved and kernel should be the same");
	}

	return true;
}

bool simpleMath()
{
	Voxels<float> a,b,c;
	a.resize(3,3,3);
	a.fill(2.0f);

	float f;
	f=a.getSum();
	TEST(fabs(f-3.0*3.0*3.0*2.0 )< FLOAT_SMALL,"getsum test");
	TEST(fabs(a.count(1.0f)- 3.0*3.0*3.0) < FLOAT_SMALL,"Count test"); 

	return true;
}

bool basicTests()
{
	Voxels<float> f;
	f.setCallbackMethod(dummyCallback);
	f.resize(3,3,3);
	
	size_t xs,ys,zs;
	f.getSize(xs,ys,zs);
	TEST(xs == ys && ys == zs && zs == 3,"resize tests");
	


	f.fill(0);
	f.setData(1,1,1,1.0f);

	TEST(fabs(f.max() - 1.0f) < FLOAT_SMALL,"Fill and data set");

	f.resizeKeepData(2,2,2);
	f.getSize(xs,ys,zs);

	TEST(xs == ys && ys == zs && zs == 2, "resizeKeepData");
	TEST(f.max() == 1.0f,"resize keep data");
	
	return true;
}


bool runVoxelTests()
{
	TEST(basicTests(),"basic voxel tests");
	TEST(testConvolve()," voxel convolve");
	TEST(simpleMath(), "voxel simple maths");	
	return true;	
}
