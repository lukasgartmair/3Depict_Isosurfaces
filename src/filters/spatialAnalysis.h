#ifndef SPATIALANALYSIS_H
#define SPATIALANALYSIS_H
#include "../filter.h"

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
		virtual void initFilter(const std::vector<const FilterStreamData *> &dataIn,
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
		
		//!Get the stream types that will be dropped during ::refresh	
		int getRefreshBlockMask() const;

		//!Get the stream types that will be generated during ::refresh	
		int getRefreshEmitMask() const;	
		
		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b) {ASSERT(false);} ;
};

#endif
