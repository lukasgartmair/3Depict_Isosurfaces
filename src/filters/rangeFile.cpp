#include "rangeFile.h"
#include "../xmlHelper.h"
#include "../commonConstants.h"

enum
{
	KEY_RANGE_ACTIVE=1,
	KEY_RANGE_IONID,
	KEY_RANGE_START,
	KEY_RANGE_END,
};

//!Error codes
enum
{
	RANGEFILE_ABORT_FAIL=1,
	RANGEFILE_BAD_ALLOC,
};
//== Range File Filter == 

RangeFileFilter::RangeFileFilter()
{
	dropUnranged=true;
	assumedFileFormat=RANGE_FORMAT_ORNL;
}


Filter *RangeFileFilter::cloneUncached() const
{
	RangeFileFilter *p=new RangeFileFilter();
	p->rng = rng;
	p->rngName=rngName;
	p->enabledRanges.resize(enabledRanges.size());
	std::copy(enabledRanges.begin(),enabledRanges.end(),
					p->enabledRanges.begin());
	p->enabledIons.resize(enabledIons.size());
	std::copy(enabledIons.begin(),enabledIons.end(),
					p->enabledIons.begin());
	p->assumedFileFormat=assumedFileFormat;
	p->dropUnranged=dropUnranged;

	//We are copying wether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;	
	return p;
}

void RangeFileFilter::initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut)
{
	//Copy any input, except range files to output
	for(size_t ui=0;ui<dataIn.size();ui++)
	{
		if(dataIn[ui]->getStreamType() != STREAM_TYPE_RANGE)
			dataOut.push_back(dataIn[ui]);
	}

	//Create a rangestream data to push through the init phase
	if(rng.getNumIons() && rng.getNumRanges())
	{
		RangeStreamData *rngData=new RangeStreamData;
		rngData->parentFilter=this;
		rngData->rangeFile=&rng;	
		rngData->enabledRanges.resize(enabledRanges.size());	
		std::copy(enabledRanges.begin(),enabledRanges.end(),rngData->enabledRanges.begin());
		rngData->enabledIons.resize(enabledIons.size());	
		std::copy(enabledIons.begin(),enabledIons.end(),rngData->enabledIons.begin());
		rngData->cached=0;

		dataOut.push_back(rngData);
	}
	
}

