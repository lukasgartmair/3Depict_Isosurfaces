#ifndef ISOSURFACE_H
#define ISOSURFACE_H

#include "mathfuncs.h"
#include "voxels.h"

class TriangleWithVertexNorm
{
	public:
		Point3D p[3];
	        Point3D normal[3];	

		void getCentroid(Point3D &p) const;
		void computeACWNormal(Point3D &p) const;
		void safeComputeACWNormal(Point3D &p) const;
		float computeArea() const;
		bool isDegenerate() const;
};

struct TriangleWithIndexedVertices
{
	size_t p[3];
};


//Perform marching cube algorithm
void marchingCubes(const Voxels<float> &v,float isoValue, 
		std::vector<TriangleWithVertexNorm> &tVec);


#endif
