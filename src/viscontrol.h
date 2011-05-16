/*
 * 	viscontrol.h - Visualisation control header; "glue" between user interface and scene
 *	Copyright (C) 2010, D Haley 

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

#include <string>
#include <vector>
#include <utility>
#include <list>
#include <stack>
#include <deque>

#include <wx/wx.h>
#include <wx/treectrl.h>
#include <wx/grid.h>

#include "tree.hh"

#include "wxcomponents.h"

class VisController;

#include "scene.h"
#include "filter.h"
#include "plot.h"

enum
{
	CACHE_DEPTH_FIRST=1,
	CACHE_NEVER,
};

const unsigned int MAX_UNDO_SIZE=10;



//!Visualisation controller
/*!
 * Keeps track of what visualisation controls the user has available
 * such as cameras, filters and data groups. 
 * This is essentially responsible for interfacing between program
 * data structures and the user interface.
 *
 * Only one of these may be instantiated at any time due to abort mechanism
 */
class VisController
{
	private:
		unsigned long long nextID;
		//!Target scene
		Scene *targetScene;
		//!targetPlot
		Multiplot *targetPlots;
		//!Target raw grid
		wxGrid *targetRawGrid;

		//!UI element for console output
		wxTextCtrl *textConsole;

		//!UI element for selecting plots from a list (for enable/disable)
		wxListBox *plotSelList;

		//!Filters that provide and act upon data. filters.begin() is posfile.
		tree<Filter *> filters;
	
		//!Undo filter tree stack 
		std::deque<tree<Filter *> > undoFilterStack,redoFilterStack;

		//!Named, stored trees that can be put aside for secondary use
		std::vector<std::pair<std::string,tree<Filter *> > > stashedFilters;

		//!Unique IDs for stash
		UniqueIDHandler stashUniqueIDs;
		//!A mapping between wxitem IDs and filters TODO: Replace with uniqueID handler?
		std::list<std::pair<unsigned long long, Filter *> > filterTreeMapping;

		//!True if viscontrol should abort current operation
		bool doProgressAbort;
	
		//!True if viscontrol is in the middle of a refresh operation
		bool amRefreshing;

		//!True if there are pending updates from the user
		bool pendingUpdates;

		//!Maximum size for cache (percent of available ram).
		float maxCachePercent;

		//!Caching stragegy
		unsigned int cacheStrategy;

		//!Current progress
		ProgressData curProg;

		//!internal function for the lading of a filter tree from its XML representation
		unsigned int loadFilterTree(const xmlNodePtr &treeParent, tree<Filter *> &newTree, std::ostream &errStream, const std::string &stateDir) const;

		//!Internal function for pointer deletion from stack during refreshing filter tree
		void popPointerStack(std::list<const FilterStreamData *> &pointerTrackList,
				std::stack<vector<const FilterStreamData * > > &inDataStack, unsigned int depth) const;

		void clear();
	
		//!Return filter pointer using the filter id value	
		Filter* getFilterByIdNonConst(unsigned long long filterId) const;

		//!Retreive the updates to the filter tree from the scene
		void getFilterUpdates();

		//!Used to remove potentially hazardous filters 
		//(filters that can do nasty things to computers, like executing commands)
		//which may have come from unsafe sources
		void stripHazardousFilters(tree<Filter *> &tree);

		//!Does a particular filter tree contain hazardous contents?
		bool hasHazardous(const tree<Filter *> &tree) const;

		//!Push the current filter tree onto the undo stack
		void pushUndoStack(); 

		//!Run the initialisation stage of the filter processing
		void initFilterTree() const;

		//!Erase the redo stack
		void clearRedoStack();
		
		//!Update the console strings
		void updateConsole(const std::vector<std::string> &v, const Filter *f) const;

		//!Get the filter refresh seed points in tree, by examination of tree caches, block/emit of filters
		//and tree topology
		void getFilterRefreshStarts(vector<tree<Filter *>::iterator > &propStarts) const;
	public:
		VisController();
		~VisController();
		//!Call to get viscontrol to abort current operation. Call once per abort.
		void abort() {ASSERT(!doProgressAbort);doProgressAbort=true;} ;

