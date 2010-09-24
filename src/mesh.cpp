#include "mesh.h"

#include <algorithm>

using std::vector;

void TriangleWithVertexNorm::computeACWNormal(Point3D &n) const
{
	Point3D a,b;

	a = p[0]-p[1];
	b = p[0]-p[2];

	n=a.crossProd(b);
	n.normalise();
}

bool TriangleWithVertexNorm::isDegenerate() const
{
	return (p[0].sqrDist(p[1]) < std::numeric_limits<float>::epsilon() ||
	p[0].sqrDist(p[2]) < std::numeric_limits<float>::epsilon() ||
	p[2].sqrDist(p[1]) < std::numeric_limits<float>::epsilon());
}


void Mesh::convertRawTriangles(std::vector<TriangleWithVertexNorm> &rawTris, float collapseTol)
{
	//TODO: Optimise this algorithm -- needs K3DTreeWithData<T>,
	//and a ::addToTree(vector<Point3D>,vector<T>) and ::rebalance()
	vector<Point3D> ps;

	//Make a flat list of points
	ps.resize(rawTris.size()*3);
	unsigned int ui=0;

	for(unsigned int ui=0;ui<rawTris.size();ui++)
	{
		ps[ui*3] = rawTris[ui].p[0];
		ps[ui*3+1] = rawTris[ui].p[1];
		ps[ui*3+2] = rawTris[ui].p[2];
	}

	vector<unsigned int> newIndices;
	newIndices.resize(rawTris.size()*3);
	vector<Point3D> collapsedPts;
	//map the points to the first occurrence of the NN in the small list
	for(size_t ui=0;ui<ps.size();ui++)
	{
		bool haveIndex;
		haveIndex=false;
		//Do linear search(NON-OPTIMAL!) for existing nearby pt
		for(size_t uj=0;uj<collapsedPts.size();uj++)
		{
			if(collapsedPts[uj].sqrDist(ps[ui]) < collapseTol)
			{
				newIndices[ui]=uj;
				haveIndex=true;
				break;
			}
		}

		if(!haveIndex)
		{
			ASSERT(find(collapsedPts.begin(),collapsedPts.end(),ps[ui]) == collapsedPts.end());
			collapsedPts.push_back(ps[ui]);

			newIndices[ui]=collapsedPts.size()-1;
		}
	}

	//Set the collapsed nodes
	nodes.swap(collapsedPts);
	collapsedPts.clear();

	//Must be a multiple of 3
	ASSERT(newIndices.size()%3 == 0);
	triangles.resize(newIndices.size()/3);

	//Assign triangle data
	for(unsigned int ui=0;ui<triangles.size();ui++)
	{
		triangles[ui].vertices[0] = newIndices[ui*3];
		triangles[ui].vertices[1] = newIndices[ui*3+1];
		triangles[ui].vertices[2] = newIndices[ui*3+2];

	}

}



void Mesh::estimateNormalsFromTris()
{

	vertexNorm.resize(nodes.size(),Point3D(0,0,0));
	for(unsigned int ui=0;ui<triangles.size();ui++)
	{
		float area;
		Point3D n,a,b;
		a = nodes[triangles[ui].vertices[0]]-
			nodes[triangles[ui].vertices[1]];
		b = nodes[triangles[ui].vertices[0]]-
			nodes[triangles[ui].vertices[2]];

		n=a.crossProd(b);
		//area of triangle is half its cross product of edges
		area=0.5f*sqrt(n.sqrMag());

		//normal is normalised cross product
		a.normalise();

		ASSERT(area > std::numeric_limits<float>::epsilon());
		//Use inverse area weighting to emphasise small tris
		area=1.0/area;	
		n*=area;
		vertexNorm[triangles[ui].vertices[0]]+=n;
		vertexNorm[triangles[ui].vertices[1]]+=n;
		vertexNorm[triangles[ui].vertices[2]]+=n;

	}
	for(size_t ui=0;ui<vertexNorm.size();ui++)
		vertexNorm[ui].normalise();


/* Parallel version (broken?)

#else
	vector<Point3D> * accumulateNorms = new vector<Point3D>[omp_get_max_threads()];

	//Initialise to 0
#pragma omp parallel for
	for(unsigned int ui=0;ui<nT;ui++)
		accumulateNorms[ui].resize(nodes.size(),Point3D(0,0,0));

	//Get vector
#pragma omp parallel for
	for(unsigned int ui=0;ui<triangles.size();ui++)
	{

		float area,a,b;
		Point3D n;
		a = triangles[ui].vertices[0]-p[1];
		b = triangles[ui].vertices[0]-p[2];

		n=a.crossProd(b);
		//area of triangle is half its cross product of edges
		sqrArea=1/2*sqrt(a.sqrMag());

		//normal is normalised cross product
		a.normalise();

		ASSERT(area > std::numeric_limits<float>::epsilon());
		//Use inverse area weighting to emphasise small tris
		n*=1.0/area;
		accumulateNorms[omp_get_thread_num()][triangles[ui].vertices[0]]+=n
		accumulateNorms[omp_get_thread_num()][triangles[ui].vertices[1]]+=n
		accumulateNorms[omp_get_thread_num()][triangles[ui].vertices[2]]+=n

	}


	//Merge vectors
	for(unsigned int ui=0;ui<nT;ui++)
	{
		#pragma omp parallel for
		for(unsigned int uj=0;uj<accumulateNorms[ui].size();uj++)
			vertexNorm[uj]+=accumulateNorms[ui];

	}

	delete[] accumulateNorms;
#endif

#pragma omp parallel for
*/
}

