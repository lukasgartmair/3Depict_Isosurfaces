#ifndef BOUNDINGBOX_H
#define BOUNDINGBOX_H

#include "../filter.h"
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
					ProgressData &progress, bool (*callback)(void));
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
		bool readState(xmlNodePtr &node, const std::string &packDir);
		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b) {ASSERT(false);} ;
};

#endif