		//!Call to set window to be partially excluded (wx dependant) from blocking during scene updates
		void setYieldWindow(wxWindow *win);

		//!Set the text console
		void setConsole(wxTextCtrl *t) { textConsole = t;}
		//!Set the backend scene
		void setScene(Scene *theScene);
		//!Set the backend plot
		void setPlot(Multiplot *thePlots){targetPlots=thePlots;};
		//!Set the listbox for plot selection
		void setPlotList(wxListBox *box){plotSelList=box;};
		
		
		//!Set the backend grid control for raw data
		void setRawGrid(wxGrid *theRawGrid){targetRawGrid=theRawGrid;};
	
	
		//!Load the ion set - returns nonzero on fail TODO: Remove me?
		unsigned int LoadIonSet(const std::string &name);
		//!Write out the filters into a wxtreecontrol.
		void updateWxTreeCtrl(wxTreeCtrl *t,const Filter *visibleFilter=0);
		//!Update a wxtGrid with the properties for a given filter
		void updateFilterPropGrid(wxPropertyGrid *g,unsigned long long filterId);
		//!Add a new filter to the tree
		void addFilter(Filter *f, unsigned long long parentId);

		//!Move a branch of the tree to a new position
		bool reparentFilter(unsigned long long filterID, unsigned long long newParentID);
		//!Duplicte a branch of the tree to a new position. Do not copy cache,
		bool copyFilter(unsigned long long toCopy, unsigned long long newParent,bool copyToRoot=false);

		//!Set the properties using a key-value result (as obtaed from updatewxPropertyGrid)
		/*
		 * The return code tells whether to reject or accept the change. 
		 * need update tells us if the change to the filter resulted in a change to the scene
		 */
		bool setFilterProperties(unsigned long long filterId, unsigned int set,
				unsigned int key, const std::string &value, bool &needUpdate);

		//!Retrieves a filter by its ID (TODO: Refactor. Try to minimise filter pointer exposure. Use ID values where possible)
		const Filter* getFilterById(unsigned long long filterId) const;
		//!Remove an element and all sub elements from the tree, 
		void removeTreeFilter(unsigned long long id);
		//!Force an update to the scene. 
		//Overall progress give the progress for all filters, as a numeral (1 of n)
		//filterprogress gives the progress within a filter, as a percentage
		//curFilter gives the current filter, or the latest filter if exiting
		//on error. if == 0, then scene update has been initiated
		unsigned int updateScene();
			

		//!Refresh the entire filter tree. Whilst this is public, great care must be taken in
		// deleting the filterstream data corrrectlty. To do this, use the "safeDeleteFilterList" function.
		unsigned int refreshFilterTree(	std::list<std::pair<Filter *, std::vector<const FilterStreamData * > > > &outData);

		//!Safely delete data generated by refreshFilterTree(...). 
		//a mask can be used to *prevent* STREAM_TYPE_blah from being deleted. Deleted items are removed from the list.
		void safeDeleteFilterList(std::list<std::pair<Filter *, std::vector<const FilterStreamData * > > > &outData, 
								unsigned long long typeMask=0, bool maskPrevents=true) const;

		//!Return the number of filters
		unsigned int numFilters() const {return filters.size();};

		//!Set the camera to use in the scene
		bool setCam(unsigned int uniqueID) ;

		//!Remove a camera from the scene
		bool removeCam(unsigned int uniqueID);

		//!Add a new camera to the scene
		unsigned int addCam(const std::string &camName);

		//!Update a wxtGrid with the properties for a given filter
		void updateCamPropertyGrid(wxPropertyGrid *g,unsigned int camUniqueId);
		
		//!Return the number of cameras
		unsigned int numCams() const ;

		//!Set the properties using a key-value result (as obtaed from updatewxPropertyGrid)
		/*! The return code tells whether to reject or accept the change. 
		 */
		bool setCamProperties(unsigned long long camUniqueID, unsigned int set,
					unsigned int key, const std::string &value);

		//!Set the filter user text
		bool setFilterString(unsigned long long id , const std::string &s);

		//!Ensure visible
		void ensureSceneVisible(unsigned int direction);

