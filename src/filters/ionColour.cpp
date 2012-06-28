#include "ionColour.h"

#include "../xmlHelper.h"

#include "../colourmap.h"

#include "../translation.h"

const unsigned int MAX_NUM_COLOURS=256;
enum
{
	KEY_IONCOLOURFILTER_COLOURMAP,
	KEY_IONCOLOURFILTER_MAPSTART,
	KEY_IONCOLOURFILTER_MAPEND,
	KEY_IONCOLOURFILTER_NCOLOURS,
	KEY_IONCOLOURFILTER_SHOWBAR,
};

enum
{
	IONCOLOUR_ABORT_ERR
};

IonColourFilter::IonColourFilter() : colourMap(0), nColours(MAX_NUM_COLOURS),
	showColourBar(true)
{
	mapBounds[0] = 0.0f;
	mapBounds[1] = 100.0f;

	cacheOK=false;
	cache=true; //By default, we should cache, but decision is made higher up

}

Filter *IonColourFilter::cloneUncached() const
{
	IonColourFilter *p=new IonColourFilter();
	p->colourMap = colourMap;
	p->mapBounds[0]=mapBounds[0];
	p->mapBounds[1]=mapBounds[1];
	p->nColours =nColours;	
	p->showColourBar =showColourBar;	
	
	//We are copying wether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

size_t IonColourFilter::numBytesForCache(size_t nObjects) const
{
		return (size_t)((float)(nObjects*IONDATA_SIZE));
}

DrawColourBarOverlay *IonColourFilter::makeColourBar() const
{
	//If we have ions, then we should draw a colour bar
	//Set up the colour bar. Place it in a draw stream type
	DrawColourBarOverlay *dc = new DrawColourBarOverlay;

	vector<float> r,g,b;
	r.resize(nColours);
	g.resize(nColours);
	b.resize(nColours);

	for (unsigned int ui=0;ui<nColours;ui++)
	{
		unsigned char rgb[3]; //RGB array
		float value;
		value = (float)(ui)*(mapBounds[1]-mapBounds[0])/(float)nColours + mapBounds[0];
		//Pick the desired colour map
		colourMapWrap(colourMap,rgb,value,mapBounds[0],mapBounds[1]);
		r[ui]=rgb[0]/255.0f;
		g[ui]=rgb[1]/255.0f;
		b[ui]=rgb[2]/255.0f;
	}

	dc->setColourVec(r,g,b);

	dc->setSize(0.08,0.6);
	dc->setPosition(0.1,0.1);
	dc->setMinMax(mapBounds[0],mapBounds[1]);


	return dc;
}


unsigned int IonColourFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(bool))
{
	//use the cached copy if we have it.
	if(cacheOK)
	{
		ASSERT(filterOutputs.size());
		for(unsigned int ui=0;ui<dataIn.size();ui++)
		{
			if(dataIn[ui]->getStreamType() != STREAM_TYPE_IONS)
				getOut.push_back(dataIn[ui]);
		}

		for(unsigned int ui=0;ui<filterOutputs.size();ui++)
			getOut.push_back(filterOutputs[ui]);

		if(filterOutputs.size() && showColourBar)
		{
			DrawStreamData *d = new DrawStreamData;
			d->parent=this;
			d->drawables.push_back(makeColourBar());
			d->cached=0;
			getOut.push_back(d);
		}
		return 0;
	}


	ASSERT(nColours >0 && nColours<=MAX_NUM_COLOURS);
	IonStreamData *d[nColours];
	unsigned char rgb[3]; //RGB array
	//Build the colourmap values, each as a unique filter output
	for(unsigned int ui=0;ui<nColours; ui++)
	{
		d[ui]=new IonStreamData;
		d[ui]->parent=this;
		float value;
		value = (float)ui*(mapBounds[1]-mapBounds[0])/(float)nColours + mapBounds[0];
		//Pick the desired colour map
		colourMapWrap(colourMap,rgb,value,mapBounds[0],mapBounds[1]);
	
		d[ui]->r=rgb[0]/255.0f;
		d[ui]->g=rgb[1]/255.0f;
		d[ui]->b=rgb[2]/255.0f;
		d[ui]->a=1.0f;
	}
	
	//Try to maintain ion size if possible
	bool haveIonSize,sameSize; // have we set the ionSize?
	float ionSize;
	haveIonSize=false;
	sameSize=true;

	//Did we find any ions in this pass?
	bool foundIons=false;	
	unsigned int totalSize=numElements(dataIn);
	unsigned int curProg=NUM_CALLBACK;
	size_t n=0;
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS: 
			{
				foundIons=true;

				//Check for ion size consistency	
				if(haveIonSize)
				{
					sameSize &= (fabs(ionSize-((const IonStreamData *)dataIn[ui])->ionSize) 
									< std::numeric_limits<float>::epsilon());
				}
				else
				{
					ionSize=((const IonStreamData *)dataIn[ui])->ionSize;
					haveIonSize=true;
				}
				for(vector<IonHit>::const_iterator it=((const IonStreamData *)dataIn[ui])->data.begin();
					       it!=((const IonStreamData *)dataIn[ui])->data.end(); ++it)
				{
					unsigned int colour;

					float tmp;	
					tmp= (it->getMassToCharge()-mapBounds[0])/(mapBounds[1]-mapBounds[0]);
					tmp = std::max(0.0f,tmp);
					tmp = std::min(tmp,1.0f);
					
					colour=(unsigned int)(tmp*(float)(nColours-1));	
					d[colour]->data.push_back(*it);
				
					//update progress every CALLBACK ions
					if(!curProg--)
					{
						n+=NUM_CALLBACK;
						progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
						curProg=NUM_CALLBACK;
						if(!(*callback)(false))
						{
							for(unsigned int ui=0;ui<nColours;ui++)
								delete d[ui];
							return IONCOLOUR_ABORT_ERR;
						}
					}
				}

				
				break;
			}
			default:
				getOut.push_back(dataIn[ui]);

		}
	}

	//create the colour bar as needd
	if(foundIons && showColourBar)
	{
		DrawStreamData *d = new DrawStreamData;
		d->drawables.push_back(makeColourBar());
		d->parent=this;
		d->cached=0;
		getOut.push_back(d);
	}


	//If all the ions are the same size, then propagate
	if(haveIonSize && sameSize)
	{
		for(unsigned int ui=0;ui<nColours;ui++)
			d[ui]->ionSize=ionSize;
	}
	//merge the results as needed
	if(cache)
	{
		for(unsigned int ui=0;ui<nColours;ui++)
		{
			if(d[ui]->data.size())
				d[ui]->cached=1;
			else
				d[ui]->cached=0;
			if(d[ui]->data.size())
				filterOutputs.push_back(d[ui]);
		}
		cacheOK=filterOutputs.size();
	}
	else
	{
		for(unsigned int ui=0;ui<nColours;ui++)
		{
			//NOTE: MUST set cached BEFORE push_back!
			d[ui]->cached=0;
		}
		cacheOK=false;
	}

	//push the colours onto the output. cached or not (their status is set above).
	for(unsigned int ui=0;ui<nColours;ui++)
	{
		if(d[ui]->data.size())
			getOut.push_back(d[ui]);
		else
			delete d[ui];
	}
	
	return 0;
}


