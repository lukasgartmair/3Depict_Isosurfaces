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

using std::vector;
using std::string;
using std::pair;
using std::make_pair;


enum
{
	KEY_VOXELSIZE,
	KEY_COLOUR,
	KEY_ISOLEVEL,
	KEY_ADAPTIVITY,
	KEY_IONSPECIES
};

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

	ionspecies_enabled = false;

	rgba=ColourRGBAf(0.5,0.5,0.5,0.9f);
	iso_level=0.07;
	voxel_size = 2.0; 
	adaptivity = 0.1;
	colourMap=0;
	autoColourMap=true;
	colourMapBounds[0]=0;
	colourMapBounds[1]=1;
	cacheOK=false;
	cache=true; //By default, we should cache, but decision is made higher up
	rsdIncoming=0;
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
	}
	else
	{
		//If we didn't have an incoming rsd, then make one up!
		if(!rsdIncoming)
		{
			rsdIncoming= new RangeStreamData;
			*rsdIncoming=*c;
			//set the numerator to all disabled
			enabledIons[0].resize(rsdIncoming->rangeFile->getNumIons(),0);
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
				//set the numerator to all disabled
				enabledIons[0].resize(rsdIncoming->rangeFile->getNumIons(),0);
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
	
	
	p->ionspecies_enabled = ionspecies_enabled;
	

	p->rgba=rgba;

	p->iso_level=iso_level;
	p->voxel_size=voxel_size;
	p->adaptivity=adaptivity;

	p->enabledIons[0].resize(enabledIons[0].size());
	std::copy(enabledIons[0].begin(),enabledIons[0].end(),p->enabledIons[0].begin());

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

	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

/// here the actual filter work is done 
unsigned int LukasAnalysisFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress)
{	
	
	// check whether easy ion count works with refreshing
	
	//Count the number of ions input
	std::string str;
	size_t numTotalPoints = numElements(dataIn,STREAM_TYPE_IONS);
			stream_cast(str,numTotalPoints);
			str=std::string(TRANS("--Counts--") );
			consoleOutput.push_back(str);

			stream_cast(str,numTotalPoints);

			consoleOutput.push_back(str);
			consoleOutput.push_back("");


	// Initialize the OpenVDB library.  This must be called at least
    	// once per program and may safely be called multiple times.
	openvdb::initialize();

	
	const float background = 0.0f;	

	// initialize the main grid containing all ions
	grid = openvdb::FloatGrid::create(background);
	openvdb::FloatGrid::Accessor accessor = grid->getAccessor();

	// initialize subgrid for one chosen ion species
	subgrid1 = openvdb::FloatGrid::create(background);
	openvdb::FloatGrid::Accessor subaccessor1 = subgrid1->getAccessor();

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

			unsigned int idIon;
			idIon = r->getIonID(ions->data[uj].getMassToCharge());
			
			// in this place the ion id has to be assigned to the chosen ion species
			// ionIDs i think are aluminum 0 and copper 1
			// is it the order in the range file?
			if (idIon == 1)
			{
			    subaccessor1.setValue(ijk, 1.0 + subaccessor1.getValue(ijk));
			}
			else
			{
			    subaccessor1.setValue(ijk, 0.0 + subaccessor1.getValue(ijk));
			}
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

	//check for negative nans and infs introduced by the division
	//set them to zero in order not to obtain nan mesh coordinates

	for (openvdb::FloatGrid::ValueAllIter iter = subgrid1->beginValueAll(); iter; ++iter)
	{   
	    if (std::isfinite(iter.getValue()) == false)
	{
	    iter.setValue(0.0);
	}
	}


	subgrid1->setTransform(openvdb::math::Transform::createLinearTransform(voxel_size));

	// volume to mesh conversion is done in Drawables.cpp where the mesh is updated

	// manage the filter output

	OpenVDBGridStreamData *gs = new OpenVDBGridStreamData();
	gs->parent=this;
	// just like the swap function of the voxelization does pass the grids here to gs->grids
	gs->grid = subgrid1->deepCopy();
	std::cout << " active voxel count gs grid" << " = " << gs->grid->activeVoxelCount() << std::endl;
	
	gs->isovalue=iso_level;
	gs->adaptivity=adaptivity;
	gs->voxelsize = voxel_size;
	
	gs->r=rgba.r();
	gs->g=rgba.g();
	gs->b=rgba.b();
	gs->a=rgba.a();
	
	gs->cached=1;
	cacheOK=true;
	filterOutputs.push_back(gs);


	//Store the vdbgrid on the output
	getOut.push_back(gs);

	return 0;
}


size_t LukasAnalysisFilter::numBytesForCache(size_t nObjects) const
{
	return 0;
}


void LukasAnalysisFilter::getProperties(FilterPropGroup &propertyList) const
{

	// 1st group computation

	FilterProperty p;
	size_t curGroup=0;

	string tmpStr = "";
	stream_cast(tmpStr,voxel_size);
	p.name=TRANS("Voxelsize");
	p.data=tmpStr;
	p.key=KEY_VOXELSIZE;
	p.type=PROPERTY_TYPE_REAL;
	p.helpText=TRANS("Voxel size in x,y,z direction");
	propertyList.addProperty(p,curGroup);
	propertyList.setGroupTitle(curGroup,TRANS("Computation"));
	
	curGroup++;
	
	// 2nd group ranges
	
	
	// in my case i only choose the numerator because the denominator is the grid with all ions
	// rsdIncoming has to be true to start Lukas Analysis
	// numerator
	
	if (rsdIncoming) 
	{
	
		ASSERT(rsdIncoming->enabledIons.size()==enabledIons[0].size());	

		//Look at the numerator	
		for(unsigned  int ui=0; ui<rsdIncoming->enabledIons.size(); ui++)
		{
			string str = "";
			str=boolStrEnc(enabledIons[0][ui]);

			//Append the ion name with a checkbox
			p.name=rsdIncoming->rangeFile->getName(ui);
			p.data=str;
			p.key = KEY_IONSPECIES;
			p.type=PROPERTY_TYPE_BOOL;
			p.helpText=TRANS("Get the concentration of THIS ion species");
			propertyList.addProperty(p,curGroup);
		}
	
		propertyList.setGroupTitle(curGroup,TRANS("Ranges"));
		curGroup++;
	}
	
	// 3rd group representation
	
	p.name=TRANS("Representation");

	//-- Isosurface parameters --

	stream_cast(tmpStr,iso_level);
	p.name=TRANS("Isovalue");
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

}

bool LukasAnalysisFilter::setProperty(  unsigned int key,
					const std::string &value, bool &needUpdate)
{
	needUpdate=false;
	switch(key)
	{
		case KEY_ADAPTIVITY: 
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
			needUpdate=true;
			adaptivity=f;
			//Go in and manually adjust the cached
			//entries to have the new value, rather
			//than doing a full recomputation
			if(cacheOK)
			{
				for(unsigned int ui=0;ui<filterOutputs.size();ui++)
				{	
					OpenVDBGridStreamData *vdbgs;
					vdbgs = (OpenVDBGridStreamData*)filterOutputs[ui];
					vdbgs->adaptivity = adaptivity;

				}
			}
			break;
		}	
		
		case KEY_IONSPECIES:
		{
			bool b;
			if(stream_cast(b,value))
				return false;
			//Set them all to enabled or disabled as a group	
			for (size_t i = 0; i < enabledIons[0].size(); i++) 
				enabledIons[0][i] = b;
			ionspecies_enabled = b;
			needUpdate=true;
			clearCache();
			break;
		}
	
	
		case KEY_VOXELSIZE: 
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
			needUpdate=true;
			voxel_size=f;
			//Go in and manually adjust the cached
			//entries to have the new value, rather
			//than doing a full recomputation
			if(cacheOK)
			{
				for(unsigned int ui=0;ui<filterOutputs.size();ui++)
				{	
					OpenVDBGridStreamData *vdbgs;
					vdbgs = (OpenVDBGridStreamData*)filterOutputs[ui];
					vdbgs->voxelsize = voxel_size;

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

			f << tabs(depth+1) << "<voxel_size value=\""<<voxel_size<< "\"/>"  << endl;
			f << tabs(depth+1) << "<iso_level value=\""<<iso_level<< "\"/>"  << endl;
			f << tabs(depth+1) << "<adaptivity value=\""<<adaptivity<< "\"/>"  << endl;
			
			for(unsigned int ui=0;ui<enabledIons[0].size(); ui++)
				f << tabs(depth+3) << "<enabled value=\"" << boolStrEnc(enabledIons[0][ui]) << "\"/>" << endl;
				
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

	//--=
	float tmpFloat = 0;
	if(!XMLGetNextElemAttrib(nodePtr,tmpFloat,"voxel_size","value"))
		return false;
	if(tmpFloat <= 0.0f)
		return false;
	voxel_size=tmpFloat;
	//--=

	//--
	tmpFloat = 0;
	if(!XMLGetNextElemAttrib(nodePtr,tmpFloat,"iso_level","value"))
		return false;
	if(tmpFloat <= 0.0f)
		return false;
	iso_level=tmpFloat;
	//--=
	
	//--
	tmpFloat = 0;
	if(!XMLGetNextElemAttrib(nodePtr,tmpFloat,"adaptivity","value"))
		return false;
	if(tmpFloat <= 0.0f)
		return false;
	adaptivity=tmpFloat;
	//--=

	if(!XMLHelpFwdToElem(nodePtr,"enabledions"))
	{

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


	}

	return true;
}

unsigned int LukasAnalysisFilter::getRefreshBlockMask() const
{
	return 0;
	//return STREAMTYPE_MASK_ALL;
}

unsigned int LukasAnalysisFilter::getRefreshEmitMask() const
{
	//return  STREAM_TYPE_OPENVDBGRID | STREAM_TYPE_DRAW | STREAM_TYPE_RANGE;
	return 0;
}

unsigned int LukasAnalysisFilter::getRefreshUseMask() const
{
	return  STREAM_TYPE_RANGE |  STREAM_TYPE_OPENVDBGRID ;
	//return 0;
}