unsigned int RangeFileFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
		std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(void))
{

	//use the cached copy of the data if we have it.
	if(cacheOK)
	{
		for(unsigned int ui=0;ui<filterOutputs.size(); ui++)
			getOut.push_back(filterOutputs[ui]);

		for(unsigned int ui=0;ui<dataIn.size() ;ui++)
		{
			//We don't cache anything but our modification
			//to the ion stream data types. so we propagate
			//these.
			if(dataIn[ui]->getStreamType() != STREAM_TYPE_IONS)
				getOut.push_back(dataIn[ui]);
		}
			
		return 0;
	}

	vector<IonStreamData *> d;

	//Split the output up into chunks, one for each range, 
	//Extra 1 for unranged ions	
	d.resize(rng.getNumIons()+1);

	//Generate output filter streams. 
	for(unsigned int ui=0;ui<d.size(); ui++)
		d[ui] = new IonStreamData;

	bool haveDefIonColour=false;
	RGBf defIonColour;

	//Try to maintain ion size if possible
	bool haveIonSize,sameSize; // have we set the ionSize?
	float ionSize;
	haveIonSize=false;
	sameSize=true;


	progress.step=1;
	progress.filterProgress=0;
	progress.stepName="Pre-Allocate";
	progress.maxStep=2;	

	vector<size_t> dSizes;
	dSizes.resize(d.size(),0);
	size_t totalSize=numElements(dataIn);
	
	//Step 1: Do a first sweep to obtain range sizes needed
	// then reserve the same amount of mem as we need on the output
	//========================
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS: 
			{

#ifdef _OPENMP
				//Create a unique array for each thread, so they don't try
				//to modify the same data structure
				unsigned int nT =omp_get_max_threads(); 
				vector<size_t> *dSizeArr = new vector<size_t>[nT];
				for(unsigned int uk=0;uk<nT;uk++)
					dSizeArr[uk].resize(dSizes.size(),0);
#endif
				const IonStreamData *src = ((const IonStreamData *)dataIn[ui]);
				size_t n=0;


				unsigned int curProg=NUM_CALLBACK;
				bool spin=false;
				#pragma omp parallel for firstprivate(curProg)
				for(size_t uj=0; uj<src->data.size();uj++)
				{
#ifdef _OPENMP
					unsigned int thisT=omp_get_thread_num();
#endif
					if(spin)
						continue;
					
					//get the range ID for this particular ion.
					unsigned int rangeID;
					rangeID=rng.getRangeID(src->data[uj].getMassToCharge());

					//If ion is unranged, then it will have a rangeID of -1
					if(rangeID != (unsigned int)-1 && enabledRanges[rangeID] )
					{
						unsigned int ionID=rng.getIonID(rangeID);

						//if we are going to keep the ion
						//then increment this array size
						if(enabledIons[ionID])
						{
							#ifdef _OPENMP
								dSizeArr[thisT][ionID]++;
							#else
								dSizes[ionID]++;
							#endif

						}
					}
					
					//update progress periodically
					if(!curProg--)
					{
#pragma omp critical
						{
						n+=NUM_CALLBACK;
						progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
						curProg=NUM_CALLBACK;


						if(!(*callback)())
							spin=true;
						}
					}
				}

				if(spin)
					return RANGEFILE_ABORT_FAIL;
#ifdef _OPENMP
				//Merge the arrays back together
				for(unsigned int uk=0;uk<nT;uk++)
				{
					for(unsigned int uj=0;uj<dSizes.size();uj++)
						dSizes[uj] = dSizes[uj]+dSizeArr[uk][uj];
				}
#endif

			}
		}
	}


	//reserve the vector to the exact size we need
	try
	{
		for(size_t ui=0;ui<d.size();ui++)
			d[ui]->data.reserve(dSizes[ui]);
	}
	catch(std::bad_alloc)
	{
		for(size_t ui=0;ui<d.size();ui++)
			delete d[ui];
		return RANGEFILE_BAD_ALLOC;
	}

	dSizes.clear();
	//===================================

	//Update progress info
	progress.step=2;
	progress.filterProgress=0;
	progress.stepName="Range";
	size_t n=0;


	

	//Step 2: Go through each data stream, if it is an ion stream, range it.
	//	I tried parallelising this a few different ways, but the linear performance wa simply better.
	//		- Tried an array of openmp locks
	//		- Tried keeping a unique offset number and fixing the size of the output vectors
	//		- Tried straight criticalling the push_back
	//	 Trying to merge vectors in // is tricky. result was 9-10x slower than linear
	//=========================================
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS: 
			{
				//Set the default (unranged) ion colour, by using
				//the first input ion colour.
				if(!haveDefIonColour)
				{
					defIonColour.red =  ((IonStreamData *)dataIn[ui])->r;
					defIonColour.green =  ((IonStreamData *)dataIn[ui])->g;
					defIonColour.blue =  ((IonStreamData *)dataIn[ui])->b;
					haveDefIonColour=true;
				}
			
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

				unsigned int curProg=NUM_CALLBACK;
				const size_t off=d.size()-1;

				for(vector<IonHit>::const_iterator it=((const IonStreamData *)dataIn[ui])->data.begin();
					       it!=((const IonStreamData *)dataIn[ui])->data.end(); ++it)
				{
					unsigned int ionID, rangeID;
					rangeID=rng.getRangeID(it->getMassToCharge());

					//If ion is unranged, then it will have a rangeID of -1
					if(rangeID != (unsigned int)-1)
					{
						ionID=rng.getIonID(rangeID);

						//Only retain the ion if the ionID and rangeID are enabled
						if(enabledRanges[rangeID] && enabledIons[ionID])
						{
							ASSERT(ionID < enabledRanges.size());

							d[ionID]->data.push_back(*it);
						}
					}
					else if(!dropUnranged)//If it is unranged, then the rangeID is still -1 (as above).
					{
						d[off]->data.push_back(*it);
					}

					//update progress periodically
					if(!curProg--)
					{
						n+=NUM_CALLBACK;
						progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
						curProg=NUM_CALLBACK;

						
						if(!(*callback)())
						{
							//Free space allocated for output ion streams...
							for(unsigned int ui=0;ui<d.size();ui++)
								delete d[ui];
							return RANGEFILE_ABORT_FAIL;
						}
					}

				}


			}
				break;
			case STREAM_TYPE_RANGE:
				//Purposely do nothing. This blocks propagation of other ranges
				//i.e. there can only be one in any given node of the tree.
				break;
			default:
				getOut.push_back(dataIn[ui]);
				break;
		}
	}
	//=========================================


	//Step 3 : Set up any properties for the output streams that we need, like colour, size, caching. Trim any empty results.
	//======================================
	//Set the colour of the output ranges
	//and whether to cache.
	for(unsigned int ui=0; ui<rng.getNumIons(); ui++)
	{
		RGBf rngCol;
		rngCol = rng.getColour(ui);
		d[ui]->r=rngCol.red;
		d[ui]->g=rngCol.green;
		d[ui]->b=rngCol.blue;
		d[ui]->a=1.0;

	}

	//If all the ions are the same size, then propagate
	//Otherwise use the default ionsize
	if(haveIonSize && sameSize)
	{
		for(unsigned int ui=0;ui<d.size();ui++)
			d[ui]->ionSize=ionSize;
	}
	
	//Set the unranged colour
	if(haveDefIonColour && d.size())
	{
		d[d.size()-1]->r = defIonColour.red;
		d[d.size()-1]->g = defIonColour.green;
		d[d.size()-1]->b = defIonColour.blue;
		d[d.size()-1]->a = 1.0f;
	}

	//remove any zero sized ranges
	for(unsigned int ui=0;ui<d.size();)
	{
		if(!(d[ui]->data.size()))
		{
			delete d[ui];
			std::swap(d[ui],d.back());
			d.pop_back();
		}
		else
			ui++;
	}

	//======================================

	//Having ranged all streams, merge them back into one ranged stream.
	if(cache)
	{
		for(unsigned int ui=0;ui<d.size(); ui++)
		{
			d[ui]->cached=1; //IMPORTANT: ->cached must be set PRIOR to push back
			filterOutputs.push_back(d[ui]);
		}
	}
	else
	{
		for(unsigned int ui=0;ui<d.size(); ui++)
			d[ui]->cached=0; //IMPORTANT: ->cached must be set PRIOR to push back
		cacheOK=false;
	}
	
	for(unsigned int ui=0;ui<d.size(); ui++)
		getOut.push_back(d[ui]);

	//Put out rangeData
	RangeStreamData *rngData=new RangeStreamData;
	rngData->parentFilter=this;
	rngData->rangeFile=&rng;	
	
	rngData->enabledRanges.resize(enabledRanges.size());	
	std::copy(enabledRanges.begin(),enabledRanges.end(),rngData->enabledRanges.begin());
	rngData->enabledIons.resize(enabledIons.size());	
	std::copy(enabledIons.begin(),enabledIons.end(),rngData->enabledIons.begin());
	
	
	rngData->cached=cache;
	
	if(cache)
		filterOutputs.push_back(rngData);

	getOut.push_back(rngData);
		
	cacheOK=cache;
	return 0;
}

