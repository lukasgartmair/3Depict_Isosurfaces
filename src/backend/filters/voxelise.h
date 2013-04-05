/*
 *	voxelise.h - Compute 3D binning (voxelisation) of point clouds
 *	Copyright (C) 2013, D Haley 

 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.

 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.

 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef VOXELISE_H
#define VOXELISE_H
#include "../filter.h"
#include "../../common/translation.h"

//!Filter that does voxelisation for various primitives (copied from CompositionFilter)
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
	unsigned int normaliseType;
	bool numeratorAll, denominatorAll;
	//This is filter's enabled ranges
	RangeStreamData *rsdIncoming;
	
	float r,g,b,a;

	//!Filter mode to apply to data before output
	unsigned int filterMode;

	//!How do we treat boundaries when applying filters
	unsigned int filterBoundaryMode;

	//!Filter size
	unsigned int filterBins;

	//!Gaussian filter standard deviation
	float gaussDev;

	//!3D Point Representation size
	float splatSize;

	//!Isosurface level
	float isoLevel;
	//!Default output representation mode
	unsigned int representation;
public:
	VoxeliseFilter();
	~VoxeliseFilter() { if(rsdIncoming) delete rsdIncoming;}
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
						 ProgressData &progress, bool (*callback)(bool));
	
	virtual std::string typeString() const { return std::string(TRANS("Voxelisation"));};

	//!Get the human-readable options for the normalisation, based upon enum 
	static std::string getNormaliseTypeString(int type);
	//!Get the human-readable options for filtering, based upon enum 
	static std::string getFilterTypeString(int type);
	//!Get the human-readable options for the visual representation (enum)
	static std::string getRepresentTypeString(int type);
	//!Get the human-readable options for boundary behaviour during filtering, based upon enum 
	static std::string getFilterBoundTypeString(int type);
	
	//!Get the properties of the filter, in key-value form. First vector is for each output.
	void getProperties(FilterPropGroup &propertyList) const;
	
	//!Set the properties for the nth filter. Returns true if prop set OK
	bool setProperty(unsigned int key, 
					 const std::string &value, bool &needUpdate);
	//!Get the human readable error string associated with a particular error code during refresh(...)
	std::string getErrString(unsigned int code) const;
	
	//!Dump state to output stream, using specified format
	bool writeState(std::ostream &f,unsigned int format, 
					unsigned int depth=0) const;
	//!Read the state of the filter from XML file. If this
	//fails, filter will be in an undefined state.
	bool readState(xmlNodePtr &node, const std::string &packDir);

	//!Get the stream types that will be dropped during ::refresh	
	unsigned int getRefreshBlockMask() const;

	//!Get the stream types that will be generated during ::refresh	
	unsigned int getRefreshEmitMask() const;	

	//!Get the stream types that will be possibly ued during ::refresh	
	unsigned int getRefreshUseMask() const;	
	//!Set internal property value using a selection binding  
	void setPropFromBinding(const SelectionBinding &b) ;
	
	
	//!calculate the widths of the bins in 3D
	void calculateWidthsFromNumBins(Point3D &widths, unsigned long long *nb) const{
		Point3D low, high;
		bc.getBounds(low, high);
		for (unsigned int i = 0; i < 3; i++) {
			widths[i] = (high[i] - low[i])/(float)nb[i];
		}
	}
	//!set the number of the bins in 3D
	void calculateNumBinsFromWidths(Point3D &widths, unsigned long long *nb) const{
		Point3D low, high;
		bc.getBounds(low, high);
		for (unsigned int i = 0; i < 3; i++) {
			if (low[i] == high[i]) nb[i] = 1;
			else nb[i] = (unsigned long long)((high[i] - low[i])/(float)widths[i]) + 1;
		}
	}

#ifdef DEBUG
	bool runUnitTests();
#endif

};

#endif
