#ifndef ANNOTATION_H
#define ANNOTATION_H

#include "../filter.h"

enum
{
	ANNOTATION_ARROW,
	ANNOTATION_TEXT,
	ANNOTATION_TEXT_WITH_ARROW,
	ANNOTATION_ANGLE_MEASURE,
	ANNOTATION_LINEAR_MEASURE,
	ANNOTATION_MODE_END
};

//!Filter to place drawing objecs to help annotate
// the 3D scene
class AnnotateFilter : public Filter
{
	private:
		//what style of annotation are we using?
		unsigned int annotationMode;
		//Position of annotation, thing to point at and text up/across vectors
		Point3D position, target, upVec,acrossVec;
		//Positions for angle measurement
		Point3D anglePos[3];
		//annotation text string
		std::string annotateText;
		//Text display style, arrow annotation size, handle size for angle spheres
		float textSize,annotateSize,sphereAngleSize;

		//Annotation colour
		float r,g,b,a;

		//Disable/enable annotation
		bool active;

		// Show included angle text
		bool showAngleText;

		//Show reflexive angle, instead of included angle when using angle annot.
		bool reflexAngle;

		//Angle format to use in 3D scene
		unsigned int angleFormatPreDecimal,angleFormatPostDecimal;

		//Using fixed spacings or not
		bool linearFixedTicks;
		//Number of ticks to use in linear measure
		unsigned int linearMeasureTicks;
	
		//Spacing to use between ticks if using fixed spacings 
		float linearMeasureSpacing;

		//Font-size for linear measure object 
		float fontSizeLinearMeasure;

		//Measure end-marker size
		float linearMeasureMarkerSize;
	public:
		//!Constructor
		AnnotateFilter();

		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;

		//!Apply filter to new data, updating cache as needed. Vector
		// of returned pointers must be deleted manually, first checking
		// ->cached.
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
							std::vector<const FilterStreamData *> &dataOut,
							ProgressData &progress, bool (*callback)(void));
		//!Get (approx) number of bytes required for cache
		size_t numBytesForCache(size_t nObjects) const;

		//!return type ID
		unsigned int getType() const { return FILTER_TYPE_ANNOTATION;}

		//!Return filter type as std::string
		std::string typeString() const { return std::string(TRANS("Annotation"));};

		//!Get the properties of the filter, in key-value form. First vector is for each output.
		void getProperties(FilterProperties &propertyList) const;

		//!Set the properties for the nth filter,
		//!needUpdate tells us if filter output changes due to property set
		bool setProperty(unsigned int set, unsigned int key,
					const std::string &value, bool &needUpdate);


		void setPropFromBinding( const SelectionBinding &b) ;

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
		int getRefreshBlockMask() const;

		//!Get the bitmask encoded list of filterstreams that this filter emits from ::refresh.
		// This MUST always be consistent with ::refresh for filters current state.
		int getRefreshEmitMask() const;
};


#endif
