#include "clusterAnalysis.h"
#include "../xmlHelper.h"

#include <map>
#include <queue>

enum
{
	KEY_CLUSTERANALYSIS_ALGORITHM,
	KEY_CORECLASSIFYDIST,
	KEY_CORECLASSIFYKNN,
	KEY_LINKDIST,
	KEY_BULKLINK,
	KEY_ERODEDIST,
	KEY_WANT_CLUSTERSIZEDIST,
	KEY_WANT_LOGSIZEDIST,
	KEY_WANT_COMPOSITIONDIST,
	KEY_NORMALISE_COMPOSITION,
	KEY_CROP_SIZE,
	KEY_SIZE_COUNT_BULK,
	KEY_CROP_NMIN,
	KEY_CROP_NMAX,
	KEY_CORE_OFFSET=100000,
	KEY_BULK_OFFSET=200000
};

enum
{
	ABORT_ERR=1,
	NOCORE_ERR,
	NOBULK_ERR
};

enum 
{
	CLUSTER_LINK_ERODE,
	CLUSTER_ALGORITHM_ENUM_END,
};


enum
{
	COMPOSITION_NONE,
	COMPOSITION_UNNORMALISED,
	COMPOSITION_NORMALISED
};

const char SIZE_DIST_DATALABEL[] ="Size Distribution";
const char CHEM_DIST_DATALABEL[] ="Chemistry Distribution";

using std::vector;

//Optimisation tuning value;
// number of points to expect in a KD query sphere before the bulk query pays off
//  in terms of algorithm speed
const float SPHERE_PRESEARCH_CUTOFF = 75;



void makeFrequencyTable(const IonStreamData *i ,const RangeFile *r, 
				std::vector<std::pair<string,size_t> > &freqTable) 
{
	ASSERT(r);
	ASSERT(i);

	unsigned int numThreads;
#ifdef _OPENMP
	numThreads=omp_get_max_threads();
#else
	numThreads=1;
#endif
	vector<size_t *> ionHist;
	ionHist.resize(numThreads);

	for(size_t ui=0;ui<numThreads;ui++)
	{
		ionHist[ui]  = new size_t[r->getNumIons()];
		for(size_t uj=0;uj<r->getNumIons();uj++)
			ionHist[ui][uj]=0;
	}


#pragma omp parallel for 
	for(size_t ui=0;ui<i->data.size();ui++)
	{
#ifdef _OPENMP
		unsigned int threadNum=omp_get_thread_num();
#endif
		unsigned int rangeId;
		rangeId= r->getIonID(i->data[ui].getMassToCharge());

		if(rangeId!=(unsigned int)-1)
		{
#ifdef _OPENMP
			ionHist[threadNum][rangeId]++;
#else
			ionHist[0][rangeId]++;
#endif
		}
	}


	//we have to re-count the total, and tally the different threads
	//in the histogram
	for(size_t uj=0;uj<r->getNumIons();uj++)
	{
		for(size_t ui=1;ui<numThreads;ui++)
			ionHist[0][uj]+=ionHist[ui][uj];
	}

	freqTable.clear();
	for(size_t uj=0;uj<r->getNumIons();uj++)
		freqTable.push_back(make_pair(r->getName(uj),ionHist[0][uj]));

}

void makeCompositionTable(const IonStreamData *i ,const RangeFile *r, 
				std::vector<std::pair<string,float> > &compTable) 
{
	std::vector<std::pair<string,size_t> > tab;
	makeFrequencyTable(i,r,tab);

	compTable.resize(tab.size());
	size_t total=0;
	for(unsigned int ui=0;ui<tab.size();ui++)
		total+=tab[ui].second;

	if(total)
	{
	for(unsigned int ui=0;ui<tab.size();ui++)
		compTable[ui]=(make_pair(tab[ui].first,(float)tab[ui].second/(float)total));
	}

}

void ClusterAnalysisFilter::checkIonEnabled(bool &core, bool &bulk) const
{
	bulk=core=false;
	for(size_t ui=0;ui<ionCoreEnabled.size();ui++)
		core|=ionCoreEnabled[ui];

	for(size_t ui=0;ui<ionBulkEnabled.size();ui++)
		bulk|=ionBulkEnabled[ui];
}


void ClusterAnalysisFilter::buildRangeEnabledMap(const RangeStreamData *r,
					map<size_t,size_t> &rangeEnabledMap) const
{

	//should be empty...
	ASSERT(!rangeEnabledMap.size());

	//"Lagging" counter to track the mapping from ionID->enabled Ion ID.
	size_t count=0;
	for(size_t ui=0;ui<r->rangeFile->getNumIons();ui++)
	{
		if(r->enabledIons[ui])
		{
			//Create map entry
			rangeEnabledMap[ui]=count;
			count++;
		}
	}


	ASSERT(rangeEnabledMap.size() == ionCoreEnabled.size());
}

ClusterAnalysisFilter::ClusterAnalysisFilter()
{
	algorithm=CLUSTER_LINK_ERODE;
	bulkLink=1.0f;
	linkDist=0.5f;
	dErosion=0.25f;
	coreDist=0;
	coreKNN=1;
	wantClusterSizeDist=false;
	logClusterSize=false;
	nMin=0;
	nMax=std::numeric_limits<size_t>::max();
	sizeCountBulk=true;

	wantCropSize=false;
	wantClusterComposition=true;
	normaliseComposition=true;

	cacheOK=false;

	//By default, we should cache, but final decision is made higher up
	cache=true; 
}


Filter *ClusterAnalysisFilter::cloneUncached() const
{
	ClusterAnalysisFilter *p=new ClusterAnalysisFilter;

	p->algorithm=algorithm;
	
	p->coreDist=coreDist;
	p->bulkLink=bulkLink;
	p->linkDist=linkDist;
	p->dErosion=dErosion;

	p->wantCropSize=wantCropSize;
	p->nMin=nMin;
	p->nMax=nMax;
	p->sizeCountBulk=sizeCountBulk;

	p->wantClusterSizeDist = wantClusterSizeDist;
	p->logClusterSize= logClusterSize;
	
	p->wantClusterComposition=wantClusterComposition;
	p->normaliseComposition = normaliseComposition;

	p->haveRangeParent=false; //lets assume not, and this will be reset at ::initFilter time

	p->ionNames.resize(ionNames.size());
	std::copy(ionNames.begin(),ionNames.end(),p->ionNames.begin());
	p->ionCoreEnabled.resize(ionCoreEnabled.size());
	std::copy(ionCoreEnabled.begin(),ionCoreEnabled.end(),p->ionCoreEnabled.begin());
	p->ionBulkEnabled.resize(ionBulkEnabled.size());
	std::copy(ionBulkEnabled.begin(),ionBulkEnabled.end(),p->ionBulkEnabled.begin());
	
	//We are copying wether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

void ClusterAnalysisFilter::initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut)
{
	//Check for range file parent
	for(unsigned int ui=0;ui<dataIn.size();ui++)
	{
		if(dataIn[ui]->getStreamType() == STREAM_TYPE_RANGE)
		{
			const RangeStreamData *r;
			r = (const RangeStreamData *)dataIn[ui];

			bool different=false;
			if(!haveRangeParent)
			{
				//well, things have may have changed, we didn't have a 
				//range parent before. Or, we could have been loaded in from
				//a file.

				if(ionCoreEnabled.size() != r->rangeFile->getNumIons() ||
					ionBulkEnabled.size() != r->rangeFile->getNumIons())
					different=true;
				else
				{
					//The ion lengths are the same; if so, we can just fill in the gaps
					// -- the file does not store names; just sequence IDs.
					ionNames.clear();
					ionNames.reserve(r->rangeFile->getNumRanges());
					for(unsigned int uj=0;uj<r->rangeFile->getNumIons();uj++)
					{
						if(r->enabledIons[uj])
							ionNames.push_back(r->rangeFile->getName(uj));
					}
				}
			}
			else
			{
				//OK, last time we had a range parent. Check to see 
				//if the ion names are the same. If they are, keep the 
				//current bools, iff the ion names are all the same
				unsigned int numEnabled=std::count(r->enabledIons.begin(),
							r->enabledIons.end(),1);
				if(ionNames.size() == numEnabled)
				{
					unsigned int pos=0;
					for(unsigned int uj=0;uj<r->rangeFile->getNumIons();uj++)
					{
						//Only look at parent-enabled ranges
						if(r->enabledIons[uj])
						{
							if(r->rangeFile->getName(uj) != ionNames[pos])
							{
								different=true;
								break;
							}
							pos++;
						}
					}
				}
				else
					different=true;
			}
			haveRangeParent=true;

			if(different)
			{
				//OK, its different. we will have to re-assign,
				//but only allow the ranges enabled in the parent filter
				ionNames.clear();
				ionNames.reserve(r->rangeFile->getNumRanges());
				for(unsigned int uj=0;uj<r->rangeFile->getNumIons();uj++)
				{

					if(r->enabledIons[uj])
						ionNames.push_back(r->rangeFile->getName(uj));
				}

				//Zerou out any current data
				ionCoreEnabled.clear(); 
				ionBulkEnabled.clear();

				ionCoreEnabled.resize(ionNames.size(),false);
				ionBulkEnabled.resize(ionNames.size(),true);
			}
			
			return;
		}
	}

	haveRangeParent=false;

}

