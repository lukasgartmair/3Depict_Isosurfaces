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

#include "wxcommon.h"
#include "xmlHelper.h"
#include "viscontrol.h"

#include <list>
#include <stack>

#ifdef DEBUG
//Only needed for assertion check
#include <cstdlib>
#endif

#include "scene.h"
#include "drawables.h"

#include "translation.h"

using std::list;
using std::stack;

//oh no! global. window to safeYield/ needs to be set
wxWindow *yieldWindow=0;

wxStopWatch* delayTime=0;
//Another global - whether to abort the vis control 
// operation, or not
bool *abortVisCtlOp=0;

void upWxTreeCtrl(const FilterTree &filterTree, wxTreeCtrl *t,
		std::map<size_t,Filter *> &filterMap,vector<const Filter *> &persistentFilters,
		const Filter *visibleFilt)
{
	//Remove any filters that don't exist any more
	for(unsigned int ui=persistentFilters.size();ui--;)
	{
		if(!filterTree.contains(persistentFilters[ui]))
		{
			std::swap(persistentFilters[ui],persistentFilters.back());
			persistentFilters.pop_back();
		}
	}

	stack<wxTreeItemId> treeIDs;
	//Warning: this generates an event, 
	//most of the time (some windows versions do not according to documentation)
	t->DeleteAllItems();

	//Clear the mapping
	filterMap.clear();
	size_t nextID=0;
	
	size_t lastDepth=0;
	//Add dummy root node. This will be invisible to wxTR_HIDE_ROOT controls
	wxTreeItemId tid;
	tid=t->AddRoot(wxT("TreeBase"));
	t->SetItemData(tid,new wxTreeUint(nextID));

	// Push on stack to prevent underflow, but don't keep a copy, 
	// as we will never insert or delete this from the UI	
	treeIDs.push(tid);

	nextID++;
	std::map<const Filter*,wxTreeItemId> reverseFilterMap;
	//Depth first  add
	for(tree<Filter * >::pre_order_iterator filtIt=filterTree.depthBegin();
					filtIt!=filterTree.depthEnd(); filtIt++)
	{	
		//Push or pop the stack to make it match the iterator position
		if( lastDepth > filterTree.depth(filtIt))
		{
			while(filterTree.depth(filtIt) +1 < treeIDs.size())
				treeIDs.pop();
		}
		else if( lastDepth < filterTree.depth(filtIt))
		{
			treeIDs.push(tid);
		}
		

		lastDepth=filterTree.depth(filtIt);
	
		//This will use the user label or the type string.	
		tid=t->AppendItem(treeIDs.top(),
			wxStr((*filtIt)->getUserString()));
		t->SetItemData(tid,new wxTreeUint(nextID));
	

		//Record mapping to filter for later reference
		filterMap[nextID]=*filtIt;
		//Remember the reverse mapping for later in
		// this function when we reset visibility
		reverseFilterMap[*filtIt] = tid;

		nextID++;
	}

	//Try to restore the  selection in a user friendly manner
	// - Try restoring all requested filter's visibility
	// - then restore either the first requested filter as the selection
	//   or the specified parameter filter as the selection.
	if(persistentFilters.size())
	{
		for(unsigned int ui=0;ui<persistentFilters.size();ui++)
			t->EnsureVisible(reverseFilterMap[persistentFilters[ui]]);

		if(!visibleFilt)
			t->SelectItem(reverseFilterMap[persistentFilters[0]]);
		else
			t->SelectItem(reverseFilterMap[visibleFilt]);

		persistentFilters.clear();
	}
	else if(visibleFilt)
	{
		t->SelectItem(reverseFilterMap[visibleFilt]);
	}
	t->GetParent()->Layout();

}
//A callback function for yielding the window bound to viscontrol.
// Calling the callback will only cause a yield if sufficient time has passed
// the parameter describes whtehr or not to allow for overriding of the timer
bool wxYieldCallback(bool forceYield)
{
	const unsigned int YIELD_MS=75;

	ASSERT(delayTime);
	//Rate limit the updates
	if(delayTime->Time() > YIELD_MS || forceYield)
	{
		wxSafeYield(yieldWindow);
		delayTime->Start();
	}

	ASSERT(abortVisCtlOp);
	return !(*abortVisCtlOp);
}


