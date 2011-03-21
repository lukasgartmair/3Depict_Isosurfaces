#ifndef COMPPROFILE_H
#define COMPPROFILE_H
#include "../filter.h"

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
		virtual void initFilter(const std::vector<const FilterStreamData *> &dataIn,
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

#endif