void RangeFileFilter::guessFormat(const std::string &s)
{
	vector<string> sVec;
	splitStrsRef(s.c_str(),'.',sVec);

	if(!sVec.size())
		assumedFileFormat=RANGE_FORMAT_ORNL;
	else if(lowercase(sVec[sVec.size()-1]) == "rrng")
		assumedFileFormat=RANGE_FORMAT_RRNG;
	else if(lowercase(sVec[sVec.size()-1]) == "env")
		assumedFileFormat=RANGE_FORMAT_ENV;
	else
		assumedFileFormat=RANGE_FORMAT_ORNL;
}

unsigned int RangeFileFilter::updateRng()
{
	unsigned int uiErr;	
	if((uiErr = rng.open(rngName.c_str(),assumedFileFormat)))
		return uiErr;

	unsigned int nRng = rng.getNumRanges();
	enabledRanges.resize(nRng);
	unsigned int nIon = rng.getNumIons();
	enabledIons.resize(nIon);
	//Turn all ranges to "on" 
	for(unsigned int ui=0;ui<enabledRanges.size(); ui++)
		enabledRanges[ui]=(char)1;
	//Turn all ions to "on" 
	for(unsigned int ui=0;ui<enabledIons.size(); ui++)
		enabledIons[ui]=(char)1;

	return 0;
}

