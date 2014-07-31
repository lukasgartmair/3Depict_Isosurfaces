/*
 *	spectrumPlot.cpp - Compute histograms of values for valued 3D point data
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
#include "spectrumPlot.h"

#include "../plot.h"

#include "filterCommon.h"

//!Error codes
enum 
{
	SPECTRUM_BAD_ALLOC=1,
	SPECTRUM_BAD_BINCOUNT,
	SPECTRUM_ABORT_FAIL,
	SPECTRUM_ERR_ENUM_END,
};

enum
{
	KEY_SPECTRUM_BINWIDTH,
	KEY_SPECTRUM_AUTOEXTREMA,
	KEY_SPECTRUM_MIN,
	KEY_SPECTRUM_MAX,
	KEY_SPECTRUM_LOGARITHMIC,
	KEY_SPECTRUM_PLOTTYPE,
	KEY_SPECTRUM_COLOUR
};

//Limit user to two :million: bins
const unsigned int SPECTRUM_MAX_BINS=2000000;

const unsigned int SPECTRUM_AUTO_MAX_BINS=45000;

SpectrumPlotFilter::SpectrumPlotFilter()
{
	minPlot=0;
	maxPlot=150;
	autoExtrema=true;	
	binWidth=0.05;
	plotStyle=0;
	logarithmic=1;

	//Default to blue plot
	rgba = ColourRGBAf(0,0,1.0f,1.0f);
}

Filter *SpectrumPlotFilter::cloneUncached() const
{
	SpectrumPlotFilter *p = new SpectrumPlotFilter();

	p->minPlot=minPlot;
	p->maxPlot=maxPlot;
	p->binWidth=binWidth;
	p->autoExtrema=autoExtrema;
	p->rgba=rgba;	
	p->plotStyle=plotStyle;
	p->logarithmic = logarithmic;


	//We are copying wether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

//!Get approx number of bytes for caching output
size_t SpectrumPlotFilter::numBytesForCache(size_t nObjects) const
{
	//Check that we have good plot limits, and bin width. if not, we cannot estimate cache size
	if(minPlot ==std::numeric_limits<float>::max() ||
		maxPlot==-std::numeric_limits<float>::max()  || 
		binWidth < sqrt(std::numeric_limits<float>::epsilon()))
	{
		return (size_t)(-1);
	}

	return (size_t)((float)(maxPlot- minPlot)/(binWidth))*2*sizeof(float);
}

unsigned int SpectrumPlotFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(bool))
{

	if(cacheOK)
	{
		//Only report the spectrum plot
		propagateCache(getOut);

		return 0;
	}



	size_t totalSize=numElements(dataIn,STREAM_TYPE_IONS);
	
	unsigned int nBins=2;
	if(totalSize)
	{

		//Determine min and max of input
		if(autoExtrema)
		{
			progress.maxStep=2;
			progress.step=1;
			progress.stepName=TRANS("Extrema");
		
			size_t n=0;
			minPlot = std::numeric_limits<float>::max();
			maxPlot =-std::numeric_limits<float>::max();
			//Loop through each type of data
			
			unsigned int curProg=NUM_CALLBACK;	
			for(unsigned int ui=0;ui<dataIn.size() ;ui++)
			{
				//Only process stream_type_ions. Do not propagate anything,
				//except for the spectrum
				if(dataIn[ui]->getStreamType() == STREAM_TYPE_IONS)
				{
					IonStreamData *ions;
					ions = (IonStreamData *)dataIn[ui];
					for(unsigned int uj=0;uj<ions->data.size(); uj++)
					{
						minPlot = std::min(minPlot,
							ions->data[uj].getMassToCharge());
						maxPlot = std::max(maxPlot,
							ions->data[uj].getMassToCharge());


						if(!curProg--)
						{
							n+=NUM_CALLBACK;
							progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
							curProg=NUM_CALLBACK;
							if(!(*callback)(false))
								return SPECTRUM_ABORT_FAIL;
						}
					}
		
				}
				
			}
		
			//Check that the plot values have been set (ie not same as initial values)
			if(minPlot !=std::numeric_limits<float>::max() &&
				maxPlot!=-std::numeric_limits<float>::max() )
			{
				//push them out a bit to make the edges visible
				maxPlot=maxPlot+1;
				minPlot=minPlot-1;
			}

			//Time to move to phase 2	
			progress.step=2;
			progress.stepName=TRANS("count");
		}



		double delta = ((double)maxPlot - (double)(minPlot))/(double)binWidth;


		//Check that we have good plot limits.
		if(minPlot ==std::numeric_limits<float>::max() || 
			minPlot ==-std::numeric_limits<float>::max() || 
			fabs(delta) >  std::numeric_limits<float>::max() || // Check for out-of-range
			 binWidth < sqrt(std::numeric_limits<float>::epsilon())	)
		{
			//If not, then simply set it to some defaults.
			minPlot=0; maxPlot=1.0; binWidth=0.1;
		}



		//Estimate number of bins in floating point, and check for potential overflow .
		float tmpNBins = (float)((maxPlot-minPlot)/binWidth);

		//If using autoextrema, use a lower limit for max bins,
		//as we may just hit a nasty data point
		if(autoExtrema)
			tmpNBins = std::min(SPECTRUM_AUTO_MAX_BINS,(unsigned int)tmpNBins);
		else
			tmpNBins = std::min(SPECTRUM_MAX_BINS,(unsigned int)tmpNBins);
		
		nBins = (unsigned int)tmpNBins;

		if (!nBins)
		{
			nBins = 10;
			binWidth = (maxPlot-minPlot)/nBins;
		}
	}


	PlotStreamData *d;
	d=new PlotStreamData;
	try
	{
		d->xyData.resize(nBins);
	}
	catch(std::bad_alloc)
	{

		delete d;
		return SPECTRUM_BAD_ALLOC;
	}

	
	d->r =rgba.r();
	d->g = rgba.g();
	d->b = rgba.b();
	d->a = rgba.a();

	d->logarithmic=logarithmic;
	d->plotStyle = plotStyle;
	d->plotMode=PLOT_MODE_1D;

	d->index=0;
	d->parent=this;
	d->dataLabel = getUserString();
	d->yLabel= TRANS("Count");

	//Check all the incoming ion data's type name
	//and if it is all the same, use it for the plot X-axis
	std::string valueName;
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS: 
			{
				const IonStreamData *ionD;
				ionD=(const IonStreamData *)dataIn[ui];
				if(!valueName.size())
					valueName=ionD->valueType;
				else
				{
					if(ionD->valueType != valueName)
					{
						valueName=TRANS("Mixed data");
						break;
					}
				}
			}
		}
	}

	d->xLabel=valueName;


	//Look for any ranges in input stream, and add them to the plot
	//while we are doing that, count the number of ions too
	for(unsigned int ui=0;ui<dataIn.size();ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_RANGE:
			{
				const RangeStreamData *rangeD;
				rangeD=(const RangeStreamData *)dataIn[ui];
				for(unsigned int uj=0;uj<rangeD->rangeFile->getNumRanges();uj++)
				{
					unsigned int ionId;
					ionId=rangeD->rangeFile->getIonID(uj);
					//Only append the region if both the range
					//and the ion are enabled
					if((rangeD->enabledRanges)[uj] && 
						(rangeD->enabledIons)[ionId])
					{
						//save the range as a "region"
						d->regions.push_back(rangeD->rangeFile->getRange(uj));
						d->regionTitle.push_back(rangeD->rangeFile->getName(ionId));
						d->regionID.push_back(uj);
						d->parent=this;
						//FIXME: Const correctness
						d->regionParent=(Filter*)rangeD->parent;

						RGBf colour;
						//Use the ionID to set the range colouring
						colour=rangeD->rangeFile->getColour(ionId);

						//push back the range colour
						d->regionR.push_back(colour.red);
						d->regionG.push_back(colour.green);
						d->regionB.push_back(colour.blue);
					}
				}
				break;
			}
			default:
				break;
		}
	}

#pragma omp parallel for
	for(unsigned int ui=0;ui<nBins;ui++)
	{
		d->xyData[ui].first = minPlot + ui*binWidth;
		d->xyData[ui].second=0;
	}	
	//Compute the plot bounds
	d->autoSetHardBounds();
	//Limit them to 1.0 or greater (due to log)
	d->hardMinY=std::min(1.0f,d->hardMaxY);


	//Number of ions currently processed
	size_t n=0;
	unsigned int curProg=NUM_CALLBACK;
	//Loop through each type of data	
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS: 
			{
				const IonStreamData *ions;
				ions = (const IonStreamData *)dataIn[ui];



				//Sum the data bins as needed
				for(unsigned int uj=0;uj<ions->data.size(); uj++)
				{
					unsigned int bin;
					bin = (unsigned int)((ions->data[uj].getMassToCharge()-minPlot)/binWidth);
					//Dependant upon the bounds,
					//actual data could be anywhere.
					if( bin < d->xyData.size())
						d->xyData[bin].second++;

					//update progress every CALLBACK ions
					if(!curProg--)
					{
						n+=NUM_CALLBACK;
						progress.filterProgress= (unsigned int)(((float)(n)/((float)totalSize))*100.0f);
						curProg=NUM_CALLBACK;
						if(!(*callback)(false))
						{
							delete d;
							return SPECTRUM_ABORT_FAIL;
						}
					}
				}

				break;
			}
			default:
				//Don't propagate any type.
				break;
		}

	}

	cacheAsNeeded(d);
	
	getOut.push_back(d);

	return 0;
}

void SpectrumPlotFilter::getProperties(FilterPropGroup &propertyList) const
{

	FilterProperty p;
	size_t curGroup=0;
	string str;

	stream_cast(str,binWidth);
	p.name=TRANS("Bin width");
	p.data=str;
	p.key=KEY_SPECTRUM_BINWIDTH;
	p.type=PROPERTY_TYPE_REAL;
	p.helpText=TRANS("Step size for spectrum");
	propertyList.addProperty(p,curGroup);

	str=boolStrEnc(autoExtrema);

	p.name=TRANS("Auto Min/max");
	p.data=str;
	p.key=KEY_SPECTRUM_AUTOEXTREMA;
	p.type=PROPERTY_TYPE_BOOL;
	p.helpText=TRANS("Automatically compute spectrum upper and lower bound");
	propertyList.addProperty(p,curGroup);

	stream_cast(str,minPlot);
	p.data=str;
	p.name=TRANS("Min");
	p.key=KEY_SPECTRUM_MIN;
	p.type=PROPERTY_TYPE_REAL;
	p.helpText=TRANS("Starting position for spectrum");
	propertyList.addProperty(p,curGroup);

	stream_cast(str,maxPlot);
	p.key=KEY_SPECTRUM_MAX;
	p.name=TRANS("Max");
	p.data=str;
	p.type=PROPERTY_TYPE_REAL;
	p.helpText=TRANS("Ending position for spectrum");
	propertyList.addProperty(p,curGroup);

	propertyList.setGroupTitle(curGroup,TRANS("Data"));
	curGroup++;

	str=boolStrEnc(logarithmic);
	p.key=KEY_SPECTRUM_LOGARITHMIC;
	p.name=TRANS("Logarithmic");
	p.data=str;
	p.type=PROPERTY_TYPE_BOOL;
	p.helpText=TRANS("Convert the plot to logarithmic mode");
	propertyList.addProperty(p,curGroup);

	//Let the user know what the valid values for plot type are
	vector<pair<unsigned int,string> > choices;


	string tmpStr;
	tmpStr=plotString(PLOT_LINE_LINES);
	choices.push_back(make_pair((unsigned int) PLOT_LINE_LINES,tmpStr));
	tmpStr=plotString(PLOT_LINE_BARS);
	choices.push_back(make_pair((unsigned int)PLOT_LINE_BARS,tmpStr));
	tmpStr=plotString(PLOT_LINE_STEPS);
	choices.push_back(make_pair((unsigned int)PLOT_LINE_STEPS,tmpStr));
	tmpStr=plotString(PLOT_LINE_STEM);
	choices.push_back(make_pair((unsigned int)PLOT_LINE_STEM,tmpStr));


	tmpStr= choiceString(choices,plotStyle);
	p.name=TRANS("Plot Type");
	p.data=tmpStr;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Visual style of plot");
	p.key=KEY_SPECTRUM_PLOTTYPE;
	propertyList.addProperty(p,curGroup);

	p.name=TRANS("Colour");
	p.data=rgba.toColourRGBA().rgbaString(); 
	p.type=PROPERTY_TYPE_COLOUR;
	p.helpText=TRANS("Colour of plotted spectrum");
	p.key=KEY_SPECTRUM_COLOUR;
	propertyList.addProperty(p,curGroup);

	propertyList.setGroupTitle(curGroup,TRANS("Appearance"));
}

bool SpectrumPlotFilter::setProperty( unsigned int key, 
					const std::string &value, bool &needUpdate) 
{
	needUpdate=false;
	switch(key)
	{
		//Bin width
		case KEY_SPECTRUM_BINWIDTH:
		{
			float newWidth;
			if(stream_cast(newWidth,value))
				return false;

			if(newWidth < std::numeric_limits<float>::epsilon())
				return false;

			//Prevent overflow on next line
			if(maxPlot == std::numeric_limits<float>::max() ||
				minPlot == std::numeric_limits<float>::min())
				return false;

			if(newWidth < 0.0f || newWidth > (maxPlot - minPlot))
				return false;



			needUpdate=true;
			binWidth=newWidth;
			clearCache();

			break;
		}
		//Auto min/max
		case KEY_SPECTRUM_AUTOEXTREMA:
		{
			if(!applyPropertyNow(autoExtrema,value,needUpdate))
				return false;
			break;

		}
		//Plot min
		case KEY_SPECTRUM_MIN:
		{
			if(autoExtrema)
				return false;

			float newMin;
			if(stream_cast(newMin,value))
				return false;

			if(newMin >= maxPlot)
				return false;

			needUpdate=true;
			minPlot=newMin;

			clearCache();
			break;
		}
		//Plot max
		case KEY_SPECTRUM_MAX:
		{
			if(autoExtrema)
				return false;
			float newMax;
			if(stream_cast(newMax,value))
				return false;

			if(newMax <= minPlot)
				return false;

			needUpdate=true;
			maxPlot=newMax;
			clearCache();

			break;
		}
		case KEY_SPECTRUM_LOGARITHMIC:
		{
			//Only allow valid values
			unsigned int valueInt;
			if(stream_cast(valueInt,value))
				return false;

			//Only update as needed
			if(valueInt ==0 || valueInt == 1)
			{
				if(logarithmic != (bool)valueInt)
				{
					needUpdate=true;
					logarithmic=valueInt;
				}
				else
					needUpdate=false;

			}
			else
				return false;		
			
			if(cacheOK)
			{
				//Change the output of the plot streams that
				//we cached, in order to avoid recomputation
				for(size_t ui=0;ui<filterOutputs.size();ui++)
				{
					if(filterOutputs[ui]->getStreamType() == STREAM_TYPE_PLOT)
					{
						PlotStreamData *p;
						p =(PlotStreamData*)filterOutputs[ui];

						p->logarithmic=logarithmic;
					}
				}

			}
	
			break;

		}
		//Plot type
		case KEY_SPECTRUM_PLOTTYPE:
		{
			unsigned int tmpPlotType;

			tmpPlotType=plotID(value);

			if(tmpPlotType >= PLOT_LINE_NONE)
				return false;

			plotStyle = tmpPlotType;
			needUpdate=true;	


			//Perform introspection on 
			//cache
			if(cacheOK)
			{
				for(size_t ui=0;ui<filterOutputs.size();ui++)
				{
					if(filterOutputs[ui]->getStreamType() == STREAM_TYPE_PLOT)
					{
						PlotStreamData *p;
						p =(PlotStreamData*)filterOutputs[ui];

						p->plotStyle=plotStyle;
					}
				}

			}

			break;
		}
		case KEY_SPECTRUM_COLOUR:
		{
			ColourRGBA tmpRgb;
			tmpRgb.parse(value);

			if(tmpRgb.toRGBAf() != rgba)
			{
				rgba=tmpRgb.toRGBAf();
				needUpdate=true;
			}
			
			if(cacheOK)
			{
				for(size_t ui=0;ui<filterOutputs.size();ui++)
				{
					if(filterOutputs[ui]->getStreamType() == STREAM_TYPE_PLOT)
					{
						PlotStreamData *p;
						p =(PlotStreamData*)filterOutputs[ui];

						p->r=rgba.r();
						p->g=rgba.g();
						p->b=rgba.b();
					}
				}

			}
			break;
		}
		default:
			ASSERT(false);
			break;

	}

	
	return true;
}

void SpectrumPlotFilter::setUserString(const std::string &s)
{
	if(userString !=s)
	{
		userString=s;
		clearCache();
		cacheOK=false;
	}
}


std::string  SpectrumPlotFilter::getErrString(unsigned int code) const
{
	const char *errStrs[] = {
		"",
		"Insufficient memory for spectrum filter.",
		"Bad bincount value in spectrum filter.",
		"Aborted."
	};
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(errStrs) == SPECTRUM_ERR_ENUM_END);
	ASSERT(code < SPECTRUM_ERR_ENUM_END);
	return errStrs[code];
}

void SpectrumPlotFilter::setPropFromBinding(const SelectionBinding &b)
{
	ASSERT(false);
}

bool SpectrumPlotFilter::writeState(std::ostream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<"  << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\""<< escapeXML(userString) << "\"/>"  << endl;

			f << tabs(depth+1) << "<extrema min=\"" << minPlot << "\" max=\"" <<
					maxPlot  << "\" auto=\"" << autoExtrema << "\"/>" << endl;
			f << tabs(depth+1) << "<binwidth value=\"" << binWidth<< "\"/>" << endl;

			f << tabs(depth+1) << "<colour r=\"" <<  rgba.r() << "\" g=\"" << rgba.g() << "\" b=\"" << rgba.b()
				<< "\" a=\"" << rgba.a() << "\"/>" <<endl;
			
			f << tabs(depth+1) << "<logarithmic value=\"" << logarithmic<< "\"/>" << endl;

			f << tabs(depth+1) << "<plottype value=\"" << plotStyle<< "\"/>" << endl;
			
			f << tabs(depth) << "</" << trueName() <<  ">" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool SpectrumPlotFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
	using std::string;
	string tmpStr;

	//Retrieve user string
	//===
	if(XMLHelpFwdToElem(nodePtr,"userstring"))
		return false;

	xmlChar *xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	userString=(char *)xmlString;
	xmlFree(xmlString);
	//===

	//Retrieve Extrema 
	//===
	float tmpMin,tmpMax;
	if(XMLHelpFwdToElem(nodePtr,"extrema"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"min");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;
	xmlFree(xmlString);

	//convert from string to digit
	if(stream_cast(tmpMin,tmpStr))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"max");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;
	xmlFree(xmlString);
	
	//convert from string to digit
	if(stream_cast(tmpMax,tmpStr))
		return false;


	if(tmpMin >=tmpMax)
		return false;

	minPlot=tmpMin;
	maxPlot=tmpMax;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"auto");
	if(!xmlString)
		return false;
	
	tmpStr=(char *)xmlString;
	if(!boolStrDec(tmpStr,autoExtrema))
	{
		xmlFree(xmlString);
		return false;
	}

	xmlFree(xmlString);
	//===
	
	//Retrieve bin width 
	//====
	//
	if(!XMLGetNextElemAttrib(nodePtr,binWidth,"binwidth","value"))
		return false;
	if(binWidth  <= 0.0)
		return false;

	if(!autoExtrema && binWidth > maxPlot - minPlot)
		return false;
	//====
	//Retrieve colour
	//====
	if(XMLHelpFwdToElem(nodePtr,"colour"))
		return false;
	ColourRGBAf tmpRgba;
	if(!parseXMLColour(nodePtr,tmpRgba))
		return false;
	rgba=tmpRgba;
	//====
	
	//Retrieve logarithmic mode
	//====
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"logarithmic","value"))
		return false;
	if(!boolStrDec(tmpStr,logarithmic))
		return false;
	//====

	//Retrieve plot type 
	//====
	if(!XMLGetNextElemAttrib(nodePtr,plotStyle,"plottype","value"))
		return false;
	if(plotStyle >= PLOT_LINE_NONE)
	       return false;	
	//====

	return true;
}

unsigned int SpectrumPlotFilter::getRefreshBlockMask() const
{
	//Absolutely nothing can go through this filter.
	return STREAMTYPE_MASK_ALL;
}

unsigned int SpectrumPlotFilter::getRefreshEmitMask() const
{
	return STREAM_TYPE_PLOT;
}

unsigned int SpectrumPlotFilter::getRefreshUseMask() const
{
	return STREAM_TYPE_IONS;
}

#ifdef DEBUG
#include <memory>

IonStreamData *synDataPoints(const unsigned int span[], unsigned int numPts)
{
	IonStreamData *d = new IonStreamData;
	
	for(unsigned int ui=0;ui<numPts;ui++)
	{
		IonHit h;
		h.setPos(Point3D(ui%span[0],
			ui%span[1],ui%span[2]));
		h.setMassToCharge(ui);
		d->data.push_back(h);
	}

	return d;
}

bool countTest()
{
	auto_ptr<IonStreamData> d;
	const unsigned int VOL[]={
				10,10,10
				};
	const unsigned int NUMPTS=100;
	d.reset(synDataPoints(VOL,NUMPTS));

	SpectrumPlotFilter *f;
	f = new SpectrumPlotFilter;


	bool needUp;
	TEST(f->setProperty(KEY_SPECTRUM_LOGARITHMIC,"0",needUp),"Set prop");
	
	ColourRGBA tmpRGBA(255,0,0);
	TEST(f->setProperty(KEY_SPECTRUM_COLOUR,tmpRGBA.rgbString(),needUp),"Set prop");

	vector<const FilterStreamData*> streamIn,streamOut;

	streamIn.push_back(d.get());

	//OK, so now do the rotation
	//Do the refresh
	ProgressData p;
	f->setCaching(false);
	TEST(!f->refresh(streamIn,streamOut,p,dummyCallback),"refresh error code");
	delete f;
	
	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_PLOT,"stream type");

	PlotStreamData *plot;
	plot = (PlotStreamData*)streamOut[0];

	
	//Check the plot colour is what we want.
	TEST(fabs(plot->r -1.0f) < sqrt(std::numeric_limits<float>::epsilon()),"colour (r)");
	TEST(plot->g< 
		sqrt(std::numeric_limits<float>::epsilon()),"colour (g)");
	TEST(plot->b < sqrt(std::numeric_limits<float>::epsilon()),"colour (b)");

	//Count the number of ions in the plot, and check that it is equal to the number of ions we put in.
	float sumV=0;
	
	for(unsigned int ui=0;ui<plot->xyData.size();ui++)
		sumV+=plot->xyData[ui].second;

	TEST(fabs(sumV - (float)NUMPTS) < 
		std::numeric_limits<float>::epsilon(),"ion count");


	delete plot;
	return true;
}

bool SpectrumPlotFilter::runUnitTests() 
{
	if(!countTest())
		return false;

	return true;
}

#endif