void IonColourFilter::getProperties(FilterProperties &propertyList) const
{
	propertyList.data.clear();
	propertyList.keys.clear();
	propertyList.types.clear();

	vector<unsigned int> type,keys;
	vector<pair<string,string> > s;

	string str,tmpStr;


	vector<pair<unsigned int, string> > choices;

	for(unsigned int ui=0;ui<NUM_COLOURMAPS; ui++)
		choices.push_back(make_pair(ui,getColourMapName(ui)));

	tmpStr=choiceString(choices,colourMap);
	
	str =string(TRANS("Colour Map")); 

	s.push_back(make_pair(str,tmpStr));
	keys.push_back(KEY_IONCOLOURFILTER_COLOURMAP);
	type.push_back(PROPERTY_TYPE_CHOICE);

	if(showColourBar)
		tmpStr="1";
	else
		tmpStr="0";
	str =string(TRANS("Show Bar"));
	s.push_back(make_pair(str,tmpStr));
	keys.push_back(KEY_IONCOLOURFILTER_SHOWBAR);
	type.push_back(PROPERTY_TYPE_BOOL);


	str = TRANS("Num Colours");	
	stream_cast(tmpStr,nColours);
	s.push_back(make_pair(str,tmpStr));
	keys.push_back(KEY_IONCOLOURFILTER_NCOLOURS);
	type.push_back(PROPERTY_TYPE_INTEGER);

	stream_cast(tmpStr,mapBounds[0]);
	s.push_back(make_pair(TRANS("Map start"), tmpStr));
	keys.push_back(KEY_IONCOLOURFILTER_MAPSTART);
	type.push_back(PROPERTY_TYPE_REAL);

	stream_cast(tmpStr,mapBounds[1]);
	s.push_back(make_pair(TRANS("Map end"), tmpStr));
	keys.push_back(KEY_IONCOLOURFILTER_MAPEND);
	type.push_back(PROPERTY_TYPE_REAL);


	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);
}

