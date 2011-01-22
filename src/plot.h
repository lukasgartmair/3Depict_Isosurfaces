/*
 *	plot.h - plotting wraper for mathgl
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



#include "basics.h"

enum
{
  PLOT_TYPE_LINES=0,
  PLOT_TYPE_BARS,
  PLOT_TYPE_STEPS,
  PLOT_TYPE_STEM,
  PLOT_TYPE_ENDOFENUM,
};

enum
{
	PLOT_ERROR_NONE,
	PLOT_ERROR_MOVING_AVERAGE,
	PLOT_ERROR_ENDOFENUM
};

enum{
	EDGE_MODE_HOLD,
};

enum{
	REGION_LEFT_EXTEND=1,
	REGION_MOVE,
	REGION_RIGHT_EXTEND
};

//!Structure to handle error bar drawing in plot
struct PLOT_ERROR
{
	//!Plot data estimator mode
	unsigned int mode;
	//!Number of data points for moving average
	unsigned int movingAverageNum;
	//!Edge mode
	unsigned int edgeMode;
};

#include "filter.h"


//!Return a human readable string for a given plot type
std::string plotString(unsigned int plotType);

//!Return a human readable string for the plot error mode
std::string plotErrmodeString(unsigned int errMode);

//!Return the plot type given a human readable string
unsigned int plotID(const std::string &plotString);

//!Return the error mode type, given the human readable string
unsigned int plotErrmodeID(const std::string &s);

class PlotData
{
	public:
		//!Bounding box for data
		float minX,maxX,minY,maxY;
		//!Data
		std::vector<float> xValues,yValues,errBars;
		//!Colour of trace
		float r,g,b;
		//!Is trace visible?
		bool visible;
		//!Should plot logarithm (+1) of data? Be careful of -ve values.
		bool logarithmic;
		//!Type of plot (lines, bars, sticks, etc)
		unsigned int plotType;
		//!xaxis label
		std::wstring xLabel;
		//!y axis label
		std::wstring yLabel;
		//!Plot title
		std::wstring title;
		//!Pointer to some constant object that generated this plot
		const void *parentObject;
		//!integer to show which of the n plots that the parent generated
		//that this data is represented by
		unsigned int parentPlotIndex;

};

class PlotRegion
{
	public:
		std::pair<float,float> bounds;
		float r,g,b;
		//The ID value for this region, used when interacting with parent filter
		unsigned int id;
		//!Unique ID for region across entire multiplot
		unsigned int uniqueID;
		//ID value for owner plot in multiplot to recognise this region
		unsigned int ownerPlot;
		//The parent filter, so region can tell who the parent is
		Filter *parentFilter;
};

class Multiplot
{
	private:
		UniqueIDHandler plotIDHandler;
		UniqueIDHandler regionIDHandler;
		//!Has the plot changed since we last rendered it?
		bool plotChanged;

		void scanBounds(float &xMin,float &xMax,float &yMin,float &yMax) const;
	protected:
		//!Elements of the plot
		std::vector<PlotData> plottingData;

		//!which plots were last visible?
		std::vector<std::pair< const void *, unsigned int> > lastVisiblePlots;

		//!Regions on plot (coloured drawing objects)
		std::vector<PlotRegion> regions;

		//!Use user-specified bounding values?
		bool applyUserBounds;
		//!User mininum bounds
		float xUserMin,yUserMin;
		//!User maximum  bounds
		float xUserMax,yUserMax;

		//!Swtich to enable or disable drawing of the plot legend
		bool drawLegend;	

		//!Nasty hack to get nearest mathgl named colour from a given RGB
		//R,G,B in [0,1]
		char getNearestMathglColour(float r, float g, float b) const;
	public:
		//!Constructor
		Multiplot(){applyUserBounds=false;plotChanged=true;drawLegend=true;};

		//Is the plot logarithmic when displayed?
		bool isLogarithmic() const;

		//!Has the contents of the plot changed since the last call to resetChange?
		bool hasChanged() const { return plotChanged;};
	
		void resetChange() { plotChanged=false;}

		//!Erase all the plot data
		void clear(bool preserveVisibility=false);
		
		//!Hide all plots (sets all visibility to false)
		void hideAll();

		//!Set the visibilty of a plot, based upon its uniqueID
		void setVisible(unsigned int uniqueID, bool isVisible=true);

		//Draw the plot onto a given MGL graph
		void drawPlot(mglGraph *graph) const;

		//!Add a plot to the multiplot, with the given XY data. 
		unsigned int addPlot(const std::vector<float> &vX, const std::vector<float> &vY,
					const PLOT_ERROR &errMode, bool logarithmic=false);
		unsigned int addPlot(const std::vector<std::pair<float, float> > &data, const PLOT_ERROR &p,
				bool logarithmic=false);
		//!Re-set the plot datya for a given plot
		void setPlotData(unsigned int plotUniqueID,
				const std::vector<float> &vX, const std::vector<float> &vY);
		//!Set the X Y and title strings
		void setStrings(unsigned int plotID,
				const char *x, const char *y, const char *t);
		void setStrings(unsigned int plotID,const std::string &x, 
				const std::string &y,const std::string &t);
		//!Set the parent information for a given plot
		void setParentData(unsigned int plotID,
				const void *parentObj, unsigned int plotIndex);
		//!Set the X Y and title strings using std::strings

		//!Set the plotting mode.
		void setType(unsigned int plotID,unsigned int mode);
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

		//!Append a region to the plot
		void addRegion(unsigned int parentPlot, unsigned int regionId,
			       		float start, float end,	float r,float g, 
						float b, Filter *parentFilter);

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

		//!Retrieve the raw data associated with the currently visible plots. 
		//note that this is the FULL data not the zoomed data for the current user bounds
		void getRawData(std::vector<std::vector<std::vector<float> > > &rawData,
				std::vector<std::vector<std::wstring> > &labels) const;

		//!Set whether to enable the legend or not
		void setLegendVisible(bool vis) { drawLegend=vis;plotChanged=true;};

		//!Get the currently visible regions
		void getRegions(std::vector<PlotRegion> &copyRegions) const;

		//!Get a particular region
		PlotRegion getRegion(unsigned int region) const { return regions[region];};

		//!Try to move a region from its current position to a new position
		//return the test coord. valid methods are 0 (left extend), 1 (slide), 2 (right extend)
		float moveRegionTest(unsigned int region, unsigned int method, float newPos) const;

		//!Move a region to a new location. MUST call moveRegionTest first.
		void moveRegion(unsigned int region, unsigned int method, float newPos);
};

#endif
