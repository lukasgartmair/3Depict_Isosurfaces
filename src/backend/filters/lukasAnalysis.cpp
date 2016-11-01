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

#include "common/colourmap.h"

#include <map>

#include "openvdb_includes.h"

#include <algorithm>

using std::vector;
using std::string;
using std::pair;
using std::make_pair;


enum
{
	KEY_COLOUR,
	KEY_ISOLEVEL,
	KEY_ENABLE_NUMERATOR,
	KEY_ENABLE_DENOMINATOR
};

LukasAnalysisFilter::LukasAnalysisFilter() : 
	rsdIncoming(0)
{ 

	rgba=ColourRGBAf(0.5,0.5,0.5,1.0f);
	iso_level=0.07;
	numeratorAll = false;
	denominatorAll = true;
	colourMap=0;
	autoColourMap=true;
	colourMapBounds[0]=0;
	colourMapBounds[1]=1;
	showColourBar=false;

	cacheOK=false;
	cache=true; //By default, we should cache, but decision is made higher up
	rsdIncoming=0;
}

Filter *LukasAnalysisFilter::cloneUncached() const
{
	LukasAnalysisFilter *p=new LukasAnalysisFilter();
	
	p->rgba=rgba;
	
	p->numeratorAll=numeratorAll;
	p->denominatorAll=denominatorAll;

	p->iso_level=iso_level;

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
	{
		p->rsdIncoming=0;
	}
	p->colourMap = colourMap;

	p->nColours = nColours;
	p->showColourBar = showColourBar;
	p->autoColourMap = autoColourMap;
	p->colourMapBounds[0] = colourMapBounds[0];
	p->colourMapBounds[1] = colourMapBounds[1];

	//We are copying whether to cache or not,
	//not the cache itself

	p->cache=cache;
	p->cacheOK=false;
	
	p->userString=userString;
	return p;
}

void LukasAnalysisFilter::clearCache() 
{
	vdbCache->clear();
	Filter::clearCache();
}


