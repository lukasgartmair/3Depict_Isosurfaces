/*
 *	filter.h - Data filter header file. 
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
#ifndef FILTER_H
#define FILTER_H
class Filter;
class FilterStreamData;
class ProgressData;
class RangeFileFilter;

#include <list>
#include <vector>
#include <string>
#include <utility>
#include <map>

#include "basics.h"
#include "common.h"
#include "APTClasses.h"
#include "mathfuncs.h"
#include "drawables.h"
#include "select.h"
#include "plot.h"
#include "voxels.h"

#include "rdf.h"

//This MUST go after the other headers,
//as there is some kind of symbol clash...
#undef ATTRIBUTE_PRINTF
#include <libxml/xmlreader.h>
#undef ATTRIBUTE_PRINTF

//!Filter types 
enum
{
	FILTER_TYPE_POSLOAD,
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
	FILTER_TYPE_VOXELS,
};

//!Filter state output formats
enum
{
	STATE_FORMAT_XML=1
};

//!Possible primitive types for ion clipping
enum
{
	IONCLIP_PRIMITIVE_SPHERE,
	IONCLIP_PRIMITIVE_PLANE,
	IONCLIP_PRIMITIVE_CYLINDER,
	IONCLIP_PRIMITIVE_AAB, //Axis aligned box

	IONCLIP_PRIMITIVE_END, //Not actually a primitive, just end of enum
};

//!Possible primitive types for composition profiles
enum
{
	COMPPROFILE_PRIMITIVE_CYLINDER,
	COMPPROFILE_PRIMITIVE_END, //Not actually a primitive, just end of enum
};

//!Possible mode for selection of origin in transform filter
enum
{
	TRANSFORM_ORIGINMODE_SELECT,
	TRANSFORM_ORIGINMODE_CENTREBOUND,
	TRANSFORM_ORIGINMODE_MASSCENTRE,
	TRANSFORM_ORIGINMODE_END, // Not actually origin mode, just end of enum
};
//Stream data types. note that bitmasks are occasionally used, so we are limited in
//the number of stream types that we can have.
//Current bitmask using functions are
//	VisController::safeDeleteFilterList
const unsigned int NUM_STREAM_TYPES=5;
enum
{
	STREAM_TYPE_IONS=1,
	STREAM_TYPE_PLOT=2,
	STREAM_TYPE_DRAW=4,
	STREAM_TYPE_RANGE=8,
	STREAM_TYPE_VOXEL=16,
};


extern const char *STREAM_NAMES[];

//Representations
enum 
{
	//IonStreamData
	ION_REPRESENT_POINTS,
	
};

//Representations
enum 
{
	//VoxelStreamData
	VOXEL_REPRESENT_POINTCLOUD,
	VOXEL_REPRESENT_ISOSURF,
	VOXEL_REPRESENT_END,
	
};

//Error codes for each of the filters. 
//These can be passed to the getErrString() function for
//a human readable error message
//---
enum
{
	IONDOWNSAMPLE_ABORT_ERR=1,
	IONDOWNSAMPLE_BAD_ALLOC,
};

enum 
{
	SPECTRUM_BAD_ALLOC=1,
	SPECTRUM_BAD_BINCOUNT,
	SPECTRUM_ABORT_FAIL,
};

enum
{
	RANGEFILE_ABORT_FAIL=1,
	RANGEFILE_BAD_ALLOC,
};

enum 
{
	IONCLIP_CALLBACK_FAIL=1,
	IONCLIP_BAD_ALLOC,
};

enum
{
	COMPPROFILE_ERR_NUMBINS=1,
	COMPPROFILE_ERR_MEMALLOC,
	COMPPROFILE_ERR_ABORT,
};

enum
{
	TRANSFORM_CALLBACK_FAIL=1,
};

enum
{
	EXTERNALPROG_COMMANDLINE_FAIL=1,
	EXTERNALPROG_SETWORKDIR_FAIL,
	EXTERNALPROG_WRITEPOS_FAIL,
	EXTERNALPROG_WRITEPLOT_FAIL,
	EXTERNALPROG_MAKEDIR_FAIL,
	EXTERNALPROG_PLOTCOLUMNS_FAIL,
	EXTERNALPROG_READPLOT_FAIL,
	EXTERNALPROG_READPOS_FAIL,
	EXTERNALPROG_SUBSTITUTE_FAIL,
	EXTERNALPROG_COMMAND_FAIL, 
};

enum
{
	SPATIAL_ANALYSIS_ABORT_ERR=1,
	SPATIAL_ANALYSIS_INSUFFICIENT_SIZE_ERR,
};

enum
{
	FILE_TYPE_NULL,
	FILE_TYPE_XML,
	FILE_TYPE_POS,
};
enum
{
	VOXEL_NORMALISETYPE_NONE,// straight count
	VOXEL_NORMALISETYPE_VOLUME,// density
	VOXEL_NORMALISETYPE_ALLATOMSINVOXEL, // concentration
	VOXEL_NORMALISETYPE_COUNT2INVOXEL,// ratio count1/count2
	VOXEL_NORMALISETYPE_MAX, // keep this at the end so it's a bookend for the last value
};

//---
//

//Forward dec.
class FilterStreamData;

//!Return the number of elements in a vector of filter data
size_t numElements(const vector<const FilterStreamData *> &v);

//!Abstract base class for data types that can propagate through filter system
class FilterStreamData
{
	protected:
		unsigned int streamType;
	public:
		//!Tells us if the filter has cacehd this data for later use. 
		//this is a boolean value, but not declared as such, as there 
		//are debug traps to tell us if this is not set by looking for non-boolean values.
		unsigned int cached;

		FilterStreamData() { cached=(unsigned int) -1; }
		virtual ~FilterStreamData() {}; 
		virtual size_t GetNumBasicObjects() const =0;
		//!Returns an integer unique to the clas to identify type (yes rttid...)
		virtual unsigned int getStreamType() const {return streamType;} ;
		//!Returns true if filter is potentially misuable by third parties if loaded from external source
		//!Free mem held by objects
		virtual void clear()=0;

};

class FilterProperties 
{
	public:
		//Filter property data, one per output, each is value then name
		std::vector<std::vector<std::pair< std::string, std::string > > > data;
		//Data types for each single element
		std::vector<std::vector<unsigned int>  > types;
		
		//!Key numbers for filter. Must be unique per set
		std::vector<std::vector<unsigned int> > keys;
	
};

//!Point with m-t-c value data
class IonStreamData : public FilterStreamData
{
public:
	IonStreamData(){ streamType=STREAM_TYPE_IONS;
		representationType = ION_REPRESENT_POINTS; 
		r=1.0,g=0.0,b=0.0,a=1.0;ionSize=2.0;valueType="Mass-to-Charge";};
	void clear();
	size_t GetNumBasicObjects() const  { return data.size();};
	
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
	VoxelStreamData(){ streamType=STREAM_TYPE_VOXEL;
		representationType = VOXEL_REPRESENT_POINTCLOUD; 
		r=1.0,g=0.0,b=0.0,a=0.3;splatSize=2.0;isoLevel=0.5;};
	size_t GetNumBasicObjects() const { return data.getSize();};
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
		PlotStreamData(){ streamType=STREAM_TYPE_PLOT;
				plotType=PLOT_TYPE_LINES;errDat.mode=PLOT_ERROR_NONE;
				r=1.0,g=0.0,b=0.0,a=1.0;logarithmic=false;};
		void clear() {xyData.clear();};
		size_t GetNumBasicObjects() const { return xyData.size();};
		float r,g,b,a;
		//Type
		unsigned int plotType;
		//use logarithmic mode?
		bool logarithmic;
		//title for data
		std::string dataLabel;
		//Label for X, Y axes
		std::string xLabel,yLabel;
		//!XY data pairs for plotting curve
		std::vector<std::pair<float,float> > xyData;
		//!Rectangular marked regions
		vector<std::pair<float,float> > regions;
		//!Region colours
		vector<float> regionR,regionB,regionG;

		//!Region indicies
		vector<unsigned int> regionID;

		//!Parent filter pointer, used for inter-refresh matching.
		const Filter *parent;
		//!Region parent filter pointer, used for matching interaction with region to parent property
		Filter *regionParent;
		//!Parent filter index
		unsigned int index;
		//!Error bar mode
		PLOT_ERROR errDat;
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
		size_t GetNumBasicObjects() const { return 0; }

		//!Erase the drawing vector, deleting its componets
		void clear();
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

		//!parent filter that spawned this range file. use with EXTREME caution.
		RangeFileFilter *parentFilter;
		

		//!constructor
		RangeStreamData(){ rangeFile=0;streamType=STREAM_TYPE_RANGE;};
		//!Destructor
		~RangeStreamData() {};
		//!Returns 0, as this does not store basic object types -- i.e. is not for data storage per se.
		size_t GetNumBasicObjects() const { return 0; }

		//!Unlink the pointer
		void clear() { rangeFile=0;enabledRanges.clear();enabledIons.clear();};

};

//FIXME: Lookup how to use static members. cant remember of top of my head. no interwebs.
//float Filter::drawScale;
const float drawScale=10.0f; //Use const for now.

//!Abstract base filter class.
class Filter
{
	protected:

		bool cache, cacheOK;
		static bool strongRandom;

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
		//!Duplicate filter contents, excluding cache.
		virtual Filter *cloneUncached() const = 0;

		//!Apply filter to new data, updating cache as needed. Vector of returned pointers must be deleted manually, first checking ->cached.
		virtual unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut,
				ProgressData &progress, bool (*callback)(void)) =0;
		//!Erase cache
		virtual void clearCache();
		//!Get (approx) number of bytes required for cache
		virtual size_t numBytesForCache(size_t nObjects) const =0;

		//!Initialise the filter's internal state using limited filter stream data propagation
		//NOTE: CONTENTS MAY NOT BE CACHED.
		virtual void initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut);

		//!return type ID
		virtual unsigned int getType() const=0;

		//!Return filter type as std::string
		virtual std::string typeString()const =0;

		//!Return the XML elements that refer to external entities (i.e. files) which do not move with the XML file
		//Each element is to be referred to using "/" as entity separator, for the first pair element, and the attribute name for the second.
		virtual void getStateOverrides(std::vector<string> &overrides) const {}; 

		//!Enable/disable caching for this filter
		void setCaching(bool enableCache) {cache=enableCache;};

		//!Have cached output data?
		bool haveCache() const;
		
		//!Get the properties of the filter, in key-value form. First vector is for each output.
		virtual void getProperties(FilterProperties &propertyList) const =0;

		//!Return a user-specified string, or just the typestring if user set string not active
		virtual std::string getUserString() const ;
		//!Set a user-specified string
		virtual void setUserString(const std::string &str) { userString=str;}; 
		

		//!Set the properties for the nth filter, 
		//!needUpdate tells us if filter output changes due to property set
		virtual bool setProperty(unsigned int set, unsigned int key,
			       		const std::string &value, bool &needUpdate) = 0;

		//!Get the human readable error string associated with a particular error code during refresh(...)
		virtual std::string getErrString(unsigned int code) const =0;

		//!Dump state to output stream, using specified format
		/* Current supported formats are STATE_FORMAT_XML
		 */
		virtual bool writeState(std::ofstream &f, unsigned int format,
			       	unsigned int depth=0) const = 0;
	
		//!Modified version of writeState for packaging. By default simply calls writeState.
		//value overrides override the values returned by getStateOverrides. In order.	
		virtual bool writePackageState(std::ofstream &f, unsigned int format,
				const std::vector<std::string> &valueOverrides,unsigned int depth=0) const {return writeState(f,format,depth);};
		
		//!Read state from XML  stream, using xml format
		/* Current supported formats are STATE_FORMAT_XML
		 */
		virtual bool readState(xmlNodePtr& n, const std::string &packDir="") = 0; 


		//!Get the selection devices for this filter. MUST be called after refresh()
		/*No checking is done that the selection devices will not interfere with one
		 * another at this level (for example setting two devices on one primitve,
		 * with the same mouse/key bindings). So dont do that.
		 */
		void getSelectionDevices(vector<SelectionDevice *> &devices);

		//!Update the output informaiton for this filter
		void updateOutputInfo(const std::vector<const FilterStreamData *> &dataOut);

		//!Set the binding value for a float
		virtual void setPropFromBinding(const SelectionBinding &b)=0;
		
		//!Set a region update
		virtual void setPropFromRegion(unsigned int method, unsigned int regionID, float newPos){ASSERT(false);};
		
		//!Can this filter perform actions that are potentially a security concern?
		virtual bool canBeHazardous() const {return false;} ;

		//!Get the number of outputs for the specified type during the filter's last refresh
		unsigned int getNumOutput(unsigned int streamType) const;

		//!Get the filter messages from the console
		void getConsoleStrings(std::vector<std::string > &v) { v.resize(consoleOutput.size());std::copy(consoleOutput.begin(),consoleOutput.end(),v.begin()); consoleOutput.clear();};

		static bool setStrongRandom(bool strongRand) {strongRandom=strongRand;}; 
};


