/*
 *	ionInfo.h -Filter to compute various properties of valued point cloud
 *	Copyright (C) 2015, D Haley 

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
#ifndef LUKASANALYSIS_H
#define LUKASANALYSIS_H

#include "../filter.h"
#include "../../common/translation.h"
#include "openvdb_includes.h"
#include "contribution_transfer_function_TestSuite/CTF_functions.h"

class LukasAnalysisFilter : public Filter
{
	private:
	
		//Enabled ions to choose for numerator
		std::vector<char> enabledIons[2];

		float voxel_size; // declaration here, definition in the source file
	
		float iso_level;  // declaration here, definition in the source file
		
		float adaptivity;
		
		bool numeratorAll, denominatorAll;
		//This is the filters enabled ranges
		RangeStreamData *rsdIncoming;
			
		ColourRGBAf rgba;

		//!Colour map to use when using axial slices
		unsigned int colourMap;

		//Number of colour levels for colour map
		size_t nColours;
		//Whether to show the colour map bar or not
		bool showColourBar;
		//Whether to use an automatic colour bound, or to use user spec
		bool autoColourMap;
		//Colour map start/end
		float colourMapBounds[2];
		
		//Cache to use for vdbgrid info
		// console warning: non-static data member initializers only available with -std=c++11 or -std=gnu++11
		openvdb::FloatGrid::Ptr vdbCache;

	public:
		//!Constructor
		LukasAnalysisFilter();
		~LukasAnalysisFilter() { if(rsdIncoming) delete rsdIncoming;}
		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;
		
		virtual void clearCache();

		//!Get approx number of bytes for caching output
		size_t numBytesForCache(size_t nObjects) const;
		
		//size_t getGridMemUsage(openvdb::FloatGrid::Ptr grid);

		//Perform filter intialisation, for pre-detection of range data
		virtual void initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut);
		
		//!Apply filter to new data, updating cache as needed. Vector
		// of returned pointers must be deleted manually, first checking
		// ->cached.
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
							std::vector<const FilterStreamData *> &dataOut,
							ProgressData &progress);

		//!return type ID
		unsigned int getType() const { return FILTER_TYPE_LUKAS_ANALYSIS;}

		//!Return filter type as std::string
		std::string typeString() const { return std::string(TRANS("Lukas Analysis"));};

		//!Get the properties of the filter, in key-value form. First vector is for each output.
		void getProperties(FilterPropGroup &propertyList) const;

		//!Set the properties for the nth filter,
		//!needUpdate tells us if filter output changes due to property set
		bool setProperty( unsigned int key,
					const std::string &value, bool &needUpdate);

		void setPropFromBinding( const SelectionBinding &b) ;

		//!Get the human readable error string associated with a particular error code during refresh(...)
		std::string getSpecificErrString(unsigned int code) const;

		//!Dump state to output stream, using specified format
		/* Current supported formats are STATE_FORMAT_XML
		 */
		bool writeState(std::ostream &f, unsigned int format,
							unsigned int depth) const;

		//!Read state from XML  stream, using xml format
		/* Current supported formats are STATE_FORMAT_XML
		 */
		bool readState(xmlNodePtr& n, const std::string &packDir="");

		//!Get the bitmask encoded list of filterStreams that this filter blocks from propagation.
		// i.e. if this filterstream is passed to refresh, it is not emitted.
		// This MUST always be consistent with ::refresh for filters current state.
		unsigned int getRefreshBlockMask() const;

		//!Get the bitmask encoded list of filterstreams that this filter emits from ::refresh.
		// This MUST always be consistent with ::refresh for filters current state.
		unsigned int getRefreshEmitMask() const;
		
		//!Get the bitmask encoded list of filterstreams that this filter may use during ::refresh.
		unsigned int getRefreshUseMask() const;

};


#endif
