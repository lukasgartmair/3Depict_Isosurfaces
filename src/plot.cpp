/*
 *	plot.cpp - mathgl plot wrapper class
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


#include "basics.h"
#include "plot.h"


#include "translation.h"

//Apple are special....
#ifdef __APPLE__
	#include <math.h>
#endif

#include <iostream>
#include <algorithm>
#ifndef ASSERT
	#define ASSERT(f)
#endif

//Uncomment me if using MGL >=1_10 to 
//enable plot cutting and colouring
#define MGL_GTE_1_10

//!Plot error bar estimation strings
const char *errModeStrings[] = { 
				NTRANS("None"),
				NTRANS("Moving avg.")
				};


using std::string;
using std::pair;
using std::vector;

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

string plotString(unsigned int traceType)
{
	switch(traceType)
	{
		case PLOT_TRACE_LINES:
			return string(TRANS("Lines"));
		case PLOT_TRACE_BARS:
			return string(TRANS("Bars"));
		case PLOT_TRACE_STEPS:
			return string(TRANS("Steps"));
		case PLOT_TRACE_STEM:
			return string(TRANS("Stem"));
		default:
			ASSERT(false);
			return string("bug:(plotString)");
	}
}

unsigned int plotID(const std::string &plotString)
{
	if(plotString == TRANS("Lines"))
		return PLOT_TRACE_LINES;
	if(plotString == TRANS("Bars"))
		return PLOT_TRACE_BARS;
	if(plotString == TRANS("Steps"))
		return PLOT_TRACE_STEPS;
	if(plotString == TRANS("Stem"))
		return PLOT_TRACE_STEM;
	ASSERT(false);
}

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

char getNearestMathglColour(float r, float g, float b) 
{
	//FIXME: nasty hack
	//This is a nasty hack, but I hvae been completely
	//unable to work out any reasonable method of
	//setting the colour of a plot to any specific value without
	//patching the MGL library.
	//Even then, I currently only have a patch for a single plot.
	//so multiple plots cannot have unique colours.
	
	//To complete the nasty hack, let us compute the minimum distance
	//from the available named colours "rgbwk..." to the desired colour
	float distanceSqr=std::numeric_limits<float>::max();
	float distanceTmp;

	//mathgl's "named" colours
	const char *mglColourString="wkrgbcymhRGBCYMHWlenuqpLENUQP";
	const unsigned int numColours= strlen(mglColourString);

	ASSERT(numColours);	
	mglColor c;
	char val;

	for(unsigned int ui=0;ui<numColours;ui++)
	{
		c.Set(mglColourString[ui]);

		if(r > 0.5)
		{
			//use alternate weighting for closer colour perception in "non-red" half of colour cube 
			distanceTmp= 2.0*(c.r - r )*(c.r - r ) +4.0*(c.g - g )*(c.g - g ) 
								+ 3.0*(c.b - b )*(c.b - b );
		}
		else
		{
			//3,4,2 weighted euclidean distance. weights allow for closer human perception
			distanceTmp= 3.0*(c.r - r )*(c.r - r ) +4.0*(c.g - g )*(c.g - g ) 
								+ 2.0*(c.b - b )*(c.b - b );
		}

		
		if(distanceSqr > distanceTmp)
		{
			distanceSqr = (distanceTmp);
			val = mglColourString[ui];
		}

	}


	return val;
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
			for(int ui=0;ui<y.size();ui++)
			{
				float mean;
				mean=0.0f;

				//Compute the local mean
				for(int uj=0;uj<errMode.movingAverageNum;uj++)
				{
					int idx;
					idx= std::max((int)ui+(int)uj-(int)errMode.movingAverageNum/2,0);
					idx=std::min(idx,(int)(y.size()-1));
					ASSERT(idx<y.size());
					mean+=y[idx];
				}

				mean/=(float)errMode.movingAverageNum;

				//Compute the local stddev
				float stddev;
				stddev=0;
				for(int uj=0;uj<errMode.movingAverageNum;uj++)
				{
					int idx;
					idx= std::max((int)ui+(int)uj-(int)errMode.movingAverageNum/2,0);
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


unsigned int PlotWrapper::addPlot(PlotBase *p)
{
	plottingData.push_back(p);

#ifdef DEBUG
	p=0; //zero out poiner, we are in command now.
#endif

	//assign a unique identifier to this plot, by which it can be referenced
	unsigned int uniqueID = plotIDHandler.genId(plottingData.size()-1);
	plotChanged=true;
	return uniqueID;
}

void PlotWrapper::clear(bool preserveVisiblity)
{

	//Do our best to preserve visiblity of
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
	ASSERT(xMin<xMax);
	ASSERT(yMin<=yMax);
	xUserMin=xMin;
	yUserMin=std::max(0.0f,yMin);
	xUserMax=xMax;
	yUserMax=yMax;

	applyUserBounds=true;
	plotChanged=true;
}

void PlotWrapper::disableUserAxisBounds(bool xBound)
{
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
	//OK, we are going to have to scan for max/min
	//from the 
	xMin=std::numeric_limits<float>::max();
	xMax=-std::numeric_limits<float>::max();
	yMin=std::numeric_limits<float>::max();
	yMax=-std::numeric_limits<float>::max();

	for(unsigned int uj=0;uj<plottingData.size(); uj++)
	{
		if(!plottingData[uj]->visible)
			continue;

		float tmpXMin,tmpXMax,tmpYMin,tmpYMax;
		plottingData[uj]->getBounds(tmpXMin,tmpXMax,tmpYMin,tmpYMax);

		xMin=std::min(xMin,tmpXMin);
		xMax=std::max(xMax,tmpXMax);
		yMin=std::min(yMin,tmpYMin);
		yMax=std::max(yMax,tmpYMax);

	}
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
	//Try to retrieve the raw data from the visible plots
	for(unsigned int ui=0;ui<plottingData.size();ui++)
	{
		//FIXME: This needs logic to ensure that mixed
		// data types are not selected by the end user
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

}

bool PlotWrapper::areVisiblePlotsMismatched() const
{
	unsigned int type;
	type=(unsigned int)-1;
	for(size_t ui=0;ui<plottingData.size(); ui++)
	{
		if( plottingData[ui]->plotType !=type)
		{
			if(type == (unsigned int)-1)
				type=plottingData[ui]->plotType;
			else
				return true;
		}

	}

	return false;
}

void PlotWrapper::drawPlot(mglGraph *gr) const
{
	if(areVisiblePlotsMismatched() || !plottingData.size())
		return;
	

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
			minX=std::min(minX,plottingData[ui]->minX);
			maxX=std::max(maxX,plottingData[ui]->maxX);
			minY=std::min(minY,plottingData[ui]->minY);
			maxY=std::max(maxY,plottingData[ui]->maxY);


			if(!xLabel.size())
				xLabel=plottingData[ui]->xLabel;
			else
			{

				if(xLabel!=plottingData[ui]->xLabel)
					xLabel=stlStrToStlWStr(TRANS("Multiple types"));
			}
			if(!yLabel.size())
				yLabel=plottingData[ui]->yLabel;
			else
			{

				if(yLabel!=plottingData[ui]->yLabel)
					yLabel=stlStrToStlWStr(TRANS("Multiple types"));
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
	

	unsigned int type;
	type=plottingData[0]->plotType;
	switch(type)
	{
		case PLOT_TYPE_ONED:
		{
			//OneD line plot
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
		
			//work out the bounding box for the plot,
			//and where the axis should cross
			mglPoint axisCross;
			mglPoint min,max;
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
				//Retreive the bounds of the data that is in the plot

				min.x=minX;
				min.y=minY;
				max.x=maxX;

				if(useLogPlot && maxY > 0.0f)
					max.y =log10(maxY);
				else
					max.y=maxY;

				axisCross.x = minX;
				axisCross.y = 0;
				
				gr->Org=axisCross;
			}
			
			//tell mathgl about the bounding box	
			gr->Axis(min,max,axisCross);


			//"Push" bounds around to prevent min == max
			if(min.x == max.x)
			{
				min.x-=0.5;
				max.x+=0.5;
			}

			if(min.y == max.y)
				max.y+=1;

			gr->AdjustTicks("x");
			gr->SetXTT("%g"); //Set the tick type
			gr->Axis("xy"); //Build an X-Y crossing axis

			//Draw regions as faces perp to z.
			//this will colour in a region of the graph as a rectangle
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
					char colourCode[2];
					colourCode[1]='\0';
					//Fake the colour by doing a lookup
					colourCode[0]= getNearestMathglColour(curPlot->r,
							curPlot->g, curPlot->b);

					gr->AddLegend(curPlot->title.c_str(),colourCode);
				}
			}

			//Prevent mathgl from dropping lines that straddle the plot bound.
			gr->SetCut(false);
		
			if(useLogPlot && !notLog)
				sY = string("\\log_{10}(") + sY + ")";
			else if (useLogPlot && notLog)
			{
				sY = string(TRANS("Mixed log/non-log:")) + sY ;
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
#ifdef MGL_GTE_1_10 
		gr->SetLegendBox(false);
#endif
		//I have NO idea what this size value is about,
		//I just kept changing the number until it looked nice.
		//the library default is -0.85 (why negative??)
		const float LEGEND_SIZE=-1.1f;

		//Legend at top right (0x3), in default font "rL" at specified size
		gr->Legend(0x3,"rL",LEGEND_SIZE);
	}
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

bool PlotWrapper::getRegionIdAtPosition(float x, float y, unsigned int &pId, unsigned int &rId) const
{
	for(size_t ui=0;ui<plottingData.size(); ui++)
	{
		//Regions can only be active for visible plots
		if(!plottingData[ui]->visible)
			continue;

		if(plottingData[ui]->getRegionIdAtPosition(x,y,rId))
		{
			pId=ui;
			return true;
		}
	}

	return false;
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
	
	maxThis=-std::numeric_limits<float>::max();
	minThis=std::numeric_limits<float>::max();
	for(unsigned int ui=0;ui<vY.size();ui++)
	{
		minThis=std::min(minThis,vY[ui]);
		maxThis=std::max(maxThis,vY[ui]);
	}
	minY=minThis;
	maxY=maxThis;
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
	for(unsigned int ui=0;ui<v.size();ui++)
	{
		minThis=std::min(minThis,v[ui].first);
		maxThis=std::max(maxThis,v[ui].first);
	}

	minX=minThis;
	maxX=maxThis;
	
	maxThis=-std::numeric_limits<float>::max();
	minThis=std::numeric_limits<float>::max();
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
	maxY=1.10f*maxThis; //The 1.10 is because mathgl chops off data


}

void Plot1D::getBounds(float &xMin,float &xMax,float &yMin,float &yMax) const
{
	//OK, we are going to have to scan for max/min
	xMin=minX;
	xMax=maxX;
	yMin=minY;
	yMax=maxY;

	//If we are in log mode, then we need to set the
	//log of that bound before emitting it.
	if(logarithmic && yMax)
	{
		yMin=log10(std::max(yMin,1.0f));
		yMax=log10(yMax);
	}
}

void Plot1D::drawPlot(mglGraph *gr) const
{
	unsigned int plotNum=0;
	bool showErrs;

	mglData xDat,yDat,eDat;
	ASSERT(visible);
		
	float *bufferX,*bufferY,*bufferErr;
	bufferX = new float[xValues.size()];
	bufferY = new float[yValues.size()];

	showErrs=errBars.size();
	if(showErrs)
		bufferErr = new float[errBars.size()];

	if(logarithmic)
	{
		for(unsigned int uj=0;uj<xValues.size(); uj++)
		{
			bufferX[uj] = xValues[uj];
			
			if(yValues[uj] > 0.0)
				bufferY[uj] = log10(yValues[uj]);
			else
				bufferY[uj] = 0;

		}
	}
	else
	{
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
	}
	
	//Mathgl needs to know where to put the error bars.	
	ASSERT(!showErrs  || errBars.size() ==xValues.size());
	
	xDat.Set(bufferX,xValues.size());
	yDat.Set(bufferY,yValues.size());

	eDat.Set(bufferErr,errBars.size());

	//Mathgl pallette colour name
	char colourCode[2];
	colourCode[1]='\0';
	//Fake the colour by doing a lookup
	colourCode[0]= getNearestMathglColour(r,g, b);

	//Plot the appropriate form	
	switch(traceType)
	{
		case PLOT_TRACE_LINES:
			//Unfortunately, when using line plots, mathgl moves the data points to the plot boundary,
			//rather than linear interpolating them back along their paths. I have emailed the author.
			//for now, we shall have to put up with missing lines :( Absolute worst case, I may have to draw them myself.
#ifdef MGL_GTE_1_10
			gr->SetCut(true);
			
			gr->Plot(xDat,yDat,colourCode);
			if(showErrs)
				gr->Error(xDat,yDat,eDat,colourCode);
			gr->SetCut(false);
#else
			gr->Plot(xDat,yDat);
#endif
			break;
		case PLOT_TRACE_BARS:
#if !defined(__WIN32) && !defined(__WIN64)
#ifdef MGL_GTE_1_10
			gr->Bars(xDat,yDat,colourCode);
#else
			gr->Bars(xDat,yDat);
#endif
#endif

			break;
		case PLOT_TRACE_STEPS:
			//Same problem as for line plot. 
#ifdef MGL_GTE_1_10
			gr->SetCut(true);
			gr->Step(xDat,yDat,colourCode);
			gr->SetCut(false);
#else
			gr->Step(xDat,yDat);
#endif
			break;
		case PLOT_TRACE_STEM:
#ifdef MGL_GTE_1_10
			gr->Stem(xDat,yDat,colourCode);
#else
			gr->Stem(xDat,yDat);
#endif
			break;
		default:
			ASSERT(false);
			break;
	}

	delete[]  bufferX;
	delete[]  bufferY;
	if(showErrs)
		delete[]  bufferErr;

	plotNum++;
			
	
}


void Plot1D::addRegion(unsigned int parentPlot,unsigned int regionID,float start, float end, 
			float rNew, float gNew, float bNew, Filter *parentFilter)
{
	ASSERT(start <end);
	ASSERT( rNew>=0.0 && rNew <= 1.0);
	ASSERT( gNew>=0.0 && gNew <= 1.0);
	ASSERT( bNew>=0.0 && bNew <= 1.0);

	PlotRegion region;
	//1D plots only have one bounding direction
	region.bounds.push_back(std::make_pair(start,end));
	region.boundAxis.push_back(0); // the bounding direction is along the X axis
	region.parentFilter = parentFilter;

	//Set the ID and create a unique ID (one that is invariant if regions are added or removed)
	//for the  region
	region.id = regionID;
	region.uniqueID = regionIDHandler.genId(regions.size());

	region.r=rNew;
	region.g=gNew;
	region.b=bNew;

	regions.push_back(region);
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
	plottingData[plotIDHandler.getPos(plotId)]->getRegion(regionId,region);
}

unsigned int PlotWrapper::plotType(unsigned int plotId) const
{
	return plottingData[plotIDHandler.getPos(plotId)]->plotType;
}


void PlotWrapper::moveRegionLimit(unsigned int plotId, unsigned int regionId,
			unsigned int movementType, float &constrainX, float &constrainY) const
{
	plottingData[plotIDHandler.getPos(plotId)]->moveRegionLimit(
				regionId,movementType,constrainX,constrainY);
}


void PlotWrapper::moveRegion(unsigned int plotID, unsigned int regionId, unsigned int movementType, 
							float newX, float newY) const
{
	plottingData[plotIDHandler.getPos(plotID)]->moveRegion(regionId,movementType,newX,newY);
}
//----

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
	labels.push_back(title);
	
	
	if(errBars.size())
	{
		tmp.resize(errBars.size());
		std::copy(errBars.begin(),errBars.end(),tmp.begin());
		
		rawData.push_back(dummy);
		rawData.back().swap(tmp);
		labels.push_back(stlStrToStlWStr(TRANS("error")));
	}
}


void Plot1D::moveRegionLimit(unsigned int regionID, 
			unsigned int method, float &newPosX, float &newPosY)  const
{

	unsigned int region=regionIDHandler.getPos(regionID);
	
	ASSERT(region<regions.size());

	//Check that moving this range will not cause any overlaps with 
	//other regions
	float mean;
	mean=(regions[region].bounds[0].first + regions[region].bounds[0].second)/2.0f;

	//Who is the owner of the current plot -- we only want to interact with our colleagues
	float xMin,xMax,yMin,yMax;
	getBounds(xMin,xMax,yMin,yMax);

	switch(method)
	{
		//Left extend
		case REGION_MOVE_EXTEND_XMINUS:
			//Check that the upper bound does not intersect any RHS of 
			//region bounds
			for(unsigned int ui=0; ui<regions.size(); ui++)
			{
				if((regions[ui].bounds[0].second < mean && ui !=region) )
						newPosX=std::max(newPosX,regions[ui].bounds[0].second);
			}
			//Dont allow past self right
			newPosX=std::min(newPosX,regions[region].bounds[0].second);
			//Dont extend outside plot
			newPosX=std::max(newPosX,xMin);
			break;
		//shift
		case REGION_MOVE_TRANSLATE_X:
			//Check that the upper bound does not intersect any RHS or LHS of 
			//region bounds
			if(newPosX > mean) 
				
			{
				//Disallow hitting other bounds
				for(unsigned int ui=0; ui<regions.size(); ui++)
				{
					if((regions[ui].bounds[0].first > mean && ui != region) )
						newPosX=std::min(newPosX,regions[ui].bounds[0].first);
				}
				newPosX=std::max(newPosX,xMin);
			}
			else
			{
				//Disallow hitting other bounds
				for(unsigned int ui=0; ui<regions.size(); ui++)
				{
					if((regions[ui].bounds[0].second < mean && ui != region))
						newPosX=std::max(newPosX,regions[ui].bounds[0].second);
				}
				//Dont extend outside plot
				newPosX=std::min(newPosX,xMax);
			}
			break;
		//Right extend
		case REGION_MOVE_EXTEND_XPLUS:
			//Disallow hitting other bounds

			for(unsigned int ui=0; ui<regions.size(); ui++)
			{
				if((regions[ui].bounds[0].second > mean && ui != region))
					newPosX=std::min(newPosX,regions[ui].bounds[0].first);
			}
			//Dont allow past self left
			newPosX=std::max(newPosX,regions[region].bounds[0].first);
			//Dont extend outside plot
			newPosX=std::min(newPosX,xMax);
			break;
		default:
			ASSERT(false);
	}

}


void Plot1D::moveRegion(unsigned int regionID, unsigned int method, float newPosX,float newPosY) const
{
	//Well, we should have called this externally to determine location
	//let us confirm that this is the case 
	moveRegionLimit(regionID,method,newPosX,newPosY);


	unsigned int region=regionIDHandler.getPos(regionID);
	ASSERT(regions[region].parentFilter);

	//Pass the filter ID value stored in the region to the filter, along with the new
	//value to take for that filter ID
	regions[region].parentFilter->setPropFromRegion(method,regions[region].id,newPosX);	

}


void Plot1D::drawRegions(mglGraph *gr,const mglPoint &min,const mglPoint &max) const
{
	//Mathgl pallette colour name
	char colourCode[2];
	colourCode[1]='\0';
	
	for(unsigned int uj=0;uj<regions.size();uj++)
	{
		//Compute region bounds, such that it will not exceed the axis
		float rMinX, rMaxX, rMinY,rMaxY;
		rMinY = min.y;
		rMaxY = max.y;
		rMinX = std::max(min.x,regions[uj].bounds[0].first);
		rMaxX = std::min(max.x,regions[uj].bounds[0].second);
		
		//Prevent drawing inverted regions
		if(rMaxX > rMinX && rMaxY > rMinY)
		{
			colourCode[0] = getNearestMathglColour(regions[uj].r,regions[uj].g,regions[uj].b);
			gr->FaceZ(rMinX,rMinY,-1,rMaxX-rMinX,rMaxY-rMinY,
					colourCode);
					
		}
	}
}


void Plot1D::clear( bool preserveVisiblity)
{
	regions.clear();
	regionIDHandler.clear();
}

bool Plot1D::getRegionIdAtPosition(float x, float y, unsigned int &id) const
{
	for(unsigned int ui=0;ui<regions.size();ui++)
	{
		ASSERT(regions[ui].boundAxis.size() == regions[ui].bounds.size() &&
				regions[ui].boundAxis.size()==1);
		ASSERT(regions[ui].boundAxis[0] == 0);


		if(regions[ui].bounds[0].first < x &&
				regions[ui].bounds[0].second > x )
		{
			id=ui;
			return true;
		}
	}


	return false;
}

void Plot1D::getRegion(unsigned int id, PlotRegion &r) const
{
	r = regions[regionIDHandler.getPos(id)];
}


//---------


//Draw the plot onto a given MGL graph
void Plot2D::drawPlot(mglGraph *gr) const
{
	unsigned int plotNum=0;
	bool showErrs;
	mglData xDat,yDat,exDat,eyDat;
	
	ASSERT(visible);
	ASSERT(xValues.size() == yValues.size());
		
	float *bufferX,*bufferY,*bufferErrX,*bufferErrY;
	bufferX = new float[xValues.size()];
	bufferY = new float[yValues.size()];

	ASSERT(xErrBars.size() == yErrBars.size() || !xErrBars.size() || !yErrBars.size());	
	if(xErrBars.size())
		bufferErrX = new float[xErrBars.size()];
	if(yErrBars.size())
		bufferErrY = new float[yErrBars.size()];

	for(unsigned int uj=0;uj<xValues.size(); uj++)
	{
		bufferX[uj] = xValues[uj];
		bufferY[uj] = yValues[uj];
	}
	if(xErrBars.size())
	{
		for(unsigned int uj=0;uj<xErrBars.size(); uj++)
			bufferErrX[uj] = xErrBars[uj];
		exDat.Set(bufferErrX,xErrBars.size());
	}

	if(yErrBars.size())
	{
		for(unsigned int uj=0;uj<yErrBars.size(); uj++)
			bufferErrY[uj] = yErrBars[uj];
		eyDat.Set(bufferErrY,yErrBars.size());
	}

	xDat.Set(bufferX,xValues.size());
	yDat.Set(bufferY,yValues.size());

	//Mathgl pallette colour name
	char colourCode[2];
	colourCode[1]='\0';
	//Fake the colour by doing a lookup
	colourCode[0]= getNearestMathglColour(r,g, b);
	
	//Plot the appropriate form	
	switch(traceType)
	{
		case PLOT_TRACE_LINES:
			//Unfortunately, when using line plots, mathgl moves the data points to the plot boundary,
			//rather than linear interpolating them back along their paths. I have emailed the author.
			//for now, we shall have to put up with missing lines :( Absolute worst case, I may have to draw them myself.
#ifdef MGL_GTE_1_10
			gr->SetCut(true);
			
			gr->Plot(xDat,yDat,colourCode);
			if(xErrBars.size() && yErrBars.size())
				gr->Error(xDat,yDat,exDat,eyDat,colourCode);
			else if(xErrBars.size())
			{
				gr->Error(xDat,yDat,exDat,colourCode);
			}
			else if(yErrBars.size())
			{
				ASSERT(false);
			}
			
			gr->SetCut(false);
#else
			gr->Plot(xDat,yDat);
#endif
			break;
	}
}

//!Scan for the data bounds.
void Plot2D::getBounds(float &xMin,float &xMax,
			       float &yMin,float &yMax) const
{
	//OK, we are going to have to scan for max/min
	xMin=minX;
	xMax=maxX;
	yMin=minY;
	yMax=maxY;
}

//Retrieve the raw data associated with this plot.
void Plot2D::getRawData(vector<vector<float> > &rawData,
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
	labels.push_back(title);
	
	
	ASSERT(xErrBars.size() == yErrBars.size()  || !xErrBars.size() || !yErrBars.size());
	
	if(xErrBars.size())
	{
		tmp.resize(xErrBars.size());
		std::copy(xErrBars.begin(),xErrBars.end(),tmp.begin());
		
		rawData.push_back(dummy);
		rawData.back().swap(tmp);
		labels.push_back(stlStrToStlWStr(TRANS("x error")));
	}
	
	if(yErrBars.size())
	{
		tmp.resize(yErrBars.size());
		std::copy(yErrBars.begin(),yErrBars.end(),tmp.begin());
		
		rawData.push_back(dummy);
		rawData.back().swap(tmp);
		labels.push_back(stlStrToStlWStr(TRANS("y error")));
	}
}

//!Retrieve the ID of the non-overlapping region in X-Y space
bool Plot2D::getRegionIdAtPosition(float x, float y, unsigned int &id) const
{
	for(unsigned int ui=0;ui<regions.size();ui++)
	{
		ASSERT(regions[ui].boundAxis.size() == regions[ui].bounds.size())

		bool containedInThis;
		containedInThis=true;

		for(unsigned int uj=0; uj<regions[ui].bounds.size();uj++)
		{
			unsigned int axis;
			axis = regions[ui].boundAxis[uj];
			if(axis == 0)
			{
				if(regions[ui].bounds[uj].first < x &&
					regions[ui].bounds[uj].second > x )
				{
					containedInThis=false;
					break;
				}
			}
			else
			{
				if(regions[ui].bounds[uj].first < y &&
					regions[ui].bounds[uj].second > y )
				{
					containedInThis=false;
					break;

				}
			}
		}	

		
		if(containedInThis)	
		{
			id=ui;
			return true;
		}
	}

	return false;
}
//!Retrieve a region using its unique ID
void Plot2D::getRegion(unsigned int id, PlotRegion &r) const
{
	r = regions[regionIDHandler.getPos(id)];
}

//!Pass the region movement information to the parent filter object
void Plot2D::moveRegion(unsigned int regionId, unsigned int method,
                        float newX, float newY) const
{
	ASSERT(false);
}

//!Obtain limit of motion for a given region movement type
void Plot2D::moveRegionLimit(unsigned int regionId,
                             unsigned int movementType, float &maxX, float &maxY) const
{
	ASSERT(false);
}
	

