#include "spectrumPlot.h"

#include "../xmlHelper.h"
#include "../plot.h"

//!Error codes
enum 
{
	SPECTRUM_BAD_ALLOC=1,
	SPECTRUM_BAD_BINCOUNT,
	SPECTRUM_ABORT_FAIL,
};

SpectrumPlotFilter::SpectrumPlotFilter()
{
	minPlot=0;
	maxPlot=150;
	autoExtrema=true;	
	binWidth=0.5;
	plotType=0;
	logarithmic=1;

	//Default to blue plot
	r=g=0;
	b=a=1;
}

Filter *SpectrumPlotFilter::cloneUncached() const
{
	SpectrumPlotFilter *p = new SpectrumPlotFilter();

	p->minPlot=minPlot;
	p->maxPlot=maxPlot;
	p->binWidth=binWidth;
	p->autoExtrema=autoExtrema;
	p->r=r;	
	p->g=g;	
	p->b=b;	
	p->a=a;	
	p->plotType=plotType;


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
	//Check that we have good plot limits, and bin width. if not, we cannot estmate cache size
	if(minPlot ==std::numeric_limits<float>::max() ||
		maxPlot==-std::numeric_limits<float>::max()  || 
		binWidth < sqrt(std::numeric_limits<float>::epsilon()))
	{
		return (size_t)(-1);
	}

	return (size_t)((float)(maxPlot- minPlot)/(binWidth))*2*sizeof(float);
}

unsigned int SpectrumPlotFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(void))
{

	if(cacheOK)
	{
		//Only report the spectrum plot
		for(unsigned int ui=0;ui<filterOutputs.size(); ui++)
			getOut.push_back(filterOutputs[ui]);
		return 0;
	}


	size_t totalSize=numElements(dataIn);
	

	//Determine min and max of input
	if(autoExtrema)
	{
		progress.maxStep=2;
		progress.step=1;
		progress.stepName="Extrema";
	
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
						if(!(*callback)())
							return SPECTRUM_ABORT_FAIL;
					}
				}
	
			}
			
		}
	
		//Check that the plot values hvae been set (ie not same as initial values)
		if(minPlot !=std::numeric_limits<float>::max() &&
			maxPlot!=-std::numeric_limits<float>::max() )
		{
			//push them out a bit to make the edges visible
			maxPlot=maxPlot+1;
			minPlot=minPlot-1;
		}

		//Time to move to phase 2	
		progress.step=2;
		progress.stepName="count";
	}



	double delta = ((double)maxPlot - (double)(minPlot))/(double)binWidth;


	//Check that we have good plot limits.
	if(minPlot ==std::numeric_limits<float>::max() || 
		minPlot ==-std::numeric_limits<float>::max() || 
		fabs(delta) >  std::numeric_limits<float>::max() || // Check for out-of-range
		 binWidth < sqrt(std::numeric_limits<float>::epsilon())	)
	{
		//If not, then simply set it to "1".
		minPlot=0; maxPlot=1.0; binWidth=0.1;
	}



	//Estimate number of bins in floating point, and check for potential overflow .
	float tmpNBins = (float)((maxPlot-minPlot)/binWidth);
	if(tmpNBins > SPECTRUM_MAX_BINS)
		tmpNBins=SPECTRUM_MAX_BINS;
	unsigned int nBins = (unsigned int)tmpNBins;

	if (!nBins)
	{
		tmpNBins = 10;
		binWidth = (maxPlot-minPlot)/nBins;
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
	d->r = r;
	d->g = g;
	d->b = b;
	d->a = a;
	d->logarithmic=logarithmic;
	d->plotType = plotType;
	d->index=0;
	d->parent=this;
	d->dataLabel = getUserString();
	d->yLabel= "Count";

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
						valueName="Mixed data";
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
						d->regionID.push_back(uj);
						d->parent=this;
						//FIXME: Const correctness
						d->regionParent=(Filter*)rangeD->parentFilter;

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


	//Number of ions currently procesed
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
						if(!(*callback)())
						{
							delete d;
							return SPECTRUM_ABORT_FAIL;
						}
					}
				}

			}
			default:
				//Don't propagate any type.
				break;
		}

	}

	if(cache)
	{
		d->cached=1; //IMPORTANT: cached must be set PRIOR to push back
		filterOutputs.push_back(d);
		cacheOK=true;
	}
	else
		d->cached=0;

	getOut.push_back(d);

	return 0;
}

