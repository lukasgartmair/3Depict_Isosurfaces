/*
 *	LukasAnalysis.cpp - Compute 3D binning (voxelisation) of point clouds
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
#include "common/colourmap.h"
#include "filterCommon.h"
#include "../plot.h"
#include "openvdb_includes.h"
#include "contribution_transfer_function_TestSuite/CTF_functions.h"
#include <math.h> // pow

#include <map>

enum
{
	KEY_FIXEDWIDTH,
	KEY_NBINSX,
	KEY_NBINSY,
	KEY_NBINSZ,
	KEY_WIDTHBINSX,
	KEY_WIDTHBINSY,
	KEY_WIDTHBINSZ,

	// vdb
	KEY_VOXELSIZE,
	
	KEY_COUNT_TYPE,
	KEY_NORMALISE_TYPE,
	KEY_SPOTSIZE,
	KEY_TRANSPARENCY,
	KEY_COLOUR,
	KEY_ISOLEVEL,
	KEY_VOXEL_REPRESENTATION_MODE,
	
	KEY_VOXEL_SLICE_COLOURAUTO,
	KEY_MAPEND,
	KEY_MAPSTART,
	KEY_SHOW_COLOURBAR,
	KEY_VOXEL_COLOURMODE,
	KEY_VOXEL_SLICE_AXIS,
	KEY_VOXEL_SLICE_OFFSET,
	KEY_VOXEL_SLICE_INTERP,
	
	KEY_FILTER_MODE,
	KEY_FILTER_RATIO,
	KEY_FILTER_STDEV,
	KEY_ENABLE_NUMERATOR,
	KEY_ENABLE_DENOMINATOR
};

//!Normalisation method
enum
{
	LukasAnalysis_NORMALISETYPE_NONE,// straight count
	LukasAnalysis_NORMALISETYPE_VOLUME,// density
	LukasAnalysis_NORMALISETYPE_ALLATOMSINVOXEL, // concentration
	LukasAnalysis_NORMALISETYPE_COUNT2INVOXEL,// ratio count1/count2
	LukasAnalysis_NORMALISETYPE_MAX // keep this at the end so it's a bookend for the last value
};

//!Filtering mode
enum
{
	LukasAnalysis_FILTERTYPE_NONE,
	LukasAnalysis_FILTERTYPE_GAUSS,
	LukasAnalysis_FILTERTYPE_LAPLACE,
	LukasAnalysis_FILTERTYPE_MAX // keep this at the end so it's a bookend for the last value
};


//Boundary behaviour for filtering 
enum
{
	LukasAnalysis_FILTERBOUNDMODE_ZERO,
	LukasAnalysis_FILTERBOUNDMODE_BOUNCE,
	LukasAnalysis_FILTERBOUNDMODE_MAX// keep this at the end so it's a bookend for the last value
};


//Error codes and corresponding strings
//--
enum
{
	LukasAnalysis_ABORT_ERR=1,
	LukasAnalysis_MEMORY_ERR,
	LukasAnalysis_CONVOLVE_ERR,
	LukasAnalysis_BOUNDS_INVALID_ERR,
	LukasAnalysis_ERR_ENUM_END
};
//--


//!Can we keep the cached contents, when transitioning from
// one representation to the other- this is only the case
// when _KEEPCACHE [] is true for both representations
const bool VOXEL_REPRESENT_KEEPCACHE[] = {
	true,
	false,
	true
};

const char *LukasAnalysis_NORMALISE_TYPE_STRING[] = {
		NTRANS("None (Raw count)"),
		NTRANS("Volume (Density)"),
		NTRANS("All Ions (conc)"),
		NTRANS("Ratio (Num/Denom)"),
	};

const char *LukasAnalysis_REPRESENTATION_TYPE_STRING[] = {
		NTRANS("Point Cloud"),
		NTRANS("Proxigram"),
		NTRANS("Axial slice")
	};

const char *LukasAnalysis_FILTER_TYPE_STRING[]={
	NTRANS("None"),
	NTRANS("Gaussian (blur)"),
	NTRANS("Lapl. of Gauss. (edges)"),
	};

const char *LukasAnalysis_SLICE_INTERP_STRING[]={
	NTRANS("None"),
	NTRANS("Linear")
	};

//This is not a member of voxels.h, as the voxels do not have any concept of the IonHit
int LukasAnalysis_countPoints(Voxels<float> &v, const std::vector<IonHit> &points, 
				bool noWrap)
{

	size_t x,y,z;
	size_t binCount[3];
	v.getSize(binCount[0],binCount[1],binCount[2]);

	unsigned int downSample=MAX_CALLBACK;
	for (size_t ui=0; ui<points.size(); ui++)
	{
		if(!downSample--)
		{
			if(*Filter::wantAbort)
				return 1;
			downSample=MAX_CALLBACK;
		}
		v.getIndexWithUpper(x,y,z,points[ui].getPos());
		//Ensure it lies within the dataset
		if (x < binCount[0] && y < binCount[1] && z< binCount[2])
		{
			{
				float value;
				value=v.getData(x,y,z)+1.0f;

				ASSERT(value >= 0.0f);
				//Prevent wrap-around errors
				if (noWrap) {
					if (value > v.getData(x,y,z))
						v.setData(x,y,z,value);
				} else {
					v.setData(x,y,z,value);
				}
			}
		}
	}
	return 0;
}

// == Voxels filter ==
LukasAnalysisFilter::LukasAnalysisFilter() 
: fixedWidth(false), normaliseType(LukasAnalysis_NORMALISETYPE_NONE)
{
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(LukasAnalysis_NORMALISE_TYPE_STRING) ==  LukasAnalysis_NORMALISETYPE_MAX);
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(LukasAnalysis_FILTER_TYPE_STRING) == LukasAnalysis_FILTERTYPE_MAX );

	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(LukasAnalysis_REPRESENTATION_TYPE_STRING) == VOXEL_REPRESENT_END);
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(VOXEL_REPRESENT_KEEPCACHE) == VOXEL_REPRESENT_END);

	splatSize=1.0f;
	rgba=ColourRGBAf(0.5,0.5,0.5,0.9f);
	isoLevel=0.5;
	
	// vdb
	voxelsize = 2.0; 

	filterRatio=3.0;
	filterMode=LukasAnalysis_FILTERTYPE_NONE;
	gaussDev=0.5;	
	
	representation=VOXEL_REPRESENT_ISOSURF;

	colourMap=0;
	autoColourMap=true;
	colourMapBounds[0]=0;
	colourMapBounds[1]=1;

	// vdb cache 
	vdbCache=openvdb::FloatGrid::create(0.0);
	vdbCache->setTransform(openvdb::math::Transform::createLinearTransform(voxelsize));


	//Fictitious bounds.
	bc.setBounds(Point3D(0,0,0),Point3D(1,1,1));

	for (unsigned int i = 0; i < INDEX_LENGTH; i++) 
		nBins[i] = 50;

	calculateWidthsFromNumBins(binWidth,nBins);

	numeratorAll = true;
	denominatorAll = true;

	sliceInterpolate=VOX_INTERP_NONE;
	sliceAxis=0;
	sliceOffset=0.5;
	showColourBar=false;


	cacheOK=false;
	cache=true; //By default, we should cache, but decision is made higher up


	rsdIncoming=0;
}


Filter *LukasAnalysisFilter::cloneUncached() const
{
	LukasAnalysisFilter *p=new LukasAnalysisFilter();
	p->splatSize=splatSize;
	p->rgba=rgba;

	p->isoLevel=isoLevel;
	p->voxelsize=voxelsize;
	
	p->filterMode=filterMode;
	p->filterRatio=filterRatio;
	p->gaussDev=gaussDev;

	p->representation=representation;

	p->normaliseType=normaliseType;
	p->numeratorAll=numeratorAll;
	p->denominatorAll=denominatorAll;

	p->bc=bc;

	for(size_t ui=0;ui<INDEX_LENGTH;ui++)
	{
		p->nBins[ui] = nBins[ui];
		p->binWidth[ui] = binWidth[ui];
	}

	p->enabledIons[0].resize(enabledIons[0].size());
	std::copy(enabledIons[0].begin(),enabledIons[0].end(),p->enabledIons[0].begin());
	
	p->enabledIons[1].resize(enabledIons[1].size());
	std::copy(enabledIons[1].begin(),enabledIons[1].end(),p->enabledIons[1].begin());

	if(rsdIncoming)
	{
		p->rsdIncoming=new RangeStreamData();
		*(p->rsdIncoming) = *rsdIncoming;
	}
	else
		p->rsdIncoming=0;



	p->colourMap = colourMap;

	p->nColours = nColours;
	p->showColourBar = showColourBar;
	p->autoColourMap = autoColourMap;
	p->colourMapBounds[0] = colourMapBounds[0];
	p->colourMapBounds[1] = colourMapBounds[1];

	p->sliceInterpolate = sliceInterpolate;
	p->sliceAxis = sliceAxis;
	p->sliceOffset = sliceOffset;

	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

void LukasAnalysisFilter::clearCache() 
{
	voxelCache.clear();
	Filter::clearCache();
}

size_t LukasAnalysisFilter::numBytesForCache(size_t nObjects) const
{
	//if we are using fixed width, we know the answer.
	//otherwise we dont until we are presented with the boundcube.
	//TODO: Modify the function description to pass in the boundcube
	if(!fixedWidth)
		return 	nBins[0]*nBins[1]*nBins[2]*sizeof(float);
	else
		return 0;
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

		enabledIons[0].clear(); //clear numerator options
		enabledIons[1].clear(); //clear denominator options

		//Prevent normalisation type being set incorrectly
		// if we have no incoming range data
		if(normaliseType == LukasAnalysis_NORMALISETYPE_ALLATOMSINVOXEL || normaliseType == LukasAnalysis_NORMALISETYPE_COUNT2INVOXEL)
			normaliseType= LukasAnalysis_NORMALISETYPE_NONE;
	}
	else
	{


		//If we didn't have an incoming rsd, then make one up!
		if(!rsdIncoming)
		{
			rsdIncoming = new RangeStreamData;
			*rsdIncoming=*c;

			//set the numerator to all disabled
			enabledIons[0].resize(rsdIncoming->rangeFile->getNumIons(),0);
			//set the denominator to have all enabled
			enabledIons[1].resize(rsdIncoming->rangeFile->getNumIons(),1);
		}
		else
		{

			//OK, so we have a range incoming already (from last time)
			//-- the question is, is it the same
			//one we had before 
			//Do a pointer comparison (its a hack, yes, but it should work)
			if(rsdIncoming->rangeFile != c->rangeFile)
			{
				//hmm, it is different. well, trash the old incoming rng
				delete rsdIncoming;

				rsdIncoming = new RangeStreamData;
				*rsdIncoming=*c;

				//set the numerator to all disabled
				enabledIons[0].resize(rsdIncoming->rangeFile->getNumIons(),0);
				//set the denominator to have all enabled
				enabledIons[1].resize(rsdIncoming->rangeFile->getNumIons(),1);
			}
		}

	}
}

unsigned int LukasAnalysisFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
		  std::vector<const FilterStreamData *> &getOut, ProgressData &progress)
{
	//Disallow copying of anything in the blockmask. Copy everything else
	propagateStreams(dataIn,getOut,getRefreshBlockMask(),true);

	// Initialize the OpenVDB library.  This must be called at least
    	// once per program and may safely be called multiple times.
	openvdb::initialize();

			//use the cached copy if we have it.
			if(cacheOK)
			{
				propagateCache(getOut);
				return 0;
			}

			const float voxelsize_levelset = 0.3;

			// get the vdb grid from the stream	
			const float background_proxi = 0.0;
			openvdb::FloatGrid::Ptr grid = openvdb::FloatGrid::create(background_proxi);
			float isoLevel_proxi = 0;

			for(size_t ui=0;ui<dataIn.size();ui++)
			{
				//Check for vdb stream types. Don't use anything else here
				if(dataIn[ui]->getStreamType() == STREAM_TYPE_OPENVDBGRID)
				{
					const OpenVDBGridStreamData  *vdbgs; 
					vdbgs = (const OpenVDBGridStreamData *)dataIn[ui];

					grid = vdbgs->grid->deepCopy();
					isoLevel_proxi = vdbgs->isovalue;
				}	

			}

			int xyzs = 3;
		

			std::vector<openvdb::Vec3s> points;
			std::vector<openvdb::Vec3I> triangles;
			std::vector<openvdb::Vec4I> quads;

			// recalculate the isosurface mesh to perform calculations based on it
			try
			{
				openvdb::tools::volumeToMesh<openvdb::FloatGrid>(*grid, points, triangles, quads, isoLevel_proxi);	
			}
			catch(const std::exception &e)
			{
				ASSERT(false);
				cerr << "Exception! :" << e.what() << endl;
			}

			// checking the mesh for nonfinite coordinates like -nan and inf

			int non_finites_counter = 0;

			for (int i=0;i<points.size();i++)
			{
				for (int j=0;j<xyzs;j++)
				{
				    if (std::isfinite(points[i][j]) == false)
					{	
						non_finites_counter += 1;    
					}
				}
			}
			std::cout << "points size" << " = " << points.size() << std::endl;

			//std::cout << "number_of_non_finites" << " = " << non_finites_counter << std::endl;

			// how are the -nans introduced if there is no -nan existing in the grid?! 
			// setting only the nan to zero will of course result in large triangles crossing the scene
			// setting all 3 coordinates to zero is also shit because triangles containing the point are also big
			// how to overcome this without discarding them, which would end up in corrupt faces
			// this behaviour gets checked in the vdb test suite

			for(unsigned int ui=0;ui<points.size();ui++)
			{
				for(unsigned int uj=0;uj<xyzs;uj++)
				{
					if (std::isfinite(points[ui][uj]) == false)
					{
						for(unsigned int uk=0;uk<xyzs;uk++)
						{
							points[ui][uk] = 0.0;
						}
					}
				}
			}

		// here is no conversion to triangles needed in contrast to drawables, as meshtosigneddistancefield takes quads and triangles

			// calculate a signed distance field

			// bandwidths are in voxel units
			float bandwidth = 30;
			float in_bandwidth = 30;
			float ex_bandwidth = 30;

			// signed distance field

			//openvdb::FloatGrid::Ptr sdf = openvdb::tools::meshToUnsignedDistanceField<openvdb::FloatGrid>(openvdb::math::Transform(), points, triangles, quads, bandwidth);

			openvdb::FloatGrid::Ptr sdf = openvdb::tools::meshToSignedDistanceField<openvdb::FloatGrid>(openvdb::math::Transform(), points, triangles, quads, ex_bandwidth, in_bandwidth);

			sdf->setTransform(openvdb::math::Transform::createLinearTransform(voxelsize_levelset));

			// two very intgeresting functions in this case are
			// extractActiveVoxelSegmentMasks - 	Return a mask for each connected component of the given grid's active voxels. More...
			// extractIsosurfaceMask - 	Return a mask of the voxels that intersect the implicit surface with the given isovalue. More...	

			// now get a copy of that grid, set all its active values from the narrow band to zero and fill only them with ions of interest
			openvdb::FloatGrid::Ptr numerator_grid_proxi = sdf->deepCopy();
			openvdb::FloatGrid::Ptr denominator_grid_proxi = sdf->deepCopy();

			// initialize another grid with signed distance fields active voxels give all actives a certain value, which can be aseked for in order to retrieve the voxel state

			openvdb::FloatGrid::Ptr voxelstate_grid = sdf->deepCopy();

			// set the identical transforms for the ion information grid
			numerator_grid_proxi->setTransform(openvdb::math::Transform::createLinearTransform(voxelsize_levelset));
			denominator_grid_proxi->setTransform(openvdb::math::Transform::createLinearTransform(voxelsize_levelset));
			voxelstate_grid->setTransform(openvdb::math::Transform::createLinearTransform(voxelsize_levelset));
			// only iterate the active voxels
			// so both the active and inactive voxels should have the value zero
			// but nevertheless different activation states - is that possible?
			// another possibility is to set the initial active value to one or something 
			// unequal to zero and just overwrite it the first time of writing 
			// -> yes the result is in the test suite

			// i do have to store the coordinates of all active voxels once here
			// as i cannot find a method to evaluate whether a single voxel is active or inactive 

			std::vector<std::vector<float> > active_voxel_indices(sdf->activeVoxelCount(), std::vector<float>(xyzs));
			openvdb::Coord hkl;

			// set all active voxels in the voxelstate grid to n
			openvdb::FloatGrid::Accessor voxelstate_accessor = voxelstate_grid->getAccessor();
			int active_voxel_state_value = 1.0;

			int voxel_counter = 0;
			for (openvdb::FloatGrid::ValueOnIter iter = numerator_grid_proxi->beginValueOn(); iter; ++iter)
			{   
					iter.setValue(0.0);

					hkl = iter.getCoord();
					active_voxel_indices[voxel_counter][0] = hkl.x();
					active_voxel_indices[voxel_counter][1] = hkl.y();
					active_voxel_indices[voxel_counter][2] = hkl.z();

					// set the reference active / passive grid's values


					voxelstate_accessor.setValue(hkl, active_voxel_state_value);

					voxel_counter += 1;
			}
			for (openvdb::FloatGrid::ValueOnIter iter = denominator_grid_proxi->beginValueOn(); iter; ++iter)
			{   
					iter.setValue(0.0);
			}


			// now run through all ions but only once, and in case they are inside a active voxel write only to them

			openvdb::FloatGrid::Accessor numerator_accessor_proxi = numerator_grid_proxi->getAccessor();
			openvdb::FloatGrid::Accessor denominator_accessor_proxi = denominator_grid_proxi->getAccessor();

			std::cout << " data stream size" << " = " << dataIn.size() << std::endl;

			for(unsigned int ui=0;ui<dataIn.size() ;ui++)
			{
				std::cout << " data stream " << ui << " = " << dataIn[ui]->getStreamType() << std::endl;
			}

			int denom_counter = 0;
			int cu_counter = 0;


			//reinitialize the range stream

			const RangeStreamData *c_proxi=0;
			//Determine if we have an incoming range
			for (size_t i = 0; i < dataIn.size(); i++) 
			{
				if(dataIn[i]->getStreamType() == STREAM_TYPE_RANGE)
				{
					c_proxi=(const RangeStreamData *)dataIn[i];

					break;
				}
			}

			//we no longer (or never did) have any incoming ranges. Not much to do
			if(!c_proxi)
			{
				//delete the old incoming range pointer
				if(rsdIncoming)
					delete rsdIncoming;
				rsdIncoming=0;

				enabledIons[0].clear(); //clear numerator options
				enabledIons[1].clear(); //clear denominator options

				//Prevent normalisation type being set incorrectly
				// if we have no incoming range data
				if(normaliseType == LukasAnalysis_NORMALISETYPE_ALLATOMSINVOXEL || normaliseType == LukasAnalysis_NORMALISETYPE_COUNT2INVOXEL)
					normaliseType= LukasAnalysis_NORMALISETYPE_NONE;
			}
			else
			{


				//If we didn't have an incoming rsd, then make one up!
				if(!rsdIncoming)
				{
					rsdIncoming = new RangeStreamData;
					*rsdIncoming=*c_proxi;

					//set the numerator to all disabled
					enabledIons[0].resize(rsdIncoming->rangeFile->getNumIons(),0);
					//set the denominator to have all enabled
					enabledIons[1].resize(rsdIncoming->rangeFile->getNumIons(),1);
				}
				else
				{

					//OK, so we have a range incoming already (from last time)
					//-- the question is, is it the same
					//one we had before 
					//Do a pointer comparison (its a hack, yes, but it should work)
					if(rsdIncoming->rangeFile != c_proxi->rangeFile)
					{
						//hmm, it is different. well, trash the old incoming rng
						delete rsdIncoming;

						rsdIncoming = new RangeStreamData;
						*rsdIncoming=*c_proxi;

						//set the numerator to all disabled
						enabledIons[0].resize(rsdIncoming->rangeFile->getNumIons(),0);
						//set the denominator to have all enabled
						enabledIons[1].resize(rsdIncoming->rangeFile->getNumIons(),1);
					}
				}

			}
	
			for(size_t ui=0;ui<dataIn.size();ui++)
			{
				//Check for ion stream types. Don't use anything else in counting
				if(dataIn[ui]->getStreamType() != STREAM_TYPE_IONS)
					continue;

				const IonStreamData  *ions; 
				ions = (const IonStreamData *)dataIn[ui];

				//get the denominator ions
				unsigned int ionID;
				bool rsd = false;
				if (rsd)
				{
					rsd = true;					
				}
				std::cout << "rsd incoming" << " = " << rsd << std::endl;
				ionID = c_proxi->rangeFile->getIonID(ions->data[0].getMassToCharge());

				bool thisDenominatorIonEnabled;
				if(ionID!=(unsigned int)-1)
					thisDenominatorIonEnabled=enabledIons[1][ionID];
				else
					thisDenominatorIonEnabled=false;

				// get the numerator ions
				ionID = getIonstreamIonID(ions,rsdIncoming->rangeFile);

				bool thisNumeratorIonEnabled;
				if(ionID!=(unsigned int)-1)
					thisNumeratorIonEnabled=enabledIons[0][ionID];
				else
					thisNumeratorIonEnabled=false;

				for(size_t uj=0;uj<ions->data.size(); uj++)
				{
					const int xyzs = 3;
					std::vector<float> atom_position(xyzs); 
					for (int i=0;i<xyzs;i++)
					{
						atom_position[i] = ions->data[uj].getPos()[i];
					}

					// 1st step - project the current atom position to unit voxel i.e. from 0 to 1
					std::vector<float> position_in_unit_voxel;
					position_in_unit_voxel = projectAtompositionToUnitvoxel(atom_position, voxelsize_levelset);

					// 2nd step - determine each contribution to the adjecent 8 voxels outgoining from the position in the unit voxel
					std::vector<float> volumes_of_subcuboids;
					std::vector<float> contributions_to_adjacent_voxels;
					bool vertex_corner_coincidence = false;

					vertex_corner_coincidence = checkVertexCornerCoincidence(position_in_unit_voxel);

					// in case of coincidence of atom and voxel the contribution becomes 100 percent
					if (vertex_corner_coincidence == false)
					{
						volumes_of_subcuboids = calcSubvolumes(position_in_unit_voxel);
						contributions_to_adjacent_voxels = calcVoxelContributions(volumes_of_subcuboids);
					}
					else
					{
						contributions_to_adjacent_voxels = handleVertexCornerCoincidence(position_in_unit_voxel);
					}

					// 3rd step - determine the adjacent voxel indices in the actual grid
					std::vector<std::vector<float> > adjacent_voxel_vertices;
					adjacent_voxel_vertices = determineAdjacentVoxelVertices(atom_position, voxelsize_levelset);

					// 4th step - assign each of the 8 adjacent voxels the corresponding contribution that results from the atom position in the unit voxel
					const int number_of_adjacent_voxels = 8;
					std::vector<float> current_voxel_index;
					for (int i=0;i<number_of_adjacent_voxels;i++)
					{
						current_voxel_index = adjacent_voxel_vertices[i];
						// normalized voxel indices based on 00, 01, 02 etc. // very important otherwise there will be spacings
						openvdb::Coord ijk(current_voxel_index[0], current_voxel_index[1], current_voxel_index[2]);


						if(voxelstate_accessor.getValue(ijk) == active_voxel_state_value)
						{

							// write to denominator grid

							denominator_accessor_proxi.setValue(ijk, contributions_to_adjacent_voxels[i] + denominator_accessor_proxi.getValue(ijk));
							denom_counter += 1;
							// write to numerator grid
							//if(thisNumeratorIonEnabled)
							if(ionID == 1)								
							{	
								numerator_accessor_proxi.setValue(ijk, contributions_to_adjacent_voxels[i] + numerator_accessor_proxi.getValue(ijk));
							}
							else
							{
								numerator_accessor_proxi.setValue(ijk, 0.0 + numerator_accessor_proxi.getValue(ijk));
								cu_counter += 1;
							}
						}
					}
				}
			}

			float minVal = 0.0;
			float maxVal = 0.0;

			std::cout << "denom counter" << " = " << denom_counter << std::endl;
			std::cout << "cu counter " << " = " << cu_counter << std::endl;

			// now there is a grid with ion information and one grid with distance information
			// now take discrete distances and calculate the mean concentration in that shell
			// this is done by checking all voxels in the narrow band whether they are inside or outside the 
			// proximity range and then add up the entries both from the numerator grid and the denominator grid
			// the division will provide the concentration in that promximity shell

			sdf->evalMinMax(minVal,maxVal);

			int number_of_proximity_ranges = 6;
			std::vector<float> proximity_ranges(number_of_proximity_ranges);

			std::vector<float> concentrations(number_of_proximity_ranges);

			// the algorithm has to look like this:
			// only iterate once over the sdf grid and in this one iteration check the several conditions
			// another time saver would be not to iterate again over the whole volume, but only over
			// the next bigger shell minus the last biggest shells

			proximity_ranges[0] = -1;
			proximity_ranges[1] = -0.5;
			proximity_ranges[2] = 1;
			proximity_ranges[3] = 1.5;
			proximity_ranges[4] = 8;
			proximity_ranges[5] = 25;	

			std::cout << " eval min max sdf" << " = " << minVal << " , " << maxVal << std::endl;
			std::cout << " active voxel count sdf " << " = " << sdf->activeVoxelCount() << std::endl;

			numerator_grid_proxi->evalMinMax(minVal,maxVal);
			std::cout << " eval min max numerator_grid" << " = " << minVal << " , " << maxVal << std::endl;
			std::cout << " active voxel count numerator_grid " << " = " << numerator_grid_proxi->activeVoxelCount() << std::endl;

			denominator_grid_proxi->evalMinMax(minVal,maxVal);
			std::cout << " eval min max denominator_grid" << " = " << minVal << " , " << maxVal << std::endl;
			std::cout << " active voxel count denominator_grid " << " = " << denominator_grid_proxi->activeVoxelCount() << std::endl;


			openvdb::math::CoordBBox bounding_box1 = sdf->evalActiveVoxelBoundingBox();
			openvdb::math::CoordBBox bounding_box2 = denominator_grid_proxi->evalActiveVoxelBoundingBox();

			std::cout << " bounding box sdf " << " = " << bounding_box1 << std::endl;
			std::cout << " bounding box denominator_grid _proxi " << " = " << bounding_box2 << std::endl;


			for (int i=0;i<number_of_proximity_ranges;i++)
			{	

				float numerator_total = 0;
				float denominator_total = 0;

				for (openvdb::FloatGrid::ValueOnIter iter = sdf->beginValueOn(); iter; ++iter)
				{   
					// a proximity of zero should theoretically depict the isosurface
					if ((proximity_ranges[i] < iter.getValue()) && (iter.getValue() <= 0))
					{
						openvdb::Coord abc = iter.getCoord();
		
			    			numerator_total += numerator_accessor_proxi.getValue(abc);
						denominator_total += denominator_accessor_proxi.getValue(abc);
					}
					if ((0 < iter.getValue()) && (iter.getValue() < proximity_ranges[i]))
					{
						openvdb::Coord abc = iter.getCoord();
		
			    			numerator_total += numerator_accessor_proxi.getValue(abc);
						denominator_total += denominator_accessor_proxi.getValue(abc);
					}

				}
				concentrations[i] = numerator_total / denominator_total;
			}

/*

// probably more clever only iterating once over the whole grid and do the binning
// but in the end it should not matter whether to do x times n mio checks 
// or vice versa n mio times x checks  

			std::vector<float> numerators(number_of_proximity_ranges);
			std::vector<float> denominators(number_of_proximity_ranges);

			for (openvdb::FloatGrid::ValueOnIter iter = sdf->beginValueOn(); iter; ++iter)
			{

				for (int i=0;i<number_of_proximity_ranges;i++)
				{

					if ((proximity_ranges[i] < iter.getValue()) && (iter.getValue() <= 0))
					{
						openvdb::Coord abc = iter.getCoord();
		
			    			numerators[i] += numerator_accessor_proxi.getValue(abc);
						denominators[i] += denominator_accessor_proxi.getValue(abc);
					}
					if ((0 < iter.getValue()) && (iter.getValue() < proximity_ranges[i]))
					{
						openvdb::Coord abc = iter.getCoord();
		
			    			numerators[i] += numerator_accessor_proxi.getValue(abc);
						denominators[i] += denominator_accessor_proxi.getValue(abc);
					}

				}
				

			}

			for (int i=0;i<number_of_proximity_ranges;i++)
			{
				concentrations[i] = numerators[i] / denominators[i];
			}
*/

			for (int i=0;i<number_of_proximity_ranges;i++)
			{
				std::cout << " concentrations " << i << " = " << concentrations[i] << std::endl;
			}

			// manage the filter output

			PlotStreamData *d;
			d=new PlotStreamData;

			d->xyData.resize(number_of_proximity_ranges);

			d->plotStyle = 0;
			d->plotMode=PLOT_MODE_2D;

			d->index=0;
			d->parent=this;



			for(unsigned int ui=0;ui<number_of_proximity_ranges;ui++)
			{
				d->xyData[ui].first = proximity_ranges[ui];
				d->xyData[ui].second = concentrations[ui];
			}

			d->autoSetHardBounds();

			getOut.push_back(d);

	

	//Copy the inputs into the outputs, provided they are not voxels
	return 0;
}



