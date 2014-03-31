/*
 * 	viscontrol.h - Visualisation control header; "glue" between user interface and scene
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

#ifndef VISCONTROL_H
#define VISCONTROL_H

#include <wx/listbox.h>
#include <wx/textctrl.h>

class VisController;
class wxGrid;
class wxCustomPropGrid;
class wxTreeCtrl;

class Scene;

#include "filtertreeAnalyse.h"
#include "backend/plot.h"
#include "backend/animator.h"
#include "state.h"

#include "backend/APT/APTFileIO.h"


//!Visualisation controller
/*!
 * Keeps track of what visualisation controls the user has available
 * such as cameras, filters and data groups. 
 * This is essentially responsible for interfacing between program
 * data structures and the user interface.
 *
 * Only one of these should be instantiated at any time (such as due to abort mechanisms).
 */
class VisController
{
	private:
		//!Target scene
		Scene *targetScene;
		//!Target Plot wrapper system
		PlotWrapper *targetPlots;
		//!Target raw grid
		wxGrid *targetRawGrid;

		//!UI element for console output
		wxTextCtrl *textConsole;

		//!UI element for selecting plots from a list (for enable/disable)
		wxListBox *plotSelList;




		//--- Data storage ----
		AnalysisState currentState;
		
		//TODO: Migrate to analysis state
		//!Primary data storage for filter tree
		FilterTree filterTree;
	
		//!Analysis results for last filter tree refresh
		FilterTreeAnalyse fta;

		//--------------------

		
		//!True if viscontrol should abort current operation
		bool doProgressAbort;
	
		//!True if viscontrol is in the middle of a refresh operation
		bool amRefreshing;

		//!True if there are pending updates from the user
		bool pendingUpdates;

		//!Current progress
		ProgressData curProg;

		//!Maximum number of ions to pass to scene
		size_t limitIonOutput;

		//TODO: Move plot visbility data into state file, 
		// thus obviating the need to do this
		//!Should we defer altering plot visibility during refresh
		// this is used when loading a state file to prevent from bringing older selection state into current plots
		bool deferClearPlotVisibility;

		void clear();
	

		//!Retreive the updates to the filter tree from the scene
		void getFilterUpdates();



		//!Push the current filter tree onto the undo stack
		void pushUndoStack(); 


		//!Erase the redo stack
		void clearRedoStack();
	
		

		//!Update the console strings
		void updateConsole(const std::vector<std::string> &v, const Filter *f) const;

		
		//!Limit the number of objects we are sending to the scene
		void throttleSceneInput(std::list<vector<const FilterStreamData *> > &outputData) const;

		//!Force an update to the scene. 
		unsigned int updateScene(std::list<vector<const FilterStreamData *> > &outputData,
					std::vector<SelectionDevice *> &devices,bool releaseData=true);
		
		//!ID handler that assigns each filter its own ID that
		// is guaranteed to be unique for the life of the filter in the filterTree	
		std::map<size_t, Filter * > filterMap;		
		
		//Filters that should be able to be seen next time we show
		// the wxTree control
		std::vector<const Filter *> persistentFilters;


	public:
		VisController();
		~VisController();

		//Filter tree access functions
		//-----------------
		//!Run a refresh of the underlying tree. Returns 0 on success
		// iff return value == 0, then scene update has been initiated
		// otherwise returns error code value for filter, pr
		// one of the "higher" value error code, ( >=FILTERTREE_REFRESH_ERR_MEM);
		unsigned int refreshFilterTree(bool doUpdateScene=true);

		
		//!Force an update to the scene. 
		unsigned int doUpdateScene(std::list<vector<const FilterStreamData *> > &outputData, 
					std::vector<SelectionDevice *> &devices,bool releaseData=true);

		//obtain the outputs from the filter tree's refresh. 
		// The outputs *must* be deleted with safeDeleteFilterList
		unsigned int refreshFilterTree(
			std::list<std::pair<Filter *, 
				std::vector<const FilterStreamData * > > > &outputData); 

