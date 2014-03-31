/*
 *	clusterAnalysis.h - Cluster analysis on valued point clouds
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
#ifndef CLUSTERANALYSIS_H
#define CLUSTERANALYSIS_H
#include "../filter.h"
#include "../../common/translation.h"

#include <map>

//!Cluster analysis filter
class ClusterAnalysisFilter : public Filter
{
	private:
		//Clustering algorithm to use
		unsigned int algorithm;
	
		//Algorithm parameters
		//---	

		//Do we want to enable the core-classification pre-step
		bool enableCoreClassify;
		//Core-linkage "core" classification distance
		float coreDist;
		//Coring kNN maximum
		unsigned int coreKNN;
		//Link distance for core
		float linkDist;
		
		//Enable bulk linking step
		bool enableBulkLink;
		//Link distance for bulk
		float bulkLink;

		//Enable erosion step
		bool enableErosion;
		//Erosion distance for bulk from nonclustered bulk
		float dErosion;
		//---	

		//post processing options
		//Minimum/max number of "core" entires to qualify as,
		//well, a meaningful cluster
		bool wantCropSize;
		size_t nMin,nMax;
		
		bool wantClusterSizeDist,logClusterSize;

		//Do we want the composition data for the cluster
		bool wantClusterComposition, normaliseComposition;

		//Do we want a morphological analysis
		bool wantClusterMorphology;

		//!Do we have range data to use 
		bool haveRangeParent;
		//!The names of the incoming ions
		std::vector<std::string > ionNames;
		
		//!Which ions are core/builk for a  particular incoming range?
		std::vector<bool> ionCoreEnabled,ionBulkEnabled;

		//Do cluster refresh using Link Algorithm (Core + max sep)
		unsigned int refreshLinkClustering(const std::vector<const FilterStreamData *> &dataIn,
				std::vector< std::vector<IonHit> > &clusteredCore, 
				std::vector<std::vector<IonHit>  > &clusteredBulk,ProgressData &progress,
					       		bool (*callback)(bool));


		//Helper function to create core and bulk vectors of ions from input ionstreams
		void createRangedIons(const std::vector<const FilterStreamData *> &dataIn,
						std::vector<IonHit> &core,std::vector<IonHit> &bulk,
					       		ProgressData &p,bool (*callback)(bool)) const;


		//Check to see if there are any core or bulk ions enabled respectively.
		void checkIonEnabled(bool &core, bool &bulk) const;

		static void buildRangeEnabledMap(const RangeStreamData *r,
					map<size_t,size_t> &rangeEnabledMap);

		//Strip out clusters with a given number of elements
		bool stripClusterBySize(vector<vector<IonHit> > &clusteredCore,
						vector<vector<IonHit> > &clusteredBulk,
							bool (*callback)(bool), ProgressData &p) const;
		//Build a plot that is the cluster size distribution as a function of cluster size
		PlotStreamData *clusterSizeDistribution(const vector<vector<IonHit> > &solutes, 
						const vector<vector<IonHit> > &matrix) const;


		//Build plots that are the cluster size distribution as
		// a function of cluster size, specific to each ion type.
		void genCompositionVersusSize(const vector<vector<IonHit> > &clusteredCore,
				const vector<vector<IonHit> > &clusteredBulk, const RangeFile *rng,
							vector<PlotStreamData *> &plots) const;

#ifdef DEBUG
		bool paranoidDebugAssert(const std::vector<std::vector<IonHit > > &core, 
				const std::vector<std::vector<IonHit> > &bulk) const;
#endif
#ifdef DEBUG
	public:
#endif
		//COmpute the singular values that area associated with each cluster
		void getSingularValues(const vector<vector<IonHit> > &clusteredCore,
				const vector<vector<IonHit> > &clusteredBulk, vector<vector<float> > &singularValues,
				vector<std::pair<Point3D,vector<Point3D> > > &singularVectors) const;
	public:
		ClusterAnalysisFilter(); 
		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;

		//!Initialise filter prior to tree propagation
		virtual void initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut);

		//!Returns -1, as range file cache size is dependant upon input.
		virtual size_t numBytesForCache(size_t nObjects) const;
		//!Returns FILTER_TYPE_SPATIAL_ANALYSIS
		unsigned int getType() const { return FILTER_TYPE_CLUSTER_ANALYSIS;};
		//update filter
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
					std::vector<const FilterStreamData *> &getOut, 
					ProgressData &progress, bool (*callback)(bool));
		//!Get the type string  for this filter
		virtual std::string typeString() const { return std::string(TRANS("Cluster Analysis"));};

		std::string getErrString(unsigned int i) const;

		//!Get the properties of the filter, in key-value form. First vector is for each output.
		void getProperties(FilterPropGroup &propertyList) const;

		//!Set the properties for the nth filter
		bool setProperty(unsigned int key, 
				const std::string &value, bool &needUpdate);
		
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
		
		//!Get the stream types that will be generated during ::refresh	
		unsigned int getRefreshUseMask() const;	
		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b)  ;

#ifdef DEBUG
		bool wantParanoidDebug;
		bool runUnitTests();
#endif
};

#endif