void SpectrumPlotFilter::getProperties(FilterProperties &propertyList) const
{
	propertyList.data.clear();
	propertyList.keys.clear();
	propertyList.types.clear();

	vector<unsigned int> type,keys;
	vector<pair<string,string> > s;

	string str;

	stream_cast(str,binWidth);
	keys.push_back(KEY_SPECTRUM_BINWIDTH);
	s.push_back(make_pair("bin width", str));
	type.push_back(PROPERTY_TYPE_REAL);

	if(autoExtrema)
		str = "1";
	else
		str = "0";

	keys.push_back(KEY_SPECTRUM_AUTOEXTREMA);
	s.push_back(make_pair("Auto Min/max", str));
	type.push_back(PROPERTY_TYPE_BOOL);

	stream_cast(str,minPlot);
	keys.push_back(KEY_SPECTRUM_MIN);
	s.push_back(make_pair("Min", str));
	type.push_back(PROPERTY_TYPE_REAL);

	stream_cast(str,maxPlot);
	keys.push_back(KEY_SPECTRUM_MAX);
	s.push_back(make_pair("Max", str));
	type.push_back(PROPERTY_TYPE_REAL);
	
	if(logarithmic)
		str = "1";
	else
		str = "0";
	keys.push_back(KEY_SPECTRUM_LOGARITHMIC);
	s.push_back(make_pair("Logarithmic", str));
	type.push_back(PROPERTY_TYPE_BOOL);


	//Let the user know what the valid values for plot type are
	string tmpChoice;
	vector<pair<unsigned int,string> > choices;


	string tmpStr;
	tmpStr=plotString(PLOT_TYPE_LINES);
	choices.push_back(make_pair((unsigned int) PLOT_TYPE_LINES,tmpStr));
	tmpStr=plotString(PLOT_TYPE_BARS);
	choices.push_back(make_pair((unsigned int)PLOT_TYPE_BARS,tmpStr));
	tmpStr=plotString(PLOT_TYPE_STEPS);
	choices.push_back(make_pair((unsigned int)PLOT_TYPE_STEPS,tmpStr));
	tmpStr=plotString(PLOT_TYPE_STEM);
	choices.push_back(make_pair((unsigned int)PLOT_TYPE_STEM,tmpStr));


	tmpStr= choiceString(choices,plotType);
	s.push_back(make_pair(string("Plot Type"),tmpStr));
	type.push_back(PROPERTY_TYPE_CHOICE);
	keys.push_back(KEY_SPECTRUM_PLOTTYPE);

	string thisCol;

	//Convert the colour to a hex string
	genColString((unsigned char)(r*255.0),(unsigned char)(g*255.0),
		(unsigned char)(b*255.0),(unsigned char)(a*255.0),thisCol);

	s.push_back(make_pair(string("colour"),thisCol)); 
	type.push_back(PROPERTY_TYPE_COLOUR);
	keys.push_back(KEY_SPECTRUM_COLOUR);

	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);
}