void LukasAnalysisFilter::setPropFromBinding(const SelectionBinding &b)
{
	switch(b.getID())
	{
		case BINDING_PLANE_ORIGIN:
		{
			ASSERT(representation == VOXEL_REPRESENT_AXIAL_SLICE);
			ASSERT(lastBounds.isValid());
		
			//Convert the world coordinate value into a
			// fractional value of voxel bounds
			Point3D p;
			float f;
			b.getValue(p);
			f=p[sliceAxis];

			float minB,maxB;
			minB = lastBounds.getBound(sliceAxis,0);
			maxB = lastBounds.getBound(sliceAxis,1);
			sliceOffset= (f -minB)/(maxB-minB);
			
			sliceOffset=std::min(sliceOffset,1.0f);
			sliceOffset=std::max(sliceOffset,0.0f);
			ASSERT(sliceOffset<=1 && sliceOffset>=0);
			break;
		}
		default:
			ASSERT(false);
	}

}

std::string LukasAnalysisFilter::getNormaliseTypeString(int type){
	ASSERT(type < LukasAnalysis_NORMALISETYPE_MAX);
	return TRANS(LukasAnalysis_NORMALISE_TYPE_STRING[type]);
}

std::string LukasAnalysisFilter::getRepresentTypeString(int type) {
	ASSERT(type<VOXEL_REPRESENT_END);
	return  std::string(TRANS(LukasAnalysis_REPRESENTATION_TYPE_STRING[type]));
}

