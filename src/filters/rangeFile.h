#ifndef RANGEFILE_H
#define RANGEFILE_H

#include "../filter.h"


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

		//Types that will be dropped during ::refresh
		int getRefreshBlockMask() const;
		
		//Types that are emitted by filer during ::refrash
		int getRefreshEmitMask() const;

	

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

#endif
