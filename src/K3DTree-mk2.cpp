/* 
 * K3DTree-mk2.cpp : 3D Point KD tree - precise implementation 
 * Copyright (C) 2011  D. Haley
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

#include "K3DTree-mk2.h"
#include "mathfuncs.h"


#include <stack>
#include <algorithm>
#include <cmath>
#include <utility>
#include <queue>

using std::stack;
using std::vector;
using std::pair;

void K3DTreeMk2::resetPts(std::vector<Point3D> &p, bool clear)
{
	//Compute bounding box for indexedPoints
	treeBounds.setBounds(p);
	indexedPoints.resize(p.size());

#pragma omp parallel for
	for(size_t ui=0;ui<indexedPoints.size();ui++)
	{
		indexedPoints[ui].first=p[ui];
		indexedPoints[ui].second=ui;
	}

	if(clear)
		p.clear();

	nodes.resize(indexedPoints.size());
}

void K3DTreeMk2::resetPts(std::vector<IonHit> &p, bool clear)
{
	indexedPoints.resize(p.size());
	nodes.resize(p.size());

	if(p.empty())
		return;
	

	//Compute bounding box for indexedPoints
	treeBounds=getIonDataLimits(p);

#pragma omp parallel for
	for(size_t ui=0;ui<indexedPoints.size();ui++)
	{
		indexedPoints[ui].first=p[ui].getPosRef();
		indexedPoints[ui].second=ui;
	}

	if(clear)
		p.clear();

}

bool K3DTreeMk2::build()
{

	using std::make_pair;

	enum
	{
		BUILT_NONE,
		BUILT_LEFT,
		BUILT_BOTH
	};

	
	//Clear any existing tags
	clearAllTags();
	maxDepth=0;

	//No indexedPoints? That was easy.
	if(indexedPoints.empty())
		return true;

	
	ASSERT(treeBounds.isValid());

	//Maintain a stack of nodeoffsets, and whether we have built the left hand side
	stack<pair<size_t,size_t> > limits;
	stack<char> buildStatus;
	stack<size_t> splitStack;

	//Data runs from 0 to size-1 INCLUSIVE
	limits.push(make_pair(0,indexedPoints.size()-1));
	buildStatus.push(BUILT_NONE);
	splitStack.push((size_t)-1);


	AxisCompareMk2 axisCmp;

	size_t numSeen=0; // for progress reporting	
	size_t splitIndex=0;


	
	size_t *childPtr=0;

#ifdef DEBUG
	for(size_t ui=0;ui<nodes.size();ui++)
	{
		nodes[ui].childLeft=nodes[ui].childRight=(size_t)-2;
	}
#endif
	do
	{

		switch(buildStatus.top())
		{
			case BUILT_NONE:
			{
				//OK, so we have not seen this data at this level before
				int curAxis=(limits.size()-1)%3;
				//First time we have seen this group? OK, we need to sort
				//along its hyper plane.
				axisCmp.setAxis(curAxis);
			
				//Sort data; note that the limits.top().second is the INCLUSIVE
				// upper end.	
				std::sort(indexedPoints.begin()+limits.top().first,
						indexedPoints.begin() + limits.top().second+1,axisCmp);

				//Initially assume that the mid node is the median; then we slide it up 
				splitIndex=(limits.top().second+limits.top().first)/2;
				
				//Keep sliding the split towards the upper boundary until we hit a different
				//data value. This ensure that all data on the left of the sub-tree is <= 
				// to this data value for the specified sort axis
				while(splitIndex != limits.top().second
					&& indexedPoints[splitIndex].first.getValue(curAxis) ==
				       		indexedPoints[splitIndex+1].first.getValue(curAxis))
					splitIndex++;

				buildStatus.top()++; //Increment the build status to "left" case.
						

				if(limits.size() ==1)
				{
					//root node
					treeRoot=splitIndex;
				}
				else
					*childPtr=splitIndex;

				//look to see if there is any left data
				if(splitIndex >limits.top().first)
				{
					//There is; we have to branch again
					limits.push(make_pair(
						limits.top().first,splitIndex-1));
					
					buildStatus.push(BUILT_NONE);
					//Set the child pointer, as we don't know
					//the correct value until the next sort.
					childPtr=&nodes[splitIndex].childLeft;
				}
				else
				{
					//There is not. Set the left branch to null
					nodes[splitIndex].childLeft=(size_t)-1;
				}
				splitStack.push(splitIndex);

				break;
			}

			case BUILT_LEFT:
			{
				//Either of these cases results in use 
				//handling the right branch.
				buildStatus.top()++;
				splitIndex=splitStack.top();
				//Check to see if there is any right data
				if(splitIndex <limits.top().second)
				{
					//There is; we have to branch again
					limits.push(make_pair(
						splitIndex+1,limits.top().second));
					buildStatus.push(BUILT_NONE);

					//Set the child pointer, as we don't know
					//the correct value until the next sort.
					childPtr=&nodes[splitIndex].childRight;
				}
				else
				{
					//There is not. Set the right branch to null
					nodes[splitIndex].childRight=(size_t)-1;
				}

				break;
			}
			case BUILT_BOTH:
			{
				ASSERT(nodes[splitStack.top()].childLeft != (size_t)-2
					&& nodes[splitStack.top()].childRight!= (size_t)-2 );
				maxDepth=std::max(maxDepth,limits.size());
				//pop limits and build status.
				limits.pop();
				buildStatus.pop();
				splitStack.pop();
				ASSERT(limits.size() == buildStatus.size());
				

				numSeen++;
				break;
			}
		}	

		if(!(numSeen%PROGRESS_REDUCE) && progress)
		{
			*progress= (unsigned int)((float)numSeen/(float)nodes.size()*100.0f);

			if(!(*callback)())
				return false;
		}

	}while(!limits.empty());


	return true;
}

/*
void K3DTreeMk2::dump(std::ostream &strm) const
{
	enum
	{
		PRINT_NONE,
		PRINT_LEFT,
		PRINT_BOTH
	};

	if(!indexedPoints.size())
		return;

	stack<char> status;
	stack<size_t> nodeStack;
	status.push(PRINT_NONE);
	nodeStack.push(indexedPoints.size()/2);

	do
	{
		for(size_t ui=0;ui<status.size(); ui++)
			strm << "\t";
	
		strm << "(" << indexedPoints[nodeStack.top()].getValue(0) 
			<< "," << indexedPoints[nodeStack.top()].getValue(1) 
			<< "," << indexedPoints[nodeStack.top()].getValue(2) << ")" << std::endl;
		switch(status.top())
		{	
			case PRINT_NONE:
				status.top()++;
				if(nodes[nodeStack.top()].childLeft != -1)
				{
					nodeStack.push(nodes[nodeStack.top()].childLeft);
					status.push(PRINT_NONE);
				}
				break;
			case PRINT_LEFT:
				status.top()++;
				if(nodes[nodeStack.top()].childRight != -1)
				{
					nodeStack.push(nodes[nodeStack.top()].childRight);
					status.push(PRINT_NONE);
				}
				break;
			case PRINT_BOTH:
				status.pop();
				nodeStack.pop();
		}
		
	}while(status.size());
}
*/

