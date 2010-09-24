#ifndef MESH_H
#define MESH_H

#include <vector>
#include <limits>

#include "basics.h"

class TriangleWithVertexNorm
{
	public:
		Point3D p[3];
	        Point3D normal[3];	

		void computeACWNormal(Point3D &p) const;
		bool isDegenerate() const;
};

class TRIANGLE
{
	public:
		unsigned int vertices[3];
};

class Mesh
{

	public:
	std::vector<TRIANGLE> triangles;
	std::vector<Point3D> nodes;
	std::vector<Point3D> vertexNorm;

		void clear() { triangles.clear();nodes.clear();vertexNorm.clear();}
		//!Convert a raw triangle set into a mesh
		void convertRawTriangles(std::vector<TriangleWithVertexNorm> &rawTris,
				float collapseTol);

		//Estimate the normals from the triangle data
		void estimateNormalsFromTris();
};

#endif
