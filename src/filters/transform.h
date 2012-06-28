#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "../filter.h"
#include "../translation.h"

//!Affine transformation filter
class TransformFilter : public Filter
{
	private:
		//!Transform mode (scale, rotate, translate)
		unsigned int transformMode;

		//!Show origin if needed;
		bool showOrigin;
		//!Mode for selection of origin for transform
		unsigned int originMode;
		//!Mode for particular noise type
		unsigned int noiseType;
		//!Scalar values for transformation (scaling factors, rotation angle )
		std::vector<float> scalarParams;
		//!Vector values for transformation (translation or rotation vectors)
		std::vector<Point3D> vectorParams;

		//!Should we show the origin primitive markers?
		bool showPrimitive;
			
		std::string getOriginTypeString(unsigned int i) const;
		
		std::string getNoiseTypeString(unsigned int i) const;

		//!Make the marker sphere
		DrawStreamData* makeMarkerSphere(SelectionDevice<Filter>* &s) const;
		
		//!random number generator
		RandNumGen randGen;
	public:
		TransformFilter(); 
		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;

		//!Returns -1, as range file cache size is dependant upon input.
		virtual size_t numBytesForCache(size_t nObjects) const;

		
		//!Returns FILTER_TYPE_TRANSFORM
		unsigned int getType() const { return FILTER_TYPE_TRANSFORM;};
		//update filter
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
					std::vector<const FilterStreamData *> &getOut, 
					ProgressData &progress, bool (*callback)(bool));
		//!Force a re-read of the rangefile Return value is range file reading error code
		unsigned int updateRng();
		virtual std::string typeString() const { return std::string(TRANS("Ion. Transform"));};

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
		
		//!Get the stream types that will be generated during ::refresh	
		unsigned int getRefreshUseMask() const;	
		
		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b);

#ifdef DEBUG
		bool runUnitTests();
#endif
};
#endif