unsigned int ClusterAnalysisFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(void))
{
	//By default, copy inputs to output, unless it is an ion stream type.
	for(unsigned int ui=0;ui<dataIn.size();ui++)
	{
		//Block ions moving through filter; we modify them.
		if(dataIn[ui]->getStreamType() != STREAM_TYPE_IONS)
			getOut.push_back(dataIn[ui]);
	}
	
	//use the cached copy if we have it.
	if(cacheOK)
	{
		for(unsigned int ui=0;ui<filterOutputs.size();ui++)
			getOut.push_back(filterOutputs[ui]);
		return 0;
	}

	//OK, we actually have to do some work.
	//-----------

	
	//Find out how much total size we need in points vector
	size_t totalDataSize=0;
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS: 
			{
				const IonStreamData *d;
				d=((const IonStreamData *)dataIn[ui]);
				totalDataSize+=d->data.size();
			}
			break;	
			default:
				break;
		}
	}
	
	//Nothing to do.
	if(!totalDataSize)
		return 0;
	
	if(!haveRangeParent)
	{
		consoleOutput.push_back(string("No range data. Can't cluster."));
		return 0;
	}


	bool haveABulk,haveACore;
	checkIonEnabled(haveACore,haveABulk);
	//Check that the user has enabled something as core	
	if(!haveACore)
	{
		consoleOutput.push_back(
			string("No ranges selected for cluster \"core\". Cannot continue with clustering."));
		return NOCORE_ERR;
	}


	//Check that the user has enabled something as matrix/bulk. 
	if(!haveABulk)
	{
		consoleOutput.push_back(
			string("No ranges selected for cluster \"bulk\". Cannot continue with clustering."));
		return NOBULK_ERR;
	}

#ifdef DEBUG
	for(unsigned int ui=0;ui<ionCoreEnabled.size();ui++)
	{
		if(ionCoreEnabled[ui])
		{
			ASSERT(!ionBulkEnabled[ui]);
		}
	}
#endif

	//Do the clustering 
	//-------------
	vector<vector<IonHit> > clusteredCore,clusteredBulk;

	switch(algorithm)
	{
		case CLUSTER_LINK_ERODE:
		{
			unsigned int errCode;
			errCode=refreshLinkClustering(dataIn,clusteredCore,
						clusteredBulk,progress,callback);

			if(errCode)
				return errCode;
			break;
		}
		default:
			ASSERT(false);
	}

	/* If you are paranoid about the quality of the output, uncomment this. 
	 * This will enable running some sanity checks that do not use the data structures
	 * involved in the clustering; ie a secondary check.
	 * However this is far too slow to enable by default, even in debug mode
	cerr << "Running paranoid debug checks -- this will be slow:" ;
	paranoidDebugAssert(clusteredCore,clusteredBulk);
	cerr << "Done" << endl;
	*/

	if(wantCropSize)
		stripClusterBySize(clusteredCore,clusteredBulk,callback,progress);

	bool haveBulk,haveCore;
	haveBulk=clusteredBulk.size();
	haveCore=clusteredCore.size();


	if(!haveBulk && !haveCore)
		return 0;

	//we can't have bulk, but no core...
	ASSERT(!(haveBulk && !haveCore));

	//-------------


	//Cluster reporting.
	const RangeStreamData *r=0;
	for(unsigned int ui=0;ui<dataIn.size();ui++)
	{
		if(dataIn[ui]->getStreamType() == STREAM_TYPE_RANGE)
		{
			r = (const RangeStreamData *)dataIn[ui];
			break;
		}
	}

	//Generate size distribution if we need it.
	if(wantClusterSizeDist)
	{
		PlotStreamData *d;
		d=clusterSizeDistribution(clusteredCore,clusteredBulk);

		if(d)
		{
			//Jump the plot index to some large number, so
			//we don't hit the cluster size distribution index
			d->index=1000;
			if(cache)
			{
				d->cached=1;
				filterOutputs.push_back(d);
			}
			else
				d->cached=0;

			getOut.push_back(d);

		}

	}

	//Generate composition distribution if we requested it
	if(wantClusterComposition)
	{
		vector<PlotStreamData *> plots;
		genCompositionVersusSize(clusteredCore,clusteredBulk,r->rangeFile,plots);

		for(unsigned int ui=0;ui<plots.size();ui++)
		{
			plots[ui]->index=ui;

			if(cache)
			{
				plots[ui]->cached=1;
				filterOutputs.push_back(plots[ui]);
			}
			else
				plots[ui]->cached=1;

			getOut.push_back(plots[ui]);
		}
	}



	//Construct the output clustered data.
	IonStreamData *i = new IonStreamData;
	
	std::string sDebugConsole,stmp;
	stream_cast(stmp,clusteredCore.size());

	sDebugConsole="Found :";
	sDebugConsole+=stmp;
	sDebugConsole+= " clusters";
	consoleOutput.push_back(sDebugConsole);

	size_t totalSize=0;

	#pragma omp parallel for reduction(+:totalSize)
	for(size_t ui=0;ui<clusteredBulk.size();ui++)
		totalSize+=clusteredBulk[ui].size();

	#pragma omp parallel for reduction(+:totalSize)
	for(size_t ui=0;ui<clusteredCore.size();ui++)
		totalSize+=clusteredCore[ui].size();
	i->data.resize(totalSize);
	


	
	
	//copy across the core and bulk ions
	//into the output
	size_t copyPos=0;
	for(size_t ui=0;ui<clusteredCore.size();ui++)
	{
		for(size_t uj=0;uj<clusteredCore[ui].size();uj++)
		{
			i->data[copyPos]=clusteredCore[ui][uj];
			copyPos++;
		}
	}
	clusteredCore.clear();

	for(size_t ui=0;ui<clusteredBulk.size();ui++)
	{
		for(size_t uj=0;uj<clusteredBulk[ui].size();uj++)
		{
			i->data[copyPos]=clusteredBulk[ui][uj];
			copyPos++;
		}
	}
	clusteredBulk.clear();

	//The result data is drawn grey...
	i->r=0.5f;	
	i->g=0.5f;	
	i->b=0.5f;	
	i->a=1.0f;	

	//Save the ion stream data.
	if(cache)
	{
		i->cached=1;
		filterOutputs.push_back(i);
		cacheOK=true;
	}
	else
		i->cached=0;

	getOut.push_back(i);


	if(wantClusterComposition)
	{
		ASSERT(r);

		if(normaliseComposition)
		{
			vector<pair<string,float> > compTable;
			makeCompositionTable(i,r->rangeFile,compTable);


			if(haveBulk)
				consoleOutput.push_back("Compositions (fractional, core+bulk)");
			else if(haveCore)
				consoleOutput.push_back("Compositions (fractional, core only)");

			std::string compString,tmp;
			for(unsigned int ui=0;ui<compTable.size();ui++)
			{
				compString= compTable[ui].first;
				compString+="\t\t";
				stream_cast(tmp,compTable[ui].second);
				compString+=tmp;
				consoleOutput.push_back(compString);
			}

		}
		else
		{
			vector<pair<string,size_t> > freqTable;
			makeFrequencyTable(i,r->rangeFile,freqTable);

			consoleOutput.push_back("Frequencies (core+bulk)");

			std::string freqString,tmp;
			for(unsigned int ui=0;ui<freqTable.size();ui++)
			{
				freqString= freqTable[ui].first;
				freqString+="\t\t";
				stream_cast(tmp,freqTable[ui].second);
				freqString+=tmp;
				consoleOutput.push_back(freqString);
			}
		}

		
	}	

	return 0;
}