// little workaround as the filter template number of bytes only takes 
// size_t n objects
/*
size_t getGridMemUsage(openvdb::FloatGrid::Ptr grid)
{
	return grid->memUsage();
}
*/
size_t LukasAnalysisFilter::numBytesForCache(size_t nObjects) const
{
	//Return the number of bytes of memory used by this grid. 
	// function implemented in OpenVDB
	//size_t num_bytes_for_cache = grid->memUsage();
	// but how can i get access to the grid from here?
	// as the function is a template only size_t nObjects can go in
	// maybe i create a new template in filter.cpp
	// but then i am still left with the problem that i need to pass the grid somewhere
	
	// current size is ca. the result of memUsage of the division grid cu / al + cu
	size_t num_bytes_for_cache =  2000000;
	return num_bytes_for_cache;
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



/// here the actual filter work is done 
unsigned int LukasAnalysisFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress)
{	

	//Disallow copying of anything in the blockmask. Copy everything else
	propagateStreams(dataIn,getOut,getRefreshBlockMask(),true);
	
	//use the cached copy if we have it.
	if(cacheOK)
	{
		propagateCache(getOut);
		return 0;
	}

	// Initialize the OpenVDB library.  This must be called at least
    	// once per program and may safely be called multiple times.
	openvdb::initialize();

	// recalculate the isosurface mesh to perform calculations based on it

	std::vector<openvdb::Vec3s> points;
  	std::vector<openvdb::Vec3I> triangles;
  	std::vector<openvdb::Vec4I> quads;	

	try
	{
		openvdb::tools::volumeToMesh<openvdb::FloatGrid>(*grid, points, triangles, quads, isovalue);	
		cacheOK=true;
	}
	catch(const std::exception &e)
	{
		ASSERT(false);
		cerr << "Exception! :" << e.what() << endl;
	}

	// checking the mesh for nonfinite coordinates like -nan and inf

	int non_finites_counter = 0;
	int xyzs = 3;
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
	//std::cout << "points size" << " = " << points.size() << std::endl;
	//std::cout << "number_of_non_finites" << " = " << non_finites_counter << std::endl;

	// how are the -nans introduced if there is no -nan existing in the grid?! 
	// setting only the nan to zero will of course result in large triangles crossing the scene
	// setting all 3 coordinates to zero is also shit because triangles containing the point are also big
	// how to overcome this without discarding them, which would end up in corrupt faces
	// this behaviour gets checked in the vdb test suite

	xyzs = 3;
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

	//std::cout << "points size" << " = " << points.size() << std::endl;
	//std::cout << "triangles size" << " = " << triangles.size() << std::endl;
	//std::cout << " active voxel count subgrid div" << " = " << grid->activeVoxelCount() << std::endl;

	// create a triangular mesh
	int number_of_splitted_triangles = 2*quads.size();
	std::vector<std::vector<float> > triangles_from_splitted_quads(number_of_splitted_triangles, std::vector<float>(xyzs));

	triangles_from_splitted_quads = splitQuadsToTriangles(points, quads);

	std::vector<std::vector<float> > triangles_combined;
	triangles_combined = concatenateTriangleVectors(triangles, triangles_from_splitted_quads);


	// calculate a signed distance field

	// bandwidths are in voxel units
	float bandwidth = 30;
	float in_bandwidth = 30;
	float ex_bandwidth = 30;

	double voxelsize_levelset = 0.001;

	//openvdb::FloatGrid::Ptr sdf = openvdb::tools::meshToUnsignedDistanceField<openvdb::FloatGrid>(openvdb::math::Transform(), points, triangles, quads, bandwidth);
	openvdb::FloatGrid::Ptr sdf = openvdb::tools::meshToSignedDistanceField<openvdb::FloatGrid>(openvdb::math::Transform(), points, triangles, quads, ex_bandwidth, in_bandwidth);
	// signed distance field
	sdf->setTransform(openvdb::math::Transform::createLinearTransform(voxelsize_levelset));

	
	// now get a copy of that grid, set all its active values from the narrow band to zero and fill only them with ions of interest

	const float background = 0.0f;	
	openvdb::FloatGrid::Ptr numerator_grid = openvdb::FloatGrid::create(background);
	openvdb::FloatGrid::Ptr denominator_grid = openvdb::FloatGrid::create(background);

	openvdb::FloatGrid::Accessor numerator_accessor = numerator_grid->getAccessor();
	openvdb::FloatGrid::Accessor denominator_accessor = denominator_grid->getAccessor();

	numerator_grid = sdf->deepCopy();
	denominator_grid = sdf->deepCopy();

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

	int voxel_counter = 0;
	for (openvdb::FloatGrid::ValueOnIter iter = numerator_grid->beginValueOn(); iter; ++iter)
	{   
    			iter.setValue(0.0);
			
			hkl = iter.getCoord();
			active_voxel_indices[voxel_counter][0] = hkl.x();
			active_voxel_indices[voxel_counter][1] = hkl.y();
			active_voxel_indices[voxel_counter][2] = hkl.z();

			voxel_counter += 1;
	}
	for (openvdb::FloatGrid::ValueOnIter iter = denominator_grid->beginValueOn(); iter; ++iter)
	{   
    			iter.setValue(0.0);
	}


	// now run through all ions but only once, and in case they are inside a active voxel write only to them

	for(size_t ui=0;ui<dataIn.size();ui++)
	{
		//Check for ion stream types. Don't use anything else in counting
		if(dataIn[ui]->getStreamType() != STREAM_TYPE_IONS)
			continue;

		const IonStreamData  *ions; 
		ions = (const IonStreamData *)dataIn[ui];
	
		//get the denominator ions
		unsigned int ionID;
		ionID = rsdIncoming->rangeFile->getIonID(ions->data[0].getMassToCharge());

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


		// run here trough all ions and check whether they are inside some of the volumes or not

		for(size_t uj=0;uj<ions->data.size(); uj++)
		{
			std::vector<float> atom_position(xyzs); 
			for (int i=0;i<xyzs;i++)
			{
				atom_position[i] = ions->data[uj].getPos()[i];
			}

			// 1st step - project the current atom position to unit voxel i.e. from 0 to 1
			std::vector<float> position_in_unit_voxel;
			position_in_unit_voxel = projectAtompositionToUnitvoxel(atom_position, voxel_size);

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
			adjacent_voxel_vertices = determineAdjacentVoxelVertices(atom_position, voxel_size);
		
			// 4th step - assign each of the 8 adjacent voxels the corresponding contribution that results from the atom position in the unit voxel
			const int number_of_adjacent_voxels = 8;
			std::vector<float> current_voxel_index;

			// for the proxigram an additional condition has to be introduced -> is the voxel active? if not pass it

			for (int i=0;i<number_of_adjacent_voxels;i++)
			{
				current_voxel_index = adjacent_voxel_vertices[i];
				// normalized voxel indices based on 00, 01, 02 etc. // very important otherwise there will be spacings
				openvdb::Coord ijk(current_voxel_index[0], current_voxel_index[1], current_voxel_index[2]);
	
				//initialize a (0,0,0) vector
				std::vector<float> vector_to_search(xyzs);
				vector_to_search[0] = ijk.x();
				vector_to_search[1] = ijk.y();
				vector_to_search[2] = ijk.z();

				// narrow band condition
				if(std::find(active_voxel_indices.begin(), active_voxel_indices.end(), vector_to_search ) != active_voxel_indices.end()) 
				{
					// write to denominator grid
					if(thisDenominatorIonEnabled)
					{
				
						denominator_accessor.setValue(ijk, contributions_to_adjacent_voxels[i] + denominator_accessor.getValue(ijk));
					}
					else
					{
						denominator_accessor.setValue(ijk, 0.0 + denominator_accessor.getValue(ijk));
					}

					// write to numerator grid
					if(thisNumeratorIonEnabled)
					{	
						numerator_accessor.setValue(ijk, contributions_to_adjacent_voxels[i] + numerator_accessor.getValue(ijk));
					}
					else
					{
						numerator_accessor.setValue(ijk, 0.0 + numerator_accessor.getValue(ijk));
					}
				}
				else
				{
					//pass
				}			
			}
		}
	}


	// manage the filter output
	std::cerr << "Completing evaluation of VDB grid..." << endl;

	OpenVDBGridStreamData *gs = new OpenVDBGridStreamData();
	gs->parent=this;
	// just like the swap function of the voxelization does pass the grids here to gs->grids
	gs->grid = divgrid->deepCopy();
	
	gs->isovalue=iso_level;
	gs->r=rgba.r();
	gs->g=rgba.g();
	gs->b=rgba.b();
	gs->a=rgba.a();
	
	if(cache)
	{
		gs->cached=1;
		cacheOK=true;
		filterOutputs.push_back(gs);
	}
	else
	{
		gs->cached=0;
	}
	//Store the vdbgrid on the output
	getOut.push_back(gs);

	return 0;
}