void K3DTreeMk2::dump(std::ostream &strm,  size_t depth, size_t offset) const
{
	if(offset==(size_t)-1)
	{
		for(unsigned int ui=0;ui<indexedPoints.size();ui++)
		{
			strm << ui << " "<< indexedPoints[ui].first << std::endl;
		}

		strm << "----------------" << std::endl;
		offset=treeRoot;
	}

	for(size_t ui=0;ui<depth; ui++)
		strm << "\t";

	strm << offset << " : (" << indexedPoints[offset].first[0] 
		<< "," << indexedPoints[offset].first[1] << "," << indexedPoints[offset].first[2]
		<< ")" << std::endl;



	for(size_t ui=0;ui<depth; ui++)
		strm << "\t";
	strm << "<l>" <<std::endl;

	if(nodes[offset].childLeft!=(size_t)-1)
	{
		dump(strm,depth+1,nodes[offset].childLeft);
	}
	for(size_t ui=0;ui<depth; ui++)
		strm << "\t";
	strm << "</l>" <<std::endl;

	for(size_t ui=0;ui<depth; ui++)
		strm << "\t";
	strm << "<r>" <<std::endl;

	if(nodes[offset].childRight!=(size_t)-1)
		dump(strm,depth+1,nodes[offset].childRight);
	
	for(size_t ui=0;ui<depth; ui++)
		strm << "\t";
	strm << "</r>" <<std::endl;
}