void ClusterAnalysisFilter::getProperties(FilterProperties &propertyList) const
{
	propertyList.data.clear();
	propertyList.keys.clear();
	propertyList.types.clear();

	vector<unsigned int> type,keys;
	vector<pair<string,string> > s;

	string tmpStr;
	vector<pair<unsigned int,string> > choices;
	tmpStr="Core Link + Erode";
	choices.push_back(make_pair((unsigned int)CLUSTER_LINK_ERODE,tmpStr));
	
	tmpStr= choiceString(choices,algorithm);
	s.push_back(make_pair(string("Algorithm"),tmpStr));
	choices.clear();
	type.push_back(PROPERTY_TYPE_CHOICE);
	keys.push_back(KEY_CLUSTERANALYSIS_ALGORITHM);
	
	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);
	propertyList.keyNames.push_back("Algorithm");

	s.clear(); type.clear(); keys.clear(); 

	if(algorithm == CLUSTER_LINK_ERODE)
	{
		stream_cast(tmpStr,coreDist);
		s.push_back(make_pair(string("Core Classify Dist"),tmpStr));
		type.push_back(PROPERTY_TYPE_REAL);
		keys.push_back(KEY_CORECLASSIFYDIST);
		
		stream_cast(tmpStr,coreKNN);
		s.push_back(make_pair(string("Classify Knn Max"),tmpStr));
		type.push_back(PROPERTY_TYPE_INTEGER);
		keys.push_back(KEY_CORECLASSIFYKNN);
		
		stream_cast(tmpStr,linkDist);
		s.push_back(make_pair(string("Core Link Dist"),tmpStr));
		type.push_back(PROPERTY_TYPE_REAL);
		keys.push_back(KEY_LINKDIST);
		
		stream_cast(tmpStr,bulkLink);
		s.push_back(make_pair(string("Bulk Link (Envelope) Dist"),tmpStr));
		type.push_back(PROPERTY_TYPE_REAL);
		keys.push_back(KEY_BULKLINK);
		
		stream_cast(tmpStr,dErosion);
		s.push_back(make_pair(string("Erode Dist"),tmpStr));
		type.push_back(PROPERTY_TYPE_REAL);
		keys.push_back(KEY_ERODEDIST);
	}
	
	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);
	propertyList.keyNames.push_back("Clustering Params");
	
	s.clear(); type.clear(); keys.clear(); 

	if(sizeCountBulk)
		tmpStr="1";
	else
		tmpStr="0";

	s.push_back(make_pair("Count bulk",tmpStr));
	type.push_back(PROPERTY_TYPE_BOOL);
	keys.push_back(KEY_SIZE_COUNT_BULK);

	if(wantCropSize)
		tmpStr="1";
	else
		tmpStr="0";

	s.push_back(make_pair("Size Cropping",tmpStr));
	type.push_back(PROPERTY_TYPE_BOOL);
	keys.push_back(KEY_CROP_SIZE);

	if(wantCropSize)
	{
		stream_cast(tmpStr,nMin);
		s.push_back(make_pair("Min Size",tmpStr));
		type.push_back(PROPERTY_TYPE_INTEGER);
		keys.push_back(KEY_CROP_NMIN);
		
		stream_cast(tmpStr,nMax);
		s.push_back(make_pair("Max Size",tmpStr));
		type.push_back(PROPERTY_TYPE_INTEGER);
		keys.push_back(KEY_CROP_NMAX);

	}	
	
	if(wantClusterSizeDist)
		tmpStr="1";
	else
		tmpStr="0";


	s.push_back(make_pair("Size Distribution",tmpStr));
	type.push_back(PROPERTY_TYPE_BOOL);
	keys.push_back(KEY_WANT_CLUSTERSIZEDIST);
	if(wantClusterSizeDist)
	{	
		if(logClusterSize)
			tmpStr="1";
		else
			tmpStr="0";

		s.push_back(make_pair("Log Scale",tmpStr));
		type.push_back(PROPERTY_TYPE_BOOL);
		keys.push_back(KEY_WANT_LOGSIZEDIST);
	}


	
	if(wantClusterComposition)
		tmpStr="1";
	else
		tmpStr="0";


	s.push_back(make_pair("Chemistry Dist.",tmpStr));
	type.push_back(PROPERTY_TYPE_BOOL);
	keys.push_back(KEY_WANT_COMPOSITIONDIST);

	
	if(wantClusterComposition)
	{	
		if(normaliseComposition)
			tmpStr="1";
		else
			tmpStr="0";

		s.push_back(make_pair("Normalise",tmpStr));
		type.push_back(PROPERTY_TYPE_BOOL);
		keys.push_back(KEY_NORMALISE_COMPOSITION);
	}

	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);
	propertyList.keyNames.push_back("Postprocess");
	
	s.clear(); type.clear(); keys.clear(); 


	if(haveRangeParent)
	{	
		ASSERT(ionCoreEnabled.size() == ionBulkEnabled.size());
		ASSERT(ionCoreEnabled.size() == ionNames.size())
		for(size_t ui=0;ui<ionNames.size();ui++)
		{
			if(ionCoreEnabled[ui])
				tmpStr="1";
			else
				tmpStr="0";
			s.push_back(make_pair(ionNames[ui],tmpStr));
			type.push_back(PROPERTY_TYPE_BOOL);
			keys.push_back(KEY_CORE_OFFSET+ui);
		}
		propertyList.data.push_back(s);
		propertyList.types.push_back(type);
		propertyList.keys.push_back(keys);
		propertyList.keyNames.push_back("Core Ranges");
		
		s.clear(); type.clear(); keys.clear(); 
		
		for(size_t ui=0;ui<ionNames.size();ui++)
		{
			if(ionBulkEnabled[ui])
				tmpStr="1";
			else
				tmpStr="0";
			s.push_back(make_pair(ionNames[ui],tmpStr));
			type.push_back(PROPERTY_TYPE_BOOL);
			keys.push_back(KEY_BULK_OFFSET+ui);
		}
		propertyList.data.push_back(s);
		propertyList.types.push_back(type);
		propertyList.keys.push_back(keys);
		propertyList.keyNames.push_back("Bulk Ranges");
	}	

}

bool ClusterAnalysisFilter::setProperty(unsigned int set,unsigned int key, 
				const std::string &value, bool &needUpdate)
{

	needUpdate=false;
	switch(key)
	{
		case KEY_CLUSTERANALYSIS_ALGORITHM:
		{
			size_t ltmp=CLUSTER_ALGORITHM_ENUM_END;

			std::string tmp;
			if(value == "Max. Sep + Erode")
				ltmp=CLUSTER_LINK_ERODE;
			else
				return false;
			
			algorithm=ltmp;
			needUpdate=true;
			clearCache();

			break;
		}
		case KEY_CORECLASSIFYDIST:
		{
			std::string tmp;
			float ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp< 0.0)
				return false;
			
			coreDist=ltmp;
			needUpdate=true;
			clearCache();

			break;
		}	
		case KEY_CORECLASSIFYKNN:
		{
			std::string tmp;
			int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp<= 0)
				return false;
			
			coreKNN=ltmp;
			needUpdate=true;
			clearCache();

			break;
		}	
		case KEY_LINKDIST:
		{
			std::string tmp;
			float ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp<= 0.0)
				return false;
			
			linkDist=ltmp;
			needUpdate=true;
			clearCache();

			break;
		}	
		case KEY_BULKLINK:
		{
			std::string tmp;
			float ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp< 0.0)
				return false;
			
			bulkLink=ltmp;
			needUpdate=true;
			clearCache();

			break;
		}	
		case KEY_ERODEDIST:
		{
			std::string tmp;
			float ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp< 0.0)
				return false;
			
			dErosion=ltmp;
			needUpdate=true;
			clearCache();

			break;
		}
		case KEY_WANT_CLUSTERSIZEDIST:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			bool lastVal=wantClusterSizeDist;
			if(stripped=="1")
				wantClusterSizeDist=true;
			else
				wantClusterSizeDist=false;

			if(lastVal!=wantClusterSizeDist)
			{
				//If we don't want the cluster composition
				//just kill it from the cache and request an update
				if(!wantClusterSizeDist)
				{
					for(size_t ui=filterOutputs.size();ui;)
					{
						ui--;
						if(filterOutputs[ui]->getStreamType() == STREAM_TYPE_PLOT)
						{
							//OK, is this the plot? 
							//We should match the title we used to generate it
							PlotStreamData *p;
							p=(PlotStreamData*)filterOutputs[ui];

							if(p->dataLabel.substr(strlen(SIZE_DIST_DATALABEL)) ==SIZE_DIST_DATALABEL )
							{
								//Yup, this is it.kill the distribution
								std::swap(filterOutputs[ui],filterOutputs.back());
								filterOutputs.pop_back();

								//Now, note we DONT break
								//here; as there is more than one
							}

						}
					}
				}
				else
				{
					//OK, we don't have one and we would like one.
					// We have to compute this. Wipe cache and start over
					clearCache(); 
				}
					needUpdate=true;
			}
			break;
		}
		case KEY_WANT_LOGSIZEDIST:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			bool lastVal=logClusterSize;
			if(stripped=="1")
				logClusterSize=true;
			else
				logClusterSize=false;

			//If the result is different
			//we need to alter the output
			if(lastVal!=logClusterSize)
			{
				//Scan through the cached output, and modify
				//the size distribution. Having to recalc this
				//just for a log/non-log change
				//is a real pain -- so lets be smart and avoid this!
				for(size_t ui=0;ui<filterOutputs.size();ui++)
				{
					if(filterOutputs[ui]->getStreamType() == STREAM_TYPE_PLOT)
					{
						//OK, is this the plot? 
						//We should match the title we used to generate it
						PlotStreamData *p;
						p=(PlotStreamData*)filterOutputs[ui];

						if(p->dataLabel ==SIZE_DIST_DATALABEL )
						{
							//Yup, this is it. Set the log status
							//and finish up
							p->logarithmic=logClusterSize;
							break;
						}

					}
				}



				needUpdate=true;
			}
			
			break;
		}
		case KEY_WANT_COMPOSITIONDIST:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			bool lastVal=wantClusterComposition;
			if(stripped=="1")
				wantClusterComposition=true;
			else
				wantClusterComposition=false;

			//if the result is different, e
			//remove the filter elements we no longer want.
			if(lastVal!=wantClusterComposition)
			{
				//If we don't want the cluster composition
				//just kill it from the cache and request an update
				if(!wantClusterComposition)
				{
					for(size_t ui=filterOutputs.size();ui;)
					{
						ui--;
						if(filterOutputs[ui]->getStreamType() == STREAM_TYPE_PLOT)
						{
							//OK, is this the plot? 
							//We should match the title we used to generate it
							PlotStreamData *p;
							p=(PlotStreamData*)filterOutputs[ui];

							if(p->dataLabel.substr(0,strlen(CHEM_DIST_DATALABEL)) ==CHEM_DIST_DATALABEL )
							{
								//Yup, this is it.kill the distribution
								std::swap(filterOutputs[ui],filterOutputs.back());
								filterOutputs.pop_back();

								//Now, note we DONT break
								//here; as there is more than one
							}

						}
					}
				}
				else
				{
					//OK, we don't have one and we would like one.
					// We have to compute this. Wipe cache and start over
					clearCache(); 
				}
					needUpdate=true;
			}
			
			break;
		}
		case KEY_NORMALISE_COMPOSITION:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			bool lastVal=normaliseComposition;
			if(stripped=="1")
				normaliseComposition=true;
			else
				normaliseComposition=false;

			//if the result is different, the
			//cache should be invalidated
			if(lastVal!=normaliseComposition)
			{
				needUpdate=true;
				clearCache();
			}
			
			break;
		}
		case KEY_CROP_SIZE:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			bool lastVal=wantCropSize;
			if(stripped=="1")
				wantCropSize=true;
			else
				wantCropSize=false;

			//if the result is different, the
			//cache should be invalidated
			if(lastVal!=wantCropSize)
			{
				needUpdate=true;
				clearCache();
			}
			
			break;
		}
		case KEY_CROP_NMIN:
		{
			std::string tmp;
			size_t ltmp;
			if(stream_cast(ltmp,value))
				return false;
		
			if( ltmp > nMax)
				return false;

			nMin=ltmp;
			needUpdate=true;
			clearCache();

			break;
		}	
		case KEY_CROP_NMAX:
		{
			std::string tmp;
			size_t ltmp;
			if(stream_cast(ltmp,value))
				return false;
	
			if(ltmp == 0)
				ltmp = std::numeric_limits<size_t>::max();

			if( ltmp < nMin)
				return false;

			nMax=ltmp;
			needUpdate=true;
			clearCache();

			break;
		}	
		case KEY_SIZE_COUNT_BULK:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			bool lastVal=sizeCountBulk;
			if(stripped=="1")
				sizeCountBulk=true;
			else
				sizeCountBulk=false;

			//if the result is different, the
			//cache should be invalidated
			if(lastVal!=sizeCountBulk)
			{
				needUpdate=true;
				clearCache();
			}
			
			break;
		}
		default:
		{
			//Set value is dictated by getProperties routine
			//and is the vector push back value
			if(set==3)
			{
				bool b;
				if(stream_cast(b,value))
					return false;
				//Core ions; convert key value to array offset
				key-=KEY_CORE_OFFSET;

				//no need to update
				if(ionCoreEnabled[key] == b)
					return false;
				
				ionCoreEnabled[key]=b;

				//Check if we need to also need to disable 
				//ion bulk to preserve mutual exclusiveness.
				if(ionBulkEnabled[key] == b && b)
					ionBulkEnabled[key]=0;

				clearCache();
				needUpdate=true;
			}
			else if(set ==4)
			{
				bool b;
				if(stream_cast(b,value))
					return false;
				//Core ions; convert key value to array offset
				key-=KEY_BULK_OFFSET;

				//no need to update
				if(ionBulkEnabled[key] == b)
					return false;
				
				ionBulkEnabled[key]=b;

				//Check if we need to also need to disable 
				//ion core to preserve mutual exclusiveness
				if(ionCoreEnabled[key] == b && b)
					ionCoreEnabled[key]=0;

				clearCache();
				needUpdate=true;

			}
			else
			{
				ASSERT(false);
			}
		}
	}
		

	return true;
}