class PosLoadFilter:public Filter
{
	protected:
		//!filename from which the ions are being loaded
		std::string ionFilename;

		//!Maximum number of ions to load, 0 if ion limiting disabled
		size_t maxIons;

		//!Default ion colour vars
		float r,g,b,a;

		//!Default ion size (view size)
		float ionSize;
	
		//!Number of columns & type of file
		int numColumns, fileType;

		//!index of columns into pos file
		static const int INDEX_LENGTH = 4;
		int index[INDEX_LENGTH];//x,y,z,value

		//!Is pos load enabled?
		bool enabled;

		//!Volume restricted load?
		bool volumeRestrict;

		//!volume restriction bounds, not sorted
		BoundCube bound;

	public:
		PosLoadFilter();
		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;
		//!Set the source string
		void setFilename(const char *name);
		void setFilename(const std::string &name);
		void guessNumColumns();
		//!Get filter type (returns FILTER_TYPE_POSLOAD)
		unsigned int getType() const { return FILTER_TYPE_POSLOAD;};

		//!Get (approx) number of bytes required for cache
		virtual size_t numBytesForCache(size_t nOBjects) const;

		//!Refresh object data
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &getOut, 
				ProgressData &progress, bool (*callback)(void));

		void updatePosData();

		virtual std::string typeString() const { return std::string("Pos Data");}
		
		//!Get the properties of the filter, in key-value form. First vector is for each output.
		void getProperties(FilterProperties &propertyList) const;

		//!Set the properties for the nth filter
		bool setProperty(unsigned int set, unsigned int key, const std::string &value, bool &needUpdate);
		
		//!Get the human readable error string associated with a particular error code during refresh(...)
		std::string getErrString(unsigned int code) const;

		//!Dump state to output stream, using specified format
		bool writeState(std::ofstream &f,unsigned int format, 
						unsigned int depth=0) const;
		
		//!write an overridden filename version of the state
		virtual bool writePackageState(std::ofstream &f, unsigned int format,
				const std::vector<std::string> &valueOverrides,unsigned int depth=0) const;
		//!Read the state of the filter from XML file. If this
		//fails, filter will be in an undefined state.
		bool readState(xmlNodePtr &node, const std::string &packDir);
		
	
		//!Pos filter has state overrides	
		virtual void getStateOverrides(std::vector<string> &overrides) const; 
		
		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b) {ASSERT(false);} ;
	
		//!Get the label for the chosen value column
	std::string getValueLabel();
};


