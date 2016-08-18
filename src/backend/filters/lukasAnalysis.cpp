/*
 *	ionInfo.cpp -Filter to compute various properties of valued point cloud
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
#include "lukasAnalysis.h"

#include "filterCommon.h"
#include "algorithms/mass.h"

#include "openvdb_includes.h"

using std::vector;
using std::string;
using std::pair;
using std::make_pair;

typedef struct coords_struct {
float x;
float y;
float z;
} coord;

// these are not mebers of lukasAnalysis.h
int roundUp(float numToRound, float multiple)
{
    if (multiple == 0)
        return numToRound;
    float remainder = fmod(abs(numToRound), multiple);
    if (remainder == 0)
        return numToRound;
    if (numToRound < 0)
        return -(abs(numToRound) - remainder);
    else
        return numToRound + multiple - remainder;
}

coord GetVoxelIndex(coord *vec, float voxsize)
{
    int cx = 0;
    int cy = 0;
    int cz = 0;

    cx = roundUp(vec->x, voxsize);
    cy = roundUp(vec->y, voxsize);
    cz = roundUp(vec->z, voxsize);

    coord voxel_index = {cx/voxsize, cy/voxsize , cz/voxsize};
    return voxel_index;

} 


LukasAnalysisFilter::LukasAnalysisFilter() : 
	rsdIncoming(0)
{
	cacheOK=false;
	cache=true; //By default, we should cache, but decision is made higher up
}

void LukasAnalysisFilter::initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut)
{
	const RangeStreamData *c=0;
	//Determine if we have an incoming range
	for (size_t i = 0; i < dataIn.size(); i++) 
	{
		if(dataIn[i]->getStreamType() == STREAM_TYPE_RANGE)
		{
			c=(const RangeStreamData *)dataIn[i];

			break;
		}
	}

	//we no longer (or never did) have any incoming ranges. Not much to do
	if(!c)
	{
		//delete the old incoming range pointer
		if(rsdIncoming)
			delete rsdIncoming;
		rsdIncoming=0;
	}
	else
	{
		//If we didn't have an incoming rsd, then make one up!
		if(!rsdIncoming)
		{
			rsdIncoming= new RangeStreamData;
			*rsdIncoming=*c;
		}
		else
		{

			//OK, so we have a range incoming already (from last time)
			//-- the question is, is it the same one we had before ?
			//Do a pointer comparison (its a hack, yes, but it should work)
			if(rsdIncoming->rangeFile != c->rangeFile)
			{
				//hmm, it is different. well, trash the old incoming rng
				delete rsdIncoming;

				rsdIncoming = new RangeStreamData;
				*rsdIncoming=*c;
			}

		}

	}

}

Filter *LukasAnalysisFilter::cloneUncached() const
{
	LukasAnalysisFilter *p=new LukasAnalysisFilter();

	//We are copying whether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

/// here the actual filter work is done 
unsigned int LukasAnalysisFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress)
{
	openvdb::initialize();

	float voxel_size = 3.0f; // definition here, declaration in the header file

	const float background = 1.0f;	

	// initialize the main grid for aluminum and copper
	openvdb::FloatGrid::Ptr grid = openvdb::FloatGrid::create(background);
	openvdb::FloatGrid::Accessor accessor = grid->getAccessor();

	// 1st subgrid for copper
	openvdb::FloatGrid::Ptr subgrid1 = openvdb::FloatGrid::create(background);
	openvdb::FloatGrid::Accessor subaccessor1 = subgrid1->getAccessor();


	int counter = 0;
	int counter_cu = 0;

	const RangeFile *r = rsdIncoming->rangeFile;

	for(size_t ui=0;ui<dataIn.size();ui++)
	{
		if(dataIn[ui]->getStreamType() != STREAM_TYPE_IONS)
			continue;

		const IonStreamData  *ions; 
		ions = (const IonStreamData *)dataIn[ui];

		coord voxel_index = {0,0,0};
		coord curr = {0,0,0};

		for(size_t uj=0;uj<ions->data.size(); uj++)
		{
		    coord curr = {ions->data[uj].getPos()[0], ions->data[uj].getPos()[1], ions->data[uj].getPos()[2]};
		
		    if (uj < 10)
			{
		    std::cout << "coord curr = " << ions->data[uj].getPos()[0] << ions->data[uj].getPos()[1] << ions->data[uj].getPos()[2] << std::endl;
			}
		    voxel_index = GetVoxelIndex(&curr, voxel_size);

		    // normalized voxel indices based on 00, 01, 02 etc. // very important otherwise there will be spacings
		    openvdb::Coord ijk(voxel_index.x, voxel_index.y, voxel_index.z);

		    accessor.setValue(ijk, 1.0 + accessor.getValue(ijk));

			counter += 1;

			unsigned int idIon;
			idIon = r->getIonID(ions->data[uj].getMassToCharge());
			
			// ionIDs i think are aluminum 0 and copper 1
			// is it the order in the range file?
			if (idIon == 1)
			{
			    subaccessor1.setValue(ijk, 1.0 + subaccessor1.getValue(ijk));
				counter_cu += 1;
			}
			    else
			{
			    subaccessor1.setValue(ijk, 0.0 + subaccessor1.getValue(ijk));
			}
			}

		}

	std::cout << "ions number" << counter << std::endl;
	std::cout << "ions copper number" << counter_cu << std::endl;

	// correct all values greater than background minus the initial background

	for (openvdb::FloatGrid::ValueAllIter iter = grid->beginValueAll(); iter; ++iter)
	{
	    if (iter.getValue() > background)
	{
	    iter.setValue(iter.getValue() - background);
	}
	}

	for (openvdb::FloatGrid::ValueAllIter iter = subgrid1->beginValueAll(); iter; ++iter)
	{
	    if (iter.getValue() > background)
	{
	    iter.setValue(iter.getValue() - background);
	}
	}


	float minVal = 0.0;
	float maxVal = 0.0;
	grid->evalMinMax(minVal,maxVal);
	std::cout << " eval min max grid" << " = " << minVal << " , " << maxVal << std::endl;
	std::cout << " active voxel count grid" << " = " << grid->activeVoxelCount() << std::endl;

	subgrid1->evalMinMax(minVal,maxVal);
	std::cout << " eval min max subgrid" << " = " << minVal << " , " << maxVal << std::endl;
	std::cout << " active voxel count subgrid" << " = " << subgrid1->activeVoxelCount() << std::endl;

	// composite.h operations modify the first grid and leave the second grid emtpy
	// compute a = a / b
	openvdb::tools::compDiv(*subgrid1, *grid);

	// set all inactive / background voxels back to zero before isosurface extraction
	// case 1 a value was initially set to 1.0 as background


	for (openvdb::FloatGrid::ValueAllIter iter = subgrid1->beginValueAll(); iter; ++iter)
	{       // that means a voxel value has not changed
	    if (iter.getValue() == (background))
	{
	    iter.setValue(0.0);
	}
	}

	subgrid1->evalMinMax(minVal,maxVal);
	std::cout << " eval min max subgrid div" << " = " << minVal << " , " << maxVal << std::endl;
	std::cout << " active voxel count subgrid div" << " = " << subgrid1->activeVoxelCount() << std::endl;

////////////////////////

//subgrid1->setTransform(openvdb::math::Transform::createLinearTransform(voxel_size));

// volume to mesh

    double isoval = 0.07;
    double adapt = 0.5;

  //std::vector<openvdb::Vec3s> points;
  //std::vector<openvdb::Vec3I> triangles;
  //std::vector<openvdb::Vec4I> quads;
    // change the grid here to be extracted
//openvdb::tools::volumeToMesh<openvdb::FloatGrid>(*subgrid1, points, triangles, quads, isovalue, adaptivity);

//std::cout << "points size" << " = " << points.size() << std::endl;


// manage the filter output

	OpenVDBGridStreamData *gs = new OpenVDBGridStreamData();
	gs->parent=this;
	// just like the swap function of the voxelization does pass the grids here to gs->grids
	gs->grid = subgrid1->deepCopy();
	std::cout << " active voxel count gs grid" << " = " << gs->grid->activeVoxelCount() << std::endl;
	
	gs->isovalue=isoval;
	gs->adaptivity=adapt;
	
	gs->r=1.0;
	gs->g=0;
	gs->b=0;
	gs->a=0.3;
	
	gs->cached=1;
	cacheOK=true;
	filterOutputs.push_back(gs);


	//Store the voxels on the output
	getOut.push_back(gs);


	return 0;
}


size_t LukasAnalysisFilter::numBytesForCache(size_t nObjects) const
{
	return 0;
}


void LukasAnalysisFilter::getProperties(FilterPropGroup &propertyList) const
{

}

bool LukasAnalysisFilter::setProperty(  unsigned int key,
					const std::string &value, bool &needUpdate)
{




	return true;
}

std::string  LukasAnalysisFilter::getSpecificErrString(unsigned int code) const
{

}

void LukasAnalysisFilter::setPropFromBinding(const SelectionBinding &b)
{
	ASSERT(false);
}


bool LukasAnalysisFilter::writeState(std::ostream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) <<  "<" << trueName() << ">" << endl;

			f << tabs(depth) << "</" <<trueName()<< ">" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool LukasAnalysisFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
	using std::string;
	string tmpStr;

	xmlChar *xmlString;
	//Retrieve user string
	if(XMLHelpFwdToElem(nodePtr,"userstring"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	userString=(char *)xmlString;
	xmlFree(xmlString);


	return true;
}

unsigned int LukasAnalysisFilter::getRefreshBlockMask() const
{
	return STREAMTYPE_MASK_ALL;
}

unsigned int LukasAnalysisFilter::getRefreshEmitMask() const
{
	return  STREAM_TYPE_OPENVDBGRID | STREAM_TYPE_DRAW;
}

unsigned int LukasAnalysisFilter::getRefreshUseMask() const
{
	return  STREAM_TYPE_RANGE;
}