size_t RangeFileFilter::numBytesForCache(size_t nObjects) const
{
	//The byte requirement is input dependant
	return (nObjects*(size_t)IONDATA_SIZE);
}


void RangeFileFilter::getProperties(FilterProperties &p) const
{
	using std::string;
	p.data.clear();
	p.types.clear();
	p.keys.clear();

	//Ensure that the file is specified
	if(!rngName.size())
		return;

	//Should/be loaded
	ASSERT(enabledRanges.size())
	
	string suffix;
	vector<pair<string, string> > strVec;
	vector<unsigned int> types,keys;

	//SET 0
	strVec.push_back(make_pair("File",rngName));
	types.push_back(PROPERTY_TYPE_STRING);
	keys.push_back(0);

	std::string tmpStr;
	if(dropUnranged)
		tmpStr="1";
	else
		tmpStr="0";

	strVec.push_back(make_pair("Drop unranged",tmpStr));
	types.push_back(PROPERTY_TYPE_BOOL);
	keys.push_back(1);

	p.data.push_back(strVec);
	p.types.push_back(types);
	p.keys.push_back(keys);

	keys.clear();
	types.clear();
	strVec.clear();

	//SET 1
	for(unsigned int ui=0;ui<rng.getNumIons(); ui++)
	{
		//Get the ion ID
		stream_cast(suffix,ui);
		strVec.push_back(make_pair(string("IonID " +suffix),rng.getName(ui)));
		types.push_back(PROPERTY_TYPE_STRING);
		keys.push_back(3*ui);


		string str;	
		if(enabledIons[ui])
			str="1";
		else
			str="0";
		strVec.push_back(make_pair(string("Active Ion ")
						+ suffix,str));
		types.push_back(PROPERTY_TYPE_BOOL);
		keys.push_back(3*ui+1);
		
		RGBf col;
		string thisCol;
	
		//Convert the ion colour to a hex string	
		col=rng.getColour(ui);
		genColString((unsigned char)(col.red*255),(unsigned char)(col.green*255),
				(unsigned char)(col.blue*255),255,thisCol);

		strVec.push_back(make_pair(string("colour ") + suffix,thisCol)); 
		types.push_back(PROPERTY_TYPE_COLOUR);
		keys.push_back(3*ui+2);


	}

	p.data.push_back(strVec);
	p.types.push_back(types);
	p.keys.push_back(keys);

	//SET 2->N
	for(unsigned  int ui=0; ui<rng.getNumRanges(); ui++)
	{
		strVec.clear();
		types.clear();
		keys.clear();

		stream_cast(suffix,ui);

		string str;	
		if(enabledRanges[ui])
			str="1";
		else
			str="0";

		strVec.push_back(make_pair(string("Active Rng ")
						+ suffix,str));
		types.push_back(PROPERTY_TYPE_BOOL);
		keys.push_back(KEY_RANGE_ACTIVE);

		strVec.push_back(make_pair(string("Ion ") + suffix, 
					rng.getName(rng.getIonID(ui))));
		types.push_back(PROPERTY_TYPE_STRING);
		keys.push_back(KEY_RANGE_IONID);

		std::pair<float,float > thisRange;
	
		thisRange = rng.getRange(ui);

		string mass;
		stream_cast(mass,thisRange.first);
		strVec.push_back(make_pair(string("Start rng ")+suffix,mass));
		types.push_back(PROPERTY_TYPE_REAL);
		keys.push_back(KEY_RANGE_START);
		
		stream_cast(mass,thisRange.second);
		strVec.push_back(make_pair(string("End rng ")+suffix,mass));
		types.push_back(PROPERTY_TYPE_REAL);
		keys.push_back(KEY_RANGE_END);

		p.types.push_back(types);
		p.data.push_back(strVec);
		p.keys.push_back(keys);
	}

}