//!Random picker filter
class IonDownsampleFilter : public Filter
{
	private:
		RandNumGen rng;
		//When usng fixed number output, maximum to allow out.
		size_t maxAfterFilter;
		//!Allow only a fixed number at output, alternate is random fraction (binomial dist).
		bool fixedNumOut;
		//Fraction to output
		float fraction;

	public:
		IonDownsampleFilter();
		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;
		//!Returns FILTER_TYPE_IONDOWNSAMPLE
		unsigned int getType() const { return FILTER_TYPE_IONDOWNSAMPLE;};
		//!Set mode, fixed num out/approximate out (fraction)
		void setControlledOut(bool controlled) {fixedNumOut=controlled;};

		//!Set the number of ions to generate after the filtering (when using count based fitlering).
		void setFilterCount(size_t nMax) { maxAfterFilter=nMax;};

		//!Get (approx) number of bytes required for cache
		virtual size_t numBytesForCache(size_t nObjects) const;
		//update filter
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &getOut, 
				 ProgressData &progress, bool (*callback)(void));

		//!return string naming the human readable type of this class
		virtual std::string typeString() const { return std::string("Ion Sampler");}
		
		
		//!Get the properties of the filter, in key-value form. First vector is for each output.
		void getProperties(FilterProperties &propertyList) const;

		//!Set the properties for the nth filter
		bool setProperty(unsigned int set, unsigned int key, const std::string &value, bool &needUpdate);
		//!Get the human readable error string associated with a particular error code during refresh(...)
		std::string getErrString(unsigned int code) const;
		
		//!Dump state to output stream, using specified format
		bool writeState(std::ofstream &f,unsigned int format, 
						unsigned int depth=0) const;
		//!Read the state of the filter from XML file. If this
		//fails, filter will be in an undefined state.
		bool readState(xmlNodePtr &node, const std::string &packDir);
		
		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b) {ASSERT(false);} ;
};

