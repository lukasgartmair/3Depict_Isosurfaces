/*
 *	rangeFile.cpp - bins ions into different value ranges given an input range file
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
#include "rangeFile.h"

#include "filterCommon.h"

enum
{
	KEY_RANGE_ACTIVE=1,
	KEY_DROP_UNRANGED,
	KEY_RANGE_FILENAME,
	KEY_RANGE_IONID,
	KEY_ENABLE_ALL_IONS, //Limited to ~100K ions
	KEY_ENABLE_ALL_RANGES=100000,
};

const unsigned int NUM_ROWS_ION=3;
const unsigned int NUM_ROWS_RANGE=4;
//!Error codes
enum
{
	RANGEFILE_ABORT_FAIL=1,
	RANGEFILE_BAD_ALLOC
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
		rngData->parent=this;
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
		std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(bool))
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
	{
		d[ui] = new IonStreamData;
		d[ui]->parent=this;
	}

	bool haveDefIonColour=false;
	//GCC complains about this, but this is protected by haveDefIonColour.
	RGBf defIonColour;

	//Try to maintain ion size if possible
	bool haveIonSize,sameSize; // have we set the ionSize?
	float ionSize;
	haveIonSize=false;
	sameSize=true;


	progress.step=1;
	progress.filterProgress=0;
	progress.stepName=TRANS("Pre-Allocate");
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


						if(!(*callback)(false))
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
				delete[] dSizeArr;
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
	progress.stepName=TRANS("Range");
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
					unsigned int rangeID;
					rangeID=rng.getRangeID(it->getMassToCharge());

					//If ion is unranged, then it will have a rangeID of -1
					if(rangeID != (unsigned int)-1)
					{
						unsigned int ionID;
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

						
						if(!(*callback)(false))
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
	rngData->parent=this;
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

bool RangeFileFilter::updateRng()
{
	if(!rng.openGuessFormat(rngName.c_str()))
		return false;

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

	return true;
}

void RangeFileFilter::setRangeData(const RangeFile &rngNew)
{
	rng=rngNew;		
	
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
}

size_t RangeFileFilter::numBytesForCache(size_t nObjects) const
{
	//The byte requirement is input dependant
	return (nObjects*(size_t)IONDATA_SIZE);
}


void RangeFileFilter::getProperties(FilterPropGroup &p) const
{
	using std::string;

	FilterProperty prop;
	//Ensure that the file is specified
	if(!rngName.size())
		return;

	size_t curGroup=0;

	prop.name=TRANS("File");
	prop.type=PROPERTY_TYPE_STRING;
	prop.helpText=TRANS("File to use for range data");
	prop.key=KEY_RANGE_FILENAME;
	prop.data=rngName;

	p.addProperty(prop,curGroup);	

	std::string tmpStr;
	if(dropUnranged)
		tmpStr="1";
	else
		tmpStr="0";

	prop.name=TRANS("Drop unranged");
	prop.type=PROPERTY_TYPE_BOOL;
	prop.helpText=TRANS("Remove unranged points when generating output");
	prop.key=KEY_DROP_UNRANGED;
	prop.data=tmpStr;

	p.addProperty(prop,curGroup);

	curGroup++;
	//---
	//Option to disable/enable all ions
	if(rng.getNumIons())
	{
		string str="1";	
		for(unsigned int uj=0;uj<rng.getNumIons();uj++)
		{
			if(!enabledIons[uj])
			{
				str="0";
				break;
			}
		}
		prop.name=TRANS("All Ions");
		prop.helpText=TRANS("Enable/disable all ions at once");
		prop.data=str;
		prop.type=PROPERTY_TYPE_BOOL;
		prop.key=KEY_ENABLE_ALL_IONS;

		p.addProperty(prop,curGroup);

	
		p.setGroupTitle(curGroup,TRANS("Species"));
		//Ions themselves
		for(unsigned int ui=0;ui<rng.getNumIons(); ui++)
		{
			std::string suffix;
			//Get the ion ID
			stream_cast(suffix,ui);
			prop.name=TRANS("IonID ") +suffix;
			prop.helpText=TRANS("Enable/disable specified ion");
			prop.data=rng.getName(ui);
			prop.type=PROPERTY_TYPE_STRING;
			prop.key=NUM_ROWS_ION*ui+1+KEY_ENABLE_ALL_IONS;

			p.addProperty(prop,curGroup);

			string str;	
			if(enabledIons[ui])
				str="1";
			else
				str="0";
			
			prop.name=TRANS("Active Ion ") + suffix;
			prop.type=PROPERTY_TYPE_BOOL;
			prop.helpText=TRANS("If true, ion is used in output");
			prop.data=str;
			prop.key=NUM_ROWS_ION*ui+2+KEY_ENABLE_ALL_IONS;
		
			p.addProperty(prop,curGroup);

			RGBf col;
			string thisCol;
		
			//Convert the ion colour to a hex string	
			col=rng.getColour(ui);
			genColString((unsigned char)(col.red*255),(unsigned char)(col.green*255),
					(unsigned char)(col.blue*255),255,thisCol);

			prop.name=TRANS("Colour ") + suffix;
			prop.data=thisCol;
			prop.type=PROPERTY_TYPE_COLOUR;
			prop.helpText=TRANS("Colour used to represent ion");
			prop.key=NUM_ROWS_ION*ui+3+KEY_ENABLE_ALL_IONS;

			p.addProperty(prop,curGroup);
		}

	}
	//----


	curGroup++;

	//----
	if(rng.getNumRanges())
	{
		string str="1";	
		for(unsigned int uj=0;uj<rng.getNumRanges();uj++)
		{
			if(!enabledRanges[uj])
			{
				str="0";
				break;
			}
		}
		prop.name=TRANS("All Ranges");
		prop.helpText=TRANS("Enable/disable all ranges");
		prop.data=str;
		prop.type=PROPERTY_TYPE_BOOL;
		prop.key=KEY_ENABLE_ALL_RANGES;
	
		p.addProperty(prop,curGroup);
		
		for(unsigned  int ui=0; ui<rng.getNumRanges(); ui++)
		{
			std::string suffix;
			stream_cast(suffix,ui);

			string str;	
			if(enabledRanges[ui])
				str="1";
			else
				str="0";

			prop.name=TRANS("Active Rng ")+suffix;
			prop.data=str;
			prop.type=PROPERTY_TYPE_BOOL;
			prop.helpText=TRANS("Enable/disable specified range (ion must also be enabled to activiate range)");
			prop.key=KEY_ENABLE_ALL_RANGES+NUM_ROWS_RANGE*ui+1;
			p.addProperty(prop,curGroup);

			prop.name=string(TRANS("Ion ")) + suffix;
			prop.data=rng.getName(rng.getIonID(ui));
			prop.type=PROPERTY_TYPE_STRING;
			prop.helpText=TRANS("Name of ion associate to this range");
			prop.key=KEY_ENABLE_ALL_RANGES+NUM_ROWS_RANGE*ui+2;
			p.addProperty(prop,curGroup);

			std::pair<float,float > thisRange;
			thisRange = rng.getRange(ui);
			string rangeVal;
			stream_cast(rangeVal,thisRange.first);
			
			prop.name=string(TRANS("Start rng "))+suffix;
			prop.data=rangeVal;
			prop.type=PROPERTY_TYPE_REAL;
			prop.helpText=TRANS("Start value for range");
			prop.key=KEY_ENABLE_ALL_RANGES + NUM_ROWS_RANGE*ui +3;
			p.addProperty(prop,curGroup);
		
			stream_cast(rangeVal,thisRange.second);
			prop.name=string(TRANS("End rng "))+suffix;
			prop.data=rangeVal;
			prop.type=PROPERTY_TYPE_REAL;
			prop.helpText=TRANS("Stopping value for range`");
			prop.key=KEY_ENABLE_ALL_RANGES+NUM_ROWS_RANGE*ui+4;
			p.addProperty(prop,curGroup);
		}
		p.setGroupTitle(curGroup,TRANS("Ranges"));
	}
	//----
	
}

bool RangeFileFilter::setProperty(unsigned int key, 
					const std::string &value, bool &needUpdate)
{
	using std::string;
	needUpdate=false;


	switch(key)
	{
		case KEY_RANGE_FILENAME:
		{
			if(value != rngName)
			{
				std::string tmp;
				tmp=rngName;
				rngName=value;
		
				RangeFile tmpRange = rng;

				//Check to see if the new file can actually be opened
				if(!updateRng())
				{
					rngName=tmp;
					rng=tmpRange;

					return false;
				}
				
				needUpdate=true;

			}
			else
				return false;
			
			if(needUpdate)
				clearCache();

			break;
		}
		case KEY_DROP_UNRANGED: //Enable/disable unranged dropping
		{
			unsigned int valueInt;
			if(stream_cast(valueInt,value))
				return false;

			if(valueInt ==0 || valueInt == 1)
			{
				if((int)dropUnranged!= valueInt)
				{
					needUpdate=true;
					dropUnranged=valueInt;
				}
				else
					needUpdate=false;
			}
			else
				return false;		
			
			if(needUpdate)
				clearCache();

			break;
		}	
		case KEY_ENABLE_ALL_RANGES:
		{

			bool allEnable;
			if(value == "1")
				allEnable=true;
			else if ( value == "0")
				allEnable=false;
			else
				return false;

			//set them to the opposite of whatever we have now
			//if any single one needs a change, then we need to
			//update
			for(unsigned int ui=0;ui<rng.getNumRanges();ui++)
			{
				if(enabledRanges[ui]!=allEnable)
					needUpdate=true;
			
				enabledRanges[ui]=allEnable;
			}
			
			if(needUpdate)
				clearCache();

			break;
		}
		case KEY_ENABLE_ALL_IONS:
		{

			bool allEnable;
			if(value == "1")
				allEnable=true;
			else if ( value == "0")
				allEnable=false;
			else
				return false;

			//set them to the opposite of whatever we have now
			//if any single one needs a change, then we need to
			//update
			for(unsigned int ui=0;ui<rng.getNumIons();ui++)
			{
				if(enabledIons[ui]!=allEnable)
					needUpdate=true;
			
				enabledIons[ui]=allEnable;
			}

			if(needUpdate)
				clearCache();
			break;
		}
		default:
		{
			if(key < KEY_ENABLE_ALL_RANGES)
			{
				//structured group, each with NUM_ROWS_ION per grouping.
				//The ion ID is simply the row number/NUM_ROWS_ION.
				//similarly the element is given by remainder 
				unsigned int ionID=((key-1)-KEY_ENABLE_ALL_IONS)/NUM_ROWS_ION;
				ASSERT(ionID < rng.getNumIons());
				ASSERT(key <=NUM_ROWS_ION*rng.getNumIons()+KEY_ENABLE_ALL_IONS);
				unsigned int prop = ((key-1)-KEY_ENABLE_ALL_IONS)-ionID*NUM_ROWS_ION;

				switch(prop)
				{
					case 0://Ion name
					{
						//only allow English alphabet, upper and lowercase, as well as 0->9
						for(unsigned int ui=0;ui<value.size();ui++)
						{

							if(!(isalpha(value[ui]) || isdigit(value[ui])))
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
			}
			else
			{
				unsigned int rangeId=((key-1)-KEY_ENABLE_ALL_RANGES)/NUM_ROWS_RANGE;
				unsigned int prop = ((key-1)-KEY_ENABLE_ALL_RANGES)-rangeId*NUM_ROWS_RANGE;
				switch(prop)
				{
					//Range active
					case 0:
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
					//Ion ID
					case 1: 
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
					case 2:
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
					case 3:
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
	}
		
	return true;
}


std::string  RangeFileFilter::getErrString(unsigned int code) const
{
	switch(code)
	{
		case RANGEFILE_ABORT_FAIL:
			return std::string(TRANS("Ranging aborted by user"));
		case RANGEFILE_BAD_ALLOC:
			return std::string(TRANS("Insufficient memory for range"));
	}

	return std::string("BUG(range file filter): Shouldn't see this!");
}

void RangeFileFilter::setPropFromBinding(const SelectionBinding &b)
{
	ASSERT(false);
}

void RangeFileFilter::setFormat(unsigned int format) 
{
	ASSERT(format < RANGE_FORMAT_END_OF_ENUM);

	assumedFileFormat=format;
}

bool RangeFileFilter::writeState(std::ostream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<" << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\""<< escapeXML(userString) << "\"/>"  << endl;

			f << tabs(depth+1) << "<file name=\""<< escapeXML(convertFileStringToCanonical(rngName)) << "\"/>"  << endl;
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
	if(!rng.openGuessFormat(rngName.c_str()))
		return false;
		
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

unsigned int RangeFileFilter::getRefreshBlockMask() const
{
	return STREAM_TYPE_RANGE | STREAM_TYPE_IONS ;
}

unsigned int RangeFileFilter::getRefreshEmitMask() const
{
	return STREAM_TYPE_RANGE | STREAM_TYPE_IONS ;
}

unsigned int RangeFileFilter::getRefreshUseMask() const
{
	return STREAM_TYPE_IONS ;
}

void RangeFileFilter::setPropFromRegion(unsigned int method, unsigned int regionID, float newPos)
{
	ASSERT(regionID < rng.getNumRanges());

	unsigned int rangeID = regionID;

	switch(method)
	{
		case REGION_MOVE_EXTEND_XMINUS:
			rng.moveRange(rangeID,false, newPos);
			break;
		case REGION_MOVE_TRANSLATE_X:
		{
			std::pair<float,float> limits;
			limits=rng.getRange(rangeID);
			float delta;
			delta = (limits.second-limits.first)/2;
			rng.moveBothRanges(rangeID,newPos-delta,newPos+delta);
			break;
		}
		case REGION_MOVE_EXTEND_XPLUS:
			rng.moveRange(rangeID,true, newPos);
			break;
		default:
			ASSERT(false);
	}

	clearCache();
}

bool RangeFileFilter::writePackageState(std::ostream &f, unsigned int format,
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

#ifdef DEBUG

bool testRanged();
//bool testRangeWithOnOffs();
bool testUnranged();

bool RangeFileFilter::runUnitTests()
{
	if(!testRanged())
		return false;

	return true;
}

bool testUnranged()
{
	return true;
}

bool testRanged()
{
	vector<const FilterStreamData*> streamIn,streamOut;
	//Synthesise data
	//-----
	IonStreamData *d = new IonStreamData;
	
	IonHit h;
	h.setPos(Point3D(1,1,1));

	for(unsigned int ui=0;ui<100; ui++)
	{
		h.setMassToCharge(ui);
		d->data.push_back(h);
	}
	
	streamIn.push_back(d);

	//Now build some range data
	RangeFile rng;

	//Insert *non overlapping* ranges.
	const unsigned int NUM[]={10,14};
	const unsigned int OFFSET[]={0,20};
	string longName,shortName;
	
	RGBf col;
	col.red=col.green=col.blue=1;
	shortName="Bl"; longName="Blahium";
	unsigned int ionID;
	ionID=rng.addIon(shortName,longName,col);
	rng.addRange((float)OFFSET[0],(float)(OFFSET[0]+NUM[0]-1),ionID);

	shortName="Pl"; longName="Palatherum";
	ionID=rng.addIon(shortName,longName,col);
	rng.addRange((float)OFFSET[1],(float)(OFFSET[1]+NUM[1]-1),ionID);

	//-----

	//Run the range filter
	//--
	RangeFileFilter *r = new RangeFileFilter;
	r->setCaching(false);
	r->setRangeData(rng);

	//Run the initialisation stage
	ProgressData prog;
	TEST(!r->refresh(streamIn,streamOut,prog,dummyCallback),"Refresh error code");
	//--
	
	//Run the tests
	//---
	vector<unsigned int> numIons;
	for(unsigned int ui=0; ui<streamOut.size(); ui++)
	{
		if(streamOut[ui]->getStreamType() == STREAM_TYPE_IONS)
		{
			numIons.push_back(streamOut[ui]->getNumBasicObjects());
			const IonStreamData *dI;
			dI = (IonStreamData*)streamOut[ui];
			for(unsigned int uj=0;uj<streamOut.size(); uj++)
			{
				TEST(rng.isRanged(dI->data[uj].getMassToCharge()),
								"Range containment");
			}
		}
	
	}

	//Ion stream output - ranges + unranged
	TEST(numIons.size() == 2, "Ranged ionstream count");
	TEST(std::find(numIons.begin(),numIons.end(),NUM[0]) 
			!= numIons.end(), "ion count test (1)");
	TEST(std::find(numIons.begin(),numIons.end(),NUM[1]) 
			!= numIons.end(), "ion count test (2)");
	
	for(unsigned int uj=0;uj<streamOut.size(); uj++)
		delete streamOut[uj];
	
	delete d;
	delete r;

	return true;
}


#endif