		//Obtain a clone of the active filter tree
		void cloneFilterTree(FilterTree &f) const{f=filterTree;};

		//!Safely delete data generated by refreshFilterTree(...). 
		//a mask can be used to *prevent* STREAM_TYPE_blah from being deleted. Deleted items are removed from the list.
		void safeDeleteFilterList(std::list<FILTER_OUTPUT_DATA> &outData, 
					size_t typeMask=0, bool maskPrevents=true) const;


		//!Add a new filter to the tree. Set isbase=false and parentID for not
		//setting a parent (ie making filter base)
		void addFilter(Filter *f, bool isBase, size_t parentId);
		
		
		//!Add a new subtree to the tree. Note that the tree will be cleared
		// as a result of this operation. Control of all pointers will be handled internally.
		// If you wish to use ::getFilterById you *must* rebuild the tree control with
		// ::updateWxTreeCtrl
		void addFilterTree(FilterTree &f,bool isBase=true, 
						size_t parentId=(unsigned int)-1); 

		//!Grab the filter tree from the internal one, and swap the 
		// internal with a cloned copy of the internal.
		// Can be used eg, to steal the cache
		// Note that the contents of the incoming filter tree will be destroyed.
		//  -> This implies the tree comes *OUT* of viscontrol,
		//     and a tree  cannot be inserted in via this function
		void switchoutFilterTree(FilterTree &f);

		//Perform a swap operation on the filter tree. 
		// - *must* have same topology, or you must call updateWxTreeCtrl
		// - can be used to *insert* a tree into this function
		void swapFilterTree(FilterTree &f) { f.swap(filterTree);}

		//!Duplicate a branch of the tree to a new position. Do not copy cache,
		bool copyFilter(size_t toCopy, size_t newParent,bool copyToRoot=false) ;

		//TODO: Deprecate me - filter information should not be leaking like this!
		//Get the ID of the filter from its actual pointer
		size_t getIdByFilter(const Filter* f) const;

		const Filter* getFilterById(size_t filterId) const; 

		//!Return all of a given type of filter from the filter tree. Type must be the exact type of filter - it is not a mask
		void getFiltersByType(std::vector<const Filter *> &filters, unsigned int type)  const;

		//!Returns true if the tree contains any state overrides (external entity referrals)
		bool hasStateOverrides() const 
				{return filterTree.hasStateOverrides();};

		//!Make the filters safe for the end user, assuming the filter tree could have had
		// its data initialised from anywhere
		void stripHazardousContents() { filterTree.stripHazardousContents();};

		//!Get the analysis results for the last refresh
		void getAnalysisResults(vector<FILTERTREE_ERR> &res) const { fta.getAnalysisResults(res);}

		//!Return the number of filters currently in the main tree
		size_t numFilters() const { return filterTree.size();};

		//!Clear the cache for the filters
		void purgeFilterCache() { filterTree.purgeCache();};

		//!Delete a filter and all its children
		void removeFilterSubtree(size_t filterId);

		//Move a filter from one part of the tree to another
		bool reparentFilter(size_t filterID, size_t newParentID);

		//!Set the properties using a key-value result (as obtained from updatewxCustomPropGrid)
		/*
		 * The return code tells whether to reject or accept the change. 
		 * need update tells us if the change to the filter resulted in a change to the scene
		 */
		bool setFilterProperty(size_t filterId,unsigned int key,
				const std::string &value, bool &needUpdate);
	
		//!Set the filter's string	
		bool setFilterString(size_t id, const std::string &s);
		
		//!Clear all caches
		void clearCache();
		
		//!Clear all caches
		void clearCacheByType(unsigned int type) { filterTree.clearCacheByType(type);};