std::string LukasAnalysisFilter::getFilterTypeString(int type)
{
	ASSERT(type < LukasAnalysis_FILTERTYPE_MAX);
	return std::string(TRANS(LukasAnalysis_FILTER_TYPE_STRING[type]));
}


void LukasAnalysisFilter::getProperties(FilterPropGroup &propertyList) const
{
	FilterProperty p;
	size_t curGroup=0;

	string tmpStr;
	stream_cast(tmpStr, fixedWidth);
	p.name=TRANS("Fixed width");
	p.data=tmpStr;
	p.key=KEY_FIXEDWIDTH;
	p.type=PROPERTY_TYPE_BOOL;
	p.helpText=TRANS("If true, use fixed size voxels, otherwise use fixed count");
	propertyList.addProperty(p,curGroup);

	if(fixedWidth)
	{
		stream_cast(tmpStr,binWidth[0]);
		p.name=TRANS("Bin width x");
		p.data=tmpStr;
		p.key=KEY_WIDTHBINSX;
		p.type=PROPERTY_TYPE_REAL;
		p.helpText=TRANS("Voxel size in X direction");
		propertyList.addProperty(p,curGroup);

		stream_cast(tmpStr,binWidth[1]);
		p.name=TRANS("Bin width y");
		p.data=tmpStr;
		p.type=PROPERTY_TYPE_REAL;
		p.helpText=TRANS("Voxel size in Y direction");
		p.key=KEY_WIDTHBINSY;
		propertyList.addProperty(p,curGroup);


		stream_cast(tmpStr,binWidth[2]);
		p.name=TRANS("Bin width z");
		p.data=tmpStr;
		p.type=PROPERTY_TYPE_REAL;
		p.helpText=TRANS("Voxel size in Z direction");
		p.key=KEY_WIDTHBINSZ;
		propertyList.addProperty(p,curGroup);
	}
	else
	{
		stream_cast(tmpStr,nBins[0]);
		p.name=TRANS("Num bins x");
		p.data=tmpStr;
		p.key=KEY_NBINSX;
		p.type=PROPERTY_TYPE_INTEGER;
		p.helpText=TRANS("Number of voxels to use in X direction");
		propertyList.addProperty(p,curGroup);
		
		stream_cast(tmpStr,nBins[1]);
		p.key=KEY_NBINSY;
		p.name=TRANS("Num bins y");
		p.data=tmpStr;
		p.type=PROPERTY_TYPE_INTEGER;
		p.helpText=TRANS("Number of voxels to use in Y direction");
		propertyList.addProperty(p,curGroup);
		
		stream_cast(tmpStr,nBins[2]);
		p.key=KEY_NBINSZ;
		p.data=tmpStr;
		p.name=TRANS("Num bins z");
		p.type=PROPERTY_TYPE_INTEGER;
		p.helpText=TRANS("Number of voxels to use in Z direction");
		propertyList.addProperty(p,curGroup);
	}

	//Let the user know what the valid values for voxel value types are
	vector<pair<unsigned int,string> > choices;
	unsigned int defaultChoice=normaliseType;
	tmpStr=getNormaliseTypeString(LukasAnalysis_NORMALISETYPE_NONE);
	choices.push_back(make_pair((unsigned int)LukasAnalysis_NORMALISETYPE_NONE,tmpStr));
	tmpStr=getNormaliseTypeString(LukasAnalysis_NORMALISETYPE_VOLUME);
	choices.push_back(make_pair((unsigned int)LukasAnalysis_NORMALISETYPE_VOLUME,tmpStr));
	if(rsdIncoming)
	{
		//Concentration mode
		tmpStr=getNormaliseTypeString(LukasAnalysis_NORMALISETYPE_ALLATOMSINVOXEL);
		choices.push_back(make_pair((unsigned int)LukasAnalysis_NORMALISETYPE_ALLATOMSINVOXEL,tmpStr));
		//Ratio is only valid if we have a way of separation for the ions i.e. range
		tmpStr=getNormaliseTypeString(LukasAnalysis_NORMALISETYPE_COUNT2INVOXEL);
		choices.push_back(make_pair((unsigned int)LukasAnalysis_NORMALISETYPE_COUNT2INVOXEL,tmpStr));
	}
	else
	{
		//prevent the case where we used to have an incoming range stream, but now we don't.
		// selected item within choice string must still be valid
		if(normaliseType > LukasAnalysis_NORMALISETYPE_VOLUME)
			defaultChoice= LukasAnalysis_NORMALISETYPE_NONE;
		
	}

	tmpStr= choiceString(choices,defaultChoice);
	p.name=TRANS("Normalise by");
	p.data=tmpStr;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Method to use to normalise scalar value in each voxel");
	p.key=KEY_NORMALISE_TYPE;
	propertyList.addProperty(p,curGroup);
	propertyList.setGroupTitle(curGroup,TRANS("Computation"));

	curGroup++;
	
	// numerator
	if (rsdIncoming) 
	{
		
		p.name=TRANS("Numerator");
		p.data=boolStrEnc(numeratorAll);
		p.type=PROPERTY_TYPE_BOOL;
		p.helpText=TRANS("Parmeter \"a\" used in fraction (a/b) to get voxel value");
		p.key=KEY_ENABLE_NUMERATOR;
		propertyList.addProperty(p,curGroup);

		ASSERT(rsdIncoming->enabledIons.size()==enabledIons[0].size());	
		ASSERT(rsdIncoming->enabledIons.size()==enabledIons[1].size());	

		//Look at the numerator	
		for(unsigned  int ui=0; ui<rsdIncoming->enabledIons.size(); ui++)
		{
			string str;
			str=boolStrEnc(enabledIons[0][ui]);

			//Append the ion name with a checkbox
			p.name=rsdIncoming->rangeFile->getName(ui);
			p.data=str;
			p.type=PROPERTY_TYPE_BOOL;
			p.helpText=TRANS("Enable this ion for numerator");
			p.key=muxKey(KEY_ENABLE_NUMERATOR,ui);
			propertyList.addProperty(p,curGroup);
		}

		propertyList.setGroupTitle(curGroup,TRANS("Numerator"));
		curGroup++;
	}
	
	
	if (normaliseType == LukasAnalysis_NORMALISETYPE_COUNT2INVOXEL && rsdIncoming) 
	{
		p.name=TRANS("Denominator");
		p.data=boolStrEnc(denominatorAll );
		p.type=PROPERTY_TYPE_BOOL;
		p.helpText=TRANS("Parameter \"b\" used in fraction (a/b) to get voxel value");
		p.key=KEY_ENABLE_DENOMINATOR;
		propertyList.addProperty(p,curGroup);

		for(unsigned  int ui=0; ui<rsdIncoming->enabledIons.size(); ui++)
		{			
			string str;
			str=boolStrEnc(enabledIons[1][ui]);

			//Append the ion name with a checkbox
			p.key=muxKey(KEY_ENABLE_DENOMINATOR,ui);
			p.data=str;
			p.name=rsdIncoming->rangeFile->getName(ui);
			p.type=PROPERTY_TYPE_BOOL;
			p.helpText=TRANS("Enable this ion for denominator contribution");

			propertyList.addProperty(p,curGroup);
		}
		propertyList.setGroupTitle(curGroup,TRANS("Denominator"));
		curGroup++;
	}

	//Start a new set for filtering
	//----
	//TODO: Other filtering? threshold/median? laplacian? etc
	
	choices.clear();
	//Post-filtering method
	for(unsigned int ui=0;ui<LukasAnalysis_FILTERTYPE_MAX; ui++)
	{
		tmpStr=getFilterTypeString(ui);
		choices.push_back(make_pair(ui,tmpStr));
	}
	tmpStr= choiceString(choices,filterMode);
	choices.clear();

	p.name=TRANS("Filtering");
	p.data=tmpStr;
	p.key=KEY_FILTER_MODE;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Smoothing method to use on voxels");

	propertyList.addProperty(p,curGroup);
	propertyList.setGroupTitle(curGroup,TRANS("Processing"));
	if(filterMode != LukasAnalysis_FILTERTYPE_NONE)
	{

		//Filter size
		stream_cast(tmpStr,gaussDev);
		p.name=TRANS("Standard Dev");
		p.data=tmpStr;
		p.key=KEY_FILTER_STDEV;
		p.type=PROPERTY_TYPE_REAL;
		p.helpText=TRANS("Filtering Scale");
		propertyList.addProperty(p,curGroup);

		
		//Filter size
		stream_cast(tmpStr,filterRatio);
		p.name=TRANS("Kernel Size");
		p.data=tmpStr;
		p.key=KEY_FILTER_RATIO;
		p.type=PROPERTY_TYPE_REAL;
		p.helpText=TRANS("Filter radius, in multiples of std. dev. Larger -> slower, more accurate");
		propertyList.addProperty(p,curGroup);

	}
	propertyList.setGroupTitle(curGroup,TRANS("Filtering"));
	curGroup++;
	//----

	//start a new group for the visual representation
	//----------------------------
	choices.clear();
	tmpStr=getRepresentTypeString(VOXEL_REPRESENT_POINTCLOUD);
	choices.push_back(make_pair((unsigned int)VOXEL_REPRESENT_POINTCLOUD,tmpStr));
	tmpStr=getRepresentTypeString(VOXEL_REPRESENT_ISOSURF);
	choices.push_back(make_pair((unsigned int)VOXEL_REPRESENT_ISOSURF,tmpStr));
	tmpStr=getRepresentTypeString(VOXEL_REPRESENT_AXIAL_SLICE);
	choices.push_back(make_pair((unsigned int)VOXEL_REPRESENT_AXIAL_SLICE,tmpStr));
	
	tmpStr= choiceString(choices,representation);

	p.name=TRANS("Representation");
	p.data=tmpStr;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("3D display method");
	p.key=KEY_VOXEL_REPRESENTATION_MODE;
	propertyList.addProperty(p,curGroup);

	switch(representation)
	{
		case VOXEL_REPRESENT_POINTCLOUD:
		{
			propertyList.setGroupTitle(curGroup,TRANS("Appearance"));

			stream_cast(tmpStr,splatSize);
			p.name=TRANS("Spot size");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("Size of the spots to use for display");
			p.key=KEY_SPOTSIZE;
			propertyList.addProperty(p,curGroup);

			stream_cast(tmpStr,1.0-rgba.a());
			p.name=TRANS("Transparency");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("How \"see through\" each point is (0 - opaque, 1 - invisible)");
			p.key=KEY_TRANSPARENCY;
			propertyList.addProperty(p,curGroup);
			
			break;
		}
		case VOXEL_REPRESENT_ISOSURF:
		{
			if(!rsdIncoming)
				break;

			// group computation
			stream_cast(tmpStr,voxelsize);
			p.name=TRANS("Voxelsize");
			p.data=tmpStr;
			p.key=KEY_VOXELSIZE;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("Voxel size in x,y,z direction");
			propertyList.addProperty(p,curGroup);

			propertyList.setGroupTitle(curGroup,TRANS("Computation"));
			curGroup++;
	
	
			//-- Isosurface parameters --

			stream_cast(tmpStr,isoLevel);
			p.name=TRANS("Isovalue [0,1]");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("Scalar value to show as isosurface");
			p.key=KEY_ISOLEVEL;
			propertyList.addProperty(p,curGroup);

			//-- 
			propertyList.setGroupTitle(curGroup,TRANS("Isosurface"));	
			curGroup++;

			//-- Isosurface appearance --
			p.name=TRANS("Colour");
			p.data=rgba.toColourRGBA().rgbString();
			p.type=PROPERTY_TYPE_COLOUR;
			p.helpText=TRANS("Colour of isosurface");
			p.key=KEY_COLOUR;
			propertyList.addProperty(p,curGroup);
			
			break;
		}
		case VOXEL_REPRESENT_AXIAL_SLICE:
		{
			//-- Slice parameters --
			propertyList.setGroupTitle(curGroup,TRANS("Slice param."));

			vector<pair<unsigned int, string> > choices;
			
			choices.push_back(make_pair(0,"x"));	
			choices.push_back(make_pair(1,"y"));	
			choices.push_back(make_pair(2,"z"));	
			p.name=TRANS("Slice Axis");
			p.data=choiceString(choices,sliceAxis);
			p.type=PROPERTY_TYPE_CHOICE;
			p.helpText=TRANS("Normal for the planar slice");
			p.key=KEY_VOXEL_SLICE_AXIS;
			propertyList.addProperty(p,curGroup);
			choices.clear();
			
			stream_cast(tmpStr,sliceOffset);
			p.name=TRANS("Slice Coord");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("Fractional coordinate that slice plane passes through");
			p.key=KEY_VOXEL_SLICE_OFFSET;
			propertyList.addProperty(p,curGroup);
		
			
			p.name=TRANS("Interp. Mode");
			for(unsigned int ui=0;ui<VOX_INTERP_ENUM_END;ui++)
			{
				choices.push_back(make_pair(ui,
					TRANS(LukasAnalysis_SLICE_INTERP_STRING[ui])));
			}
			p.data=choiceString(choices,sliceInterpolate);
			p.type=PROPERTY_TYPE_CHOICE;
			p.helpText=TRANS("Interpolation mode for direction normal to slice");
			p.key=KEY_VOXEL_SLICE_INTERP;
			propertyList.addProperty(p,curGroup);
			choices.clear();
			// ---	
			propertyList.setGroupTitle(curGroup,TRANS("Surface"));	
			curGroup++;

			

			//-- Slice visualisation parameters --
			for(unsigned int ui=0;ui<NUM_COLOURMAPS; ui++)
				choices.push_back(make_pair(ui,getColourMapName(ui)));

			tmpStr=choiceString(choices,colourMap);

			p.name=TRANS("Colour mode");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_CHOICE;
			p.helpText=TRANS("Colour scheme used to assign points colours by value");
			p.key=KEY_VOXEL_COLOURMODE;
			propertyList.addProperty(p,curGroup);
			
			stream_cast(tmpStr,1.0-rgba.a());
			p.name=TRANS("Transparency");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("How \"see through\" each facet is (0 - opaque, 1 - invisible)");
			p.key=KEY_TRANSPARENCY;
			propertyList.addProperty(p,curGroup);

			tmpStr=boolStrEnc(showColourBar);
			p.name=TRANS("Show Bar");
			p.key=KEY_SHOW_COLOURBAR;
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_BOOL;
			propertyList.addProperty(p,curGroup);

			tmpStr=boolStrEnc(autoColourMap);
			p.name=TRANS("Auto Bounds");
			p.helpText=TRANS("Auto-compute min/max values in map"); 
			p.data= tmpStr;
			p.key=KEY_VOXEL_SLICE_COLOURAUTO;;
			p.type=PROPERTY_TYPE_BOOL;
			propertyList.addProperty(p,curGroup);

			if(!autoColourMap)
			{

				stream_cast(tmpStr,colourMapBounds[0]);
				p.name=TRANS("Map start");
				p.helpText=TRANS("Assign points with this value to the first colour in map"); 
				p.data= tmpStr;
				p.key=KEY_MAPSTART;;
				p.type=PROPERTY_TYPE_REAL;
				propertyList.addProperty(p,curGroup);

				stream_cast(tmpStr,colourMapBounds[1]);
				p.name=TRANS("Map end");
				p.helpText=TRANS("Assign points with this value to the last colour in map"); 
				p.data= tmpStr;
				p.key=KEY_MAPEND;
				p.type=PROPERTY_TYPE_REAL;
				propertyList.addProperty(p,curGroup);
			}
			// ---	

			break;
		}
		default:
			ASSERT(false);
			;
	}
	
	propertyList.setGroupTitle(curGroup,TRANS("Appearance"));	
	curGroup++;

	//----------------------------
}