//!Range file filter
class RangeFileFilter : public Filter
{
	private:
		std::string rngName;
		//!Vector of chars stating if user has enabled a particular vector or not
		std::vector<char> enabledRanges;
		//!Vector of chars stating if user has enabled a particular Ion or not.
		std::vector<char> enabledIons;

		//!Whether to drop unranged ions in our output
		bool dropUnranged;
		
		//!Assumed file format when loading.
		unsigned int assumedFileFormat;

		void guessFormat(const std::string &s);

	public:
		//!range file -- whilst this is public, I am not advocating its use directly..
		RangeFile rng;

		//!Set the format to assume when loading file
		void setFormat(unsigned int format);
	
		std::vector<char> getEnabledRanges() {return enabledRanges;};
		void setEnabledRanges(vector<char> i) {enabledRanges = i;};


		RangeFileFilter(); 
		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;
		void setRangeFileName(std::string filename){rngName=filename;};

		//!Returns -1, as range file cache size is dependant upon input.
		virtual size_t numBytesForCache(size_t nObjects) const;
		//!Returns FILTER_TYPE_RANGEFILE
		unsigned int getType() const { return FILTER_TYPE_RANGEFILE;};
		
		//!Propagates a range stream data through the filter init stage. Blocks any other range stream datas
		virtual void initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut);
		//update filter
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
					std::vector<const FilterStreamData *> &getOut, 
					ProgressData &progress, bool (*callback)(void));
		//!Force a re-read of the rangefile Return value is range file reading error code
		unsigned int updateRng();
		virtual std::string typeString() const { return std::string("Ranging");};

		//!Get the properties of the filter, in key-value form. First vector is for each output.
		void getProperties(FilterProperties &propertyList) const;

		//!Set the properties for the nth filter
		bool setProperty(unsigned int set,unsigned int key, 
				const std::string &value, bool &needUpdate);
		
		//!Set a region update
		virtual void setPropFromRegion(unsigned int method, unsigned int regionID, float newPos);
		//!Get the human readable error string associated with a particular error code during refresh(...)
		std::string getErrString(unsigned int code) const;
		
		//!Dump state to output stream, using specified format
		bool writeState(std::ofstream &f,unsigned int format,
						unsigned int depth=0) const;
		
		//!Modified version of writeState for packaging. By default simply calls writeState.
		//value overrides override the values returned by getStateOverrides. In order.	
		virtual bool writePackageState(std::ofstream &f, unsigned int format,
				const std::vector<std::string> &valueOverrides,unsigned int depth=0) const;
		//!Read the state of the filter from XML file. If this
		//fails, filter will be in an undefined state.
		bool readState(xmlNodePtr &node, const std::string &packDir);
		
		//!filter has state overrides	
		virtual void getStateOverrides(std::vector<string> &overrides) const; 
		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b) {ASSERT(false);} ;
};

//!Spectrum plot filter
class SpectrumPlotFilter : public Filter
{
	private:
		float minPlot,maxPlot;
		float binWidth;
		bool autoExtrema;
		bool logarithmic;


