/*
 *	plot.h - plotting wraper for mathgl
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

#ifndef PLOT_H
#define PLOT_H


#include "backend/filter.h"

#include <map>


#if  defined(WIN32) || defined(WIN64)
	//Help mathgl out a bit: we don't need the GSL on this platform
	#define NO_GSL
#endif

//Use config header to determine if we need to enable mgl2 support
#include "../config.h"
#ifdef USE_MGL2
#include <mgl2/mgl.h>
#else
#include <mgl/mgl.h>
#endif

//mathgl shadows std::isnan
#undef isnan


enum
{
	PLOT_TYPE_ONED,
	PLOT_TYPE_MIXED,
	PLOT_TYPE_ENUM_END //not a plot, just end of enum
};




//!Return a human readable string for a given plot type
std::string plotString(unsigned int traceType);

//!Return a human readable string for the plot error mode
std::string plotErrmodeString(unsigned int errMode);

//!Return the plot type given a human readable string
unsigned int plotID(const std::string &plotString);

//!Return the error mode type, given the human readable string
unsigned int plotErrmodeID(const std::string &s);
		
//!Nasty hack class to change mathgl API from named char palette to rgb specification
class MGLColourFixer
{
	private:
		vector<float> rs,gs,bs;
		static int maxCols;
	public:
		//Restore the MGL colour strings
		void reset();
		//Return the exact colour, if there is one
		char haveExactColour(float r, float g, float b) const;
		//Get the best colour that is available
		// returns the char to give to mathgl; may be exact,
		// maybe nearest match, depending upon number of colours used
		// and mgl palette size
		char getNextBestColour(float r, float g, float b);

		static unsigned int getMaxColours();
};


//!Data class  for holding info about non-overlapping 
// interactive rectilinear "zones" overlaid on plots 
// update method must be set.
class PlotRegion
{
	private:
		size_t accessMode;

		void *parentObject;


	public:
		enum
		{
			ACCESS_MODE_FILTER,
			ACCESS_MODE_RANGEFILE,
			ACCESS_MODE_ENUM_END
		};
		
		//Bounding limits for axial bind
		vector<std::pair<float,float> > bounds;

		//Bounding region colour
		float r,g,b;

		//The ID value for this region, used when interacting with parent object
		unsigned int id;
		
		PlotRegion();

		PlotRegion(size_t updateMethod,void *parentObject);

		const PlotRegion &operator=(const PlotRegion &r);

		//Alter the update method
		void setUpdateMethod(size_t updateMethod, void *parentObject);

		//Update the parent object using the curretn update method
		void updateParent(size_t method, const vector<float> &newBounds, bool updateSelf=true);

		//Retrieve the parent as a filter object - must be in ACCESS_MODE_FILTER
		Filter *getParentAsFilter() const { ASSERT(accessMode==ACCESS_MODE_FILTER); return (Filter*)parentObject;};

		RangeFile *getParentAsRangeFile() const { ASSERT(accessMode==ACCESS_MODE_RANGEFILE); return (RangeFile*)parentObject;};

};

//Handles an array of regions, for drawing and editing of the array

class RegionGroup
{
	private:
		//cache for region overlaps, to reduce need to search
		mutable vector<pair<size_t,size_t> > overlapIdCache;
		mutable vector<pair<float,float> > overlapCoordsCache;

		mutable bool haveOverlapCache;
	public:
		RegionGroup() { haveOverlapCache=false;};
		//!Interactive, or otherwise marked plot regions
		vector<PlotRegion> regions;


		void clear() {regions.clear(); };
		
		//!Append a region to the plot
		void addRegion(unsigned int regionId, float start, float end,	
				float r,float g, float b, Filter *parentFilter);
		//!Retrieve the ID of the non-overlapping region in X-Y space
		bool getRegionIdAtPosition(float x, float y, unsigned int &id) const;
		//!Retrieve a region using its ID
		void getRegion(unsigned int id, PlotRegion &r) const;

		//!Get the number of regions;
		size_t getNumRegions() const { return regions.size();};
		
		//!Pass the region movement information to the parent filter object
		// newX and newY should be absolute positions.
		void moveRegion(unsigned int regionId, unsigned int method, bool selfUpdate,
							float newX, float newY);
		//!Obtain limit of motion for a given region movement type
		void findRegionLimit(unsigned int regionId,
				unsigned int movementType, float &maxX, float &maxY) const;


		void getOverlaps(vector<pair<size_t,size_t> > &ids,
				vector< pair<float,float> > &coords) const;
};

struct  OVERLAY_DATA
{
	//Coordinate and amplitude
	vector<pair<float, float> > coordData;
	//title for all of te specified overlay data
	string title;
	//If the overlay is enabled or not
	bool enabled;
};

class PlotOverlays
{
	private:
		bool isEnabled;
		// List of the overlays that can be shown on the given plot
		vector<OVERLAY_DATA>  overlayData;
	public:
		PlotOverlays() : isEnabled(true) {}
		//Add a new overlay to the plot
		void add(const OVERLAY_DATA &overlay) {overlayData.push_back(overlay);}
		//Draw the overlay on the current plot
		void draw(mglGraph *g,MGLColourFixer &fixer, 
			const mglPoint &boundMin, const mglPoint &boundMax,bool logMode) const;
		//Enable the specified overlay
		void setEnabled(size_t offset,bool isEnabled) 
			{ASSERT(offset <overlayData.size()); overlayData[offset].enabled=isEnabled;}

		void setEnabled(bool doEnable)  {isEnabled=doEnable;}
		bool enabled() const { return isEnabled;}
		
		void clear() {overlayData.clear();}
		void erase(size_t item) {ASSERT(item < overlayData.size()); overlayData.erase(overlayData.begin() + item);;}

		const vector<OVERLAY_DATA> &getOverlays() const { return overlayData;};

};

//!Base class for data plotting
class PlotBase
{
	public:
		PlotBase(){};
		virtual ~PlotBase(){};

		virtual PlotBase *clone() const = 0;

		//The type of plot (ie what class is it?)	
		unsigned int plotType;
		
		//!Bounding box for plot -  may exceed plot data area
		float minX,maxX,minY,maxY;

		//!Colour of trace
		float r,g,b;
		//!Is trace visible?
		bool visible;
		//!Type of plot (lines, bars, sticks, etc)
		unsigned int traceType;
		//!xaxis label
		std::wstring xLabel;
		//!y axis label
		std::wstring yLabel;
		//!Plot title
		std::wstring title;

		//!Use the plot title for Y data label when exporting raw data
		// (true), or use the yLabel
		bool titleAsRawDataLabel;

		//!Pointer to some constant object that generated this plot
		const void *parentObject;

		//!integer to show which of the n plots that the parent generated
		//that this data is represented by
		unsigned int parentPlotIndex;


		RegionGroup regionGroup;

	
		//True if the plot has data
		virtual bool empty() const=0;

		//Draw the plot onto a given MGL graph
		virtual void drawPlot(mglGraph *graph, MGLColourFixer &fixer) const=0;

		//!Scan for the data bounds.
		virtual void getBounds(float &xMin,float &xMax,
					float &yMin,float &yMax) const = 0;

		//Retrieve the raw data associated with this plot.
		virtual void getRawData(vector<vector<float> > &f,
				std::vector<std::wstring> &labels) const=0;


};

//!1D Function f(x) Plot with ranges
// data must be a pure Function.
class Plot1D : public PlotBase
{
	private: 	

		//!Should plot logarithm (+1) of data? Be careful of -ve values.
		bool logarithmic;
		//!Data
		std::vector<float> xValues,yValues,errBars;
		
	public:
		Plot1D();
		virtual bool empty() const;
		virtual PlotBase *clone() const;
			
		void getBounds(float &xMin,float &xMax,float &yMin,float &yMax) const;

		//!Set the plot data from a pair and symmetric Y error
		void setData(const vector<std::pair<float,float> > &v);
		void setData(const vector<std::pair<float,float> > &v,const vector<float> &symYErr);
		//!Set the plot data from two vectors and symmetric Y error
		void setData(const vector<float> &vX, const vector<float> &vY);
		void setData(const vector<float> &vX, const vector<float> &vY,
							const vector<float> &symYErr);

		
		//!Move a region to a new location. 
		void moveRegion(unsigned int region, unsigned int method, float newPos);

		void clear(bool preserveVisibility);
		
		
		//Draw the plot onto a given MGL graph
		virtual void drawPlot(mglGraph *graph,MGLColourFixer &fixer) const;

		//Draw the associated regions		
		void drawRegions(mglGraph *graph, MGLColourFixer &fixer,
				const mglPoint &min, const mglPoint &max) const;


		//!Retrieve the raw data associated with the currently visible plots. 
		//note that this is the FULL data not the zoomed data for the current user bounds
		void getRawData(std::vector<std::vector<float> >  &rawData,
				std::vector<std::wstring> &labels) const;
		
		//!Retrieve the ID of the non-overlapping region in X-Y space
		bool getRegionIdAtPosition(float x, float y, unsigned int &id) const;

		//!Retrieve a region using its ID
		void getRegion(unsigned int id, PlotRegion &r) const;
	
		//!Pass the region movement information to the parent filter object
		void moveRegion(unsigned int regionId, unsigned int method, float newX, float newY) const;

		//Get the region motion limits, to ensure that the selected region does not 
		//overlap after a move operation. Note that the output variables will only
		//be set for the appropriate motion direction. Eg, an X only move will not
		//set limitY.
		void findRegionLimit(unsigned int regionId,
				unsigned int movementType, float &limitX, float &limitY) const;

		bool wantLogPlot() const { return logarithmic;};
		void setLogarithmic(bool p){logarithmic=p;};
};

//Wrapper class for containing multiple plots 
class PlotWrapper
{
	protected:
		//!Has the plot changed since we last rendered it?
		bool plotChanged;
		//!Elements of the plot
		std::vector<PlotBase *> plottingData;

		//! Data regarding plots were visible previously
		// first pair entry is the parent object pointer. second is the parent plot index
		std::vector<std::pair< const void *, unsigned int> > lastVisiblePlots;
	
		//Position independant ID handling for the 
		//plots inserted into the vector
		UniqueIDHandler plotIDHandler;


		//!Use user-specified bounding values?
		bool applyUserBounds;
		//!User mininum bounds
		float xUserMin,yUserMin;
		//!User maximum  bounds
		float xUserMax,yUserMax;

		//!Switch to enable or disable drawing of the plot legend
		bool drawLegend;	

		//!is user interaction with the plot supposed to be locked?
		// - is used to ensure that when updating plot, UI control
		//  will be hinted to take correct action
		bool interactionLocked;

		//!Do we want to highlight positions where regions overlap?
		bool highlightRegionOverlaps;

	public:
		//"stick" type overlays for marking amplitudes on top of the plot
		PlotOverlays overlays;
		
		//!Constructor
		PlotWrapper();

		//!Destructor must delete target plots
		~PlotWrapper();

		const PlotWrapper &operator=(const PlotWrapper &oth);

		//Return  the number of plots
		size_t numPlots() const { return plottingData.size();}

		//Retrieve the IDs for the stored plots
		void getPlotIDs(vector<unsigned int> &ids) const ;

		//Retrieve the title of the plot
		std::wstring getTitle(size_t plotId) const;


		void setEnableHighlightOverlap(bool enable=true) { highlightRegionOverlaps=enable;}

		//get the type of parent filter that generated the plot
		size_t getParentType(size_t plotId) const;

		//True if user is not allowed to interact with plot
		bool isInteractionLocked() const { return interactionLocked;};

		//Disallow interaction with plot (lock=true), or enable interaction (lock=false)
		void lockInteraction(bool lock=true) {interactionLocked=lock;};

		//!Has the contents of the plot changed since the last call to resetChange?
		bool hasChanged() const { return plotChanged;};
	
		void resetChange() { plotChanged=false;}

		//!Erase all the plot data
		void clear(bool preserveVisibility=false);
		
		//!Hide all plots (sets all visibility to false)
		void hideAll();

		//!Set the visibilty of a plot, based upon its uniqueID
		void setVisible(unsigned int uniqueID, bool isVisible=true);

		//!Get the bounds for the plot
		void scanBounds(float &xMin,float &xMax,float &yMin,float &yMax) const;
		//Draw the plot onto a given MGL graph. Only one type (1D,2D etc) of plot may be visible
		void drawPlot(mglGraph *graph) const;

		//!Set the X Y and title strings
		void setStrings(unsigned int plotID,
				const char *x, const char *y, const char *t);
		void setStrings(unsigned int plotID,const std::string &x, 
				const std::string &y,const std::string &t);
		//!Set the parent information for a given plot
		void setParentData(unsigned int plotID,
				const void *parentObj, unsigned int plotIndex);
	
		//!Get the parent object fo rthis plot
		const void *getParent(unsigned int plotID) { ASSERT(plotID < plottingData.size()); return plottingData[plotID]->parentObject;}

		//!Set the plotting mode.
		void setTraceStyle(unsigned int plotID,unsigned int mode);

		//!Set the plot colours
		void setColours(unsigned int plotID, float rN,float gN,float bN);
		//!Set the bounds on the plot 
		void setBounds(float xMin, float xMax,
					float yMin,float yMax);
		//!Get the bounds for the plot
		void getBounds(float &xMin, float &xMax,
					float &yMin,float &yMax) const;
		//!Automatically use the data limits to compute bounds
		void resetBounds();


		//!Get the number of visible plots
		unsigned int getNumVisible() const;
		
		//!Get the number of visible plots
		unsigned int getNumTotal() const { return plottingData.size(); };

		//!Returns true if plot is visible, based upon its uniqueID.
		bool isPlotVisible(unsigned int plotID) const;

		//!Disable user bounds
		void disableUserBounds(){plotChanged=true;applyUserBounds=false;};

		//!Disable user axis bounds along one axis only
		void disableUserAxisBounds(bool xAxis);

		//!Do our best to restore the visibility of the plot to what it was 
		//before the last clear based upon the plot data owner information.
		//Note that this will erase the last stored visibility data when complete.
		void bestEffortRestoreVisibility();


		//!Set whether to enable the legend or not
		void setLegendVisible(bool vis) { drawLegend=vis;plotChanged=true;};

		//!Add a plot to the list of available plots. Control of the pointer becomes
		//transferred to this class, so do *NOT* delete it after calling this function
		unsigned int addPlot(PlotBase *plot);

		//!Get the ID (return value) and the contents of the plot region at the given position. 
		// Returns false if no region can be found, and true if valid region found
		bool getRegionIdAtPosition(float px, float py, 
			unsigned int &plotId, unsigned int &regionID ) const;


		//Return the region data for the given regionID/plotID combo
		void getRegion(unsigned int plotId, unsigned int regionId, PlotRegion &r) const;

		//Get all of the (id, regions) for plots. Bool allows for only the plots that are visible to be obtained
		void getRegions(vector<pair<size_t,vector<PlotRegion> > > &regions, bool visibleOnly=true) const;
		
		//Return the ID and coordinates of any overlapping regions
		// - this only returns overlaps for individual plots - not between plots
		void getRegionOverlaps(std::vector<pair<size_t,size_t> > &ids,
							std::vector< pair<float,float> > &coords) const;

		//!Retrieve the raw data associated with the selected plots.
		void getRawData(vector<vector<vector<float> > >  &data, std::vector<std::vector<std::wstring> >  &labels) const;
	

		//!obtain the type of a plot, given the plot's uniqueID
		unsigned int plotType(unsigned int plotId) const;

		//Retrieve the types of visible plots
		unsigned int getVisibleType() const;

		//!Obtain limit of motion for a given region movement type
		void findRegionLimit(unsigned int plotId, unsigned int regionId,
				unsigned int movementType, float &maxX, float &maxY) const;

		//!Pass the region movement information to the parent filter object
		void moveRegion(unsigned int plotID, unsigned int regionId, bool regionSelfUp,
					unsigned int movementType, float newX, float newY) const;

		//Change the regions to modify the given rangefiles, for each of the 
		// rangefile filters in the map
		void switchOutRegionParent(std::map<const RangeFileFilter *, RangeFile> &switchMap);

		void setRegionGroup(size_t plotId, RegionGroup &r);
};

#endif