VisController::VisController()
{
	targetScene=0;
	targetPlots=0;
	targetRawGrid=0;
	doProgressAbort=false;
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
	//clean up the stash trees
	stashedFilters.clear();

	//Delete the undo and redo stack trees
	undoFilterStack.clear();
	redoFilterStack.clear();

	ASSERT(delayTime);
	//delete global variable that visControl is responsible for
	delete delayTime;
	delayTime=0;
}

					
void VisController::addFilter(Filter *f, bool isBase,size_t parentId)
{ 
	if(!isBase)
		filterTree.addFilter(f,filterMap[parentId]);
	else
		filterTree.addFilter(f,0);

	//the filter map is now invalid, as we added an element to the tree,
	//and dont' have a unique value for it. we need to relayout.
	filterMap.clear();
}

void VisController::addFilterTree(FilterTree &f, bool isBase,size_t parentId)
{ 
	ASSERT(!(isBase && parentId==(unsigned int)-1));

	if(isBase)
		filterTree.addFilterTree(f,0);
	else
		filterTree.addFilterTree(f,filterMap[parentId]);

	//the filter map is now invalid, as we added an element to the tree,
	//and dont' have a unique value for it. we need to relayout.
	filterMap.clear();
}

void VisController::switchoutFilterTree(FilterTree &f)
{
	//Create a clone of the internal tree
	f=filterTree;

	//Fix up the internal filterMap to reflect the contents of the new tree
	//---
	//
	//Build a map from old filter*->new filter *
	tree<Filter*>::pre_order_iterator itB;
	itB=filterTree.depthBegin();
	std::map<Filter*,Filter*> filterRemap;
	for(tree<Filter*>::pre_order_iterator itA=f.depthBegin(); itA!=f.depthEnd(); itA++)
	{
		filterRemap[*itA]=*itB;	
		itB++;
	}

	//Overwrite the internal map
	for(map<size_t,Filter*>::iterator it=filterMap.begin();it!=filterMap.end();++it)
		it->second=filterRemap[it->second];


	//Swap the internal tree with our clone
	f.swap(filterTree);
}

//!Duplicate a branch of the tree to a new position. Do not copy cache,
bool VisController::copyFilter(size_t toCopy, size_t newParent,bool copyToRoot) 
{
	bool ret;
	if(copyToRoot)
		ret=filterTree.copyFilter(filterMap[toCopy],0);
	else
	
		ret=filterTree.copyFilter(filterMap[toCopy],filterMap[newParent]);

	if(ret)
	{
		//Delete the filtermap, as the current data is not valid anymore
		filterMap.clear();
	}

	return ret;
}


const Filter* VisController::getFilterById(size_t filterId) const 
{
	//If triggering this assertion, check that
	//::updateWxTreeCtrl called after calling addFilterTree.
	ASSERT(filterMap.size());

	//Check that the mapping exists
	ASSERT(filterMap.find(filterId)!=filterMap.end());
	return filterMap.at(filterId);
}


void VisController::getFiltersByType(std::vector<const Filter *> &filters, unsigned int type)  const
{
	filterTree.getFiltersByType(filters,type);
}


void VisController::removeFilterSubtree(size_t filterId)
{
       	filterTree.removeSubtree(filterMap[filterId]);
	//Delete the filtermap, as the current data is not valid anymore
	filterMap.clear();
}


bool VisController::setFilterProperty(size_t filterId, unsigned int set,
				unsigned int key, const std::string &value, bool &needUpdate)
{
	//Save current filter state to undo stack
	pushUndoStack();
	bool setOK;
	setOK=filterTree.setFilterProperty(filterMap[filterId],set,key,value,needUpdate);

	if(!setOK)
	{
		WARN(false,"need some kinda undo top pop w/o restore function");
		//popUndoStack(false);
	}

	return setOK;
}