		//Vector of spectra. Each spectra is comprised of a sorted Y data
		std::vector< std::vector<float > > spectraCache;
		float r,g,b,a;
		unsigned int plotType;
	public:
		SpectrumPlotFilter();
		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;
		//!Returns FILTER_TYPE_SPECTRUMPLOT
		unsigned int getType() const { return FILTER_TYPE_SPECTRUMPLOT;};

		//!Get approx number of bytes for caching output
		size_t numBytesForCache(size_t nObjects) const;

		//!update filter
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
			std::vector<const FilterStreamData *> &getOut, 
			ProgressData &progress, bool (*callback)(void));
		
		virtual std::string typeString() const { return std::string("Spectrum");};

		//!Get the properties of the filter, in key-value form. First vector is for each output.
		void getProperties(FilterProperties &propertyList) const;

		//!Set the properties for the nth filter. Returns true if prop set OK
		bool setProperty(unsigned int set,unsigned int key, 
				const std::string &value, bool &needUpdate);
		//!Get the human readable error string associated with a particular error code during refresh(...)
		std::string getErrString(unsigned int code) const;
		
		//!Dump state to output stream, using specified format
		bool writeState(std::ofstream &f,unsigned int format, unsigned int depth=0) const;
		
		//!Read the state of the filter from XML file. If this
		//fails, filter will be in an undefined state.
		bool readState(xmlNodePtr &node, const std::string &packDir);
		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b) {ASSERT(false);} ;
};

//!Ion spatial clipping filter
class IonClipFilter :  public Filter
{
	protected:
		//!Number explaining basic primitive type
		/* Possible Modes:
		 * Planar clip (origin + normal)
		 * spherical clip (origin + radius)
		 * Cylindrical clip (origin + axis + length)
		 */
		unsigned int primitiveType;

		//!Whether to reverse the clip. True means that the interior is excluded
		bool invertedClip;
		//!Whether to show the primitive or not
		bool showPrimitive;
		//!Vector paramaters for different primitives
		vector<Point3D> vectorParams;
		//!Scalar paramaters for different primitives
		vector<float> scalarParams;
		//Lock the primitive axis during for cylinder?
		bool lockAxisMag; 

	public:
		IonClipFilter() { primitiveType=IONCLIP_PRIMITIVE_PLANE;vectorParams.push_back(Point3D(0.0,0.0,0.0)); vectorParams.push_back(Point3D(0,1.0,0.0));invertedClip=false;showPrimitive=true;lockAxisMag=false;};
		
		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;
		
		//!Returns FILTER_TYPE_IONCLIP
		unsigned int getType() const { return FILTER_TYPE_IONCLIP;};

		//!Get approx number of bytes for caching output
		size_t numBytesForCache(size_t nObjects) const;

		//!update filter
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
			std::vector<const FilterStreamData *> &getOut, 
			ProgressData &progress, bool (*callback)(void));
	
		//!Return human readable name for filter	
		virtual std::string typeString() const { return std::string("Clipping");};

		//!Get the properties of the filter, in key-value form. First vector is for each output.
		void getProperties(FilterProperties &propertyList) const;

		//!Set the properties for the nth filter. Returns true if prop set OK
		bool setProperty(unsigned int set,unsigned int key, 
				const std::string &value, bool &needUpdate);
		//!Get the human readable error string associated with a particular error code during refresh(...)
		std::string getErrString(unsigned int code) const;

		//!Dump state to output stream, using specified format
		bool writeState(std::ofstream &f,unsigned int format, unsigned int depth=0) const;
		
		//!Read the state of the filter from XML file. If this
		//fails, filter will be in an undefined state.
		bool readState(xmlNodePtr &node, const std::string &packDir);
		//!Set internal property value using a selection binding 
		void setPropFromBinding(const SelectionBinding &b);
};

//!Ion colouring filter
class IonColourFilter: public Filter
{
	private:
		//!Colourmap to use
		/* 0 jetColorMap  |  4 positiveColorMap
		 * 1 hotColorMap  |  5 negativeColorMap
		 * 2 coldColorMap |  6 colorMap
		 * 3 blueColorMap |  7 cyclicColorMap
		 * 8 randColorMap |  9 grayColorMap
		 */
		unsigned int colourMap;
		//!map start & end (spectrum value to align start and end of map to)
		float mapBounds[2];

		//!Number of unique colours to generate, max 256
		unsigned int nColours;

		//!Should we display the colour bar?
		bool showColourBar;

		DrawColourBarOverlay* makeColourBar() const;
	public:
		IonColourFilter();
		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;
		//!Returns FILTER_TYPE_IONCOLOURFILTER
		unsigned int getType() const { return FILTER_TYPE_IONCOLOURFILTER;};
		//!Get (approx) number of bytes required for cache
		virtual size_t numBytesForCache(size_t nObjects) const;
		//update filter
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &getOut, 
				ProgressData &progress, bool (*callback)(void));

		//!return string naming the human readable type of this class
		virtual std::string typeString() const { return std::string("Spectral Colour");}
		
		
		//!Get the properties of the filter, in key-value form. First vector is for each output.
		void getProperties(FilterProperties &propertyList) const;

		//!Set the properties for the nth filter
		bool setProperty(unsigned int set, unsigned int key, const std::string &value, bool &needUpdate);
		//!Get the human readable error string associated with a particular error code during refresh(...)
		std::string getErrString(unsigned int code) const;

		//!Dump state to output stream, using specified format
		bool writeState(std::ofstream &f,unsigned int format, 
						unsigned int depth=0) const;
		
		//!Read the state of the filter from XML file. If this
		//fails, filter will be in an undefined state.
		bool readState(xmlNodePtr &node, const std::string &packDir);
		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b) {ASSERT(false);} ;
};

