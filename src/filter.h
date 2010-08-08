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

//This MUST go after the other headers,
//as there is some kind of symbol clash...
#undef ATTRIBUTE_PRINTF
#include <libxml/xmlreader.h>
#undef ATTRIBUTE_PRINTF

const unsigned int FILTER_PROGRESS_UNKNOWN=(unsigned int)-1;

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
	IONCLIP_PRIMITIVE_END, //Not actually a primitive, just end of enum
};

//!Possible primitive types for composition profiles
enum
{
	COMPPROFILE_PRIMITIVE_CYLINDER,
	COMPPROFILE_PRIMITIVE_END, //Not actually a primitive, just end of enum
};

//Stream data types. note that bitmasks are occasionally used, so we are limited in
//the number of stream types that we can have.
//Current bitmask using functions are
//	VisController::safeDeleteFilterList
const unsigned int NUM_STREAM_TYPES=4;
enum
{
	STREAM_TYPE_IONS=1,
	STREAM_TYPE_PLOT=2,
	STREAM_TYPE_DRAW=4,
	STREAM_TYPE_RANGE=8,
};


extern const char *STREAM_NAMES[];

//Representations
enum 
{
	//IonStreamData
	ION_REPRESENT_POINTS,

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
};

//---
//

//Forward dec.
class FilterStreamData;

//!Return the number of elements in a vector of filter data
//TODO: Modify to use an optional bitmask to allow for different types of objects
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

//!Plotting data
class PlotStreamData : public FilterStreamData
{
	public:
		PlotStreamData(){ streamType=STREAM_TYPE_PLOT;
				plotType=PLOT_TYPE_LINES;
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
		//!Parent filter pointer, used for inter-refresh matching.
		const Filter *parent;
		//!Parent filter index
		unsigned int index;
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
		//This points to the owning filter's enabled ranges
		vector<char> *enabledRanges;
		//This points to the owning filter's enabled ions
		vector<char> *enabledIons;

		

		//!constructor
		RangeStreamData(){ rangeFile=0;streamType=STREAM_TYPE_RANGE;};
		//!Destructor
		~RangeStreamData() {};
		//!Returns 0, as this does not store basic object types -- i.e. is not for data storage per se.
		size_t GetNumBasicObjects() const { return 0; }

		//!Unlink the pointer
		void clear() { rangeFile=0;enabledRanges=0;};

};

//FIXME: Lookup how to use static members. cant remember of top of my head. no interwebs.
//float Filter::drawScale;
const float drawScale=10.0f; //Use const for now.

//!Abstract base filter class.
class Filter
{
	protected:

		bool cache, cacheOK;
		unsigned int progress; //Progress

		//!Array of the number of streams propagated on last refresh
		//THis is initialised to -1, which is considered invalid
		unsigned int numStreamsLastRefresh[NUM_STREAM_TYPES];
	
		//!Is this filter active?
		bool enabled;

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

		bool isEnabled() const {return enabled;};

		//!Apply filter to new data, updating cache as needed. Vector of returned pointers must be deleted manually, first checking ->cached.
		virtual unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut,
				unsigned int &progress, bool (*callback)(void)) =0;
		//!Erase cache
		virtual void clearCache();
		//!Get (approx) number of bytes required for cache
		virtual size_t numBytesForCache(size_t nObjects) const =0;

		//!return type ID
		virtual unsigned int getType() const=0;

		//!Return filter type as std::string
		virtual std::string typeString()const =0;

		//!Enable/disable caching for this filter
		void setCaching(bool enableCache) {cache=enableCache;};

		//!Get the filter output data from this filter
		void getOutputData(std::vector<FilterStreamData *> * &getOut);

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
		virtual bool writeState(std::ofstream &f,
				unsigned int format, unsigned int depth=0) const = 0;
		
		//!Read state from XML  stream, using xml format
		/* Current supported formats are STATE_FORMAT_XML
		 */
		virtual bool readState(xmlNodePtr& n) = 0; 


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
		
		//!Can this filter perform actions that are potentially a security concern?
		virtual bool canBeHazardous() const {return false;} ;

		//!Get the number of outputs for the specified type during the filter's last refresh
		unsigned int getNumOutput(unsigned int streamType) const;
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
		//!Get filter type (returns FILTER_TYPE_POSLOAD)
		unsigned int getType() const { return FILTER_TYPE_POSLOAD;};

