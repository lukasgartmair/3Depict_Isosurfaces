#ifndef IONCOLOUR_H
#define IONCOLOUR_H

#include "../filter.h"
#include "../translation.h"

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
				ProgressData &progress, bool (*callback)(bool));

		//!return string naming the human readable type of this class
		virtual std::string typeString() const { return std::string(TRANS("Spectral Colour"));}
		
		
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
		
		//!Get the stream types that will be dropped during ::refresh	
		unsigned int getRefreshBlockMask() const;

		//!Get the stream types that will be generated during ::refresh	
		unsigned int getRefreshEmitMask() const;	
		
		//!Get the stream types that will be used during ::refresh	
		unsigned int getRefreshUseMask() const;	
		
		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b) {ASSERT(false);} ;

#ifdef DEBUG
		bool runUnitTests() ; 
#endif
};
#endif
