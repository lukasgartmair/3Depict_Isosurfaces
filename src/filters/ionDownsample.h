#ifndef IONDOWNSAMPLE_H
#define IONDOWNSAMPLE_H

#include "../filter.h"
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
	

		//!Should we use a per-species split or not?
		bool perSpecies;	
		//This is filter's enabled ranges. 0 if we don't have a range
		RangeStreamData *rsdIncoming;

		//!Fractions to output for species specific
		std::vector<float> ionFractions;
		std::vector<size_t> ionLimits;
	public:
		IonDownsampleFilter();
		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;
		//!Returns FILTER_TYPE_IONDOWNSAMPLE
		unsigned int getType() const { return FILTER_TYPE_IONDOWNSAMPLE;};
		//!Initialise filter prior to tree propagation
		virtual void initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut);
		
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
	
		//!Get the stream types that will be dropped during ::refresh	
		int getRefreshBlockMask() const;

		//!Get the stream types that will be generated during ::refresh	
		int getRefreshEmitMask() const;	
		
};

#endif
