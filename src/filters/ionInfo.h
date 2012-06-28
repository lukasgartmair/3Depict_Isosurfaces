#ifndef IONINFO_H
#define IONINFO_H

#include "../filter.h"
#include "../translation.h"


//!Ion derived information filter, things like volume, composition, etc.
class IonInfoFilter : public Filter
{
	private:
		//!Do we want to know information about the number of ions/composition
		bool wantIonCounts;

		//!Do we want to normalise the ion count data?
		bool wantNormalise;


		//!Parent rangefile in tree
		RangeStreamData *range;

		//!Do we want to know about the volume
		bool wantVolume;

		//!Method for volume computation
		unsigned int volumeAlgorithm;

		//Side length for filled cube volume estimation
		float cubeSideLen;

#ifdef DEBUG
		float lastVolume;
#endif

		//!String for 
		size_t volumeEstimationStringFromID(const char *str) const;

		//Convex hull volume estmation routine.
		//returns 0 on success. global "qh " "object"  will contain
		//the hull. Volume is computed.
		unsigned int convexHullEstimateVol(const vector<const FilterStreamData*> &data, 
							float &vol,bool (*callback)(bool)) const;
	public:
		//!Constructor
		IonInfoFilter();

		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;

		//Perform filter intialisation, for pre-detection of range data
		virtual void initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut);
		
		//!Apply filter to new data, updating cache as needed. Vector
		// of returned pointers must be deleted manually, first checking
		// ->cached.
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
							std::vector<const FilterStreamData *> &dataOut,
							ProgressData &progress, bool (*callback)(bool));
		//!Get (approx) number of bytes required for cache
		size_t numBytesForCache(size_t nObjects) const;

		//!return type ID
		unsigned int getType() const { return FILTER_TYPE_IONINFO;}

		//!Return filter type as std::string
		std::string typeString() const { return std::string(TRANS("Ion info"));};

		//!Get the properties of the filter, in key-value form. First vector is for each output.
		void getProperties(FilterProperties &propertyList) const;

		//!Set the properties for the nth filter,
		//!needUpdate tells us if filter output changes due to property set
		bool setProperty(unsigned int set, unsigned int key,
					const std::string &value, bool &needUpdate);


		void setPropFromBinding( const SelectionBinding &b) {ASSERT(false)};

		//!Get the human readable error string associated with a particular error code during refresh(...)
		std::string getErrString(unsigned int code) const;

		//!Dump state to output stream, using specified format
		/* Current supported formats are STATE_FORMAT_XML
		 */
		bool writeState(std::ofstream &f, unsigned int format,
							unsigned int depth) const;

		//!Read state from XML  stream, using xml format
		/* Current supported formats are STATE_FORMAT_XML
		 */
		bool readState(xmlNodePtr& n, const std::string &packDir="");

		//!Get the bitmask encoded list of filterStreams that this filter blocks from propagation.
		// i.e. if this filterstream is passed to refresh, it is not emitted.
		// This MUST always be consistent with ::refresh for filters current state.
		unsigned int getRefreshBlockMask() const;

		//!Get the bitmask encoded list of filterstreams that this filter emits from ::refresh.
		// This MUST always be consistent with ::refresh for filters current state.
		unsigned int getRefreshEmitMask() const;
		
		//!Get the bitmask encoded list of filterstreams that this filter may use during ::refresh.
		unsigned int getRefreshUseMask() const;

#ifdef DEBUG
		bool runUnitTests();

		//Debugging function only; must be called after refresh. 
		//Returns the last estimation for volume.
		float getLastVolume() { float tmp=lastVolume; lastVolume=0;return tmp; } 
#endif
};


#endif