bool LukasAnalysisFilter::setProperty(unsigned int key,
		  const std::string &value, bool &needUpdate)
{
	
	needUpdate=false;
	switch(key)
	{


		case KEY_VOXELSIZE: 
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
			needUpdate=true;
			voxelsize=f;
			//Go in and manually adjust the cached
			//entries to have the new value, rather
			//than doing a full recomputation
			if(cacheOK)
			{
				for(unsigned int ui=0;ui<filterOutputs.size();ui++)
				{	
					OpenVDBGridStreamData *vdbgs;
					vdbgs = (OpenVDBGridStreamData*)filterOutputs[ui];
					vdbgs->voxelsize = voxelsize;
				}
			}
			break;
		}

		case KEY_FIXEDWIDTH: 
		{
			if(!applyPropertyNow(fixedWidth,value,needUpdate))
				return false;
			break;
		}	
		case KEY_NBINSX:
		case KEY_NBINSY:
		case KEY_NBINSZ:
		{
			if(!applyPropertyNow(nBins[key-KEY_NBINSX],value,needUpdate))
				return false;
			calculateWidthsFromNumBins(binWidth, nBins);
			break;
		}
		case KEY_WIDTHBINSX:
		case KEY_WIDTHBINSY:
		case KEY_WIDTHBINSZ:
		{
			if(!applyPropertyNow(binWidth[key-KEY_WIDTHBINSX],value,needUpdate))
				return false;
			calculateNumBinsFromWidths(binWidth, nBins);
			break;
		}
		case KEY_NORMALISE_TYPE:
		{
			unsigned int i;
			for(i = 0; i < LukasAnalysis_NORMALISETYPE_MAX; i++)
				if (value == getNormaliseTypeString(i)) break;
			if (i == LukasAnalysis_NORMALISETYPE_MAX)
				return false;
			if(normaliseType!=i)
			{
				needUpdate=true;
				clearCache();
				vdbCache->clear();
				normaliseType=i;
			}
			break;
		}
		case KEY_SPOTSIZE:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
			if(f !=splatSize)
			{
				splatSize=f;
				needUpdate=true;

				//Go in and manually adjust the cached
				//entries to have the new value, rather
				//than doing a full recomputation
				if(cacheOK)
				{
					for(unsigned int ui=0;ui<filterOutputs.size();ui++)
					{
						VoxelStreamData *d;
						d=(VoxelStreamData*)filterOutputs[ui];
						d->splatSize=splatSize;
					}
				}

			}
			break;
		}
		case KEY_TRANSPARENCY:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f < 0.0f || f > 1.0)
				return false;
			needUpdate=true;
			//Alpha is opacity, which is 1-transparancy
			rgba.a(1.0f-f);
			//Go in and manually adjust the cached
			//entries to have the new value, rather
			//than doing a full recomputation
			if(cacheOK)
			{
				for(unsigned int ui=0;ui<filterOutputs.size();ui++)
				{	
					OpenVDBGridStreamData *vdbgs;
					vdbgs = (OpenVDBGridStreamData*)filterOutputs[ui];
					vdbgs->a = rgba.a();
				}
			}
			break;
		}
		case KEY_ISOLEVEL:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
			needUpdate=true;
			isoLevel=f;
			//Go in and manually adjust the cached
			//entries to have the new value, rather
			//than doing a full recomputation
			if(cacheOK)
			{
				for(unsigned int ui=0;ui<filterOutputs.size();ui++)
				{	
					OpenVDBGridStreamData *vdbgs;
					vdbgs = (OpenVDBGridStreamData*)filterOutputs[ui];
					vdbgs->isovalue = isoLevel;
				}
			}
			break;
		}
		case KEY_COLOUR:
		{
			ColourRGBA tmpRGBA;

			if(!tmpRGBA.parse(value))
				return false;

			if(tmpRGBA.toRGBAf() != rgba)
			{
				rgba=tmpRGBA.toRGBAf();
				needUpdate=true;
			}

			//Go in and manually adjust the cached
			//entries to have the new value, rather
			//than doing a full recomputation
			if(cacheOK)
			{
				for(unsigned int ui=0;ui<filterOutputs.size();ui++)
				{
					switch(representation)
					{
						case VOXEL_REPRESENT_AXIAL_SLICE:
						case VOXEL_REPRESENT_POINTCLOUD:
						{

							VoxelStreamData *d;
							d=(VoxelStreamData*)filterOutputs[ui];
							d->r=rgba.r();
							d->g=rgba.g();
							d->b=rgba.b();
						}
						case VOXEL_REPRESENT_ISOSURF:
						{
							OpenVDBGridStreamData *vdbgs;
							vdbgs = (OpenVDBGridStreamData*)filterOutputs[ui];
							vdbgs->r=rgba.r();
							vdbgs->g=rgba.g();
							vdbgs->b=rgba.b();
						}
					}
				}
			}
			break;
		}
		case KEY_VOXEL_REPRESENTATION_MODE:
		{
			unsigned int i;
			for (i = 0; i < VOXEL_REPRESENT_END; i++)
				if (value == getRepresentTypeString(i)) break;
			if (i == VOXEL_REPRESENT_END)
				return false;
			needUpdate=true;
			//Go in and manually adjust the cached
			//entries to have the new value, rather
			//than doing a full recomputation

			//TODO: Can we instead of caching the Stream, simply cache the voxel data?
			representation=i;
			if(cacheOK && (VOXEL_REPRESENT_KEEPCACHE[i] && VOXEL_REPRESENT_KEEPCACHE[representation]))
			{
				for(unsigned int ui=0;ui<filterOutputs.size();ui++)
				{

					switch(representation)
					{
						case VOXEL_REPRESENT_AXIAL_SLICE:
						case VOXEL_REPRESENT_POINTCLOUD:
						{
							VoxelStreamData *d;
							d=(VoxelStreamData*)filterOutputs[ui];
							d->representationType=representation;

						}

						case VOXEL_REPRESENT_ISOSURF:
						{
							OpenVDBGridStreamData *vdbgs;
							vdbgs = (OpenVDBGridStreamData*)filterOutputs[ui];
							vdbgs->representationType=representation;
						}	
					}

				}
			}
			else
			{
				clearCache();
				vdbCache->clear();
			}
			
			break;
		}
		case KEY_ENABLE_NUMERATOR:
		{
			bool b;
			if(stream_cast(b,value))
				return false;
			//Set them all to enabled or disabled as a group	
			for (size_t i = 0; i < enabledIons[0].size(); i++) 
				enabledIons[0][i] = b;
			numeratorAll = b;
			needUpdate=true;
			clearCache();
			vdbCache->clear();
			break;
		}
		case KEY_ENABLE_DENOMINATOR:
		{
			bool b;
			if(stream_cast(b,value))
				return false;
	
			//Set them all to enabled or disabled as a group	
			for (size_t i = 0; i < enabledIons[1].size(); i++) 
				enabledIons[1][i] = b;
			
			denominatorAll = b;
			needUpdate=true;			
			clearCache();
			vdbCache->clear();
			break;
		}
		case KEY_FILTER_MODE:
		{
			//Locate the current string
			unsigned int i;
			for (i = 0; i < LukasAnalysis_FILTERTYPE_MAX; i++)
			{
				if (value == getFilterTypeString(i)) 
					break;
			}
			if (i == LukasAnalysis_FILTERTYPE_MAX)
				return false;
			if(i!=filterMode)
			{
				needUpdate=true;
				filterMode=i;
				clearCache();
			}
			break;
		}
		case KEY_FILTER_RATIO:
		{
			float i;
			if(stream_cast(i,value))
				return false;
			//forbid negative sizes
			if(i <= 0)
				return false;
			if(i != filterRatio)
			{
				needUpdate=true;
				filterRatio=i;
				clearCache();
			}
			break;
		}
		case KEY_FILTER_STDEV:
		{
			float i;
			if(stream_cast(i,value))
				return false;
			//forbid negative sizes
			if(i <= 0)
				return false;
			if(i != gaussDev)
			{
				needUpdate=true;
				gaussDev=i;
				clearCache();
			}
			break;
		}
		case KEY_VOXEL_SLICE_COLOURAUTO:
		{
			bool b;
			if(!boolStrDec(value,b))
				return false;

			//if the result is different, the
			//cache should be invalidated
			if(b!=autoColourMap)
			{
				needUpdate=true;
				autoColourMap=b;
				//Clear the generic filter cache, but
				// not the voxel cache
				Filter::clearCache();
			}
			break;
		}	
		case KEY_VOXEL_SLICE_AXIS:
		{
			unsigned int i;
			string axisLabels[3]={"x","y","z"};
			for (i = 0; i < 3; i++)
				if( value == axisLabels[i]) break;
				
			if( i >= 3)
				return false;

			if(i != sliceAxis)
			{
				needUpdate=true;
				//clear the generic filter cache (i.e. cached outputs)
				//but not the voxel cache
				Filter::clearCache();
				sliceAxis=i;
			}
			break;
		}
		case KEY_VOXEL_SLICE_INTERP:
		{
			unsigned int i;
			for (i = 0; i < VOX_INTERP_ENUM_END; i++)
				if( value == TRANS(LukasAnalysis_SLICE_INTERP_STRING[i])) break;
				
			if( i >= VOX_INTERP_ENUM_END)
				return false;

			if(i != sliceInterpolate)
			{
				needUpdate=true;
				//clear the generic filter cache (i.e. cached outputs)
				//but not the voxel cache
				Filter::clearCache();
				sliceInterpolate=i;
			}
			break;
		}
		case KEY_VOXEL_SLICE_OFFSET:
		{
			float f;
			if(stream_cast(f,value))
				return false;

			if(f < 0.0f || f > 1.0f)
				return false;


			if( f != sliceOffset)
			{
				needUpdate=true;
				//clear the generic filter cache (i.e. cached outputs)
				//but not the voxel cache
				Filter::clearCache();
				sliceOffset=f;
			}

			break;
		}
		case KEY_VOXEL_COLOURMODE:
		{
			unsigned int tmpMap;
			tmpMap=(unsigned int)-1;
			for(unsigned int ui=0;ui<NUM_COLOURMAPS;ui++)
			{
				if(value== getColourMapName(ui))
				{
					tmpMap=ui;
					break;
				}
			}

			if(tmpMap >=NUM_COLOURMAPS || tmpMap ==colourMap)
				return false;

			//clear the generic filter cache (i.e. cached outputs)
			//but not the voxel cache
			Filter::clearCache();
			
			needUpdate=true;
			colourMap=tmpMap;
			break;
		}
		case KEY_SHOW_COLOURBAR:
		{
			bool b;
			if(!boolStrDec(value,b))
				return false;

			//if the result is different, the
			//cache should be invalidated
			if(b!=showColourBar)
			{
				needUpdate=true;
				showColourBar=b;
				//clear the generic filter cache (i.e. cached outputs)
				//but not the voxel cache
				Filter::clearCache();
			}
			break;
		}	
		case KEY_MAPSTART:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f >= colourMapBounds[1])
				return false;
		
			if(f!=colourMapBounds[0])
			{
				needUpdate=true;
				colourMapBounds[0]=f;
				//clear the generic filter cache (i.e. cached outputs)
				//but not the voxel cache
				Filter::clearCache();
			}
			break;
		}
		case KEY_MAPEND:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= colourMapBounds[0])
				return false;
		
			if(f!=colourMapBounds[1])
			{
				needUpdate=true;
				colourMapBounds[1]=f;
				//clear the generic filter cache (i.e. cached outputs)
				//but not the voxel cache
				Filter::clearCache();
			}
			break;
		}
		default:
		{
			unsigned int subKeyType,offset;
			demuxKey(key,subKeyType,offset);
			
			//Check for jump to denominator or numerator section
			// TODO: This is a bit of a hack.
			if (subKeyType==KEY_ENABLE_DENOMINATOR) {
				bool b;
				if(!boolStrDec(value,b))
					return false;

				enabledIons[1][offset]=b;
				if (!b) {
					denominatorAll = false;
				}
				needUpdate=true;			
				clearCache();
				vdbCache->clear();
			} else if (subKeyType == KEY_ENABLE_NUMERATOR) {
				bool b;
				if(!boolStrDec(value,b))
					return false;
				
				enabledIons[0][offset]=b;
				if (!b) {
					numeratorAll = false;
				}
				needUpdate=true;			
				clearCache();
				vdbCache->clear();
			}
			else
			{
				ASSERT(false);
			}
			break;
		}
	}
	return true;
}

