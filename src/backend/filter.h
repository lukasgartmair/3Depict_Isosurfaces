/*
 *	filter.h - Data filter header file. 
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
#ifndef FILTER_H
#define FILTER_H

class Filter;
class FilterStreamData;

class ProgressData;
class RangeFileFilter;

#include "APT/ionhit.h"

#include "APT/APTRanges.h"
#include "common/constants.h"

#include "gl/select.h"
#include "gl/drawables.h"

#include "common/voxels.h"

//ifdef inclusion as there is some kind of symbol clash...
#ifdef ATTRIBUTE_PRINTF
	#pragma push_macro("ATTRIBUTE_PRINTF")
	#include <libxml/xmlreader.h>
	#pragma pop_macro(" ATTRIBUTE_PRINTF")
#else
	#include <libxml/xmlreader.h>
	#undef ATTRIBUTE_PRINTF
#endif



const unsigned int NUM_CALLBACK=50000;

const unsigned int IONDATA_SIZE=4;

//!Filter types  -- must match array FILTER_NAMES
enum
{
	FILTER_TYPE_DATALOAD,
	FILTER_TYPE_IONDOWNSAMPLE,
	FILTER_TYPE_RANGEFILE,
	FILTER_TYPE_SPECTRUMPLOT, 
	FILTER_TYPE_IONCLIP,
	FILTER_TYPE_IONCOLOURFILTER,
	FILTER_TYPE_COMPOSITION,
	FILTER_TYPE_BOUNDBOX,
	FILTER_TYPE_TRANSFORM,
	FILTER_TYPE_EXTERNALPROC,
	FILTER_TYPE_SPATIAL_ANALYSIS,
	FILTER_TYPE_CLUSTER_ANALYSIS,
	FILTER_TYPE_VOXELS,
	FILTER_TYPE_IONINFO,
	FILTER_TYPE_ANNOTATION,
	FILTER_TYPE_ENUM_END // not a filter. just end of enum
};

extern const char *FILTER_NAMES[];



//Stream data types. note that bitmasks are occasionally used, so we are limited in
//the number of stream types that we can have.
//Current bitmask using functions are
//	VisController::safeDeleteFilterList
const unsigned int NUM_STREAM_TYPES=5;
const unsigned int STREAMTYPE_MASK_ALL= (1<<(NUM_STREAM_TYPES)) -1;
enum
{
	STREAM_TYPE_IONS=1,
	STREAM_TYPE_PLOT=2,
	STREAM_TYPE_DRAW=4,
	STREAM_TYPE_RANGE=8,
	STREAM_TYPE_VOXEL=16
};


//Keys for binding IDs
enum
{
	BINDING_CYLINDER_RADIUS=1,
	BINDING_SPHERE_RADIUS,
	BINDING_CYLINDER_ORIGIN,
	BINDING_SPHERE_ORIGIN,
	BINDING_PLANE_ORIGIN,
	BINDING_CYLINDER_DIRECTION,
	BINDING_PLANE_DIRECTION,
	BINDING_RECT_TRANSLATE,
	BINDING_RECT_CORNER_MOVE
};

extern const char *STREAM_NAMES[];

//Representations
enum 
{
	//IonStreamData
	ION_REPRESENT_POINTS
	
};

//Representations
enum 
{
	//VoxelStreamData
	VOXEL_REPRESENT_POINTCLOUD,
	VOXEL_REPRESENT_ISOSURF,
	VOXEL_REPRESENT_AXIAL_SLICE,
	VOXEL_REPRESENT_END
};

//Error codes for each of the filters. 
//These can be passed to the getErrString() function for
//a human readable error message
//---

enum
{
	FILE_TYPE_NULL,
	FILE_TYPE_XML,
	FILE_TYPE_POS
};

//---
//

//Forward dec.
class wxPropertyGrid;

//!Return the number of elements in a vector of filter data - i.e. the sum of the number of objects within each stream. Only masked streams (STREAM_TYPE_*) will be counted
size_t numElements(const vector<const FilterStreamData *> &vm, unsigned int mask=STREAMTYPE_MASK_ALL);


void updateFilterPropertyGrid(wxPropertyGrid *g, const Filter *f);

//!Abstract base class for data types that can propagate through filter system
class FilterStreamData
{
	protected:
		unsigned int streamType;
	public:
		//!Parent filter pointer
		const Filter *parent;

		//!Tells us if the filter has cacehd this data for later use. 
		//this is a boolean value, but not declared as such, as there 
		//are debug traps to tell us if this is not set by looking for non-boolean values.
		unsigned int cached;

		FilterStreamData();
		virtual ~FilterStreamData() {}; 
		virtual size_t getNumBasicObjects() const =0;
		//!Returns an integer unique to the clas to identify type (yes rttid...)
		virtual unsigned int getStreamType() const {return streamType;} ;
		//!Free mem held by objects
		virtual void clear()=0;

#ifdef DEBUG
		//Cross-checks fields to determine if (best guess)
		///data structure has a sane combination of values
		virtual void checkSelfConsistent() const {}
#endif

};

class FilterProperty
{
	public:
	//!Human readable short help (tooltip) for each of the keys
	std::string helpText;
	//!Data type for this element
	unsigned int type;
	//!Unique key value for this element
	size_t key;
	//!Property data
	std::string data;
	//!name of property
	std::string name;

	bool checkSelfConsistent() const;
};

class FilterPropGroup
{
	private:
		//!The groupings for the keys in contained properties.
		// First entry is the key ID, second is he group that it belongs to
		std::vector<std::pair<unsigned int,unsigned int> > keyGroupings;
		//!Names for each group of keys.
		std::vector<std::string> groupNames;
		//!Key information
		std::vector<FilterProperty > properties;
		
		size_t groupCount;
	public:
		FilterPropGroup() { groupCount=0;}
		//!Add a property to a grouping
		void addProperty(const FilterProperty &prop, size_t group);


		//!Set the title text for a particular group
		void setGroupTitle(size_t group, const std::string &str);

		//!Obtain the title of the nth group
		void getGroupTitle(size_t group, std::string &str) const ;

		//Obtain a property by its key
		const FilterProperty &getPropValue(size_t key) const;

		//Retrieve the number of groups 
		size_t numGroups() const { return groupCount;};

		bool hasProp(size_t key) const;
		//Get number of keys
		size_t numProps() const { return properties.size(); }
		//Erase all stored  information 
		void clear() 
			{ groupNames.clear();keyGroupings.clear(); properties.clear();
				groupCount=0;}

		//!Grab all properties from the specified group
		void getGroup(size_t group, vector<FilterProperty> &groupVec) const;
		//!Get the nth key
		const FilterProperty &getNthProp(size_t nthProp) const { return properties[nthProp];};

#ifdef DEBUG
		void checkConsistent() const; 
#endif
	
};

//!Point with m-t-c value data
class IonStreamData : public FilterStreamData
{
public:
	IonStreamData();
	void clear();

	//Sample the data vector to the specified fraction
	void sample(float fraction);

	//Duplicate this object, but only using a sampling of the data
	// vector. The retuend object must be deleted by the
	// caller. Cached status is *not* duplicated
	IonStreamData *cloneSampled(float fraction) const;

	size_t getNumBasicObjects() const;
	
	unsigned int representationType;
	float r,g,b,a;
	float ionSize;
	
	//!The name for the type of data -- nominally "mass-to-charge"
	std::string valueType;
	
	//!Apply filter to input data stream	
	std::vector<IonHit> data;
};

//!Point with m-t-c value data
class VoxelStreamData : public FilterStreamData
{
public:
	VoxelStreamData();
	size_t getNumBasicObjects() const { return data.getSize();};
	void clear();
	
	unsigned int representationType;
	float r,g,b,a;
	float splatSize;
	float isoLevel;
	//!Apply filter to input data stream	
	Voxels<float> data;
		
};

//!Plotting data
class PlotStreamData : public FilterStreamData
{
	public:
		PlotStreamData();

		bool save(const char *filename) const; 

		//erase plot contents	
		void clear() {xyData.clear();};
		//Get data size
		size_t getNumBasicObjects() const { return xyData.size();};

		//Use the contained XY data to set hard plot bounds
		void autoSetHardBounds();
		//plot colour
		float r,g,b,a;
		//plot trace mode - enum PLOT_TRACE
		unsigned int plotStyle;
		//plot mode - enum PLOT_MODE
		unsigned int plotMode;

		//use logarithmic mode?
		bool logarithmic;
		//title for data
		std::string dataLabel;
		//Label for X, Y axes
		std::string xLabel,yLabel;

		//!When showing raw XY data, is the data 
		// label a better descriptor of Y than the y-label?
		bool useDataLabelAsYDescriptor;

		//!XY data pairs for plotting curve
		std::vector<std::pair<float,float> > xyData;
		//!Rectangular marked regions
		vector<std::pair<float,float> > regions;
		//!Region colours
		vector<float> regionR,regionB,regionG;

		//!Region indicies from parent region
		vector<unsigned int> regionID;

		//!Region parent filter pointer, used for matching interaction 
		// with region to parent property
		Filter *regionParent;
		//!Parent filter index
		unsigned int index;
		//!Error bar mode
		PLOT_ERROR errDat;
		
		//!Hard bounds that cannot be exceeded when drawing plot
		float hardMinX,hardMaxX,hardMinY,hardMaxY;

#ifdef DEBUG
		//Cross-checks fields to determine if (best guess)
		///data structure has a sane combination of values
		virtual void checkSelfConsistent() const; 
#endif

};

//!Drawable objects, for 3D decoration. 
class DrawStreamData: public FilterStreamData
{
	public:
		//!Vector of 3D objects to draw.
		vector<DrawableObj *> drawables;
		//!constructor
		DrawStreamData(){ streamType=STREAM_TYPE_DRAW;};
		//!Destructor
		~DrawStreamData();
		//!Returns 0, as this does not store basic object types -- i.e. is not for data storage per se.
		size_t getNumBasicObjects() const { return 0; }

		//!Erase the drawing vector, deleting its componets
		void clear();
#ifdef DEBUG
		//Cross-checks fields to determine if (best guess)
		///data structure has a sane combination of values
		void checkSelfConsistent() const; 
#endif
};

//!Range file propagation
class RangeStreamData :  public FilterStreamData
{
	public:
		//!range file filter from whence this propagated. Do not delete[] pointer at all, this class does not OWN the range data
		//it merely provides access to existing data.
		RangeFile *rangeFile;
		//Enabled ranges from source filter
		vector<char> enabledRanges;
		//Enabled ions from source filter 
		vector<char> enabledIons;

		
		//!constructor
		RangeStreamData();
		//!Destructor
		~RangeStreamData() {};
		//!save the range data to a file
		bool save(const char *filename, size_t format) const; 
		//!Returns 0, as this does not store basic object types -- i.e. is not for data storage per se.
		size_t getNumBasicObjects() const { return 0; }

		//!Unlink the pointer
		void clear() { rangeFile=0;enabledRanges.clear();enabledIons.clear();};

#ifdef DEBUG
		void checkSelfConsistent() const ; 
#endif
};

//!Abstract base filter class.
class Filter
{
	protected:

		bool cache, cacheOK;
		static bool strongRandom;

		bool wantMonitor;

		//!Array of the number of streams propagated on last refresh
		//THis is initialised to -1, which is considered invalid
		unsigned int numStreamsLastRefresh[NUM_STREAM_TYPES];
	

		//!temporary console output. Should be only nonzero size immediately after refresh
		vector<string> consoleOutput;
		//!User settable labelling string (human readable ID, etc etc)
		std::string userString;
		//Filter output cache
		std::vector<FilterStreamData *> filterOutputs;
		//!User interaction "Devices" associated with this filter
		std::vector<SelectionDevice *> devices;
	public:	
		Filter() ;
		virtual ~Filter();
		
		//Pure virtual functions
		//====
		//!Duplicate filter contents, excluding cache.
		virtual Filter *cloneUncached() const = 0;

		//!Apply filter to new data, updating cache as needed. Vector of returned pointers must be deleted manually, first checking ->cached.
		virtual unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut,
				ProgressData &progress, bool (*callback)(bool)) =0;
		//!Erase cache
		virtual void clearCache();

		//!Erase any active devices
		virtual void clearDevices(); 

		//!Get (approx) number of bytes required for cache
		virtual size_t numBytesForCache(size_t nObjects) const =0;

		//!return type ID
		virtual unsigned int getType() const=0;

		//!Return filter type as std::string
		virtual std::string typeString()const =0;
		
		//!Get the properties of the filter, in key-value form. First vector is for each output.
		virtual void getProperties(FilterPropGroup &propertyList) const =0;

		//!Set the properties for the nth filter, 
		//!needUpdate tells us if filter output changes due to property set
		//NOte that if you modify a result without clearing the cache,
		//then any downstream decision based upon that may not be noted in an update
		//Take care.
		virtual bool setProperty(unsigned int key,
			const std::string &value, bool &needUpdate) = 0;

		//!Get the human readable error string associated with a particular error code during refresh(...)
		virtual std::string getErrString(unsigned int code) const =0;

		//!Dump state to output stream, using specified format
		/* Current supported formats are STATE_FORMAT_XML
		 */
		virtual bool writeState(std::ostream &f, unsigned int format,
			       	unsigned int depth=0) const = 0;
	
		//!Read state from XML  stream, using xml format
		/* Current supported formats are STATE_FORMAT_XML
		 */
		virtual bool readState(xmlNodePtr& n, const std::string &packDir="") = 0; 
		
		//!Get the bitmask encoded list of filterStreams that this filter blocks from propagation.
		// i.e. if this filterstream is passed to refresh, it is not emitted.
		// This MUST always be consistent with ::refresh for filters current state.
		virtual unsigned int getRefreshBlockMask() const =0; 
		
		//!Get the bitmask encoded list of filterstreams that this filter emits from ::refresh.
		// This MUST always be consistent with ::refresh for filters current state.
		virtual unsigned int getRefreshEmitMask() const = 0;


		//!Mask of filter streams that will not examined by the filter in its computation.
		// note that these *may* be emitted as a pass-through - check emitmask if needed
		virtual unsigned int getRefreshUseMask() const =0;

		//====
	
		//!Return the unique name for a given filter -- DO NOT TRANSLATE	
		string trueName() const { return FILTER_NAMES[getType()];};



		//!Initialise the filter's internal state using limited filter stream data propagation
		//NOTE: CONTENTS MAY NOT BE CACHED.
		virtual void initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut);


		//!Return the XML elements that refer to external entities (i.e. files) which do not move with the XML file
		//Each element is to be referred to using "/" as entity separator, for the first pair element, and the attribute name for the second.
		virtual void getStateOverrides(std::vector<string> &overrides) const {}; 

		//!Enable/disable caching for this filter
		void setCaching(bool enableCache) {cache=enableCache;};
		
		//!Have cached output data?
		bool haveCache() const;
		

		//!Return a user-specified string, or just the typestring if user set string not active
		virtual std::string getUserString() const ;
		//!Set a user-specified string return value is 
		virtual void setUserString(const std::string &str) { userString=str;}; 
		

		//!Modified version of writeState for packaging. By default simply calls writeState.
		//value overrides override the values returned by getStateOverrides. In order.	
		virtual bool writePackageState(std::ostream &f, unsigned int format,
				const std::vector<std::string> &valueOverrides,unsigned int depth=0) const {return writeState(f,format,depth);};
		


		//!Get the selection devices for this filter. MUST be called after refresh()
		/*No checking is done that the selection devices will not interfere with one
		 * another at this level (for example setting two devices on one primitve,
		 * with the same mouse/key bindings). So dont do that.
		 */
		void getSelectionDevices(vector<SelectionDevice *> &devices) const;


		//!Update the output statistics for this filter (num items of streams of each type output)
		void updateOutputInfo(const std::vector<const FilterStreamData *> &dataOut);

		//!Set the binding value for a float
		virtual void setPropFromBinding(const SelectionBinding &b)=0;
		
		//!Set a region update
		virtual void setPropFromRegion(unsigned int method, unsigned int regionID, float newPos);
		
		//!Can this filter perform actions that are potentially a security concern?
		virtual bool canBeHazardous() const {return false;} ;

		//!Get the number of outputs for the specified type during the filter's last refresh
		unsigned int getNumOutput(unsigned int streamType) const;

		//!Get the filter messages from the console -- also deletes the 
		// console messages as a side-effect
		void getConsoleStrings(std::vector<std::string > &v) { v.resize(consoleOutput.size());std::copy(consoleOutput.begin(),consoleOutput.end(),v.begin()); consoleOutput.clear();};

		//!Should filters use strong randomisation (where applicable) or not?
		static void setStrongRandom(bool strongRand) {strongRandom=strongRand;}; 

		//Check to see if the filter needs to be refreshed 
		virtual bool monitorNeedsRefresh() const { return false;};

		//Are we a pure data source  - i.e. can function with no input
		virtual bool isPureDataSource() const { return false;};

		//Can we be a useful filter, even if given no input specified by the Use mask?
		virtual bool isUsefulAsAppend() const { return false;}
#ifdef DEBUG
		//!Run all the registered unit tests for this filter
		virtual bool runUnitTests() { cerr << "No test for " << typeString() << endl; return true;} ;
		//!Is the filter caching?
		bool cacheEnabled() const {return cache;};

		static bool boolToggleTests() ;
		
		static bool helpStringTests() ;
#endif

};


//!Class that tracks the progress of scene updates
class ProgressData
{
	public:
		//!Progress of filter (out of 100) for current filter
		unsigned int filterProgress;
		//!Number of filters (n) that we have proccessed (n out of m filters)
		unsigned int totalProgress;

		//!number of filters which need processing for this update
		unsigned int totalNumFilters;

		//!Current step
		unsigned int step;
		//!Maximum steps
		unsigned int maxStep;
		
		//!Pointer to the current filter that is being updated. Only valid during an update callback
		const Filter *curFilter;

		//!Name of current operation, if specified
		std::string stepName;

		bool operator==(const ProgressData &o) const;
		const ProgressData &operator=(const ProgressData &o);

		void reset() { filterProgress=totalProgress=step=maxStep=0;curFilter=0; stepName.clear();};
		void clock() { filterProgress=step=maxStep=0;curFilter=0;totalProgress++; stepName.clear();};
};



#endif