size_t K3DTreeMk2::findNearestUntagged(const Point3D &searchPt,
				const BoundCube &domainCube, bool shouldTag)
{
	enum { NODE_FIRST_VISIT, //First visit is when you descend the tree
		NODE_SECOND_VISIT, //Second visit is when you come back from ->Left()
		NODE_THIRD_VISIT // Third visit is when you come back from ->Right()
		};
	
	size_t nodeStack[maxDepth+1];
	float domainStack[maxDepth+1][2];
	unsigned int visitStack[maxDepth+1];

	size_t bestPoint;
	size_t curNode;

	BoundCube curDomain;
	unsigned int visit;
	unsigned int stackTop;
	unsigned int curAxis;
	
	float bestDistSqr;
	float tmpEdge;

	if(nodes.empty())
		return -1;

	bestPoint=(size_t)-1; 
	bestDistSqr =std::numeric_limits<float>::max();
	curDomain=domainCube;
	visit=NODE_FIRST_VISIT;
	curAxis=0;
	stackTop=0;

	//Start at medan of array, which is top of tree,
	//by definition
	curNode=treeRoot;

	//check root node	
	if(!nodes[curNode].tagged)
	{
		float tmpDistSqr;
		tmpDistSqr = indexedPoints[curNode].first.sqrDist(searchPt); 
		if(tmpDistSqr < bestDistSqr)
		{
			bestDistSqr  = tmpDistSqr;
			bestPoint=curNode;
		}
	}

	do
	{
		switch(visit)
		{
			//Examine left branch
			case NODE_FIRST_VISIT:
			{
				if(searchPt[curAxis] < indexedPoints[curNode].first[curAxis])
				{
					if(nodes[curNode].childLeft!=(size_t)-1)
					{
						//Check bounding box when shrunk overlaps best
						//estimate sphere
						tmpEdge= curDomain.bounds[curAxis][1];
						curDomain.bounds[curAxis][1] = indexedPoints[curNode].first[curAxis];
						if(!curDomain.intersects(searchPt,bestDistSqr))
						{
							curDomain.bounds[curAxis][1] = tmpEdge; 
							visit++;
							continue;		
						}	
						//Preserve our current state.
						nodeStack[stackTop]=curNode;
						visitStack[stackTop] = NODE_SECOND_VISIT; //Oh, It will be. It will be.
						domainStack[stackTop][1] = tmpEdge;
						domainStack[stackTop][0]= curDomain.bounds[curAxis][0];
						stackTop++;

						//Update the current information
						curNode=nodes[curNode].childLeft;
						visit=NODE_FIRST_VISIT;
						curAxis++;
						curAxis%=3;
						continue;
					}
				}	
				else
				{
					if(nodes[curNode].childRight!=(size_t)-1)
					{
						//Check bounding box when shrunk overlaps best
						//estimate sphere
						tmpEdge= curDomain.bounds[curAxis][0];
						curDomain.bounds[curAxis][0] = indexedPoints[curNode].first[curAxis];
						
						if(!curDomain.intersects(searchPt,bestDistSqr))
						{
							curDomain.bounds[curAxis][0] =tmpEdge; 
							visit++;
							continue;		
						}	

						//Preserve our current state.
						nodeStack[stackTop]=curNode;
						visitStack[stackTop] = NODE_SECOND_VISIT; //Oh, It will be. It will be.
						domainStack[stackTop][0] = tmpEdge;
						domainStack[stackTop][1]= curDomain.bounds[curAxis][1];
						stackTop++;

						//Update the information
						curNode=nodes[curNode].childRight;
						visit=NODE_FIRST_VISIT;
						curAxis++;
						curAxis%=3;
						continue;	
					}
				}
				visit++;
				//Fall through
			}
			//Examine right branch
			case NODE_SECOND_VISIT:
			{
				if(searchPt[curAxis]< indexedPoints[curNode].first[curAxis])
				{
					if(nodes[curNode].childRight!=(size_t)-1)
					{
						//Check bounding box when shrunk overlaps best
						//estimate sphere
						tmpEdge= curDomain.bounds[curAxis][0];
						curDomain.bounds[curAxis][0] = indexedPoints[curNode].first[curAxis];
						
						if(!curDomain.intersects(searchPt,bestDistSqr))
						{
							curDomain.bounds[curAxis][0] = tmpEdge; 
							visit++;
							continue;		
						}
	
						nodeStack[stackTop]=curNode;
						visitStack[stackTop] = NODE_THIRD_VISIT; 
						domainStack[stackTop][0] = tmpEdge;
						domainStack[stackTop][1]= curDomain.bounds[curAxis][1];
						stackTop++;
						
						//Update the information
						curNode=nodes[curNode].childRight;
						visit=NODE_FIRST_VISIT;
						curAxis++;
						curAxis%=3;
						continue;	

					}
				}
				else
				{
					if(nodes[curNode].childLeft!=(size_t)-1)
					{
						//Check bounding box when shrunk overlaps best
						//estimate sphere
						tmpEdge= curDomain.bounds[curAxis][1];
						curDomain.bounds[curAxis][1] = indexedPoints[curNode].first[curAxis];
						
						if(!curDomain.intersects(searchPt,bestDistSqr))
						{
							curDomain.bounds[curAxis][1] = tmpEdge; 
							visit++;
							continue;		
						}	
						//Preserve our current state.
						nodeStack[stackTop]=curNode;
						visitStack[stackTop] = NODE_THIRD_VISIT; 
						domainStack[stackTop][1] = tmpEdge;
						domainStack[stackTop][0]= curDomain.bounds[curAxis][0];
						stackTop++;
						
						//Update the information
						curNode=nodes[curNode].childLeft;
						visit=NODE_FIRST_VISIT;
						curAxis++;
						curAxis%=3;
						continue;	

					}
				}
				visit++;
				//Fall through
			}
			case NODE_THIRD_VISIT:
			{
				//Decide if we should promote the current node
				//to "best" (ie nearest untagged) node.
				//To promote, it musnt be tagged, and it must
				//be closer than cur best estimate.
				if(!nodes[curNode].tagged)
				{
					float tmpDistSqr;
					tmpDistSqr = indexedPoints[curNode].first.sqrDist(searchPt); 
					if(tmpDistSqr < bestDistSqr)
					{
						bestDistSqr  = tmpDistSqr;
						bestPoint=curNode;
					}
				}

				//DEBUG
				ASSERT(stackTop%3 == curAxis)
				//
				if(curAxis)
					curAxis--;
				else
					curAxis=2;


				
				ASSERT(stackTop < maxDepth+1);	
				if(stackTop)
				{
					stackTop--;
					visit=visitStack[stackTop];
					curNode=nodeStack[stackTop];
					curDomain.bounds[curAxis][0]=domainStack[stackTop][0];
					curDomain.bounds[curAxis][1]=domainStack[stackTop][1];
					ASSERT((stackTop)%3 == curAxis);
				}
			
				break;
			}
			default:
				ASSERT(false);


		}
		

	//Keep going until we meet the root nde for the third time (one left, one right, one finish)	
	}while(!(curNode== treeRoot &&  visit== NODE_THIRD_VISIT));

	if(bestPoint != (size_t) -1)
		nodes[bestPoint].tagged|=shouldTag;
	return bestPoint;	

}


