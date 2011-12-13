/*
 *	plot.h - plotting wraper for mathgl
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

#ifndef PLOT_H
#define PLOT_H

#include <cmath>
#include <string>
#include <limits>
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <utility>

#if  defined(WIN32) || defined(WIN64)
	//Help mathgl out a bit: we don't need the GSL on this platform
	#define NO_GSL
#endif
#include <mgl/mgl.h>

//mathgl shadows std::isnan
#undef isnan

#include "basics.h"



enum{
	EDGE_MODE_HOLD,
};


enum
{
	PLOT_TYPE_ONED,
	PLOT_TYPE_TWOD,
	PLOT_TYPE_MIXED, //When multiple plots are visible of different types
	PLOT_TYPE_ENUM_END //not a plot, just end of enum
};


#include "filter.h"


//!Return a human readable string for a given plot type
std::string plotString(unsigned int traceType);

//!Return a human readable string for the plot error mode
std::string plotErrmodeString(unsigned int errMode);

//!Return the plot type given a human readable string
unsigned int plotID(const std::string &plotString);

//!Return the error mode type, given the human readable string
unsigned int plotErrmodeID(const std::string &s);
		
//!Nasty hack class to change mathgl API from named char pallette to rgb specification
class MGLColourFixer
{
	private:
		vector<float> rs,gs,bs;
		static int maxCols;
		mglGraph *graph;
	public:
		void setGraph(mglGraph *g) { graph=g;};
		//Restore the MGL colour strings
		void reset();
		//Return the exact colour, if there is one
		char haveExactColour(float r, float g, float b) const;
		//Get the best colour that is available
		// returns the char to give to mathgl; may be exact,
		// maybe nearest match, depending upon number of colours used
		// and mgl pallette size
		char getNextBestColour(float r, float g, float b);

		unsigned int getMaxColours() const;
};

//!Data class  for holding info about non-overlapping 
// interactive rectilinear "zones" overlaid on plots 
class PlotRegion
{
	public:
		//Axis along which bounds are set. Each entry is unique
		vector<int> boundAxis;
		//Bounding limits for axial bind
		vector<std::pair<float,float> > bounds;

		//Bounding region colour
		float r,g,b;
		//The parent filter, so region can tell who the parent is
		Filter *parentFilter;
		//The ID value for this region, used when interacting with parent filter
		unsigned int id;
		//!Unique ID for region across entire multiplot
		unsigned int uniqueID;
};

//!Base class for data plotting
class PlotBase
{
	public:
		//The type of plot (ie what class is it?)	
		unsigned int plotType;
		
		//!Bounding box for data
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
		//!Pointer to some constant object that generated this plot
		const void *parentObject;

		//!Interactive, or otherwise marked plot regions
		vector<PlotRegion> regions;
		//ID handler for regions
		UniqueIDHandler regionIDHandler;

		//!integer to show which of the n plots that the parent generated
		//that this data is represented by
		unsigned int parentPlotIndex;
		
		//Draw the plot onto a given MGL graph
		virtual void drawPlot(mglGraph *graph, MGLColourFixer &fixer) const=0;

		//!Scan for the data bounds.
		virtual void getBounds(float &xMin,float &xMax,
					float &yMin,float &yMax) const = 0;

		//Retrieve the raw data associated with this plot.
		virtual void getRawData(vector<vector<float> > &f,
				std::vector<std::wstring> &labels) const=0;

		//!Retrieve the ID of the non-overlapping region in X-Y space
		virtual bool getRegionIdAtPosition(float x, float y, unsigned int &id) const=0;
		//!Retrieve a region using its unique ID
		virtual void getRegion(unsigned int id, PlotRegion &r) const=0;

		//!Get the number of regions;
		size_t getNumRegions() const { return regions.size();};
		
		//!Pass the region movement information to the parent filter object
		virtual void moveRegion(unsigned int regionId, unsigned int method, 
							float newX, float newY) const=0;

		//!Obtain limit of motion for a given region movement type
		virtual void moveRegionLimit(unsigned int regionId,
				unsigned int movementType, float &maxX, float &maxY) const=0;

};

//1D Plot with ranges
class Plot1D : public PlotBase
{
	private: 	

		//!Should plot logarithm (+1) of data? Be careful of -ve values.
		bool logarithmic;
		//!Data
		std::vector<float> xValues,yValues,errBars;
		
		void getBounds(float &xMin,float &xMax,float &yMin,float &yMax) const;
		
	public:
		Plot1D();

		//!Set the plot data from a pair and symmetric Y error
		void setData(const vector<std::pair<float,float> > &v);
		void setData(const vector<std::pair<float,float> > &v,const vector<float> &symYErr);
		//!Set the plot data from two vectors and symmetric Y error
		void setData(const vector<float> &vX, const vector<float> &vY);
		void setData(const vector<float> &vX, const vector<float> &vY,
							const vector<float> &symYErr);

		//!Append a region to the plot
		void addRegion(unsigned int parentPlot, unsigned int regionId,
			       		float start, float end,	float r,float g, 
						float b, Filter *parentFilter);
		
		//!Try to move a region from its current position to a new position
		//return the test coord. valid methods are 0 (left extend), 1 (slide), 2 (right extend)
		float moveRegionTest(unsigned int region, unsigned int method, float newPos) const;

		//!Move a region to a new location. MUST call moveRegionTest first.
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

		//!Retrieve a region using its unique ID
		void getRegion(unsigned int id, PlotRegion &r) const;
	
		//!Pass the region movement information to the parent filter object
		void moveRegion(unsigned int regionId, unsigned int method, float newX, float newY) const;

		//Get the region motion limits, to ensure that the selected region does not 
		//overlap after a move operation. Note that the output variables will only
		//be set for the appropriate motion direction. Eg, an X only move will not
		//set limitY.
		void moveRegionLimit(unsigned int regionId,
				unsigned int movementType, float &limitX, float &limitY) const;

		bool wantLogPlot() const { return logarithmic;};
		void setLogarithmic(bool p){logarithmic=p;};
};

//2D (value-value pair) 
class Plot2D : public PlotBase
{
	private: 	
		//!Data
		std::vector<float> xValues,yValues,xErrBars,yErrBars;
		//Retrieve the bounds on the plot
		void getBounds(float &xMin,float &xMax,float &yMin,float &yMax) const;

	public:
		//Draw the plot onto a given MGL graph
		virtual void drawPlot(mglGraph *graph,MGLColourFixer &fixer) const;

		//Retrieve the raw data associated with this plot.
		virtual void getRawData(vector<vector<float> > &f,
				std::vector<std::wstring> &labels) const;

		//!Retrieve the ID of the non-overlapping region in X-Y space
		virtual bool getRegionIdAtPosition(float x, float y, unsigned int &id) const;
		//!Retrieve a region using its unique ID
		virtual void getRegion(unsigned int id, PlotRegion &r) const;
		
		//!Pass the region movement information to the parent filter object
		virtual void moveRegion(unsigned int regionId, unsigned int method, 
							float newX, float newY) const;

		//!Obtain limit of motion for a given region movement type
		virtual void moveRegionLimit(unsigned int regionId,
				unsigned int movementType, float &maxX, float &maxY) const;
	
};

//Wrapper class for containing multiple plots 
class PlotWrapper
{
	protected:
		//!Has the plot changed since we last rendered it?
		bool plotChanged;
		//!Elements of the plot
		std::vector<PlotBase *> plottingData;

		//!which plots were last visible?
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

		//!Swtich to enable or disable drawing of the plot legend
		bool drawLegend;	

	public:
		//!Constructor
		PlotWrapper(){applyUserBounds=false;plotChanged=true;drawLegend=true;};


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
		//Draw the plot onto a given MGL graph
		void drawPlot(mglGraph *graph) const;

		//!Set the X Y and title strings
		void setStrings(unsigned int plotID,
				const char *x, const char *y, const char *t);
		void setStrings(unsigned int plotID,const std::string &x, 
				const std::string &y,const std::string &t);
		//!Set the parent information for a given plot
		void setParentData(unsigned int plotID,
				const void *parentObj, unsigned int plotIndex);
		
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

		//!Retrieve the raw data associated with the selected plots.
		void getRawData(vector<vector<vector<float> > >  &data, std::vector<std::vector<std::wstring> >  &labels) const;
	

		//!obtain the type of a plot, given the plot's uniqueID
		unsigned int plotType(unsigned int plotId) const;

		//Retrieve the types of visible plots
		unsigned int getVisibleType() const;

		//!Obtain limit of motion for a given region movement type
		void moveRegionLimit(unsigned int plotId, unsigned int regionId,
				unsigned int movementType, float &maxX, float &maxY) const;

		//!Pass the region movement information to the parent filter object
		void moveRegion(unsigned int plotID, unsigned int regionId, unsigned int movementType, 
							float newX, float newY) const;
};

#endif