void LukasAnalysisFilter::getProperties(FilterPropGroup &propertyList) const
{
	if(!rsdIncoming)
		return;

	// group computation

	FilterProperty p;
	size_t curGroup=0;

	// group numerator
		
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
		p.key=muxKey(KEY_ENABLE_NUMERATOR,ui);
		p.data=str;
		p.name=rsdIncoming->rangeFile->getName(ui);
		p.type=PROPERTY_TYPE_BOOL;
		p.helpText=TRANS("Enable this ion for numerator");
		propertyList.addProperty(p,curGroup);
	}

	propertyList.setGroupTitle(curGroup,TRANS("Numerator"));
	curGroup++;
	
	// group denominator

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

	
	// group representation
	
	p.name=TRANS("Representation");

	//-- Isosurface parameters --

	stream_cast(tmpStr,iso_level);
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
	propertyList.setGroupTitle(curGroup,TRANS("Appearance"));	
	curGroup++;

}

bool LukasAnalysisFilter::setProperty(  unsigned int key,
					const std::string &value, bool &needUpdate)
{
	needUpdate=false;
	switch(key)
	{

	
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
			break;
		}

		case KEY_ISOLEVEL:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f < 0.0f)
				return false;
			if(f > 1.0f)
				return false;
			needUpdate=true;
			iso_level=f;
			//Go in and manually adjust the cached
			//entries to have the new value, rather
			//than doing a full recomputation
			if(cacheOK)
			{
				for(unsigned int ui=0;ui<filterOutputs.size();ui++)
				{	
					OpenVDBGridStreamData *vdbgs;
					vdbgs = (OpenVDBGridStreamData*)filterOutputs[ui];
					vdbgs->isovalue = iso_level;
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
					OpenVDBGridStreamData *vdbgs;
					vdbgs = (OpenVDBGridStreamData*)filterOutputs[ui];
					vdbgs->r=rgba.r();
					vdbgs->g=rgba.g();
					vdbgs->b=rgba.b();
				}
			}
			break;
		}
		default:
		{
		
		// this default entry is important for the independent choice
		// for the numerators and denominators !!!
		
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
			f << tabs(depth+1) << "<userstring value=\""<< escapeXML(userString) << "\"/>"  << endl;

			f << tabs(depth+1) << "<iso_level value=\""<<iso_level<< "\"/>"  << endl;
			
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
			
			f << tabs(depth+1) << "<colour r=\"" <<  rgba.r()<< "\" g=\"" << rgba.g() << "\" b=\"" <<rgba.b()
				<< "\" a=\"" << rgba.a() << "\"/>" <<endl;
				
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

	stack<xmlNodePtr> nodeStack;
	xmlChar *xmlString;
	
	//Retrieve user string
	if(XMLHelpFwdToElem(nodePtr,"userstring"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	userString=(char *)xmlString;
	xmlFree(xmlString);

	//--
	tmpFloat = 0;
	if(!XMLGetNextElemAttrib(nodePtr,tmpFloat,"iso_level","value"))
		return false;
	if(tmpFloat <= 0.0f)
		return false;
	iso_level=tmpFloat;
	//--=
	

	
	//Retrieve colour
	//====
	if(XMLHelpFwdToElem(nodePtr,"colour"))
		return false;
	ColourRGBAf tmpRgba;
	if(!parseXMLColour(nodePtr,tmpRgba))
		return false;
	rgba=tmpRgba;

	//====
	
	//--=
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

	return true;
}

unsigned int LukasAnalysisFilter::getRefreshBlockMask() const
{
	return 0;
}

unsigned int LukasAnalysisFilter::getRefreshEmitMask() const
{
	return STREAM_TYPE_OPENVDBGRID;
}

unsigned int LukasAnalysisFilter::getRefreshUseMask() const
{
	return STREAM_TYPE_OPENVDBGRID | STREAM_TYPE_RANGE;
}