		//Overwrite the contents of the pointed-to range files with
		// the map contents
		void modifyRangeFiles(const map<const RangeFile *, const RangeFile *> &toModify) { filterTree.modifyRangeFiles(toModify);};

		//-----------------


		//!Call to get viscontrol to abort current operation. Call once per abort.
		void abort()  ;

		//!Call to set window to be partially excluded (wx dependant) from blocking during scene updates
		void setYieldWindow(wxWindow *win);

		//!Set the text console
		void setConsole(wxTextCtrl *t) { textConsole = t;}
		//!Set the backend scene
		void setScene(Scene *theScene);
		//!Set the backend plot
		void setPlotWrapper(PlotWrapper *thePlots){targetPlots=thePlots;};
		
		PlotWrapper *getPlotWrapper(){return targetPlots;};
		//!Set the listbox for plot selection
		void setPlotList(wxListBox *box){plotSelList=box;};
		
		
		//!Set the backend grid control for raw data
		void setRawGrid(wxGrid *theRawGrid){targetRawGrid=theRawGrid;};
	
	
		//!Write out the filters into a wxtreecontrol.
		// optional argument is the fitler to keep visible in the control
		void updateWxTreeCtrl(wxTreeCtrl *t,const Filter *f=0);
		//!Update a wxCustomPropGrid with the properties for a given filter
		void updateFilterPropGrid(wxCustomPropGrid *g,size_t filterId) const;
			



		//!Set the camera to use in the scene
		bool setCam(unsigned int uniqueID) ;

		//!Remove a camera from the scene
		bool removeCam(unsigned int uniqueID);

		//!Add a new camera to the scene
		unsigned int addCam(const std::string &camName);

		//!Update a wxtGrid with the properties for a given filter
		void updateCamPropertyGrid(wxCustomPropGrid *g,unsigned int camId) const;
		
		//!Return the number of cameras
		unsigned int numCams() const ;

		//!Set the properties using a key-value result (as obtaed from updatewxCustomPropGrid)
		/*! The return code tells whether to reject or accept the change. 
		 */
		bool setCamProperties(size_t camId, 
				unsigned int key, const std::string &value);


		//!Ensure visible
		void ensureSceneVisible(unsigned int direction);

	

		//!Return the current working directory for when loading/saving state file contents
		std::string getWorkDir() const { return currentState.getWorkingDir();};
		
		//!Set current working dir used when saving state files
		void setWorkDir(const std::string &wd) { currentState.setWorkingDir(wd);};

		
		//!Save the viscontrol state: writes an XML file containing the viscontrol state
		bool saveState(const char *filename, std::map<string,string> &fileMapping,
					bool writePackage=false, bool resetModifyLevel=true) const;
		
		//!Save the viscontrol "package":this  writes an XML file containing the viscontrol state, altering the output of the filters to obtain the files it needs 
		bool savePackage(const char *filename) const;

		//!Load the viscontrol state, optionally merging this tree with the currently loaded tree. Also, as an option, we can bypass updating any UI data, for debug purposes	
		bool loadState(const char *filename, std::ostream &f,bool merge=false,bool noUpdating=false);	

		//!Are we currently using relative paths due to a previous load?
		bool usingRelPaths() const { return currentState.getUseRelPaths();};

		//!Set whether to use relative paths when saving
		void setUseRelPaths(bool useRelPaths){ currentState.setUseRelPaths(useRelPaths);};

		//!Get the camera ID-pair data TODO: this is kinda a halfway house between
		//updating the camera data internally and passing in the dialog box and not
		//should homogenise this...
		void getCamData(std::vector<std::string> &camData) const;

		//!Get the active camera ID
		unsigned int getActiveCamId() const;

		//Obtain updated camera from the scene and then commit it to the state
		void getCameraUpdates();

		//Set the maximum number of ions to allow the scene to display
		void setIonDisplayLimit(size_t newLimit) { limitIonOutput=newLimit;}

		size_t getIonDisplayLimit() const { return limitIonOutput;}


