#ifndef VOXELISE_H
#define VOXELISE_H
#include "../filter.h"

//!Filter that does voxelisation for various primitives (copid from CompositionFilter)
class VoxeliseFilter : public Filter
{
private:
	const static size_t INDEX_LENGTH = 3;

	//Enabled ions for numerator/denom
	std::vector<char> enabledIons[2];

	//!Stepping mode - fixed width or fixed number of bins
	bool fixedWidth;
	
	//!number of bins (if using fixed bins)
	unsigned long long nBins[INDEX_LENGTH];
	//!Width of each bin (if using fixed wdith)
	Point3D binWidth;
	//!boundcube for the input data points
	BoundCube bc;
	
	//!density-based or count-based	
	int normaliseType;
	bool numeratorAll, denominatorAll;
	//This is filter's enabled ranges
	RangeStreamData *rsdIncoming;
	
	float r,g,b,a;

	//!3D Point Representation size
	float splatSize;

	//!Isosurface level
	float isoLevel;
	//!Default output representation mode
	unsigned int representation;
public:
	VoxeliseFilter();
	//!Duplicate filter contents, excluding cache.
	Filter *cloneUncached() const;


	
	//!Get approx number of bytes for caching output
	size_t numBytesForCache(size_t nObjects) const;

	unsigned int getType() const { return FILTER_TYPE_VOXELS;};
	
	virtual void initFilter(const std::vector<const FilterStreamData *> &dataIn,
					std::vector<const FilterStreamData *> &dataOut);
	//!update filter
	unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
						 std::vector<const FilterStreamData *> &getOut, 
						 ProgressData &progress, bool (*callback)(void));
	
	virtual std::string typeString() const { return std::string("Voxelisation");};

	//!Get the human-readable options for the normalisation, based upon enum 
	std::string getNormaliseTypeString(int type) const;
	//!Get the human-readable options for the visual representation (enum)
	std::string getRepresentTypeString(int type) const;
	
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
	
	VoxeliseFilter operator=(const VoxeliseFilter &v);
	
	//!calculate the widths of the bins in 3D
	void calculateWidthsFromNumBins(Point3D &widths, unsigned long long *nb) {
		Point3D low, high;
		bc.getBounds(low, high);
		for (int i = 0; i < 3; i++) {
			widths[i] = (high[i] - low[i])/(float)nb[i];
		}
	}
	//!set the number of the bins in 3D
	void calculateNumBinsFromWidths(Point3D &widths, unsigned long long *nb) {
		Point3D low, high;
		bc.getBounds(low, high);
		for (int i = 0; i < 3; i++) {
			if (low[i] == high[i]) nb[i] = 1;
			else nb[i] = (unsigned long long)((high[i] - low[i])/(float)widths[i]) + 1;
		}
	}
};



#endif

