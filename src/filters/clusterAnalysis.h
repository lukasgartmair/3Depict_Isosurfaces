#ifndef CLUSTERANALYSIS_H
#define CLUSTERANALYSIS_H
#include "../filter.h"
#include "../K3DTree-mk2.h"

//!Cluster analysis filter
class ClusterAnalysisFilter : public Filter
{
	private:
		//Clustering algorithm to use
		unsigned int algorithm;
	
		//Algorithm parameters
		//---	
		//Core-linkage "core" classification distance
		float coreDist;
		//Coring kNN maximum
		int coreKNN;
		//Link distance for core
		float linkDist;
		//Link distance for bulk
		float bulkLink;
		//Erosion distance for bulk from nonclustered bulk
		float dErosion;
		//---	

		//post processing options
		//Minimum/max number of "core" entires to qualify as,
		//well, a meaningful cluster
		bool wantCropSize;
		size_t nMin,nMax;
		bool sizeCountBulk;
		
		bool wantClusterSizeDist,logClusterSize;

		bool wantClusterComposition, normaliseComposition;

		//!Do we have range data to use 
		bool haveRangeParent;
		//!The names of the incoming ions
		std::vector<std::string > ionNames;
		
		//!Which ions are core/builk for a  particular incoming range?
		std::vector<bool> ionCoreEnabled,ionBulkEnabled;
	
	
		unsigned int refreshLinkClustering(const std::vector<const FilterStreamData *> &dataIn,
				std::vector< std::vector<IonHit> > &clusteredCore, 
				std::vector<std::vector<IonHit>  > &clusteredBulk,ProgressData &progress,
					       		bool (*callback)(void)) const;


		//Helper function to create core and bulk vectors of ions from input ionstreams
		void createRangedIons(const std::vector<const FilterStreamData *> &dataIn,
						std::vector<IonHit> &core,std::vector<IonHit> &bulk,
					       		ProgressData &p,bool (*callback)(void)) const;


		//Check to see if there are any core or bulk ions enabled respectively.
		void checkIonEnabled(bool &core, bool &bulk) const;

		void buildRangeEnabledMap(const RangeStreamData *r,
					map<size_t,size_t> &rangeEnabledMap) const;

		//Strip out clusters with a given number of elements
		bool stripClusterBySize(vector<vector<IonHit> > &clusteredCore,
						vector<vector<IonHit> > &clusteredBulk,
							bool (*callback)(void), ProgressData &p) const;
		//Build a plot that is the cluster size distribution as  afunction of cluster size
		PlotStreamData *clusterSizeDistribution(vector<vector<IonHit> > &solutes, 
						vector<vector<IonHit> > &matrix) const;


		//Build plots that are the cluster size distribution as
		// a function of cluster size, specific to each ion type.
		void genCompositionVersusSize(const vector<vector<IonHit> > &clusteredCore,
				const vector<vector<IonHit> > &clusteredBulk, const RangeFile *rng,
							vector<PlotStreamData *> &plots) const;

#ifdef DEBUG
		bool paranoidDebugAssert(const std::vector<std::vector<IonHit > > &core, 
				const std::vector<std::vector<IonHit> > &bulk) const;
#endif
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
					ProgressData &progress, bool (*callback)(void));
		//!Get the type string  for this fitler
		virtual std::string typeString() const { return std::string("Cluster Analysis");};

		std::string getErrString(unsigned int i) const;

		//!Get the properties of the filter, in key-value form. First vector is for each output.
		void getProperties(FilterProperties &propertyList) const;

		//!Set the properties for the nth filter
		bool setProperty(unsigned int set,unsigned int key, 
				const std::string &value, bool &needUpdate);
		
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