		//!export given filterstream data pointers
		static unsigned int exportIonStreams(const std::vector<const FilterStreamData *> &selected, 
								const std::string &outFile, unsigned int format=IONFORMAT_POS);
		
		//!Returns true if the filter is in the midst of a refresh
		bool isRefreshing() const {return amRefreshing; }

		//!Inform viscontrol that it has new updates to filters from external sources (eg bindings)
		void setUpdates() { pendingUpdates=true;};

		//!Returns true if the scene has updates that need to be processed
		bool hasUpdates() const; 


		//"Stash" operations
		//	- The stashes are secondary trees that are not part of the update
		//	  but can be stored for reference (eg to recall common tree sequences)
		//----
		//!Create a new stash. Returns ID for stash
		unsigned int stashFilters(unsigned int filterID, const char *stashName);

		//!Add a stash as a subtree for the specified parent filter
		void addStashedToFilters(const Filter *parentFilter, unsigned int stashID);

		//!Delete a stash using its uniqueID
		void eraseStash(unsigned int stashID);

		//!Retrieve the stash filters
		void getStashes(std::vector<std::pair<std::string,unsigned int > > &stashes) const;

		//!Retrieve a given stash tree by ID
		void getStashTree(unsigned int stashId, FilterTree &) const;

		//!Get the number of stashes
		unsigned int getNumStashes() const { return currentState.getStashCount();}

		//----

		void updateRawGrid() const;

		//!Set the percentage of ram to use for cache. 0 to disable
		void setCachePercent(unsigned int percent); 


		//!restore top filter tree from undo stack
		void popUndoStack(bool restorePopped=true);

		//!restore top filter tree from redo stack
		void popRedoStack();

		//!Get the size of the undo stack
		unsigned int getUndoSize() const { return currentState.getUndoSize();};
		//!Get the size of the redo stack
		unsigned int getRedoSize() const { return currentState.getRedoSize();};

		//!Get the current progress
		ProgressData getProgress() const { return curProg;}

		//!reset the progress data
		void resetProgress() { curProg.reset();};

		//!Return the scene's world axis visibility
		bool getAxisVisible() const;

		//!Set whether filter should use strong or weak randomisation
		void setStrongRandom(bool strongRand);

		//!Set whether to use effects or not
		void setEffects(bool enable); 

		
		//!return true if the primary tree contains hazardous filters
		bool hasHazardousContents() const ;

		//!Ask that next time we build the tree, this filter is kept visible/selected.
		//	may be used repeatedly to make more items visible, 
		//	prior to calling updateWxTreeCtrl. 
		//	filterId must exist during call.
		void setWxTreeFilterViewPersistence(size_t filterId);

		//Erase the filters that will persist in the view
		void clearTreeFilterViewPersistence() { persistentFilters.clear();}

		//!Ask if a given filter is a pure data source
		bool filterIsPureDataSource(size_t filterId) const ;  


		//true if the state has been modified since last load/save.
		int stateModifyLevel() const { return currentState.stateModifyLevel();};

		bool hasStateData() const { return currentState.hasStateData(); }

		//Return the current state's filename
		string getFilename() const { return currentState.getFilename(); }
		//Return the current state's filename
		void setFilename(std::string &s) {currentState.setFilename(s); }

		//!Set the animation state, by copy, overwriting the current one
		// pathmapping provides an animation ID <-> serialised filter path mapping
		void setAnimationState(const PropertyAnimator &pA, 
				const std::vector<pair<string,size_t> > &pathMapping) ;
		
		//!Retrieve the animation state, by copy, overwriting the current one
		void getAnimationState(PropertyAnimator &pA, 
				std::vector<pair<string,size_t> > &pathMapping) const;

#ifdef DEBUG
		//Check that the tree conrol is synced up to the filter map correctly
		void checkTree(wxTreeCtrl *t);
#endif
};


#endif