bool ClusterAnalysisFilter::writeState(std::ofstream &f,unsigned int format,
				unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<" << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\""<<userString << "\"/>"  << endl;
			f << tabs(depth+1) << "<algorithm value=\""<<algorithm<< "\"/>"  << endl;
		
			//Core-linkage algorithm parameters	
			f << tabs(depth+1) << "<coredist value=\""<<coreDist<< "\"/>"  << endl;
			f << tabs(depth+1) << "<coringknn value=\""<<coreKNN<< "\"/>"  << endl;
			f << tabs(depth+1) << "<linkdist value=\""<<linkDist<< "\"/>"  << endl;
			f << tabs(depth+1) << "<bulklink value=\""<<bulkLink<< "\"/>"  << endl;
			f << tabs(depth+1) << "<derosion value=\""<<dErosion<< "\"/>"  << endl;
			
			//Cropping control
			f << tabs(depth+1) << "<wantcropsize value=\""<<wantCropSize<< "\"/>"  << endl;
			f << tabs(depth+1) << "<nmin value=\""<<nMin<< "\"/>"  << endl;
			f << tabs(depth+1) << "<nmax value=\""<<nMax<< "\"/>"  << endl;
			
			//Postprocessing
			f << tabs(depth+1) << "<wantclustersizedist value=\""<<wantClusterSizeDist<< "\" logarithmic=\"" << 
					logClusterSize <<  "\" sizecountbulk=\""<<sizeCountBulk<< "\"/>"  << endl;
			f << tabs(depth+1) << "<wantclustercomposition value=\"" <<wantClusterComposition<< "\" normalise=\"" << 
					normaliseComposition<< "\"/>"  << endl;


			f << tabs(depth+1) << "<enabledions>"  << endl;
			f << tabs(depth+2) << "<core>"  << endl;
			for(size_t ui=0;ui<ionCoreEnabled.size();ui++)
			{
				f<< tabs(depth+3) << "<ion enabled=\"" << (int)ionCoreEnabled[ui] << "\"/>" <<  std::endl; 
			}
			f << tabs(depth+2) << "</core>"  << endl;
			f << tabs(depth+2) << "<bulk>"  << endl;
			for(size_t ui=0;ui<ionBulkEnabled.size();ui++)
			{
				f<< tabs(depth+3) << "<ion enabled=\"" << (int)ionBulkEnabled[ui] << "\"/>" <<  std::endl; 
			}
			f << tabs(depth+2) << "</bulk>"  << endl;
			f << tabs(depth+1) << "</enabledions>"  << endl;
			
			f << tabs(depth) << "</" << trueName() << ">" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

size_t ClusterAnalysisFilter::numBytesForCache(size_t nObjects) const
{
	return (size_t)nObjects*IONDATA_SIZE;
}

bool ClusterAnalysisFilter::readState(xmlNodePtr &nodePtr, const std::string &packDir)
{
	using std::string;
	string tmpStr;

	//Retrieve user string
	//===
	if(XMLHelpFwdToElem(nodePtr,"userstring"))
		return false;

	xmlChar *xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	userString=(char *)xmlString;
	xmlFree(xmlString);
	//===

	//Retrieve algorithm
	//====== 
	if(!XMLGetNextElemAttrib(nodePtr,algorithm,"algorithm","value"))
		return false;
	if(algorithm >=CLUSTER_ALGORITHM_ENUM_END)
		return false;
	//===
	
	//Retrieve parameter distances
	//===
	switch(algorithm)
	{
		case CLUSTER_LINK_ERODE:
		{
			if(!XMLGetNextElemAttrib(nodePtr,coreDist,"coredist","value"))
				return false;
			if(coreDist<0)
				return false;
			if(!XMLGetNextElemAttrib(nodePtr,coreKNN,"coringknn","value"))
				return false;
			if(coreKNN<=0)
				return false;
			if(!XMLGetNextElemAttrib(nodePtr,linkDist,"linkdist","value"))
				return false;
			if(linkDist<=0)
				return false;
			if(!XMLGetNextElemAttrib(nodePtr,bulkLink,"bulklink","value"))
				return false;
			if(bulkLink<0)
				return false;
			if(!XMLGetNextElemAttrib(nodePtr,dErosion,"derosion","value"))
				return false;
			if(dErosion<0)
				return false;
			break;
		}
		default:
		{
			ASSERT(false);
			return false;
		}
	}
	//===

	//Retrieve cropping info
	//===
	xmlNodePtr tmpPtr;
	tmpPtr=nodePtr;
	if(!XMLGetNextElemAttrib(nodePtr,wantClusterSizeDist,"wantclustersizedist","value"))
		return false;
	nodePtr=tmpPtr;
	if(!XMLGetNextElemAttrib(nodePtr,logClusterSize,"wantclustersizedist","logarithmic"))
		return false;
	nodePtr=tmpPtr;
	if(!XMLGetNextElemAttrib(nodePtr,sizeCountBulk,"wantclustersizedist","sizecountbulk"))
		return false;
	
	tmpPtr=nodePtr;
	if(!XMLGetNextElemAttrib(nodePtr,wantClusterComposition,"wantclustercomposition","value"))
		return false;
	nodePtr=tmpPtr;
	if(!XMLGetNextElemAttrib(nodePtr,normaliseComposition,"wantclustercomposition","normalise"))
		return false;
	//===


	//erase current enabled list.	
	ionCoreEnabled.clear();
	ionBulkEnabled.clear();

	//Retrieve enabled selections
	if(XMLHelpFwdToElem(nodePtr,"enabledions"))
		return false;

	//Jump to ion sequence (<core>/<bulk> level)
	nodePtr=nodePtr->xmlChildrenNode;

	if(XMLHelpFwdToElem(nodePtr,"core"))
		return false;
	//Jump to <ion> level
	tmpPtr=nodePtr->xmlChildrenNode;
	
	while(!XMLHelpFwdToElem(tmpPtr,"ion"))
	{
		int enabled;
		if(!XMLGetAttrib(tmpPtr,enabled,"enabled"))
			return false;

		ionCoreEnabled.push_back(enabled);
	}

	if(XMLHelpFwdToElem(nodePtr,"bulk"))
		return false;
	tmpPtr=nodePtr->xmlChildrenNode;

	while(!XMLHelpFwdToElem(tmpPtr,"ion"))
	{
		int enabled;
		if(!XMLGetAttrib(tmpPtr,enabled,"enabled"))
			return false;

		ionBulkEnabled.push_back(enabled);
	}

	return true;
}

int ClusterAnalysisFilter::getRefreshBlockMask() const
{
	//Anything but ions can go through this filter.
	return STREAM_TYPE_IONS;
}

int ClusterAnalysisFilter::getRefreshEmitMask() const
{
	if(wantClusterSizeDist || wantClusterComposition)
		return STREAM_TYPE_IONS | STREAM_TYPE_PLOT;
	else
		return STREAM_TYPE_IONS;
}

std::string ClusterAnalysisFilter::getErrString(unsigned int i) const
{
	switch(i)
	{
		case ABORT_ERR:
			return std::string("Clustering aborted");
		case NOCORE_ERR:
			return std::string("No core ions for cluster");
		case NOBULK_ERR:
			return std::string("No bulk ions for cluster");
		default:
			ASSERT(false);
	}
}

unsigned int ClusterAnalysisFilter::refreshLinkClustering(const std::vector<const FilterStreamData *> &dataIn,
		std::vector< std::vector<IonHit> > &clusteredCore, 
		std::vector<std::vector<IonHit>  > &clusteredBulk,ProgressData &progress,
					bool (*callback)(void)) 
{

	//Clustering algorithm, as per 
	//Stephenson, L. T.; et al
	//Microscopy and Microanalysis, 2007, 13, 448-463 
	//
	//See also
	//Vaumousse & Cerezo,
	//Ultramicroscopy 95 (2003) 215–22

	//Basic steps. Optional steps are denoted with a * 
	//
	//1*) Core classification; work only on core ions (bulk is ignored)
	//	- Each "core" ion has sphere of specified size placed around it,
	//	  if ion's kth-NN is within a given radius, then it is used as 
	//	  core, otherwise it is rejected to "bulk"
	//
	//2)  Cluster Construction: A "backbone" is constructed using 
	//    the core ions (after classification). 
	//	- Each ion has a sphere placed around it of fixed size; if it contacts
	//	  another ion, then these are considered as part of the same cluster.
	//
	//3*) Bulk inclusion step
	//	- For each cluster, every ion has a sphere placed around it. Bulk
	//	 ions that lie within this union of spheres are assigned to the cluster
	//	 This assignment is unambigious *iff* this radius is smaller than that
	//	 for the cluster construction step
	//
	//4*) Bulk Erosion step
	//	- Each unclustered bulk ion has a sphere placed around it. This sphere
	//	 strips out ions from the cluster. This is only done once (ie, not iterative)
	//
	// In the implementation, there are more steps, due to data structure construction
	// and other comptuational concerns
	
	bool needCoring=coreDist> std::numeric_limits<float>::epsilon();
	bool needBulkLink=bulkLink > std::numeric_limits<float>::epsilon();
	bool needErosion=dErosion> std::numeric_limits<float>::epsilon() && needBulkLink;
	unsigned int numClusterSteps=4;
	if(needBulkLink)
		numClusterSteps+=2;
	if(needErosion)
		numClusterSteps++;
	if(needCoring)
		numClusterSteps++;



	//Quick sanity check
	if(needBulkLink)
	{
		//It is mildly dodgy to use a "bulk" distance larger than your core distance
		//with relative dodgyness, depending upon cluster number density.
		//
		//This is because bulk components can "bridge", and assignment to the core
		//clusters will depend upon the order in which the ions are traversed.
		//At this point we should warn the user that this is the case, and suggest to them
		//that we hope they know what they are doing.


		if(bulkLink > linkDist)
		{
			consoleOutput.push_back("");
			consoleOutput.push_back(" --------------------------- Parameter selection notice ------------- "  );
			consoleOutput.push_back("You have specified a bulk distance larger than your link distance."  );
			consoleOutput.push_back("You can do this; thats OK, but the output is no longer independant of the computational process;"  );
			consoleOutput.push_back("This will be a problem in the case where two or more clusters can equally lay claim to a \"bulk\" ion. "  );
			consoleOutput.push_back(" If your inter-cluster distance is sufficiently large (larger than your bulk linking distance), then you can get away with this."  );
			consoleOutput.push_back(" In theory it is possible to \"join\" the clusters, but this has not been implemented for speed reasons.");
			consoleOutput.push_back("If you want this, please contact the author, or just use the source to add this in yourself."  );
			consoleOutput.push_back("---------------------------------------------------------------------- "  );
			consoleOutput.push_back("");
		}	

	}

	//Collate the ions into "core", and "bulk" ions, based upon our ranging data
	//----------
	progress.step=1;
	progress.filterProgress=0;
	progress.stepName="Collate";
	progress.maxStep=numClusterSteps;
	if(!(*callback)())
		return ABORT_ERR;

	vector<IonHit> coreIons,bulkIons;
	createRangedIons(dataIn,coreIons,bulkIons,progress,callback);

	if(!coreIons.size())
		return 0;
	//----------

	K3DTreeMk2 coreTree,bulkTree;
	BoundCube bCore,bBulk;

	//Build the core KD tree
	//----------
	progress.step++;
	progress.filterProgress=0;
	progress.stepName="Build Core";
	if(!(*callback)())
		return ABORT_ERR;

	//Build the core tree (eg, solute ions)
	coreTree.setCallback(callback);
	coreTree.setProgressPointer(&(progress.filterProgress));
	coreTree.resetPts(coreIons,false);
	coreTree.build();

	coreTree.getBoundCube(bCore);
	//----------

	if(needCoring)
	{
		//Perform Clustering Stage (1) : clustering classification
		//==	
		progress.step++;
		progress.stepName="Classify Core";
		if(!(*callback)())
			return ABORT_ERR;
		
		vector<bool> coreOK;
		ASSERT(coreIons.size() == coreTree.size());
		coreOK.resize(coreTree.size());
		float coreDistSqr=coreDist*coreDist;

		//TODO: the trees internal Tags prevent us from parallelising this. 
		//       :(. If we could pass a tag map to the tree, this would solve the problem
		for(size_t ui=0;ui<coreTree.size();ui++)
		{
			const Point3D *p;
			size_t pNN;	
			unsigned int k;
			float nnSqrDist;
			vector<size_t> tagsToClear;
		
			//Don't match ourselves -- to do this we must "tag" this tree node before we start
			p=coreTree.getPt(ui);
			coreTree.tag(ui);
			tagsToClear.push_back(ui);
			
			k=1;

			//Loop through this ions NNs, seeing if the kth NN is within a given radius
			do
			{
				pNN=coreTree.findNearestUntagged(*p,bCore,true);
				tagsToClear.push_back(pNN);
				k++;

			}while( pNN !=(size_t)-1 && k<coreKNN);
			

			//Core is only OK if the NN is good, and within the 
			//specified distance
			if(pNN == (size_t)-1)
			{
				coreOK[ui]=false;
				ASSERT(tagsToClear.back() == (size_t) -1);
				tagsToClear.pop_back(); //get rid of the -1
			}
			else
			{
				nnSqrDist=p->sqrDist(*(coreTree.getPt(pNN)));
				coreOK[ui] = nnSqrDist < coreDistSqr;
			}


			//reset the tags, so we can find near NNs
			coreTree.clearTags(tagsToClear);
			tagsToClear.clear();

			if(!(ui%PROGRESS_REDUCE))
			{
				progress.filterProgress= (unsigned int)(((float)ui/(float)coreTree.size())*100.0f);
				if(!(*callback)())
					return ABORT_ERR;
			}
		}


		
		for(size_t ui=coreOK.size();ui;)
		{
			ui--;

			if(!coreOK[ui])
			{
				//We have to convert the core ion to a bulk ion
				//as it is rejected.
				bulkIons.push_back(coreIons[ui]);
				coreIons[ui]=coreIons.back();
				coreIons.pop_back();
			}
		}

		//Re-Build the core KD tree
		coreTree.setCallback(callback);
		coreTree.setProgressPointer(&(progress.filterProgress));
		coreTree.resetPts(coreIons);
		coreTree.build();

		coreTree.getBoundCube(bCore);
		//==	
	}


	//Build the bulk tree (eg matrix ions.), as needed
	if(needBulkLink)
	{
		progress.step++;
		progress.stepName="Build Bulk";
		if(!(*callback)())
			return ABORT_ERR;
		bulkTree.setCallback(callback);
		bulkTree.setProgressPointer(&(progress.filterProgress));
		bulkTree.resetPts(bulkIons,false);
		bulkTree.build();
	}
	//----------
	
	
	//Step 2 in the  Process : Cluster Construction 
	//====
	//Loop over the solutes in the material, 
	//running searches from each solute. Group them using a queue
	//that keeps on adding newly found solutes until it can find no more
	//within a given radius. This becomes one cluster.

	//Update progress stuff
	progress.step++;
	progress.stepName="Core";
	progress.filterProgress=0;
	if(!(*callback)())
		return ABORT_ERR;
		

	vector<vector<size_t> > allCoreClusters,allBulkClusters;
	const float linkDistSqr=linkDist*linkDist;
	
	size_t progressCount=0;
	
	//When this queue is exhausted, move to the next cluster
	for(size_t ui=0;ui<coreTree.size();ui++)
	{
		size_t curPt;
		float curDistSqr;
		//Indicies of each cluster
		vector<size_t> soluteCluster,dummy;
		//Queue for atoms in this cluster waiting to be checked
		//for their NNs.
		std::queue<size_t> thisClusterQueue;
		
		
		//This solute is already clustered. move along.
		if(coreTree.getTag(ui))
			continue;
		coreTree.tag(ui);

	
	
		size_t clustIdx;
		thisClusterQueue.push(ui);
		soluteCluster.push_back(ui);
		do
		{
			curPt=thisClusterQueue.front();
			const Point3D *thisPt;
			thisPt=coreTree.getPt(curPt);
			curDistSqr=0;



			//Now loop over this solute's NNs not found by prev. method
			while(true)
			{
				ASSERT(curPt < coreTree.size());
				//Find the next point that we have not yet retreived
				//the find will tag the point, so we won't see it again
				ASSERT(bCore.isValid());
				clustIdx=coreTree.findNearestUntagged(
						*thisPt,bCore, true);


				ASSERT(clustIdx == -1 || coreTree.getTag(clustIdx));
				if(clustIdx != (size_t)-1)
				{
					curDistSqr=coreTree.getPt(clustIdx)->sqrDist(
							*(coreTree.getPt(curPt)) );

				}

				//Point out of clustering range, or no more points
				if(clustIdx == (size_t)-1 || curDistSqr > linkDistSqr)
				{
					//If the point was not tagged,
					//Un-tag the point; as it was too far away
					if(clustIdx !=(size_t)-1)
						coreTree.tag(clustIdx,false);
					
					thisClusterQueue.pop();
					break;
				}
				else
				{
					//Record it as part of the cluster	
					thisClusterQueue.push(clustIdx);
					soluteCluster.push_back(clustIdx);
				}
			

				progressCount++;
				if(!(progressCount%PROGRESS_REDUCE))
				{
					//Progress may be a little non-linear if cluster sizes are not random
					progress.filterProgress= (unsigned int)(((float)ui/(float)coreTree.size())*100.0f);
					if(!(*callback)())
						return ABORT_ERR;
				}
			}


		} // Keep looping whilst we have coreTree to cluster.
		while(thisClusterQueue.size() && clustIdx !=(size_t)-1);
			

		if(soluteCluster.size())
		{
			//Record the solute cluster in the total array
			allCoreClusters.push_back(dummy);
			allCoreClusters.back().swap(soluteCluster);
			soluteCluster.clear();
		}
	}

	//====

	//update progress
	if(!(*callback)())
		return ABORT_ERR;

	//NOTE : Speedup trick. If we know the cluster size at this point
	// and we know we don't want to count the bulk, we can strip out clusters
	// now, as we are going to do that anyway as soon as we return from our cluster
	// computation.
	// The advantage to doing it now is that we can (potentially) drop lots of clusters
	// from or analysis before we do the following steps, saving lots of time
	if(!sizeCountBulk && (nMin > 0 || nMax <(size_t)-1) && wantCropSize )
	{
		for(size_t ui=0;ui<allCoreClusters.size();)
		{
			size_t count;
			count =allCoreClusters[ui].size();
			if(count < nMin || count > nMax)
			{
				allCoreClusters.back().swap(allCoreClusters[ui]);
				allCoreClusters.pop_back();
			}
			else
				ui++;

		}
	}

	//Step 3 in the  Process : Bulk inclusion : AKA envelope
	//====
	//If there is no bulk link step, we don't need to do that.,
	//or any of the following stages
	if(needBulkLink)
	{
		//Update progress stuff
		progress.step++;
		progress.stepName="Bulk";
		progress.filterProgress=0;
		if(!(*callback)())
			return ABORT_ERR;

		bulkTree.getBoundCube(bBulk);

		//Compute the expected number of points that we would encapsulate
		//if we were to place  a sphere of size bulkLink in the KD domain.
		// This allows us to choose whether to use a bulk "grab" approach, or not.
		bool expectedPtsInSearchEnough;
		expectedPtsInSearchEnough=((float)bulkTree.size())/bBulk.volume()*4.0/3.0*M_PI*pow(bulkLink,3.0)> SPHERE_PRESEARCH_CUTOFF;

		//So-called "envelope" step.
		float bulkLinkSqr=bulkLink*bulkLink;
		size_t prog=PROGRESS_REDUCE;
		//Now do the same thing with the matrix, but use the clusters as the "seed"
		//positions
		for(size_t ui=0;ui<allCoreClusters.size();ui++)
		{
			//The bulkTree component of the cluster
			vector<size_t> thisBulkCluster,dummy;
			for(size_t uj=0;uj<allCoreClusters[ui].size();uj++)
			{
				size_t curIdx,bulkTreeIdx;
				curIdx=allCoreClusters[ui][uj];

				const Point3D *curPt;
				curPt=(coreTree.getPt(curIdx));


				//First do a grab of any sub-regions for cur pt
				// based upon intersecting KD regions
				// For readability, I have not factored this
				// out of the loop; it should not have  a giant performance
				// cost.
				//---
				if(expectedPtsInSearchEnough)
				{
					vector<pair<size_t,size_t> > blocks;
					bulkTree.getTreesInSphere(*curPt,bulkLinkSqr,bBulk,blocks);

					for(size_t uj=0; uj<blocks.size();uj++)
					{
						for(size_t uk=blocks[uj].first; uk<=blocks[uj].second;uk++)
						{
							if(!bulkTree.getTag(uk))
							{
								//Tag, then record it as part of the cluster	
								bulkTree.tag(uk);
								thisBulkCluster.push_back(uk);
							}
						}

					}
				}

				//--



				//Scan for bulkTree NN.
				while(true)
				{
					float curDistSqr;
					//Find the next point that we have not yet retrieved
					//the find will tag the point, so we won't see it again
					bulkTreeIdx=bulkTree.findNearestUntagged(
							*curPt,bBulk, true);
		

					if(bulkTreeIdx !=(size_t)-1)
					{
						curDistSqr=bulkTree.getPt(
							bulkTreeIdx)->sqrDist(*curPt);
					}

					//Point out of clustering range, or no more points
					if(bulkTreeIdx == (size_t)-1 || curDistSqr > bulkLinkSqr )
					{
						//Un-tag the point; as it was too far away
						if(bulkTreeIdx !=(size_t)-1)
							bulkTree.tag(bulkTreeIdx,false);
						break;
					}
					else
					{
						//Record it as part of the cluster	
						thisBulkCluster.push_back(bulkTreeIdx);
					}
				
					prog--;	
					if(!prog)
					{
						prog=PROGRESS_REDUCE;
						//Progress may be a little non-linear if cluster sizes are not random
						progress.filterProgress= (unsigned int)(((float)ui/(float)allCoreClusters.size())*100.0f);
						if(!(*callback)())
							return ABORT_ERR;

					}
				}

				

			}


			allBulkClusters.push_back(dummy);
			allBulkClusters.back().swap(thisBulkCluster);
			thisBulkCluster.clear();
		}	
	}
	//====



	//Step 4 in the  Process : Bulk erosion 
	//====
	//Check if we need the erosion step
	if(needErosion)
	{
		//Update progress stuff
		progress.step++;
		progress.stepName="Erode";
		progress.filterProgress=0;
		if(!(*callback)())
			return ABORT_ERR;

		//Now perform the "erosion" step, where we strip off previously
		//tagged matrix, if it is within a given distance of some untagged
		//matrix
		size_t numCounted=0;
		bool spin=false;

		const float dErosionSqr=dErosion*dErosion;
		#pragma omp parallel for 
		for(size_t ui=0;ui<allBulkClusters.size();ui++)
		{
			if(spin)
				continue;
			for(size_t uj=0;uj<allBulkClusters[ui].size();)
			{

				size_t bulkTreeId,nnId;
				bulkTreeId=allBulkClusters[ui][uj];

				//Find the nearest untagged bulkTree, but tagging is irrelevant, as it
				//is already tagged from previous "envelope" step.
				nnId = bulkTree.findNearestUntagged(
							*(bulkTree.getPt(bulkTreeId)),bBulk, false);
				
				if(nnId !=(size_t)-1)
				{
					float curDistSqr;
					curDistSqr=bulkTree.getPt(bulkTreeId)->sqrDist(
							*(bulkTree.getPt(nnId)) );
					if( curDistSqr < dErosionSqr)
					{
						//Bulk is to be eroded. Swap it with the vector tail
						//and pop it into oblivion.
						std::swap(allBulkClusters[ui][uj],
								allBulkClusters[ui].back());
						allBulkClusters[ui].pop_back();
						//Purposely do NOT advance the iterator, as we have
						//new data at our current position (or we have hit end of
						//array)
					}
					else
						uj++;

				}
				else
					uj++;

			}
			
			if(!(ui%PROGRESS_REDUCE))
			{
				#pragma omp critical 
				{
				numCounted+=PROGRESS_REDUCE;
				//Progress may be a little non-linear if cluster sizes are not random
				progress.filterProgress= (unsigned int)(((float)numCounted/(float)allBulkClusters.size())*100.0f);
				if(!(*callback)())
					spin=true;
				}

			}

		}

		if(spin)
			return ABORT_ERR;
	}
	//===

	progress.step++;
	progress.stepName="Re-Collate";
	progress.filterProgress=0;

	//update progress
	if(!(*callback)())
		return ABORT_ERR;
	clusteredCore.resize(allCoreClusters.size());
	clusteredBulk.resize(allBulkClusters.size());

	//Use a no-barrier construct, to avoid the 
	//flush wait in the middle
	#pragma omp parallel 
	{
		#pragma omp for
		for(size_t ui=0;ui<allCoreClusters.size();ui++)
		{
			clusteredCore[ui].resize(allCoreClusters[ui].size());
			for(size_t uj=0;uj<allCoreClusters[ui].size();uj++)
				clusteredCore[ui][uj]=coreIons[coreTree.getOrigIndex(allCoreClusters[ui][uj])];
		}


		#pragma omp for
		for(size_t ui=0;ui<allBulkClusters.size();ui++)
		{
			clusteredBulk[ui].resize(allBulkClusters[ui].size());
			for(size_t uj=0;uj<allBulkClusters[ui].size();uj++)
				clusteredBulk[ui][uj]=bulkIons[bulkTree.getOrigIndex(allBulkClusters[ui][uj])];
		}
	}

	//Update progress
	(*callback)();

	return 0;	
}

#ifdef DEBUG

bool ClusterAnalysisFilter::paranoidDebugAssert(
	const std::vector<vector<IonHit> > &core, const std::vector<vector<IonHit> > &bulk) const
{
	using std::cerr;
	using std::endl;

	for(size_t ui=0;ui<bulk.size(); ui++)
	{
		if(bulk[ui].size())
		{
			ASSERT(core[ui].size());
		}
	}

	//Check a few assertable properties of the algorithm
	switch(algorithm)
	{
		case CLUSTER_LINK_ERODE:
		{
			float bulkLinkSqr = bulkLink*bulkLink;

			//Every bulk ion should be within the enveloping distance from the corresponding core ions
			//If the bulklink is zero, we shouldn't have ANY bulk at all.

			bool failure=false;
			for(size_t ui=0;ui<bulk.size(); ui++)
			{
				if(failure)
					continue;
				for(size_t uj=0;uj<bulk[ui].size(); uj++)
				{
					bool haveNear;
					haveNear=false;
					//check bulk UI against core UI
					for(size_t um=0;um<core[ui].size();um++)
					{
						if(core[ui][um].getPos().sqrDist(bulk[ui][uj].getPos()) < bulkLinkSqr)
						{
							haveNear=true;
							break;
						}

					}

					if(!haveNear)
					{
						//What!? We Don't have an NN? How did we get 
						//clustered in the first place? This is wrong.
						failure=true;
						cerr << "FAILED! " << endl;

						cerr << "BULK:" << bulk[ui].size() << endl;
						for(unsigned int un=0;un<bulk[ui].size();un++)
						{
							cerr << bulk[ui][un].getPos() << endl;
						}
						
						cerr << "CORE:" << core[ui].size() << endl;
						for(unsigned int un=0;un<core[ui].size();un++)
						{
							cerr << core[ui][un].getPos() << endl;
						}


						break;
					}
				}
			}

			//Other Ideas:
			//*every core ion should have a core ion, other than itself
			// within the linkage distance
			ASSERT(!failure)
			break;
		}
		default:
		;
	}

	return true;
}
#endif

void ClusterAnalysisFilter::createRangedIons(const std::vector<const FilterStreamData *> &dataIn,vector<IonHit> &core,
			vector<IonHit> &bulk, ProgressData &p, bool (*callback)(void)) const
{

	//TODO: Progress reporting and callback
	ASSERT(haveRangeParent);
	const RangeStreamData *r=0;
	for(size_t ui=0;ui<dataIn.size();ui++)
	{
		if(dataIn[ui]->getStreamType() == STREAM_TYPE_RANGE)
		{
			r = (const RangeStreamData *)dataIn[ui];
			break;
		}
	}
	
	ASSERT(r);
	ASSERT(r->rangeFile->getNumIons() >=ionCoreEnabled.size());
	ASSERT(r->rangeFile->getNumIons() >=ionBulkEnabled.size());

	//Maps the ionID for ranges in the PARENT rangeStreamData, to
	//array offsets in the ionEnabled vectors.
	// For exmaple if ions 1 2 and 4 are  enabled in the PARENT
	// then this maps to offsets 1 2 and 3 in the ion(Core/Bulk)Enabled vectors 
	map<size_t,size_t> rangeEnabledMap;
	buildRangeEnabledMap(r,rangeEnabledMap);

	bool needBulk = bulkLink > std::numeric_limits<float>::epsilon();

	if(needBulk)
	{
		for(size_t ui=0;ui<dataIn.size();ui++)
		{
			if(dataIn[ui]->getStreamType() == STREAM_TYPE_IONS)
			{
				const IonStreamData *d;
				d=(const IonStreamData *)dataIn[ui];
				#pragma omp parallel for 
				for(size_t ui=0;ui<d->data.size();ui++)
				{
					unsigned int ionId;
					ionId=r->rangeFile->getIonID(d->data[ui].getMassToCharge());
					if(ionId!=(unsigned int)-1)
					{
						if( ionCoreEnabled[rangeEnabledMap[ionId]])
						{
							#pragma omp critical 
							core.push_back(d->data[ui]);
						}
						else if(ionBulkEnabled[rangeEnabledMap[ionId]]) //mutually exclusive with core (both cannot be true)
						{
							#pragma omp critical 
							bulk.push_back(d->data[ui]);
						}
					}
				}
			}
		
		
		}
	}
	else
	{
#pragma omp parallel for 
		for(size_t ui=0;ui<dataIn.size();ui++)
		{
			if(dataIn[ui]->getStreamType() == STREAM_TYPE_IONS)
			{
				const IonStreamData *d;
				d=(const IonStreamData *)dataIn[ui];
				for(size_t ui=0;ui<d->data.size();ui++)
				{
					unsigned int ionId;
					ionId=r->rangeFile->getIonID(d->data[ui].getMassToCharge());
					if(ionId!=(unsigned int)-1 && ionCoreEnabled[rangeEnabledMap[ionId]])
					{
						#pragma omp critical 
						core.push_back(d->data[ui]);
					}
				}
			}
		}
	}
	
}

PlotStreamData* ClusterAnalysisFilter::clusterSizeDistribution(const vector<vector<IonHit> > &core, 
						const vector<vector<IonHit> > &bulk) const
{
	//each cluster is represented by one entry in core and bulk
	ASSERT(bulk.size() == core.size() || !bulk.size());

	//Map that maps input number to frequency
	map<size_t,size_t> countMap;
	size_t maxSize=0;
	if(sizeCountBulk && bulk.size())
	{
		ASSERT(bulk.size() == core.size());
		for(size_t ui=0;ui<core.size();ui++)
		{
			size_t curSize;
			curSize=core[ui].size()+bulk[ui].size();
			//Check map for existing entry
			if(countMap.find(curSize) ==countMap.end())
			{
				//we haven't seen this size before, push it back
				countMap.insert(make_pair(curSize,1));
			}
			else
				countMap[curSize]++; //increment size.

			maxSize=max(maxSize,curSize);//update max size
		}
	}
	else	
	{
		for(size_t ui=0;ui<core.size();ui++)
		{
			size_t curSize;
			curSize=core[ui].size();
			//Check map for existing entry
			if(countMap.find(curSize) ==countMap.end())
			{
				//we haven't seen this size before, push it back
				countMap.insert(make_pair(curSize,1));
			}
			else
				countMap[curSize]++; //increment size.

			maxSize=max(maxSize,curSize); //update max size
		}
	}

	if(!maxSize)
		return 0;

	PlotStreamData* dist=new PlotStreamData;

	dist->parent=this;
	dist->r=1;
	dist->g=0;
	dist->b=0;


	dist->xLabel="Cluster Size";
	dist->yLabel="Frequency";

	dist->dataLabel=SIZE_DIST_DATALABEL;
	dist->logarithmic=logClusterSize;

	dist->plotType=PLOT_TYPE_STEM;
	dist->xyData.resize(countMap.size());
	std::copy(countMap.begin(),countMap.end(),dist->xyData.begin());

	return dist;
}


bool ClusterAnalysisFilter::stripClusterBySize(vector<vector<IonHit> > &clusteredCore,
						vector<vector<IonHit> > &clusteredBulk,
							bool (*callback)(void),ProgressData &progress) const

{

	//TODO: Parallelise? Could create a vector of bools and then 
	// spin through, find the ones we want to kill, then do a cull.
	// Progress reporting would be a bit more difficult.

	if(clusteredBulk.size() && sizeCountBulk)
	{
		//should be the same numbers of bulk as core
		ASSERT(clusteredBulk.size() == clusteredCore.size());
		for(size_t ui=clusteredCore.size();ui;)
		{
			ui--;
			//Count both bulk and core, and operate on both.
			size_t count;
			count =clusteredCore[ui].size() + clusteredBulk[ui].size() ;

			if(count < nMin || count > nMax)
			{
				clusteredCore[ui].swap(clusteredCore.back());
				clusteredCore.pop_back();
				clusteredBulk[ui].swap(clusteredBulk.back());
				clusteredBulk.pop_back();
			}
			if(!(ui%PROGRESS_REDUCE)  && clusteredCore.size())
			{
				progress.filterProgress= (unsigned int)(((float)ui/(float)clusteredCore.size()+1)*100.0f);
				
				if(!(*callback)())
					return ABORT_ERR;
			}
		}
	}
	else if(sizeCountBulk)
	{
		//OK, we don't have any bulk, but we wanted it.. Just work on core.
		for(size_t ui=clusteredCore.size();ui;)
		{
			ui--;

			if(clusteredCore[ui].size() <  nMin || clusteredCore[ui].size() > nMax)
			{
				clusteredCore[ui].swap(clusteredCore.back());
				clusteredCore.pop_back();
			}
			if(!(ui%PROGRESS_REDUCE) )
			{
				progress.filterProgress= (unsigned int)(((float)ui/(float)clusteredCore.size()+1)*100.0f);
				
				if(!(*callback)())
					return ABORT_ERR;
			}
		}
	}
	else
	{
		//OK, we have bulk, but we just want to count core;
		//but operate on both
		for(size_t ui=clusteredCore.size();ui;)
		{
			ui--;

			if(clusteredCore[ui].size() <  nMin || clusteredCore[ui].size() > nMax)
			{
				clusteredCore[ui].swap(clusteredCore.back());
				clusteredCore.pop_back();
				clusteredBulk[ui].swap(clusteredBulk.back());
				clusteredBulk.pop_back();
			}
			if(!(ui%PROGRESS_REDUCE) )
			{
				progress.filterProgress= (unsigned int)(((float)ui/(float)clusteredCore.size()+1)*100.0f);
				
				if(!(*callback)())
					return ABORT_ERR;
			}
		}

	}

	return true;
}

void ClusterAnalysisFilter::genCompositionVersusSize(const vector<vector<IonHit> > &clusteredCore,
		const vector<vector<IonHit> > &clusteredBulk, const RangeFile *rng,vector<PlotStreamData *> &plots) const
{
	ASSERT(rng && haveRangeParent)

	//Frequency of ions, as a function of composition.
	//The inner vector<size_t> is the the array of frequencies
	//for this particular sie for each ion (ie, the array is of size rng->getNumIons)
	map<size_t,vector<size_t> > countMap;
	
	bool needCountBulk=clusteredBulk.size() && sizeCountBulk;


	vector<size_t> ionFreq;
	ionFreq.resize(rng->getNumIons(),0);	
	//Create the frequency table, per ion
	//-------
	//TODO: Below, there is a multi-threaded version. When we are happy with the single-threaded code
	// try implementing the multi-threaded routine.
	//Count the cluster elements, then increment the frequency table
	if(needCountBulk)
	{
		ASSERT(clusteredBulk.size() == clusteredCore.size());
		
		//Create entries of zero vectors for ion counting
		for(size_t ui=0;ui<clusteredCore.size();ui++)
		{
			size_t curSize;
			curSize=clusteredCore[ui].size() + clusteredBulk[ui].size();
			if(countMap.find(curSize) ==countMap.end())
				countMap.insert(make_pair(curSize,ionFreq));
		}

		//Fill up the vectors by counting ions
		for(size_t ui=0;ui<clusteredCore.size();ui++)
		{
			size_t curSize,offset;
			curSize=clusteredCore[ui].size() + clusteredBulk[ui].size();
			for(size_t uj=0;uj<clusteredCore[ui].size();uj++)
			{
				offset= rng->getIonID(clusteredCore[ui][uj].getMassToCharge());
				countMap[curSize][offset]++;
			}
			
			for(size_t uj=0;uj<clusteredBulk[ui].size();uj++)
			{
				offset= rng->getIonID(clusteredBulk[ui][uj].getMassToCharge());
				countMap[curSize][offset]++;
			}
		}
	}
	else
	{
		//Create entries of zero vectors for ion counting
		for(size_t ui=0;ui<clusteredCore.size();ui++)
		{
			size_t curSize;
			curSize=clusteredCore[ui].size();
			if(countMap.find(curSize) ==countMap.end())
				countMap.insert(make_pair(curSize,ionFreq));
		}

		//Now count the ions
		for(size_t ui=0;ui<clusteredCore.size();ui++)
		{
			size_t offset,curSize;
			curSize=clusteredCore[ui].size();
			for(size_t uj=0;uj<clusteredCore[ui].size();uj++)
			{
				offset= rng->getIonID(clusteredCore[ui][uj].getMassToCharge());
				
				//this should not happen, as to cluster the ion,it must be ranged
				ASSERT(offset!=(size_t)-1); 

				countMap[curSize][offset]++;
			}
		}
	}
	//-------

	//Now that we have the freq table; we need to discard any elements that are not 
	//completely empty across the map.
	//
	// A vector that tells us if a given ionID is zero for all map entries. I.e. not in cluster
	vector<bool> isZero; 
	isZero.resize(rng->getNumIons(),true);
	
	for(map<size_t,vector<size_t> >::iterator 
			it=countMap.begin(); it!=countMap.end();++it)
	{
		for(size_t ui=0;ui<it->second.size();ui++)
		{
			if(it->second[ui])
				isZero[ui]=false;
		}
	}



	//Ok now we know which frequency values are non-zero. Good!
	// We need to build the plots, and their respective XY data,
	// also we should normalise the compositions (if needed).
	plots.reserve(rng->getNumIons());
	for(size_t ui=0;ui<rng->getNumIons();ui++)
	{
		//we don't need to plot this,
		//as we didn't have any clustered ions of this type
		if(isZero[ui])
			continue;
		
		//Make a new plot
		PlotStreamData *p;
		p=new PlotStreamData;
		p->parent=this;

		RGBf ionColour;
		ionColour=rng->getColour(ui);

		//Colour it as per the range file
		p->r=ionColour.red;
		p->g=ionColour.green;
		p->b=ionColour.blue;

		p->xLabel="Cluster Size";
		if(normaliseComposition)
			p->yLabel="Composition";
		else
			p->yLabel="Frequency";

		p->dataLabel=string(CHEM_DIST_DATALABEL) + string(":") + rng->getName(ui);
		p->logarithmic=logClusterSize && !normaliseComposition;

		p->plotType=PLOT_TYPE_STEM;

		p->xyData.resize(countMap.size());

		size_t offset;
		offset=0;
		//set the data from our particular ion
		for(map<size_t,vector<size_t> > ::iterator it=countMap.begin();it!=countMap.end();++it)
		{
			p->xyData[offset].first=it->first;
			p->xyData[offset].second=it->second[ui];

			//if we need to normalise compositions, we have to normalise over all 
			//ion types for this cluster size (ie the sum of this vector)
			if(normaliseComposition)
			{
				size_t sum=0;
				for(size_t uk=0; uk<it->second.size();uk++)
					sum+=it->second[uk];
				p->xyData[offset].second /=(float)sum;
			}
			offset++;
		}

		plots.push_back(p);
	}


}

/* I think this is roughly OK (written, but not compiled/tested), but we are going to get a code explosion, as
 * I have to enumerate all the differeing paths, such that I don't have to
 * manually *check* each boolean option (count bulk/no count bulk, use paralell/use single)
 * So this is disabled for now.
void ClusterAnalysisFilter::makeRangedFreqMap(const <vector<vector<IonHIT> > &clusteredCore,
			const vector<vector<IonHit> > &clusteredBulk, const RangeFile *rng.
			map<size_t,vector<size_t> > countMap)
{
	vector<size_t> dummyIonFreqTable;
	dummyInFreqTable.resize(rng->getNumIons(),0);	

#ifdef OPENMP
	//parallel version
	//---
	vector< map<size_t,vector<size_t> > > countMaps;
	unsigned int maxThreads=omp_get_max_num_threads();
	
	//OK, so the composition will be the core and bulk both;
	//Now, lets figure out who belongs where.
	#pragma omp parallel for
	for(size_t ui=0;ui<clusteredCore.size();ui++)
	{
		unsigned int thread = omp_get_thread_num();
		if(countMaps[thread].find(curSize) ==countMaps.end())
			countMaps[thread].insert(make_pair(curSize,dummyIonFreqTable));
	}
	
	#pragma omp parallel for 	
	for(size_t ui=0;ui<clusteredCore.size();ui++)
	{
		unsigned int thread = omp_get_thread_num();
		for(size_t uj=0;uj<clusteredCore[ui].size();uj++)
		{
			offset= rng->getIonID(clusteredCore[ui][uj]);
			countMaps[thread][curSize][offset]++;
		}
		
		for(size_t uj=0;uj<clusteredBulk[ui].size();uj++)
		{
			offset= rng->getIonID(clusteredBulk[ui][uj]);
			countMaps[thread][curSize][offset]++;
		}

	}


	//merge the differing threads into a single map
	//which is our final frequency tally


	//firstly we have to "prepare" the final "master" map.
	//ie, fill it with zeros at the appropriate sizes.
	for(size_t ui=0;ui<countMaps.size();ui++)
	{
		//Loop through the map, and prepare the final "master" map with
		//zero values
		for(map<size_t,vector<size_t> >::const_iterator 
			it=countMaps.begin(); it!=countMaps.end();it++)
		{
			if(countMap.find(it->first) == countMaps.end())
				countMap.insert(it->first,dummyIonFreqTable);
		}
	}

	//Now we have to sum the thread-specific maps into the master map
	for(size_t ui=0;ui<countMaps.size();ui++)
	{
		for(map<size_t,vector<size_t> >::const_iterator 
			it=countMaps.begin(); it!=countMaps.end();it++)
			countMap[it->first]+=it->second;
	}
	//---
#else
	//Single-threaded version
	//---
	for(size_t ui=0;ui<clusteredCore.size();ui++)
	{
		if(countMap.find(curSize) ==countMap.end())
			countMap.insert(make_pair(curSize,ionFreq));
	}
	
	for(size_t ui=0;ui<clusteredCore.size();ui++)
	{
		for(size_t uj=0;uj<clusteredCore[ui].size();uj++)
		{
			offset= rng->getIonID(clusteredCore[ui][uj]);
			countMap[curSize][offset]++;
		}
		
		for(size_t uj=0;uj<clusteredBulk[ui].size();uj++)
		{
			offset= rng->getIonID(clusteredBulk[ui][uj]);
			countMap[curSize][offset]++;
		}

	}
#endif
}
 */
