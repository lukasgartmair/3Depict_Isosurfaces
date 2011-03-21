#include "K3DTree-mk2.h"
#include "mathfuncs.h"


#include <stack>
#include <algorithm>
#include <cmath>
#include <utility>

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

	if(!p.size())
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

	maxDepth=0;
	//No indexedPoints? That was easy.
	if(!indexedPoints.size())
		return true;
	
	ASSERT(treeBounds.isValid());

	//Maintain a stack of nodeoffsets, and whether we have built the left hand side
	stack<pair<size_t,size_t> > limits;
	stack<char> buildStatus;

	limits.push(make_pair(0,indexedPoints.size()-1));
	buildStatus.push(BUILT_NONE);
	
	AxisCompareMk2 axisCmp;

	size_t numSeen=0; // for progress reporting	
	size_t median;
	do
	{
		//Sort our current subregion
		//with the exception of this sort, we don't even need to LOOK
		//at the indexedPoints. Its all implicit in the balanced tree strucutre
		median=(limits.top().second+limits.top().first)/2;

		switch(buildStatus.top())
		{
			case BUILT_NONE:
			{
				//First time we have seen this group? OK, we need to sort
				//along its hyper plane.
				axisCmp.setAxis((limits.size()-1)%3);
				std::sort(indexedPoints.begin()+limits.top().first,
						indexedPoints.begin() + limits.top().second,axisCmp);
				nodes[median].tagged=false;
				//Either of these cases results in use 
				//handling the left branch.
				buildStatus.top()++;

				//look to see if there is any left data
				if(median >limits.top().first)
				{
					//There is; we have to branch again
					limits.push(make_pair(
						limits.top().first,median-1));
					
					buildStatus.push(BUILT_NONE);

					size_t newMedian;
					newMedian=(limits.top().first+(median-1))/2;
					nodes[median].childLeft=newMedian;

				}
				else
				{
					//There is not. Set the left branch to null
					nodes[median].childLeft=(size_t)-1;
				}
				break;
			}
			case BUILT_LEFT:
			{
				//Either of these cases results in use 
				//handling the right branch.
				buildStatus.top()++;
				//Check to see if there is any right data
				if(median <limits.top().second)
				{
					//There is; we have to branch again
					limits.push(make_pair(
						median+1,limits.top().second));
					buildStatus.push(BUILT_NONE);

					size_t newMedian;
					newMedian=(limits.top().second+(median+1))/2;
					nodes[median].childRight=newMedian;

				}
				else
				{
					//There is not. Set the right branch to null
					nodes[median].childRight=(size_t)-1;
				}

				break;
			}
			case BUILT_BOTH:
			{
				maxDepth=std::max(maxDepth,limits.size());
				//pop limits and build status.
				limits.pop();
				buildStatus.pop();
				
				ASSERT(limits.size() == buildStatus.size());
				

				numSeen++;
				break;
			}
			default:
				ASSERT(false);

		}	

		if(!(numSeen%PROGRESS_REDUCE) && progress)
		{
			*progress= (unsigned int)((float)numSeen/(float)nodes.size()*100.0f);

			if(!(*callback)())
				return false;
		}
	
	}while(!limits.empty());

	//Set the tree root to the median of the tree
	treeRoot=(indexedPoints.size()-1)/2;

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
		offset=(indexedPoints.size()-1)/2;
	}

	for(size_t ui=0;ui<depth; ui++)
		strm << "\t";

	strm << "(" << indexedPoints[offset].first[0] 
		<< "," << indexedPoints[offset].first[1] << "," << indexedPoints[offset].first[2]
		<< ")" << std::endl;




	if(nodes[offset].childLeft!=(size_t)-1)
	{
		dump(strm,depth+1,nodes[offset].childLeft);
	}
	
	if(nodes[offset].childRight!=(size_t)-1)
		dump(strm,depth+1,nodes[offset].childRight);
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

	if(!nodes.size())
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
						if(bestPoint!=(size_t)-1 && 
							!curDomain.intersects(indexedPoints[bestPoint].first,bestDistSqr))
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
						
						if(bestPoint !=(size_t)-1 && 
							!curDomain.intersects(indexedPoints[bestPoint].first,bestDistSqr))
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
						
						if(bestPoint !=(size_t)-1&& 
							!curDomain.intersects(indexedPoints[bestPoint].first,bestDistSqr))
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
						
						if(bestPoint !=(size_t)-1&& !curDomain.intersects(indexedPoints[bestPoint].first,bestDistSqr))
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
		

void K3DTreeMk2::clearTags(std::vector<size_t> &tagsToClear)
{
#pragma omp parallel for
	for(size_t ui=0;ui<tagsToClear.size();ui++)
		nodes[ui].tagged=false;
}
