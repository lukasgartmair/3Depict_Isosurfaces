#ifndef EXTERNALPROGRAM_H
#define EXTERNALPROGRAM_H

#include "../filter.h"
#include "../translation.h"
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
		bool canBeHazardous() const {return true;}

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
					ProgressData &progress, bool (*callback)(bool));
		
		virtual std::string typeString() const { return std::string(TRANS("Ext. Program"));};

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
		
		//!Get the stream types that will be dropped during ::refresh	
		unsigned int getRefreshBlockMask() const;

		//!Get the stream types that will be generated during ::refresh	
		unsigned int getRefreshEmitMask() const;	
		
		//!Get the stream types possibly used during ::refresh	
		unsigned int getRefreshUseMask() const;	
		
		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b) {ASSERT(false);} ;

#ifdef DEBUG
		bool runUnitTests();
#endif
};

#endif
