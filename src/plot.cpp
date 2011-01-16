/*
 *	plot.cpp - mathgl plot wrapper class
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


#include "basics.h"
#include "plot.h"


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
				"None",
				"Moving avg."
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

string plotString(unsigned int plotType)
{
	switch(plotType)
	{
		case PLOT_TYPE_LINES:
			return string("Lines");
		case PLOT_TYPE_BARS:
			return string("Bars");
		case PLOT_TYPE_STEPS:
			return string("Steps");
		case PLOT_TYPE_STEM:
			return string("Stem");
		default:
			ASSERT(false);
			return string("bug:(plotString)");
	}
}

unsigned int plotID(const std::string &plotString)
{
	if(plotString == "Lines")
		return PLOT_TYPE_LINES;
	if(plotString == "Bars")
		return PLOT_TYPE_BARS;
	if(plotString == "Steps")
		return PLOT_TYPE_STEPS;
	if(plotString == "Stem")
		return PLOT_TYPE_STEM;
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

//===

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

				stddev=sqrt(stddev/(float)errMode.movingAverageNum);
				errBars[ui]=stddev;
			}
			break;
		}
	}

}

void Multiplot::clear(bool preserveVisiblity)
{

	//Do our best to preserve visiblity of
	//plots. 
	lastVisiblePlots.clear();
	if(preserveVisiblity)
	{
		//Remember which plots were visible, who owned them, and their index
		for(unsigned int ui=0;ui<plottingData.size(); ui++)
		{
			if(plottingData[ui].visible && plottingData[ui].parentObject)
			{
				lastVisiblePlots.push_back(std::make_pair(plottingData[ui].parentObject,
								plottingData[ui].parentPlotIndex));
			}
		}
	}
	else
		applyUserBounds=false;


	plottingData.clear();
	regions.clear();
	plotIDHandler.clear();
	regionIDHandler.clear();
	plotChanged=true;
}

void Multiplot::setStrings(unsigned int plotID, const std::string &x, 
				const std::string &y, const std::string &t)
{
	unsigned int plotPos=plotIDHandler.getPos(plotID);
	plottingData[plotPos].xLabel = strToWStr(x);
	plottingData[plotPos].yLabel = strToWStr(y);
	
	plottingData[plotPos].title = strToWStr(t);
	plotChanged=true;
}

void Multiplot::setParentData(unsigned int plotID, const void *parentObj, unsigned int idx)
{
	unsigned int plotPos=plotIDHandler.getPos(plotID);
	plottingData[plotPos].parentObject= parentObj;
	plottingData[plotPos].parentPlotIndex=idx;
	
	plotChanged=true;
}

unsigned int Multiplot::addPlot(const vector<float> &vX, const vector<float> &vY,
				const PLOT_ERROR &errMode,bool isLog)
{
	PlotData newPlotData ;

	plottingData.push_back(newPlotData);
	PlotData &newDataRef=plottingData.back();

	//push back, the swap later.
	newDataRef.logarithmic=isLog;
	
	//Fill up vectors with data
	newDataRef.xValues.resize(vX.size());
	std::copy(vX.begin(),vX.end(),newDataRef.xValues.begin());
	newDataRef.yValues.resize(vY.size());
	std::copy(vY.begin(),vY.end(),newDataRef.yValues.begin());
	genErrBars(newDataRef.xValues,newDataRef.yValues,newDataRef.errBars,errMode);


	//Compute minima and maxima of plot data, and keep a copy of it
	float maxThis=-std::numeric_limits<float>::max();
	float minThis=std::numeric_limits<float>::max();
	for(unsigned int ui=0;ui<vX.size();ui++)
	{
		minThis=std::min(minThis,vX[ui]);
		maxThis=std::max(maxThis,vX[ui]);
	}

	newDataRef.minX=minThis;
	newDataRef.maxX=maxThis;
	
	maxThis=-std::numeric_limits<float>::max();
	minThis=std::numeric_limits<float>::max();
	for(unsigned int ui=0;ui<vY.size();ui++)
	{
		minThis=std::min(minThis,vY[ui]);
		maxThis=std::max(maxThis,vY[ui]);
	}
	newDataRef.minY=minThis;
	newDataRef.maxY=maxThis;

	//Set the default plot properties
	newDataRef.visible=(true);
	newDataRef.plotType=0;
	newDataRef.xLabel=(strToWStr(""));
	newDataRef.yLabel=(strToWStr(""));
	newDataRef.title=(strToWStr(""));
	newDataRef.r=(0);newDataRef.g=(0);newDataRef.b=(1);

	//assign a unique identifier to this plot, by which it can be referenced
	unsigned int uniqueID = plotIDHandler.genId(plottingData.size()-1);
	plotChanged=true;
	return uniqueID;
}


unsigned int Multiplot::addPlot(const vector<std::pair<float,float> > &v, 
						const PLOT_ERROR &errMode, bool isLog)
{
	PlotData newPlotData;

	plottingData.push_back(newPlotData);
	PlotData &newDataRef=plottingData.back();

	//push back, the swap later.
	newDataRef.logarithmic=isLog;
	
	//Fill up vectors with data
	newDataRef.xValues.resize(v.size());
	newDataRef.yValues.resize(v.size());
	for(unsigned int ui=0;ui<v.size();ui++)
	{
		newDataRef.xValues[ui]=v[ui].first;
		newDataRef.yValues[ui]=v[ui].second;
	}
	genErrBars(newDataRef.xValues,newDataRef.yValues,newDataRef.errBars,errMode);

	//Compute minima and maxima of plot data, and keep a copy of it
	float maxThis=-std::numeric_limits<float>::max();
	float minThis=std::numeric_limits<float>::max();
	for(unsigned int ui=0;ui<v.size();ui++)
	{
		minThis=std::min(minThis,v[ui].first);
		maxThis=std::max(maxThis,v[ui].first);
	}

	newDataRef.minX=minThis;
	newDataRef.maxX=maxThis;
	
	maxThis=-std::numeric_limits<float>::max();
	minThis=std::numeric_limits<float>::max();
	if(newDataRef.errBars.size())
	{
		ASSERT(newDataRef.errBars.size() == v.size());
		for(unsigned int ui=0;ui<v.size();ui++)
		{
			minThis=std::min(minThis,v[ui].second-newDataRef.errBars[ui]);
			maxThis=std::max(maxThis,v[ui].second+newDataRef.errBars[ui]);
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
	newDataRef.minY=minThis;
	newDataRef.maxY=1.10f*maxThis; //The 1.10 is because mathgl chops off data


	//Set the default plot properties
	newDataRef.visible=(true);
	newDataRef.plotType=0;
	newDataRef.xLabel=(strToWStr(""));
	newDataRef.yLabel=(strToWStr(""));
	newDataRef.title=(strToWStr(""));
	newDataRef.r=(0);newDataRef.g=(0);newDataRef.b=(1);

	//assign a unique identifier to this plot, by which it can be referenced
	unsigned int uniqueID = plotIDHandler.genId(plottingData.size()-1);
	plotChanged=true;
	return uniqueID;
}



void Multiplot::setType(unsigned int plotUniqueID,unsigned int mode)
{

	ASSERT(mode<PLOT_TYPE_ENDOFENUM);
	plottingData[plotIDHandler.getPos(plotUniqueID)].plotType=mode;
	plotChanged=true;
}

void Multiplot::setColours(unsigned int plotUniqueID, float rN,float gN,float bN) 
{
	unsigned int plotPos=plotIDHandler.getPos(plotUniqueID);
	plottingData[plotPos].r=rN;
	plottingData[plotPos].g=gN;
	plottingData[plotPos].b=bN;
	plotChanged=true;
}


void Multiplot::setBounds(float xMin, float xMax,
			float yMin,float yMax)
{
	ASSERT(xMin < xMax);
	ASSERT(yMin< yMax);
	xUserMin=xMin;
	yUserMin=std::max(0.0f,yMin);
	xUserMax=xMax;
	yUserMax=yMax;

	applyUserBounds=true;
	plotChanged=true;
}

void Multiplot::getBounds(float &xMin, float &xMax,
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
	{
		//OK, we are going to have to scan for max/min
		xMin=std::numeric_limits<float>::max();
		xMax=-std::numeric_limits<float>::max();
		yMin=std::numeric_limits<float>::max();
		yMax=-std::numeric_limits<float>::max();

		for(unsigned int ui=0;ui<plottingData.size();ui++)
		{
			if(plottingData[ui].visible)
			{
				xMin=std::min(plottingData[ui].minX,xMin);
				xMax=std::max(plottingData[ui].maxX,xMax);

				yMin=std::min(plottingData[ui].minY,yMin);
				yMax=std::max(plottingData[ui].maxY,yMax);
			}
		}

		//If we are in log mode, then we need to set the
		//log of that bound before emitting it.
		if(isLogarithmic())
		{
			yMin=log10(std::max(yMin,1.0f));
			yMax=log10(yMax);
		}

	}

	ASSERT(xMin < xMax && yMin < yMax);
}

void Multiplot::bestEffortRestoreVisibility()
{
	//Try to match up the last visible plots to the
	//new plots. Use index and owner as guiding data

	for(unsigned int uj=0;uj<plottingData.size(); uj++)
		plottingData[uj].visible=false;
	
	for(unsigned int ui=0; ui<lastVisiblePlots.size();ui++)
	{
		for(unsigned int uj=0;uj<plottingData.size(); uj++)
		{
			if(plottingData[uj].parentObject == lastVisiblePlots[ui].first
				&& plottingData[uj].parentPlotIndex == lastVisiblePlots[ui].second)
			{
				plottingData[uj].visible=true;
				break;
			}
		
		}
	}

	lastVisiblePlots.clear();
	plotChanged=true;
}

//Is the plot being displayed in lgo mode?
bool Multiplot::isLogarithmic() const
{
	for(unsigned int ui=0;ui<plottingData.size(); ui++)
	{
		if(plottingData[ui].visible)
		{
			if(plottingData[ui].logarithmic) 
				return true;
		}
	}

	return false;
}

void Multiplot::drawPlot(mglGraph *gr) const
{
	if(!plottingData.size())
		return;


	bool logarithmic=false;
	bool haveMultiTitles=false;
	float minX,maxX,minY,maxY;

	minX=std::numeric_limits<float>::max();
	maxX=-std::numeric_limits<float>::max();
	minY=std::numeric_limits<float>::max();
	maxY=-std::numeric_limits<float>::max();
	std::wstring xLabel,yLabel,plotTitle;
	bool notLog=false;
	for(unsigned int ui=0;ui<plottingData.size(); ui++)
	{
		if(plottingData[ui].visible)
		{
			minX=std::min(minX,plottingData[ui].minX);
			maxX=std::max(maxX,plottingData[ui].maxX);
			minY=std::min(minY,plottingData[ui].minY);
			maxY=std::max(maxY,plottingData[ui].maxY);


			if(!xLabel.size())
				xLabel=plottingData[ui].xLabel;
			else
			{

				if(xLabel!=plottingData[ui].xLabel)
					xLabel=L"Multiple types"; //L prefix means wide char
			}
			if(!yLabel.size())
				yLabel=plottingData[ui].yLabel;
			else
			{

				if(yLabel!=plottingData[ui].yLabel)
					yLabel=L"Multiple types";//L prefix means wide char
			}
			if(!haveMultiTitles && !plotTitle.size())
				plotTitle=plottingData[ui].title;
			else
			{

				if(plotTitle!=plottingData[ui].title)
				{
					plotTitle=L"";//L prefix means wide char
					haveMultiTitles=true;
				}
			}


		}

		if(plottingData[ui].visible)
		{
			if(plottingData[ui].logarithmic) 
				logarithmic=true;
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
		ASSERT(yUserMax > yUserMin);
		ASSERT(xUserMax > xUserMin);

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

		if(logarithmic && maxY > 0.0f)
			max.y =log10(maxY);
		else
			max.y=maxY;

		axisCross.x = minX;
		axisCross.y = 0;
		
		gr->Org=axisCross;
	}
	gr->Axis(min,max,axisCross);

	//I don't like the default, nor this. 
	//may have to make my own ticks & labels :(
//	gr->SetTuneTicks(false); // Disable common multiplier factorisation for X axis
	gr->AdjustTicks("x");
	gr->SetXTT("%g");
	gr->Axis("xy");

	//Mathgl pallette colour name
	char colourCode[2];
	colourCode[1]='\0';

	unsigned int regionNum=0;
	//Draw regions as faces perp to z.
	//this will colour in a region of the graph as a rectangle
	for(unsigned int ui=0;ui<plottingData.size();ui++)
	{
		//If a plot is not visible, it cannot own a region in this run.
		if(!plottingData[ui].visible)
			continue;

		for(unsigned int uj=0;uj<regions.size();uj++)
		{
			//Do not plot regions that do not belong to current plot
			if(regions[uj].ownerPlot!= plotIDHandler.getId(ui))
				continue;
			//Compute region bounds, such that it will not exceed the axis
			float rMinX, rMaxX, rMinY,rMaxY;
			rMinY = min.y;
			rMaxY = max.y;
			rMinX = std::max(min.x,regions[uj].bounds.first);
			rMaxX = std::min(max.x,regions[uj].bounds.second);
			
			//Prevent drawing inverted regions
			if(rMaxX > rMinX && rMaxY > rMinY)
			{
				colourCode[0] = getNearestMathglColour(regions[uj].r,regions[uj].g,regions[uj].b);
				gr->FaceZ(rMinX,rMinY,-1,rMaxX-rMinX,rMaxY-rMinY,
						colourCode);
						
				regionNum++;
			}
		}
	}
	
	//Prevent mathgl from dropping lines that straddle the plot region.
#ifdef MGL_GTE_1_10
	gr->SetCut(false);
#endif
	unsigned int plotNum=0;
	for(unsigned int ui=0;ui<plottingData.size(); ui++)
	{
		bool showErrs;

		mglData xDat,yDat,eDat;
		if(!plottingData[ui].visible)
			continue;	
		
		float *bufferX,*bufferY,*bufferErr;
		bufferX = new float[plottingData[ui].xValues.size()];
		bufferY = new float[plottingData[ui].yValues.size()];

		showErrs=plottingData[ui].errBars.size();
		bufferErr = new float[plottingData[ui].errBars.size()];



		if(logarithmic)
		{
			for(unsigned int uj=0;uj<plottingData[ui].xValues.size(); uj++)
			{
				bufferX[uj] = plottingData[ui].xValues[uj];
				
				if(plottingData[ui].yValues[uj] > 0.0)
					bufferY[uj] = log10(plottingData[ui].yValues[uj]);
				else
					bufferY[uj] = 0;

			}
		}
		else
		{
			for(unsigned int uj=0;uj<plottingData[ui].xValues.size(); uj++)
			{
				bufferX[uj] = plottingData[ui].xValues[uj];
				bufferY[uj] = plottingData[ui].yValues[uj];

			}
			if(showErrs)
			{
				for(unsigned int uj=0;uj<plottingData[ui].errBars.size(); uj++)
					bufferErr[uj] = plottingData[ui].errBars[uj];
			}
		}
	
		//Mathgl needs to know where to put the error bars.	
		ASSERT(!showErrs  || plottingData[ui].errBars.size() ==plottingData[ui].xValues.size());
		
		xDat.Set(bufferX,plottingData[ui].xValues.size());
		yDat.Set(bufferY,plottingData[ui].yValues.size());
	
		eDat.Set(bufferErr,plottingData[ui].errBars.size());

		//Fake it.
		colourCode[0]= getNearestMathglColour(plottingData[ui].r,
					plottingData[ui].g, plottingData[ui].b);

		//Plot the appropriate form	
		switch(plottingData[ui].plotType)
		{
			case PLOT_TYPE_LINES:
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
			case PLOT_TYPE_BARS:
#if !defined(__WIN32) && !defined(__WIN64)
#ifdef MGL_GTE_1_10
				gr->Bars(xDat,yDat,colourCode);
#else
				gr->Bars(xDat,yDat);
#endif
#endif

				break;
			case PLOT_TYPE_STEPS:
				//Same problem as for line plot. 
#ifdef MGL_GTE_1_10
				gr->SetCut(true);
				gr->Step(xDat,yDat,colourCode);
				gr->SetCut(false);
#else
				gr->Step(xDat,yDat);
#endif
				break;
			case PLOT_TYPE_STEM:
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

		if(drawLegend)
			gr->AddLegend(plottingData[ui].title.c_str(),colourCode);

		delete[]  bufferX;
		delete[]  bufferY;
		delete[]  bufferErr;

		plotNum++;
	}
	
	string sX;
	sX.assign(xLabel.begin(),xLabel.end()); //unicode conversion
#ifdef MGL_GTE_1_10
	gr->Label('x',sX.c_str());
#endif
	string sY;
	sY.assign(yLabel.begin(), yLabel.end()); //unicode conversion
	if(logarithmic && !notLog)
		sY = string("\\log_{10}(") + sY + ")";
	else if (logarithmic && notLog)
	{
		sY = string("Mixed log/non-log:") + sY ;
	}
#ifdef MGL_GTE_1_10
	gr->Label('y',sY.c_str(),0);
#endif	
	string sT;
	sT.assign(plotTitle.begin(), plotTitle.end()); //unicode conversion
	gr->Title(sT.c_str());
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

void Multiplot::hideAll()
{
	for(unsigned int ui=0;ui<plottingData.size(); ui++)
		plottingData[ui].visible=false;
	plotChanged=true;
}

void Multiplot::setVisible(unsigned int uniqueID, bool setVis)
{
	unsigned int plotPos = plotIDHandler.getPos(uniqueID);

	plottingData[plotPos].visible=setVis;
	plotChanged=true;
}

void Multiplot::addRegion(unsigned int parentPlot,unsigned int regionID,float start, float end, 
			float rNew, float gNew, float bNew, Filter *parentFilter)
{
	ASSERT(start <end);
	ASSERT( rNew>=0.0 && rNew <= 1.0);
	ASSERT( gNew>=0.0 && gNew <= 1.0);
	ASSERT( bNew>=0.0 && bNew <= 1.0);

	PlotRegion region;
	region.ownerPlot=parentPlot;
	region.bounds=std::make_pair(start,end);
	region.parentFilter = parentFilter;
	region.id = regionID;
	region.uniqueID = regionIDHandler.genId(regions.size());

	region.r=rNew;
	region.g=gNew;
	region.b=bNew;

	regions.push_back(region);
	plotChanged=true;
}

unsigned int Multiplot::getNumVisible() const
{
	unsigned int num=0;
	for(unsigned int ui=0;ui<plottingData.size();ui++)
	{
		if(plottingData[ui].visible)
			num++;
	}
	
	
	return num;
}

bool Multiplot::isPlotVisible(unsigned int plotID) const
{
	return plottingData[plotIDHandler.getPos(plotID)].visible;
}

void Multiplot::getRawData(std::vector<std::vector<std::vector< float> > > &rawData,
				std::vector<std::vector<std::wstring> > &labels) const
{
	std::vector<vector<float> > tmp;
	labels.clear();
	for(unsigned int ui=0; ui<plottingData.size();ui++)
	{
		if(plottingData[ui].visible)
		{
			vector<float> datX;
			vector<float> datY;
			vector<float> datE;

			std::vector<std::wstring> strVec;
			
			for(unsigned int uj=0;uj<plottingData[ui].xValues.size(); uj++)
			{
				datX.push_back(plottingData[ui].xValues[uj]);
				datY.push_back(plottingData[ui].yValues[uj]);
				if(plottingData[ui].errBars.size())
					datE.push_back(plottingData[ui].errBars[uj]);
			}

			if(datX.size())
			{
				tmp.push_back(datX);
				tmp.push_back(datY);
				strVec.push_back(plottingData[ui].xLabel);
				strVec.push_back(plottingData[ui].title);
			}

			if(datE.size())
			{
				tmp.push_back(datE);
				strVec.push_back(std::wstring(L"Error (std. dev)"));
			}


			rawData.push_back(tmp);
			labels.push_back(strVec);
			tmp.clear();
		}
	}
}

void Multiplot::getRegions(std::vector<PlotRegion> &copyRegions) const
{
	
	copyRegions.reserve(regions.size());

	for(unsigned int ui=0;ui<regions.size();ui++)
	{
		if(isPlotVisible(regions[ui].ownerPlot))
			copyRegions.push_back(regions[ui]); 
	}
}


char Multiplot::getNearestMathglColour(float r, float g, float b) const
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


float Multiplot::moveRegionTest(unsigned int regionID, 
			unsigned int method, float newPos)  const
{
	unsigned int region=(unsigned int)-1;
	for(unsigned int ui=0;ui<regions.size();ui++)
	{
		if(regionID== regions[ui].uniqueID)
			region=ui;
	}

	ASSERT(region<regions.size());
	ASSERT(isPlotVisible(regions[region].ownerPlot));

	//Check that moving this range will not cause any overlaps with 
	//other regions
	float mean;
	mean=(regions[region].bounds.first + regions[region].bounds.second)/2.0f;

	//Who is the owner of the current plot -- we only want to interact with our colleagues
	unsigned int curOwnerPlot= regions[region].ownerPlot;
	float xMin,xMax,yMin,yMax;
	getBounds(xMin,xMax,yMin,yMax);

	switch(method)
	{
		//Left extend
		case REGION_LEFT_EXTEND:
			//Check that the upper bound does not intersect any RHS of 
			//region bounds
			for(unsigned int ui=0; ui<regions.size(); ui++)
			{
				if(regions[ui].ownerPlot == curOwnerPlot &&
					(regions[ui].bounds.second < mean && ui !=region) )
						newPos=std::max(newPos,regions[ui].bounds.second);
			}
			//Dont allow past self right
			newPos=std::min(newPos,regions[region].bounds.second);
			//Dont extend outside plot
			newPos=std::max(newPos,xMin);
			break;
		//shift
		case REGION_MOVE:
			//Check that the upper bound does not intersect any RHS or LHS of 
			//region bounds
			if(newPos > mean) 
				
			{
				//Disallow hitting other bounds
				for(unsigned int ui=0; ui<regions.size(); ui++)
				{
					if(regions[ui].ownerPlot == curOwnerPlot &&
						(regions[ui].bounds.first > mean && ui != region) )
						newPos=std::min(newPos,regions[ui].bounds.first);
				}
				newPos=std::max(newPos,xMin);
			}
			else
			{
				//Disallow hitting other bounds
				for(unsigned int ui=0; ui<regions.size(); ui++)
				{
					if(regions[ui].ownerPlot == curOwnerPlot &&
						(regions[ui].bounds.second < mean && ui != region))
						newPos=std::max(newPos,regions[ui].bounds.second);
				}
				//Dont extend outside plot
				newPos=std::min(newPos,xMax);
			}
			break;
		//Right extend
		case REGION_RIGHT_EXTEND:
			//Disallow hitting other bounds

			for(unsigned int ui=0; ui<regions.size(); ui++)
			{
				if(regions[ui].ownerPlot == curOwnerPlot &&
					(regions[ui].bounds.second > mean && ui != region))
					newPos=std::min(newPos,regions[ui].bounds.first);
			}
			//Dont allow past self left
			newPos=std::max(newPos,regions[region].bounds.first);
			//Dont extend outside plot
			newPos=std::min(newPos,xMax);
			break;
		default:
			ASSERT(false);
	}



	return newPos;
}


void Multiplot::moveRegion(unsigned int regionID, unsigned int method, float newPos)
{

	//Well, we should have called this externally to determine location
	//let us confirm that this is the case 
	moveRegionTest(regionID,method,newPos);

	unsigned int region=(unsigned int)-1;
	for(unsigned int ui=0;ui<regions.size();ui++)
	{
		if(regionID== regions[ui].uniqueID)
			region=ui;
	}

	ASSERT(region<regions.size());
	ASSERT(regions[region].parentFilter);
	regions[region].parentFilter->setPropFromRegion(method,regions[region].id,newPos);	



}
