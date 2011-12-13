/*
 *	filter.h - modular data filter implementation 
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

#include "filter.h"
#include "K3DTree.h"
#include "xmlHelper.h"

#include "colourmap.h"
#include "mathfuncs.h"
#include "rdf.h"

#include "translation.h"

#include <limits>
#include <fstream>
#include <algorithm>


#ifdef _OPENMP
#include <omp.h>
#endif

using std::list;
using std::vector;
using std::string;
using std::pair;
using std::make_pair;




bool Filter::strongRandom= false;

const char *STREAM_NAMES[] = { NTRANS("Ion"),
				NTRANS("Plot"),
				NTRANS("Draw"),
				NTRANS("Range"),
				NTRANS("Voxel")};

//Internal names for each filter
const char *FILTER_NAMES[] = { "posload",
				"iondownsample",
				"rangefile",
				"spectrumplot",
				"ionclip",
				"ioncolour",
				"compositionprofile",
				"boundingbox",
				"transform",
				"externalprog",
				"spatialanalysis",
				"clusteranalysis",
				"voxelise",
				"ioninfo",
				"annotation"
				};

void updateFilterPropertyGrid(wxPropertyGrid *g, const Filter *f)
{

	FilterProperties p;
	f->getProperties(p);


	g->clearKeys();
	g->setNumSets(p.data.size());

	ASSERT(!p.keyNames.size() || p.keyNames.size() == p.data.size());

	//Create the keys for the property grid to do its thing
	for(unsigned int ui=0;ui<p.data.size();ui++)
	{
		if(p.keyNames.size())
			g->setSetName(ui,p.keyNames[ui]);

		for(unsigned int uj=0;uj<p.data[ui].size();uj++)
		{
			g->addKey(p.data[ui][uj].first, ui,p.keys[ui][uj],
					p.types[ui][uj],p.data[ui][uj].second);
		}
	}

	//Let the property grid layout what it needs to
	g->propertyLayout();
}

bool parseXMLColour(xmlNodePtr &nodePtr, float &r,float&g,float&b,float&a)
{
	xmlChar *xmlString;
	std::string tmpStr;
	//--red--
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"r");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(r,tmpStr))
		return false;

	//disallow negative or values gt 1.
	if(r < 0.0f || r > 1.0f)
		return false;
	xmlFree(xmlString);


	//--green--
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"g");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(g,tmpStr))
	{
		xmlFree(xmlString);
		return false;
	}
	
	xmlFree(xmlString);

	//disallow negative or values gt 1.
	if(g < 0.0f || g > 1.0f)
		return false;

	//--blue--
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"b");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(b,tmpStr))
	{
		xmlFree(xmlString);
		return false;
	}
	xmlFree(xmlString);
	
	//disallow negative or values gt 1.
	if(b < 0.0f || b > 1.0f)
		return false;

	//--Alpha--
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"a");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(a,tmpStr))
	{
		xmlFree(xmlString);
		return false;
	}
	xmlFree(xmlString);

	//disallow negative or values gt 1.
	if(a < 0.0f || a > 1.0f)
		return false;

	return true;
}

size_t numElements(const vector<const FilterStreamData *> &v, unsigned int mask)
{
	size_t nE=0;
	for(unsigned int ui=0;ui<v.size();ui++)
	{
		if((v[ui]->getStreamType() & mask))
			nE+=v[ui]->getNumBasicObjects();
	}

	return nE;
}


const RangeFile *getRangeFile(const std::vector<const FilterStreamData*> &dataIn)
{
	for(size_t ui=0;ui<dataIn.size();ui++)
	{
		if(dataIn[ui]->getStreamType() == STREAM_TYPE_RANGE)
			return ((const RangeStreamData*)(dataIn[ui]))->rangeFile;
	}

	ASSERT(false);
}

unsigned int getIonstreamIonID(const IonStreamData *d, const RangeFile *r)
{
	if(d->data.empty())
		return (unsigned int)-1;

	unsigned int tentativeRange;

	tentativeRange=r->getIonID(d->data[0].getMassToCharge());


	//TODO: Currently, we have no choice but to brute force it.
	//In the future, it might be worth storing some data inside the IonStreamData itself
	//and to use that first, rather than try to brute force the result
#ifdef _OPENMP
	bool spin=false;
	#pragma omp parallel for shared(spin)
	for(size_t ui=1;ui<d->data.size();ui++)
	{
		if(spin)
			continue;
		if(r->getIonID(d->data[ui].getMassToCharge()) !=tentativeRange)
			spin=true;
	}

	//Not a range
	if(spin)
		return (unsigned int)-1;

#else
	for(size_t ui=1;ui<d->data.size();ui++)
	{
		if(r->getIonID(d->data[ui].getMassToCharge()) !=tentativeRange)
			return (unsigned int)-1;
	}
#endif

	return tentativeRange;	
}


//!Extend a point data vector using some ion data
unsigned int extendPointVector(std::vector<Point3D> &dest, const std::vector<IonHit> &vIonData,
				bool (*callback)(),unsigned int &progress, size_t offset)
{
	unsigned int curProg=NUM_CALLBACK;
	unsigned int n =offset;
#ifdef _OPENMP
	//Parallel version
	bool spin=false;
	#pragma omp parallel for shared(spin)
	for(size_t ui=0;ui<vIonData.size();ui++)
	{
		if(spin)
			continue;
		dest[offset+ ui] = vIonData[ui].getPosRef();
		
		//update progress every CALLBACK entries
		if(!curProg--)
		{
			#pragma omp critical
			{
			n+=NUM_CALLBACK;
			progress= (unsigned int)(((float)n/(float)dest.size())*100.0f);
			if(!omp_get_thread_num())
			{
				if(!(*callback)())
					spin=true;
			}
			}
		}

	}

	if(spin)
		return 1;
#else

	for(size_t ui=0;ui<vIonData.size();ui++)
	{
		dest[offset+ ui] = vIonData[ui].getPosRef();
		
		//update progress every CALLBACK ions
		if(!curProg--)
		{
			n+=NUM_CALLBACK;
			progress= (unsigned int)(((float)n/(float)dest.size())*100.0f);
			if(!(*callback)())
				return 1;
		}

	}
#endif


	return 0;
}

void IonStreamData::clear()
{
	data.clear();
}

void VoxelStreamData::clear()
{
	data.clear();
}

void DrawStreamData::clear()
{
	for(unsigned int ui=0; ui<drawables.size(); ui++)
		delete drawables[ui];
}

DrawStreamData::~DrawStreamData()
{
	clear();
}

PlotStreamData::PlotStreamData()
{
	streamType=STREAM_TYPE_PLOT;
	plotType=PLOT_TRACE_LINES;
	errDat.mode=PLOT_ERROR_NONE;
	r=1.0,g=0.0,b=0.0,a=1.0;
	logarithmic=false;
	index=(unsigned int)-1;

	hardMinX=hardMinY=-std::numeric_limits<float>::max();
	hardMaxX=hardMaxY=std::numeric_limits<float>::max();
}

Filter::Filter()
{
	cacheOK=false;
	cache=true;
	for(unsigned int ui=0;ui<NUM_STREAM_TYPES;ui++)
		numStreamsLastRefresh[ui]=0;
}

Filter::~Filter()
{
    if(cacheOK)
	    clearCache();
}

void Filter::clearCache()
{
	using std::endl;
	cacheOK=false; 

	//Free mem held by objects	
	for(unsigned int ui=0;ui<filterOutputs.size(); ui++)
	{
		ASSERT(filterOutputs[ui]->cached);
		delete filterOutputs[ui];
	}

	filterOutputs.clear();	
}

bool Filter::haveCache() const
{
	return cacheOK;
}

void Filter::getSelectionDevices(vector<SelectionDevice<Filter> *> &outD)
{
	outD.resize(devices.size());

	std::copy(devices.begin(),devices.end(),outD.begin());

#ifdef DEBUG
	for(unsigned int ui=0;ui<outD.size();ui++)
	{
		ASSERT(outD[ui]);
		//Ensure that pointers coming in are valid, by attempting to perform an operation on them
		vector<std::pair<const Filter *,SelectionBinding> > tmp;
		outD[ui]->getModifiedBindings(tmp);
		tmp.clear();
	}
#endif
}

void Filter::updateOutputInfo(const std::vector<const FilterStreamData *> &dataOut)
{
	//Reset the number ouf output streams to zero
	for(unsigned int ui=0;ui<NUM_STREAM_TYPES;ui++)
		numStreamsLastRefresh[ui]=0;
	
	//Count the different types of output stream.
	for(unsigned int ui=0;ui<dataOut.size();ui++)
	{
		ASSERT(getBitNum(dataOut[ui]->getStreamType()) < NUM_STREAM_TYPES);
		numStreamsLastRefresh[getBitNum(dataOut[ui]->getStreamType())]++;
	}
}

unsigned int Filter::getNumOutput(unsigned int streamType) const
{
	ASSERT(streamType < NUM_STREAM_TYPES);
	return numStreamsLastRefresh[streamType];
}

std::string Filter::getUserString() const
{
	if(userString.size())
		return userString;
	else
		return typeString();
}

void Filter::initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut)
{
	dataOut.resize(dataIn.size());
	std::copy(dataIn.begin(),dataIn.end(),dataOut.begin());
}



