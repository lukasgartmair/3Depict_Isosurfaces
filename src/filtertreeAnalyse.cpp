#include "filtertreeAnalyse.h"

#include "translation.h"

#include "filter.h"

//Needed to obtain filter data keys
//----
#include "filters/dataLoad.h"
#include "filters/ionDownsample.h"
#include "filters/compositionProfile.h"

//----

bool findKey( const vector<vector<unsigned int > > &keys, unsigned int targetKey,
		size_t &i, size_t &j)
{
	for(size_t ui=0;ui<keys.size();ui++)
	{
		for(size_t uj=0;uj<keys[ui].size();uj++)
		{
			if(keys[ui][uj] == targetKey)
			{
				i=ui;
			       	j=uj;
				return true;
			}
		}
	}

	return false;
}

void FilterTreeAnalyse::getAnalysisResults(std::vector<FILTERTREE_ERR> &errs) const
{
	errs.resize(analysisResults.size());
	std::copy(analysisResults.begin(),analysisResults.end(),errs.begin());
}

void FilterTreeAnalyse::analyse(const FilterTree &f)
{
	f.getAccumulatedPropagationMaps(emitTypes,blockTypes);

	blockingPairError(f);

	spatialSampling(f);


	emitTypes.clear();
	blockTypes.clear();

}




void FilterTreeAnalyse::blockingPairError(const FilterTree &f)
{
	//Examine the emit and block/use masks for each filter's parent (emit)
	// child relationship(block/use), such that in the case of a child filter that is expecting
	// a particular input, but the parent cannot generate it

	const tree<Filter *> &treeFilt=f.getTree();
	for(tree<Filter*>::pre_order_iterator it = treeFilt.begin(); it!=treeFilt.end(); it++)
	{

		tree_node_<Filter*> *myNode=it.node->first_child;
		
		size_t parentEmit;	
		parentEmit = emitTypes[(*it) ]| (*it)->getRefreshEmitMask();

		while(myNode)
		{
			Filter *childFilter;
			childFilter = myNode->data;

			size_t curBlock,curUse;
			curBlock=blockTypes[childFilter] | childFilter->getRefreshBlockMask();
			curUse=childFilter->getRefreshUseMask();

			//If the child filter cannot use and blocks all parent emit values
			// emission of the all possible output filters,
			// then this is a bad filter pairing
			bool passedThrough;
			passedThrough=parentEmit & ~curBlock;

			if(!parentEmit && curUse)
			{
				FILTERTREE_ERR treeErr;
				treeErr.reportedFilters.push_back(childFilter);
				treeErr.reportedFilters.push_back(*it);
				treeErr.verboseReportMessage = TRANS("Parent filter has no output, but filter requires input -- there is no point in placing a child filter here.");
				treeErr.shortReportMessage = TRANS("Leaf-only filter with child");
				treeErr.severity=SEVERITY_ERROR; //This is definitely a bad thing.
			
				analysisResults.push_back(treeErr);
			}
			else if(!(parentEmit & curUse) && !passedThrough )
			{
				FILTERTREE_ERR treeErr;
				treeErr.reportedFilters.push_back(childFilter);
				treeErr.reportedFilters.push_back(*it);
				treeErr.verboseReportMessage = TRANS("Parent filters' output will be blocked by child, without use. Parent results will be dropped.");
				treeErr.shortReportMessage = TRANS("Bad parent->child pair");
				treeErr.severity=SEVERITY_ERROR; //This is definitely a bad thing.
			
				analysisResults.push_back(treeErr);
			}
			//If the parent does not emit a useable objects 
			//for the child filter, this is bad too.
			// - else if, so we don't double up on warnings
			else if( !(parentEmit & curUse))
			{
				FILTERTREE_ERR treeErr;
				treeErr.reportedFilters.push_back(childFilter);
				treeErr.reportedFilters.push_back(*it);
				treeErr.verboseReportMessage = TRANS("First filter does not output anything useable by child filter. Child filter not useful.");
				treeErr.shortReportMessage = TRANS("Bad parent->child pair");
				treeErr.severity=SEVERITY_ERROR; //This is definitely a bad thing.
			
				analysisResults.push_back(treeErr);

			}


			//Move to next sibling
			myNode = myNode->next_sibling;
		}


	}	

}