//!Filter that does composition profiles for various primitives
class CompositionProfileFilter : public Filter
{
	private:

		//!Number explaining basic primitive type
		/* Possible Modes:
		 * Cylindrical (origin + axis + length)
		 */
		unsigned int primitiveType;
		//!Whether to show the primitive or not
		bool showPrimitive;
		//Lock the primitive axis during for cylinder?
		bool lockAxisMag; 
		//!Vector paramaters for different primitives
		vector<Point3D> vectorParams;
		//!Scalar paramaters for different primitives
		vector<float> scalarParams;

		//!Frequency or percentile mode (0 - frequency; 1-normalised (ion freq))
		bool normalise;
		//!Stepping mode - fixed width or fixed number of bins
		unsigned int stepMode;
		//!Use fixed bins?
		bool fixedBins;
		
		//!number of bins (if using fixed bins)
		unsigned int nBins;
		//!Width of each bin (if using fixed wdith)
		float binWidth;
		
		//Plotting stuff
		//Vector of spectra. Each spectra is comprised of a sorted Y data
		std::vector< std::vector<float > > spectraCache;
		float r,g,b,a;
		unsigned int plotType;
	
		PLOT_ERROR errMode;

		//!Do we have a range file above us in our filter tree? This is set by ::initFilter
		bool haveRangeParent;
		
		//!internal function for binning an ion dependant upon range data
		void binIon(unsigned int targetBin, const RangeStreamData* rng, const std::map<unsigned int,unsigned int> &ionIDMapping,
			vector<vector<size_t> > &frequencyTable, float massToCharge) const;

	public:
		CompositionProfileFilter();
		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;
		//!Returns FILTER_TYPE_COMPOSITION
		unsigned int getType() const { return FILTER_TYPE_COMPOSITION;};

		//!Get approx number of bytes for caching output
		size_t numBytesForCache(size_t nObjects) const;

		//!Initialise filter, check for upstream range
		void initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut);
		//!update filter
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
						std::vector<const FilterStreamData *> &getOut, 
						ProgressData &progress, bool (*callback)(void));
		
		virtual std::string typeString() const { return std::string("Comp. Prof.");};

		//!Get the properties of the filter, in key-value form. First vector is for each output.
		void getProperties(FilterProperties &propertyList) const;

		//!Set the properties for the nth filter. Returns true if prop set OK
		bool setProperty(unsigned int set,unsigned int key, 
				const std::string &value, bool &needUpdate);
		//!Get the human readable error string associated with a particular error code during refresh(...)
		std::string getErrString(unsigned int code) const;
		
		//!Dump state to output stream, using specified format
		bool writeState(std::ofstream &f,unsigned int format, 
						unsigned int depth=0) const;
		//!Read the state of the filter from XML file. If this
		//fails, filter will be in an undefined state.
		bool readState(xmlNodePtr &node, const std::string &packDir);
		//!Set internal property value using a selection binding  
		void setPropFromBinding(const SelectionBinding &b) ;
};

//!Filter that does voxelisation for various primitives (copid from CompositionFilter)
class VoxeliseFilter : public Filter
{
private:
	const static size_t INDEX_LENGTH = 3;

	//Enabled ions for numerator/denom
	vector<char> enabledIons[2];

	//!Stepping mode - fixed width or fixed number of bins
	bool fixedWidth;
	
	//!number of bins (if using fixed bins)
	unsigned long long nBins[INDEX_LENGTH];
	//!Width of each bin (if using fixed wdith)
	Point3D binWidth;
	//!boundcube for the input data points
	BoundCube bc;
	
	//!density-based or count-based	
	int normaliseType;
	bool numeratorAll, denominatorAll;
	//This is filter's enabled ranges
	RangeStreamData *rsdIncoming;
	
	float r,g,b,a;

	//!3D Point Representation size
	float splatSize;

	//!Isosurface level
	float isoLevel;
	//!Default output representation mode
	unsigned int representation;
public:
	VoxeliseFilter();
	//!Duplicate filter contents, excluding cache.
	Filter *cloneUncached() const;
	
	//!Get approx number of bytes for caching output
	size_t numBytesForCache(size_t nObjects) const;

	unsigned int getType() const { return FILTER_TYPE_VOXELS;};
	
