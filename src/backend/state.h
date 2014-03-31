/*
 *	state.h - user session state handler
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


#include <string>
#include <vector>

#include "gl/cameras.h"
#include "gl/effect.h"


#include "tree.hh"
#include "filtertree.h"

#include "animator.h"

//Unit tests
#ifdef DEBUG
bool runStateTests();
#endif


struct FilterTreeState
{
	//The tree of filters in the current state
	FilterTree filterTree;
	//True for items that can be seen by the user
	tree<bool> visibility;

	//index (BFS search wise) of the selected item
	size_t selectedBFSIndex;
};

enum
{
	STATE_MODIFIED_NONE=0,
	STATE_MODIFIED_VIEW, // the 3D view has chaged
	STATE_MODIFIED_ANCILLARY, //Eg stashes, inactive cameras, and other things that might get saved
	STATE_MODIFIED_DATA // actual data output is latered
};

//The underlying data for any given state in the analysis toolchain
class AnalysisState
{
	private:
		//Items that should be written to file
		// on state save
		//===
		//!Viewing cameras for looking at results
		std::vector<Camera *> savedCameras;

		//!Filter trees that have been designated as inactive, but
		// user would like to have them around for use
		std::vector<std::pair<string,FilterTree> > stashedTrees;

		//!Undo/redo stack for current state
		std::vector<FilterTreeState > undoTrees,redoTrees;

		//Scene modification 3D Effects 
		std::vector<const Effect *> effects;

		//!Filter analysis stree	
		FilterTree activeTree;

		//Background colours
		float rBack,gBack,bBack;
		
		//Viewing mode for the world indication axes
		int worldAxisMode;

		//Camera user has currently activated
		size_t activeCamera;

		//Should the plot legend be enabled
		bool plotLegendEnable;

		//Filter path and ID of plots that need to be enabled at startup 
		vector<pair<string,unsigned int> > enabledStartupPlots;

		//true if system should be using relative paths when
		// saving state
		bool useRelativePathsForSave;
		
		//!Working directory for saving
		std::string workingDir;
		//===

		//true if modification to state has occurred
		mutable int modifyLevel;

		//file to save to
		std::string fileName;
		
		//!Undo filter tree stack 
		std::deque<FilterTree> undoFilterStack,redoFilterStack;
		
	
		//!User-set animation properties
		PropertyAnimator animationState;

		//TODO: Migrte into some state wrapper class with animationState
		//Additional state information for animation
		vector<pair<string,size_t>  > animationPaths;

		bool camNameExists(const std::string &s)  const ;

		//Clear the effect vector
		void clearEffects();
		
		//Clear the camera data vector
		void clearCams();

		void setModifyLevel(int newLevel) { modifyLevel=std::max(newLevel,modifyLevel);}
	public:
		AnalysisState();

		~AnalysisState();

		//Wipe the state clean
		void clear();


		void operator=(const AnalysisState &oth);

		

		//Load an XML representation of the analysis state
		// - returns true on success, false on fail
		// - errStream will have human readable messages in 
		//	the case that there is a failure
		// - set merge to true, if should attempt to merge 
		//	the two states together
		bool load(const char *cpFilename, 
				std::ostream &errStream, 
				bool merge) ;

		//save an XML-ised representation of the analysis sate
		//	- mapping provides the on-disk to local name mapping to use when saving
		// 	- write package says if state should attempt to ensure that output
		// 		state is fully self-contained, and locally referenced
		bool save(const char *cpFilename, std::map<string,string> &fileMapping,
				bool writePackage) const ;


		//Return the current state's filename
		string getFilename() const { return fileName; }
		//Return the current state's filename
		void setFilename(std::string &s) {fileName=s; }
	
		//obtain the world axis display state
		int getWorldAxisMode() const;



		//obtain the scene background colour
		void getBackgroundColour(float &r, float &g, float &b) const;

		//Copy the internal effect vector. 
		//	-Must manually delete each pointer
		void copyEffects(vector<Effect *> &effects) const;

		//Set the background colour for the 
		void setBackgroundColour(float r, float g, float b);

		//set the display mode for the world XYZ axes
		void setWorldAxisMode(unsigned int mode);
	
		// === Cameras ===
		//Set the camera vector, clearing any existing cams
		// note that control of pointers will be taken
		void setCamerasByCopy(vector<Camera *> &c, unsigned int active);


		void setCameraByClone(const Camera *c, unsigned int offset) ;

		//Obtain the ID of the active camera
		size_t getActiveCam() const  { return activeCamera;};

		//Set
		void setActiveCam(size_t offset) {ASSERT(offset < savedCameras.size()); activeCamera=offset; };

		//Remove the  camera at the specified offset
		void removeCam(size_t offset);
		
		const Camera *getCam(size_t offset) const;
		//Obtain a copy of the internal camera vector.
		// - must delete the copy manually.
		void copyCams(vector<Camera *> &cams) const;

		//Obtain a copy of the internal camera vector.
		// note that this reference has limited validity, and may be
		// invalidated if the state is modified
		void copyCamsByRef(vector<const Camera *> &cams) const;

		size_t getNumCams() const { return savedCameras.size();}

		//Add a camera by cloning an existing camera
		void addCamByClone(const Camera *c);

		bool setCamProperty(size_t offset, unsigned int key, const std::string &value);
		//=====

		//Set the effect vector
		void setEffectsByCopy(const vector<const Effect *> &e);

		//Plotting functions
		//=======

		void setPlotLegend(bool enabled) {plotLegendEnable=enabled;}
		void setEnabledPlots(const vector<pair<string,unsigned int> > &enabledPlots) {enabledStartupPlots = enabledPlots;}

		void getEnabledPlots(vector<pair<string,unsigned int> > &enabledPlots) const { enabledPlots=enabledStartupPlots;}

		//Set whether to use relative paths in saved file
		void setUseRelPaths(bool useRel);
		//get whether to use relative paths in saved file
		bool getUseRelPaths() const;

		//Set the working directory to be specified when using relative paths
		void setWorkingDir(const std::string &work);
		//Set the working directory to be specified when using relative paths
		std::string getWorkingDir() const { return workingDir;};

		//Set the active filter tree - note that the tree is a "clone" of the
		// original tree - i.e. the incoming tree is duplicated
		void setFilterTreeByClone(const FilterTree &f);
		
		//Obtain a copy of the internal filter tree
		// - underlying pointers will be different!
		void copyFilterTree(FilterTree &f) const;

		///Set the stashed filters to use internally
		void setStashedTreesByClone(const vector<std::pair<string,FilterTree> > &s);

		//Add an element to the stashed filters
		void addStashedTree( const std::pair<string,FilterTree> &);

		//Retrieve the specified stashed filter
		void copyStashedTree(size_t offset, std::pair<string,FilterTree > &) const;

		//retrieve all stashed filters
		void copyStashedTrees(std::vector<std::pair<std::string,FilterTree> > &stashList) const;

		//Remove the stash at the specified offset
		void eraseStash(size_t offset);

		//Return the number of stash elements
		size_t getStashCount()  const { return stashedTrees.size();}


		//Push the filter tree undo stack
		void pushUndoStack();

		//Pop the filter tree undo stack. If restorePopped is true,
		// then the internal filter tree is updated with the stack tree
		void popUndoStack(bool restorePopped=true);

		//Pop the redo stack, this unconditionally enforces an update of the
		// active internal tree
		void popRedoStack();

		//Obtain the size of the undo stack
		size_t getUndoSize() const { return undoFilterStack.size();};
		//obtain the size of the redo stack
		size_t getRedoSize() const { return redoFilterStack.size();};

		//Clear undo/redo filter tree stacks
		void clearUndoRedoStacks() { undoFilterStack.clear(); redoFilterStack.clear();}
		
		//true if the state has been modified since last load/save.
		int stateModifyLevel() const { return modifyLevel;};

		//Returns true if there is any data in the stash or the active tree
		bool hasStateData() const { return (stashedTrees.size() || activeTree.size());}


		void setAnimationState(const PropertyAnimator &p,const vector<pair<string,size_t> > &animPth) {animationState=p;animationPaths=animPth;}
		
		void getAnimationState( PropertyAnimator &p, vector<pair<string,size_t> > &animPth) const; 

		//TODO: REMOVE ME - needed for viscontrol linkage
		void setStateModified(int state) const { modifyLevel=state;}

};


