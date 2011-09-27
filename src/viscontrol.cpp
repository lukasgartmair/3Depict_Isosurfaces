/*
 *	viscontrol.cpp - visualation-user interface glue code
 *	Copyright (C) 2011, D Haley 

 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.

 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.

 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "xmlHelper.h"
#include "viscontrol.h"

#include <list>
#include <stack>

#include "scene.h"
#include "drawables.h"

#include "filters/allFilter.h"
#include "translation.h"

using std::list;
using std::stack;

//oh no! global. window to safeYield/ needs to be set
wxWindow *yieldWindow=0;

wxStopWatch* delayTime=0;
//Another global!
bool *abortVisCtlOp=0;

const float DEFAULT_MAX_CACHE_PERCENT=50;

bool wxYieldCallback()
{
	ASSERT(delayTime);
	//Rate limit the updates
	if(delayTime->Time() > 75)
	{
		wxSafeYield(yieldWindow);
		delayTime->Start();
	}

	ASSERT(abortVisCtlOp);
	return !(*abortVisCtlOp);
}


//!Returns true if the testChild is a child of testParent.
// returns false if testchild == testParent, or if the testParent
// is not a parent of testChild.
bool isChild(const tree<Filter *> &treeInst,
		tree<Filter *>::iterator testParent,
		tree<Filter *>::iterator testChild)
{
	// NOTE: A comparison against tree root (treeInst.begin())is INVALID
	// for trees that have multiple base nodes.
	while(treeInst.depth(testChild))
	{
		testChild=treeInst.parent(testChild);
		
		if(testChild== testParent)
			return true;
	}

	return false;
}

size_t countChildFilters(const tree<Filter *> &treeInst,
			const vector<tree<Filter *>::iterator> &nodes)
{
	set<Filter*> childIts;
	for(size_t ui=0;ui<nodes.size();ui++)
	{
		for(tree<Filter*>::pre_order_iterator it=nodes[ui];
				it!=treeInst.end();it++)
			childIts.insert(*it);

	}

	return childIts.size()-nodes.size();
}


VisController::VisController()
{
	targetScene=0;
	nextID=0; 
	targetPlots=0;
	targetRawGrid=0;
	doProgressAbort=false;
	maxCachePercent=DEFAULT_MAX_CACHE_PERCENT;
	cacheStrategy=CACHE_DEPTH_FIRST;
	ASSERT(!abortVisCtlOp);
	abortVisCtlOp=&doProgressAbort;
	amRefreshing=false;
	pendingUpdates=false;
	useRelativePathsForSave=false;
	curProg.reset();

	//Assign global variable its init value
	ASSERT(!delayTime); //Should not have been inited yet.

	delayTime = new wxStopWatch();
}

VisController::~VisController()
{
	//clean up
	for(tree<Filter * >::iterator it=filters.begin(); 
						it!=filters.end(); ++it)
	{
		delete *it;
	}


	//clean up the stash trees
	for(unsigned int ui=0;ui<stashedFilters.size();ui++)
	{
		tree<Filter *> t;
		t=stashedFilters[ui].second;
		for(tree<Filter * >::iterator it=t.begin(); it!=t.end(); ++it)
			delete *it;
	}

	//Delete the undo stack trees
	while(undoFilterStack.size())
	{
		//clean up
		for(tree<Filter * >::iterator it=undoFilterStack.back().begin(); 
							it!=undoFilterStack.back().end(); ++it)
		{
			delete *it;
		}

		undoFilterStack.pop_back();	
	}

	//Delete the redo stack trees
	while(redoFilterStack.size())
	{
		//clean up
		for(tree<Filter * >::iterator it=redoFilterStack.back().begin(); 
							it!=redoFilterStack.back().end(); ++it)
		{
			delete *it;
		}

		redoFilterStack.pop_back();	
	}

	ASSERT(delayTime);
	//delete global variable that visControl is responsible for
	delete delayTime;
	delayTime=0;
}
	
void VisController::setScene(Scene *theScene)
{
	targetScene=theScene;
	//Set a default camera as needed. We don't need to track its unique ID, as this is
	//"invisible" to the UI
	if(!targetScene->getNumCams())
	{
		Camera *c=new CameraLookAt();
		unsigned int id;
		id=targetScene->addCam(c);
		targetScene->setActiveCam(id);
	}

	//Inform scene about vis control.
	targetScene->setViscontrol(this);
}
	
void VisController::setYieldWindow(wxWindow *newYield)
{
	yieldWindow=newYield;
}

unsigned int VisController::LoadIonSet(const std::string &filename, Filter *filtPos,Filter *down)
{

	//Test to see if we can open the file
	ifstream f(filename.c_str());

	if(!f)
	{
		delete filtPos;
		delete down;
		return POS_OPEN_FAIL;
	}

	f.close();

	ASSERT(filtPos->getType() == FILTER_TYPE_POSLOAD);
	ASSERT(down->getType() == FILTER_TYPE_IONDOWNSAMPLE);
	DataLoadFilter *p=(DataLoadFilter*)filtPos;
	//Save current filter state to undo stack
	pushUndoStack();

	p->setFilename(filename);


	tree<Filter *>::iterator it;
	//From version 1.45 of ttree.h the top element should be added
	// using 'insert'.	
	if(!filters.size())
		it=filters.insert(filters.begin(),p);
	else
		it=filters.insert_after(filters.begin(),p);

	IonDownsampleFilter *d=(IonDownsampleFilter*)down;
	//Append a new filter to the filter tree
	filters.append_child(it,d);

	return 0;
}

void VisController::updateWxTreeCtrl(wxTreeCtrl *t, const Filter *visibleFilt)
{
	stack<wxTreeItemId> treeIDs;
	wxTreeItemId visibleTreeId;
	//Warning: this generates an event, 
	//most of the time (some windows versions do not according to documentation)
	t->DeleteAllItems();

	//Clear the mapping
	filterTreeMapping.clear();
	nextID=0;
	
	int lastDepth=0;
	//Add dummy root node. This will be invisible to wxTR_HIDE_ROOT controls
	wxTreeItemId tid;
	tid=t->AddRoot(wxT("TreeBase"));
	t->SetItemData(tid,new wxTreeUint(nextID));

	// Push on stack to prevent underflow, but don't keep a copy, 
	// as we will never insert or delete this from the UI	
	treeIDs.push(tid);

	nextID++;	
	//Depth first  add
	for(tree<Filter * >::pre_order_iterator filtIt=filters.begin();
					filtIt!=filters.end(); filtIt++)
	{	
		//Push or pop the stack to make it match the iterator position
		if( lastDepth > filters.depth(filtIt))
		{
			while(filters.depth(filtIt) +1 < (int)treeIDs.size())
				treeIDs.pop();
		}
		else if( lastDepth < filters.depth(filtIt))
		{
			treeIDs.push(tid);
		}
		

		lastDepth=filters.depth(filtIt);
	
		//This will use the user label or the type string.	
		tid=t->AppendItem(treeIDs.top(),
			wxStr((*filtIt)->getUserString()));
		
		//If we have been given a child ID, and it matches this item.
		//remember this ID, because we need to ensure that it is visible later
		if(visibleFilt && *filtIt == visibleFilt)
			visibleTreeId=tid;
		
		t->SetItemData(tid,new wxTreeUint(nextID));
	

		//Record mapping to filter for later reference
		filterTreeMapping.push_back(std::make_pair(nextID,*filtIt));
		nextID++;
	}

	if(visibleTreeId.IsOk())
	{
		t->EnsureVisible(visibleTreeId);
		t->SelectItem(visibleTreeId);
	}

	t->GetParent()->Layout();

}

void VisController::updateFilterPropGrid(wxPropertyGrid *g,unsigned long long filterId)
{

	Filter *targetFilter;
	targetFilter=getFilterByIdNonConst(filterId);
	updateFilterPropertyGrid(g,targetFilter);
}

const Filter* VisController::getFilterById(unsigned long long filterId) const
{
	return getFilterByIdNonConst(filterId);
}	

Filter* VisController::getFilterByIdNonConst(unsigned long long filterId) const
{

	Filter *targetFilter=0;

	//Look up tree mapping 
	for(std::list<std::pair<unsigned long long, Filter *> >::const_iterator it=filterTreeMapping.begin();
			it!=filterTreeMapping.end(); ++it)
	{
		if(it->first == filterId)
		{
			targetFilter=it->second;
			break;	
		}
	}

	ASSERT(targetFilter);
	return targetFilter;
}	

bool VisController::setFilterProperties(unsigned long long filterId, unsigned int set, 
						unsigned int key, const std::string &value, bool &needUpdate)
{
	//Save current filter state to undo stack
	pushUndoStack();
	
	Filter *targetFilter=0;
	targetFilter=getFilterByIdNonConst(filterId);

	if(!targetFilter->setProperty(set,key,value,needUpdate))
		return false;

	//If we no longer have a cache, and the filter needs an update, then we must
	//modify the downstream objects
	if(needUpdate)
	{
		for(tree<Filter * >::pre_order_iterator filtIt=filters.begin();
						filtIt!=filters.end(); filtIt++)
		{
			if(targetFilter == *filtIt)
			{
				//Kill all cache below filtIt
				for(tree<Filter *>::pre_order_iterator it(filtIt);it!= filters.end(); ++it)
				{
					//Do not traverse siblings
					if(filters.depth(filtIt) >= filters.depth(it) && it!=filtIt )
						break;

					//Do not clear the cache for the target filter. 
					//This is the respnsibility of the setProperty function for the filter
					if(*it !=targetFilter)
						(*it)->clearCache();
				}

			}
		}

	}

	initFilterTree();
	return true;

}

bool VisController::setCamProperties(unsigned long long camUniqueID,unsigned int key, const std::string &value)
{
	return targetScene->setCamProperty(camUniqueID,key,value);
}

//MUST re-force an update after calling this; as it is an error
//to attempt to re-use this ID.
void VisController::removeTreeFilter(unsigned long long tId )
{
	//Save current filter state to undo stack
	pushUndoStack();

	Filter *removeFilt=getFilterByIdNonConst(tId);

	ASSERT(removeFilt);	

	//Remove element and all children
	for(tree<Filter * >::pre_order_iterator filtIt=filters.begin();
					filtIt!=filters.end(); filtIt++)
	{
		if(removeFilt == *filtIt)
		{

			for(tree<Filter *>::pre_order_iterator it(filtIt);it!= filters.end(); ++it)
			{
				//Do not traverse siblings
				if(filters.depth(filtIt) >= filters.depth(it) && it!=filtIt )
					break;
				
				//Delete the children filters.
				delete *it;
			}

			//Remove the children from the tree
			filters.erase_children(filtIt);
			filters.erase(filtIt);
			break;
		}

	}

	//Topology has changed, notify filters
	initFilterTree();
}

void VisController::addFilter(Filter *f,unsigned long long parentId)
{
	//Save current filter state to undo stack
	pushUndoStack();

	Filter *parentFilter=0;

	parentFilter=getFilterByIdNonConst(parentId);

	tree<Filter *>::iterator it= std::find(filters.begin(),filters.end(),parentFilter);

	ASSERT(it != filters.end());

	//Add the child to the tree
	filters.append_child(it,f);

	filterTreeMapping.push_back(make_pair(nextID,f));
	nextID++;


	//Topology has changed, notify filters
	initFilterTree();
}

void VisController::popPointerStack(std::list<const FilterStreamData *> &pointerTrackList,
				std::stack<vector<const FilterStreamData * > > &inDataStack, unsigned int depth) const
{

	while(inDataStack.size() > depth)
	{
		//Look at each filter pointer on this stack level.
		for(unsigned int ui=0; ui<inDataStack.top().size(); ui++)
		{
			const FilterStreamData *thisData;
			thisData=inDataStack.top()[ui];
			//Search through the pointer list. If we find it,
			//it is an unhandled pointer and should be erased.
			//If not, it has been handled.
			list<const FilterStreamData *>::iterator it;
			it=find(pointerTrackList.begin(),pointerTrackList.end(),thisData);
			if(it!=pointerTrackList.end())
			{
				ASSERT(thisData->cached ==0);

				delete thisData;
				
				//Remove from tracking list. This has been erased
				pointerTrackList.erase(it);
				//Pointer should be only in the track list once.
				ASSERT(find(pointerTrackList.begin(),
					pointerTrackList.end(),thisData) == pointerTrackList.end());
			}
		}
		//We no longer need this level
		inDataStack.pop();
	}
}

void VisController::initFilterTree() const
{

	//TODO: This shares a lot of code with the refresh function. Could
	//	share better (i.e. not cut and paste)
	vector< const FilterStreamData *> curData;
	stack<vector<const FilterStreamData * > > inDataStack;
	list<vector<const FilterStreamData * > > outData;

	list<const FilterStreamData *> pointerTrackList;
	
	//Do not allow stack to empty
	inDataStack.push(curData);
	//Depth-first search from root node, refreshing filters as we proceed.
	for(tree<Filter * >::pre_order_iterator filtIt=filters.begin();
					filtIt!=filters.end(); filtIt++)
	{
		//Step 0 : Pop the cache until we reach our current level, 
		//	deleting any pointers that would otherwise be lost.
		//---
		popPointerStack(pointerTrackList,
			inDataStack,filters.depth(filtIt)+1);
		//---
			
		//Step 1: Take the stack top, and turn it into "curdata" using the filter
		//	record the result on the stack
		//---
		//Take the stack top, filter it and generate "curData"
		(*filtIt)->initFilter(inDataStack.top(),curData);

		//Step 2: Track pointers as needed. Leaves need to be placed in
		//outdata. Missing pointers need to be garbage collected. New non-leaf
		//pointers need to be tracked

		//is this node a leaf of the tree?
		bool isLeaf;
		isLeaf=false;
		for(tree<Filter *>::leaf_iterator leafIt=filters.begin_leaf();
				leafIt!=filters.end_leaf(); leafIt++)
		{
			if(*leafIt == *filtIt)
			{
				isLeaf=true;
				break;
			}
		}
	
		//If this is not a leaf, keep track of intermediary pointers
		if(!isLeaf)
		{
			//The filter will generate a list of new pointers. If any out-going data 
			//streams are un-cached, track them
			for(unsigned int ui=0;ui<curData.size(); ui++)
			{
				//Keep an eye on this pointer. It is not cached (owned) by the filter
				//and needs to be tracked (for possible later deletion)
				ASSERT(curData[ui]);

				list<const FilterStreamData *>::iterator it;
				it = find(pointerTrackList.begin(),pointerTrackList.end(),curData[ui]);
				//Caching is *Forbidden* in filter initialisation
				ASSERT(!curData[ui]->cached);
				
				//Check that we are not already tracking it.
				if(it!=pointerTrackList.end()) 
				{
					//track pointer.
					pointerTrackList.push_back(curData[ui]);
				}
			}	
			
			//Put this in the intermediary stack, 
			//so it is available for any other children at this leve.
			inDataStack.push(curData);
		}
		else
		{
			//The filter has created an ouput. Record it for passing to updateScene
			outData.push_back(curData);
			for(unsigned int ui=0;ui<curData.size();ui++)
			{
				//Search through the pointer list. If we find it,
				//then it is handled by VisController::updateScene, 
				//and we do not need to delete it using the stack
				//if we don't find it, then it is a new pointer, and we still don't need
				//to delete it. At any rate, we need to make sure that it is not
				//deleted by the popPointerStack function.
				list<const FilterStreamData *>::iterator it;
				it=find(pointerTrackList.begin(),pointerTrackList.end(),
							curData[ui]);

				if(it!=pointerTrackList.end())
					pointerTrackList.erase(it);

				//Pointer should be only in the track list once.
				ASSERT(find(pointerTrackList.begin(),pointerTrackList.end(),
							curData[ui]) == pointerTrackList.end());
			
			}
		}	

		//Cur data is recorded either in outDta or on the data stack
		curData.clear();
		//---

	}

	popPointerStack(pointerTrackList,inDataStack,0);
	
	//Pointer tracking list should be empty.
	ASSERT(pointerTrackList.size() == 0);
	ASSERT(inDataStack.size() ==0);

	//mop up the output
	list<const FilterStreamData *> deletedPtrs;
	for(list<vector<const FilterStreamData * > >::iterator 
					it=outData.begin();it!=outData.end(); ++it)

	{

		//Only delete things once.
		for(size_t ui=0; ui<it->size();ui++)
		{
			if(find(deletedPtrs.begin(),deletedPtrs.end(),(*it)[ui]) == deletedPtrs.end())
			{
				deletedPtrs.push_back((*it)[ui]);
				delete (*it)[ui];
			}
		}
	}
}

void VisController::getFilterRefreshStarts(vector<tree<Filter *>::iterator > &propStarts) const
{

	if(!filters.size())
		return;

	const bool STUPID_ALGORITHM=false;
	if(STUPID_ALGORITHM)
	{
		//Stupid version
		//start at root every time
		propStarts.push_back(filters.begin());
	}
	else
	{
		//Do something hopefully non-stupid. Here we examine the types of data that are
		//propagated through the tree, and which filters emit, or block transmission
		//of any given type (ie their output is influenced only by certain data types).

		//From this information, and the cache status of each filter
		//(recall caches only cache data generated inside the filter), it is possible to
		//skip certain initial element refreshes.

		//Block and emit adjuncts for tree
		map<Filter *, size_t> accumulatedEmitTypes,accumulatedBlockTypes;


		//Build the accumulated emit type map. This describes
		//what possible types can be emitted at any point in the tree.
		for(tree<Filter *>::iterator it=filters.begin_breadth_first(); 
					it!=filters.end_breadth_first(); ++it)
		{
			//FIXME: HACK -- why does the BFS not terminate correctly?
			if(!filters.is_valid(it))
				break;

			size_t curEmit;
			//Root node is special, does not combine with the 
			//previous filter
			if(!filters.depth(it)) 
				curEmit=(*it)->getRefreshEmitMask();
			else
			{
				//Normal child. We need to remove any types that
				//are blocked (& (~blocked)), then add any types that are emitted
				//(|)
				curEmit=accumulatedEmitTypes[*(filters.parent(it))];
				curEmit&=(~(*it)->getRefreshBlockMask() )& STREAMTYPE_MASK_ALL;
				curEmit|=(*it)->getRefreshEmitMask();
			}
			
			accumulatedEmitTypes.insert(make_pair(*it,curEmit));
						
		}

		//Build the accumulated block map;  this describes
		//what types, if emitted, will NOT be propagated to the final output
		//Nor affect any downstream filters
		for(size_t ui=filters.max_depth()+1; ui; )
		{
			ui--;

			for(tree<Filter * >::iterator it=filters.begin();it!=filters.end();++it)
			{
				//Check to see if we are at the correct depth
				if(filters.depth(it) != ui)
					continue;


				//Initially assume that everything is passed through
				//filter
				int blockMask=0x0;


				if((*it)->haveCache())
				{
					//Loop over the children of this filter, grab their block masks
					for(tree<Filter *>::sibling_iterator itJ=it.begin(); itJ!=it.end();itJ++)
					{

						if((*itJ)->haveCache())
						{
							int curBlockMask;
							curBlockMask=(*itJ)->getRefreshBlockMask();
							blockMask= (blockMask & curBlockMask);

						}
						else
						{
							blockMask&=0;
							//The only reason to keep looping is to 
							//alter the blockmask. If it is at any point zero,
							//then the answer will be zero, due to the & operator.
							break;
						}

					}
				
					//OK, so we now know which filters the children will ALL block.
					//Combine this with our block list for this filter, and we this will give ush
					//the blocklist for this subtree section
					blockMask|=(*it)->getRefreshBlockMask();
				}
				else
					blockMask=0;
			
				accumulatedBlockTypes.insert(make_pair(*it,blockMask));
			}
		
		}
		
		vector<tree<Filter *>::iterator > seedFilts;
	



		for(tree<Filter *>::iterator it=filters.begin_breadth_first(); 
					it!=filters.end_breadth_first(); ++it)
		{
			//FIXME: HACK -- why does the BFS not terminate correctly?
			if(!filters.is_valid(it))
				break;

			//Check to see if we have an insertion point above us.
			//if so, we cannot press on, as we have determined that
			//we must start higher up.
			bool isChildFilt;
			isChildFilt=false;
			for(unsigned int ui=0;ui<seedFilts.size();ui++)
			{
				if(isChild(filters,seedFilts[ui],it))
				{
					isChildFilt=true;
					continue;
				}
			}

			if(isChildFilt)
				continue;
			
			//If we are a leaf, then we have to do our work, or nothing will be generated
			//so check that
			bool isLeaf;
			isLeaf=false;
			for(tree<Filter*>::leaf_iterator  itJ= filters.begin_leaf(); 
							itJ!=filters.end_leaf(); itJ++)

			{
				if(itJ == it)
				{
					isLeaf=true;
					seedFilts.push_back(it);
					break;
				}
			}

			if(isLeaf)
				continue;

			//Check to see if we can use these children as insertion
			//points in the tree
			//i.e., ask, "Do all subtrees block everything we emit from here?"
			int emitMask,blockMask;
			emitMask=accumulatedEmitTypes[*it];
			blockMask=~0;
			for(tree<Filter *>::sibling_iterator itJ=filters.begin(it); itJ!=filters.end(it);itJ++)
				blockMask&=accumulatedBlockTypes[*itJ];


			 


			if( emitMask & ((~blockMask) & STREAMTYPE_MASK_ALL)) 
			{

				//Oh noes! we don't block, we will have to stop here,
				// for this subtree. We cannot go further down.
				seedFilts.push_back(it);
			}

		}


		propStarts.swap(seedFilts);


	}

#ifdef DEBUG
	for(unsigned int ui=0;ui<propStarts.size();ui++)
	{
		for(unsigned int uj=ui+1;uj<propStarts.size();uj++)
		{
			//Check for uniqueness
			ASSERT(propStarts[ui] !=propStarts[uj]);


			//Check for no-parent relation (either direction)
			ASSERT(!isChild(filters,propStarts[ui],propStarts[uj]) &&
				!isChild(filters,propStarts[uj],propStarts[ui]));
		}
	}



#endif



}

unsigned int VisController::refreshFilterTree(list<std::pair<Filter *,vector<const FilterStreamData * > > > &outData)
{
	amRefreshing=true;
	doProgressAbort=false;
	delayTime->Start();
	
	unsigned int errCode=0;

	if(!filters.size())
	{
		targetScene->clearObjs();
		return 0;	
	}

	//Destroy any caches that belong to monitored filters that need
	//refreshing. Failing to do this can lead to filters being skipped
	//during the refresh 
	for(tree<Filter *>::iterator filterIt=filters.begin();
			filterIt!=filters.end();++filterIt)
	{
		//We need to clear the cache of *all* 
		//downstream filters, as otherwise
		//their cache's could block our update.
		if((*filterIt)->monitorNeedsRefresh())
		{
			for(tree<Filter *>::pre_order_iterator it(filterIt);it!= filters.end(); ++it)
			{
				//Do not traverse siblings
				if(filters.depth(filterIt) >= filters.depth(it) && it!=filterIt )
					break;
			
				(*it)->clearCache();
			}
		}
	}


	// -- Build data streams --	
	vector< const FilterStreamData *> curData;
	stack<vector<const FilterStreamData * > > inDataStack;

	list<const FilterStreamData *> pointerTrackList;

	//Push some dummy data onto the stack to prime first-pass (call to refresh(..) requires stack
	//size to be non-zero)
	inDataStack.push(curData);

	//Keep redoing the refresh until the user stops fiddling with the filter tree.
	do
	{
		if(pendingUpdates)
		{
			getFilterUpdates();
			curProg.reset();
		}

		vector<tree<Filter *>::iterator> baseTreeNodes;

		baseTreeNodes.clear();

		getFilterRefreshStarts(baseTreeNodes);
		

		curProg.totalNumFilters=countChildFilters(filters,baseTreeNodes)+baseTreeNodes.size();

		for(unsigned int itPos=0;itPos<baseTreeNodes.size(); itPos++)
		{
			//Depth-first search from root node, refreshing filters as we proceed.
			for(tree<Filter * >::pre_order_iterator filtIt=baseTreeNodes[itPos];
							filtIt!=filters.end(); ++filtIt)
			{
				//Check to see if this node is a child of the base node.
				//if not, move on.
				if( filtIt!= baseTreeNodes[itPos] &&
					!isChild(filters,baseTreeNodes[itPos],filtIt))
					continue;


				//Step 0 : Pop the cache until we reach our current level, 
				//	deleting any pointers that would otherwise be lost.
				//	Recall that the zero size in the stack may not correspond to the
				//	tree root, but rather corresponds to our insertion level
				//---
				popPointerStack(pointerTrackList,
					inDataStack,filters.depth(filtIt) - filters.depth(baseTreeNodes[itPos])+1);
				//---

				//Step 1: Set up the progress system
				//---
				curProg.clock();
				curProg.curFilter=*filtIt;	
				//---
				
				//Step 2: Check if we should cache this filter or not.
				//Get the number of bytes that the filter expects to use
				//---
				unsigned long long cacheBytes;
				if(inDataStack.empty())
					cacheBytes=(*filtIt)->numBytesForCache(0);
				else
					cacheBytes=(*filtIt)->numBytesForCache(numElements(inDataStack.top()));

				if(cacheBytes != (unsigned long long)(-1))
				{
					//As long as we have caching enabled, let us cache according to the
					//selected strategy
					switch(cacheStrategy)
					{
						case CACHE_NEVER:
							(*filtIt)->setCaching(false);
							break;
						case CACHE_DEPTH_FIRST:
							(*filtIt)->setCaching(cacheBytes/(1024*1024) < maxCachePercent*getAvailRAM());
							break;

					}
				}
				else
					(*filtIt)->setCaching(false);

				//---

				//Step 3: Take the stack top, and turn it into "curdata" and refresh using the filter.
				//	Record the result on the stack.
				//	We also record any Selection devices that are generated by the filter.
				//	This is the guts of the system.
				//---
				//
				(*wxYieldCallback)();


				//Take the stack top, filter it and generate "curData"
				errCode=(*filtIt)->refresh(inDataStack.top(),
							curData,curProg,wxYieldCallback);

#ifdef DEBUG
				//Perform sanity checks on filter output
				checkRefreshValidity(curData,*filtIt);
#endif
				//Ensure that (1) yield is called, regardless of what filter does
				//(2) yield is called after 100% update	
				curProg.filterProgress=100;	
				(*wxYieldCallback)();


				//Retrieve the user interaction "devices", and send them to the scene
				vector<SelectionDevice<Filter> *> curDevices;
				(*filtIt)->getSelectionDevices(curDevices);
				targetScene->addSelectionDevices(curDevices);
				curDevices.clear();

				//Retrieve any console messages from the filter
				vector<string> consoleMessages;
				(*filtIt)->getConsoleStrings(consoleMessages);
				updateConsole(consoleMessages,(*filtIt));

				//check for any error in filter update (including user abort)
				if(errCode)
				{
					//clear any intermediary pointers
					popPointerStack(pointerTrackList,inDataStack,0);
					ASSERT(pointerTrackList.size() == 0);
					ASSERT(inDataStack.size() ==0);
					amRefreshing=false;
					return errCode;
				}


				//Update the filter output information
				(*filtIt)->updateOutputInfo(curData);
				
				
				//is this node a leaf of the tree?
				bool isLeaf;
				isLeaf=false;
				for(tree<Filter *>::leaf_iterator leafIt=filters.begin_leaf();
						leafIt!=filters.end_leaf(); leafIt++)
				{
					if(*leafIt == *filtIt)
					{
						isLeaf=true;
						break;
					}
				}
			
				//If this is not a leaf, keep track of intermediary pointers
				if(!isLeaf)
				{
					//The filter will generate a list of new pointers. If any out-going data 
					//streams are un-cached, track them
					for(unsigned int ui=0;ui<curData.size(); ui++)
					{
						//Keep an eye on this pointer. It is not cached (owned) by the filter
						//and needs to be tracked (for possible later deletion)
						ASSERT(curData[ui]);

						list<const FilterStreamData *>::iterator it;
						it = find(pointerTrackList.begin(),pointerTrackList.end(),curData[ui]);
						//Check it is not cached, and that we are not already tracking it.
						if(!curData[ui]->cached && it!=pointerTrackList.end()) 
						{
							//track pointer.
							pointerTrackList.push_back(curData[ui]);
						}
					}	
					
					//Put this in the intermediary stack, 
					//so it is available for any other children at this leve.
					inDataStack.push(curData);
				}
				else
				{
					//The filter has created an ouput. Record it for passing to updateScene
					outData.push_back(make_pair(*filtIt,curData));
					for(unsigned int ui=0;ui<curData.size();ui++)
					{
						//Search through the pointer list. If we find it,
						//then it is handled by VisController::updateScene, 
						//and we do not need to delete it using the stack
						//if we don't find it, then it is a new pointer, and we still don't need
						//to delete it. At any rate, we need to make sure that it is not
						//deleted by the popPointerStack function.
						list<const FilterStreamData *>::iterator it;
						it=find(pointerTrackList.begin(),pointerTrackList.end(),
									curData[ui]);

						//The pointer is an output from the filter, so we don't need to track it
						if(it!=pointerTrackList.end())
							pointerTrackList.erase(it);

						//Pointer should be only in the track list once.
						ASSERT(find(pointerTrackList.begin(),pointerTrackList.end(),
									curData[ui]) == pointerTrackList.end());
					
					}
				}	

				//Cur data is recorded either in outDta or on the data stack
				curData.clear();
				//---
				
				//check for any pending updates to the filter data
				//in case we need to start again, user has changed something
				if(pendingUpdates)
					break;
			}

		}
		
		popPointerStack(pointerTrackList,inDataStack,0);
	}while(pendingUpdates);
	
	
	//Pointer tracking list should be empty.
	ASSERT(pointerTrackList.size() == 0);
	ASSERT(inDataStack.size() ==0);

	//Progress data should reflect number of filters
	//ASSERT(curProg.totalProgress == numFilters());

	//====Output scrubbing ===

	//Should be no duplicate pointers in output data.
	//(this makes preventing double frees easier, and
	// minimises unneccesary output)
	//Construct a single list of all pointers in output,
	//checking for uniqueness. Delete duplicates
	list<const FilterStreamData *> flatList;
	
	for(list<std::pair<Filter *,vector<const FilterStreamData * > > >::iterator 
					it=outData.begin();it!=outData.end(); ++it)
	{
		vector<const FilterStreamData *>::iterator itJ;

		itJ=it->second.begin();
		while(itJ!=it->second.end()) 
		{
			//Each stream data pointer should only occur once in the entire lot.
			if(find(flatList.begin(),flatList.end(),*itJ) == flatList.end())
			{
				flatList.push_back(*itJ);
				++itJ;
			}
			else
				itJ=it->second.erase(itJ);
		}
	}

	//outData List is complete, and contains only unique entries. clear the checking list.
	flatList.clear();

	//======

	//Stop timer
	delayTime->Pause();


	return 0;
}

bool VisController::hasUpdates() const
{
	if(pendingUpdates)
		return true;

	for(tree<Filter *>::iterator it=filters.begin();it!=filters.end();++it)
	{
		if((*it)->monitorNeedsRefresh())
			return true;
	}

	return false;
}


void VisController::getFilterUpdates()
{
	vector<pair<const Filter *,SelectionBinding> > bindings;
	targetScene->getModifiedBindings(bindings);

	if(bindings.size())
		pushUndoStack();

	for(unsigned int ui=0;ui<bindings.size();ui++)
	{
#ifdef DEBUG
		bool haveBind;
		haveBind=false;
#endif
		for(tree<Filter *>::iterator it=filters.begin(); it!=filters.end();++it)
		{
			if(*it  == bindings[ui].first)
			{
				//We are modifying the contents of
				//the filter, this could make a chankge that
				//modifies output so we need to invalidate 
				//all subtree caches to force reprocessing
				invalidateCache(*it);

				(*it)->setPropFromBinding(bindings[ui].second);
#ifdef DEBUG
				haveBind=true;
#endif
				break;
			}
		}

		ASSERT(haveBind);

	}

	//we have retrieved the updates.
	pendingUpdates=false;
}

unsigned int VisController::updateScene()
{
	typedef std::pair<Filter *,vector<const FilterStreamData * > > filterOutputData;
	list<filterOutputData > refreshData;

	//Apply any remaining updates if we have them
	if(pendingUpdates)
		getFilterUpdates();
	targetScene->clearBindings();

	//Run the tree refresh system.
	//we will delete the pointers ourselves, rather than
	//using safedelete so we ca free memory as we go.
	unsigned int errCode=0;
	errCode=refreshFilterTree(refreshData);
	if(errCode)
		return errCode;

	//strip off the source filter information for
	//convenience in this routine
	list<vector<const FilterStreamData *> > outData;

	for(list<filterOutputData>::iterator it=refreshData.begin(); it!=refreshData.end(); ++it)
		outData.push_back(it->second);

	// -- Build target scene --- 
	targetScene->clearObjs();
	targetScene->clearRefObjs();
	
	//Construct a new drawing object for ions
	vector<DrawManyPoints *> drawIons;

	//erase the contents of each plot in turn
	ASSERT(targetPlots);
	targetPlots->clear(true); //Clear, but preserve visibity information.

	ASSERT(amRefreshing);
	plotSelList->Clear();


	//Loop through the outputs from the filters to construct the input
	//to the scene & plot windows
	for(list<vector<const FilterStreamData *> > ::iterator it=outData.begin(); 
							it!=outData.end(); ++it)
	{
		for(unsigned int ui=0;ui<it->size(); ui++)
		{
			//Filter must specify whether it is cached or not. other values
			//are inadmissible, but useful to catch uninitialised values
			ASSERT((*it)[ui]->cached == 0 || (*it)[ui]->cached == 1);
			switch((*it)[ui]->getStreamType())
			{
				case STREAM_TYPE_IONS:
				{
					//Create a new group for this stream. We have to have individual groups because of colouring.
					DrawManyPoints *curIonDraw;
					curIonDraw=new DrawManyPoints;
					//Iterator points to vector. Typecast elements in vector to IonStreamData s
					const IonStreamData *ionData;
					ionData=((const IonStreamData *)((*it)[ui]));

					//ASSERT(ionData->data.size());
					//Can't just do a swap, as we need to strip the m/c value.
					for(size_t ui=0;ui<ionData->data.size();ui++)
						curIonDraw->addPoint(ionData->data[ui].getPosRef());

					//Set the colour from the ionstream data
					curIonDraw->setColour(ionData->r,
								ionData->g,
								ionData->b,
								ionData->a);
					//set the size from the ionstream data
					curIonDraw->setSize(ionData->ionSize);
					//Randomly shuffle the ion data before we draw it
					curIonDraw->shuffle();

					
					drawIons.push_back(curIonDraw);

					ASSERT(ionData->cached == 1 ||
						ionData->cached == 0);

					if(!ionData->cached)
						delete ionData;



					break;
				}
				case STREAM_TYPE_PLOT:
				{
					
					//Draw plot
					const PlotStreamData *plotData;
					plotData=((PlotStreamData *)((*it)[ui]));
					
					//The plot should have some data in it.
					ASSERT(plotData->GetNumBasicObjects());
					//The plto should have a parent filter
					ASSERT(plotData->parent);
					//The plot should have an index, so we can keep
					//filter choices between refreshes (where possible)
					ASSERT(plotData->index !=(unsigned int)-1);
					//Construct a new plot
					unsigned int plotID;

					//Create a 1D plot
					Plot1D *oneDPlot = new Plot1D;

					oneDPlot->setData(plotData->xyData);
					oneDPlot->setLogarithmic(plotData->logarithmic);

					//Construct any regions that the plot may have
					for(unsigned int ui=0;ui<plotData->regions.size();ui++)
					{
						//add a region to the plot,
						//using the region data stored
						//in the plot stream
						oneDPlot->addRegion(plotID,
							plotData->regionID[ui],
							plotData->regions[ui].first,
							plotData->regions[ui].second,
							plotData->regionR[ui],
							plotData->regionG[ui],
							plotData->regionB[ui],plotData->regionParent);
					}

					plotID=targetPlots->addPlot(oneDPlot);
					
					targetPlots->setTraceStyle(plotID,plotData->plotType);
					targetPlots->setColours(plotID,plotData->r,
								plotData->g,plotData->b);

					std::wstring xL,yL,titleW;
					std::string x,y,t;

					xL=stlStrToStlWStr(plotData->xLabel);
					yL=stlStrToStlWStr(plotData->yLabel);
					titleW=stlStrToStlWStr(plotData->dataLabel);

					x=stlWStrToStlStr(xL);
					y=stlWStrToStlStr(yL);
					t=stlWStrToStlStr(titleW);

					targetPlots->setStrings(plotID,x,y,t);
					targetPlots->setParentData(plotID,plotData->parent,
								plotData->index);
					

					//Append the plot to the list in the user interface
					wxListUint *l = new wxListUint(plotID);
					plotSelList->Append(wxStr(plotData->dataLabel),l);

					ASSERT(plotData->cached == 1 ||
						plotData->cached == 0);
					if(!plotData->cached)
						delete plotData;
					
					break;
				}
				case STREAM_TYPE_DRAW:
				{
					DrawStreamData *drawData;
					drawData=((DrawStreamData *)((*it)[ui]));
					
					//Retrieve vector
					const std::vector<DrawableObj *> *drawObjs;
					drawObjs = &(drawData->drawables);
					//Loop through vector, Adding each object to the scene
					for(unsigned int ui=0;ui<drawObjs->size();ui++)
						targetScene->addDrawable((*drawObjs)[ui]);

					//Although we do not delete the drawable objects
					//themselves, we do delete the container
					ASSERT(!drawData->cached);
					//Zero-size the internal vector to 
					//prevent destructor from deleting pointers
					//we have transferrred ownership of to scene
					drawData->drawables.clear();
					delete drawData;
					break;
				}
				case STREAM_TYPE_RANGE:
					break;
				case STREAM_TYPE_VOXEL:
				{
					//Technically, we are violating const-ness
					VoxelStreamData *vSrc = (VoxelStreamData *)((*it)[ui]);
					//Create a new Field3D
					Voxels<float> *v = new Voxels<float>;

					//Make a copy if cached; otherwise just steal it.
					if(vSrc->cached)
						vSrc->data.clone(*v);
					else
						v->swap(vSrc->data);

					switch(vSrc->representationType)
					{
						case VOXEL_REPRESENT_POINTCLOUD:
						{
							DrawField3D  *d = new DrawField3D;
							d->setField(v);
							d->setColourMapID(0);
							d->setColourMinMax();
							d->setBoxColours(vSrc->r,vSrc->g,vSrc->b,vSrc->a);
							d->setPointSize(vSrc->splatSize);
							d->setAlpha(vSrc->a);
							d->wantsLight=false;

							targetScene->addDrawable(d);
							break;
						}
						case VOXEL_REPRESENT_ISOSURF:
						{
#ifdef HPMC_GPU_ISOSURFACE
							//GPU based isosurface
							DrawIsoSurfaceWithShader *dS = new DrawIsoSurfaceWithShader;
							

							if(dS->canRun())
							{
								dS->init(*v);

								dS->wantsLight=true;
//								dS->setColour(vSrc->r,vSrc->g,
//										vSrc->b,1.0);
								dS->setScalarThresh(vSrc->isoLevel);
								targetScene->addDrawable(dS);
								//Don't process normal isosurf.
								//code.
								break;
							}
					
							delete dS;
#else

							DrawIsoSurface *d = new DrawIsoSurface;

							d->swapVoxels(v);
							d->setColour(vSrc->r,vSrc->g,
									vSrc->b,vSrc->a);
							d->setScalarThresh(vSrc->isoLevel);

							d->wantsLight=true;

							targetScene->addDrawable(d);
#endif
							break;
						}
						default:
							ASSERT(false);
							;
					}

					break;
				}
				default:
					ASSERT(false);

				

				
			}

		}
	
	}


	outData.clear();	
	//---

	//If there is only one spectrum, select it
	if(plotSelList->GetCount() == 1 )
		plotSelList->SetSelection(0);
	else if( plotSelList->GetCount() > 1)
	{
		//Otherwise try to use the last visibility information
		//to set the selection
		targetPlots->bestEffortRestoreVisibility();

#if defined(__WIN32__) || defined(__WIN64__)
		//Bug under windows. SetSelection(wxNOT_FOUND) does not work for multiseletion list boxes
		plotSelList->SetSelection(-1, false);
#else
 		plotSelList->SetSelection(wxNOT_FOUND); //Clear selection
#endif
		for(unsigned int ui=0; ui<plotSelList->GetCount();ui++)
		{
			wxListUint *l;
			unsigned int plotID;

			//Retreive the uniqueID
			l=(wxListUint*)plotSelList->GetClientData(ui);
			plotID = l->value;
			if(targetPlots->isPlotVisible(plotID))
				plotSelList->SetSelection(ui);
		}
	}


	updateRawGrid();
	//Construct an OpenGL display list from the dataset

	//Check how many points we have. Too many can cause the display list to crash
	size_t totalIonCount=0;
	for(unsigned int ui=0;ui<drawIons.size();ui++)
		totalIonCount+=drawIons[ui]->getNumPts();

	//OK, we can only create a display list if
	//there is a valid bounding box
	if(totalIonCount < MAX_NUM_DRAWABLE_POINTS && drawIons.size() >1)
	{
		//Try to use a display list where we can.
		//note that the display list reuqires a valid bounding box,
		//so single point entities, or overlapped points can
		//produce an invalid bounding box
		DrawDispList *displayList;
		displayList = new DrawDispList();

		bool listStarted=false;
		for(unsigned int ui=0;ui<drawIons.size(); ui++)
		{
			BoundCube b;
			drawIons[ui]->getBoundingBox(b);
			if(b.isValid())
			{

				if(!listStarted)
				{
					displayList->startList(false);
					listStarted=true;
				}
				displayList->addDrawable(drawIons[ui]);
				delete drawIons[ui];
			}
			else
				targetScene->addDrawable(drawIons[ui]);
		}

		if(listStarted)	
		{
			displayList->endList();
			targetScene->addDrawable(displayList);
		}
	}
	else
	{
		for(unsigned int ui=0;ui<drawIons.size(); ui++)
			targetScene->addDrawable(drawIons[ui]);
	}

	//clean up uncached data
	for(list<vector<const FilterStreamData *> > ::iterator it=outData.begin(); 
							it!=outData.end(); ++it)
	{
		//iterator points to a vector of pointers, which are the filter stream 
		//data. We need to delete any uncached items before we quit
		for(unsigned int ui=0;ui<it->size(); ui++)
		{
			if(!((*it)[ui])->cached)
				delete ((*it)[ui]);
		}
	}

	targetScene->computeSceneLimits();
	amRefreshing=false;
	return 0;
}


#ifdef DEBUG
void VisController::checkRefreshValidity(const vector< const FilterStreamData *> curData, 
							const Filter *refreshFilter) const
{
	//Filter outputs should
	//	- never be null pointers.
	for(unsigned int ui=0; ui<curData.size(); ui++)
	{
		ASSERT(curData[ui]);
	}

	//Filter outputs should
	//	- never contain duplicate pointers
	for(unsigned int ui=0; ui<curData.size(); ui++)
	{
		for(unsigned int uj=ui+1; uj<curData.size(); uj++)
		{
			ASSERT(curData[ui]!= curData[uj]);
		}
	}


	//Filter outputs should
	//	- Not contain zero sized point streams
	for(size_t ui=0; ui<curData.size(); ui++)
	{
		const FilterStreamData *f;
		f=(curData[ui]);

		switch(f->getStreamType())
		{
		case STREAM_TYPE_IONS:
		{
			const IonStreamData *ionData;
			ionData=((const IonStreamData *)f);

		//	ASSERT(ionData->data.size());
			break;
		}
		default:
			;
		}

	}

	//Filter outputs should
	//	- Always have isCached set to 0 or 1.
	for(unsigned int ui=0; ui<curData.size(); ui++)
	{
		ASSERT(curData[ui]->cached == 1 ||
		curData[ui]->cached == 0);
	}

	//Filter outputs for this filter should only be from those specified in filter emit mask
	for(unsigned int ui=0; ui<curData.size(); ui++)
	{
		if(!curData[ui]->parent)
		{
			cerr << "Warning: orphan filter stream (FilterStreamData::parent == 0). This must be set when creating new filters in the ::refresh function for the filter. " << endl;
			cerr << "Filter :"  << refreshFilter->getUserString() << "Stream Type: " << STREAM_NAMES[getBitNum(curData[ui]->getStreamType())] << endl;
		}
		if(curData[ui]->parent == refreshFilter)
		{
			ASSERT(curData[ui]->getStreamType() & refreshFilter->getRefreshEmitMask());
		}
	}
}
#endif

void VisController::safeDeleteFilterList(
		std::list<std::pair<Filter *, std::vector<const FilterStreamData * > > > &outData, 
						unsigned long long typeMask, bool maskPrevents) const
{
	//Loop through the list of vectors of filterstreamdata, then drop any elements that are deleted
	for(list<pair<Filter *,vector<const FilterStreamData *> > > ::iterator it=outData.begin(); 
							it!=outData.end(); ) 
	{
		//Note the No-op at the loop iterator. this is needed so we can safely .erase()
		for(vector<const FilterStreamData *>::iterator itJ=(it->second).begin(); itJ!=(it->second).end(); )  
		{
			//Don't operate on streams if we have a nonzero mask, and the (mask is active XOR mask mode)
			//NOTE: the XOR flips the action of the mask. if maskprevents is true, then this logical switch
			//prevents the masked item from being deleted. if not, ONLY the maked types are deleted.
			//In any case, a zero mask makes this whole thing not do anything, and everything gets deleted.
			if(typeMask && ( ((bool)((*itJ)->getStreamType() & typeMask)) ^ !maskPrevents)) 
			{
				++itJ; //increment.
				continue;
			}
		
			switch((*itJ)->getStreamType())
			{
				case STREAM_TYPE_IONS:
				{
					//Iterator points to vector. Typecast elements in vector to IonStreamData 
					const IonStreamData *ionData;
					ionData=((const IonStreamData *)(*itJ));
					
					ASSERT(ionData->cached == 1 ||
						ionData->cached == 0);

					if(!ionData->cached)
						delete ionData;
					break;
				}
				case STREAM_TYPE_PLOT:
				{
					
					//Draw plot
					const PlotStreamData *plotData;
					plotData=((PlotStreamData *)(*itJ));

					ASSERT(plotData->cached == 1 ||
						plotData->cached == 0);
					if(!plotData->cached)
						delete plotData;
					
					break;
				}
				case STREAM_TYPE_DRAW:
				{
					DrawStreamData *drawData;
					drawData=((DrawStreamData *)(*itJ));
					
					ASSERT(drawData->cached == 1 ||
						drawData->cached == 0);
					if(drawData->cached)
						delete drawData;
					break;
				}
				case STREAM_TYPE_RANGE:
					//Range data has no allocated pointer
					break;

				
				default:
					ASSERT(false);
			}

			//deleting increments.
			itJ=it->second.erase(itJ);
		}

		//Check to see if this element still has any items in its vector. if not,
		//then discard the element
		if(!(it->second.size()))
			it=outData.erase(it);
		else
			++it;

	}
}

unsigned int VisController::addCam(const std::string &camName)
{
	Camera *c=targetScene->cloneActiveCam();
	c->setUserString(camName);
	//Store the camera
	unsigned int id = targetScene->addCam(c);
	targetScene->setActiveCam(0);
	return id;
}

bool VisController::removeCam(unsigned int uniqueID)
{
	targetScene->removeCam(uniqueID);
	targetScene->setDefaultCam();
	return true;
}

bool VisController::setCam(unsigned int uniqueID)
{
	targetScene->setActiveCam(uniqueID);
	return true;
}

void VisController::updateCamPropertyGrid(wxPropertyGrid *g,unsigned int camID)
{

	g->clearKeys();
	if(targetScene->isDefaultCam())
		return;

	CameraProperties p;
	
	targetScene->getCamProperties(camID,p);

	g->setNumSets(p.data.size());
	//Create the keys for the property grid to do its thing
	for(unsigned int ui=0;ui<p.data.size();ui++)
	{
		for(unsigned int uj=0;uj<p.data[ui].size();uj++)
		{
			g->addKey(p.data[ui][uj].first, ui,p.keys[ui][uj],
				p.types[ui][uj],p.data[ui][uj].second);
		}
	}

	//Let the property grid layout what it needs to
	g->propertyLayout();
}

bool VisController::reparentFilter(unsigned long long toMove, 
					unsigned long long newParent)
{
	//Save current filter state to undo stack
	pushUndoStack();
	

	const Filter *toMoveFilt=getFilterById(toMove);
	const Filter *newParentFilt=getFilterById(newParent);


	ASSERT(toMoveFilt && newParentFilt && 
		!(toMoveFilt==newParentFilt));

	//Look for both newparent and sibling iterators	
	bool found[2] = {false,false};
	tree<Filter *>::iterator moveFilterIt,parentFilterIt;
	for(tree<Filter * >::iterator it=filters.begin(); 
						it!=filters.end(); ++it)
	{
		if(!found[0])
		{
			if(*it == toMoveFilt)
			{
				moveFilterIt=it;
				found[0]=true;
			}
		}
		if(!found[1])
		{
			if(*it == newParentFilt)
			{
				parentFilterIt=it;
				found[1]=true;
			}
		}

		if(found[0] && found[1] )
			break;
	}
	
	ASSERT(found[0] && found[1] );

	//ensure that this is actually a parent-child relationship, and not the other way around!
	for(tree<Filter *>::pre_order_iterator it(moveFilterIt);it!= filters.end(); ++it)
	{
		//Do not traverse siblings
		if(filters.depth(moveFilterIt) >= filters.depth(it) && it!=moveFilterIt )
			break;
		
		if(it == parentFilterIt)
			return false;
	}
	
	//Move the "tomove" filter, and its children to be a child of the
	//newly nominated parent (DoCs "adoption" you might say.)
	if(parentFilterIt != moveFilterIt)
	{
		for(tree<Filter *>::pre_order_iterator it(moveFilterIt);it!= filters.end(); ++it)
		{
			//Do not traverse siblings
			if(filters.depth(moveFilterIt) >= filters.depth(it) && it!=moveFilterIt )
				break;
		
			(*it)->clearCache();
		}
		//Erase the cache of moveFilterIt, and then move it to a new location
		(*moveFilterIt)->clearCache();
		//Create a dummy node after this parent
		tree<Filter*>::iterator node = filters.append_child(parentFilterIt,0);
		//This doesn't actually nuke the original subtree, but rather copies it, replacing the dummy node.
		filters.replace(node,moveFilterIt); 
		//Nuke the original subtree
		filters.erase(moveFilterIt);
	}

	//Topology of filter tree has changed.
	//some filters may need to know about this	
	initFilterTree();

	return moveFilterIt!=parentFilterIt;
}

bool VisController::copyFilter(unsigned long long toCopy, 
					unsigned long long newParent, bool copyToRoot)
{
	//Save current filter state to undo stack
	pushUndoStack();
	

	const Filter *toCopyFilt=getFilterById(toCopy);
	//Copy a filter child to a filter child
	if(!copyToRoot)
	{
		const Filter *newParentFilt=getFilterById(newParent);


		ASSERT(toCopyFilt && newParentFilt && 
			!(toCopyFilt==newParentFilt));

		//Look for both newparent and sibling iterators	
		bool found[2] = {false,false};
		tree<Filter *>::iterator moveFilterIt,parentFilterIt;
		for(tree<Filter * >::iterator it=filters.begin(); 
							it!=filters.end(); ++it)
		{
			if(!found[0])
			{
				if(*it == toCopyFilt)
				{
					moveFilterIt=it;
					found[0]=true;
				}
			}
			if(!found[1])
			{
				if(*it == newParentFilt)
				{
					parentFilterIt=it;
					found[1]=true;
				}
			}

			if(found[0] && found[1] )
				break;
		}
		
		ASSERT(found[0] && found[1] );

		//ensure that this is actually a parent-child relationship
		for(tree<Filter *>::pre_order_iterator it(moveFilterIt);it!= filters.end(); ++it)
		{
			//Do not traverse siblings
			if(filters.depth(moveFilterIt) >= filters.depth(it) && it!=moveFilterIt )
				break;
			
			if(it == parentFilterIt)
				return false;
		}
		
		//Move the "tomove" filter, and its children to be a child of the
		//newly nominated parent (DoCS* "adoption" you might say.) 
		//*DoCs : Department of Child Services (bad taste .au joke)
		if(parentFilterIt != moveFilterIt)
		{
			//Create a temporary tree and copy the contents into here
			tree<Filter *> tmpTree;
			tree<Filter *>::iterator node= tmpTree.insert(tmpTree.begin(),0);
			tmpTree.replace(node,moveFilterIt); //Note this doesn't kill the original
			
			//Replace each of the filters in the temporary_tree with a clone of the original
			for(tree<Filter*>::iterator it=tmpTree.begin();it!=tmpTree.end(); ++it)
				*it= (*it)->cloneUncached();

			//In the original tree, create a new null node
			node = filters.append_child(parentFilterIt,0);
			//Replace the node with the tmpTree's contents
			filters.replace(node,tmpTree.begin()); 

		}
		
		initFilterTree();
		return parentFilterIt != moveFilterIt;
	}
	else
	{
		//copy a selected base of the tree to a new base component

		//Look for both newparent and sibling iterators	
		bool found = false;
		tree<Filter *>::iterator moveFilterIt;
		for(tree<Filter * >::iterator it=filters.begin(); 
							it!=filters.end(); ++it)
		{
			if(*it == toCopyFilt)
			{
				moveFilterIt=it;
				found=true;
				break;
			}
		}

		if(!found)
			return false;

		//Create a temporary tree and copy the contents into here
		tree<Filter *> tmpTree;
		tree<Filter *>::iterator node= tmpTree.insert(tmpTree.begin(),0);
		tmpTree.replace(node,moveFilterIt); //Note this doesn't kill the original
		
		//Replace each of the filters in the temporary_tree with a clone of the original
		for(tree<Filter*>::iterator it=tmpTree.begin();it!=tmpTree.end(); ++it)
			*it= (*it)->cloneUncached();

		//In the original tree, create a new null node
		node = filters.insert_after(filters.begin(),0);
		//Replace the node with the tmpTree's contents
		filters.replace(node,tmpTree.begin()); 
		initFilterTree();
		return true;
	}

	ASSERT(false);
}


bool VisController::setFilterString(unsigned long long id,const std::string &s)
{
	//Save current filter state to undo stack
	pushUndoStack();
	
	Filter *p=(Filter *)getFilterById(id);

	if(s != p->getUserString())
	{
		p->setUserString(s);
		return true;
	}

	return false;
}

void VisController::invalidateCache(const Filter *filtPtr)
{

	if(!filtPtr)
	{
		//Invalidate everything
		for(tree<Filter * >::iterator it=filters.begin(); 
						it!=filters.end(); ++it)
			(*it)->clearCache();
	}
	else
	{
		//Find the filter in the tree
		tree<Filter *>::iterator filterIt;	
		for(tree<Filter * >::iterator it=filters.begin(); 
							it!=filters.end(); ++it)
		{
			if(*it == filtPtr)
			{
				filterIt=it;
				break;
			}
		}

		for(tree<Filter *>::pre_order_iterator it(filterIt);it!= filters.end(); ++it)
		{
			//Do not traverse siblings
			if(filters.depth(filterIt) >= filters.depth(it) && it!=filterIt )
				break;
		
			(*it)->clearCache();
		}
	}

	


}

unsigned int VisController::numCams() const 
{
	return targetScene->getNumCams();
}
		
void VisController::ensureSceneVisible(unsigned int direction)
{
	targetScene->ensureVisible(direction);
}

bool VisController::saveState(const char *cpFilename, std::map<string,string> &fileMapping,
		bool writePackage) const
{
	//Open file for output
	std::ofstream f(cpFilename);

	if(!f)
		return false;

	//Write state open tag 
	f<< "<threeDepictstate>" << endl;
	f<<tabs(1)<< "<writer version=\"" << PROGRAM_VERSION << "\"/>" << endl;
	//write general settings
	//---------------
	float backR,backG,backB;

	targetScene->getBackgroundColour(backR,backG,backB);
	f << tabs(1) << "<backcolour r=\"" << backR << "\" g=\"" << backG  << "\" b=\"" << backB << "\"/>" <<  endl;
	
	f << tabs(1) << "<showaxis value=\"";
       	if(targetScene->getWorldAxisVisible())
	       f<< "1";
       	else
		f << "0";
	f<< "\"/>"  << endl;


	if(writePackage || useRelativePathsForSave)
	{
		//Are we saving the sate as a package, if so
		//make sure we let other 3depict loaders know
		//that we want to use relative paths
		f << tabs(1) << "<userelativepaths/>"<< endl;
	}

	//---------------


	//Write filter tree
	//---------------
	f << tabs(1) << "<filtertree>" << endl;
	//Depth-first search, enumerate all filters in depth-first fashion
	unsigned int depthLast=0;
	unsigned int child=0;
	for(tree<Filter * >::pre_order_iterator filtIt=filters.begin();
					filtIt!=filters.end(); filtIt++)
	{
		unsigned int depth;
		depth = filters.depth(filtIt);
		if(depth >depthLast)
		{
			while(depthLast++ < depth)
			{
				f << tabs(depthLast+1);
				f  << "<children>" << endl; 
				child++;
			}
		}
		else if (depth < depthLast)
		{
			while(depthLast-- > depth)
			{
				f << tabs(depthLast+2);
				f  << "</children>" << endl; 
				child--;
			}
		}

		//If we are writing a package, override the filter storage values
		if(writePackage || useRelativePathsForSave)
		{
			vector<string> valueOverrides;
			(*filtIt)->getStateOverrides(valueOverrides);

			//The overrides, at the moment, only are files. 
			//So lets find them & move them
			for(unsigned int ui=0;ui<valueOverrides.size();ui++)
			{
				string newFilename;
				newFilename=string("./") + onlyFilename(valueOverrides[ui]);

				map<string,string>::iterator it;
				it =fileMapping.find(valueOverrides[ui]);
				
				if(it == fileMapping.end()) 
				{
					//map does not exist, so make it!
					fileMapping[newFilename]=valueOverrides[ui];
				}
				else if (it->second !=valueOverrides[ui])
				{
					//Keep adding a prefix until we find a valid new filename
					while(fileMapping.find(newFilename) != fileMapping.end())
						newFilename="remap"+newFilename;
					//map does not exist, so make it!
					fileMapping[newFilename]=valueOverrides[ui];
				}
				valueOverrides[ui] = newFilename;
			}			
		
			if(!(*filtIt)->writePackageState(f,STATE_FORMAT_XML,valueOverrides,depth+2))
				return false;
		}
		else
		{
			if(!(*filtIt)->writeState(f,STATE_FORMAT_XML,depth+2))
				return false;
		}
		depthLast=depth;
	}

	//Close out filter tree.
	while(child--)
	{
		f << tabs(child+2) << "</children>" << endl;
	}
	f << tabs(1) << "</filtertree>" << endl;
	//---------------

	vector<Camera *> camVec;

	unsigned int active=targetScene->duplicateCameras(camVec);

	//First camera is hidden "working" camera. do not record 
	if(camVec.size() > 1)
	{
		f <<tabs(1) <<  "<cameras>" << endl;
		//subtract 1 to eliminate "hidden" camera offset 
		f << tabs(2) << "<active value=\"" << active-1 << "\"/>" << endl;
		
		for(unsigned int ui=1;ui<camVec.size();ui++)
		{
			//ask each camera to write its own state, tab indent 2
			camVec[ui]->writeState(f,STATE_FORMAT_XML,2);
			delete camVec[ui];
		}
		f <<tabs(1) <<  "</cameras>" << endl;
	}
	
	if(camVec.size())
		delete camVec[0];
	camVec.clear();

	if(stashedFilters.size())
	{
		f << tabs(1) << "<stashedfilters>" << endl;

		for(unsigned int ui=0;ui<stashedFilters.size();ui++)
		{
			f << tabs(2) << "<stash name=\"" <<  stashedFilters[ui].first <<"\">" << endl;

			//Depth-first search, enumerate all filters in depth-first fashion
			unsigned int depthLast;
			depthLast=0;
			unsigned int child;
			child=0;

			const unsigned int tabOffset=2;
			for(tree<Filter * >::pre_order_iterator filtIt=stashedFilters[ui].second.begin();
							filtIt!=stashedFilters[ui].second.end(); filtIt++)
			{
				unsigned int depth;
				depth = filters.depth(filtIt);
				if(depth >depthLast)
				{
					while(depthLast++ < depth)
					{
						f << tabs(depthLast+1+tabOffset);
						f  << "<children>" << endl; 
						child++;
					}
				}
				else if (depth < depthLast)
				{
					while(depthLast-- > depth)
					{
						f << tabs(depthLast+2+tabOffset);
						f  << "</children>" << endl; 
						child--;
					}
				}

				//If we are writing a package, override the filter storage values
				if(writePackage || useRelativePathsForSave)
				{
					vector<string> valueOverrides;
					(*filtIt)->getStateOverrides(valueOverrides);

					//The overrides, at the moment, only are files. 
					//So lets find them & move them
					for(unsigned int ui=0;ui<valueOverrides.size();ui++)
					{
						string newFilename;
						newFilename=string("./") + onlyFilename(valueOverrides[ui]);

						map<string,string>::iterator it;
						it =fileMapping.find(valueOverrides[ui]);
						
						if(it == fileMapping.end()) 
						{
							//map does not exist, so make it!
							fileMapping[newFilename]=valueOverrides[ui];
						}
						else if (it->second !=valueOverrides[ui])
						{
							//Keep adding a prefix until we find a valid new filename
							while(fileMapping.find(newFilename) != fileMapping.end())
								newFilename="remap"+newFilename;
							//map does not exist, so make it!
							fileMapping[newFilename]=valueOverrides[ui];
						}
						valueOverrides[ui] = newFilename;
					}			
						
					if(!(*filtIt)->writePackageState(f,STATE_FORMAT_XML,valueOverrides,depth+2))
						return false;
				}
				else
				{
					if(!(*filtIt)->writeState(f,STATE_FORMAT_XML,depth+2))
						return false;
				}
				
				depthLast=depth;
			}
			
			//Close out stash's filter tree
			while(child--)
				f << tabs(child+1+tabOffset) << "</children>" << endl;

			//Finish stash
			f << tabs(2) << "</stash>" << endl;

		}




		f << tabs(1) << "</stashedfilters>" << endl;
	}

	//Save any effects
	vector<const Effect *> effectVec;
	targetScene->getEffects(effectVec);

	if(effectVec.size())
	{
		f <<tabs(1) <<  "<effects>" << endl;
		for(unsigned int ui=0;ui<effectVec.size();ui++)
			effectVec[ui]->writeState(f,STATE_FORMAT_XML,1);
		f <<tabs(1) <<  "</effects>" << endl;

	}



	//Close XMl tag.	
	f<< "</threeDepictstate>" << endl;



	return true;
}

bool VisController::loadState(const char *cpFilename, std::ostream &errStream, bool merge) 
{
	//Load the state from an XML file
	//FIXME: This could leave the viscontrol in a half
	//  loaded state if the file was non-parseable.
	//  Create disposable data structure to hold intermed. state
	
	//here we use libxml2's loading routines
	//http://xmlsoft.org/
	//Tutorial: http://xmlsoft.org/tutorial/xmltutorial.pdf
	xmlDocPtr doc;
	xmlParserCtxtPtr context;

	context =xmlNewParserCtxt();


	if(!context)
	{
		errStream << TRANS("Failed to allocate parser") << std::endl;
		return false;
	}

	//Open the XML file
	doc = xmlCtxtReadFile(context, cpFilename, NULL,0);

	if(!doc)
		return false;
	
	//release the context
	xmlFreeParserCtxt(context);
	

	//By default, lets not use relative paths
	if(!merge)
		useRelativePathsForSave=false;

	//Lets do some parsing goodness
	//ahh parsing - verbose and boring
	tree<Filter *> newFilterTree;
	vector<Camera *> newCameraVec;
	vector<Effect *> newEffectVec;
	vector<pair<string,tree<Filter * > > > newStashes;

	std::string stateDir=onlyDir(cpFilename);
	unsigned int activeCam;
	try
	{
		std::stack<xmlNodePtr>  nodeStack;
		//retrieve root node	
		xmlNodePtr nodePtr = xmlDocGetRootElement(doc);

		//Umm where is our root node guys?
		if(!nodePtr)
		{
			errStream << TRANS("Unable to retrieve root node in input state file... Is this really a non-empty XML file?") <<  endl;
			throw 1;
		}
		
		//This *should* be an threeDepict state file
		if(xmlStrcmp(nodePtr->name, (const xmlChar *)"threeDepictstate"))
		{
			errStream << TRANS("Base state node missing. Is this really a state XML file??") << endl;
			throw 1;
		}
		//push root tag	
		nodeStack.push(nodePtr);
		
		//Now in threeDepictstate tag
		nodePtr = nodePtr->xmlChildrenNode;
		xmlChar *xmlString;
		//check for version tag & number
		if(!XMLHelpFwdToElem(nodePtr,"writer"))
		{
			xmlString=xmlGetProp(nodePtr, (const xmlChar *)"version"); 

			vector<string> vecStrs;
			vecStrs.push_back((char *)xmlString);
			//Check to see if only contains 0-9 and period characters (valid version number)
			if(vecStrs[0].find_first_not_of("0123456789.")== std::string::npos)
			{
				vecStrs.push_back(PROGRAM_VERSION);
				if(getMaxVerStr(vecStrs)!=PROGRAM_VERSION)
				{
					errStream << TRANS("State was created by a newer version of this program.. ")
						<< TRANS("file reading will continue, but may fail.") << endl ;
				}
			}
			else
			{
				errStream<< TRANS("Warning, unparseable version number in state file. File reading will continue, but may fail") << endl;
			}
			xmlFree(xmlString);
		}
		else
		{
			errStream<< TRANS("Unable to find the \"writer\" node") << endl;
			throw 1;
		}
	

		//Get the background colour
		//====
		float rTmp,gTmp,bTmp;
		if(XMLHelpFwdToElem(nodePtr,"backcolour"))
		{
			errStream<< TRANS("Unable to find the \"backcolour\" node.") << endl;
			throw 1;
		}

		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"r");
		if(!xmlString)
		{
			errStream<< TRANS("\"backcolour\" node missing \"r\" value.") << endl;
			throw 1;
		}
		if(stream_cast(rTmp,(char *)xmlString))
		{
			errStream<< TRANS("Unable to interpret \"backColour\" node's \"r\" value.") << endl;
			throw 1;
		}

		xmlFree(xmlString);
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"g");
		if(!xmlString)
		{
			errStream<< TRANS("\"backcolour\" node missing \"g\" value.") << endl;
			throw 1;
		}

		if(stream_cast(gTmp,(char *)xmlString))
		{
			errStream<< TRANS("Unable to interpret \"backColour\" node's \"g\" value.") << endl;
			throw 1;
		}

		xmlFree(xmlString);
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"b");
		if(!xmlString)
		{
			errStream<< TRANS("\"backcolour\" node missing \"b\" value.") << endl;
			throw 1;
		}

		if(stream_cast(bTmp,(char *)xmlString))
		{
			errStream<< TRANS("Unable to interpret \"backColour\" node's \"b\" value.") << endl;
			throw 1;
		}

		if(rTmp > 1.0 || gTmp>1.0 || bTmp > 1.0 || 
			rTmp < 0.0 || gTmp < 0.0 || bTmp < 0.0)
		{
			errStream<< TRANS("\"backcolour\"s rgb values must be in range [0,1]") << endl;
			throw 1;
		}
		targetScene->setBackgroundColour(rTmp,gTmp,bTmp);
		xmlFree(xmlString);

		nodeStack.push(nodePtr);


		if(!XMLHelpFwdToElem(nodePtr,"userelativepaths"))
			useRelativePathsForSave|=true;
		
		nodePtr=nodeStack.top();

		//====
		
		//Get the axis visibilty
		unsigned int axisIsVis;
		if(!XMLGetNextElemAttrib(nodePtr,axisIsVis,"showaxis","value"))
		{
			errStream << TRANS("Unable to find or interpret \"showaxis\" node") << endl;
			throw 1;
		}
		targetScene->setWorldAxisVisible(axisIsVis==1);

		//find filtertree data
		if(XMLHelpFwdToElem(nodePtr,"filtertree"))
		{
			errStream << TRANS("Unable to locate \"filtertree\" node.") << endl;
			throw 1;
		}

		//Load the filter tree
		if(loadFilterTree(nodePtr,newFilterTree,errStream,stateDir))
			throw 1;

		//Read camera states, if present
		nodeStack.push(nodePtr);
		if(!XMLHelpFwdToElem(nodePtr,"cameras"))
		{
			//Move to camera active tag 
			nodePtr=nodePtr->xmlChildrenNode;
			if(XMLHelpFwdToElem(nodePtr,"active"))
			{
				errStream << TRANS("Cameras section missing \"active\" node.") << endl;
				throw 1;
			}

			//read ID of active cam
			xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
			if(!xmlString)
			{
				errStream<< TRANS("Unable to find property \"value\"  for \"cameras->active\" node.") << endl;
				throw 1;
			}

			if(stream_cast(activeCam,xmlString))
			{
				errStream<< TRANS("Unable to interpret property \"value\"  for \"cameras->active\" node.") << endl;
				throw 1;
			}

		
			//Spin through the list of each camera	
			while(!XMLHelpNextType(nodePtr,XML_ELEMENT_NODE))
			{
				std::string tmpStr;
				tmpStr =(const char *)nodePtr->name;
				Camera *thisCam;
				thisCam=0;
				
				//work out the camera type
				if(tmpStr == "persplookat")
				{
					thisCam = new CameraLookAt;
					if(!thisCam->readState(nodePtr->xmlChildrenNode))
						return false;
				}
				else
				{
					errStream << TRANS("Unable to interpret the camera type") << endl;
					throw 1;
				}

				ASSERT(thisCam);
				newCameraVec.push_back(thisCam);	
			}

			if(newCameraVec.size() < activeCam)
				activeCam=0;

		}
		
		nodePtr=nodeStack.top();
		nodeStack.pop();
		
		nodeStack.push(nodePtr);
		//Read stashes if present
		if(!XMLHelpFwdToElem(nodePtr,"stashedfilters"))
		{
			nodeStack.push(nodePtr);

			//Move to stashes 
			nodePtr=nodePtr->xmlChildrenNode;

			while(!XMLHelpFwdToElem(nodePtr,"stash"))
			{
				string stashName;
				tree<Filter *> newStashTree;
				newStashTree.clear();

				//read name of stash
				xmlString=xmlGetProp(nodePtr,(const xmlChar *)"name");
				if(!xmlString)
				{
					errStream << TRANS("Unable to locate stash name for stash ") << newStashTree.size()+1 << endl;
					throw 1;
				}
				stashName=(char *)xmlString;

				if(!stashName.size())
				{
					errStream << TRANS("Empty stash name for stash ") << newStashTree.size()+1 << endl;
					throw 1;
				}

				if(loadFilterTree(nodePtr,newStashTree,errStream,stateDir))
				{
					errStream << TRANS("For stash ") << newStashTree.size()+1 << endl;
					throw 1;
				}
			
				newStashes.push_back(make_pair(stashName,newStashTree));
			}

			nodePtr=nodeStack.top();
			nodeStack.pop();
		}
		nodePtr=nodeStack.top();
		nodeStack.pop();
	
		//Read effects, if present
		nodeStack.push(nodePtr);

		//Read effects if present
		if(!XMLHelpFwdToElem(nodePtr,"effects"))
		{
			std::string tmpStr;
			nodePtr=nodePtr->xmlChildrenNode;
			while(!XMLHelpNextType(nodePtr,XML_ELEMENT_NODE))
			{
				tmpStr =(const char *)nodePtr->name;

				Effect *e;
				e = makeEffect(tmpStr);
				if(!e)
				{
					errStream << TRANS("Unrecognised effect :") << tmpStr << endl;
					throw 1;
				}

				//Check the effects are unique
				for(unsigned int ui=0;ui<newEffectVec.size();ui++)
				{
					if(newEffectVec[ui]->getType()== e->getType())
					{
						delete e;
						errStream << TRANS("Duplicate effect found") << tmpStr << TRANS(" cannot use.") << endl;
						throw 1;
					}

				}

				nodeStack.push(nodePtr);
				//Parse the effect
				if(!e->readState(nodePtr))
				{
					errStream << TRANS("Error reading effect : ") << e->getName() << std::endl;
				
					throw 1;
				}
				nodePtr=nodeStack.top();
				nodeStack.pop();


				newEffectVec.push_back(e);				
			}
		}
		nodePtr=nodeStack.top();
		nodeStack.pop();



		nodeStack.push(nodePtr);
	}
	catch (int)
	{
		//Code threw an error, just say "bad parse" and be done with it
		xmlFreeDoc(doc);
		return false;
	}
	xmlFreeDoc(doc);	


	
	//Set the filter tree to the new one
	//the new mapping will be created during the next tree ctrl update
	if(!merge)
	{
		clear();
		for(tree<Filter * >::pre_order_iterator filtIt=filters.begin();
						filtIt!=filters.end(); filtIt++)
			delete *filtIt;

	
		for(unsigned int ui=0;ui<stashedFilters.size();ui++)
		{
			for(tree<Filter * >::pre_order_iterator filtIt=stashedFilters[ui].second.begin();
							filtIt!=stashedFilters[ui].second.end(); filtIt++)
				delete *filtIt;
		}
	}

	//Check that stashes are uniquely named
	for(unsigned int ui=0;ui<newStashes.size();ui++)
	{
		for(unsigned int uj=0;uj<newStashes.size();uj++)
		{
			if(ui == uj)
				continue;

			if(newStashes[ui].first == newStashes[uj].first)
			{
				//Shit. Stashes not uniquely named,
				//this state file is invalid

				//Clear the stashes & filter and abort
				for(tree<Filter * >::pre_order_iterator filtIt=newFilterTree.begin();
								filtIt!=newFilterTree.end(); filtIt++)
					delete *filtIt;

				
				for(unsigned int ui=0;ui<newStashes.size();ui++)
				{
					for(tree<Filter * >::pre_order_iterator filtIt=stashedFilters[ui].second.begin();
									filtIt!=stashedFilters[ui].second.end(); filtIt++)
						delete *filtIt;


				}


				return false;
			}

		}
	}




	if(!merge)
	{
		std::swap(filters,newFilterTree);

		std::swap(stashedFilters,newStashes);

	}
	else
	{
		//If we are merging, then there is a chance
		//of a name-clash. We avoid this by trying to append -merge continuously
		for(unsigned int ui=0;ui<newStashes.size();ui++)
		{
			//protect against overload (very unlikely)
			unsigned int maxCount;
			maxCount=100;
			while(hasFirstInPairVec(stashedFilters,newStashes[ui]) && --maxCount)
				newStashes[ui].first+="-merge";

			if(maxCount)
				stashedFilters.push_back(newStashes[ui]);
			else
				errStream << TRANS(" Unable to merge stashes correctly. This is improbable, so please report this.") << endl;
		}

		if(filters.size())
		{
			filters.insert_subtree_after(filters.begin(),newFilterTree.begin());
		}
		else
			std::swap(filters,newFilterTree);
	}

	//Re-generate the ID mapping for the stashes
	stashUniqueIDs.clear();
	for(unsigned int ui=0;ui<stashedFilters.size();ui++)
		stashUniqueIDs.genId(ui);

	//Wipe the existing cameras, and then put the new cameras in place
	if(!merge)
		targetScene->clearCams();
	
	//Set a default camera as needed. We don't need to track its unique ID, as this is
	//"invisible" to the UI
	if(!targetScene->getNumCams())
	{
		Camera *c=new CameraLookAt();
		unsigned int id;
		id=targetScene->addCam(c);
		targetScene->setActiveCam(id);
	}

	for(unsigned int ui=0;ui<newCameraVec.size();ui++)
	{
		unsigned int id;


		unsigned int maxCount;
		maxCount=100;
		while(targetScene->camNameExists(newCameraVec[ui]->getUserString()) && --maxCount)
		{
			newCameraVec[ui]->setUserString(newCameraVec[ui]->getUserString()+"-merge");
		}

		if(maxCount)
		{
			id=targetScene->addCam(newCameraVec[ui]);
			//Use the unique identifier to set the active cam,
			//if this is the active camera.
			if(ui == activeCam)
				targetScene->setActiveCam(id);
		}
	}

	if(newEffectVec.size())
	{
		targetScene->clearEffects();
		for(unsigned int ui=0;ui<newEffectVec.size();ui++)
		{
			targetScene->addEffect(newEffectVec[ui]);
		}
	}

	initFilterTree();
	//Perform sanitisation on results
	return true;
}


unsigned int VisController::loadFilterTree(const xmlNodePtr &treeParent, tree<Filter *> &newTree, std::ostream &errStream,const std::string &stateFileDir) const

{
	newTree.clear();
	//List of filters to delete if parsing goes pear shaped
	list<Filter *> cleanupList;

	//Parse the filter tree in the XML file.
	//generating a filter tree
	bool inTree=true;
	tree<Filter *>::iterator lastFilt=newTree.begin();
	tree<Filter *>::iterator lastTop=newTree.begin();
	stack<tree<Filter *>::iterator> treeNodeStack;

	xmlNodePtr nodePtr = treeParent->xmlChildrenNode;


	//push root tag	
	std::stack<xmlNodePtr>  nodeStack;
	nodeStack.push(nodePtr);

	bool needCleanup=false;
	while (inTree)
	{
		//Jump to the next XML node at this depth
		if (XMLHelpNextType(nodePtr,XML_ELEMENT_NODE))
		{
			//If there is not one, pop the tree stack
			if (treeNodeStack.size())
			{
				//Pop the node stack for the XML and filter trees.
				nodePtr=nodeStack.top();
				nodeStack.pop();
				lastFilt=treeNodeStack.top();
				treeNodeStack.pop();
			}
			else
			{
				//Did we run out of stack?
				//then we have finished the tree.
				inTree=false;
			}
			continue;
		}

		Filter *newFilt;
		bool nodeUnderstood;
		newFilt=0;
		nodeUnderstood=true; //assume by default we understand, and set false if not

		//If we encounter a "children" node. then we need to look at the children of this filter
		if (!xmlStrcmp(nodePtr->name,(const xmlChar*)"children"))
		{
			//Can't have children without parent
			if (!newTree.size())
			{
				needCleanup=true;
				break;
			}

			//Child node should have its own child
			if (!nodePtr->xmlChildrenNode)
			{
				needCleanup=true;
				break;
			}

			nodeStack.push(nodePtr);
			treeNodeStack.push(lastFilt);

			nodePtr=nodePtr->xmlChildrenNode;
			continue;
		}
		else
		{
			//Well, its not  a "children" node, so it could
			//be a filter... Lets find out
			std::string tmpStr;
			tmpStr=(char *)nodePtr->name;

			newFilt=makeFilter(tmpStr);
			if(newFilt)
			{
				cleanupList.push_back(newFilt);
				if (!newFilt->readState(nodePtr->xmlChildrenNode,stateFileDir))
				{
					needCleanup=true;
					break;
				}
			}
			else
			{
				errStream << TRANS("WARNING: Skipping node ") << (const char *)nodePtr->name << TRANS(" as it was not recognised") << endl;
				nodeUnderstood=false;
			}
		}


		//Skip this item
		if (nodeUnderstood)
		{
			ASSERT(newFilt);

			//Add the new item the tree
			if (!newTree.size())
				lastFilt=newTree.insert(newTree.begin(),newFilt);
			else
			{
				if (treeNodeStack.size())
					lastFilt=newTree.append_child(treeNodeStack.top(),newFilt);
				else
				{
					lastTop=newTree.insert(lastTop,newFilt);
					lastFilt=lastTop;
				}


			}

		}
	}


	//All good?
	if(!needCleanup)
		return 0;

	//OK, we hit an error, we need to delete any pointers on the
	//cleanup list
	if(nodePtr)
		errStream << TRANS("Error processing node: ") << (const char *)nodePtr->name << endl;

	for(list<Filter*>::iterator it=cleanupList.begin();
			it!=cleanupList.end(); ++it)
		delete *it;

	//No good..
	return 1;

}

void VisController::makeFiltersSafe()
{
	stripHazardousFilters(filters);
	
	for(unsigned int ui=0;ui<stashedFilters.size();ui++)
		stripHazardousFilters(stashedFilters[ui].second);
}

void VisController::stripHazardousFilters(tree<Filter *> &thisTree)
{
	for(tree<Filter * >::pre_order_iterator it=thisTree.begin();
					it!=thisTree.end(); ++it)
	{
		if ((*it)->canBeHazardous())
		{
			//delete filters from this branch
			for(tree<Filter *>::pre_order_iterator itj(it); itj!=thisTree.end(); itj++)
				delete *itj;
	

			//nuke this branch
			it=thisTree.erase(it);
			it--;
		}
	}

}

bool VisController::hasHazardous(const tree<Filter *> &thisTree) const
{
	for(tree<Filter * >::pre_order_iterator it=thisTree.begin();
					it!=thisTree.end(); ++it)
	{
		if ((*it)->canBeHazardous())
			return true;
	}

	return false;
}

bool VisController::hasHazardousContents() const
{
	//Check the filter system and the 
	//stashes for potentially dangerous content
	if(hasHazardous(filters))
		return true;
	for(unsigned int ui=0;ui<stashedFilters.size();ui++)
	{
		if(hasHazardous(stashedFilters[ui].second))
			return true;
	}

	return false;
}

void VisController::clear()
{
	filterTreeMapping.clear();
	nextID=0;

	//clean filter tree
	for(tree<Filter * >::iterator it=filters.begin(); 
						it!=filters.end(); ++it)
		delete *it;

	filters.clear();
	undoFilterStack.clear();
}

void VisController::getCamData(std::vector<std::pair<unsigned int, std::string> > &camData) const
{
	targetScene->getCameraIDs(camData);
}

unsigned int VisController::getActiveCamId() const
{ 
	return targetScene->getActiveCamId();
}

unsigned int VisController::exportIonStreams(const std::vector<const FilterStreamData * > &selectedStreams,
		const std::string &outFile, unsigned int format) const
{

	//test file open, and truncate file to zero bytes
	ofstream f(outFile.c_str(),ios::trunc);
	
	if(!f)
		return 1;

	f.close();

	for(unsigned int ui=0; ui<selectedStreams.size(); ui++)
	{
		switch(selectedStreams[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS:
			{
				const IonStreamData *ionData;
				ionData=((const IonStreamData *)(selectedStreams[ui]));
				switch(format)
				{
					case IONFORMAT_POS:
					{
						//Append this ion stream to the posfile
						appendPos(ionData->data,outFile.c_str());

						break;
					}
					default:
						ASSERT(false);
						break;
				}
			}
		}
	}

	return 0;
}

void VisController::getFilterTypes(std::vector<const Filter *> &filtersOut, unsigned int type)
{
	for(tree<Filter * >::iterator it=filters.begin(); 
						it!=filters.end(); ++it)
	{
		if((*it)->getType() == type)
			filtersOut.push_back(*it);
	}
}

void VisController::purgeFilterCache()
{
	for(tree<Filter *>::iterator it=filters.begin();it!=filters.end();++it)
		(*it)->clearCache();
}

void VisController::addStashedToFilters(const Filter *parent, unsigned int stashId)
{
	//Save current filter state to undo stack
	pushUndoStack();
	
	//Splice a copy of the stash into the subtree of the selected parent filter
	tree<Filter *>::iterator it= std::find(filters.begin(),filters.end(),parent);

	ASSERT(it != filters.end())

	
	tree<Filter*>::iterator node = filters.append_child(it,0);
	
	tree<Filter *> stashTree;

	getStashTree(stashId,stashTree);

	//Duplicate the contents of this filter
	
	//Replace each of the filters in the temporary_tree with a clone of the original
	for(tree<Filter*>::iterator copyIt=stashTree.begin();copyIt!=stashTree.end(); ++copyIt)
		*copyIt= (*copyIt)->cloneUncached();

	//insert the tree as a sibling of the node 
	filters.insert_subtree_after(node,stashTree.begin()); 

	//nuke the appended node, which was just acting as a dummy
	filters.erase(node);

	initFilterTree();
}

void VisController::deleteStash(unsigned int stashId)
{
	unsigned int stashPos=stashUniqueIDs.getPos(stashId);

	//swap out the one we wish to kill with the last element
	if(stashPos != stashedFilters.size() -1)
		swap(stashedFilters[stashedFilters.size()-1],stashedFilters[0]);

	//nuke the tail
	stashedFilters.pop_back();
}

unsigned int VisController::stashFilters(unsigned int filterId, const char *stashName)
{
 
	tree<Filter *> t;
	Filter *target = (Filter*)getFilterById(filterId);

	tree<Filter *>::iterator targetIt;
	for(tree<Filter *>::iterator it=filters.begin(); it!= filters.end(); ++it)
	{
		if(*it == target)
		{
			targetIt=it;
			break;
		}
	}


	tree<Filter *> subTree;

	tree<Filter *>::iterator node= subTree.insert(subTree.begin(),0);
	subTree.replace(node,targetIt); //Note this doesn't kill the original

	//Replace each of the filters in the temporary_tree with a clone of the original
	for(tree<Filter*>::iterator it=subTree.begin();it!=subTree.end(); ++it)
		*it= (*it)->cloneUncached();
	
	stashedFilters.push_back(std::make_pair(string(stashName),subTree));
	return stashUniqueIDs.genId(stashedFilters.size()-1); 
}

void VisController::getStashTree(unsigned int stashId, tree<Filter *> &t) const
{
	unsigned int pos = stashUniqueIDs.getPos(stashId);
	ASSERT(pos < stashedFilters.size());
	t = stashedFilters[pos].second;
}

void VisController::getStashList(std::vector<std::pair<std::string,unsigned int > > &stashList) const
{
	ASSERT(stashList.size() == 0); // should be empty	
	//Duplicate the stash list, but replace the filter tree ID
	//with the ID value that corresponds to that position
	stashList.reserve(stashedFilters.size());	
	for(unsigned int ui=0;ui<stashedFilters.size(); ui++)
	{
		stashList.push_back(std::make_pair(stashedFilters[ui].first,
					stashUniqueIDs.getId(ui)));
	}
}

void VisController::updateRawGrid() const
{
	vector<vector<vector<float> > > plotData;
	vector<std::vector<std::wstring> > labels;
	//grab the data for the currently visible plots
	targetPlots->getRawData(plotData,labels);


	unsigned int curCol=0;
	unsigned int startCol;

	//Clear the grid
	if(targetRawGrid->GetNumberCols())
		targetRawGrid->DeleteCols(0,targetRawGrid->GetNumberCols());
	if(targetRawGrid->GetNumberRows())
		targetRawGrid->DeleteRows(0,targetRawGrid->GetNumberRows());
	
	for(unsigned int ui=0;ui<plotData.size(); ui++)
	{
		//Create new columns
		targetRawGrid->AppendCols(plotData[ui].size());
		ASSERT(labels[ui].size() == plotData[ui].size());
	

		startCol=curCol;
		for(unsigned int uj=0;uj<labels[ui].size();uj++)
		{
			std::string s;
			s=stlWStrToStlStr(labels[ui][uj]);
			targetRawGrid->SetColLabelValue(curCol,wxStr(s));
			curCol++;
		}

		//set the data
		for(unsigned int uj=0;uj<plotData[ui].size();uj++)
		{
			//Check to see if we need to add rows to make our data fit
			if(plotData[ui][uj].size() > targetRawGrid->GetNumberRows())
				targetRawGrid->AppendRows(plotData[ui][uj].size()-targetRawGrid->GetNumberRows());

			for(unsigned int uk=0;uk<plotData[ui][uj].size();uk++)
			{
				std::string tmpStr;
				stream_cast(tmpStr,plotData[ui][uj][uk]);
				targetRawGrid->SetCellValue(uk,startCol,wxStr(tmpStr));
			}
			startCol++;
		}

	}
}

void VisController::setCachePercent(unsigned int newCache)
{
	ASSERT(newCache <= 100);
	if(!newCache)
		cacheStrategy=CACHE_NEVER;
	else
	{
		cacheStrategy=CACHE_DEPTH_FIRST;
		maxCachePercent=newCache;
	}
}


void VisController::pushUndoStack()
{
	tree<Filter *> newTree;

	newTree = filters;
	
	//Replace each of the filters in the temporary_tree with a clone of the original
	for(tree<Filter*>::iterator it=newTree.begin();it!=newTree.end(); ++it)
		*it= (*it)->cloneUncached();

	if(undoFilterStack.size() > MAX_UNDO_SIZE)
	{
		//clean up
		for(tree<Filter * >::iterator it=undoFilterStack.front().begin(); 
							it!=undoFilterStack.front().end(); ++it)
			delete *it;

		undoFilterStack.pop_front();
	}

	undoFilterStack.push_back(newTree);

	clearRedoStack();
}

void VisController::popUndoStack()
{
	ASSERT(undoFilterStack.size());
	
	//Save the current filters to the redo stack.
	redoFilterStack.push_back(filters);

	if(redoFilterStack.size() > MAX_UNDO_SIZE)
	{
		//clean up
		for(tree<Filter * >::iterator it=redoFilterStack.front().begin(); 
							it!=redoFilterStack.front().end(); ++it)
			delete *it;

		redoFilterStack.pop_front();
	}


	//Clear the current filter caches
	for(tree<Filter*>::iterator it=filters.begin();it!=filters.end(); ++it)
		(*it)->clearCache();
	
	//Swap the current filter cache out with the undo stack result
	std::swap(filters,undoFilterStack.back());

	//Pop the undo stack
	undoFilterStack.pop_back();
	
	//Filter topology has changed. Update
	initFilterTree();
}

void VisController::popRedoStack()
{

	//Push the current state back onto the undo stack
	tree<Filter *> newTree;

	//Copy the iterators onto the new tree, which we will clone later
	//so we are modifying a clone, not the original
	newTree = filters;
	
	//Replace each of the filters in the temporary_tree with a clone of the original
	for(tree<Filter*>::iterator it=newTree.begin();it!=newTree.end(); ++it)
		*it= (*it)->cloneUncached();

	ASSERT(undoFilterStack.size() <=MAX_UNDO_SIZE);


	undoFilterStack.push_back(newTree);

	for(tree<Filter*>::iterator it=filters.begin();it!=filters.end(); ++it)
		delete *it;

#ifdef DEBUG
	for(tree<Filter*>::iterator it=redoFilterStack.back().begin();
			it!=redoFilterStack.back().end(); ++it)
	{
		ASSERT(!(*it)->haveCache());
	}
#endif


	//Swap the current filter cache out with the redo stack result
	std::swap(filters,redoFilterStack.back());
	
	//Pop the redo stack
	redoFilterStack.pop_back();

	//Filter topology has changed. Update
	initFilterTree();
}


void VisController::clearRedoStack()
{
	//Delete the undo stack trees
	while(redoFilterStack.size())
	{
		//clean up
		for(tree<Filter * >::iterator it=redoFilterStack.back().begin(); 
							it!=redoFilterStack.back().end(); ++it)
		{
			delete *it;
		}

		redoFilterStack.pop_back();	
	}
}

void VisController::updateConsole(const std::vector<std::string> &v, const Filter *f) const
{
	for(unsigned int ui=0; ui<v.size();ui++)
	{
		std::string s;
		s = f->getUserString() + string(" : ") + v[ui];
		textConsole->AppendText(wxStr(s));
		textConsole->AppendText(_("\n"));

	}
}

void VisController::invalidateRangeCaches()
{

	vector<const Filter * > rangeFilters;

	for(tree<Filter * >::iterator it=filters.begin(); 
					it!=filters.end(); ++it)
	{
		if((*it)->getType() == FILTER_TYPE_RANGEFILE)
			rangeFilters.push_back(*it);
	}


	for(size_t ui=0;ui<rangeFilters.size();ui++)
		invalidateCache(rangeFilters[ui]);
}

bool VisController::getAxisVisible()
{
	return targetScene->getWorldAxisVisible();
}


void VisController::setStrongRandom(bool strongRand)
{
	Filter::setStrongRandom(strongRand);

	//Invalidate every filter cache.
	invalidateCache(0);

}


void VisController::setEffects(bool enable)
{
	targetScene->setEffects(enable);
}

bool VisController::hasStateOverrides() const
{
	for(tree<Filter *>::iterator it=filters.begin(); it!=filters.end(); ++it)
	{
		vector<string> overrides;
		(*it)->getStateOverrides(overrides);

		if(overrides.size())
			return true;
	}

	for(unsigned int uj=0;uj<stashedFilters.size();uj++)
	{
		for(tree<Filter *>::iterator it=stashedFilters[uj].second.begin(); it!=stashedFilters[uj].second.end(); ++it)
		{
			vector<string> overrides;
			(*it)->getStateOverrides(overrides);

			if(overrides.size())
				return true;
		}
	}

	return false;
}