	void initFilter(const std::vector<const FilterStreamData *> &dataIn,
					std::vector<const FilterStreamData *> &dataOut);
	//!update filter
	unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
						 std::vector<const FilterStreamData *> &getOut, 
						 ProgressData &progress, bool (*callback)(void));
	
	virtual std::string typeString() const { return std::string("Voxelisation");};

	//!Get the human-readable options for the normalisation, based upon enum 
	std::string getNormaliseTypeString(int type) const;
	//!Get the human-readable options for the visual representation (enum)
	std::string getRepresentTypeString(int type) const;
	
	//!Get the properties of the filter, in key-value form. First vector is for each output.
	void getProperties(FilterProperties &propertyList) const;
	
	//!Set the properties for the nth filter. Returns true if prop set OK
	bool setProperty(unsigned int set,unsigned int key, 
					 const std::string &value, bool &needUpdate);
	//!Get the human readable error string associated with a particular error code during refresh(...)
	std::string getErrString(unsigned int code) const;
	
	//!Dump state to output stream, using specified format
	bool writeState(std::ofstream &f,unsigned int format, 
					unsigned int depth=0) const;
	//!Read the state of the filter from XML file. If this
	//fails, filter will be in an undefined state.
	bool readState(xmlNodePtr &node, const std::string &packDir);
	//!Set internal property value using a selection binding  
	void setPropFromBinding(const SelectionBinding &b) ;
	
	VoxeliseFilter operator=(const VoxeliseFilter &v);
	
	//!calculate the widths of the bins in 3D
	void calculateWidthsFromNumBins(Point3D &widths, unsigned long long *nb) {
		Point3D low, high;
		bc.getBounds(low, high);
		for (int i = 0; i < 3; i++) {
			widths[i] = (high[i] - low[i])/(float)nb[i];
		}
	}
	//!set the number of the bins in 3D
	void calculateNumBinsFromWidths(Point3D &widths, unsigned long long *nb) {
		Point3D low, high;
		bc.getBounds(low, high);
		for (int i = 0; i < 3; i++) {
			if (low[i] == high[i]) nb[i] = 1;
			else nb[i] = (unsigned long long)((high[i] - low[i])/(float)widths[i]) + 1;
		}
	}
};

//!Bounding box filter
class BoundingBoxFilter : public Filter
{
	private:
		//!visibility
		bool isVisible;
		//!Should tick positions be computed using fixed tick counts or spacing?
		bool fixedNumTicks;
		//!Number of ticks (XYZ) if using fixed num ticks
		unsigned int numTicks[3];
		//!Spacing of ticks (XYZ) if using fixed spacing ticks
		float tickSpacing[3];
		//!Font size
		unsigned int fontSize;

		//!Line colour
		float rLine,gLine,bLine,aLine;
		//!Line width 
		float lineWidth;
	
		//!Use 3D text?
		bool threeDText;
	public:
		BoundingBoxFilter(); 
		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;

		//!Returns -1, as range file cache size is dependant upon input.
		virtual size_t numBytesForCache(size_t nObjects) const;
		//!Returns FILTER_TYPE_RANGEFILE
		unsigned int getType() const { return FILTER_TYPE_BOUNDBOX;};
		//update filter
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
					std::vector<const FilterStreamData *> &getOut, 
					ProgressData &progress, bool (*callback)(void));
		//!Force a re-read of the rangefile Return value is range file reading error code
		unsigned int updateRng();
		virtual std::string typeString() const { return std::string("Bound box");};

		//!Get the properties of the filter, in key-value form. First vector is for each output.
		void getProperties(FilterProperties &propertyList) const;

		//!Set the properties for the nth filter
		bool setProperty(unsigned int set,unsigned int key, 
				const std::string &value, bool &needUpdate);
		//!Get the human readable error string associated with a particular error code during refresh(...)
		std::string getErrString(unsigned int code) const;
		
		//!Dump state to output stream, using specified format
		bool writeState(std::ofstream &f,unsigned int format,
						unsigned int depth=0) const;
		//!Read the state of the filter from XML file. If this
		//fails, filter will be in an undefined state.
		bool readState(xmlNodePtr &node, const std::string &packDir);
		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b) {ASSERT(false);} ;
};


//!Affine transformation filter
class TransformFilter : public Filter
{
	private:
		//!Transform mode (scale, rotate, translate)
		unsigned int transformMode;

		//!Show origin if needed;
		bool showOrigin;
		//!Mode for selection of origin for transform
		unsigned int originMode;
		//!Scalar values for transformation (scaling factors, rotation angle )
		vector<float> scalarParams;
		//!Vector values for transformation (translation or rotation vectors)
		vector<Point3D> vectorParams;

		//!Should we show the origin primitive markers?
		bool showPrimitive;
			
		std::string getOriginTypeString(unsigned int i) const;

		//!Make the marker sphere
		DrawStreamData* makeMarkerSphere(SelectionDevice* &s) const;
	public:
		TransformFilter(); 
		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;

		//!Returns -1, as range file cache size is dependant upon input.
		virtual size_t numBytesForCache(size_t nObjects) const;
		//!Returns FILTER_TYPE_TRANSFORM
		unsigned int getType() const { return FILTER_TYPE_TRANSFORM;};
		//update filter
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
					std::vector<const FilterStreamData *> &getOut, 
					ProgressData &progress, bool (*callback)(void));
		//!Force a re-read of the rangefile Return value is range file reading error code
		unsigned int updateRng();
		virtual std::string typeString() const { return std::string("Spat. Transform");};

		//!Get the properties of the filter, in key-value form. First vector is for each output.
		void getProperties(FilterProperties &propertyList) const;

		//!Set the properties for the nth filter
		bool setProperty(unsigned int set,unsigned int key, 
				const std::string &value, bool &needUpdate);
		//!Get the human readable error string associated with a particular error code during refresh(...)
		std::string getErrString(unsigned int code) const;
		
		//!Dump state to output stream, using specified format
		bool writeState(std::ofstream &f,unsigned int format,
						unsigned int depth=0) const;
		//!Read the state of the filter from XML file. If this
		//fails, filter will be in an undefined state.
		bool readState(xmlNodePtr &node, const std::string &packDir);
		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b);

};

