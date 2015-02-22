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


#ifdef DEBUG

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
	
	//Test slice functions
	//--
	Voxels<float> v;
	v.resize(2,2,2);
	for(size_t ui=0;ui<8;ui++)
		v.setData(ui&1, (ui & 2) >> 1, (ui &4)>>2, ui);

	float *slice = new float[4];
	//Test Z slice
	v.getSlice(2,0,slice);
	for(size_t ui=0;ui<4;ui++)
	{
		ASSERT(slice[ui] == ui);
	}

	//Expected results
	float expResults[4];
	//Test X slice
	expResults[0]=0; expResults[1]=2;expResults[2]=4; expResults[3]=6;
	v.getSlice(0,0,slice);
	for(size_t ui=0;ui<4;ui++)
	{
		ASSERT(slice[ui] == expResults[ui]);
	}

	//Test Y slice
	v.getSlice(1,1,slice);
	expResults[0]=2; expResults[1]=3;expResults[2]=6; expResults[3]=7;
	for(size_t ui=0;ui<4;ui++)
	{
		ASSERT(slice[ui] == expResults[ui]);
	}

	delete[] slice;

	//-- try again with nonuniform voxels
	v.resize(4,3,2);
	for(size_t ui=0;ui<24;ui++)
		v.setData(ui, ui);

	slice = new float[12];
	//Test Z slice
	v.getSlice(2,1,slice);
	for(size_t ui=0;ui<12;ui++)
	{
		ASSERT( slice[ui] >=12);
	}

	delete[] slice;
	//--

	return true;
}


bool edgeCountTests()
{
	Voxels<float> v;
	v.resize(4,4,4);


	TEST(v.getEdgeUniqueIndex(0,0,0,3) == v.getEdgeUniqueIndex(0,1,1,0),"Edge coincidence");
	TEST(v.getEdgeUniqueIndex(0,0,0,6) == v.getEdgeUniqueIndex(1,0,0,4),"Edge coincidence");
	TEST(v.getEdgeUniqueIndex(0,0,0,2) == v.getEdgeUniqueIndex(0,0,1,0),"Edge coincidence");

	//Check for edge -> index -> edge round tripping 
	//for single cell
	size_t x,y,z;
	x=1;
	y=2;
	z=3;
	for(size_t ui=0;ui<12;ui++)
	{
		size_t idx;
		idx= v.getCellUniqueEdgeIndex(x,y,z,ui);
		
		size_t axis;
		size_t xN,yN,zN;
		v.getEdgeCell(idx,xN,yN,zN,axis);
		
		//if we ask for the cell, we should also 
		//get the index
		ASSERT(x == xN && y==yN && z==zN);	

		//TODO: Check that the axis of the edge was preserved (not the edge itself)
		ASSERT( axis == ui/4);
	}
	
	return true;	
}



bool runVoxelTests()
{
	bool wantAbort=false;
	voxelsWantAbort = &wantAbort;

	TEST(basicTests(),"basic voxel tests");
	TEST(simpleMath(), "voxel simple maths");	
	TEST(edgeCountTests(), "voxel edge tests");	
	return true;	
}

#endif