bool IonColourFilter::setProperty( unsigned int set, unsigned int key,
					const std::string &value, bool &needUpdate)
{
	ASSERT(!set);

	needUpdate=false;
	switch(key)
	{
		case KEY_IONCOLOURFILTER_COLOURMAP:
		{
			unsigned int tmpMap;
			tmpMap=(unsigned int)-1;
			for(unsigned int ui=0;ui<NUM_COLOURMAPS;ui++)
			{
				if(value== getColourMapName(ui))
				{
					tmpMap=ui;
					break;
				}
			}

			if(tmpMap >=NUM_COLOURMAPS || tmpMap ==colourMap)
				return false;

			clearCache();
			needUpdate=true;
			colourMap=tmpMap;
			break;
		}
		case KEY_IONCOLOURFILTER_MAPSTART:
		{
			float tmpBound;
			stream_cast(tmpBound,value);
			if(tmpBound >=mapBounds[1])
				return false;

			clearCache();
			needUpdate=true;
			mapBounds[0]=tmpBound;
			break;
		}
		case KEY_IONCOLOURFILTER_MAPEND:
		{
			float tmpBound;
			stream_cast(tmpBound,value);
			if(tmpBound <=mapBounds[0])
				return false;

			clearCache();
			needUpdate=true;
			mapBounds[1]=tmpBound;
			break;
		}
		case KEY_IONCOLOURFILTER_NCOLOURS:
		{
			unsigned int numColours;
			if(stream_cast(numColours,value))
				return false;

			clearCache();
			needUpdate=true;
			//enforce 1->MAX_NUM_COLOURS range
			nColours=std::min(numColours,MAX_NUM_COLOURS);
			if(!nColours)
				nColours=1;
			break;
		}
		case KEY_IONCOLOURFILTER_SHOWBAR:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			bool lastVal=showColourBar;
			showColourBar=(stripped == "1");

			//Only need update if changed
			if(lastVal!=showColourBar)
				needUpdate=true;
			break;
		}	

		default:
			ASSERT(false);
	}	
	return true;
}


std::string  IonColourFilter::getErrString(unsigned int code) const
{
	//Currently the only error is aborting
	return std::string(TRANS("Aborted"));
}


bool IonColourFilter::writeState(std::ofstream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<" << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\""<< escapeXML(userString) << "\"/>"  << endl;

			f << tabs(depth+1) << "<colourmap value=\"" << colourMap << "\"/>" << endl;
			f << tabs(depth+1) << "<extrema min=\"" << mapBounds[0] << "\" max=\"" 
				<< mapBounds[1] << "\"/>" << endl;
			f << tabs(depth+1) << "<ncolours value=\"" << nColours << "\"/>" << endl;

			string str;
			if(showColourBar)
				str="1";
			else
				str="0";
			f << tabs(depth+1) << "<showcolourbar value=\"" << str << "\"/>" << endl;
			
			f << tabs(depth) << "</" << trueName() << ">" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool IonColourFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
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

	std::string tmpStr;	
	//Retrieve colourmap
	//====
	if(XMLHelpFwdToElem(nodePtr,"colourmap"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(colourMap,tmpStr))
		return false;

	if(colourMap>= NUM_COLOURMAPS)
	       return false;	
	xmlFree(xmlString);
	//====
	
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

	if(tmpMin > tmpMax)
		return false;

	mapBounds[0]=tmpMin;
	mapBounds[1]=tmpMax;

	//===
	
	//Retrieve num colours 
	//====
	if(XMLHelpFwdToElem(nodePtr,"ncolours"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(nColours,tmpStr))
		return false;

	xmlFree(xmlString);
	//====
	
	//Retrieve num colours 
	//====
	if(XMLHelpFwdToElem(nodePtr,"showcolourbar"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(showColourBar,tmpStr))
		return false;

	xmlFree(xmlString);
	//====
	return true;
}

unsigned int IonColourFilter::getRefreshBlockMask() const
{
	//Anything but ions can go through this filter.
	return STREAM_TYPE_IONS;
}

unsigned int IonColourFilter::getRefreshEmitMask() const
{
	return  STREAM_TYPE_DRAW | STREAM_TYPE_IONS;
}

unsigned int IonColourFilter::getRefreshUseMask() const
{
	return  STREAM_TYPE_IONS;
}
#ifdef DEBUG

IonStreamData *sythIonCountData(unsigned int numPts, float mStart, float mEnd)
{
	IonStreamData *d = new IonStreamData;
	d->data.resize(numPts);
	for(unsigned int ui=0; ui<numPts;ui++)
	{
		IonHit h;

		h.setPos(Point3D(ui,ui,ui));
		h.setMassToCharge( (mEnd-mStart)*(float)ui/(float)numPts + mStart);
		d->data[ui] =h;
	}

	return d;
}


bool ionCountTest()
{
	const int NUM_PTS=1000;
	vector<const FilterStreamData*> streamIn,streamOut;
	IonStreamData *d=sythIonCountData(NUM_PTS,0,100);
	streamIn.push_back(d);


	IonColourFilter *f = new IonColourFilter;
	f->setCaching(false);

	bool needUpdate;
	f->setProperty(0,KEY_IONCOLOURFILTER_NCOLOURS,"100",needUpdate);
	f->setProperty(0,KEY_IONCOLOURFILTER_MAPSTART,"0",needUpdate);
	f->setProperty(0,KEY_IONCOLOURFILTER_MAPEND,"100",needUpdate);
	f->setProperty(0,KEY_IONCOLOURFILTER_SHOWBAR,"0",needUpdate);
	
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p,dummyCallback),"refresh error code");
	delete f;
	delete d;
	
	TEST(streamOut.size() == 99,"stream count");

	for(unsigned int ui=0;ui<streamOut.size();ui++)
	{
		TEST(streamOut[ui]->getStreamType() == STREAM_TYPE_IONS,"stream type");
	}

	for(unsigned int ui=0;ui<streamOut.size();ui++)
		delete streamOut[ui];

	return true;
}


bool IonColourFilter::runUnitTests()
{
	if(!ionCountTest())
		return false;

	return true;
}


#endif