//!External program filter
class ExternalProgramFilter : public Filter
{

		//!The command line strings; prior to expansion 
		std::string commandLine;

		//!Working directory for program
		std::string workingDir;

		//!Always cache output from program
		bool alwaysCache;
		//!Erase generated input files for ext. program after running?
		bool cleanInput;

	public:
		//!As this launches external programs, this could be misused.
		bool canBeHazardous() const {return commandLine.size();}

		ExternalProgramFilter();
		virtual ~ExternalProgramFilter(){};

		Filter *cloneUncached() const;
		//!Returns cache size as a function fo input
		virtual size_t numBytesForCache(size_t nObjects) const;
		
		//!Returns FILTER_TYPE_EXTERNALPROC
		unsigned int getType() const { return FILTER_TYPE_EXTERNALPROC;};
		//update filter
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
					std::vector<const FilterStreamData *> &getOut, 
					ProgressData &progress, bool (*callback)(void));
		
		virtual std::string typeString() const { return std::string("Ext. Program");};

		//!Get the properties of the filter, in key-value form. First vector is for each output.
		void getProperties(FilterProperties &propertyList) const;

		//!Set the properties for the nth filter
		bool setProperty(unsigned int set,unsigned int key, 
				const std::string &value, bool &needUpdate);
		//!Get the human readable error string associated with a particular error code during refresh(...)
		std::string getErrString(unsigned int code) const;
		
		//!Dump state to output stream, using specified format
		bool writeState(std::ofstream &f,unsigned int format,
						unsigned int depth=0) const;
		//!Read the state of the filter from XML file. If this
		//fails, filter will be in an undefined state.
		bool readState(xmlNodePtr &node, const std::string &packDir);
		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b) {ASSERT(false);} ;
};

//!Spatial analysis filter
class SpatialAnalysisFilter : public Filter
{
	private:
		//!Colour to use for output plots
		float r,g,b,a;

		//!Which algorithm to use
		unsigned int algorithm;

		//!Stopping criterion
		unsigned int stopMode;

		//!NN stopping criterion (max)
		unsigned int nnMax;

		//!Distance maximum
		float distMax;

		//!Do we have range data to use (is nonzero)
		bool haveRangeParent;
		//!The names of the incoming ions
		std::vector<std::string > ionNames;
		//!Are the sources/targets enabled for a  particular incoming range?
		std::vector<bool> ionSourceEnabled,ionTargetEnabled;

		//RDF specific params
		//--------
		//RDF bin count
		unsigned int numBins;

		//!Optional convex hull reduction
		bool excludeSurface;

		//!Surface reduction distance (convex hull)
		float reductionDistance;


		//--------

	public:
		SpatialAnalysisFilter(); 
		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;

		//!Initialise filter prior to tree propagation
		void initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut);

		//!Returns -1, as range file cache size is dependant upon input.
		virtual size_t numBytesForCache(size_t nObjects) const;
		//!Returns FILTER_TYPE_SPATIAL_ANALYSIS
		unsigned int getType() const { return FILTER_TYPE_SPATIAL_ANALYSIS;};
		//update filter
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
					std::vector<const FilterStreamData *> &getOut, 
					ProgressData &progress, bool (*callback)(void));
		//!Get the type string  for this fitler
		virtual std::string typeString() const { return std::string("Spat. Analysis");};

		//!Get the properties of the filter, in key-value form. First vector is for each output.
		void getProperties(FilterProperties &propertyList) const;

		//!Set the properties for the nth filter
		bool setProperty(unsigned int set,unsigned int key, 
				const std::string &value, bool &needUpdate);
		//!Get the human readable error string associated with a particular error code during refresh(...)
		std::string getErrString(unsigned int code) const;
		
		//!Dump state to output stream, using specified format
		bool writeState(std::ofstream &f,unsigned int format,
						unsigned int depth=0) const;
		//!Read the state of the filter from XML file. If this
		//fails, filter will be in an undefined state.
		bool readState(xmlNodePtr &node, const std::string &packDir);
		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b) {ASSERT(false);} ;
};


//!Class that tracks the progress of scene updates
class ProgressData
{
	public:
		//!Progress of filter (out of 100) for current filter
		unsigned int filterProgress;
		//!Number of filters that we have proccessed (n out of m filters)
		unsigned int totalProgress;
		//!Current step
		unsigned int step;
		//!Maximum steps
		unsigned int maxStep;
		
		//!Pointer to the current filter that is being updated. Only valid during an update callback
		const Filter *curFilter;

		//!Name of current operation, if specified
		std::string stepName;

		void reset() { filterProgress=totalProgress=step=maxStep=0;curFilter=0; stepName.clear();};
		void clock() { filterProgress=step=maxStep=0;curFilter=0;totalProgress++; stepName.clear();};
};

#endif