bool filterIsSampling(const Filter *f)
{

	bool affectsSampling=false;


	FilterProperties props;
	f->getProperties(props);



	size_t ui,uj;
	switch(f->getType())
	{
		case FILTER_TYPE_POSLOAD:
		{
			if(findKey(props.keys,DATALOAD_KEY_SAMPLE,ui,uj))
			{
				//Check if load limiting is on
				//Not strictly true. If data file is smaller (in MB) than this number
				// (which we don't know here), then this will be false.
				affectsSampling = (props.data[ui][uj].second != "0");
			}


			break;
		}
		case FILTER_TYPE_IONDOWNSAMPLE:
		{
			findKey(props.keys,KEY_IONDOWNSAMPLE_FIXEDOUT,ui,uj);
			
			if(props.data[ui][uj].second == "1")
			{
				//If using  fixed output mode, then
				// we may affect the output ion density
				// if the count is low. How low? 
				// We don't know with the information to hand...
				affectsSampling=true;
			}
			else
			{
				//If randomly sampling, then we are definitely affecting the results
				//if we are not including every ion
				findKey(props.keys,KEY_IONDOWNSAMPLE_FRACTION,ui,uj);
				float sampleFrac;
				stream_cast(sampleFrac,props.data[ui][uj].second);
				affectsSampling=(sampleFrac < 1.0f);
			}

			break;
		}
	}


	return affectsSampling;

}

bool affectedBySampling(const Filter *f, bool haveRngParent)
{
	FilterProperties props;
	f->getProperties(props);
	
	bool affected;
	//See if filter is configured to affect spatial analysis
	switch(f->getType())
	{
		case FILTER_TYPE_CLUSTER_ANALYSIS:
		{
			affected=haveRngParent;
			break;
		}
		case FILTER_TYPE_COMPOSITION:
		{
			for(unsigned int ui=0;ui<props.keys.size();ui++)
			{
				for(unsigned int uj=0;uj<props.keys[ui].size();uj++)
				{
					if( props.keys[ui][uj] == COMPOSITION_KEY_NORMALISE)
					{
						//If using normalise mode, and we do not have a range parent
						//then filter is in "density" plotting mode, which is affected by
						//this analysis
						affected= (props.data[ui][uj].second == "1" && !haveRngParent);
						break;
					}
				}
			}
			break;
		}
		case FILTER_TYPE_SPATIAL_ANALYSIS:
		{
			affected=true;
			break;
		}
	}

	return affected;
}


void FilterTreeAnalyse::spatialSampling(const FilterTree &f)
{
	//True if spatial sampling is (probably) happening for children of 
	//filter. 
	vector<int> affectedFilters;
	affectedFilters.push_back(FILTER_TYPE_CLUSTER_ANALYSIS); //If have range parent
	affectedFilters.push_back(FILTER_TYPE_COMPOSITION); //If using density
	affectedFilters.push_back(FILTER_TYPE_SPATIAL_ANALYSIS); //If using density

	const tree<Filter *> &treeFilt=f.getTree();
	for(tree<Filter*>::pre_order_iterator it(treeFilt.begin()); it!=treeFilt.end(); it++)
	{
		//Check to see if we have a filter that can cause sampling
		if(filterIsSampling(*it))
		{
			tree_node_<Filter*> *childNode=it.node->first_child;

			if(childNode)
			{		

				//TODO: Not the most efficient method of doing this...
				//shouldn't need to continually compute depth to iterate over children	
				size_t minDepth=treeFilt.depth(it);	
				for(tree<Filter*>::pre_order_iterator itJ(childNode); treeFilt.depth(itJ) > minDepth;itJ++)
				{
					//ignore filters that are not affected by spatial sampling
					size_t filterType;
					filterType=(*itJ)->getType();
					if(std::find(affectedFilters.begin(),affectedFilters.end(),filterType)== affectedFilters.end())
						continue;

					childNode=itJ.node;

					//Check to see if we have a "range" type ancestor
					// - we will need to know this in a second
					bool haveRngParent=false;
					{
						tree_node_<Filter*> *ancestor;
						ancestor = childNode->parent;
						while(true) 
						{
							if(ancestor->data->getType() == FILTER_TYPE_RANGEFILE)
							{
								haveRngParent=true;
								break;
							}

							if(!ancestor->parent)
								break;

							ancestor=ancestor->parent;

						}
					}

					if(affectedBySampling(*itJ,haveRngParent))
					{
						FILTERTREE_ERR treeErr;
						treeErr.reportedFilters.push_back(*it);
						treeErr.reportedFilters.push_back(*itJ);
						treeErr.shortReportMessage=TRANS("Spatial results possibly altered");
						treeErr.verboseReportMessage=TRANS("Filters and settings selected that could alter reported results that depend upon density. Check to see if spatial sampling may be happening in the filter tree - this warning is provisional only.");						treeErr.severity=SEVERITY_WARNING;

						analysisResults.push_back(treeErr);
					}
				}
			}

			//No need to walk child nodes	
			it.skip_children();
		}
		


	}
}