bool RangeFileFilter::setProperty(unsigned int set, unsigned int key, 
					const std::string &value, bool &needUpdate)
{
	using std::string;
	needUpdate=false;

	switch(set)
	{
		case 0:
		{
			switch(key)
			{
				case 0:
				{
					//Check to see if the new file can actually be opened
					RangeFile rngTwo;

					if(value != rngName)
					{
						if(!rngTwo.open(value.c_str()))
							return false;
						else
						{
							rng.swap(rngTwo);
							rngName=value;
							needUpdate=true;
						}

					}
					else
						return false;

					break;
				}
				case 1: //Enable/disable unranged dropping
				{
					unsigned int valueInt;
					if(stream_cast(valueInt,value))
						return false;

					if(valueInt ==0 || valueInt == 1)
					{
						if(dropUnranged!= (char)valueInt)
						{
							needUpdate=true;
							dropUnranged=(char)valueInt;
						}
						else
							needUpdate=false;
					}
					else
						return false;		
			
					break;
				}	
				default:
					ASSERT(false);
					break;
			}
			if(needUpdate)
				clearCache();

			break;
		}
		case 1:
		{
			//Each property is stored in the same
			//structured group, each with NUM_ROWS per grouping.
			//The ion ID is simply the row number/NUM_ROWS.
			//similarly the element is given by remainder 
			const unsigned int NUM_ROWS=3;
			unsigned int ionID=key/NUM_ROWS;
			ASSERT(key < NUM_ROWS*rng.getNumIons());
			unsigned int prop = key-ionID*NUM_ROWS;

			switch(prop)
			{
				case 0://Ion name
				{
					
					//only allow english alphabet, upper and lowercase
					for(unsigned int ui=0;ui<value.size();ui++)
					{
						if( value[ui] < 'A'  || value[ui] > 'z' ||
							 (value[ui] > 'Z' && value[ui] < 'a')) 
							return false;
					}
					//TODO: At some point I should work out a 
					//nice way of setting the short and long names.
					rng.setIonShortName(ionID,value);
					rng.setIonLongName(ionID,value);
					needUpdate=true;
					break;
				}
				case 1: //Enable/disable ion
				{
					unsigned int valueInt;
					if(stream_cast(valueInt,value))
						return false;

					if(valueInt ==0 || valueInt == 1)
					{
						if(enabledIons[ionID] != (char)valueInt)
						{
							needUpdate=true;
							enabledIons[ionID]=(char)valueInt;
						}
						else
							needUpdate=false;
					}
					else
						return false;		
			
					break;
				}	
				case 2: //Colour of the ion
				{
					unsigned char r,g,b,a;
					parseColString(value,r,g,b,a);

					RGBf newCol;
					newCol.red=(float)r/255.0f;
					newCol.green=(float)g/255.0f;
					newCol.blue=(float)b/255.0f;

					rng.setColour(ionID,newCol);
					needUpdate=true;
					break;
				}
				default:
					ASSERT(false);
			}

			if(needUpdate)
				clearCache();
			break;
		}
		default:
		{
			unsigned int rangeId=set-2;
			switch(key)
			{
				//Range active
				case KEY_RANGE_ACTIVE:
				{
					unsigned int valueInt;
					if(stream_cast(valueInt,value))
						return false;

					if(valueInt ==0 || valueInt == 1)
					{
						if(enabledRanges[rangeId] != (char)valueInt)
						{
							needUpdate=true;
							enabledRanges[rangeId]=(char)valueInt;
						}
						else
							needUpdate=false;
					}
					else
						return false;		
			
					break;
				}	
				case KEY_RANGE_IONID: 
				{
					unsigned int newID;

					if(stream_cast(newID,value))
						return false;

					if(newID == rng.getIonID(rangeId))
						return false;

					if(newID > rng.getNumRanges())
						return false;

					rng.setIonID(rangeId,newID);
					needUpdate=true;
					break;
				}
				//Range start
				case KEY_RANGE_START:
				{

					//Check for valid data type conversion
					float newMass;
					if(stream_cast(newMass,value))
						return false;

					//Ensure that it has actually changed
					if(newMass == rng.getRange(rangeId).first)
						return false;

					//Attempt to move the range to a new position
					if(!rng.moveRange(rangeId,0,newMass))
						return false;

					needUpdate=true;
					
					break;
				}
				//Range end
				case KEY_RANGE_END:
				{

					//Check for valid data type conversion
					float newMass;
					if(stream_cast(newMass,value))
						return false;

					//Ensure that it has actually changed
					if(newMass == rng.getRange(rangeId).second)
						return false;

					//Attempt to move the range to a new position
					if(!rng.moveRange(rangeId,1,newMass))
						return false;

					needUpdate=true;
					
					break;
				}


			}

			if(needUpdate)
				clearCache();
		}


	}
		
	return true;
}