		//!Invalidate the cache of a given Filter and all its children. set to 0 to invalidate all.
		void invalidateCache(const Filter *filt);
		
		//!FIXME: Invalidates all caches downstream of ranges. Remove me. 
		/*This is a hack. This is a special
		call to invalidate all range file caches in the event
		of a region update, as we cannot match the region that
		was actually updated to the filter tree. This requires
		re-writing the selection & region feedback code to do a filter
		tree comparison between the "last" and current trees,
		which i don't do atm.*/
		void invalidateRangeCaches();

		//!Save the viscontrol state: writes an XML file containing the viscontrol state
		bool saveState(const char *filename, std::map<string,string> &fileMapping,
					bool writePackage=false) const;
		
		//!Save the viscontrol "package":this  writes an XML file containing the viscontrol state, altering the output of the filters to obtain the files it needs 
		bool savePackage(const char *filename) const;

		//!Load the viscontrol state.	
		bool loadState(const char *filename, std::ostream &f,bool merge=false);	

		//!Get the camera ID-pair data TODO: this is kinda a halfway house between
		//updating the camera data internally and passing in the dialog box and not
		//should homogenise this...
		void getCamData(std::vector<std::pair<unsigned int, std::string> > &camData) const;

		//!Get the active camera ID
		unsigned int getActiveCamId() const;

		//!export given filterstream data pointers
		unsigned int exportIonStreams(const std::vector<const FilterStreamData *> &selected, 
								const std::string &outFile, unsigned int format=IONFORMAT_POS) const;
		
		//!Return all of a given type of filter from the filter tree
		void getFilterTypes(std::vector<const Filter *> &filters, unsigned int type);

		//!Returns true if the filter is in the midst of a refresh
		bool isRefreshing() const {return amRefreshing; }

		//!Inform viscontrol that it has new updates to filters from external sources (eg bindings)
		void setUpdates() { pendingUpdates=true;};

		//!Returns true if the scene has updates that need to be processed
		bool hasUpdates() const {return pendingUpdates;} ;

		//!Force a wipe of all caches in the filter tree
		void purgeFilterCache();

		//!Create a new stash. Returns ID for stash
		unsigned int stashFilters(unsigned int filterID, const char *stashName);

		//!Add a stash as a subtree for the specified parent filter
		void addStashedToFilters(const Filter *parentFilter, unsigned int stashID);

		//!Delete a stash using its uniqueID
		void deleteStash(unsigned int stashID);

		//!Retreive the stash filters
		void getStashList(std::vector<std::pair<std::string,unsigned int > > &stashList) const;

		//!Retreive a given stash tree by ID
		void getStashTree(unsigned int stashId, tree<Filter *> &t) const;

		//!Write out the filters into a wxtreecontrol.
		void updateWxTreeCtrlFromStash(wxTreeCtrl *t, unsigned int stashId) const ;
		//!Write out the filters into a wxtreecontrol.
		void updateStashPropertyGrid(wxGrid *g, unsigned int stashId, const Filter *stashFilter) const ;

		void updateRawGrid() const;

		//!Set the percentage of ram to use for cache. 0 to disable
		void setCachePercent(unsigned int percent); 

		//!Make the filter system safe (non-hazardous)
		void makeFiltersSafe();

		//!return true if the viscontrol contains hazardous filters
		bool hasHazardousContents() const ;

		//!restore top filter tree from undo stack
		void popUndoStack();

		//!restore top filter tree from redo stack
		void popRedoStack();

		//!Get the size of the undo stack
		unsigned int getUndoSize() const { return undoFilterStack.size();};
		//!Get the size of the redo stack
		unsigned int getRedoSize() const { return redoFilterStack.size();};

		//!Get the current progress
		ProgressData getProgress() const { return curProg;}

		//!reset the progress data
		void resetProgress() { curProg.reset();};

		//!Reset the refreshing state. use very carefully.
		void setRefreshed(){amRefreshing=false;};

		//!Return the scene's world axis visibility
		bool getAxisVisible();

		//!Set whether filter should use strong or weak randomisation
		void setStrongRandom(bool strongRand);

		//!Set whether to use effects or not
		void setEffects(bool enable); 
};
#endif