bool SpectrumPlotFilter::setProperty(unsigned int set, unsigned int key, 
					const std::string &value, bool &needUpdate) 
{
	ASSERT(!set);

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

			break;
		}
		//Auto min/max
		case KEY_SPECTRUM_AUTOEXTREMA:
		{
			//Only allow valid values
			unsigned int valueInt;
			if(stream_cast(valueInt,value))
				return false;

			//Only update as needed
			if(valueInt ==0 || valueInt == 1)
			{
				if(autoExtrema != valueInt)
				{
					needUpdate=true;
					autoExtrema=valueInt;
				}
				else
					needUpdate=false;

			}
			else
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
				if(logarithmic != valueInt)
				{
					needUpdate=true;
					logarithmic=valueInt;
				}
				else
					needUpdate=false;

			}
			else
				return false;		
	
			break;

		}
		//Plot type
		case KEY_SPECTRUM_PLOTTYPE:
		{
			unsigned int tmpPlotType;

			tmpPlotType=plotID(value);

			if(tmpPlotType >= PLOT_TYPE_ENDOFENUM)
				return false;

			plotType = tmpPlotType;
			needUpdate=true;	
			break;
		}
		case KEY_SPECTRUM_COLOUR:
		{
			unsigned char newR,newG,newB,newA;

			parseColString(value,newR,newG,newB,newA);

			if(newB != b || newR != r ||
				newG !=g || newA != a)
				needUpdate=true;
			r=newR/255.0;
			g=newG/255.0;
			b=newB/255.0;
			a=newA/255.0;
			break;
		}
		default:
			ASSERT(false);
			break;

	}

	
	clearCache();
	return true;
}

std::string  SpectrumPlotFilter::getErrString(unsigned int code) const
{
	switch(code)
	{
		case SPECTRUM_BAD_ALLOC:
			return string("Insufficient memory for spectrum fitler.");
		case SPECTRUM_BAD_BINCOUNT:
			return string("Bad bincount value in spectrum filter.");
	}
	return std::string("BUG: (SpectrumPlotFilter::getErrString) Shouldn't see this!");
}

bool SpectrumPlotFilter::writeState(std::ofstream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<"  << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\""<<userString << "\"/>"  << endl;

			f << tabs(depth+1) << "<extrema min=\"" << minPlot << "\" max=\"" <<
					maxPlot  << "\" auto=\"" << autoExtrema << "\"/>" << endl;
			f << tabs(depth+1) << "<binwidth value=\"" << binWidth<< "\"/>" << endl;

			f << tabs(depth+1) << "<colour r=\"" <<  r<< "\" g=\"" << g << "\" b=\"" <<b
				<< "\" a=\"" << a << "\"/>" <<endl;
			
			f << tabs(depth+1) << "<logarithmic value=\"" << logarithmic<< "\"/>" << endl;

			f << tabs(depth+1) << "<plottype value=\"" << plotType<< "\"/>" << endl;
			
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

	//convert from string to digit
	if(stream_cast(tmpMin,tmpStr))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"max");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(tmpMax,tmpStr))
		return false;

	xmlFree(xmlString);

	if(tmpMin >=tmpMax)
		return false;

	minPlot=tmpMin;
	maxPlot=tmpMax;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"auto");
	if(!xmlString)
		return false;
	
	tmpStr=(char *)xmlString;
	if(tmpStr == "1") 
		autoExtrema=true;
	else if(tmpStr== "0")
		autoExtrema=false;
	else
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
	if(!parseXMLColour(nodePtr,r,g,b,a))
		return false;
	//====
	
	//Retrieve logarithmic mode
	//====
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"logarithmic","value"))
		return false;
	if(tmpStr == "0")
		logarithmic=false;
	else if(tmpStr == "1")
		logarithmic=true;
	else
		return false;
	//====

	//Retrieve plot type 
	//====
	if(!XMLGetNextElemAttrib(nodePtr,plotType,"plottype","value"))
		return false;
	if(plotType >= PLOT_TYPE_ENDOFENUM)
	       return false;	
	//====

	return true;
}

int SpectrumPlotFilter::getRefreshBlockMask() const
{
	//Absolutely nothing can go through this filter.
	return STREAMTYPE_MASK_ALL;
}

int SpectrumPlotFilter::getRefreshEmitMask() const
{
	return STREAM_TYPE_PLOT;
}