std::string  RangeFileFilter::getErrString(unsigned int code) const
{
	switch(code)
	{
		case RANGEFILE_ABORT_FAIL:
			return std::string("Ranging aborted by user");
		case RANGEFILE_BAD_ALLOC:
			return std::string("Insufficient memory for range");
	}

	return std::string("BUG(range file filter): Shouldn't see this!");
}

void RangeFileFilter::setFormat(unsigned int format) 
{
	ASSERT(format < RANGE_FORMAT_END_OF_ENUM);

	assumedFileFormat=format;
}

bool RangeFileFilter::writeState(std::ofstream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<" << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\""<<userString << "\"/>"  << endl;

			f << tabs(depth+1) << "<file name=\""<< convertFileStringToCanonical(rngName) << "\"/>"  << endl;
			f << tabs(depth+1) << "<dropunranged value=\""<<(int)dropUnranged<< "\"/>"  << endl;
			f << tabs(depth+1) << "<enabledions>"<< endl;
			for(unsigned int ui=0;ui<enabledIons.size();ui++)
			{
				RGBf col;
				string colourString;
				col = rng.getColour(ui);

				genColString((unsigned char)(col.red*255),(unsigned char)(col.green*255),
						(unsigned char)(col.blue*255),255,colourString);
				f<< tabs(depth+2) << "<ion id=\"" << ui << "\" enabled=\"" 
					<< (int)enabledIons[ui] << "\" colour=\"" << colourString << "\"/>" << endl;
			}
			f << tabs(depth+1) << "</enabledions>"<< endl;

			f << tabs(depth+1) << "<enabledranges>"<< endl;
			
			for(unsigned int ui=0;ui<enabledRanges.size();ui++)
			{
				f<< tabs(depth+2) << "<range id=\"" << ui << "\" enabled=\"" 
					<< (int)enabledRanges[ui] << "\"/>" << endl;
			}
			f << tabs(depth+1) << "</enabledranges>"<< endl;
			
			f << tabs(depth) << "</" << trueName() << ">" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool RangeFileFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
	
	//Retrieve user string
	//==
	if(XMLHelpFwdToElem(nodePtr,"userstring"))
		return false;

	xmlChar *xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	userString=(char *)xmlString;
	xmlFree(xmlString);
	//==

	//Retrieve file name	
	//==
	//Retrieve file name	
	if(XMLHelpFwdToElem(nodePtr,"file"))
		return false;
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"name");
	if(!xmlString)
		return false;
	rngName=(char *)xmlString;
	xmlFree(xmlString);

	//Override the string, as needed
	if( (stateFileDir.size()) &&
		(rngName.size() > 2 && rngName.substr(0,2) == "./") )
	{
		rngName=stateFileDir + rngName.substr(2);
	}

	rngName=convertFileStringToNative(rngName);

	//try using the extension name of the file to guess format
	guessFormat(rngName);

	//Load range file using guessed format
	if(rng.open(rngName.c_str(),assumedFileFormat))
	{
		//If that failed, go to plan B-- Brute force.
		//try all readers
		bool openOK=false;

		for(unsigned int ui=1;ui<RANGE_FORMAT_END_OF_ENUM; ui++)
		{
			if(!rng.open(rngName.c_str(),ui))
			{
				assumedFileFormat=ui;
				openOK=true;
				break;
			}
		}
	
		if(!openOK)
			return false;
	}
	


		
	//==
	
	std::string tmpStr;
	//Retrieve user string
	//==
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"dropunranged","value"))
		return false;
	
	if(tmpStr=="1")
		dropUnranged=true;
	else if(tmpStr=="0")
		dropUnranged=false;
	else
		return false;

	//==
	

	//Retrieve enabled ions	
	//===
	if(XMLHelpFwdToElem(nodePtr,"enabledions"))
		return false;
	xmlNodePtr tmpNode=nodePtr;

	nodePtr=nodePtr->xmlChildrenNode;

	unsigned int ionID;
	bool enabled;
	//By default, turn ions off, but use state file to turn them on
	enabledIons.resize(rng.getNumIons(),false);
	while(!XMLHelpFwdToElem(nodePtr,"ion"))
	{
		//Get ID value
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"id");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;
		xmlFree(xmlString);

		//Check it is streamable
		if(stream_cast(ionID,tmpStr))
			return false;

		if(ionID>= rng.getNumIons()) 
			return false;
		
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"enabled");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;

		if(tmpStr == "0")
			enabled=false;
		else if(tmpStr == "1")
			enabled=true;
		else
			return false;

		enabledIons[ionID]=enabled;
		xmlFree(xmlString);
		
		
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"colour");
		if(!xmlString)
			return false;


		tmpStr=(char *)xmlString;

		unsigned char r,g,b,a;
		if(!parseColString(tmpStr,r,g,b,a))
			return false;
		
		RGBf col;
		col.red=(float)r/255.0f;
		col.green=(float)g/255.0f;
		col.blue=(float)b/255.0f;
		rng.setColour(ionID,col);	
		xmlFree(xmlString);
	}

	//===


	nodePtr=tmpNode;
	//Retrieve enabled ranges
	//===
	if(XMLHelpFwdToElem(nodePtr,"enabledranges"))
		return false;
	tmpNode=nodePtr;

	nodePtr=nodePtr->xmlChildrenNode;

	//By default, turn ranges off (cause there are lots of them), and use state to turn them on
	enabledRanges.resize(rng.getNumRanges(),true);
	unsigned int rngID;
	while(!XMLHelpFwdToElem(nodePtr,"range"))
	{
		//Get ID value
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"id");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;
		xmlFree(xmlString);

		//Check it is streamable
		if(stream_cast(rngID,tmpStr))
			return false;

		if(rngID>= rng.getNumRanges()) 
			return false;
		
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"enabled");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;

		if(tmpStr == "0")
			enabled=false;
		else if(tmpStr == "1")
			enabled=true;
		else
			return false;

		xmlFree(xmlString);
		enabledRanges[rngID]=enabled;
	}
	//===

	return true;
}