unsigned int VisController::refreshFilterTree(bool doUpdateScene)
{
	doProgressAbort=false;
	amRefreshing=true;
	delayTime->Start();
	
	curProg.reset();
	
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
	vector<SelectionDevice<Filter> *> devices;
	vector<string> consoleMessages;
	errCode=filterTree.refreshFilterTree(refreshData,devices,
			consoleMessages,curProg,wxYieldCallback);

	for(size_t ui=0;ui<consoleMessages.size(); ui++)
	{
		textConsole->AppendText(wxStr(consoleMessages[ui]));
		textConsole->AppendText(wxT("\n"));
	}


	if(errCode)
	{
		amRefreshing=false;
		return errCode;
	}
	
	//strip off the source filter information for
	//convenience in this routine
	list<vector<const FilterStreamData *> > outData;
	for(list<filterOutputData>::iterator it=refreshData.begin(); it!=refreshData.end(); ++it)
		outData.push_back(it->second);
	
	
	targetScene->clearObjs();
	targetScene->addSelectionDevices(devices);

	if(doUpdateScene)
		updateScene(outData);
	//Stop timer
	delayTime->Pause();
	amRefreshing=false;

	return 0;
}

unsigned int VisController::refreshFilterTree(
		list<std::pair<Filter *,vector<const FilterStreamData * > > > &outData)
{
	vector<SelectionDevice<Filter> *> devices;
	vector<string> consoleStrs;
	return filterTree.refreshFilterTree(outData,devices,
			consoleStrs,curProg,wxYieldCallback);
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

void VisController::setWxTreeFilterViewPersistence(size_t filterId)
{
	persistentFilters.push_back(filterMap[filterId]);
}

void VisController::updateWxTreeCtrl(wxTreeCtrl *t, const Filter *visibleFilt)
{
	upWxTreeCtrl(filterTree,t,filterMap,persistentFilters,visibleFilt);
}

void VisController::updateFilterPropGrid(wxPropertyGrid *g,size_t filterId)
{
	//The filterID can never be set to zero,
	//except for the root item, as set by
	//upWxTreeCtrl
	ASSERT(filterId);
	ASSERT(filterMap.size() == filterTree.size());

	Filter *targetFilter;
	targetFilter=filterMap[filterId];

	ASSERT(targetFilter);
	
	updateFilterPropertyGrid(g,targetFilter);
}


bool VisController::setCamProperties(size_t camUniqueID,unsigned int key, const std::string &value)
{
	return targetScene->setCamProperty(camUniqueID,key,value);
}





bool VisController::hasUpdates() const
{
	if(pendingUpdates)
		return true;

	return filterTree.hasUpdates();

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
		for(tree<Filter *>::iterator it=filterTree.depthBegin(); 
				it!=filterTree.depthEnd();++it)
		{
			if(*it  == bindings[ui].first)
			{
				//We are modifying the contents of
				//the filter, this could make a change that
				//modifies output so we need to clear 
				//all subtree caches to force reprocessing
				filterTree.clearCache(*it);

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

unsigned int VisController::updateScene(list<vector<const FilterStreamData *> > &sceneData)
{


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
	for(list<vector<const FilterStreamData *> > ::iterator it=sceneData.begin(); 
							it!=sceneData.end(); ++it)
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
				
					(*wxYieldCallback)(true);
					//Randomly shuffle the ion data before we draw it
					curIonDraw->shuffle();
					
					(*wxYieldCallback)(true);

					
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
					ASSERT(plotData->getNumBasicObjects());
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
							DrawIsoSurface *d = new DrawIsoSurface;

							d->swapVoxels(v);
							d->setColour(vSrc->r,vSrc->g,
									vSrc->b,vSrc->a);
							d->setScalarThresh(vSrc->isoLevel);

							d->wantsLight=true;

							targetScene->addDrawable(d);
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
			
			//Run the callback to update the window as needed
			(*wxYieldCallback)(true);

		}
	
	}


	sceneData.clear();	
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
			l=(wxListUint*)plotSelList->GetClientObject(ui);
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
	for(list<vector<const FilterStreamData *> > ::iterator it=sceneData.begin(); 
							it!=sceneData.end(); ++it)
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

bool VisController::reparentFilter(size_t filter, size_t newParent)
{
	//Save current filter state to undo stack
	pushUndoStack();

	//Try to reparent this filter. It might not work, if, for example
	// the new parent is actually a child of the filter we are trying to
	// assign the parent to. 
	if(!filterTree.reparentFilter(filterMap[filter],filterMap[newParent]))
	{
		//Pop the undo stack, to reverse our 
		//push, but don't restore it,
		// as this would cost us our filter caches
		popUndoStack(false);
		return false;
	}
	
	return true;
}


bool VisController::setFilterString(size_t id,const std::string &s)
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
	
	//Write header, also use local language if available
	const char *headerMessage = NTRANS("This file is a \"state\" file for the 3Depict program, and stores information about a particular analysis session. This file should be a valid \"XML\" file");

	f << "<!--" <<  headerMessage;
	if(TRANS(headerMessage) != headerMessage) 
		f << endl << TRANS(headerMessage); 
	
	f << "-->" <<endl;



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
	if(!filterTree.saveXML(f,fileMapping,writePackage,useRelativePathsForSave))
		return  false;
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
			stashedFilters[ui].second.saveXML(f,fileMapping,
					writePackage,useRelativePathsForSave,1);
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

	//Debug check to ensure we have written a valid xml file
	ASSERT(isValidXML(cpFilename));

	return true;
}

bool VisController::loadState(const char *cpFilename, std::ostream &errStream, bool merge) 
{
	//Load the state from an XML file
	
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
	FilterTree newFilterTree;
	vector<Camera *> newCameraVec;
	vector<Effect *> newEffectVec;
	vector<pair<string,FilterTree > > newStashes;

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

			if(xmlString)
			{
				string tmpVer;
				
				tmpVer =(char *)xmlString;
				//Check to see if only contains 0-9 period and "-" characters (valid version number)
				if(tmpVer.find_first_not_of("0123456789.-")== std::string::npos)
				{
					//Check between the writer reported version, and the current program version
					vector<string> vecStrs;
					vecStrs.push_back(tmpVer);
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
		if(newFilterTree.loadXML(nodePtr,errStream,stateDir))
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
				FilterTree newStashTree;
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

				if(newStashTree.loadXML(nodePtr,errStream,stateDir))
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

	//Check that stashes are uniquely named
	// do brute force search, as there are unlikely to be many stashes	
	for(unsigned int ui=0;ui<newStashes.size();ui++)
	{
		for(unsigned int uj=0;uj<newStashes.size();uj++)
		{
			if(ui == uj)
				continue;

			//If these match, states not uniquely named,
			//and thus statefile is invalid.
			if(newStashes[ui].first == newStashes[uj].first)
				return false;

		}
	}

	if(!merge)
	{
		//Erase any viscontrol data, seeing as we got this far	
		clear(); 
		//Now replace it with the new data
		filterTree.swap(newFilterTree);
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
				newStashes[ui].first+=TRANS("-merge");

			if(maxCount)
				stashedFilters.push_back(newStashes[ui]);
			else
				errStream << TRANS(" Unable to merge stashes correctly. This is improbable, so please report this.") << endl;
		}

		filterTree.addFilterTree(newFilterTree,0);
		undoFilterStack.clear();
		redoFilterStack.clear();
		filterMap.clear();
	}

	//NOTE: We must be careful about :return: at this point
	// as we now have mixed in the new filter data.

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

	filterTree.initFilterTree();
	//Perform sanitisation on results
	return true;
}


void VisController::clear()
{
	filterMap.clear();

	filterTree.clear();

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


void VisController::addStashedToFilters(const Filter *parent, unsigned int stashId)
{
	ASSERT(stashId<stashedFilters.size());
	
	//Save current filter state to undo stack
	pushUndoStack();

	filterTree.addFilterTree(stashedFilters[stashId].second,parent);
	filterTree.initFilterTree();
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
	Filter *target = filterMap[filterId];
	
	FilterTree fTree;
	filterTree.cloneSubtree(fTree,target);
	
	stashedFilters.push_back(std::make_pair(string(stashName),fTree));
	return stashUniqueIDs.genId(stashedFilters.size()-1); 
}

void VisController::getStashTree(unsigned int stashId, FilterTree &f) const
{
	unsigned int pos = stashUniqueIDs.getPos(stashId);
	f = stashedFilters[pos].second;
}

void VisController::getStashes(std::vector<std::pair<std::string,unsigned int > > &stashList) const
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
	filterTree.setCachePercent(newCache);
}


void VisController::pushUndoStack()
{
	FilterTree newTree;
	newTree = filterTree;
	if(undoFilterStack.size() > MAX_UNDO_SIZE)
		undoFilterStack.pop_front();

	undoFilterStack.push_back(newTree);
	redoFilterStack.clear();
}

void VisController::popUndoStack(bool restorePopped)
{
	ASSERT(undoFilterStack.size());

	//Save the current filters to the redo stack.
	// note that the copy constructor will generate a clone for us.
	redoFilterStack.push_back(filterTree);

	if(redoFilterStack.size() > MAX_UNDO_SIZE)
		redoFilterStack.pop_front();

	if(restorePopped)
	{
		//Swap the current filter cache out with the undo stack result
		std::swap(filterTree,undoFilterStack.back());
		
		//Filter topology has changed. Update
		filterTree.initFilterTree();
	}

	//Pop the undo stack
	undoFilterStack.pop_back();
	

}

void VisController::popRedoStack()
{
	//Push the current state back onto the undo stack
	FilterTree newTree;

	//Copy the iterators onto the new tree
	//- due to copy const. we are modifying a clone, not the original
	newTree = filterTree;
	ASSERT(undoFilterStack.size() <=MAX_UNDO_SIZE);
	undoFilterStack.push_back(newTree);

	//Swap the current filter cache out with the redo stack result
	std::swap(filterTree,redoFilterStack.back());
	
	//Pop the redo stack
	redoFilterStack.pop_back();

	//Filter topology has changed. Update
	filterTree.initFilterTree();
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


bool VisController::getAxisVisible()
{
	return targetScene->getWorldAxisVisible();
}


void VisController::setStrongRandom(bool strongRand)
{
	Filter::setStrongRandom(strongRand);

	//Invalidate every filter cache.
	filterTree.clearCache(0);

}


void VisController::setEffects(bool enable)
{
	targetScene->setEffects(enable);
}


bool VisController::hasHazardousContents() const
{
	for(size_t ui=0;ui<stashedFilters.size();ui++)
	{
		if(stashedFilters[ui].second.hasHazardousContents())
			return true;
	}

	return filterTree.hasHazardousContents();
}

bool VisController::filterIsPureDataSource(size_t filterId) const
{
	ASSERT(filterId); //root node, as set by upWxTreeCtrl uses 0.
	ASSERT(filterMap.find(filterId)!=filterMap.end()); //needs to exist

	return filterMap.at(filterId)->isPureDataSource();
	
}

void VisController:: safeDeleteFilterList(std::list<std::pair<Filter *, std::vector<const FilterStreamData * > > > &outData, 
								size_t typeMask, bool maskPrevents) const
{
	filterTree.safeDeleteFilterList(outData,typeMask,maskPrevents);
}
//---------------

#ifdef DEBUG
void VisController::checkTree(wxTreeCtrl *t)
{
/*
	//spin through the non-root items in the tree,
	//and check they have a valid ID & map
	wxTreeItemIdValue cookie;
	wxTreeItemId debugId;


	stack<wxTreeItemId> itemStack;
	itemStack.push(t->GetRootItem());
	while(itemStack.size())
	{
		wxTreeItemData *tData;
		size_t mapVal;
	
		if(debugId.IsOk() && debugId!=t->GetRootItem())
		{	
			tData=t->GetItemData(debugId);
			mapVal=((wxTreeUint *)tData)->value;
			ASSERT(mapVal);
			ASSERT(filterMap.find(mapVal)!=filterMap.end());
		}

		debugId=t->GetNextSibling(debugId);
		if(debugId.IsOk())
		{
			itemStack.push(debugId);
			debugId=t->GetNextChild(debugId,cookie);
		}
		else
		{
			debugId=itemStack.top();
			itemStack.pop();
		}
	}
*/
}
#endif
