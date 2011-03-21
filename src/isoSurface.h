#ifndef ISOSURFACE_H
#define ISOSURFACE_H

#include "mathfuncs.h"
#include "voxels.h"

class TriangleWithVertexNorm
{
	public:
		Point3D p[3];
	        Point3D normal[3];	

		void computeACWNormal(Point3D &p) const;
		bool isDegenerate() const;
};


//Perform marching cube algorithm
void marchingCubes(const Voxels<float> &v,float isoValue, 
		std::vector<TriangleWithVertexNorm> &tVec);


#endif
