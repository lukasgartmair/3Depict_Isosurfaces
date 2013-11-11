#ifndef OCTREE_H
#define OCTREE_H

#include "basics.h"
#include <vector>


struct PtOctNode
{
	//Index vector to points
	std::vector<size_t> *pts;
	PtOctNode *children[8];
};


//
//Octtree layout
//--
//x+ => (offset & 1) == true
//y+ => (offset & 2) == true
//z+ => (offset & 4) == true
//--

class PointOctTree
{
	private:
		//Maximum depth of tree 0 corresponds to level of tree root
		// depth-1 corresponds to lowest level of tree.
		size_t depth;

		//Bounding cube for entire octtree
		BoundCube bounds;
		PtOctNode *root;
	
		PtOctNode *getInsertNode(const Point3D &p) const;
	public:
		PointOctTree();

		//Build the tree to the specified depth. Optionally for performance,
		// the octtree boundcube may be set in advance.
		void build(const std::vector<Point3D> &vecPts,size_t newDepth, bool cubeSet=false);

//		void overwrite(const vector<Point3D> &vecPts);

		//Force setting of the oct-cube bounds
		void setBounds(const BoundCube &b){ bounds=b;};

		void clear(bool eraseNodePtrs=true);

		void getNodesInFrustrum(const Point3D &origin, const Point3D &forwards, 
					const Point3D &upVec,float aspect, float near, float far,
					std::vector<size_t> &indicies) const;
#ifdef DEBUG
		bool runUnitTests() const;
#endif
};



#endif

