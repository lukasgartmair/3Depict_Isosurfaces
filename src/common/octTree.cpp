#include "octTree.h"


#include <map>
#include <list>

#include <algorithm>
#include <utility>
#include <deque>


using std::vector;
using std::list;
using std::map;
using std::make_pair;
using std::pair;
using std::deque;

PointOctTree::PointOctTree()
{
}

void PointOctTree::clear(bool eraseNodePtrs)
{
	root = new PtOctNode;
	deque<pair<PtOctNode *,size_t> > queuedNodes;
	
	queuedNodes.push_back(make_pair(root,0));
	while(queuedNodes.size())
	{
		PtOctNode *p;
		p = queuedNodes.front().first;

		size_t curDepth;
		curDepth=queuedNodes.front().second; 

		if(curDepth< depth)
		{
			//queue children for deletion
			for(size_t ui=0;ui<8;ui++)
				queuedNodes.push_back(make_pair(p->children[ui],curDepth+1));
			
			//If we are at the base of the tree, we need
			// to erase children
			if(queuedNodes.back().second == depth-1)
				delete p->pts;
			//Remove the parent object as needed
			if(eraseNodePtrs)
				delete p;
		}
		queuedNodes.pop_front();
	}
	
}

void PointOctTree::build(const vector<Point3D> &vecPts, size_t newDepth, bool cubeSet)
{
	ASSERT(newDepth);
	clear();

	depth=newDepth;

	//Check if we have to manually compute the bounding cube
	if(!cubeSet)
		bounds.setBounds(vecPts);

	//Build the new tree, empty of poitns
	//--
	root = new PtOctNode;
	deque<pair<PtOctNode *,size_t> > queuedNodes;
	
	queuedNodes.push_back(make_pair(root,0));
	while(queuedNodes.size())
	{
		PtOctNode *p;
		size_t curDepth;
		p = queuedNodes.front().first;

		curDepth=queuedNodes.front().second;
		if(curDepth< depth -1)
		{

			//Create the child nodes.
			// if the children are at the bottom level (depth-1)
			// then we hve to create their children for them
			for(size_t ui=0;ui<8;ui++)
			{
				p->children[ui]=new PtOctNode;
				p->children[ui]->pts=0;

				queuedNodes.push_back(make_pair(p->children[ui],curDepth+1));
			}
		}
		else if( queuedNodes.front().second == depth -1)
		{
			for(size_t ui=0;ui<8;ui++)
				p->children[ui]->pts=new vector<size_t>;
		}
		else
		{
			ASSERT(false);
		}

			
		queuedNodes.pop_front();
		
	}
	//--


	//Work out which points go into which octtree node
	//--
	map<PtOctNode*,list<size_t> > targetMap;

	//TODO: Convert to per-thread maps?
#pragma omp parallel for
	for(size_t ui=0;ui<vecPts.size();ui++)
	{
		PtOctNode *p;
		p=getInsertNode(vecPts[ui]);


		map<PtOctNode*,list<size_t> >::iterator it;

		it=targetMap.find(p);
		if(it!=targetMap.end())
		{
			#pragma omp atomic
			it->second.push_back(ui);
		}
		else
		{
			list<size_t> l;
			l.push_back(ui);
			#pragma omp atomic
			targetMap.insert(make_pair(p,ui));
		}

	}
	//--

	//Now spin through the map, assigning each 
	// octant its points
	//--
#pragma omp parallel for
	for(map<PtOctNode*,list<size_t> >::const_iterator it=targetMap.begin();
			it!=targetMap.end();++it)
	{
		size_t numPts;
		PtOctNode *p;
		
		numPts=it->second.size();
		p=it->first;
		
		p->pts->resize(numPts);

		size_t ui;
		ui=0;
		for(list<size_t>::const_iterator itJ=it->second.begin();
				itJ!=it->second.end();++itJ)
		{
			(*(p->pts))[ui]=*itJ;
			ui++;
		}
	}
	//--

}

PtOctNode *PointOctTree::getInsertNode(const Point3D &p) const
{
	BoundCube b=bounds;

	PtOctNode *node=root;
	for(size_t ui=0;ui<depth;ui++)
	{
		size_t octant;
		octant=0;

		Point3D centre;
		centre=b.getCentroid();
		for(size_t uj=0;uj<3;uj++)
		{
			if(p[uj] >=centre[uj])
			{
				//Bring the lower bound up
				b.setBound(uj,0,centre[uj]);
				octant|=(1<<uj);
			}
			else
			{
				//Bring the upper bound down
				b.setBound(uj,1,centre[uj]);
			}

			ASSERT(b.isValid());
		}

		node=node->children[octant];
	}

	return node;
}

//Aspect is Height/width, fov is the fov in the horizontal direction, full angle from left to right of frustrum
void PointOctTree::getNodesInFrustrum(const Point3D &origin, const Point3D &forwards, 
					const Point3D &up,float aspect, float near, float far, float fov,
					vector<size_t> &indicies) const
{

	ASSERT(aspect > 0.0f && aspect < M_PI/2);

	//Convert to half angle
	fov=fov/2.0f;

	Point3D right=forwards.cross(up);

	float tanFOVH = tan(fov);
	float tanFOVV= tanFOVH*aspect;

	float nearPlaneHHeight = tanFOVV*near;  // half height for NP
	float nearPlaneHWidth = tanFOVH*near;  // halfWidth for np

	float farPlaneHeight = tanFOVV*far;    // far plane half height
	float farPlaneHwidth = tanFOVH*far;    // far plane half width

	//Find the equations of the frustrum planes
	Point3D centre[2];
	centre[0] = origin + forwards*near;
	centre[1] = origin + forwards*far;

	Point3D pts[8];
	//Corners for near and far planes
	for(size_t ui=0;ui<4;ui++)
	{
		float m1,m2;
		if ( ui &1)
			m1=-1.0f;
		else
			m1=1.0f;
		if (ui & 2)
			m2=-1.0f;
		else
			m2=1.0f;

		pts[ui] = centre[0] + m1*(up * nearPlaneHHeight) +m2*(right * nearPlaneHWidth);  
		pts[ui+4] = centre[1] + m1*(up * farPlaneHHeight) +m2*(right * farPlaneHWidth);  
	}


	//Set the bounding box for the frustrum
	BoundCube bcFrust;
	bcFrust.setBounds(pts,8);

	//Find all nodes that may lie within the frustrum
	// by not traversing parts of the oct-tree that do not
	// lie within the bound cube
	BoundCube b=bounds;
	for(size_t ui=0;ui<depth;ui++)
	{
		/* FIXME: IMPLEMENT ME
		size_t bits;
		bits=0;
		Point3D centre=bounds.getCentroid();
		for(size_t uj=0;uj<3;uj++)
		{
			if(bounds.getBound(0,uj)  > centre[uj])


		}

		*/

	}

	//having done this, apply the full frusturm test



	//Find the front/back planes for the frustrum

}
#ifdef DEBUG

bool PointOctTree::runUnitTests() const
{
	//TODO: IMPLEMENT ME!
	return false;
}


#endif


