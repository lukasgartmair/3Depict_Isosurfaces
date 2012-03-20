#ifndef SPECTRUMPLOT_H
#define SPECTRUMPLOT_H
#include "../filter.h"
#include "../translation.h"


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
			ProgressData &progress, bool (*callback)(bool));
		
		virtual std::string typeString() const { return std::string(TRANS("Spectrum"));};

		//!Get the properties of the filter, in key-value form. First vector is for each output.
		void getProperties(FilterProperties &propertyList) const;

		//!Set the properties for the nth filter. Returns true if prop set OK
		bool setProperty(unsigned int set,unsigned int key, 
				const std::string &value, bool &needUpdate);
		//!Get the human readable error string associated with a particular error code during refresh(...)
		std::string getErrString(unsigned int code) const;

		//!Set the user string.
		void setUserString(const std::string &s);

		//!Dump state to output stream, using specified format
		bool writeState(std::ofstream &f,unsigned int format, unsigned int depth=0) const;
		
		//!Read the state of the filter from XML file. If this
		//fails, filter will be in an undefined state.
		bool readState(xmlNodePtr &node, const std::string &packDir);
	
		//!Get the stream types that will be dropped during ::refresh	
		unsigned int getRefreshBlockMask() const;

		//!Get the stream types that will be generated during ::refresh	
		unsigned int getRefreshEmitMask() const;	

		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b) {ASSERT(false);} ;

#ifdef DEBUG
		bool runUnitTests() ;

#endif
};

#endif