std::string  LukasAnalysisFilter::getSpecificErrString(unsigned int code) const
{
	const char *errStrs[]={
	 	"",
		"Voxelisation aborted",
		"Out of memory",
		"Unable to perform filter convolution",
		"Voxelisation bounds are invalid",
	};
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(errStrs) == LukasAnalysis_ERR_ENUM_END);	
	
	ASSERT(code < LukasAnalysis_ERR_ENUM_END);
	return errStrs[code];
}

bool LukasAnalysisFilter::writeState(std::ostream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<" << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\"" << escapeXML(userString) << "\"/>" << endl;
			f << tabs(depth+1) << "<voxelsize value=\""<<voxelsize<< "\"/>"  << endl;
			f << tabs(depth+1) << "<fixedwidth value=\""<<fixedWidth << "\"/>"  << endl;
			f << tabs(depth+1) << "<nbins values=\""<<nBins[0] << ","<<nBins[1]<<","<<nBins[2] << "\"/>"  << endl;
			f << tabs(depth+1) << "<binwidth values=\""<<binWidth[0] << ","<<binWidth[1]<<","<<binWidth[2] << "\"/>"  << endl;
			f << tabs(depth+1) << "<normalisetype value=\""<<normaliseType << "\"/>"  << endl;
			f << tabs(depth+1) << "<enabledions>" << endl;

			f << tabs(depth+2) << "<numerator>" << endl;
			for(unsigned int ui=0;ui<enabledIons[0].size(); ui++)
				f << tabs(depth+3) << "<enabled value=\"" << boolStrEnc(enabledIons[0][ui]) << "\"/>" << endl;
			f << tabs(depth+2) << "</numerator>" << endl;

			f << tabs(depth+2) << "<denominator>" << endl;
			for(unsigned int ui=0;ui<enabledIons[1].size(); ui++)
				f << tabs(depth+3) << "<enabled value=\"" << boolStrEnc(enabledIons[1][ui]) << "\"/>" << endl;
			f << tabs(depth+2) << "</denominator>" << endl;

			f << tabs(depth+1) << "</enabledions>" << endl;

			f << tabs(depth+1) << "<representation value=\""<<representation << "\"/>" << endl;
			f << tabs(depth+1) << "<isovalue value=\""<<isoLevel << "\"/>" << endl;
			f << tabs(depth+1) << "<colour r=\"" <<  rgba.r()<< "\" g=\"" << rgba.g() << "\" b=\"" <<rgba.b()
				<< "\" a=\"" << rgba.a() << "\"/>" <<endl;

			f << tabs(depth+1) << "<axialslice>" << endl;
			f << tabs(depth+2) << "<offset value=\""<<sliceOffset<< "\"/>" << endl;
			f << tabs(depth+2) << "<interpolate value=\""<<sliceInterpolate<< "\"/>" << endl;
			f << tabs(depth+2) << "<axis value=\""<<sliceAxis<< "\"/>" << endl;
			f << tabs(depth+2) << "<colourbar show=\""<<boolStrEnc(showColourBar)<< 
						"\" auto=\"" << boolStrEnc(autoColourMap)<< "\" min=\"" <<
					colourMapBounds[0] << "\" max=\"" <<  colourMapBounds[1] << "\"/>" << endl;
			f << tabs(depth+1) << "</axialslice>" << endl;



			f << tabs(depth) << "</" << trueName() <<">" << endl;
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
	stack<xmlNodePtr> nodeStack;

	//--=
	float tmpFloat = 0;
	if(!XMLGetNextElemAttrib(nodePtr,tmpFloat,"voxelsize","value"))
		return false;
	if(tmpFloat <= 0.0f)
		return false;
	voxelsize=tmpFloat;
	//--=

	//Retrieve user string
	//===
	if(XMLHelpFwdToElem(nodePtr,"userstring"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	userString=(char *)xmlString;
	xmlFree(xmlString);
	//===

	//Retrieve fixedWidth mode
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"fixedwidth","value"))
		return false;
	if(!boolStrDec(tmpStr,fixedWidth))
		return false;
	
	//Retrieve nBins	
	if(XMLHelpFwdToElem(nodePtr,"nbins"))
		return false;
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"values");
	if(!xmlString)
		return false;
	std::vector<string> v1;
	splitStrsRef((char *)xmlString,',',v1);
	for (size_t i = 0; i < INDEX_LENGTH && i < v1.size(); i++)
	{
		if(stream_cast(nBins[i],v1[i]))
			return false;
		
		if(nBins[i] <= 0)
			return false;
	}
	xmlFree(xmlString);
	
	//Retrieve bin width 
	if(XMLHelpFwdToElem(nodePtr,"binwidth"))
		return false;
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"values");
	if(!xmlString)
		return false;
	std::vector<string> v2;
	splitStrsRef((char *)xmlString,',',v2);
	for (size_t i = 0; i < INDEX_LENGTH && i < v2.size(); i++)
	{
		if(stream_cast(binWidth[i],v2[i]))
			return false;
		
		if(binWidth[i] <= 0)
			return false;
	}
	xmlFree(xmlString);
	
	//Retrieve normaliseType
	if(!XMLGetNextElemAttrib(nodePtr,normaliseType,"normalisetype","value"))
		return false;
	if(normaliseType >= LukasAnalysis_NORMALISETYPE_MAX)
		return false;

	//Look for the enabled ions bit
	//-------	
	//
	
	if(!XMLHelpFwdToElem(nodePtr,"enabledions"))
	{

		nodeStack.push(nodePtr);
		if(!nodePtr->xmlChildrenNode)
			return false;
		nodePtr=nodePtr->xmlChildrenNode;
		
		//enabled ions for numerator
		if(XMLHelpFwdToElem(nodePtr,"numerator"))
			return false;

		nodeStack.push(nodePtr);

		if(!nodePtr->xmlChildrenNode)
			return false;

		nodePtr=nodePtr->xmlChildrenNode;

		while(nodePtr)
		{
			char c;
			//Retrieve representation
			if(!XMLGetNextElemAttrib(nodePtr,c,"enabled","value"))
				break;

			if(c == '1')
				enabledIons[0].push_back(true);
			else
				enabledIons[0].push_back(false);


			nodePtr=nodePtr->next;
		}

		nodePtr=nodeStack.top();
		nodeStack.pop();

		//enabled ions for denominator
		if(XMLHelpFwdToElem(nodePtr,"denominator"))
			return false;


		if(!nodePtr->xmlChildrenNode)
			return false;

		nodeStack.push(nodePtr);
		nodePtr=nodePtr->xmlChildrenNode;

		while(nodePtr)
		{
			char c;
			//Retrieve representation
			if(!XMLGetNextElemAttrib(nodePtr,c,"enabled","value"))
				break;

			if(c == '1')
				enabledIons[1].push_back(true);
			else
				enabledIons[1].push_back(false);
				

			nodePtr=nodePtr->next;
		}


		nodeStack.pop();
		nodePtr=nodeStack.top();
		nodeStack.pop();

		//Check that the enabled ions size makes at least some sense...
		if(enabledIons[0].size() != enabledIons[1].size())
			return false;

	}

	//-------	
	//Retrieve representation
	if(!XMLGetNextElemAttrib(nodePtr,representation,"representation","value"))
		return false;
	if(representation >=VOXEL_REPRESENT_END)
		return false;

	//-------	
	//Retrieve representation
	if(!XMLGetNextElemAttrib(nodePtr,isoLevel,"isovalue","value"))
		return false;

	//Retrieve colour
	//====
	if(XMLHelpFwdToElem(nodePtr,"colour"))
		return false;
	ColourRGBAf tmpRgba;
	if(!parseXMLColour(nodePtr,tmpRgba))
		return false;
	rgba=tmpRgba;

	//====

	//try to retrieve slice, where possible
	if(!XMLHelpFwdToElem(nodePtr,"axialslice"))
	{
		xmlNodePtr sliceNodes;
		sliceNodes=nodePtr->xmlChildrenNode;

		if(!sliceNodes)
			return false;

		if(!XMLGetNextElemAttrib(sliceNodes,sliceOffset,"offset","value"))
			return false;

		sliceOffset=std::min(sliceOffset,1.0f);
		sliceOffset=std::max(sliceOffset,0.0f);
		
		
		if(!XMLGetNextElemAttrib(sliceNodes,sliceInterpolate,"interpolate","value"))
			return false;

		if(sliceInterpolate >=VOX_INTERP_ENUM_END)
			return false;

		if(!XMLGetNextElemAttrib(sliceNodes,sliceAxis,"axis","value"))
			return false;

		if(sliceAxis > 2)
			sliceAxis=2;
		
		if(!XMLGetNextElemAttrib(sliceNodes,showColourBar,"colourbar","show"))
			return false;

		if(!XMLGetAttrib(sliceNodes,autoColourMap,"auto"))
			return false;
		
		if(!XMLGetAttrib(sliceNodes,colourMapBounds[0],"min"))
			return false;

		if(!XMLGetAttrib(sliceNodes,colourMapBounds[1],"max"))
			return false;

		if(colourMapBounds[0] >= colourMapBounds[1])
			return false;
	}
	



	return true;
	
}