		//!Get (approx) number of bytes required for cache
		virtual size_t numBytesForCache(size_t nOBjects) const;

		//!Refresh object data
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &getOut, 
				unsigned int &progress, bool (*callback)(void));

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
		//!Read the state of the filter from XML file. If this
		//fails, filter will be in an undefined state.
		bool readState(xmlNodePtr &node);
		
		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b) {ASSERT(false);} ;
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
				unsigned int &progress, bool (*callback)(void));

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
		bool readState(xmlNodePtr &node);
		
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

	public:
		//!range file -- whilst this is public, I am not advocating its use directly..
		RangeFile rng;

		//!Set the format to assume when loading file
		void setFormat(unsigned int format);


		RangeFileFilter(); 
		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;
		void setRangeFileName(std::string filename){rngName=filename;};

		//!Returns -1, as range file cache size is dependant upon input.
		virtual size_t numBytesForCache(size_t nObjects) const;
		//!Returns FILTER_TYPE_RANGEFILE
		unsigned int getType() const { return FILTER_TYPE_RANGEFILE;};
		//update filter
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
					std::vector<const FilterStreamData *> &getOut, 
					unsigned int &progress, bool (*callback)(void));
		//!Force a re-read of the rangefile Return value is range file reading error code
		unsigned int updateRng();
		virtual std::string typeString() const { return std::string("Ranging");};

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
		bool readState(xmlNodePtr &node);
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
			unsigned int &progress, bool (*callback)(void));
		
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
		bool readState(xmlNodePtr &node);
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

	public:
		IonClipFilter() { primitiveType=IONCLIP_PRIMITIVE_PLANE;vectorParams.push_back(Point3D(0.0,0.0,0.0)); vectorParams.push_back(Point3D(0,1.0,0.0));invertedClip=false;showPrimitive=true;};
		
		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;
		
		//!Returns FILTER_TYPE_IONCLIP
		unsigned int getType() const { return FILTER_TYPE_IONCLIP;};

		//!Get approx number of bytes for caching output
		size_t numBytesForCache(size_t nObjects) const;

		//!update filter
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
			std::vector<const FilterStreamData *> &getOut, 
			unsigned int &progress, bool (*callback)(void));
	
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
		bool readState(xmlNodePtr &node);
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
				unsigned int &progress, bool (*callback)(void));

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
		bool readState(xmlNodePtr &node);
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

		//!update filter
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
						std::vector<const FilterStreamData *> &getOut, 
						unsigned int &progress, bool (*callback)(void));
		
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
		bool readState(xmlNodePtr &node);
		//!Set internal property value using a selection binding  
		void setPropFromBinding(const SelectionBinding &b) ;
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
					unsigned int &progress, bool (*callback)(void));
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
		bool readState(xmlNodePtr &node);
		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b) {ASSERT(false);} ;
};


//!Affine transformation filter
class TransformFilter : public Filter
{
	private:
		//!Transform mode (scale, rotate, translate)
		unsigned int transformMode;

		//!Scalar values for transformation (scaling factors, rotation angle )
		vector<float> scalarParams;
		//!Vector values for transformation (translation or rotation vectors)
		vector<Point3D> vectorParams;

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
					unsigned int &progress, bool (*callback)(void));
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
		bool readState(xmlNodePtr &node);
		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b) {ASSERT(false);} ;
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
					unsigned int &progress, bool (*callback)(void));
		
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
		bool readState(xmlNodePtr &node);
		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b) {ASSERT(false);} ;
};

//!Spatial analysis filter
class SpatialAnalysisFilter : public Filter
{
	private:
		//!Which algorithm to use
		unsigned int algorithm;

		//Density analysis vars
		//--------

		//!Stopping criterion
		unsigned int stopMode;

		//!NN stopping criterion (max)
		unsigned int nnMax;

		//!Distance maximum
		float distMax;
		//--------

	public:
		SpatialAnalysisFilter(); 
		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;

		//!Returns -1, as range file cache size is dependant upon input.
		virtual size_t numBytesForCache(size_t nObjects) const;
		//!Returns FILTER_TYPE_SPATIAL_ANALYSIS
		unsigned int getType() const { return FILTER_TYPE_SPATIAL_ANALYSIS;};
		//update filter
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
					std::vector<const FilterStreamData *> &getOut, 
					unsigned int &progress, bool (*callback)(void));
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
		bool readState(xmlNodePtr &node);
		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b) {ASSERT(false);} ;
};

#endif
