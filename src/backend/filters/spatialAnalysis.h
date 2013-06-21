/*
 *	spatialAnalysis.h - Perform various data analysis on 3D point clouds
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
#ifndef SPATIALANALYSIS_H
#define SPATIALANALYSIS_H
#include "../filter.h"
#include "../../common/translation.h"

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
		
		//Density filtering specific params
		//-------
		//Do we keep points with density >= cutoff (true), or
		// points with density < cutoff (false)
		bool keepDensityUpper;

		//Cutoff value when performing density filtering
		float densityCutoff; 

		//!Vector paramaters for different primitives
		vector<Point3D> vectorParams;
		//!Scalar paramaters for different primitives
		vector<float> scalarParams;
	
		//Reset the scalar and vector parameters
		// to their defaults, if the required parameters
		// are not available
		void resetParamsAsNeeded();
		//-------



	
		//Radial distribution function - creates a 1D histogram of spherical atom counts, centered around each atom
		size_t algorithmRDF(ProgressData &progress, bool (*callback)(bool), size_t totalDataSize, 
			const vector<const FilterStreamData *>  &dataIn, 
			vector<const FilterStreamData * > &getOut,const RangeFile *rngF);


		//Local density function - places a sphere around each point to compute per-point density
		size_t algorithmDensity(ProgressData &progress, bool (*callback)(bool), size_t totalDataSize, 
			const vector<const FilterStreamData *>  &dataIn, 
			vector<const FilterStreamData * > &getOut);
		
		//Density filter function - same as density function, but then drops points from output
		// based upon their local density and some density cutoff data
		size_t algorithmDensityFilter(ProgressData &progress, bool (*callback)(bool), size_t totalDataSize, 
			const vector<const FilterStreamData *>  &dataIn, 
			vector<const FilterStreamData * > &getOut);

		size_t algorithmAxialDf(ProgressData &progress, bool (*callback)(bool), size_t totalDataSize, 
			const vector<const FilterStreamData *>  &dataIn, 
			vector<const FilterStreamData * > &getOut,const RangeFile *rngF);

		//Create a 3D manipulable cylinder as an output drawable
		// using the parameters stored inside the vector/scalar params
		// both parameters are outputs from this function
		void createCylinder(DrawStreamData* &d, SelectionDevice * &s) const;

		//Scan input datstreams to build a two point vectors,
		// one of those with points specified as "target" 
		// which is a copy of the input points
		//Returns 0 on no error, otherwise nonzero
		size_t buildSplitPoints(const vector<const FilterStreamData *> &dataIn,
					ProgressData &progress, size_t totalDataSize,
					const RangeFile *rngF, bool (*callback)(bool),
					vector<Point3D> &pSource, vector<Point3D> &pTarget) const;


		//From the given input ions, filter them down using the user
		// selection for ranges. If sourceFilter is true, filter by user
		// source selection, otherwise by user target selection
		void filterSelectedRanges(const vector<IonHit> &ions, bool sourceFilter, const RangeFile *rngF, vector<IonHit> &output) const;
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
					ProgressData &progress, bool (*callback)(bool));
		//!Get the type string  for this fitler
		virtual std::string typeString() const { return std::string(TRANS("Spat. Analysis"));};

		//!Get the properties of the filter, in key-value form. First vector is for each output.
		void getProperties(FilterPropGroup &propertyList) const;

		//!Set the properties for the nth filter
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
		
		//!Get the stream types that will be possibly used during ::refresh	
		unsigned int getRefreshUseMask() const;	
		
		//!Set internal property value using a selection binding  
		void setPropFromBinding(const SelectionBinding &b)  ;
		
		//Set the filter's Title "user string"
		void setUserString(const std::string &s); 
#ifdef DEBUG
		bool runUnitTests();
#endif
};

#endif
