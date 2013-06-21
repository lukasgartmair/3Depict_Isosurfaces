/*
 *	filter.h - modular data filter implementation 
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

#include "filter.h"

#include "common/stringFuncs.h"
#include "common/translation.h"

#include "wxcomponents.h"

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

#ifdef DEBUG
bool FilterProperty::checkSelfConsistent() const
{	
	//Filter data type must be known
	ASSERT(type < PROPERTY_TYPE_ENUM_END);
	ASSERT(name.size());

	//Check each item is parseable as its own type
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
			float f;
			if(stream_cast(f,data))
				return false;
			break;
		}
		case PROPERTY_TYPE_COLOUR:
		{
			unsigned char r,g,b,a;
			if(!parseColString(data,r,g,b,a))
				return false;

			break;
		}
		case PROPERTY_TYPE_CHOICE:
		{
			if(!isMaybeChoiceString(data))
				return false;
			break;
		}
		case PROPERTY_TYPE_POINT3D:
		{
			Point3D p;
			if(!p.parse(data))
				return false;
			break;
		}
		case PROPERTY_TYPE_INTEGER:
		{
			int i;
			if(stream_cast(i,data))
				return false;
			break;
		}
		default:
		{
			//Check for the possibility that a choice string is mistyped
			// - this has happened. However, its possible to get a false positive
			if(isMaybeChoiceString(data))
			{
				WARN(false, "Possible property not set as choice? It seems to be a choice string...");
			}
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

void IonStreamData::clear()
{
	data.clear();
}

IonStreamData *IonStreamData::cloneSampled(float fraction) const

{
	IonStreamData *out = new IonStreamData;

	out->representationType=representationType;
	out->r=r;
	out->g=g;
	out->b=b;
	out->a=a;
	out->ionSize=ionSize;
	out->valueType=valueType;
	out->parent=parent;
	out->cached=0;


	out->data.reserve(fraction*data.size()*0.9f);

	
	RandNumGen rng;
	rng.initTimer();
	for(size_t ui=0;ui<data.size();ui++)
	{
		if(rng.genUniformDev() < fraction)
			out->data.push_back(data[ui]);	
	}

	return out;
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

void Filter::setPropFromRegion(unsigned int method, unsigned int regionID, float newPos)
{
	//Must overload if using this function.
	ASSERT(false);
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

void Filter::getSelectionDevices(vector<SelectionDevice *> &outD) const
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

bool ProgressData::operator==( const ProgressData &oth) const
{
	if(filterProgress!=oth.filterProgress ||
		(totalProgress!=oth.totalProgress) ||
		(totalNumFilters!=oth.totalNumFilters) ||
		(step!=oth.step) ||
		(maxStep!=oth.maxStep) ||
		(curFilter!=oth.curFilter )||
		(stepName!=oth.stepName) )
		return false;

	return true;
}

const ProgressData &ProgressData::operator=(const ProgressData &oth)
{
	filterProgress=oth.filterProgress;
	totalProgress=oth.totalProgress;
	totalNumFilters=oth.totalNumFilters;
	step=oth.step;
	maxStep=oth.maxStep;
	curFilter=oth.curFilter;
	stepName=oth.stepName;
}

#ifdef DEBUG
extern Filter *makeFilter(unsigned int ui);
extern Filter *makeFilter(const std::string &s);

bool Filter::boolToggleTests()
{
	//Each filter should allow user to toggle any boolean value
	// here we just test the default visible ones
	for(unsigned int ui=0;ui<FILTER_TYPE_ENUM_END;ui++)
	{
		Filter *f;
		f=makeFilter(ui);
		
		FilterPropGroup propGroupOrig;

		//Grab all the default properties
		f->getProperties(propGroupOrig);


		for(size_t ui=0;ui<propGroupOrig.numProps();ui++)
		{
			FilterProperty p;
			p=propGroupOrig.getNthProp(ui);
			//Only consider boolean values 
			if( p.type != PROPERTY_TYPE_BOOL )
				continue;

			//Toggle value to other status
			if(p.data== "0")
				p.data= "1";
			else if (p.data== "1")
				p.data= "0";
			else
			{
				ASSERT(false);
			}


			//set value to toggled version
			bool needUp;
			f->setProperty(p.key,p.data,needUp);

			//Re-get properties to find altered property 
			FilterPropGroup propGroup;
			f->getProperties(propGroup);
			
			FilterProperty p2;
			p2 = propGroup.getPropValue(p.key);
			//Check the property values
			TEST(p2.data == p.data,"displayed bool property can't be toggled");
			
			//Toggle value back to original status
			if(p2.data== "0")
				p2.data= "1";
			else 
				p2.data= "0";
			//re-set value to toggled version
			f->setProperty(p2.key,p2.data,needUp);
		
			//Re-get properties to see if original value is restored
			FilterPropGroup fp2;
			f->getProperties(fp2);
			p = fp2.getPropValue(p2.key);

			TEST(p.data== p2.data,"failed trying to set bool value back to original after toggle");
		}
		delete f;
	}

	return true;
}

bool Filter::helpStringTests()
{
	//Each filter should provide help text for each property
	// here we just test the default visible ones
	for(unsigned int ui=0;ui<FILTER_TYPE_ENUM_END;ui++)
	{
		Filter *f;
		f=makeFilter(ui);


		
		FilterPropGroup propGroup;
		f->getProperties(propGroup);
		for(size_t ui=0;ui<propGroup.numProps();ui++)
		{
			FilterProperty p;
			p=propGroup.getNthProp(ui);
			TEST(p.helpText.size(),"Property help text must not be empty");
		}
		delete f;
	}

	return true;
}
#endif



