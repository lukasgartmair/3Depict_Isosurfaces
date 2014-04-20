/*
 *	plot.cpp - mathgl plot wrapper class
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
#include "plot.h"

#include "common/stringFuncs.h"

#include "common/translation.h"

#ifdef USE_MGL2
	#include <mgl2/canvas_wnd.h>
#endif

//!Plot error bar estimation strings
const char *errModeStrings[] = { 
				NTRANS("None"),
				NTRANS("Moving avg.")
				};

const char *plotModeStrings[]= {
	NTRANS("Lines"),
	NTRANS("Bars"),
	NTRANS("Steps"),
	NTRANS("Stem"),
	NTRANS("Points")
				};

using std::string;
using std::pair;
using std::vector;

//Axis min/max bounding box is disallowed to be exactly zero on any given axis
// perform a little "push off" by this fudge factor
const float AXIS_MIN_TOLERANCE=10*sqrtf(std::numeric_limits<float>::epsilon());

const unsigned int MGL_RESERVED_COLOURS=2;


//Mathgl uses some internal for(float=...) constructions, 
// which are just generally a bad idea, as they often won't terminate
// as the precision is not guaranteed. Try to catch these by detecting this
bool mglFloatTooClose(float a, float b)
{
	//For small numbers an absolute delta can catch
	// too close values
	if(fabs(a-b) < sqrt(std::numeric_limits<float>::epsilon()))
		return true;

	const int FLOAT_ACC_MASK=0xffff0000;
	union FLT_INT
	{
		float f;
		int i;
	};
	FLT_INT u;
	u.f=a;
	//For big numbers, we have to either bit-bash, or something
	u.i&=FLOAT_ACC_MASK;
	a=u.f;

	u.f=b;
	u.i&=FLOAT_ACC_MASK;
	b=u.f;

	if(fabs(a-b) < sqrt(std::numeric_limits<float>::epsilon()))
		return true;

	return false;
}


//Nasty string conversion functions.
std::wstring strToWStr(const std::string& s)
{
	std::wstring temp(s.length(),L' ');
	std::copy(s.begin(), s.end(), temp.begin());
	return temp;
}

std::string wstrToStr(const std::wstring& s)
{
	std::string temp(s.length(), ' ');
	std::copy(s.begin(), s.end(), temp.begin());
	return temp;
}


std::string mglColourCode(float r, float g, float b)
{
	ASSERT(r >=0.0f && g >=0.0f && b >=0.0f)
	ASSERT(r <=255.0f && g <=255.0f && b <=255.0f)
	std::string s;
	genColString((unsigned char)(r*255),
		(unsigned char)(g*255),(unsigned char)(b*255),s);
	s=s.substr(1);

	return string("{x") + uppercase(s) + string("}");
}

//TODO: Refactor these functions to use a common string map
//-----------
string plotString(unsigned int traceType)
{
	ASSERT(traceType< PLOT_TRACE_ENDOFENUM);
	return TRANS(plotModeStrings[traceType]); 
}

unsigned int plotID(const std::string &plotString)
{
	for(unsigned int ui=0;ui<PLOT_TRACE_ENDOFENUM; ui++)
	{
		if(plotString==TRANS(plotModeStrings[ui]))
			return ui;
	}

	ASSERT(false);
}
//-----------

unsigned int plotErrmodeID(const std::string &s)
{
	for(unsigned int ui=0;ui<PLOT_ERROR_ENDOFENUM; ui++)
	{
		if(s ==  errModeStrings[ui])
			return ui;
	}
	ASSERT(false);
}

string plotErrmodeString(unsigned int plotID)
{
	return errModeStrings[plotID];
}


void genErrBars(const std::vector<float> &x, const std::vector<float> &y, 
			std::vector<float> &errBars, const PLOT_ERROR &errMode)
{
	switch(errMode.mode)
	{
		case PLOT_ERROR_NONE:
			return;	
		case PLOT_ERROR_MOVING_AVERAGE:
		{
			ASSERT(errMode.movingAverageNum);
			errBars.resize(y.size());
			for(int ui=0;ui<(int)y.size();ui++)
			{
				float mean;
				mean=0.0f;

				//Compute the local mean
				for(int uj=0;uj<(int)errMode.movingAverageNum;uj++)
				{
					//TODO: Why are we using (int) here?
					int idx;
					idx= std::max(ui+uj-(int)errMode.movingAverageNum/2,0);
					idx=std::min(idx,(int)(y.size()-1));
					ASSERT(idx<(int)y.size());
					mean+=y[idx];
				}

				mean/=(float)errMode.movingAverageNum;

				//Compute the local stddev
				float stddev;
				stddev=0;
				for(int uj=0;uj<(int)errMode.movingAverageNum;uj++)
				{
					int idx;
					idx= std::max(ui+uj-(int)errMode.movingAverageNum/2,0);
					idx=std::min(idx,(int)(y.size()-1));
					stddev+=(y[idx]-mean)*(y[idx]-mean);
				}

				stddev=sqrtf(stddev/(float)errMode.movingAverageNum);
				errBars[ui]=stddev;
			}
			break;
		}
	}

}
//===

PlotRegion::PlotRegion()
{
	accessMode=ACCESS_MODE_ENUM_END;
	parentObject=0;
}

PlotRegion::PlotRegion(size_t updateAccessMode,void *parentObj)
{
	setUpdateMethod(updateAccessMode,parentObj);
}

void PlotRegion::setUpdateMethod(size_t updateAccessMode,void *parentObj)
{
	ASSERT(updateAccessMode< ACCESS_MODE_ENUM_END);
	ASSERT(parentObj);
	
	parentObject=parentObj;
	accessMode=updateAccessMode;
}

const PlotRegion &PlotRegion::operator=(const PlotRegion &oth)
{
	accessMode=oth.accessMode;
	parentObject=oth.parentObject;
	id=oth.id;
	label=oth.label;

	r=oth.r; g=oth.g; b=oth.b;
	bounds=oth.bounds;

	return *this;
}

void PlotRegion::updateParent(size_t regionChangeType,
		const vector<float> &newPositions, bool updateSelf) 
{
	ASSERT(newPositions.size() >= bounds.size());
	ASSERT(parentObject);

	//Update the parent object, using the requested access mode for the parent
	switch(accessMode)
	{
		case ACCESS_MODE_FILTER:
		{
			Filter *f;
			f = (Filter*)parentObject;
			f->setPropFromRegion(regionChangeType,id,newPositions[0]);

			break;
		}
		case ACCESS_MODE_RANGEFILE:
		{
			RangeFile *rng= (RangeFile *) parentObject;
			switch(regionChangeType)
			{
				case REGION_MOVE_EXTEND_XMINUS:
				{
					//Disallow zero sided region
					if(rng->getRangeByRef(id).second ==newPositions[0])
						break;
					
					rng->getRangeByRef(id).first=newPositions[0];
					break;
				}
				case REGION_MOVE_EXTEND_XPLUS:
				{
					//Disallow zero sided region
					if(rng->getRangeByRef(id).first==newPositions[0])
						break;

					rng->getRangeByRef(id).second=newPositions[0];
					break;
				}
				//move the centroid to the new absolute position
				case REGION_MOVE_TRANSLATE_X:
				{
					float delta;
					pair<float,float> &bound=rng->getRangeByRef(id);

					delta = (bound.second-bound.first)/2;
					bound.first=newPositions[0]-delta;
					bound.second=newPositions[0]+delta;
					break;
				}
				default:
					ASSERT(false);
			}
			
			//Check for inversion
			if(rng->getRangeByRef(id).first  > rng->getRangeByRef(id).second)
			{
				std::swap(rng->getRangeByRef(id).first,
					rng->getRangeByRef(id).second);
			}
			
			break;
		}
		default:
			ASSERT(false);
	}


	//Update own region data
	if(updateSelf)
	{
		switch(regionChangeType)
		{
			case REGION_MOVE_EXTEND_XMINUS:
			{
				bounds[0].first = newPositions[0];
				break;
			}
			case REGION_MOVE_EXTEND_XPLUS:
			{
				bounds[0].second= newPositions[0];
				break;
			}
			//move the centroid to the new absolute position
			case REGION_MOVE_TRANSLATE_X:
			{
				float delta;
				delta = (bounds[0].second-bounds[0].first)/2;
				bounds[0].first=newPositions[0]-delta;
				bounds[0].second=newPositions[0]+delta;
				break;
			}
			default:
				ASSERT(false);
		}

		//Check for inversion
		if(bounds[0].first  > bounds[0].second)
		{
			std::swap(bounds[0].first, bounds[0].second);
		}

	}
}

std::string PlotRegion::getName() const
{
	return label; 
}

PlotWrapper::PlotWrapper()
{
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(plotModeStrings) == PLOT_TRACE_ENDOFENUM);

	applyUserBounds=false;
	plotChanged=true;
	drawLegend=true;
	interactionLocked=false;
	highlightRegionOverlaps=false;
}

PlotWrapper::~PlotWrapper()
{
	for(size_t ui=0;ui<plottingData.size();ui++)
		delete plottingData[ui];
}

const PlotWrapper &PlotWrapper::operator=(const PlotWrapper &p)
{
	plotChanged=p.plotChanged;

	plottingData.resize(p.plottingData.size());

	for(size_t ui=0; ui<p.plottingData.size(); ui++)
	{
		PlotBase *pb;
		pb=((p.plottingData[ui])->clone());

		plottingData[ui] =pb; 
	
	}

	plotChanged=p.plotChanged;
	lastVisiblePlots=p.lastVisiblePlots;
	
	plotIDHandler=p.plotIDHandler;
	
	applyUserBounds=p.applyUserBounds;
	xUserMin=p.xUserMin;
	yUserMin=p.yUserMin;
	xUserMax=p.xUserMax;
	yUserMax=p.yUserMax;

	highlightRegionOverlaps=p.highlightRegionOverlaps;
	drawLegend=p.drawLegend;
	interactionLocked=p.interactionLocked;

	return *this;
}

std::wstring PlotWrapper::getTitle(size_t plotId) const
{
	unsigned int plotPos=plotIDHandler.getPos(plotId);
	return plottingData[plotPos]->title;
}

void PlotWrapper::getPlotIDs(vector<unsigned int> &ids) const
{
	plotIDHandler.getIds(ids);
}

//TODO: Refactor. Should not make the assumption that parentObject is a filter,
// in here. Could be anything.
size_t PlotWrapper::getParentType(size_t plotId) const
{
	unsigned int plotPos=plotIDHandler.getPos(plotId);
	return ((const Filter*)plottingData[plotPos]->parentObject)->getType();
	
}

unsigned int PlotWrapper::addPlot(PlotBase *p)
{
	plottingData.push_back(p);

	//assign a unique identifier to this plot, by which it can be referenced
	unsigned int uniqueID = plotIDHandler.genId(plottingData.size()-1);
	plotChanged=true;
	return uniqueID;
}

void PlotWrapper::clear(bool preserveVisiblity)
{

	//Do our best to preserve visibility of
	//plots. 
	lastVisiblePlots.clear();
	if(preserveVisiblity)
	{
		//Remember which plots were visible, who owned them, and their index
		for(unsigned int ui=0;ui<plottingData.size(); ui++)
		{
			if(plottingData[ui]->visible && plottingData[ui]->parentObject)
			{
				lastVisiblePlots.push_back(std::make_pair(plottingData[ui]->parentObject,
								plottingData[ui]->parentPlotIndex));
			}
		}
	}
	else
		applyUserBounds=false;


	//Free the plotting data pointers
	for(size_t ui=0;ui<plottingData.size();ui++)
		delete plottingData[ui];

	plottingData.clear();
	plotIDHandler.clear();
	plotChanged=true;
}

void PlotWrapper::setStrings(unsigned int plotID, const std::string &x, 
				const std::string &y, const std::string &t)
{
	unsigned int plotPos=plotIDHandler.getPos(plotID);
	plottingData[plotPos]->xLabel = strToWStr(x);
	plottingData[plotPos]->yLabel = strToWStr(y);
	
	plottingData[plotPos]->title = strToWStr(t);
	plotChanged=true;
}

void PlotWrapper::setParentData(unsigned int plotID, const void *parentObj, unsigned int idx)
{
	unsigned int plotPos=plotIDHandler.getPos(plotID);
	plottingData[plotPos]->parentObject= parentObj;
	plottingData[plotPos]->parentPlotIndex=idx;
	
	plotChanged=true;
}

void PlotWrapper::setTraceStyle(unsigned int plotUniqueID,unsigned int mode)
{

	ASSERT(mode<PLOT_TRACE_ENDOFENUM);
	plottingData[plotIDHandler.getPos(plotUniqueID)]->traceType=mode;
	plotChanged=true;
}

void PlotWrapper::setColours(unsigned int plotUniqueID, float rN,float gN,float bN) 
{
	unsigned int plotPos=plotIDHandler.getPos(plotUniqueID);
	plottingData[plotPos]->r=rN;
	plottingData[plotPos]->g=gN;
	plottingData[plotPos]->b=bN;
	plotChanged=true;
}

void PlotWrapper::setBounds(float xMin, float xMax,
			float yMin,float yMax)
{
	ASSERT(!interactionLocked);

	ASSERT(xMin<xMax);
	ASSERT(yMin<=yMax);
	xUserMin=xMin;
	yUserMin=yMin;
	xUserMax=xMax;
	yUserMax=yMax;



	applyUserBounds=true;
	plotChanged=true;
}

void PlotWrapper::disableUserAxisBounds(bool xBound)
{
	ASSERT(!interactionLocked);
	float xMin,xMax,yMin,yMax;
	scanBounds(xMin,xMax,yMin,yMax);

	if(xBound)
	{
		xUserMin=xMin;
		xUserMax=xMax;
	}
	else
	{
		yUserMin=std::max(0.0f,yMin);
		yUserMax=yMax;
	}

	//Check to see if we have zoomed all the bounds out anyway
	if(fabs(xUserMin -xMin)<=std::numeric_limits<float>::epsilon() &&
		fabs(yUserMin -yMin)<=std::numeric_limits<float>::epsilon())
	{
		applyUserBounds=false;
	}

	plotChanged=true;
}

void PlotWrapper::getBounds(float &xMin, float &xMax,
			float &yMin,float &yMax) const
{
	if(applyUserBounds)
	{
		xMin=xUserMin;
		yMin=yUserMin;
		xMax=xUserMax;
		yMax=yUserMax;
	}
	else
		scanBounds(xMin,xMax,yMin,yMax);

	ASSERT(xMin <xMax && yMin <=yMax);
}

void PlotWrapper::scanBounds(float &xMin,float &xMax,float &yMin,float &yMax) const
{
	//We are going to have to scan for max/min bounds
	//from the shown plots 
	xMin=std::numeric_limits<float>::max();
	xMax=-std::numeric_limits<float>::max();
	yMin=std::numeric_limits<float>::max();
	yMax=-std::numeric_limits<float>::max();

	for(unsigned int uj=0;uj<plottingData.size(); uj++)
	{
		//only consider the bounding boxes from visible plots
		if(!plottingData[uj]->visible)
			continue;

		//Expand our bounding box to encompass that of this visible plot
		float tmpXMin,tmpXMax,tmpYMin,tmpYMax;
		plottingData[uj]->getBounds(tmpXMin,tmpXMax,tmpYMin,tmpYMax);

		xMin=std::min(xMin,tmpXMin);
		xMax=std::max(xMax,tmpXMax);
		yMin=std::min(yMin,tmpYMin);
		yMax=std::max(yMax,tmpYMax);

	}

	ASSERT(xMin < xMax && yMin <=yMax);
}

void PlotWrapper::bestEffortRestoreVisibility()
{
	//Try to match up the last visible plots to the
	//new plots. Use index and owner as guiding data

	for(unsigned int uj=0;uj<plottingData.size(); uj++)
		plottingData[uj]->visible=false;
	
	for(unsigned int ui=0; ui<lastVisiblePlots.size();ui++)
	{
		for(unsigned int uj=0;uj<plottingData.size(); uj++)
		{
			if(plottingData[uj]->parentObject == lastVisiblePlots[ui].first
				&& plottingData[uj]->parentPlotIndex == lastVisiblePlots[ui].second)
			{
				plottingData[uj]->visible=true;
				break;
			}
		
		}
	}

	lastVisiblePlots.clear();
	plotChanged=true;
}

void PlotWrapper::getRawData(vector<vector<vector<float> > > &data,
				std::vector<std::vector<std::wstring> > &labels) const
{
	if(plottingData.empty())
		return;

	//Determine if we have multiple types of plot.
	//if so, we cannot really return the raw data for this
	//in a meaningful fashion
	switch(getVisibleType())
	{
		case PLOT_TYPE_ONED:
		{
			//Try to retrieve the raw data from the visible plots
			for(unsigned int ui=0;ui<plottingData.size();ui++)
			{
				if(plottingData[ui]->visible)
				{
					vector<vector<float> > thisDat,dummy;
					vector<std::wstring> thisLabel;
					plottingData[ui]->getRawData(thisDat,thisLabel);
					
					//Data title size should hopefully be the same
					//as the label size
					ASSERT(thisLabel.size() == thisDat.size());

					if(thisDat.size())
					{
						data.push_back(dummy);
						data.back().swap(thisDat);
						
						labels.push_back(thisLabel);	
					}
				}
			}
			break;
		}
		case PLOT_TYPE_ENUM_END:
			return;
		default:
			ASSERT(false);
	}
}

unsigned int PlotWrapper::getVisibleType() const
{
	unsigned int visibleType=PLOT_TYPE_ENUM_END;
	for(unsigned int ui=0;ui<plottingData.size() ; ui++)
	{
		if(plottingData[ui]->visible &&
			plottingData[ui]->plotType!= visibleType)
		{
			if(visibleType == PLOT_TYPE_ENUM_END)
			{
				visibleType=plottingData[ui]->plotType;
				continue;
			}
			else
			{
				visibleType=PLOT_TYPE_MIXED;
				break;
			}
		}
	}

	return visibleType;
}

void PlotWrapper::getVisibleIDs(vector<unsigned int> &visiblePlotIDs ) const
{

	for(size_t ui=0;ui<plottingData.size() ; ui++)
	{
		if(isPlotVisible(ui))
			visiblePlotIDs.push_back(ui);
	}

}



void PlotWrapper::findRegionLimit(unsigned int plotId, unsigned int regionId,
				unsigned int movementType, float &maxX, float &maxY) const
{
	unsigned int plotPos=plotIDHandler.getPos(plotId);
	plottingData[plotPos]->regionGroup.findRegionLimit(regionId,movementType,maxX,maxY);

}

void PlotWrapper::drawPlot(mglGraph *gr, bool &haveUsedLog) const
{
	unsigned int visType = getVisibleType();
	if(visType == PLOT_TYPE_ENUM_END || 
		visType == PLOT_TYPE_MIXED)
	{
		//We don't handle the drawing case well here, so assert this.
		// calling code should check this case and ensure that it draws something
		// meaningful
		ASSERT(false);
		return;
	}

	//Un-fudger for mathgl plots

	bool haveMultiTitles=false;
	float minX,maxX,minY,maxY;
	minX=std::numeric_limits<float>::max();
	maxX=-std::numeric_limits<float>::max();
	minY=std::numeric_limits<float>::max();
	maxY=-std::numeric_limits<float>::max();

	//Compute the bounding box in data coordinates	
	std::wstring xLabel,yLabel,plotTitle;

	for(unsigned int ui=0;ui<plottingData.size(); ui++)
	{
		if(plottingData[ui]->visible)
		{
			float tmpMinX,tmpMinY,tmpMaxX,tmpMaxY;
			plottingData[ui]->getBounds(
				tmpMinX,tmpMaxX,tmpMinY,tmpMaxY);

			minX=std::min(minX,tmpMinX);
			maxX=std::max(maxX,tmpMaxX);
			minY=std::min(minY,tmpMinY);
			maxY=std::max(maxY,tmpMaxY);


			if(!xLabel.size())
				xLabel=plottingData[ui]->xLabel;
			else
			{

				if(xLabel!=plottingData[ui]->xLabel)
					xLabel=stlStrToStlWStr(TRANS("Multiple data types"));
			}
			if(!yLabel.size())
				yLabel=plottingData[ui]->yLabel;
			else
			{

				if(yLabel!=plottingData[ui]->yLabel)
					yLabel=stlStrToStlWStr(TRANS("Multiple data types"));
			}
			if(!haveMultiTitles && !plotTitle.size())
				plotTitle=plottingData[ui]->title;
			else
			{

				if(plotTitle!=plottingData[ui]->title)
				{
					plotTitle=L"";//L prefix means wide char
					haveMultiTitles=true;
				}
			}


		}
	}



	string sX,sY;
	sX.assign(xLabel.begin(),xLabel.end()); //unicode conversion
	sY.assign(yLabel.begin(),yLabel.end()); //unicode conversion
	
	string sT;
	sT.assign(plotTitle.begin(), plotTitle.end()); //unicode conversion
	gr->Title(sT.c_str());
	

	haveUsedLog=false;
	mglPoint min,max;
	switch(visType)
	{
		case PLOT_TYPE_ONED:
		{
			//OneD connected value line plot f(x)
			bool useLogPlot=false;
			bool notLog=false;

		
			for(unsigned int ui=0;ui<plottingData.size(); ui++)
			{
				if(plottingData[ui]->visible)
				{
					if(((Plot1D*)plottingData[ui])->wantLogPlot()) 
						useLogPlot=true;
					else
						notLog=true;
				}
			}

			haveUsedLog|=useLogPlot;
		
			//work out the bounding box for the plot,
			//and where the axis should cross
			mglPoint axisCross;
			if(applyUserBounds)
			{
				ASSERT(yUserMax >=yUserMin);
				ASSERT(xUserMax >=xUserMin);

				max.x =xUserMax;
				max.y=yUserMax;
				
				min.x =xUserMin;
				min.y =yUserMin;

				axisCross.x=min.x;
				axisCross.y=min.y;
				
			}
			else
			{
				//Retrieve the bounds of the data that is in the plot

				min.x=minX;
				min.y=minY;
				max.x=maxX;
				max.y=maxY;

				axisCross.x = minX;
				axisCross.y=min.y;
			}
			
			//Allow logarithmic mode, as needed.
			// mathgl does not like a zero coordinate for plotting
			if(min.y == 0 && useLogPlot)
			{
				min.y=0.1;
				if(axisCross.y < min.y)
					axisCross.y=min.y;
			}
			//"Push" bounds around to prevent min == max
			// This is a hack to prevent mathgl from inf. looping
			//---
			if(mglFloatTooClose(min.x , max.x))
			{
				min.x-=5;
				max.x+=5;
			}

			if(mglFloatTooClose(min.y , max.y))
				max.y+=1;
			//------
			

			//tell mathgl about the bounding box	
#ifdef USE_MGL2
			gr->SetRanges(min,max);
			
			gr->SetOrigin(axisCross);
#endif

			WARN((fabs(min.x-max.x) > sqrt(std::numeric_limits<float>::epsilon())), 
					"WARNING: Mgl limits (X) too Close! Due to limitiations in MGL, This may inf. loop!");
			WARN((fabs(min.y-max.y) > sqrt(std::numeric_limits<float>::epsilon())), 
					"WARNING: Mgl limits (Y) too Close! Due to limitiations in MGL, This may inf. loop!");

			if(useLogPlot)
				gr->SetFunc("","lg(y)");
			else
				gr->SetFunc("","");
#ifdef USE_MGL2
			gr->Axis();

			mglCanvas *canvas = dynamic_cast<mglCanvas *>(gr->Self());
			canvas->AdjustTicks("x");
			canvas->SetTickTempl('x',"%g"); //Set the tick type
			canvas->Axis("xy"); //Build an X-Y crossing axis
#else
			gr->Axis(min,max,axisCross);
			gr->AdjustTicks("x");
			gr->SetXTT("%g"); //Set the tick type
			gr->Axis("xy"); //Build an X-Y crossing axis
#endif
			//---

			//Loop through the plots, drawing them as needed
			for(unsigned int ui=0;ui<plottingData.size();ui++)
			{
				Plot1D *curPlot;
				curPlot=(Plot1D*)plottingData[ui];

				//If a plot is not visible, it cannot own a region
				//nor have a legend in this run.
				if(!curPlot->visible)
					continue;

				curPlot->drawRegions(gr,min,max);
				curPlot->drawPlot(gr);
				
				if(drawLegend)
				{
					//Fake an mgl colour code
					std::string mglColStr;
					mglColStr= mglColourCode(curPlot->r,curPlot->g,curPlot->b);
					gr->AddLegend(curPlot->title.c_str(),mglColStr.c_str());
				}
			}

			//Prevent mathgl from dropping lines that straddle the plot bound.
			gr->SetCut(false);
			
			//if we have to draw overlapping regions, do so
			if(highlightRegionOverlaps)
			{
				vector<pair<size_t,size_t> > overlapId;
				vector<pair<float,float> > overlapXCoords;

				string colourCode=mglColourCode(1.0f,0.0f,0.0f);
				getRegionOverlaps(overlapId,overlapXCoords);

				float rMinY,rMaxY;
				const float ABOVE_AXIS_CONST = 0.1;
				rMinY = max.y + (max.y-min.y)*(ABOVE_AXIS_CONST-0.025);
				rMaxY = max.y + (max.y-min.y)*(ABOVE_AXIS_CONST+0.025);

				for(size_t ui=0; ui<overlapXCoords.size();ui++)
				{
					float rMinX, rMaxX; 
					
					rMinX=overlapXCoords[ui].first;
					rMaxX=overlapXCoords[ui].second;


					//Make sure we don't leave the plot boundary
					rMinX=std::max(rMinX,(float)min.x);
					rMaxX=std::min(rMaxX,(float)max.x);

					//If the region is of negligble size, don't bother drawing it
					if(fabs(rMinX -rMaxX)
						< sqrt(std::numeric_limits<float>::epsilon()))
						continue;


#ifdef USE_MGL2
					gr->FaceZ(mglPoint(rMinX,rMinY,-1),rMaxX-rMinX,rMaxY-rMinY,
							colourCode.c_str());
#else
					gr->FaceZ(rMinX,rMinY,-1,rMaxX-rMinX,rMaxY-rMinY,
							colourCode.c_str());
#endif
				}

			}

			break;
		}
		default:
			ASSERT(false);
	}


	gr->Label('x',sX.c_str());
	gr->Label('y',sY.c_str(),0);

	if(haveMultiTitles)
	{
#ifdef USE_MGL2
		if(drawLegend)
			gr->Legend();
#else
		gr->SetLegendBox(false);
		//I have NO idea what this size value is about,
		//I just kept changing the number until it looked nice.
		//the library default is -0.85 (why negative??)
		const float LEGEND_SIZE=-1.1f;

		//Legend at top right (0x3), in default font "rL" at specified size
		if(drawLegend)
			gr->Legend(0x3,"rL",LEGEND_SIZE);
#endif
	}
	
	overlays.draw(gr,min,max,haveUsedLog);
}

void PlotWrapper::hideAll()
{
	for(unsigned int ui=0;ui<plottingData.size(); ui++)
		plottingData[ui]->visible=false;
	plotChanged=true;
}

void PlotWrapper::setVisible(unsigned int uniqueID, bool setVis)
{
	unsigned int plotPos = plotIDHandler.getPos(uniqueID);

	plottingData[plotPos]->visible=setVis;
	plotChanged=true;
}

void PlotWrapper::getRegions(vector<pair<size_t,vector<PlotRegion> > > &regions, bool visibleOnly) const
{
	vector<unsigned int> ids;
	getPlotIDs(ids);
	regions.resize(ids.size());

	for(size_t ui=0;ui<ids.size();ui++)
	{
		PlotBase *b;
		b=plottingData[ids[ui]];
		if(b->visible || !visibleOnly)
			regions[ui] = make_pair(ids[ui],b->regionGroup.regions);
	}
}

bool PlotWrapper::getRegionIdAtPosition(float x, float y, unsigned int &pId, unsigned int &rId) const
{
	for(size_t ui=0;ui<plottingData.size(); ui++)
	{
		//Regions can only be active for visible plots
		if(!plottingData[ui]->visible)
			continue;

		if(plottingData[ui]->regionGroup.getRegionIdAtPosition(x,y,rId))
		{
			pId=ui;
			return true;
		}
	}

	return false;
}


void PlotWrapper::getRegionOverlaps(vector<pair<size_t,size_t> > &ids,
					vector< pair<float,float> > &coords) const
{
	ids.clear();
	coords.clear();

	for(size_t uk=0;uk<plottingData.size(); uk++)
	{
		RegionGroup *r;
		r=&(plottingData[uk]->regionGroup);

		r->getOverlaps(ids,coords);
	}
}

unsigned int PlotWrapper::getNumVisible() const
{
	unsigned int num=0;
	for(unsigned int ui=0;ui<plottingData.size();ui++)
	{
		if(plottingData[ui]->visible)
			num++;
	}
	
	
	return num;
}

bool PlotWrapper::isPlotVisible(unsigned int plotID) const
{
	return plottingData[plotIDHandler.getPos(plotID)]->visible;
}

void PlotWrapper::getRegion(unsigned int plotId, unsigned int regionId, PlotRegion &region) const
{
	plottingData[plotIDHandler.getPos(plotId)]->regionGroup.getRegion(regionId,region);
}

unsigned int PlotWrapper::plotType(unsigned int plotId) const
{
	return plottingData[plotIDHandler.getPos(plotId)]->plotType;
}


void PlotWrapper::moveRegion(unsigned int plotID, unsigned int regionId, bool regionSelfUpdate,
		unsigned int movementType, float newX, float newY) const
{
	plottingData[plotIDHandler.getPos(plotID)]->regionGroup.moveRegion(regionId,
						movementType,regionSelfUpdate,newX,newY);
}


void PlotWrapper::switchOutRegionParent(std::map<const RangeFileFilter *, RangeFile> &switchMap)
{

	for(size_t ui=0;ui<plottingData.size();ui++)
	{
		PlotBase *pb;
		pb=plottingData[ui];
		
		RegionGroup *rg;
		rg = &(pb->regionGroup);
		for(size_t uj=0; uj<rg->regions.size();uj++)
		{
			//Obtain the parent filter f rthis region, then re-map it
			// to the rangefile
			Filter *parentFilt = rg->regions[uj].getParentAsFilter();

			if(parentFilt->getType() != FILTER_TYPE_RANGEFILE)
				continue;

			RangeFileFilter *rngFilt = (RangeFileFilter *)parentFilt;
			ASSERT(switchMap.find(rngFilt) != switchMap.end());

			//Set the update method to use the new rangefile
			rg->regions[uj].setUpdateMethod(PlotRegion::ACCESS_MODE_RANGEFILE,
						&(switchMap[rngFilt]));
		}
	}
}

void PlotWrapper::overrideLastVisible(vector< pair<const void *,unsigned int>  > &overridden)
{

	lastVisiblePlots=overridden;	

}

//-----------


Plot1D::Plot1D()
{
	//Set the default plot properties
	visible=(true);
	traceType=PLOT_TRACE_LINES;
	plotType=PLOT_TYPE_ONED;
	xLabel=(strToWStr(""));
	yLabel=(strToWStr(""));
	title=(strToWStr(""));
	r=(0);g=(0);b=(1);
}


PlotBase *Plot1D::clone() const
{
	Plot1D *p = new Plot1D;

	p->logarithmic=logarithmic;
	p->xValues=xValues;
	p->yValues=yValues;
	p->errBars=errBars;

	//TODO Move to base
	p->plotType=plotType;
	p->minX=minX;
	p->maxX=maxX;
	p->minY=minY;
	p->maxY=maxY;
	p->r=r;
	p->g=g;
	p->b=b;
	p->visible=visible;
	p->traceType=traceType;
	p->xLabel=xLabel;
	p->yLabel=yLabel;
	p->title=title;
	p->titleAsRawDataLabel=titleAsRawDataLabel;
	p->parentObject=parentObject;
	p->regionGroup=regionGroup;
	p->parentPlotIndex=parentPlotIndex;

	return p;
}

void Plot1D::setData(const vector<float> &vX, const vector<float> &vY, 
		const vector<float> &vErr)
{

	ASSERT(vX.size() == vY.size());
	ASSERT(vErr.size() == vY.size() || !vErr.size());


	//Fill up vectors with data
	xValues.resize(vX.size());
	std::copy(vX.begin(),vX.end(),xValues.begin());
	yValues.resize(vY.size());
	std::copy(vY.begin(),vY.end(),yValues.begin());
	
	errBars.resize(vErr.size());
	std::copy(vErr.begin(),vErr.end(),errBars.begin());

	//Compute minima and maxima of plot data, and keep a copy of it
	float maxThis=-std::numeric_limits<float>::max();
	float minThis=std::numeric_limits<float>::max();
	for(unsigned int ui=0;ui<vX.size();ui++)
	{
		minThis=std::min(minThis,vX[ui]);
		maxThis=std::max(maxThis,vX[ui]);
	}

	minX=minThis;
	maxX=maxThis;

	if(maxX - minX < AXIS_MIN_TOLERANCE)
	{
		minX-=AXIS_MIN_TOLERANCE;
		maxX+=AXIS_MIN_TOLERANCE;
	}

	
	maxThis=-std::numeric_limits<float>::max();
	minThis=std::numeric_limits<float>::max();
	for(unsigned int ui=0;ui<vY.size();ui++)
	{
		minThis=std::min(minThis,vY[ui]);
		maxThis=std::max(maxThis,vY[ui]);
	}
	minY=minThis;
	maxY=maxThis;

	if(maxY - minY < AXIS_MIN_TOLERANCE)
	{
		minY-=AXIS_MIN_TOLERANCE;
		maxY+=AXIS_MIN_TOLERANCE;
	}
}


void Plot1D::setData(const vector<std::pair<float,float> > &v)
{
	vector<float> dummyVar;

	setData(v,dummyVar);

}

void Plot1D::setData(const vector<std::pair<float,float> > &v,const vector<float> &vErr) 
{
	//Fill up vectors with data
	xValues.resize(v.size());
	yValues.resize(v.size());
	for(unsigned int ui=0;ui<v.size();ui++)
	{
		xValues[ui]=v[ui].first;
		yValues[ui]=v[ui].second;
	}

	errBars.resize(vErr.size());
	std::copy(vErr.begin(),vErr.end(),errBars.begin());

	

	//Compute minima and maxima of plot data, and keep a copy of it
	float maxThis=-std::numeric_limits<float>::max();
	float minThis=std::numeric_limits<float>::max();

	// --------- X Values ---
	for(unsigned int ui=0;ui<v.size();ui++)
	{
		minThis=std::min(minThis,v[ui].first);
		maxThis=std::max(maxThis,v[ui].first);
	}
	minX=minThis;
	maxX=maxThis;
	//------------


	// Y values, taking into account any error bars
	//------------------
	minThis=std::numeric_limits<float>::max();
	maxThis=-std::numeric_limits<float>::max();
	if(vErr.size())
	{
		ASSERT(vErr.size() == v.size());
		for(unsigned int ui=0;ui<v.size();ui++)
		{
			minThis=std::min(minThis,v[ui].second-vErr[ui]);
			maxThis=std::max(maxThis,v[ui].second+vErr[ui]);
		}
	}
	else
	{
		for(unsigned int ui=0;ui<v.size();ui++)
		{
			minThis=std::min(minThis,v[ui].second);
			maxThis=std::max(maxThis,v[ui].second);
		}

	}
	minY=minThis;
	maxY=maxThis; 
	//------------------
}

void Plot1D::getBounds(float &xMin,float &xMax,float &yMin,float &yMax) const
{
	xMin=minX;
	xMax=maxX;
	yMin=minY;
	yMax=maxY;

	ASSERT(yMin <=yMax);
}

bool Plot1D::empty() const
{
	ASSERT(xValues.size() == yValues.size());
	return xValues.empty();
}

void Plot1D::drawPlot(mglGraph *gr) const
{
	bool showErrs;

	mglData xDat,yDat,eDat;

	ASSERT(visible);
	

	//Make a copy of the data we need to use
	float *bufferX,*bufferY,*bufferErr;
	bufferX = new float[xValues.size()];
	bufferY = new float[yValues.size()];

	showErrs=errBars.size();
	if(showErrs)
		bufferErr = new float[errBars.size()];

	//Pre-process the data, before handing to mathgl
	//--
	for(unsigned int uj=0;uj<xValues.size(); uj++)
	{
		bufferX[uj] = xValues[uj];
		bufferY[uj] = yValues[uj];

	}
	if(showErrs)
	{
		for(unsigned int uj=0;uj<errBars.size(); uj++)
			bufferErr[uj] = errBars[uj];
	}
	//--
	
	//Mathgl needs to know where to put the error bars.	
	ASSERT(!showErrs  || errBars.size() ==xValues.size());
	
	//Initialise the mathgl data
	//--
	xDat.Set(bufferX,xValues.size());
	yDat.Set(bufferY,yValues.size());

	if(showErrs)
		eDat.Set(bufferErr,errBars.size());
	//--
	
	
	//Obtain a colour code to use for the plot, based upon
	// the actual colour we wish to use
	string colourCode;
	colourCode=mglColourCode(r,g,b);
	//---


	//Plot the appropriate form	
	switch(traceType)
	{
		case PLOT_TRACE_LINES:
			//Unfortunately, when using line plots, mathgl moves the data points to the plot boundary,
			//rather than linear interpolating them back along their paths. I have emailed the author.
			//for now, we shall have to put up with missing lines :( Absolute worst case, I may have to draw them myself.
			gr->SetCut(true);
		
			gr->Plot(xDat,yDat,colourCode.c_str());
			if(showErrs)
				gr->Error(xDat,yDat,eDat,colourCode.c_str());
			gr->SetCut(false);
			break;
		case PLOT_TRACE_BARS:
			gr->Bars(xDat,yDat,colourCode.c_str());
			break;
		case PLOT_TRACE_STEPS:
			//Same problem as for line plot. 
			gr->SetCut(true);
			gr->Step(xDat,yDat,colourCode.c_str());
			gr->SetCut(false);
			break;
		case PLOT_TRACE_STEM:
			gr->Stem(xDat,yDat,colourCode.c_str());
			break;

		case PLOT_TRACE_POINTS:
		{
			std::string s;
			s = colourCode;
			//Mathgl uses strings to manipulate line styles
			s+=" "; 
				//space means "no line"
			s+="x"; //x shaped point markers

			gr->SetCut(true);
				
			gr->Plot(xDat,yDat,s.c_str());
			if(showErrs)
				gr->Error(xDat,yDat,eDat,s.c_str());
			gr->SetCut(false);
			break;
		}
		default:
			ASSERT(false);
			break;
	}

	delete[]  bufferX;
	delete[]  bufferY;
	if(showErrs)
		delete[]  bufferErr;

			
	
}

void Plot1D::getRawData(std::vector<std::vector< float> > &rawData,
				std::vector<std::wstring> &labels) const
{

	vector<float> tmp,dummy;

	tmp.resize(xValues.size());
	std::copy(xValues.begin(),xValues.end(),tmp.begin());
	rawData.push_back(dummy);
	rawData.back().swap(tmp);

	tmp.resize(yValues.size());
	std::copy(yValues.begin(),yValues.end(),tmp.begin());
	rawData.push_back(dummy);
	rawData.back().swap(tmp);

	labels.push_back(xLabel);
	if(titleAsRawDataLabel)
		labels.push_back(title);
	else
		labels.push_back(yLabel);
	
	
	if(errBars.size())
	{
		tmp.resize(errBars.size());
		std::copy(errBars.begin(),errBars.end(),tmp.begin());
		
		rawData.push_back(dummy);
		rawData.back().swap(tmp);
		labels.push_back(stlStrToStlWStr(TRANS("error")));
	}
}


void Plot1D::clear( bool preserveVisiblity)
{
	regionGroup.clear();
}

void Plot1D::drawRegions(mglGraph *gr,
		const mglPoint &min,const mglPoint &max) const
{
	//Mathgl palette colour name
	string colourCode;

	for(unsigned int uj=0;uj<regionGroup.regions.size();uj++)
	{
		//Compute region bounds, such that it will not exceed the axis
		float rMinX, rMaxX, rMinY,rMaxY;
		rMinY = min.y;
		rMaxY = max.y;
		rMinX = std::max((float)min.x,regionGroup.regions[uj].bounds[0].first);
		rMaxX = std::min((float)max.x,regionGroup.regions[uj].bounds[0].second);
		
		//Prevent drawing inverted regionGroup.regions
		if(rMaxX > rMinX && rMaxY > rMinY)
		{
			colourCode = mglColourCode(regionGroup.regions[uj].r,
						regionGroup.regions[uj].g,
						regionGroup.regions[uj].b);
#ifdef USE_MGL2
			gr->FaceZ(mglPoint(rMinX,rMinY,-1),rMaxX-rMinX,rMaxY-rMinY,
					colourCode.c_str());
#else
			gr->FaceZ(rMinX,rMinY,-1,rMaxX-rMinX,rMaxY-rMinY,
					colourCode.c_str());
#endif
					
		}
	}
}

//--

bool RegionGroup::getRegionIdAtPosition(float x, float y, unsigned int &id) const
{
	for(unsigned int ui=0;ui<regions.size();ui++)
	{
		if(regions[ui].bounds[0].first < x &&
				regions[ui].bounds[0].second > x )
		{
			id=ui;
			return true;
		}
	}


	return false;
}

void RegionGroup::getRegion(unsigned int offset, PlotRegion &r) const
{
	r = regions[offset];
}


void RegionGroup::addRegion(unsigned int regionID,const std::string &name, float start, float end, 
			float rNew, float gNew, float bNew, Filter *parentFilter)
{
	ASSERT(start <end);
	ASSERT( rNew>=0.0 && rNew <= 1.0);
	ASSERT( gNew>=0.0 && gNew <= 1.0);
	ASSERT( bNew>=0.0 && bNew <= 1.0);

	PlotRegion region(PlotRegion::ACCESS_MODE_FILTER,parentFilter);
	//1D plots only have one bounding direction
	region.bounds.push_back(std::make_pair(start,end));
	//Set the ID for the  region
	region.id = regionID;
	region.label=name;
#ifdef DEBUG
	//Ensure ID value is unique per parent
	for(size_t ui=0;ui<regions.size();ui++)
	{
		if(regions[ui].getParentAsFilter()== parentFilter)
		{
			ASSERT(regionID !=regions[ui].id);
		}
	}
#endif

	region.r=rNew;
	region.g=gNew;
	region.b=bNew;

	regions.push_back(region);
}

void RegionGroup::findRegionLimit(unsigned int offset, 
			unsigned int method, float &newPosX, float &newPosY)  const
{

	ASSERT(offset<regions.size());

	//Check that moving this range will not cause any overlaps with 
	//other regions
	float mean;
	mean=(regions[offset].bounds[0].first + regions[offset].bounds[0].second)/2.0f;

	switch(method)
	{
		//Left extend
		case REGION_MOVE_EXTEND_XMINUS:
		{
			//Check that the upper bound does not intersect any RHS of 
			//region bounds
			for(unsigned int ui=0; ui<regions.size(); ui++)
			{
				if((regions[ui].bounds[0].second < mean && ui !=offset) )
						newPosX=std::max(newPosX,regions[ui].bounds[0].second);
			}
			//Dont allow past self right
			newPosX=std::min(newPosX,regions[offset].bounds[0].second);
			break;
		}
		//shift
		case REGION_MOVE_TRANSLATE_X:
		{
			//Check that the upper bound does not intersect any RHS or LHS of 
			//region bounds
			if(newPosX > mean) 
				
			{
				//Disallow hitting other bounds
				for(unsigned int ui=0; ui<regions.size(); ui++)
				{
					if((regions[ui].bounds[0].first > mean && ui != offset) )
						newPosX=std::min(newPosX,regions[ui].bounds[0].first);
				}
			}
			else
			{
				//Disallow hitting other bounds
				for(unsigned int ui=0; ui<regions.size(); ui++)
				{
					if((regions[ui].bounds[0].second < mean && ui != offset))
						newPosX=std::max(newPosX,regions[ui].bounds[0].second);
				}
			}
			break;
		}
		//Right extend
		case REGION_MOVE_EXTEND_XPLUS:
		{
			//Disallow hitting other bounds

			for(unsigned int ui=0; ui<regions.size(); ui++)
			{
				if((regions[ui].bounds[0].second > mean && ui != offset))
					newPosX=std::min(newPosX,regions[ui].bounds[0].first);
			}
			//Dont allow past self left
			newPosX=std::max(newPosX,regions[offset].bounds[0].first);
			break;
		}
		default:
			ASSERT(false);
	}

}


void RegionGroup::moveRegion(unsigned int offset, unsigned int method, bool selfUpdate,
						float newPosX,float newPosY) 
{
	//TODO:  Change function signature to handle vector directly,
	// rather than repackaging it?
	vector<float> v;

	v.push_back(newPosX);
	v.push_back(newPosY);
	
	regions[offset].updateParent(method,v,selfUpdate);

	haveOverlapCache=false;
}

void RegionGroup::getOverlaps(vector<pair<size_t,size_t> > &ids,
				vector< pair<float,float> > &coords) const
{

	//Rebuild the cache as needed
	if(!haveOverlapCache)
	{
		overlapIdCache.clear();
		overlapCoordsCache.clear();
		//Loop through upper triangular region of cross, checking for overlap
		for(unsigned int ui=0;ui<regions.size();ui++)
		{
			float minA,maxA;
			minA=regions[ui].bounds[0].first;
			maxA=regions[ui].bounds[0].second;
			for(unsigned int uj=ui+1;uj<regions.size();uj++)
			{
				float minB,maxB;
				minB=regions[uj].bounds[0].first;
				maxB=regions[uj].bounds[0].second;
				//If the coordinates overlap, then record their ID
				// and their coordinates, in plot units
				if(rangesOverlap(minA,maxA,minB,maxB))
				{
					overlapIdCache.push_back(make_pair(ui,uj));
					overlapCoordsCache.push_back(
						make_pair(std::max(minA,minB),std::min(maxA,maxB)) );
				}
			}
		}

		haveOverlapCache=true;
	}

	ids.reserve(ids.size() + overlapIdCache.size());
	for(unsigned int ui=0;ui<overlapIdCache.size();ui++)
		ids.push_back(overlapIdCache[ui]);

	coords.reserve(coords.size() + overlapCoordsCache.size());
	for(unsigned int ui=0;ui<overlapCoordsCache.size();ui++)
		coords.push_back(overlapCoordsCache[ui]);

}


void PlotWrapper::setRegionGroup(size_t plotId,RegionGroup &r)
{
	size_t offset=plotIDHandler.getPos(plotId);
	//Overwrite the plot's region group
	plottingData[offset]->regionGroup=r;
}


void PlotOverlays::draw(mglGraph *gr,
		const mglPoint &boundMin, const mglPoint &boundMax,bool logMode ) const
{

	if(!isEnabled)
		return;

	string colourCode;

	//Draw the overlays in black
	colourCode = mglColourCode(0.0,0.0,0.0);
	
	for(size_t ui=0;ui<overlayData.size();ui++)
	{
		if(!overlayData[ui].enabled)
			continue;

		vector<float> bufX,bufY;
		float maxV;
		maxV=-std::numeric_limits<float>::max();

		bufX.resize(overlayData[ui].coordData.size());
		bufY.resize(overlayData[ui].coordData.size());
		for(size_t uj=0;uj<overlayData[ui].coordData.size();uj++)	
		{
			bufX[uj]=overlayData[ui].coordData[uj].first;
			bufY[uj]=overlayData[ui].coordData[uj].second;
			
			maxV=std::max(maxV,bufY[uj]);
		}

		//Rescale to plot size
		for(size_t uj=0;uj<overlayData[ui].coordData.size();uj++)
		{
			if(logMode)
			{
				//Compute log10(probability*maximum) = 
				//	log10(probability) + log10(maximum)
				// maximum = 10^boundMax.y

				bufY[uj]=log10(bufY[uj]) +boundMax.y*0.95;
				bufY[uj]=std::max(bufY[uj],0.0f);
			}
			else
			{
				bufY[uj]*=boundMax.y/maxV*0.95;
			}
		}

		//TODO: Deprecate me. Upstream now allows single stems
		//Draw stems. can't use stem plot due to mathgl bug whereby single stems
		// will not be drawn
		for(size_t uj=0;uj<overlayData[ui].coordData.size();uj++)
		{
			if(bufX[uj]> boundMin.x && bufX[uj]< boundMax.x && 
					boundMin.y < bufY[uj])
			{
				gr->Line (mglPoint(bufX[uj],std::max(0.0f,(float)boundMin.y)),
					mglPoint(bufX[uj],bufY[uj]),colourCode.c_str(),100);

				//Print labels near to the text
				const float STANDOFF_FACTOR=1.05;
#ifdef USE_MGL2
				gr->Puts(mglPoint(bufX[uj],bufY[uj]*STANDOFF_FACTOR),
					overlayData[ui].title.c_str());
#else
				//Font size in mathgl uses negative values to set a relative font size
				gr->Text(mglPoint(bufX[uj],bufY[uj]*STANDOFF_FACTOR),
					overlayData[ui].title.c_str(),"",-0.6);

#endif
			}
		}
	}
}

