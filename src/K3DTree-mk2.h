/* 
 * K3DTreeMk2.h  - Precise KD tree implementation
 * Copyright (C) 2008  D. Haley
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

#ifndef K3DTREEMK2_H
#define K3DTREEMK2_H

//This is the second revision of my KD tree implementation
//The goals here are, as compared to the first
//	- Improved build performance by minimising memory allocation calls
//	  and avoiding recursive implementations
//	- index based construction for smaller in-tree storage


#include "basics.h"

#include "APTClasses.h" //For IonHit

#include <vector>

//!Functor allowing for sorting of points in 3D
/*! Used by KD Tree to sort points based around which splitting axis is being used
 * once the axis is set, points will be ranked based upon their relative value in
 * that axis only
 */ 
class AxisCompareMk2
{
	private:
		unsigned int axis;
	public:
		void setAxis(unsigned int Axis){axis=Axis;};
		inline bool operator() (const std::pair<Point3D,size_t> &p1,const std::pair<Point3D,size_t> &p2)
		{return p1.first[axis]<p2.first[axis];};
};

#include <vector>

//!Node Class for storing point
class K3DNodeMk2
{
	public:
		//Index of left child in parent tree array
		size_t childLeft;
		//Index of right child in parent tree array
		size_t childRight;

		//Has this point been marked by an external algorithm?
		bool tagged;
};

//!3D specific KD tree
class K3DTreeMk2
{
	private:
		//!The maximum depth of the tree
		size_t maxDepth;

		//!Tree array. First element is spatial data. 
		//Second is original array index upon build
		std::vector<std::pair<Point3D,size_t> > indexedPoints;

		//!Tree node array (stores parent->child relations)
		std::vector<K3DNodeMk2> nodes;

		//!total size of array
		size_t arraySize;

		//!Which entry is the root of the tree?
		size_t treeRoot;

		BoundCube treeBounds;
		
		//Callback for progress reporting
		bool (*callback)(void);

		unsigned int *progress; //Progress counter

	public:
		//KD Tree constructor
		K3DTreeMk2(){};

		//!Cleans up tree, deallocates nodes
		~K3DTreeMk2(){};

		//Reset the points
		void resetPts(std::vector<Point3D> &pts, bool clear=true);
		void resetPts(std::vector<IonHit> &pts, bool clear=true);

		/*! Builds a balanced KD tree from a list of points
		 *  previously set by "resetPts". returns false if callback returns
		 *  false;
		 */	
		bool build();

		void getBoundCube(BoundCube &b) {b.setBounds(treeBounds);}

		//!Textual output of tree. tabs are used to separate different levels of the tree
		/*!The output from this function can be quite large for even modest trees. 
		 * Recommended for debugging only*/
		void dump(std::ostream &,size_t depth=0, size_t offset=-1) const;


		//Find the nearest "untagged" point's internal index.
		//Mark the found point as "tagged" in the tree. Returns -1 on failure (no untagged points) 
		size_t findNearestUntagged(const Point3D &queryPt,
						const BoundCube &b, bool tag=true);

	
		//!Get the contigous node IDs for a subset of points in the tree that are contained
		// within a sphere positioned about pt, with a sqr radius of sqrDist.
		// 	- This does *NOT* get *all* points - only some. 
		// 	- It should be faster than using findNearestUntagged repeatedly 
		// 	  for large enough sqrDist. 
		// 	- It does not check tags.	
		void getTreesInSphere(const Point3D &pt, float sqrDist, const BoundCube &domainCube,
					std::vector<std::pair<size_t,size_t> > &contigousBlocks ) const;

		//Obtain a point from its internal index
		const Point3D *getPt(size_t index) { return &(indexedPoints[index].first);};

		//reset the specified "tags" in the tree
		void clearTags(std::vector<size_t> &tagsToClear);

		size_t getOrigIndex(size_t treeIndex){return indexedPoints[treeIndex].second;};
		
		//Set the callback routine for progress reporting
		void setCallback(bool (*cb)(void)) {callback = cb;}
		
		//Set a pointer that can be used to write the current progress
		void setProgressPointer(unsigned int *p) { progress=p;};

		//Erase tree contents
		void clear() { nodes.clear(); indexedPoints.clear();};
		void tag(size_t tagID,bool tagVal=true) {nodes[tagID].tagged=tagVal;}

		bool getTag(size_t tagID) const { return nodes[tagID].tagged;};

		size_t size() { return indexedPoints.size(); ASSERT(nodes.size() == indexedPoints.size());}
		
		size_t rootIdx() { return treeRoot;}

		size_t tagCount() const;

		void clearAllTags();
};

#endif
