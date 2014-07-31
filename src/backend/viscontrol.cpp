/*
 *	viscontrol.cpp - visualisation-user interface glue code
 *	Copyright (C) 2013, D Haley 

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

#include "viscontrol.h"

#include "wx/wxcommon.h"
#include "wx/wxcomponents.h"
#include "wx/propertyGridUpdater.h"
#include "gl/scene.h"



#include "common/stringFuncs.h"

using std::list;
using std::stack;

//Window that will be yielded to, when calling safeYield/ needs to be set in viscontrol
wxWindow *yieldWindow=0;

wxStopWatch* delayTime=0;
//Another global - whether to abort the vis control 
// operation, or not
bool *abortVisCtlOp=0;


//Number of points to limit to, by default
const unsigned int DEFAULT_POINT_OUTPUT_LIMIT = 1500000;

//A callback function for yielding the window bound to viscontrol.
// Calling the callback will only cause a yield if sufficient time has passed
// the parameter describes whether or not to allow for overriding of the timer
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

	limitIonOutput=DEFAULT_POINT_OUTPUT_LIMIT;


	amRefreshing=false;
	pendingUpdates=false;
	deferClearPlotVisibility=false;
	
	curProg.reset();
	//Assign global variable its init value
	ASSERT(!delayTime); //Should not have been inited yet.

	delayTime = new wxStopWatch();

}

VisController::~VisController()
{
	ASSERT(delayTime);
	//delete global variable that visControl is responsible for
	delete delayTime;
	delayTime=0;
}

void VisController::abort()
{
	ASSERT(!doProgressAbort);
	doProgressAbort=true;
}
					
void VisController::addFilter(Filter *f, bool isBase,size_t parentId)
{ 
	pushUndoStack();
	if(!isBase)
		filterTree.addFilter(f,filterMap[parentId]);
	else
		filterTree.addFilter(f,0);

	currentState.setFilterTreeByClone(filterTree);

	//the filter map is now invalid, as we added an element to the tree,
	//and don't have a unique value for it. We need to relayout.
	filterMap.clear();
}

void VisController::addFilterTree(FilterTree &f, bool isBase,size_t parentId)
{ 
	ASSERT(!(isBase && parentId==(unsigned int)-1));

	if(isBase)
		filterTree.addFilterTree(f,0);
	else
		filterTree.addFilterTree(f,filterMap[parentId]);

	currentState.setFilterTreeByClone(filterTree);
	//the filter map is now invalid, as we added an element to the tree,
	//and don't have a unique value for it. we need to relayout.
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
	for(tree<Filter*>::pre_order_iterator itA=f.depthBegin(); itA!=f.depthEnd(); ++itA)
	{
		ASSERT(itB != filterTree.depthEnd());
		filterRemap[*itA]=*itB;	
		++itB;
	}

	//Overwrite the internal map
	for(map<size_t,Filter*>::iterator it=filterMap.begin();it!=filterMap.end();++it)
		it->second=filterRemap[it->second];


	//Swap the internal tree with our clone
	f.swap(filterTree);
	
	currentState.setFilterTreeByClone(filterTree);
}

//!Duplicate a branch of the tree to a new position. Do not copy cache,
bool VisController::copyFilter(size_t toCopy, size_t newParent,bool copyToRoot) 
{
	pushUndoStack();
	
	bool ret;
	if(copyToRoot)
		ret=filterTree.copyFilter(filterMap[toCopy],0);
	else
	
		ret=filterTree.copyFilter(filterMap[toCopy],filterMap[newParent]);

	currentState.setFilterTreeByClone(filterTree);
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

size_t VisController::getPlotID(size_t position) const
{ 
	ASSERT(plotMap.size());
	ASSERT(plotMap.find(position)!=plotMap.end());
	return plotMap.find(position)->second;
}

size_t VisController::getIdByFilter(const Filter *f) const 
{
	for(map<size_t,Filter *>::const_iterator it=filterMap.begin();it!=filterMap.end();++it)
	{
		if(it->second == f)
			return it->first;

	}

	ASSERT(false);
}

void VisController::getFiltersByType(std::vector<const Filter *> &filters, unsigned int type)  const
{
	filterTree.getFiltersByType(filters,type);
}


void VisController::removeFilterSubtree(size_t filterId)
{
	//Save current filter state to undo stack
	pushUndoStack();
       	filterTree.removeSubtree(filterMap[filterId]);
	currentState.setFilterTreeByClone(filterTree);
	//Delete the filtermap, as the current data is not valid anymore
	filterMap.clear();
}


bool VisController::setFilterProperty(size_t filterId, 
				unsigned int key, const std::string &value, bool &needUpdate)
{
	//Save current filter state to undo stack
	//for the case where the property change is good
	pushUndoStack();
	bool setOK;
	setOK=filterTree.setFilterProperty(filterMap[filterId],key,value,needUpdate);

	if(!setOK)
	{
		//Didn't work, so we need to discard the undo
		//Pop the undo stack, but don't restore it -
		// restoring would destroy the cache
		popUndoStack(false);
	}
	else
		currentState.setFilterTreeByClone(filterTree);

	return setOK;
}

unsigned int VisController::refreshFilterTree(bool doUpdateScene)
{
	ASSERT(!amRefreshing);

	//Analyse the filter tree structure for any errors
	//--	
	fta.clear();
	fta.analyse(filterTree);
	//--

	doProgressAbort=false;
	amRefreshing=true;
	delayTime->Start();
	
	curProg.reset();
	
	list<FILTER_OUTPUT_DATA > refreshData;


	//Apply any remaining updates if we have them
	if(pendingUpdates)
		getFilterUpdates();
	targetScene->clearBindings();

	//Run the tree refresh system.
	unsigned int errCode;
	vector<SelectionDevice *> devices;
	vector<pair<const Filter*, string> > consoleMessages;
	errCode=filterTree.refreshFilterTree(refreshData,devices,
			consoleMessages,curProg,wxYieldCallback);


	const Filter *lastFilt=0;
	//Copy the console mesages to the output text box
	for(size_t ui=0;ui<consoleMessages.size(); ui++)
	{
		//If the consol messages are from a new filter
		if(lastFilt !=consoleMessages[ui].first || !ui)
		{
			//If we are at a new filter, which is not the first,
			//close out the "=" line
			if(ui)
				textConsole->AppendText(wxT("============\n\n\n"));

			lastFilt=consoleMessages[ui].first;
			textConsole->AppendText((lastFilt->getUserString()));
			textConsole->AppendText(wxT("\n============\n"));
		}
		textConsole->AppendText((consoleMessages[ui].second));
		textConsole->AppendText(wxT("\n"));
	}

	if(consoleMessages.size())
	{
		textConsole->AppendText(wxT("\n============\n"));
	}


	if(errCode)
	{
		amRefreshing=false;
		return errCode;
	}
	
	//strip off the source filter information for
	//convenience in this routine
	list<vector<const FilterStreamData *> > outData;
	for(list<FILTER_OUTPUT_DATA>::iterator it=refreshData.begin(); it!=refreshData.end(); ++it)
	{
		ASSERT(it->second.size());
		outData.push_back(it->second);
	}
	
	
	if(doUpdateScene)
		updateScene(outData,devices);
	else
	{
		targetScene->clearObjs();
		targetScene->clearRefObjs();
	}
	
	//Stop timer
	delayTime->Pause();
	amRefreshing=false;

	return 0;
}

unsigned int VisController::refreshFilterTree(list<FILTER_OUTPUT_DATA> &outData)
{
	vector<SelectionDevice *> devices;
	vector<pair<const Filter *, string> > consoleStrs;
	return filterTree.refreshFilterTree(outData,devices,
			consoleStrs,curProg,wxYieldCallback);
}

void VisController::setScene(Scene *theScene)
{
	targetScene=theScene;
	//Inform scene about vis control.
	targetScene->setViscontrol(this);
	
	Camera *c;
	c= targetScene->cloneActiveCam();
	currentState.addCamByClone(c);
	delete c;
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

void VisController::updateFilterPropGrid(wxPropertyGrid *g,size_t filterId, const std::string &stateStr) const
{

	//The filterID can never be set to zero,
	//except for the root item, as set by
	//upWxTreeCtrl
	ASSERT(filterId);
	ASSERT(filterMap.size() == filterTree.size());

	Filter *targetFilter;
	targetFilter=filterMap.at(filterId);

	ASSERT(targetFilter);
	
	updateFilterPropertyGrid(g,targetFilter,stateStr);
}

void VisController::updateCameraPropGrid(wxPropertyGrid *g, size_t camId) const
{
	ASSERT(g);

	const Camera *c;
	c= currentState.getCam(camId);

	updateCameraPropertyGrid(g,c);
}

bool VisController::setCamProperties(size_t camID,unsigned int key, const std::string &value)
{
	size_t offset=camID;

	bool result=currentState.setCamProperty(offset,key,value);
	if(result && offset == currentState.getActiveCam())
	{
		const Camera *c;
		c=currentState.getCam(currentState.getActiveCam());
		targetScene->setActiveCamByClone(c);
	}

	return result;
}

void VisController::getCameraUpdates()
{
	const Camera *c=targetScene->getActiveCam();
	currentState.setCameraByClone(c,currentState.getActiveCam());
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

				filterTree.clearCache(*it,false);

				(*it)->setPropFromBinding(bindings[ui].second);
#ifdef DEBUG
				haveBind=true;
#endif
				break;
			}
		}

		ASSERT(haveBind);

	}

	targetScene->resetModifiedBindings();

	currentState.setFilterTreeByClone(filterTree);
	//we have retrieved the updates.
	pendingUpdates=false;
}

//public interface to updateScene
unsigned int VisController::doUpdateScene(list<vector<const FilterStreamData *> > &sceneData, vector<SelectionDevice *> &devices,
		bool releaseData)
{
	amRefreshing=true;
	unsigned int errCode=updateScene(sceneData,devices,releaseData);
	amRefreshing=false;
	return errCode;

}

void VisController::throttleSceneInput(list<vector<const FilterStreamData *> > &sceneData) const
{
	//Count the number of input ions, as we may need to perform culling,
	if(!limitIonOutput)
		return;

	size_t inputIonCount=0;
	for(list<vector<const FilterStreamData *> >::const_iterator it=sceneData.begin(); 
							it!=sceneData.end(); ++it)
		inputIonCount+=numElements(*it,STREAM_TYPE_IONS);

	//If limit is higher than what we have, no culling required
	if(limitIonOutput >=inputIonCount)
		return;

	//Need to cull
	float cullFraction = (float)limitIonOutput/(float)inputIonCount;

	for(list<vector<const FilterStreamData *> >::iterator it=sceneData.begin(); 
							it!=sceneData.end(); ++it)
	{
		for(unsigned int ui=0;ui<it->size(); ui++)
		{
			if((*it)[ui]->getStreamType() != STREAM_TYPE_IONS)
				continue;
			//Obtain the ion data pointer
			const IonStreamData *ionData;
			ionData=((const IonStreamData *)((*it)[ui]));


			//TODO: Is there a way we can treat cached and uncached itmes
			// differently, without violating const-ness?
			//Duplicate this object, then forget
			// about the old one
			// We can't modify the input, even when uncached,
			// as the object is const
			IonStreamData *newIonData;
			newIonData=ionData->cloneSampled(cullFraction);

			//Prevent leakage due to non-cached-ness
			if(!ionData->cached)
				delete ionData;

			(*it)[ui] = newIonData;

		}
	}
		


}


unsigned int VisController::updateScene(list<vector<const FilterStreamData *> > &sceneData, vector<SelectionDevice *> &devices,
				bool releaseData)
{
	//Plot wrapper should be set
	ASSERT(targetPlots);
	//Plot window should be set
	ASSERT(plotSelList)

	//Should be called from a viscontrol refresh
	ASSERT(amRefreshing);

	//Lock the opengl scene interaction,
	// to prevent user interaction (e.g. devices) during callbacks
	targetScene->lockInteraction();
	targetPlots->lockInteraction();

	//Buffer to transfer to scene
	vector<DrawableObj *> sceneDrawables;
	
	//erase the contents of each plot 
	if(deferClearPlotVisibility)
		deferClearPlotVisibility=false;
	else
		targetPlots->clear(true); //Clear, but preserve selection information.


	//Names for plots
	vector<std::pair<size_t,string> > plotLabels;

	//rate-limit the number of drawables to show in the scene
	throttleSceneInput(sceneData);

	//-- Build buffer of new objects to send to scene
	for(list<vector<const FilterStreamData *> > ::iterator it=sceneData.begin(); 
							it!=sceneData.end(); ++it)
	{

		ASSERT(it->size());
		for(unsigned int ui=0;ui<it->size(); ui++)
		{
			//Filter must specify whether it is cached or not. Other values
			//are inadmissible, but useful to catch uninitialised values
			ASSERT((*it)[ui]->cached == 0 || (*it)[ui]->cached == 1);

			switch((*it)[ui]->getStreamType())
			{
				case STREAM_TYPE_IONS:
				{
					//Create a new group for this stream. 
					// We have to have individual groups 
					// because of colouring/sizing concerns.
					DrawManyPoints *curIonDraw;
					curIonDraw=new DrawManyPoints;


					//Obtain the ion data pointer
					const IonStreamData *ionData;
					ionData=((const IonStreamData *)((*it)[ui]));


					curIonDraw->resize(ionData->data.size());
					//Slice out just the coordinate data for the 
					// ion pointer, run callback immediately 
					// after, as its a long operation
					#pragma omp parallel for shared(curIonDraw,ionData)
					for(size_t ui=0;ui<ionData->data.size();ui++)
						curIonDraw->setPoint(ui,ionData->data[ui].getPosRef());
					(*wxYieldCallback)(true);
					
					//Set the colour from the ionstream data
					curIonDraw->setColour(ionData->r,
								ionData->g,
								ionData->b,
								ionData->a);
					//set the size from the ionstream data
					curIonDraw->setSize(ionData->ionSize);
					//Randomly shuffle the ion data before we draw it
					curIonDraw->shuffle();
					//Run callback to update as needed, as shuffle is slow.
					(*wxYieldCallback)(true);
				
					//place in special holder for ions,
					// as we need to accumulate for display-listing
					// later.
					sceneDrawables.push_back(curIonDraw);
					break;
				}
				case STREAM_TYPE_PLOT:
				{
					const PlotStreamData *plotData;
					plotData=((PlotStreamData *)((*it)[ui]));
					
					//The plot should have some data in it.
					ASSERT(plotData->getNumBasicObjects());
					//The plot should have an index, so we can keep
					//filter choices between refreshes (where possible)
					ASSERT(plotData->index !=(unsigned int)-1);
					//Construct a new plot
					unsigned int plotID;

					
					//No other plot mode is currently implemented.
					ASSERT(plotData->plotMode == PLOT_MODE_1D);
					
					//Create a 1D plot
					Plot1D *plotNew= new Plot1D;

					plotNew->setData(plotData->xyData);
					plotNew->setLogarithmic(plotData->logarithmic);
					plotNew->titleAsRawDataLabel=plotData->useDataLabelAsYDescriptor;
					
					//Construct any regions that the plot may have
					for(unsigned int ui=0;ui<plotData->regions.size();ui++)
					{
						//add a region to the plot,
						//using the region data stored
						//in the plot stream
						plotNew->regionGroup.addRegion(plotData->regionID[ui],
							plotData->regionTitle[ui],	
							plotData->regions[ui].first,
							plotData->regions[ui].second,
							plotData->regionR[ui],
							plotData->regionG[ui],
							plotData->regionB[ui],plotData->regionParent);
					}

					//transfer the axis labels
					plotNew->setStrings(plotData->xLabel,
						plotData->yLabel,plotData->dataLabel);
					
					//set the appearance of the plot
					//plotNew->setTraceStyle(plotStyle);
					plotNew->setColour(plotData->r,plotData->g,plotData->b);
					
					
					plotNew->parentObject=plotData->parent;
					plotNew->parentPlotIndex=plotData->index;
					
					plotID=targetPlots->addPlot(plotNew);

					plotLabels.push_back(make_pair(plotID,plotData->dataLabel));
					
					break;
				}
				//TODO: Merge back into STREAM_TYPE_PLOT
				case STREAM_TYPE_PLOT2D:
				{
					const Plot2DStreamData *plotData;
					plotData=((Plot2DStreamData *)((*it)[ui]));
					//The plot should have some data in it.
					ASSERT(plotData->getNumBasicObjects());
					//The plot should have an index, so we can keep
					//filter choices between refreshes (where possible)
					ASSERT(plotData->index !=(unsigned int)-1);
					unsigned int plotID;
		
					PlotBase *plotNew;
					switch(plotData->plotType) 
					{
						case PLOT_2D_DENS:
						{
							//Create a 2D plot
							plotNew= new Plot2DFunc;

							//set the plot info
							((Plot2DFunc*)plotNew)->setData(plotData->xyData,
										plotData->xMin, plotData->xMax, 
										plotData->yMin,plotData->yMax);
						
							break;
						}
						case PLOT_2D_SCATTER:
						{
							//Create a 2D plot
							plotNew= new Plot2DScatter;

							//set the plot info
							if(plotData->scatterIntensity.size())
								((Plot2DScatter*)plotNew)->setData(plotData->scatterData,plotData->scatterIntensity);
							else
								((Plot2DScatter*)plotNew)->setData(plotData->scatterData);
							//FIXME: scatter intesity data??
							break;
						}
						default:
							ASSERT(false);
					}
					//transfer the axis labels
					plotNew->setStrings(plotData->xLabel,
						plotData->yLabel,plotData->dataLabel);
					
					//transfer the parent info
					plotNew->parentObject=plotData->parent;
					plotNew->parentPlotIndex=plotData->index;
					
					plotID=targetPlots->addPlot(plotNew);
					
					// -----

					plotLabels.push_back(make_pair(plotID,plotData->dataLabel));
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
					if(drawData->cached)
					{
						//Create a *copy* for scene. Filter still holds
						//originals, and will dispose of the pointers accordingly
						for(unsigned int ui=0;ui<drawObjs->size();ui++)
							sceneDrawables.push_back((*drawObjs)[ui]->clone());
					}
					else
					{
						//Place the *pointers* to the drawables in the scene
						// list, to avoid copying
						for(unsigned int ui=0;ui<drawObjs->size();ui++)
							sceneDrawables.push_back((*drawObjs)[ui]);

						//Although we do not delete the drawable objects
						//themselves, we do delete the container
						
						//Zero-size the internal vector to 
						//prevent vector destructor from deleting pointers
						//we have transferred ownership of to scene
						drawData->drawables.clear();
					}
					break;
				}
				case STREAM_TYPE_RANGE:
					//silently drop rangestreams
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

							sceneDrawables.push_back(d);
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

							sceneDrawables.push_back(d);
							break;
						}
						default:
							ASSERT(false);
							delete v;
							;
					}

					break;
				}
			}
			
			//delete drawables as needed
			if(!(*it)[ui]->cached && releaseData)
			{
				delete (*it)[ui];
				(*it)[ui]=0;	
			}
			
			//Run the callback to update the window as needed
			(*wxYieldCallback)(true);

		}
			
	}
	//---

	//Construct an OpenGL display list from the dataset

	//Check how many points we have. Too many can cause the display list to crash
	size_t totalIonCount=0;
	for(unsigned int ui=0;ui<sceneDrawables.size();ui++)
	{
		if(sceneDrawables[ui]->getType() == DRAW_TYPE_MANYPOINT)
			totalIonCount+=((const DrawManyPoints*)(sceneDrawables[ui]))->getNumPts();
	}
	
	
	//Must lock UI controls, or not run callback from here on in!
	//==========

	//Update the plotting UI contols
	//-----------
	plotSelList->Clear(); // erase wx list
	plotMap.clear();
	for(size_t ui=0;ui<plotLabels.size();ui++)
	{
		//Append the plot to the list in the user interface
		plotSelList->Append((plotLabels[ui].second));
		plotMap[ui] = plotLabels[ui].first;
	}

	//If there is only one spectrum, select it
	if(plotSelList->GetCount() == 1 )
		plotSelList->SetSelection(0);
	else if( plotSelList->GetCount() > 1)
	{
		//Otherwise try to use the last visibility information
		//to set the selection
		targetPlots->bestEffortRestoreVisibility();

	}

	for(unsigned int ui=0; ui<plotSelList->GetCount();ui++)
	{
#if defined(__WIN32__) || defined(__WIN64__)
		//Bug under windows. SetSelection(wxNOT_FOUND) does not work for multi-selection list boxes
		plotSelList->SetSelection(-1, false);
#else
 		plotSelList->SetSelection(wxNOT_FOUND); //Clear selection
#endif
		for(unsigned int ui=0; ui<plotSelList->GetCount();ui++)
		{
			//Retrieve the uniqueID
			unsigned int plotID;
			plotID=plotMap[ui];
			if(targetPlots->isPlotVisible(plotID))
				plotSelList->SetSelection(ui);
		}
	}
	targetPlots->lockInteraction(false);
	//-----------
		

	
	targetScene->clearObjs();
	targetScene->clearRefObjs();



	//For speed, we have to treat ions specially.
	// for now, use a display list (these are no longer recommended in opengl, 
	// but they are much easier to use than extensions)
	vector<DrawManyPoints *> drawIons;
	for(size_t ui=0;ui<sceneDrawables.size();ui++)
	{
		if(sceneDrawables[ui]->getType() == DRAW_TYPE_MANYPOINT)
		{
			drawIons.push_back((DrawManyPoints*)sceneDrawables[ui]);
			sceneDrawables.erase(sceneDrawables.begin()+ui);
			ui--;

		}
	}

	if(totalIonCount < MAX_NUM_DRAWABLE_POINTS && drawIons.size() >1)
	{
		//Try to use a display list where we can.
		//note that the display list requires a valid bounding box,
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
		else
			delete displayList;
	}
	else
	{
		for(unsigned int ui=0;ui<drawIons.size(); ui++)
			targetScene->addDrawable(drawIons[ui]);
	}

	//add all drawable objects (not ions)	
	for(size_t ui=0;ui<sceneDrawables.size();ui++)
		targetScene->addDrawable(sceneDrawables[ui]);
	
	sceneDrawables.clear();
	targetScene->computeSceneLimits();
	targetScene->addSelectionDevices(devices);
	targetScene->lockInteraction(false);
	//===============



	return 0;
}




unsigned int VisController::addCam(const std::string &camName)
{
	//Duplicate the current camera, and give it a new name
	Camera *c=targetScene->cloneActiveCam();
	c->setUserString(camName);

	//create an ID for this camera, then add to scene 
	size_t id =currentState.getNumCams();
	
	currentState.addCamByClone(c);
	delete c;
	
	return id;
}

bool VisController::removeCam(unsigned int offset)
{
	//obtain the offset to the camera from its unique id
	currentState.removeCam(offset);

	if(!currentState.getNumCams())
	{
		const Camera *c;
		c = targetScene->getActiveCam();
		currentState.addCamByClone(c);
	}
	else
	{
		size_t activeCam=currentState.getActiveCam();
		targetScene->setActiveCamByClone(currentState.getCam(activeCam) );
	}

	return true;
}

bool VisController::setCam(unsigned int offset)
{
	ASSERT(offset < currentState.getNumCams());
	
	currentState.setActiveCam(offset);
	targetScene->setActiveCamByClone(currentState.getCam(offset));
	
	return true;
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
	
	currentState.setFilterTreeByClone(filterTree);
	return true;
}


bool VisController::setFilterString(size_t id,const std::string &s)
{
	
	Filter *p=(Filter *)getFilterById(id);

	if(s != p->getUserString())
	{
		//Save current filter state to undo stack
		pushUndoStack();
		
		//Do the actual update
		p->setUserString(s);
		
		//Update the current state	
		currentState.setFilterTreeByClone(filterTree);
		return true;
	}

	return false;
}

unsigned int VisController::numCams() const 
{
	return currentState.getNumCams();
}
		
void VisController::ensureSceneVisible(unsigned int direction)
{
	currentState.setStateModified(true);
	targetScene->ensureVisible(direction);
}


bool VisController::saveState(const char *cpFilename, std::map<string,string> &fileMapping,
		bool writePackage,bool resetModifyLevel) const
{
	AnalysisState state;


	//Make a copy of the state, as some variables are still stored in viscontrol's scope
	state=currentState; 
	
	
	//-- scene variables --
	float rBack,gBack,bBack;
	targetScene->getBackgroundColour(rBack,gBack,bBack);
	state.setBackgroundColour(rBack,gBack,bBack);
       	
	state.setWorldAxisMode(targetScene->getWorldAxisVisible());

	vector<const Effect *> effectVec;
	
	targetScene->getEffects(effectVec);
	state.setEffectsByCopy(effectVec);
	//------

	//Plotting variables
	//------------------
	//TODO: Migrate plot status out of viscontrol
	// and into state
	state.setPlotLegend(targetPlots->getLegendVisible());

	{
	vector<unsigned int> visiblePlotIDs;
	targetPlots->getVisibleIDs(visiblePlotIDs);
	
	//Obtain filter pointer -> serialised name data
	map<const Filter *,string> serialisedFilterNames;
	filterTree.serialiseToStringPaths(serialisedFilterNames);

	vector<pair<string, unsigned int> > visiblePlotNames;
	visiblePlotNames.resize(visiblePlotIDs.size());

	for(size_t ui=0;ui<visiblePlotIDs.size (); ui++)
	{
		unsigned int thisID;
		thisID= visiblePlotIDs[ui];

		const Filter *f;
		f=(const Filter *)targetPlots->getParent(thisID);
	
		ASSERT(serialisedFilterNames.find(f) != serialisedFilterNames.end());
		visiblePlotNames[ui] = make_pair(
			serialisedFilterNames[f], targetPlots->getParentIndex(thisID));
	}

	state.setEnabledPlots(visiblePlotNames);
	}
	//------------------

	//-- viscontrol variables
	state.setFilterTreeByClone(filterTree);
	//--

	if(resetModifyLevel)
		currentState.setStateModified(false);


	return state.save(cpFilename,fileMapping,writePackage);
}

bool VisController::loadState(const char *cpFilename, std::ostream &errStream, bool merge,bool noUpdating) 
{

	//Load into a temporary state
	// and if successful, transfer to full state
	AnalysisState state;
	bool result=state.load(cpFilename,errStream,merge);

	if(!result)
		return false;

	currentState=state;

	//Synchronise scene and viscontrol components to 
	// current state

	//Grab the filter tree from the state
	currentState.copyFilterTree(filterTree);
	
	if(!noUpdating)
	{
		// -- Set scene options --
		float rBack,gBack,bBack;
		currentState.getBackgroundColour(rBack,gBack,bBack);
		targetScene->setBackgroundColour(rBack,gBack,bBack);

		targetScene->setWorldAxisVisible(currentState.getWorldAxisMode());

		vector<const Camera *> cams;
		currentState.copyCamsByRef(cams);

		if(cams.size())
		{
			size_t activeCam=currentState.getActiveCam();
			targetScene->setActiveCamByClone(cams[activeCam]);
		}

		vector<Effect*> e;
		currentState.copyEffects(e);
		targetScene->setEffectVec(e);
		//----

		//Conver the enabled plots to underlying
		// pointer representation, then pass to 
		// plot functions, so it can enable plots
		// after refresh
		vector<pair<string, unsigned int> > enabledPlotsPath;
		currentState.getEnabledPlots(enabledPlotsPath);

		map<string,const Filter *> pathMap;
		filterTree.serialiseToStringPaths(pathMap);	
		
		vector<pair<const void *, unsigned int> > enabledPlotsPtr;
		for(unsigned int ui=0;ui<enabledPlotsPath.size();ui++)
		{
			std::string curPath;
			curPath=enabledPlotsPath[ui].first;
			//Check to see if the filter tree we loaded
			// has the same info as the selected item
			if(pathMap.find(curPath)!=pathMap.end())
			{
		
				enabledPlotsPtr.push_back(
					make_pair((void *)pathMap[curPath],enabledPlotsPath[ui].second));
			}
		}

		//override the target plots internal rep. of what is visible,
		// so that at next refresh it will pick this up and auto-select the plots,
		// if it caun
		targetPlots->overrideLastVisible(enabledPlotsPtr); 
		deferClearPlotVisibility=true;
	}




	fta.clear();

	filterTree.initFilterTree();
		
	//Try to restore the working directory as needed
	std::string wd;
	wd=currentState.getWorkingDir();
	if(wd.size() && wxDirExists((wd)))
		wxSetWorkingDirectory((currentState.getWorkingDir()));

	currentState.setStateModified(false);
	return true;
}

void VisController::clear()
{
	filterMap.clear();
	filterTree.clear();
	fta.clear();

	currentState.setFilterTreeByClone(filterTree);
	currentState.clearUndoRedoStacks();
}

void VisController::getCamData(std::vector<std::string>  &camData) const
{
	vector<const Camera *> camRefs;
	currentState.copyCamsByRef(camRefs);	
	
	camData.resize(camRefs.size());
	for(size_t ui=0;ui<camRefs.size();ui++)
	{
		camData[ui]=camRefs[ui]->getUserString();
	}
}

unsigned int VisController::getActiveCamId() const
{ 
	return currentState.getActiveCam();
}

unsigned int VisController::exportIonStreams(const std::vector<const FilterStreamData * > &selectedStreams,
		const std::string &outFile, unsigned int format)
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
						IonHit::appendPos(ionData->data,outFile.c_str());

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


void VisController::addStashedToFilters(const Filter *parentFilter, unsigned int stashOffset)
{
	//Retrieve the specified stash
	pair<string,FilterTree> f;

	currentState.copyStashedTree(stashOffset,f);

	filterTree.addFilterTree(f.second,parentFilter);
	currentState.setFilterTreeByClone(filterTree);

	//Save current filter state to undo stack
	pushUndoStack();

	
	filterTree.initFilterTree();
}

void VisController::eraseStash(unsigned int stashPos)
{
	//Remove viscontrol's mapping to this
	currentState.eraseStash(stashPos);
}

unsigned int VisController::stashFilters(unsigned int filterId, const char *stashName)
{
	Filter *target = filterMap[filterId];
	
	FilterTree fTree;
	filterTree.cloneSubtree(fTree,target);

	currentState.addStashedTree(std::make_pair(string(stashName),fTree));
	return currentState.getStashCount()-1; 
}

void VisController::getStashTree(unsigned int stashPos, FilterTree &f) const
{
	pair<string,FilterTree > stash;
	currentState.copyStashedTree(stashPos,stash);
	f=stash.second;
}

//TDOO: Drop pairm and just use string
void VisController::getStashes(std::vector<std::pair<std::string,unsigned int > > &stashList) const
{
	ASSERT(stashList.empty()); // should be empty	

	vector<pair<string,FilterTree> > tmp;
	currentState.copyStashedTrees(tmp);
	stashList.resize(tmp.size());

	for(size_t ui=0;ui<tmp.size();ui++)
		stashList[ui]= std::make_pair(tmp[ui].first,ui);
}

void VisController::updateRawGrid() const
{
	vector<vector<vector<float> > > plotData;
	vector<std::vector<std::string> > labels;
	//grab the data for the currently visible plots
	targetPlots->getRawData(plotData,labels);



	//Clear the grid
	if(targetRawGrid->GetNumberCols())
		targetRawGrid->DeleteCols(0,targetRawGrid->GetNumberCols());
	if(targetRawGrid->GetNumberRows())
		targetRawGrid->DeleteRows(0,targetRawGrid->GetNumberRows());
	
	unsigned int curCol=0;
	for(unsigned int ui=0;ui<plotData.size(); ui++)
	{
		unsigned int startCol;
		//Create new columns
		targetRawGrid->AppendCols(plotData[ui].size());
		ASSERT(labels[ui].size() == plotData[ui].size());

		startCol=curCol;
		for(unsigned int uj=0;uj<labels[ui].size();uj++)
		{
			std::string s;
			s=(labels[ui][uj]);
			targetRawGrid->SetColLabelValue(curCol,(s));
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
				targetRawGrid->SetCellValue(uk,startCol,(tmpStr));
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
	currentState.pushUndoStack();
}

void VisController::popUndoStack(bool restorePopped)
{
	currentState.popUndoStack(restorePopped);

	if(restorePopped)
	{
		currentState.copyFilterTree(filterTree);
		filterTree.initFilterTree();
	}
}

void VisController::popRedoStack()
{
	currentState.popRedoStack();
	currentState.copyFilterTree(filterTree);
	
	filterTree.initFilterTree();
}

void VisController::updateConsole(const std::vector<std::string> &v, const Filter *f) const
{
	for(unsigned int ui=0; ui<v.size();ui++)
	{
		std::string s;
		s = f->getUserString() + string(" : ") + v[ui];
		textConsole->AppendText((s));
		textConsole->AppendText(wxT("\n"));

	}
}


bool VisController::getAxisVisible() const
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

void VisController::setAnimationState(const PropertyAnimator &p,
		const std::vector<pair<string,size_t> > &pathMapping) 
{
	//Add animation state saving
	currentState.setAnimationState(p,pathMapping);
}

void VisController::getAnimationState(PropertyAnimator &p, std::vector<pair<string,size_t> > &pathMapping) const
{
	currentState.getAnimationState(p,pathMapping);
}

bool VisController::hasHazardousContents() const
{
	//Filters can contain hazardous content - that is 
	// content that can come from untrusted sources, and be used
	// in a nefarious way. Check to see if any filter that
	// could potentially be used in such a manner exists in our
	// current state

	//Check the stashed state
	vector<pair<string, FilterTree > > stashedFilters;

	currentState.copyStashedTrees(stashedFilters);
	for(size_t ui=0;ui<stashedFilters.size();ui++)
	{
		if(stashedFilters[ui].second.hasHazardousContents())
			return true;
	}

	//Check the active filter tree
	FilterTree f;
	currentState.copyFilterTree(f);	
	return f.hasHazardousContents();
}

bool VisController::filterIsPureDataSource(size_t filterId) const
{
	ASSERT(filterId); //root node, as set by upWxTreeCtrl uses 0.
	ASSERT(filterMap.find(filterId)!=filterMap.end()); //needs to exist

	return filterMap.at(filterId)->isPureDataSource();
	
}

void VisController:: safeDeleteFilterList(std::list<FILTER_OUTPUT_DATA> &outData, 
								size_t typeMask, bool maskPrevents) const
{
	filterTree.safeDeleteFilterList(outData,typeMask,maskPrevents);
}
//---------------