void RangeFileFilter::getStateOverrides(std::vector<string> &externalAttribs) const 
{
	externalAttribs.push_back(rngName);
}

int RangeFileFilter::getRefreshBlockMask() const
{
	return STREAM_TYPE_RANGE | STREAM_TYPE_IONS ;
}

int RangeFileFilter::getRefreshEmitMask() const
{
	return STREAM_TYPE_RANGE | STREAM_TYPE_IONS ;
}

void RangeFileFilter::setPropFromRegion(unsigned int method, unsigned int regionID, float newPos)
{
	ASSERT(regionID < rng.getNumRanges());

	unsigned int rangeID = regionID;

	switch(method)
	{
		case REGION_LEFT_EXTEND:
			rng.moveRange(rangeID,false, newPos);
			break;
		case REGION_MOVE:
		{
			std::pair<float,float> limits;
			limits=rng.getRange(rangeID);
			float delta;
			delta = (limits.second-limits.first)/2;
			rng.moveBothRanges(rangeID,newPos-delta,newPos+delta);
			break;
		}
		case REGION_RIGHT_EXTEND:
			rng.moveRange(rangeID,true, newPos);
			break;
		default:
			ASSERT(false);
	}

	clearCache();
}

bool RangeFileFilter::writePackageState(std::ofstream &f, unsigned int format,
			const std::vector<std::string> &valueOverrides, unsigned int depth) const
{
	ASSERT(valueOverrides.size() == 1);

	//Temporarily modify the state of the filter, then call writestate
	string tmpFilename=rngName;


	//override const -- naughty, but we know what we are doing...
	const_cast<RangeFileFilter*>(this)->rngName=valueOverrides[0];
	bool result;
	result=writeState(f,format,depth);
	const_cast<RangeFileFilter*>(this)->rngName=tmpFilename;

	return result;
}