unsigned int LukasAnalysisFilter::getRefreshBlockMask() const
{
	//Ions, plots and voxels cannot pass through this filter
	return STREAM_TYPE_PLOT | STREAM_TYPE_VOXEL;
}

unsigned int LukasAnalysisFilter::getRefreshEmitMask() const
{

	switch(representation)
	{
		case VOXEL_REPRESENT_ISOSURF:
			{
				return STREAM_TYPE_OPENVDBGRID | STREAM_TYPE_IONS | STREAM_TYPE_RANGE | STREAM_TYPE_PLOT;
			}

		case VOXEL_REPRESENT_POINTCLOUD:
		case VOXEL_REPRESENT_AXIAL_SLICE:
			{
				return STREAM_TYPE_VOXEL | STREAM_TYPE_DRAW |STREAM_TYPE_RANGE;
			}
	}
}

unsigned int LukasAnalysisFilter::getRefreshUseMask() const
{
	switch(representation)
	{
		case VOXEL_REPRESENT_ISOSURF:
			{
				return STREAM_TYPE_OPENVDBGRID| STREAM_TYPE_IONS | STREAM_TYPE_RANGE;
			}

		case VOXEL_REPRESENT_POINTCLOUD:
		case VOXEL_REPRESENT_AXIAL_SLICE:
			{
				return STREAM_TYPE_IONS | STREAM_TYPE_RANGE;
			}
	}
}

