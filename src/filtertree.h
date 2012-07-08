/*
 * 	filtertree.h - Filter tree topology and data propagation handling
 *	Copyright (C) 2011, D Haley 

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
#include "select.h"
#include "filter.h"


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
		
			
		//Retrieve the filter's pointer from its ID value	
		Filter* getFilterByIdNonConst(unsigned long long filterId) const;
		
#ifdef DEBUG
		//!Check that the output of filter refresh functions
		void checkRefreshValidity(const vector< const FilterStreamData *> curData, 
					const Filter *refreshFilter) const;
#endif
		
		
		//TODO: Move me to tree.hh	
		//!Returns true if the testChild is a child of testParent.
		// returns false if testchild == testParent, or if the testParent
		// is not a parent of testChild.
		bool isChild(const tree<Filter *> &treeInst,
				tree<Filter *>::iterator testParent,
				tree<Filter *>::iterator testChild) const;


		size_t countChildFilters(const tree<Filter *> &treeInst,
					const vector<tree<Filter *>::iterator> &nodes) const;
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

		size_t depth(const tree<Filter*>::pre_order_iterator &it) const 
			{ ASSERT(std::find(filters.begin(),filters.end(),*it)!=filters.end()); return filters.depth(it);};

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

		bool setFilterProperty(Filter *f, unsigned int set, 
						unsigned int key, const std::string &value, bool &needUpdate);
		
		//!Refresh the entire filter tree. Whilst this is public, great care must be taken in
		// deleting the filterstream data correctly. To do this, use the "safeDeleteFilterList" function.
		unsigned int refreshFilterTree(	
			std::list<std::pair<Filter *, std::vector<const FilterStreamData * > > > &outData,
			std::vector<SelectionDevice<Filter> *> &devices,std::vector<std::pair<const Filter *,string> > &consoleMessages,
						ProgressData &curProg, bool (*callback)(bool));
		
		//!Safely delete data generated by refreshFilterTree(...). 
		//a mask can be used to *prevent* STREAM_TYPE_blah from being deleted. Deleted items are removed from the list.
		void safeDeleteFilterList(std::list<std::pair<Filter *, std::vector<const FilterStreamData * > > > &outData, 
								size_t typeMask=STREAMTYPE_MASK_ALL, bool maskPrevents=false) const;

		void getAccumulatedPropagationMaps(map<Filter*, size_t> &emitTypes, map<Filter*,size_t> &blockTypes) const;
		//---


		//!function for the loading of a filter tree from its XML representation
		unsigned int loadXML(const xmlNodePtr &treeParent, 
				std::ostream &errStream, const std::string &stateDir);


		//Write out the filters into their XML representation
		bool saveXML(std::ofstream &f, std::map<string,string> &fileMapping,
					bool writePackage, bool useRelativePaths, unsigned int minTabDepth=0) const;


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
		
		//!Invalidate the cache of a given Filter and all its children. set to 0 to clear all.
		void clearCache(const Filter *filt);
		
		
		//!Invalidate the cache of a given type of filter
		//	and all their children.
		void clearCacheByType(unsigned int type);
		
		//!Return all of a given type of filter from the filter tree
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
};

#endif
