#ifndef POSLOAD_H
#define POSLOAD_H

#include "../filter.h"

#include "../translation.h"

enum
{
	DATALOAD_FLOAT_FILE,
	DATALOAD_TEXT_FILE
};

enum
{
	DATALOAD_KEY_FILE,
	DATALOAD_KEY_FILETYPE,
	DATALOAD_KEY_SAMPLE,
	DATALOAD_KEY_SIZE,
	DATALOAD_KEY_COLOUR,
	DATALOAD_KEY_IONSIZE,
	DATALOAD_KEY_ENABLED,
	DATALOAD_KEY_VALUELABEL,
	DATALOAD_KEY_SELECTED_COLUMN0,
	DATALOAD_KEY_SELECTED_COLUMN1,
	DATALOAD_KEY_SELECTED_COLUMN2,
	DATALOAD_KEY_SELECTED_COLUMN3,
	DATALOAD_KEY_NUMBER_OF_COLUMNS,
	DATALOAD_KEY_MONITOR
};

class DataLoadFilter:public Filter
{
	protected:
		//!filename from which the ions are being loaded
		std::string ionFilename;

		//!Type of file to open
		unsigned int fileType;

		//!Try our best to guess the file type?
		bool guessType;


		//!Whether to randomly sample dataset during load or not
		bool doSample;

		//!Maximum number of ions to load, if performing sampling
		size_t maxIons;

		//!Default ion colour vars
		float r,g,b,a;

		//!Default ion size (view size)
		float ionSize;
	
		//!Number of columns & type of file
		unsigned int numColumns;

		static const unsigned int INDEX_LENGTH = 4;
		//!index of columns into pos file, if pos data is visualised as a set of float record presented as a table (one line per record)
		unsigned int index[INDEX_LENGTH];//x,y,z,value

		//!Is pos load enabled?
		bool enabled;

		//!Volume restricted load?
		bool volumeRestrict;

		//!volume restriction bounds, not sorted
		BoundCube bound;

		//Epoch timestamp for the mointored file. -1 if invalid
		time_t monitorTimestamp;

		//File size for monitored file
		size_t monitorSize;

		//Do we want to be monitoring
		//the timestamp of the file
		bool wantMonitor;

		//!string to use in error situation, set during ::refresh
		std::string errStr;

		//!String to use to set the value type
		std::string valueLabel;
	public:
		DataLoadFilter();
		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;
		//!Set the source string
		void setFilename(const char *name);
		void setFilename(const std::string &name);
		void guessNumColumns();

		//!Set the filter to either use text or pos as requested,
		// this does not require exposing the file parameter key
		void setFileMode(unsigned int mode);

		//!Get filter type (returns FILTER_TYPE_POSLOAD)
		unsigned int getType() const { return FILTER_TYPE_POSLOAD;};

		//!Get (approx) number of bytes required for cache
		virtual size_t numBytesForCache(size_t nOBjects) const;

		//!Refresh object data
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &getOut, 
				ProgressData &progress, bool (*callback)(bool));

		void updatePosData();

		virtual std::string typeString() const { return std::string(TRANS("Pos Data"));}
		
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
		
		//!Get the block mask for this filter (bitmaks of streams blocked from propagation during ::refresh)
		virtual unsigned int getRefreshBlockMask() const; 
		//!Get the refresh mask for this filter (bitmaks of streams emitted during ::refresh)
		virtual unsigned int getRefreshEmitMask() const; 
		
		//!Get the refresh use mask for this filter (bitmaks of streams possibly used during ::refresh)
		virtual unsigned int getRefreshUseMask() const; 
	
		//!Pos filter has state overrides	
		virtual void getStateOverrides(std::vector<string> &overrides) const; 
		
		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b) {ASSERT(false);} ;
	
		//!Get the label for the chosen value column
		std::string getValueLabel();

		//!Return if we need monitoring or not
		virtual bool monitorNeedsRefresh() const;
		
		//Are we a pure data source  - i.e. can function with no input
		virtual bool isPureDataSource() const { return true;};

#ifdef DEBUG
		bool runUnitTests();
#endif
};

#endif