void LukasAnalysisFilter::getTexturedSlice(const Voxels<float> &v, 
			size_t axis,float offset, size_t interpolateMode,
			float &minV,float &maxV,DrawTexturedQuad &texQ) const
{
	ASSERT(axis < 3);


	size_t dim[3]; //dim0 and 2 are the in-plane axes. dim3 is the normal axis
	v.getSize(dim[0],dim[1],dim[2]);

	switch(axis)
	{
		//x-normal
		case 0:
			rotate3(dim[0],dim[1],dim[2]);
			std::swap(dim[0],dim[1]);
			break;
		//y-normal
		case 1:
			rotate3(dim[2],dim[1],dim[0]);
			break;
		//z-normal
		case 2:
			std::swap(dim[0],dim[1]);
			break;
			
	}


	ASSERT(dim[0] >0 && dim[1] >0);
	
	texQ.resize(dim[0],dim[1],3);

	//Generate the texture from the voxel data
	//---
	float *data = new float[dim[0]*dim[1]];
	

	ASSERT(offset >=0 && offset <=1.0f);

	v.getInterpSlice(axis,offset,data,interpolateMode);

	if(autoColourMap)
	{
		minV=minValue(data,dim[0]*dim[1]);
		maxV=maxValue(data,dim[0]*dim[1]);
	}
	else
	{
		minV=colourMapBounds[0];
		maxV=colourMapBounds[1];

	}
	ASSERT(minV <=maxV);

	unsigned char rgb[3];
	for(size_t ui=0;ui<dim[0];ui++)
	{
		for(size_t uj=0;uj<dim[1];uj++)
		{
			colourMapWrap(colourMap,rgb, data[ui*dim[1] + uj],
					minV,maxV,false);
			
			texQ.setData(ui,uj,rgb);	
		}
	}

	delete[] data;
	//---
	
	
	
	//Set the vertices of the quad
	//--
	//compute the real position of the plane
	float minPos,maxPos,offsetRealPos;
	v.getAxisBounds(axis,minPos,maxPos);
	offsetRealPos = ((float)offset)*(maxPos-minPos) + minPos; 
	
	
	Point3D verts[4];
	v.getBounds(verts[0],verts[2]);
	//set opposite vertices to upper and lower bounds of quad
	verts[0][axis]=verts[2][axis]=offsetRealPos;
	//set other vertices to match, then shift them in the axis plane
	verts[1]=verts[0]; 
	verts[3]=verts[2];


	unsigned int shiftAxis=(axis+1)%3;
	verts[1][shiftAxis] = verts[2][shiftAxis];
	verts[3][shiftAxis] = verts[0][shiftAxis];

	//Correction for y texture orientation
	if(axis==1)
		std::swap(verts[1],verts[3]);

	texQ.setVertices(verts);
	//--


}

