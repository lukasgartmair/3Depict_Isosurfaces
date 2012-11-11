/*
 *	filter.h - modular data filter implementation 
 *	Copyright (C) 2012, D Haley 

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

#include "translation.h"

#include <limits>
#include <fstream>
#include <algorithm>
#include <set>

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

	ASSERT(f);
	ASSERT(g);
	
	FilterPropGroup p;
	f->getProperties(p);
#ifdef DEBUG
	//If debugging, test self consistency
	p.checkConsistent();
#endif	
	g->clearKeys();
	g->setNumGroups(p.numGroups());

	
	//Create the keys to add to the grid
	for(size_t ui=0;ui<p.numGroups();ui++)
	{
		vector<FilterProperty> propGrouping;
		p.getGroup(ui,propGrouping);

		for(size_t uj=0;uj<propGrouping.size();uj++)
		{
			g->addKey(propGrouping[uj].name,ui,
				propGrouping[uj].key,
				propGrouping[uj].type,
				propGrouping[uj].data,
				propGrouping[uj].helpText);
		}

		//Set the name that is to be displayed for this grouping
		// of properties
		std::string title;
		p.getGroupTitle(ui,title);
		g->setGroupName(ui,title);
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
				bool (*callback)(bool),unsigned int &progress, size_t offset)
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
				if(!(*callback)(false))
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
			if(!(*callback)(false))
				return 1;
		}

	}
#endif


	return 0;
}
#ifdef DEBUG
bool FilterProperty::checkSelfConsistent() const
{	
	//Filter data type must be known
	ASSERT(type < PROPERTY_TYPE_ENUM_END);
	ASSERT(name.size());

	switch(type)
	{
		case PROPERTY_TYPE_BOOL:
		{
			if(data != "0"  && data != "1")
				return false;
			break;
		}
		case PROPERTY_TYPE_REAL:
		{
			//TODO: Check if numerical
			break;
		}
		case PROPERTY_TYPE_COLOUR:
		{
			unsigned char r,g,b,a;
			if(!parseColString(data,r,g,b,a))
				return false;
		}
		case PROPERTY_TYPE_CHOICE:
		{
			if(!isMaybeChoiceString(data))
				return false;
		}

	}

	return true;
}
#endif


void FilterPropGroup::addProperty(const FilterProperty &prop, size_t group)
{
#ifdef DEBUG
	prop.checkSelfConsistent();
#endif
	if(group >=groupCount)
	{
#ifdef DEBUG
		WARN(!(group > (groupCount+1)),"Appeared to add multiple groups in one go - not wrong, just unusual.");
#endif
		groupCount=std::max(group+1,groupCount);
		groupNames.resize(groupCount,string(""));
	}	

	keyGroupings.push_back(make_pair(prop.key,group));
	properties.push_back(prop);
}

void FilterPropGroup::setGroupTitle(size_t group, const std::string &str)
{
	ASSERT(group <numGroups());
	groupNames[group]=str;
}

void FilterPropGroup::getGroup(size_t targetGroup, vector<FilterProperty> &vec) const
{
	ASSERT(targetGroup<groupCount);
	for(size_t ui=0;ui<keyGroupings.size();ui++)
	{
		if(keyGroupings[ui].second==targetGroup)
		{
			vec.push_back(properties[ui]);
		}
	}

#ifdef DEBUG
	checkConsistent();
#endif
}

void FilterPropGroup::getGroupTitle(size_t group, std::string &s) const
{
	ASSERT(group < groupNames.size());
	s = groupNames[group];	
}

const FilterProperty &FilterPropGroup::getPropValue(size_t key) const
{
	for(size_t ui=0;ui<keyGroupings.size();ui++)
	{
		if(keyGroupings[ui].first == key)
			return properties[ui];
	}

	ASSERT(false);
}

bool FilterPropGroup::hasProp(size_t key) const
{
	for(size_t ui=0;ui<keyGroupings.size();ui++)
	{
		if(keyGroupings[ui].first == key)
			return true;
	}

	return false;
}

#ifdef DEBUG
void FilterPropGroup::checkConsistent() const
{
	ASSERT(keyGroupings.size() == properties.size());
	std::set<size_t> s;

	//Check that each key is unique in the keyGroupings list
	for(size_t ui=0;ui<keyGroupings.size();ui++)
	{
		ASSERT(std::find(s.begin(),s.end(),keyGroupings[ui].first) == s.end());
		s.insert(keyGroupings[ui].first);
	}

	//Check that each key in the properties is also unique
	s.clear();
	for(size_t ui=0;ui<properties.size(); ui++)
	{
		ASSERT(std::find(s.begin(),s.end(),properties[ui].key) == s.end());
		s.insert(properties[ui].key);
	}

	for(size_t ui=0;ui<properties.size(); ui++)
	{
		ASSERT(properties[ui].helpText.size());	
	}

	//Check that the group names are the same as the number of groups
	ASSERT(groupNames.size() ==groupCount);

}
#endif

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

#ifdef DEBUG
void DrawStreamData::checkSelfConsistent() const
{
	//Drawable pointers should be unique
	for(unsigned int ui=0;ui<drawables.size();ui++)
	{
		for(unsigned int uj=0;uj<drawables.size();uj++)
		{
			if(ui==uj)
				continue;
			//Pointers must be unique
			ASSERT(drawables[ui] != drawables[uj]);
		}
	}
}
#endif

PlotStreamData::PlotStreamData() :  r(1.0f),g(0.0f),b(0.0f),a(1.0f),
	plotStyle(PLOT_TRACE_LINES), logarithmic(false) , useDataLabelAsYDescriptor(true), 
	index((unsigned int)-1)
{
	streamType=STREAM_TYPE_PLOT;
	errDat.mode=PLOT_ERROR_NONE;

	hardMinX=hardMinY=-std::numeric_limits<float>::max();
	hardMaxX=hardMaxY=std::numeric_limits<float>::max();
}

void PlotStreamData::autoSetHardBounds()
{
	if(xyData.size())
	{
		hardMinX=std::numeric_limits<float>::max();
		hardMinY=std::numeric_limits<float>::max();
		hardMaxX=-std::numeric_limits<float>::max();
		hardMaxY=-std::numeric_limits<float>::max();

		for(size_t ui=0;ui<xyData.size();ui++)
		{
			hardMinX=std::min(hardMinX,xyData[ui].first);
			hardMinY=std::min(hardMinY,xyData[ui].second);
			hardMaxX=std::max(hardMinX,xyData[ui].first);
			hardMaxY=std::max(hardMaxY,xyData[ui].second);
		}
	}
	else
	{
		hardMinX=hardMinY=-1;
		hardMaxX=hardMaxY=1;
	}
}

bool PlotStreamData::save(const char *filename) const
{

	std::ofstream f(filename);

	if(!f)
		return false;

	bool pendingEndl = false;
	if(xLabel.size())
	{
		f << xLabel;
		pendingEndl=true;
	}
	if(yLabel.size())
	{
		f << "\t" << yLabel;
		pendingEndl=true;
	}

	if(errDat.mode==PLOT_ERROR_NONE)
	{
		if(pendingEndl)
			f << endl;
		for(size_t ui=0;ui<xyData.size();ui++)
			f << xyData[ui].first << "\t" << xyData[ui].second << endl;
	}
	else
	{
		if(pendingEndl)
		{
			f << "\t" << TRANS("Error") << endl;
		}
		else
			f << "\t\t" << TRANS("Error") << endl;
		for(size_t ui=0;ui<xyData.size();ui++)
			f << xyData[ui].first << "\t" << xyData[ui].second << endl;
	}

	return true;
}

#ifdef DEBUG
void PlotStreamData::checkSelfConsistent() const
{
	//Colour vectors should be the same size
	ASSERT(regionR.size() == regionB.size() && regionB.size() ==regionG.size());

	//region's should have a colour and ID vector of same size
	ASSERT(regionID.size() ==regionR.size());

	//log plots should have hardmin >=0
	ASSERT(!(logarithmic && hardMinY < 0) );

	//hardMin should be <=hardMax
	//--
	ASSERT(hardMinX<=hardMaxX);

	ASSERT(hardMinY<=hardMaxY);
	//--

	//If we have regions that can be interacted with, need to have parent
	ASSERT(!(regionID.size() && !regionParent));

	//Must have valid trace style
	ASSERT(plotStyle<PLOT_TRACE_ENDOFENUM);
	//Must have valid error bar style
	ASSERT(errDat.mode<PLOT_ERROR_ENDOFENUM);

	ASSERT(plotMode <PLOT_MODE_ENUM_END);

	//Must set the "index" for this plot 
	ASSERT(index != (unsigned int)-1);
}
#endif

#ifdef DEBUG
void RangeStreamData::checkSelfConsistent() const
{
	if(!rangeFile)
		return;

	ASSERT(rangeFile->getNumIons() == enabledIons.size());

	ASSERT(rangeFile->getNumRanges() == enabledIons.size());

}
#endif

FilterStreamData::FilterStreamData() : parent(0),cached((unsigned int)-1)
{
}

IonStreamData::IonStreamData() : representationType(ION_REPRESENT_POINTS), 
	r(1.0f), g(0.0f), b(0.0f), a(1.0f), 
	ionSize(2.0f), valueType("Mass-to-Charge (amu/e)")
{
	streamType=STREAM_TYPE_IONS;
}

VoxelStreamData::VoxelStreamData() : representationType(VOXEL_REPRESENT_POINTCLOUD),
	r(1.0f),g(0.0f),b(0.0f),a(0.3f), splatSize(2.0f),isoLevel(0.5f)
{
	streamType=STREAM_TYPE_VOXEL;
}

RangeStreamData::RangeStreamData() : rangeFile(0)
{
	streamType = STREAM_TYPE_RANGE;
}

bool RangeStreamData::save(const char *filename, size_t format) const
{
	return !rangeFile->write(filename,format);
}

Filter::Filter() : cache(true), cacheOK(false)
{
	for(unsigned int ui=0;ui<NUM_STREAM_TYPES;ui++)
		numStreamsLastRefresh[ui]=0;
}

Filter::~Filter()
{
    if(cacheOK)
	    clearCache();

    clearDevices();
}

void Filter::clearDevices()
{
	for(unsigned int ui=0; ui<devices.size(); ui++)
	{
		delete devices[ui];
	}
	devices.clear();
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



