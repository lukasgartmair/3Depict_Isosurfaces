/*
 * 	filtertree.h - Filter tree topology and data propagation handling
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

#ifndef FILTERTREE_H
#define FILTERTREE_H

#include "tree.hh"
#include "filter.h"
#include "common/bimap.h"

#include <map>
#include <utility>

typedef std::pair<Filter *,vector<const FilterStreamData * > > FILTER_OUTPUT_DATA;



//Generic filter tree refresh error codes
enum
{
	FILTERTREE_REFRESH_ERR_BEGIN=100000,
	FILTERTREE_REFRESH_ERR_MEM,
	FILTERTREE_REFRESH_ERR_ENUM_END
};



//!Tree of filters, which link together to perform an analysis
// this class allows for manupulating and execution of filters
class FilterTree
{
	private:

		
		//!Caching strategy
		unsigned int cacheStrategy;
		
		//!Maximum size for cache (percent of available ram).
		float maxCachePercent;
		
		//!Filters that provide and act upon datastreams. 
		tree<Filter *> filters;
	
			
		//!Get the filter refresh seed points in tree, by examination of tree caches, block/emit of filters
		//and tree topology
		void getFilterRefreshStarts(vector<tree<Filter *>::iterator > &propStarts) const;
		
			
		
#ifdef DEBUG
		//!Check that the output of filter refresh functions
		void checkRefreshValidity(const vector< const FilterStreamData *> &curData, 
					const Filter *refreshFilter) const;
#endif
		
		
		//TODO: Move me to tree.hh	
		//!Returns true if the testChild is a child of testParent.
		// returns false if testchild == testParent, or if the testParent
		// is not a parent of testChild.
		bool isChild(const tree<Filter *> &treeInst,
				const tree<Filter *>::iterator &testParent,
				tree<Filter *>::iterator testChild) const;


		static size_t countChildFilters(const tree<Filter *> &treeInst,
					const vector<tree<Filter *>::iterator> &nodes);
	public:
		FilterTree();
		~FilterTree();

		FilterTree(const FilterTree &orig);

		void swap(FilterTree &);
		//Note that the = operator creates a *CLONE* of the orignal tree,
		// not an exact duplicate. underlying pointers will not be the same
		const FilterTree &operator=(const FilterTree &orig);
	
		//Return iterator to tree contents begin.	
		tree<Filter *>::pre_order_iterator depthBegin() const { return filters.begin();};
		//Return iterator to tree contents end
		tree<Filter *>::pre_order_iterator depthEnd() const { return filters.end();}
		//Return depth of a given iterator
		size_t maxDepth() const;

		size_t depth(const tree<Filter*>::pre_order_iterator &it) const ;

		//Get a reference to the underlying tree
		const tree<Filter*> &getTree() const  { return filters;}

		//!Return the number of filters
		size_t size() const {return filters.size();};
	
		//!Remove all tree contents
		void clear(); 

		bool contains(const Filter *f) const;
	
		//Refresh functions	
		//---	
		//!Run the initialisation stage of the filter processing
		void initFilterTree() const;

		bool setFilterProperty(Filter *f, unsigned int key,
				const std::string &value, bool &needUpdate);
		
		//!Refresh the entire filter tree. Whilst this is public, great care must be taken in
		// deleting the filterstream data correctly. To do this, use the "safeDeleteFilterList" function.
		unsigned int refreshFilterTree(	
			std::list<FILTER_OUTPUT_DATA> &outData,
			std::vector<SelectionDevice *> &devices,std::vector<std::pair<const Filter *,string> > &consoleMessages,
						ProgressData &curProg, bool (*callback)(bool)) const;

		static string getRefreshErrString(unsigned int errCode);
		
		//!Safely delete data generated by refreshFilterTree(...). 
		//a mask can be used to *prevent* STREAM_TYPE_blah from being deleted. Deleted items are removed from the list.
		void safeDeleteFilterList(std::list<FILTER_OUTPUT_DATA> &outData, 
								size_t typeMask=STREAMTYPE_MASK_ALL, bool maskPrevents=false) const;

		//!compute the integrated (accumulated) propagation maps for emission and blocking.
		// For emission this value gives the possible types that
		//  can be emitted from each filter. It is not possible to
		//  emit types not in the mask
		// For blocking, give the types that cannot reach the tree output (leaf exit)
		void getAccumulatedPropagationMaps(map<Filter*, size_t> &emitTypes, map<Filter*,size_t> &blockTypes) const;
		//---


		//!function for the loading of a filter tree from its XML representation
		unsigned int loadXML(const xmlNodePtr &treeParent, 
				std::ostream &errStream, const std::string &stateDir);


		//Write out the filters into their XML representation
		bool saveXML(std::ofstream &f, std::map<string,string> &fileMapping,
					bool writePackage, bool useRelativePaths, unsigned int minTabDepth=0) const;


		//!Convert tree to a series of flat strings representing the topology
		//TODO: COnvert to bimap
		void serialiseToStringPaths(std::map<const Filter *,string > &serialisedPaths) const;
		void serialiseToStringPaths(std::map<string,const Filter *> &serialisedPaths) const;


		//Topological alteration  & examination functions
		//----------	
		//!Remove an element and all sub elements from the tree, 
		void removeSubtree(Filter *f);
		
		//!Add a new filter to the tree. Note that pointer will be released
		// by filter destructor
		void addFilter(Filter *f, const Filter *parent);
		
		//!Add a new tree as a subtree to a node 
		void addFilterTree(FilterTree &f, const Filter *parent);

		//!Move a branch of the tree to a new position
		bool reparentFilter(Filter *f, const Filter *newParent);
		//!Duplicate a branch of the tree to a new position. Do not copy cache,
		bool copyFilter(Filter *id, const Filter *newParent);

		//Obtain a copy of the filters in the specified subtree 
		void cloneSubtree(FilterTree &f,Filter *targetFilt) const;
		//---------
	

		//Filter alteration functions
		//---------	
		//!Set the filter user text
		bool setFilterString(Filter *, const std::string &s);
		
		//!Invalidate the cache of a given Filter and all its children. 
		// set to 0 to clear all.
		void clearCache(const Filter *filt,bool includeSelf=true);
		
		
		//!Invalidate the cache of a given type of filter
		//	and all their children.
		void clearCacheByType(unsigned int type);
		
		//!Return all of a given type of filter from the filter tree. Type must be the exact type of filter - it is not a mask
		void getFiltersByType(std::vector<const Filter *> &filters, unsigned int type) const;
		
		//!Make the filter system safe (non-hazardous)
		void removeHazardousContents();

		
		//!Used to remove potentially hazardous filters 
		//(filters that can do nasty things to computers, like executing commands)
		//which may have come from unsafe sources
		void stripHazardousContents();
		
		//!return true if the tree contains hazardous filters
		bool hasHazardousContents() const ;
		
		//!Force a wipe of all caches in the filter tree
		void purgeCache();
	

		//!Returns true if any of the filters (incl. stash)
		//return a state override (i.e. refer to external entities, such as files)
		bool hasStateOverrides() const ;

		bool hasUpdates() const ;

		//---------	
		
		void setCachePercent(unsigned int newCache);
		
		//Overwrite the contents of the pointed-to range files with
		// the map contents
		void modifyRangeFiles(const map<const RangeFile *, const RangeFile *> &toModify);
};

#endif