void K3DTreeMk2::getTreesInSphere(const Point3D &pt, float sqrDist, const BoundCube &domainCube,
					vector<pair<size_t,size_t> > &contigousBlocks ) const
{
	using std::queue;
	using std::pair;
	using std::make_pair;

	queue<int> nodeQueue;
	queue<int> axisQueue;
	queue<BoundCube> boundQueue;

	queue<pair<int,int> > limitQueue;
	if(treeRoot == (size_t) -1)
		return;


	nodeQueue.push(treeRoot);
	boundQueue.push(domainCube);
	axisQueue.push(0);
	limitQueue.push(make_pair(0,nodes.size()-1));	
	do
	{
		BoundCube tmpCube;
		tmpCube=boundQueue.front();

		ASSERT(nodeQueue.size() == boundQueue.size() &&
			nodeQueue.size() == axisQueue.size() &&
			nodeQueue.size() == limitQueue.size());
		
		//There are three cases here.
		//	- KD limits for this subdomain
		//	   wholly contained by sphere
		//	   	> contigous subtree is interior.
		//	- KD Limits for this subdomain partially
		//	    contained by sphere.
		//	    	> some subtree may be interior -- refine.
		//	- Does not intersect, do nothing.

		if(tmpCube.containedInSphere(pt,sqrDist))
		{
			//We are? Interesting. We must be a contigous block from our lower
			//to upper limits
			contigousBlocks.push_back(limitQueue.front());
		}
		else if(tmpCube.intersects(pt,sqrDist))
		{
			size_t axis,curNode;
			//Not wholly contained... but our kids might be!
			axis=axisQueue.front();
			curNode=nodeQueue.front();

			if(nodes[curNode].childLeft !=(size_t)-1)
			{
				//Construct left hand domain
				tmpCube=boundQueue.front();
				//Set upper bound
				tmpCube.bounds[axis][1] = indexedPoints[curNode].first[axis];
				
				if(tmpCube.intersects(pt,sqrDist))
				{
					//We have to examine left child.
					nodeQueue.push(nodes[curNode].childLeft);
					boundQueue.push(tmpCube);
					limitQueue.push(make_pair(
						limitQueue.front().first,curNode-1));
					axisQueue.push((axis+1)%3);
				}
			}
			
			if(nodes[curNode].childRight !=(size_t)-1)
			{
				//Construct right hand domain
				tmpCube=boundQueue.front();
				//Set lower bound
				tmpCube.bounds[axis][0] = indexedPoints[curNode].first[axis];
				
				if(tmpCube.intersects(pt,sqrDist))
				{
					//We have to examine right child.
					nodeQueue.push(nodes[curNode].childRight);
					boundQueue.push(tmpCube);
					limitQueue.push(make_pair(curNode+1,limitQueue.front().second ));
					axisQueue.push((axis+1)%3);
				}
			}
		}
	
		axisQueue.pop();
		limitQueue.pop();
		boundQueue.pop();
		nodeQueue.pop();	
	}
	while(!nodeQueue.empty());

}

size_t K3DTreeMk2::tagCount() const
{
	size_t count=0;
	for(size_t ui=0;ui<nodes.size();ui++)
	{
		if(nodes[ui].tagged)
			count++;
	
	}

	return count;
}

void K3DTreeMk2::clearTags(std::vector<size_t> &tagsToClear)
{
#pragma omp parallel for
	for(size_t ui=0;ui<tagsToClear.size();ui++)
		nodes[tagsToClear[ui]].tagged=false;
}

void K3DTreeMk2::clearAllTags()
{
#pragma omp parallel for
	for(size_t ui=0;ui<nodes.size();ui++)
		nodes[ui].tagged=false;
}
