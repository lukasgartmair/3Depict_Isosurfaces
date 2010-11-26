/*
 *	filter.h - modular data filter implementation 
 *	Copyright (C) 2010, D Haley 

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

#include "filter.h"
#include "K3DTree.h"
#include "XMLHelper.h"

#include "colourmap.h"
#include "quat.h"
#include "rdf.h"

#include <limits>
#include <fstream>
#include <algorithm>

#include <wx/process.h>
#include <wx/filename.h>
#include <wx/dir.h>

#ifdef _OPENMP
#include <omp.h>
#endif

using std::list;
using std::vector;
using std::string;
using std::pair;
using std::make_pair;

const unsigned int NUM_CALLBACK=50000;
const size_t MAX_IONS_LOAD_DEFAULT=5*1024*1024/sizeof(IONHIT); //5 MB worth.

const unsigned int MAX_NUM_COLOURS=256;
// Tp prevent the dropdown lists from getting too unwieldy, set an artificial maximum
const unsigned int MAX_NUM_FILE_COLS=5000; 

const unsigned int SPECTRUM_MAX_BINS=100000;

bool Filter::strongRandom= false;

const char *STREAM_NAMES[] = { "Ion",
				"Plot",
				"Draw",
				"Range",
				"Voxel"};

//Key types for filter properties.
enum
{
	KEY_IONCLIP_ORIGIN=1,
	KEY_IONCLIP_PRIMITIVE_TYPE,
	KEY_IONCLIP_RADIUS,
	KEY_IONCLIP_PRIMITIVE_SHOW,
	KEY_IONCLIP_PRIMITIVE_INVERTCLIP,
	KEY_IONCLIP_NORMAL,
	KEY_IONCLIP_CORNER,
	KEY_IONCLIP_AXIS_LOCKMAG,

	KEY_IONCOLOURFILTER_COLOURMAP,
	KEY_IONCOLOURFILTER_MAPSTART,
	KEY_IONCOLOURFILTER_MAPEND,
	KEY_IONCOLOURFILTER_NCOLOURS,
	KEY_IONCOLOURFILTER_SHOWBAR,

	KEY_SPECTRUM_BINWIDTH,
	KEY_SPECTRUM_AUTOEXTREMA,
	KEY_SPECTRUM_MIN,
	KEY_SPECTRUM_MAX,
	KEY_SPECTRUM_LOGARITHMIC,
	KEY_SPECTRUM_PLOTTYPE,
	KEY_SPECTRUM_COLOUR,

	KEY_POSLOAD_FILE,
	KEY_POSLOAD_SIZE,
	KEY_POSLOAD_COLOUR,
	KEY_POSLOAD_IONSIZE,
	KEY_POSLOAD_ENABLED,
	KEY_POSLOAD_SELECTED_COLUMN0,
	KEY_POSLOAD_SELECTED_COLUMN1,
	KEY_POSLOAD_SELECTED_COLUMN2,
	KEY_POSLOAD_SELECTED_COLUMN3,
	KEY_POSLOAD_NUMBER_OF_COLUMNS,

	KEY_IONDOWNSAMPLE_FRACTION,
	KEY_IONDOWNSAMPLE_FIXEDOUT,
	KEY_IONDOWNSAMPLE_COUNT,

	KEY_RANGE_ACTIVE,
	KEY_RANGE_IONID,
	KEY_RANGE_START,
	KEY_RANGE_END,

	KEY_COMPPROFILE_BINWIDTH,
	KEY_COMPPROFILE_FIXEDBINS,
	KEY_COMPPROFILE_NORMAL,
	KEY_COMPPROFILE_NUMBINS,
	KEY_COMPPROFILE_ORIGIN,
	KEY_COMPPROFILE_PLOTTYPE,
	KEY_COMPPROFILE_PRIMITIVETYPE,
	KEY_COMPPROFILE_RADIUS,
	KEY_COMPPROFILE_SHOWPRIMITIVE,
	KEY_COMPPROFILE_NORMALISE,
	KEY_COMPPROFILE_COLOUR,
	KEY_COMPPROFILE_ERRMODE,
	KEY_COMPPROFILE_AVGWINSIZE,
	KEY_COMPPROFILE_LOCKAXISMAG,

	KEY_BOUNDINGBOX_VISIBLE,
	KEY_BOUNDINGBOX_COUNT_X,
	KEY_BOUNDINGBOX_COUNT_Y,
	KEY_BOUNDINGBOX_COUNT_Z,
	KEY_BOUNDINGBOX_FONTSIZE,
	KEY_BOUNDINGBOX_FONTCOLOUR,
	KEY_BOUNDINGBOX_FIXEDOUT,
	KEY_BOUNDINGBOX_LINECOLOUR,
	KEY_BOUNDINGBOX_LINEWIDTH,
	KEY_BOUNDINGBOX_SPACING_X,
	KEY_BOUNDINGBOX_SPACING_Y,
	KEY_BOUNDINGBOX_SPACING_Z,

	KEY_TRANSFORM_MODE,
	KEY_TRANSFORM_SCALEFACTOR,
	KEY_TRANSFORM_ORIGIN,
	KEY_TRANSFORM_SHOWORIGIN,
	KEY_TRANSFORM_ORIGINMODE,
	KEY_TRANSFORM_ROTATE_ANGLE,
	KEY_TRANSFORM_ROTATE_AXIS,
	

	KEY_EXTERNALPROGRAM_COMMAND,
	KEY_EXTERNALPROGRAM_WORKDIR,
	KEY_EXTERNALPROGRAM_ALWAYSCACHE,
	KEY_EXTERNALPROGRAM_CLEANUPINPUT,

	KEY_SPATIALANALYSIS_STOPMODE,
	KEY_SPATIALANALYSIS_ALGORITHM,
	KEY_SPATIALANALYSIS_DISTMAX,
	KEY_SPATIALANALYSIS_NNMAX,
	KEY_SPATIALANALYSIS_NUMBINS,
	KEY_SPATIALANALYSIS_REMOVAL,
	KEY_SPATIALANALYSIS_REDUCTIONDIST,
	KEY_SPATIALANALYSIS_COLOUR,
	KEY_SPATIALANALYSIS_ENABLE_SOURCE,
	KEY_SPATIALANALYSIS_ENABLE_TARGET,

	KEY_VOXEL_FIXEDWIDTH,
	KEY_VOXEL_NBINSX,
	KEY_VOXEL_NBINSY,
	KEY_VOXEL_NBINSZ,
	KEY_VOXEL_WIDTHBINSX,
	KEY_VOXEL_WIDTHBINSY,
	KEY_VOXEL_WIDTHBINSZ,
	KEY_VOXEL_COUNT_TYPE,
	KEY_VOXEL_NORMALISE_TYPE,
	KEY_VOXEL_SPOTSIZE,
	KEY_VOXEL_TRANSPARANCY,
	KEY_VOXEL_COLOUR,
	KEY_VOXEL_ISOLEVEL,
	KEY_VOXEL_REPRESENTATION_MODE,
	KEY_VOXEL_ENABLE_NUMERATOR,
	KEY_VOXEL_ENABLE_DENOMINATOR,
};

//Keys for binding IDs
enum
{
	BINDING_CYLINDER_RADIUS=1,
	BINDING_SPHERE_RADIUS,
	BINDING_CYLINDER_ORIGIN,
	BINDING_SPHERE_ORIGIN,
	BINDING_PLANE_ORIGIN,
	BINDING_CYLINDER_DIRECTION,
	BINDING_PLANE_DIRECTION,
	BINDING_RECT_TRANSLATE,
	BINDING_RECT_CORNER_MOVE,
};

//Possible primitive types (eg cylinder,sphere etc)
enum
{
	PRIMITIVE_SPHERE,
	PRIMITIVE_PLANE,
	PRIMITIVE_CYLINDER,
	PRIMITIVE_AAB,
};

//Possible transform modes (scaling, rotation etc)
enum
{
	TRANSFORM_TRANSLATE,
	TRANSFORM_SCALE,
	TRANSFORM_ROTATE,
	TRANSFORM_MODE_ENUM_END
};


enum {
	SPATIAL_ANALYSIS_DENSITY,
	SPATIAL_ANALYSIS_RDF, //Radial Distribution Function
	SPATIAL_ANALYSIS_ENUM_END,
};

enum{
	SPATIAL_DENSITY_NEIGHBOUR,
	SPATIAL_DENSITY_RADIUS,
	SPATIAL_DENSITY_ENUM_END
};


bool parseXMLColour(xmlNodePtr &nodePtr, float &r,float&g,float&b,float&a)
{
	xmlChar *xmlString;
	std::string tmpStr;
	//--red--
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"r");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(r,tmpStr))
		return false;

	//disallow negative or values gt 1.
	if(r < 0.0f || r > 1.0f)
		return false;

	//--green--
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"g");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(g,tmpStr))
		return false;

	//disallow negative or values gt 1.
	if(g < 0.0f || g > 1.0f)
		return false;

	//--blue--
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"b");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(b,tmpStr))
		return false;

	//disallow negative or values gt 1.
	if(b < 0.0f || b > 1.0f)
		return false;

	//--Alpha--
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"a");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(a,tmpStr))
		return false;

	//disallow negative or values gt 1.
	if(a < 0.0f || a > 1.0f)
		return false;

	return true;
}

size_t numElements(const vector<const FilterStreamData *> &v)
{
	size_t nE=0;
	for(unsigned int ui=0;ui<v.size();ui++)
		nE+=v[ui]->GetNumBasicObjects();

	return nE;
}

bool inSphere(const Point3D &testPt, const Point3D &origin, float sqrRadius)
{
	return testPt.sqrDist(origin)< sqrRadius;
}

bool inFrontPlane(const Point3D &testPt, const Point3D &origin, const Point3D &planeVec)
{
	return ((testPt-origin).dotProd(planeVec) > 0.0f);
}

unsigned int primitiveID(const std::string &str)
{
	if(str == "Sphere")
		return PRIMITIVE_SPHERE;
	if(str == "Plane")
		return PRIMITIVE_PLANE;
	if(str == "Cylinder")
		return PRIMITIVE_CYLINDER;
	if(str == "Aligned box")
		return PRIMITIVE_AAB;

	ASSERT(false);
	return (unsigned int)-1;
}

std::string primitiveStringFromID(unsigned int id)
{
	switch(id)
	{
		case PRIMITIVE_SPHERE:
			return string("Sphere");
		case PRIMITIVE_PLANE:
			return string("Plane");
		case PRIMITIVE_CYLINDER:
			return string("Cylinder");
	 	case PRIMITIVE_AAB:
			return string("Aligned box");
		default:
			ASSERT(false);
	}
	return string("");
}


const RangeFile *getRangeFile(const std::vector<const FilterStreamData*> &dataIn)
{
	for(size_t ui=0;ui<dataIn.size();ui++)
	{
		if(dataIn[ui]->getStreamType() == STREAM_TYPE_RANGE)
			return ((const RangeStreamData*)(dataIn[ui]))->rangeFile;
	}

	ASSERT(false);
}

unsigned int getIonstreamIonID(const IonStreamData *d, const RangeFile *r)
{
	if(!d->data.size())
		return (unsigned int)-1;

	unsigned int tentativeRange;

	tentativeRange=r->getIonID(d->data[0].getMassToCharge());


	//TODO: Currently, we have no choice but to brute force it.
	//In the future, it might be worth storing some data inside the IonStreamData itself
	//and to use that first, rather than try to brute force the result
#ifdef OPENMP
	bool spin=false;
	#pragma omp parallel for shared(spin)
	for(size_t ui=1;ui<d->data.size();ui++)
	{
		if(spin)
			continue
		if(r->getIonID(d->data[ui].getMassToCharge()) !=tentativeRange)
			spin=true;
	}

	//Not a range
	if(spin)
		return (unsigned int)-1;

#else
	for(size_t ui=1;ui<d->data.size();ui++)
	{
		if(r->getIonID(d->data[ui].getMassToCharge()) !=tentativeRange)
			return (unsigned int)-1;
	}
#endif

	return tentativeRange;	
}


//!Extend a point data vector using some ion data
unsigned int extendPointVector(std::vector<Point3D> &dest, const std::vector<IonHit> &vIonData,
				bool (*callback)(),unsigned int &progress, size_t offset)
{
	unsigned int curProg=NUM_CALLBACK;
	unsigned int n =offset;
#ifdef OPENMP
	//Parallel version
	bool spin=false;
	#pragma omp parallel for shared(spin)
	for(size_t ui=0;ui<vIonData.size();ui++)
	{
		if(spin)
			continue;
		dest[offset+ ui] = vIonData[ui].getPosRef();
		
		//update progress every CALLBACK entries
		if(!curProg--)
		{
			#pragma omp critical
			{
			n+=NUM_CALLBACK;
			progress= (unsigned int)(((float)n/(float)dest.size())*100.0f);
			if(!omp_get_thread_num())
			{
				if(!(*callback)())
					spin=true;
			}
			}
		}

	}

	if(spin)
		return 1;
#else

	for(size_t ui=0;ui<vIonData.size();ui++)
	{
		dest[offset+ ui] = vIonData[ui].getPosRef();
		
		//update progress every CALLBACK ions
		if(!curProg--)
		{
			n+=NUM_CALLBACK;
			progress= (unsigned int)(((float)n/(float)dest.size())*100.0f);
			if(!(*callback)())
				return 1;
		}

	}
#endif


	return 0;
}

void IonStreamData::clear()
{
	data.clear();
}

void VoxelStreamData::clear()
{
	data.clear();
}

void DrawStreamData::clear()
{
	for(unsigned int ui=0; ui<drawables.size(); ui++)
		delete drawables[ui];
}

DrawStreamData::~DrawStreamData()
{
	clear();
}

Filter::Filter()
{
	cacheOK=false;
	cache=true;
	for(unsigned int ui=0;ui<NUM_STREAM_TYPES;ui++)
		numStreamsLastRefresh[ui]=0;
}

Filter::~Filter()
{
    if(cacheOK)
	    clearCache();
}

void Filter::clearCache()
{
	using std::endl;
	cacheOK=false; 

	//Free mem held by objects	
	for(unsigned int ui=0;ui<filterOutputs.size(); ui++)
	{
		ASSERT(filterOutputs[ui]->cached);
		delete filterOutputs[ui];
	}

	filterOutputs.clear();	
}

bool Filter::haveCache() const
{
	return cacheOK;
}

void Filter::getSelectionDevices(vector<SelectionDevice *> &outD)
{
	outD.resize(devices.size());

	std::copy(devices.begin(),devices.end(),outD.begin());

#ifdef DEBUG
	for(unsigned int ui=0;ui<outD.size();ui++)
	{
		ASSERT(outD[ui]);
		//Ensure that pointers coming in are valid, by attempting to perform an operation on them
		vector<std::pair<const Filter *,SelectionBinding> > tmp;
		outD[ui]->getModifiedBindings(tmp);
		tmp.clear();
	}
#endif
}

void Filter::updateOutputInfo(const std::vector<const FilterStreamData *> &dataOut)
{
	//Reset the number ouf output streams to zero
	for(unsigned int ui=0;ui<NUM_STREAM_TYPES;ui++)
		numStreamsLastRefresh[ui]=0;
	
	//Count the different types of output stream.
	for(unsigned int ui=0;ui<dataOut.size();ui++)
	{
		ASSERT(getBitNum(dataOut[ui]->getStreamType()) < NUM_STREAM_TYPES);
		numStreamsLastRefresh[getBitNum(dataOut[ui]->getStreamType())]++;
	}
}

unsigned int Filter::getNumOutput(unsigned int streamType) const
{
	ASSERT(streamType < NUM_STREAM_TYPES);
	return numStreamsLastRefresh[streamType];
}

std::string Filter::getUserString() const
{
	if(userString.size())
		return userString;
	else
		return typeString();
}

void Filter::initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut)
{
	dataOut.resize(dataIn.size());
	std::copy(dataIn.begin(),dataIn.end(),dataOut.begin());
}


// == Pos load filter ==
PosLoadFilter::PosLoadFilter()
{
	cache=true;
	maxIons=MAX_IONS_LOAD_DEFAULT;
	fileType=FILE_TYPE_POS;
	//default ion colour is red.
	r=a=1.0f;
	g=b=0.0f;

	enabled=true;
	volumeRestrict=false;
	bound.setInverseLimits();
	//Ion size (rel. size)..
	ionSize=2.0;

	numColumns = 4;
	for (int i  = 0; i < numColumns; i++) {
		index[i] = i;
	}
}

Filter *PosLoadFilter::cloneUncached() const
{
	PosLoadFilter *p=new PosLoadFilter;
	p->ionFilename=ionFilename;
	p->maxIons=maxIons;
	p->ionSize=ionSize;
	p->r=r;	
	p->g=g;	
	p->b=b;	
	p->a=a;	
	p->bound.setBounds(bound);
	p->volumeRestrict=volumeRestrict;
	p->numColumns=numColumns;
	p->fileType=fileType;
	p->enabled=enabled;

	for(size_t ui=0;ui<INDEX_LENGTH;ui++)
		p->index[ui]=index[ui];

	//We are copying wether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->enabled=enabled;
	p->userString=userString;

	// this is for a pos file
	memcpy(p->index, index, sizeof(int) * 4);
	p->numColumns=numColumns;

	return p;
}

void PosLoadFilter::setFilename(const char *name)
{
	ionFilename = name;
	guessNumColumns();
}

void PosLoadFilter::setFilename(const std::string &name)
{
	ionFilename = name;
	guessNumColumns();
}

void PosLoadFilter::guessNumColumns()
{
	//Test the extention to determine what we will do
	string extension;
	if(ionFilename.size() > 4)
		extension = ionFilename.substr ( ionFilename.size() - 4, 4 );

	//Set extention to lowercase version
	for(size_t ui=0;ui<extension.size();ui++)
		extension[ui] = tolower(extension[ui]);

	if( extension == std::string(".pos")) {
		numColumns = 4;
		fileType = FILE_TYPE_POS;
		return;
	}
	fileType = FILE_TYPE_NULL;
	numColumns = 4;
}

//!Get (approx) number of bytes required for cache
size_t PosLoadFilter::numBytesForCache(size_t nObjects) const 
{
	//Otherwise we hvae to work it out, based upon the file
	size_t result;
	getFilesize(ionFilename.c_str(),result);

	if(maxIons)
		return std::min(maxIons*sizeof(IONHIT),result);


	return result;	
}

string PosLoadFilter::getValueLabel()
{
	return std::string("Mass-to-Charge (amu/e)");
}

unsigned int PosLoadFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(void))
{
	//use the cached copy if we have it.
	if(cacheOK)
	{
		ASSERT(filterOutputs.size());
		for(unsigned int ui=0;ui<dataIn.size();ui++)
			getOut.push_back(dataIn[ui]);

		for(unsigned int ui=0;ui<filterOutputs.size();ui++)
			getOut.push_back(filterOutputs[ui]);

		return 0;
	}

	if(!enabled)
	{
		for(unsigned int ui=0;ui<dataIn.size();ui++)
			getOut.push_back(dataIn[ui]);

		return 0;
	}

	IonStreamData *ionData = new IonStreamData;


	unsigned int uiErr;	
	if(maxIons)
	{
		//Load the pos file, limiting how much you pull from it
		if((uiErr = LimitLoadPosFile(numColumns, INDEX_LENGTH, index, ionData->data, ionFilename.c_str(),
							maxIons,progress.filterProgress,callback,strongRandom)))
		{
			consoleOutput.push_back(string("Error loading file: ") + ionFilename);
			delete ionData;
			return uiErr;
		}
	}
	else
	{
		//Load the pos file
		if((uiErr = GenericLoadFloatFile(numColumns, INDEX_LENGTH, index, ionData->data, ionFilename.c_str(),
							progress.filterProgress,callback)))
		{
			consoleOutput.push_back(string("Error loading file: ") + ionFilename);
			delete ionData;
			return uiErr;
		}
	}

	ionData->r = r;
	ionData->g = g;
	ionData->b = b;
	ionData->a = a;
	ionData->ionSize=ionSize;
	ionData->valueType=getValueLabel();
	
	random_shuffle(ionData->data.begin(),ionData->data.end());


	string s;
	stream_cast(s,ionData->data.size());
	consoleOutput.push_back( string("Loaded ") + s + " Ions" );
	if(cache)
	{
		ionData->cached=true;
		filterOutputs.push_back(ionData);
		cacheOK=true;
	}
	else
		ionData->cached=false;

	for(unsigned int ui=0;ui<dataIn.size();ui++)
		getOut.push_back(dataIn[ui]);

	//Append the ion data 
	getOut.push_back(ionData);

	return 0;
};

void PosLoadFilter::getProperties(FilterProperties &propertyList) const
{
	propertyList.data.clear();
	propertyList.types.clear();
	propertyList.keys.clear();

	vector<pair<string,string> > s;
	vector<unsigned int> type,keys;

	s.push_back(std::make_pair("File", ionFilename));
	type.push_back(PROPERTY_TYPE_STRING);
	keys.push_back(KEY_POSLOAD_FILE);
	
	string colStr;
	stream_cast(colStr,numColumns);
	s.push_back(std::make_pair("Number of columns", colStr));
	keys.push_back(KEY_POSLOAD_NUMBER_OF_COLUMNS);
	type.push_back(PROPERTY_TYPE_INTEGER);

	vector<pair<unsigned int,string> > choices;
	for (int i = 0; i < numColumns; i++) {
		string tmp;
		stream_cast(tmp,i);
		choices.push_back(make_pair(i,tmp));
	}
	
	colStr= choiceString(choices,index[0]);
	s.push_back(std::make_pair("x", colStr));
	keys.push_back(KEY_POSLOAD_SELECTED_COLUMN0);
	type.push_back(PROPERTY_TYPE_CHOICE);
	
	colStr= choiceString(choices,index[1]);
	s.push_back(std::make_pair("y", colStr));
	keys.push_back(KEY_POSLOAD_SELECTED_COLUMN1);
	type.push_back(PROPERTY_TYPE_CHOICE);
	
	colStr= choiceString(choices,index[2]);
	s.push_back(std::make_pair("z", colStr));
	keys.push_back(KEY_POSLOAD_SELECTED_COLUMN2);
	type.push_back(PROPERTY_TYPE_CHOICE);
	
	colStr= choiceString(choices,index[3]);
	s.push_back(std::make_pair("value", colStr));
	keys.push_back(KEY_POSLOAD_SELECTED_COLUMN3);
	type.push_back(PROPERTY_TYPE_CHOICE);
	
	string tmpStr;
	stream_cast(tmpStr,enabled);
	s.push_back(std::make_pair("Enabled", tmpStr));
	keys.push_back(KEY_POSLOAD_ENABLED);
	type.push_back(PROPERTY_TYPE_BOOL);

	if(enabled)
	{
		std::string tmpStr;
		stream_cast(tmpStr,maxIons*sizeof(IONHIT)/(1024*1024));
		s.push_back(std::make_pair("Load Limit (MB)",tmpStr));
		type.push_back(PROPERTY_TYPE_INTEGER);
		keys.push_back(KEY_POSLOAD_SIZE);
		
		string thisCol;
		//Convert the ion colour to a hex string	
		genColString((unsigned char)(r*255),(unsigned char)(g*255),
				(unsigned char)(b*255),(unsigned char)(a*255),thisCol);

		s.push_back(make_pair(string("Default colour "),thisCol)); 
		type.push_back(PROPERTY_TYPE_COLOUR);
		keys.push_back(KEY_POSLOAD_COLOUR);

		stream_cast(tmpStr,ionSize);
		s.push_back(make_pair(string("Draw Size"),tmpStr)); 
		type.push_back(PROPERTY_TYPE_REAL);
		keys.push_back(KEY_POSLOAD_IONSIZE);
	}

	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);
}

bool PosLoadFilter::setProperty( unsigned int set, unsigned int key, 
					const std::string &value, bool &needUpdate)
{
	
	needUpdate=false;
	switch(key)
	{
		case KEY_POSLOAD_FILE:
		{
			//ensure that the new file can be found
			//Try to open the file
			std::ifstream f(value.c_str());
			if(!f)
				return false;
			f.close();

			setFilename(value);
			//Invalidate the cache
			clearCache();

			needUpdate=true;
			break;
		}
		case KEY_POSLOAD_ENABLED:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			bool lastVal=enabled;
			if(stripped=="1")
				enabled=true;
			else
				enabled=false;

			//if the result is different, the
			//cache should be invalidated
			if(lastVal!=enabled)
				needUpdate=true;
			
			clearCache();
			break;
		}
		case KEY_POSLOAD_SIZE:
		{
			std::string tmp;
			size_t ltmp;
			if(stream_cast(ltmp,value))
				return false;

			if(ltmp*(1024*1024/sizeof(IONHIT)) != maxIons)
			{
				//Convert from MB to ions.			
				maxIons = ltmp*(1024*1024/sizeof(IONHIT));
				needUpdate=true;
				//Invalidate cache
				clearCache();
			}
			break;
		}
		case KEY_POSLOAD_COLOUR:
		{
			unsigned char newR,newG,newB,newA;

			parseColString(value,newR,newG,newB,newA);

			if(newB != b || newR != r ||
				newG !=g || newA != a)
			{
				r=newR/255.0;
				g=newG/255.0;
				b=newB/255.0;
				a=newA/255.0;


				//Check the cache, updating it if needed
				if(cacheOK)
				{
					for(unsigned int ui=0;ui<filterOutputs.size();ui++)
					{
						if(filterOutputs[ui]->getStreamType() == STREAM_TYPE_IONS)
						{
							IonStreamData *i;
							i=(IonStreamData *)filterOutputs[ui];
							i->r=r;
							i->g=g;
							i->b=b;
							i->a=a;
						}
					}

				}
				needUpdate=true;
			}


			break;
		}
		case KEY_POSLOAD_IONSIZE:
		{
			std::string tmp;
			float ltmp;
			if(stream_cast(ltmp,value))
				return false;

			if(ltmp < 0)
				return false;

			ionSize=ltmp;
			needUpdate=true;

			//Check the cache, updating it if needed
			if(cacheOK)
			{
				for(unsigned int ui=0;ui<filterOutputs.size();ui++)
				{
					if(filterOutputs[ui]->getStreamType() == STREAM_TYPE_IONS)
					{
						IonStreamData *i;
						i=(IonStreamData *)filterOutputs[ui];
						i->ionSize=ionSize;
					}
				}
			}
			needUpdate=true;

			break;
		}
		case KEY_POSLOAD_SELECTED_COLUMN0:
		{
			std::string tmp;
			int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp < 0 || ltmp >= numColumns)
				return false;
			
			index[0]=ltmp;
			needUpdate=true;
			clearCache();
			
			break;
		}
		case KEY_POSLOAD_SELECTED_COLUMN1:
		{
			std::string tmp;
			int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp < 0 || ltmp >= numColumns)
				return false;
			
			index[1]=ltmp;
			needUpdate=true;
			clearCache();
			
			break;
		}
		case KEY_POSLOAD_SELECTED_COLUMN2:
		{
			std::string tmp;
			int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp < 0 || ltmp >= numColumns)
				return false;
			
			index[2]=ltmp;
			needUpdate=true;
			clearCache();
			
			break;
		}
		case KEY_POSLOAD_SELECTED_COLUMN3:
		{
			std::string tmp;
			unsigned int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp < 0 || ltmp >= numColumns)
				return false;
			
			index[3]=ltmp;
			needUpdate=true;
			clearCache();
			
			break;
		}
		case KEY_POSLOAD_NUMBER_OF_COLUMNS:
		{
			std::string tmp;
			int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp <=0 || ltmp >= MAX_NUM_FILE_COLS)
				return false;
			
			numColumns=ltmp;
			for (int i = 0; i < INDEX_LENGTH; i++) {
				index[i] = (index[i] < numColumns? index[i]: numColumns - 1);
			}
			needUpdate=true;
			clearCache();
			
			break;
		}
		default:
			ASSERT(false);
			break;
	}
	return true;
}

bool PosLoadFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
	//Retrieve user string
	//===
	if(XMLHelpFwdToElem(nodePtr,"userstring"))
		return false;

	xmlChar *xmlString;
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	userString=(char *)xmlString;
	xmlFree(xmlString);
	//===

	//Retrieve file name	
	if(XMLHelpFwdToElem(nodePtr,"file"))
		return false;
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"name");
	if(!xmlString)
		return false;
	ionFilename=(char *)xmlString;
	xmlFree(xmlString);

	//Override the string, as needed
	if( (stateFileDir.size()) &&
		(ionFilename.size() > 2 && ionFilename.substr(0,2) == "./") )
	{
		ionFilename=stateFileDir + ionFilename.substr(2);
	}

	//Filenames need to be converted from unix format (which I make canonical on disk) into native format 
	ionFilename=convertFileStringToNative(ionFilename);

	//Retrieve number of columns	
	if(!XMLGetNextElemAttrib(nodePtr,numColumns,"columns","value"))
		return false;
	if(numColumns <=0 || numColumns >= MAX_NUM_FILE_COLS)
		return false;
	
	//Retrieve index	
	if(XMLHelpFwdToElem(nodePtr,"xyzm"))
		return false;
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"values");
	if(!xmlString)
		return false;
	std::vector<string> v;
	splitStrsRef((char *)xmlString,',',v);
	for (int i = 0; i < INDEX_LENGTH && i < v.size(); i++)
	{
		if(stream_cast(index[i],v[i]))
			return false;

		if(index[i] >=numColumns)
			return false;
	}
	xmlFree(xmlString);
	
	//Retreive enabled/disabled
	//--
	unsigned int tmpVal;
	if(!XMLGetNextElemAttrib(nodePtr,tmpVal,"enabled","value"))
		return false;
	enabled=tmpVal;
	//--

	//Get max Ions
	//--
	if(!XMLGetNextElemAttrib(nodePtr,maxIons,"maxions","value"))
		return false;
	//--
	
	std::string tmpStr;
	//Retrieve colour
	//====
	if(XMLHelpFwdToElem(nodePtr,"colour"))
		return false;

	if(!parseXMLColour(nodePtr,r,g,b,a))
		return false;	
	//====

	//Retrieve drawing size value
	//--
	if(!XMLGetNextElemAttrib(nodePtr,ionSize,"ionsize","value"))
		return false;
	//check positive or zero
	if(ionSize <=0)
		return false;
	//--


	return true;
}

std::string  PosLoadFilter::getErrString(unsigned int code) const
{
	ASSERT(code < POS_ERR_FINAL);
	return posErrStrings[code];
}

bool PosLoadFilter::writeState(std::ofstream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<posload>" << endl;

			f << tabs(depth+1) << "<userstring value=\""<<userString << "\"/>"  << endl;
			f << tabs(depth+1) << "<file name=\"" << convertFileStringToCanonical(ionFilename) << "\"/>" << endl;
			f << tabs(depth+1) << "<columns value=\"" << numColumns << "\"/>" << endl;
			f << tabs(depth+1) << "<xyzm values=\"" << index[0] << "," << index[1] << "," << index[2] << "," << index[3] << "\"/>" << endl;
			f << tabs(depth+1) << "<enabled value=\"" << enabled<< "\"/>" << endl;
			f << tabs(depth+1) << "<maxions value=\"" << maxIons << "\"/>" << endl;

			f << tabs(depth+1) << "<colour r=\"" <<  r<< "\" g=\"" << g << "\" b=\"" <<b
				<< "\" a=\"" << a << "\"/>" <<endl;
			f << tabs(depth+1) << "<ionsize value=\"" << ionSize << "\"/>" << endl;
			f << tabs(depth) << "</posload>" << endl;
			break;
		}
		default:
			//Shouldn't get here, unhandled format string 
			ASSERT(false);
			return false;
	}

	return true;

}

bool PosLoadFilter::writePackageState(std::ofstream &f, unsigned int format,
			const std::vector<std::string> &valueOverrides, unsigned int depth) const
{
	ASSERT(valueOverrides.size() == 1);

	//Temporarily modify the state of the filter, then call writestate
	string tmpIonFilename=ionFilename;


	//override const -- naughty, but we know what we are doing...
	const_cast<PosLoadFilter*>(this)->ionFilename=valueOverrides[0];
	bool result;
	result=writeState(f,format,depth);

	const_cast<PosLoadFilter*>(this)->ionFilename=tmpIonFilename;

	return result;
}

void PosLoadFilter::getStateOverrides(std::vector<string> &externalAttribs) const 
{
	externalAttribs.push_back(ionFilename);
}


// == Ion Downsampling filter ==

IonDownsampleFilter::IonDownsampleFilter()
{
	rng.initTimer();
	fixedNumOut=true;
	fraction=0.1f;
	maxAfterFilter=5000;

	cacheOK=false;
	cache=true; //By default, we should cache, but decision is made higher up

}

Filter *IonDownsampleFilter::cloneUncached() const
{
	IonDownsampleFilter *p=new IonDownsampleFilter();
	p->rng = rng;
	p->maxAfterFilter=maxAfterFilter;
	p->fraction=fraction;
	//We are copying wether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	p->fixedNumOut=fixedNumOut;
	return p;
}

size_t IonDownsampleFilter::numBytesForCache(size_t nObjects) const
{
	if(fixedNumOut)
	{
		if(nObjects > maxAfterFilter)
			return maxAfterFilter*sizeof(IONHIT);
		else
			return nObjects*sizeof(IONHIT);
	}
	else
	{
		return (size_t)((float)(nObjects*sizeof(IONHIT))*fraction);
	}
}

unsigned int IonDownsampleFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(void))
{
	//use the cached copy if we have it.
	if(cacheOK)
	{
		for(size_t ui=0;ui<dataIn.size();ui++)
		{
			if(dataIn[ui]->getStreamType() != STREAM_TYPE_IONS)
				getOut.push_back(dataIn[ui]);
		}
		for(size_t ui=0;ui<filterOutputs.size();ui++)
			getOut.push_back(filterOutputs[ui]);
		return 0;
	}



	size_t numIons=0;
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		if(dataIn[ui]->getStreamType() == STREAM_TYPE_IONS)
				numIons+=((const IonStreamData*)dataIn[ui])->data.size();
	}
	
	
	size_t totalSize = numElements(dataIn);
	for(size_t ui=0;ui<dataIn.size() ;ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS: 
			{
				if(!numIons)
					continue;

				IonStreamData *d;
				d=new IonStreamData;
				try
				{
					if(fixedNumOut)
					{
						float frac;
						frac = (float)(((const IonStreamData*)dataIn[ui])->data.size())/(float)numIons;

						randomSelect(d->data,((const IonStreamData *)dataIn[ui])->data,
									rng,maxAfterFilter*frac,progress.filterProgress,callback,strongRandom);


					}
					else
					{

						unsigned int curProg=NUM_CALLBACK;
						size_t n=0;

						//highly unlikely with even modest numbers of ions
						//that this will not be exceeeded
						d->data.reserve(fraction/1.1*totalSize);

						ASSERT(dataIn[ui]->getStreamType() == STREAM_TYPE_IONS);

						for(vector<IonHit>::const_iterator it=((const IonStreamData *)dataIn[ui])->data.begin();
							       it!=((const IonStreamData *)dataIn[ui])->data.end(); ++it)
						{
							if(rng.genUniformDev() <  fraction)
								d->data.push_back(*it);
						
							//update progress every CALLBACK ions
							if(!curProg--)
							{
								n+=NUM_CALLBACK;
								progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
								if(!(*callback)())
								{
									delete d;
									return IONDOWNSAMPLE_ABORT_ERR;
								}
							}
						}
					}
				}
				catch(std::bad_alloc)
				{
					delete d;
					return IONDOWNSAMPLE_BAD_ALLOC;
				}


				//Copy over other attributes
				d->r = ((IonStreamData *)dataIn[ui])->r;
				d->g = ((IonStreamData *)dataIn[ui])->g;
				d->b =((IonStreamData *)dataIn[ui])->b;
				d->a =((IonStreamData *)dataIn[ui])->a;
				d->ionSize =((IonStreamData *)dataIn[ui])->ionSize;
				d->representationType=((IonStreamData *)dataIn[ui])->representationType;
				d->valueType=((IonStreamData *)dataIn[ui])->valueType;

				//getOut is const, so shouldn't be modified
				if(cache)
				{
					d->cached=true;
					filterOutputs.push_back(d);
					cacheOK=true;
				}
				else
					d->cached=false;
		

				getOut.push_back(d);
				break;
			}
		
			default:
				getOut.push_back(dataIn[ui]);
				break;
		}

	}
	

	return 0;
}


void IonDownsampleFilter::getProperties(FilterProperties &propertyList) const
{
	propertyList.data.clear();
	propertyList.keys.clear();
	propertyList.types.clear();

	vector<unsigned int> type,keys;
	vector<pair<string,string> > s;

	string tmpStr;
	stream_cast(tmpStr,fixedNumOut);
	s.push_back(std::make_pair("By Count", tmpStr));
	keys.push_back(KEY_IONDOWNSAMPLE_FIXEDOUT);
	type.push_back(PROPERTY_TYPE_BOOL);

	if(fixedNumOut)
	{
		stream_cast(tmpStr,maxAfterFilter);
		keys.push_back(KEY_IONDOWNSAMPLE_COUNT);
		s.push_back(make_pair("Output Count", tmpStr));
		type.push_back(PROPERTY_TYPE_INTEGER);
	}
	else
	{
		stream_cast(tmpStr,fraction);
		s.push_back(make_pair("Out Fraction", tmpStr));
		keys.push_back(KEY_IONDOWNSAMPLE_FRACTION);
		type.push_back(PROPERTY_TYPE_REAL);
	}

	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);
}

bool IonDownsampleFilter::setProperty( unsigned int set, unsigned int key,
					const std::string &value, bool &needUpdate)
{
	ASSERT(!set);

	needUpdate=false;
	switch(key)
	{
		case KEY_IONDOWNSAMPLE_FIXEDOUT: 
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			bool lastVal=fixedNumOut;
			if(stripped=="1")
				fixedNumOut=true;
			else
				fixedNumOut=false;

			//if the result is different, the
			//cache should be invalidated
			if(lastVal!=fixedNumOut)
			{
				needUpdate=true;
				clearCache();
			}

			break;
		}	
		case KEY_IONDOWNSAMPLE_FRACTION:
		{
			float newFraction;
			if(stream_cast(newFraction,value))
				return false;

			if(newFraction < 0.0f || newFraction > 1.0f)
				return false;

			//In the case of fixed number output, 
			//our cache is invalidated
			if(!fixedNumOut)
			{
				needUpdate=true;
				clearCache();
			}

			fraction=newFraction;
			

			break;
		}
		case KEY_IONDOWNSAMPLE_COUNT:
		{
			size_t count;

			if(stream_cast(count,value))
				return false;

			maxAfterFilter=count;
			//In the case of fixed number output, 
			//our cache is invalidated
			if(fixedNumOut)
			{
				needUpdate=true;
				clearCache();
			}
			
			break;
		}	
		default:
			ASSERT(false);
	}	
	return true;
}


std::string  IonDownsampleFilter::getErrString(unsigned int code) const
{
	switch(code)
	{
		case IONDOWNSAMPLE_ABORT_ERR:
			return std::string("Downsample Aborted");
		case IONDOWNSAMPLE_BAD_ALLOC:
			return std::string("Insuffient memory for downsample");
	}	

	return std::string("BUG! Should not see this (IonDownsample)");
}

bool IonDownsampleFilter::writeState(std::ofstream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<iondownsample>" << endl;
			f << tabs(depth+1) << "<userstring value=\""<<userString << "\"/>"  << endl;

			f << tabs(depth+1) << "<fixednumout value=\""<<fixedNumOut<< "\"/>"  << endl;
			f << tabs(depth+1) << "<fraction value=\""<<fraction<< "\"/>"  << endl;
			f << tabs(depth+1) << "<maxafterfilter value=\"" << maxAfterFilter << "\"/>" << endl;
			f << tabs(depth) << "</iondownsample>" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool IonDownsampleFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
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

	//Retrieve number out (yes/no) mode
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"fixednumout","value"))
		return false;
	
	if(tmpStr == "1") 
		fixedNumOut=true;
	else if(tmpStr== "0")
		fixedNumOut=false;
	else
		return false;
	//===
		
	//Retrieve Fraction
	//===
	if(!XMLGetNextElemAttrib(nodePtr,fraction,"fraction","value"))
		return false;
	//disallow negative or values gt 1.
	if(fraction < 0.0f || fraction > 1.0f)
		return false;
	//===
	
	
	//Retrieve count "maxafterfilter"
	if(!XMLGetNextElemAttrib(nodePtr,maxAfterFilter,"maxafterfilter","value"))
		return false;
	
	return true;
}


// == Voxels filter ==
VoxeliseFilter::VoxeliseFilter() 
: fixedWidth(false), bc(), normaliseType(VOXEL_NORMALISETYPE_NONE)
{
	splatSize=1.0f;
	a=0.9f;
	r=g=b=0.5;
	isoLevel=0.5;
	bc.setBounds(0, 0, 0, 1, 1, 1);
	representation=VOXEL_REPRESENT_POINTCLOUD;
	for (int i = 0; i < INDEX_LENGTH; i++) {
		nBins[i] = 50;
	}
	calculateWidthsFromNumBins(binWidth, nBins);
	numeratorAll = false;
	denominatorAll = true;

	rsdIncoming=0;
}


Filter *VoxeliseFilter::cloneUncached() const
{
	VoxeliseFilter *p=new VoxeliseFilter();
	p->splatSize=splatSize;
	p->a=a;
	p->r=r;
	p->g=g;
	p->b=b;
	
	p->isoLevel=isoLevel;
	p->representation=representation;
	p->splatSize=splatSize;

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

	return p;
}

//TODO
size_t VoxeliseFilter::numBytesForCache(size_t nObjects) const
{
	return 0;
}

void VoxeliseFilter::initFilter(const std::vector<const FilterStreamData *> &dataIn,
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

// TODO: create plotstream
unsigned int VoxeliseFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
										  std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(void))
{	

	Point3D minP,maxP;

	bc.setInverseLimits();
		
	for (size_t i = 0; i < dataIn.size(); i++) 
	{
		//Check for ion stream types. Block others from propagation.
		if (dataIn[i]->getStreamType() != STREAM_TYPE_IONS) continue;

		const IonStreamData *is = (const IonStreamData *)dataIn[i];
		//Don't work on empty or single object streams (bounding box needs to be defined)
		if (is->GetNumBasicObjects() < 2) continue;
	
		BoundCube bcTmp;
		bcTmp=getIonDataLimits(is->data);

		//Bounds could be invalid if, for example, we had coplanar axis aligned points
		if (!bcTmp.isValid()) continue;

		bc.expand(bcTmp);
	}
	//No bounding box? Tough cookies
	if (!bc.isValid() || bc.isFlat()) return VOXEL_BOUNDS_INVALID_ERR;

	bc.getBounds(minP,maxP);	
	if (fixedWidth) 
		calculateNumBinsFromWidths(binWidth, nBins);
	else
		calculateWidthsFromNumBins(binWidth, nBins);
	
	//Disallow empty bounding boxes (ie, produce no output)
	if(minP == maxP)
		return 0;
		
	VoxelStreamData *vs = new VoxelStreamData();
	vs->cached = false;
	vs->data.setCallbackMethod(callback);
	vs->data.init(nBins[0], nBins[1], nBins[2], bc);
	vs->representationType= representation;
	vs->splatSize = splatSize;
	vs->isoLevel=isoLevel;
	vs->data.fill(0);
	vs->r=r;
	vs->g=g;
	vs->b=b;
	vs->a=a;

	VoxelStreamData *vsDenom = NULL;
	if (normaliseType == VOXEL_NORMALISETYPE_COUNT2INVOXEL ||
		normaliseType == VOXEL_NORMALISETYPE_ALLATOMSINVOXEL) {
		//Check we actually have incoming data
		ASSERT(rsdIncoming);
		vsDenom = new VoxelStreamData();
		vsDenom->cached = false;
		vsDenom->data.setCallbackMethod(callback);
		vsDenom->data.init(nBins[0], nBins[1], nBins[2], bc);
		vsDenom->representationType= representation;
		vsDenom->splatSize = splatSize;
		vsDenom->isoLevel=isoLevel;
		vsDenom->data.fill(0);
		vsDenom->a=a;
	}

	const IonStreamData *is;
	if(rsdIncoming)
	{

		for (size_t i = 0; i < dataIn.size(); i++) 
		{
			
			//Check for ion stream types. Don't use anything else in counting
			if (dataIn[i]->getStreamType() != STREAM_TYPE_IONS) continue;
			
			is= (const IonStreamData *)dataIn[i];

			
			//Count the numerator ions	
			if(is->data.size())
			{
				//Check what Ion type this stream belongs to. Assume all ions
				//in the stream belong to the same group
				unsigned int ionID;
				ionID = getIonstreamIonID(is,rsdIncoming->rangeFile);

				bool thisIonEnabled;
				if(ionID!=(unsigned int)-1)
					thisIonEnabled=enabledIons[0][ionID];
				else
					thisIonEnabled=false;

				if(thisIonEnabled)
					vs->data.countPoints(is->data,true,false);
			}
		
			//If the user requests normalisation, compute the denominator datset
			if (normaliseType == VOXEL_NORMALISETYPE_COUNT2INVOXEL) {
				if(is->data.size())
				{
					//Check what Ion type this stream belongs to. Assume all ions
					//in the stream belong to the same group
					unsigned int ionID;
					ionID = rsdIncoming->rangeFile->getIonID(is->data[0].getMassToCharge());

					bool thisIonEnabled;
					if(ionID!=(unsigned int)-1)
						thisIonEnabled=enabledIons[1][ionID];
					else
						thisIonEnabled=false;

					if(thisIonEnabled)
						vsDenom->data.countPoints(is->data,true,false);
				}
			} else if (normaliseType == VOXEL_NORMALISETYPE_ALLATOMSINVOXEL)
				vsDenom->data.countPoints(is->data,true,false);

			if(!(*callback)())
			{
				delete vs;
				return VOXEL_ABORT_ERR;
			}
		}
	
		//Perform normalsiation	
		if (normaliseType == VOXEL_NORMALISETYPE_VOLUME)
			vs->data.calculateDensity();
		else if (normaliseType == VOXEL_NORMALISETYPE_COUNT2INVOXEL ||
				 normaliseType == VOXEL_NORMALISETYPE_ALLATOMSINVOXEL)
			vs->data /= vsDenom->data;
	}
	else
	{
		//No range data.  Just count
		for (size_t i = 0; i < dataIn.size(); i++) 
		{
			
			is= (const IonStreamData *)dataIn[i];
			vs->data.countPoints(is->data,true,false);
			
			if(!(*callback)())
			{
				delete vs;
				return VOXEL_ABORT_ERR;
			}
		}
		ASSERT(normaliseType != VOXEL_NORMALISETYPE_COUNT2INVOXEL
				&& normaliseType!=VOXEL_NORMALISETYPE_ALLATOMSINVOXEL);
		if (normaliseType == VOXEL_NORMALISETYPE_VOLUME)
			vs->data.calculateDensity();
	}	
	delete vsDenom;
	
	getOut.push_back(vs);

	return 0;
}

std::string VoxeliseFilter::getNormaliseTypeString(int type) const {
	switch (type) {
		case VOXEL_NORMALISETYPE_NONE:
			return std::string("None (Raw count)");
			break;
		case VOXEL_NORMALISETYPE_VOLUME:
			return std::string("Volume (Density)");
			break;
		case VOXEL_NORMALISETYPE_COUNT2INVOXEL:
			return std::string("Ratio (Num/Denom)");
			break;
		case VOXEL_NORMALISETYPE_ALLATOMSINVOXEL:
			return std::string("All Ions (conc)");
			break;
		default:
			return "";
			break;
	}
}

std::string VoxeliseFilter::getRepresentTypeString(int type) const {
	switch (type) {
		case VOXEL_REPRESENT_POINTCLOUD:
			return std::string("Point Cloud");
			break;
		case VOXEL_REPRESENT_ISOSURF:
			return std::string("Isosurface");
			break;
		default:
			return "";
			break;
	}
}

void VoxeliseFilter::getProperties(FilterProperties &propertyList) const
{
	propertyList.data.clear();
	propertyList.keys.clear();
	propertyList.types.clear();
	
	vector<unsigned int> type,keys;
	vector<pair<string,string> > s;

	string tmpStr;
	stream_cast(tmpStr, fixedWidth);
	s.push_back(std::make_pair("Fixed width", tmpStr));
	keys.push_back(KEY_VOXEL_FIXEDWIDTH);
	type.push_back(PROPERTY_TYPE_BOOL);
	
	if(fixedWidth)
	{
		stream_cast(tmpStr,binWidth[0]);
		keys.push_back(KEY_VOXEL_WIDTHBINSX);
		s.push_back(make_pair("Bin width x", tmpStr));
		type.push_back(PROPERTY_TYPE_REAL);

		stream_cast(tmpStr,binWidth[1]);
		keys.push_back(KEY_VOXEL_WIDTHBINSY);
		s.push_back(make_pair("Bin width y", tmpStr));
		type.push_back(PROPERTY_TYPE_REAL);

		stream_cast(tmpStr,binWidth[2]);
		keys.push_back(KEY_VOXEL_WIDTHBINSZ);
		s.push_back(make_pair("Bin width z", tmpStr));
		type.push_back(PROPERTY_TYPE_REAL);
	}
	else
	{
		stream_cast(tmpStr,nBins[0]);
		keys.push_back(KEY_VOXEL_NBINSX);
		s.push_back(make_pair("Num bins x", tmpStr));
		type.push_back(PROPERTY_TYPE_INTEGER);
		
		stream_cast(tmpStr,nBins[1]);
		keys.push_back(KEY_VOXEL_NBINSY);
		s.push_back(make_pair("Num bins y", tmpStr));
		type.push_back(PROPERTY_TYPE_INTEGER);
		
		stream_cast(tmpStr,nBins[2]);
		keys.push_back(KEY_VOXEL_NBINSZ);
		s.push_back(make_pair("Num bins z", tmpStr));
		type.push_back(PROPERTY_TYPE_INTEGER);
	}

	//Let the user know what the valid values for voxel value types are
	string tmpChoice;	
	vector<pair<unsigned int,string> > choices;
	tmpStr=getNormaliseTypeString(VOXEL_NORMALISETYPE_NONE);
	choices.push_back(make_pair((unsigned int)VOXEL_NORMALISETYPE_NONE,tmpStr));
	tmpStr=getNormaliseTypeString(VOXEL_NORMALISETYPE_VOLUME);
	choices.push_back(make_pair((unsigned int)VOXEL_NORMALISETYPE_VOLUME,tmpStr));
	if(rsdIncoming)
	{
		//Concentration mode
		tmpStr=getNormaliseTypeString(VOXEL_NORMALISETYPE_ALLATOMSINVOXEL);
		choices.push_back(make_pair((unsigned int)VOXEL_NORMALISETYPE_ALLATOMSINVOXEL,tmpStr));
		//Ratio is only valid if we have a way of seperation for the ions i.e. range
		tmpStr=getNormaliseTypeString(VOXEL_NORMALISETYPE_COUNT2INVOXEL);
		choices.push_back(make_pair((unsigned int)VOXEL_NORMALISETYPE_COUNT2INVOXEL,tmpStr));
	}
	tmpStr= choiceString(choices,normaliseType);
	s.push_back(make_pair(string("Normalise by"),tmpStr));
	type.push_back(PROPERTY_TYPE_CHOICE);
	keys.push_back(KEY_VOXEL_NORMALISE_TYPE);
	
	
	//TODO
	//1. range file
	//2. threshold
	//3. gaussian
	
	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);

	s.clear();
	type.clear();
	keys.clear();

		
	// numerator
	if (rsdIncoming) {
		s.push_back(make_pair("Numerator", numeratorAll ? "1" : "0"));
		type.push_back(PROPERTY_TYPE_BOOL);
		keys.push_back(KEY_VOXEL_ENABLE_NUMERATOR);

		ASSERT(rsdIncoming->enabledIons.size()==enabledIons[0].size());	
		ASSERT(rsdIncoming->enabledIons.size()==enabledIons[1].size());	

		//Look at the numerator	
		for(unsigned  int ui=0; ui<rsdIncoming->enabledIons.size(); ui++)
		{
			string str;
			if(enabledIons[0][ui])
				str="1";
			else
				str="0";

			//Append the ion name with a checkbox
			s.push_back(make_pair(
				rsdIncoming->rangeFile->getName(ui), str));
			type.push_back(PROPERTY_TYPE_BOOL);
			keys.push_back(KEY_VOXEL_ENABLE_NUMERATOR*1000+ui);
		}
		propertyList.types.push_back(type);
		propertyList.data.push_back(s);
		propertyList.keys.push_back(keys);
	}
	
	s.clear();
	type.clear();
	keys.clear();
	
	if (normaliseType == VOXEL_NORMALISETYPE_COUNT2INVOXEL && rsdIncoming) {
		// denominator
		s.push_back(make_pair("Denominator", denominatorAll ? "1" : "0"));
		type.push_back(PROPERTY_TYPE_BOOL);
		keys.push_back(KEY_VOXEL_ENABLE_DENOMINATOR);

		for(unsigned  int ui=0; ui<rsdIncoming->enabledIons.size(); ui++)
		{			
			string str;
			if(enabledIons[1][ui])
				str="1";
			else
				str="0";

			//Append the ion name with a checkbox
			s.push_back(make_pair(
				rsdIncoming->rangeFile->getName(ui), str));

			type.push_back(PROPERTY_TYPE_BOOL);
			keys.push_back(KEY_VOXEL_ENABLE_DENOMINATOR*1000+ui);
		}
		propertyList.types.push_back(type);
		propertyList.data.push_back(s);
		propertyList.keys.push_back(keys);

		s.clear();
		type.clear();
		keys.clear();
	}
	
	//start a new set for the visual representation
	//----------------------------
	choices.clear();
	tmpStr=getRepresentTypeString(VOXEL_REPRESENT_POINTCLOUD);
	choices.push_back(make_pair((unsigned int)VOXEL_REPRESENT_POINTCLOUD,tmpStr));
	tmpStr=getRepresentTypeString(VOXEL_REPRESENT_ISOSURF);
	choices.push_back(make_pair((unsigned int)VOXEL_REPRESENT_ISOSURF,tmpStr));
	
	
	tmpStr= choiceString(choices,representation);
	s.push_back(make_pair(string("Representation"),tmpStr));
	type.push_back(PROPERTY_TYPE_CHOICE);
	keys.push_back(KEY_VOXEL_REPRESENTATION_MODE);
	switch(representation)
	{
		case VOXEL_REPRESENT_POINTCLOUD:
		{
			stream_cast(tmpStr,splatSize);
			s.push_back(make_pair("Spot size",tmpStr));
			type.push_back(PROPERTY_TYPE_REAL);
			keys.push_back(KEY_VOXEL_SPOTSIZE);

			stream_cast(tmpStr,1.0-a);
			s.push_back(make_pair("Transparency",tmpStr));
			type.push_back(PROPERTY_TYPE_REAL);
			keys.push_back(KEY_VOXEL_TRANSPARANCY);
			break;
		}
		case VOXEL_REPRESENT_ISOSURF:
		{
			stream_cast(tmpStr,isoLevel);
			s.push_back(make_pair("Isovalue",tmpStr));
			type.push_back(PROPERTY_TYPE_REAL);
			keys.push_back(KEY_VOXEL_ISOLEVEL);
		
				

			//Convert the ion colour to a hex string	
			genColString((unsigned char)(r*255),(unsigned char)(g*255),
					(unsigned char)(b*255),(unsigned char)(a*255),tmpStr);
			s.push_back(make_pair("Colour",tmpStr));
			type.push_back(PROPERTY_TYPE_COLOUR);
			keys.push_back(KEY_VOXEL_COLOUR);
			
			break;
		}
		default:
			ASSERT(false);
			;
	}
	
	//----------------------------
	
	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);
}

bool VoxeliseFilter::setProperty( unsigned int set, unsigned int key,
									  const std::string &value, bool &needUpdate)
{
	
	needUpdate=false;
	switch(key)
	{
		case KEY_VOXEL_FIXEDWIDTH: 
		{
			bool b;
			if(stream_cast(b,value))
				return false;
			fixedWidth=b;
			needUpdate=true;			
			break;
		}	
		case KEY_VOXEL_NBINSX:
		{
			int i;
			if(stream_cast(i,value))
				return false;
			if(i <= 0)
				return false;
			needUpdate=true;
			nBins[0]=i;
			calculateWidthsFromNumBins(binWidth, nBins);
			break;
		}
		case KEY_VOXEL_NBINSY:
		{
			int i;
			if(stream_cast(i,value))
				return false;
			if(i <= 0)
				return false;
			needUpdate=true;
			nBins[1]=i;
			calculateWidthsFromNumBins(binWidth, nBins);
			break;
		}
		case KEY_VOXEL_NBINSZ:
		{
			int i;
			if(stream_cast(i,value))
				return false;
			if(i <= 0)
				return false;
			needUpdate=true;
			nBins[2]=i;
			calculateWidthsFromNumBins(binWidth, nBins);
			break;
		}
		case KEY_VOXEL_WIDTHBINSX:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
			needUpdate=true;
			binWidth[0]=f;
			calculateNumBinsFromWidths(binWidth, nBins);
			break;
		}
		case KEY_VOXEL_WIDTHBINSY:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
			needUpdate=true;
			binWidth[1]=f;
			calculateNumBinsFromWidths(binWidth, nBins);
			break;
		}
		case KEY_VOXEL_WIDTHBINSZ:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
			needUpdate=true;
			binWidth[2]=f;
			calculateNumBinsFromWidths(binWidth, nBins);
			break;
		}
		case KEY_VOXEL_NORMALISE_TYPE:
		{
			int i;
			for (i = 0; i < VOXEL_NORMALISETYPE_MAX; i++)
				if (value == getNormaliseTypeString(i)) break;
			if (i == VOXEL_NORMALISETYPE_MAX)
				return false;
			if(normaliseType!=i)
				needUpdate=true;
			normaliseType=i;
			break;
		}
		case KEY_VOXEL_SPOTSIZE:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
			needUpdate=true;
			splatSize=f;
			break;
		}
		case KEY_VOXEL_TRANSPARANCY:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f || f > 1.0)
				return false;
			needUpdate=true;
			//Alpha is opacity, which is 1-transparancy
			a=1.0f-f;
			break;
		}
		case KEY_VOXEL_ISOLEVEL:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
			needUpdate=true;
			isoLevel=f;
			break;
		}
		case KEY_VOXEL_COLOUR:
		{
			unsigned char newR,newG,newB,newA;

			parseColString(value,newR,newG,newB,newA);

			if(newB != b || newR != r ||
				newG !=g || newA != a)
				needUpdate=true;
			r=newR/255.0;
			g=newG/255.0;
			b=newB/255.0;
			break;
		}
		case KEY_VOXEL_REPRESENTATION_MODE:
		{
			int i;
			for (i = 0; i < VOXEL_REPRESENT_END; i++)
				if (value == getRepresentTypeString(i)) break;
			if (i == VOXEL_REPRESENT_END)
				return false;
			needUpdate=true;
			representation=i;
			break;
		}
		case KEY_VOXEL_ENABLE_NUMERATOR:
		{
			bool b;
			if(stream_cast(b,value))
				return false;
			//Set them all to enabled or disabled as a group	
			for (size_t i = 0; i < enabledIons[0].size(); i++) 
				enabledIons[0][i] = b;
			numeratorAll = b;
			needUpdate=true;			
			break;
		}
		case KEY_VOXEL_ENABLE_DENOMINATOR:
		{
			bool b;
			if(stream_cast(b,value))
				return false;
	
			//Set them all to enabled or disabled as a group	
			for (size_t i = 0; i < enabledIons[1].size(); i++) 
				enabledIons[1][i] = b;
			
			denominatorAll = b;
			needUpdate=true;			
			break;
		}
		default:
		{
			if (key >= KEY_VOXEL_ENABLE_DENOMINATOR*1000) {
				bool b;
				if(stream_cast(b,value))
					return false;
//				if (b && !rsdIncoming->enabledIons[key - KEY_VOXEL_ENABLE_DENOMINATOR*1000]) {
//					return false;
//				}
				enabledIons[1][key - KEY_VOXEL_ENABLE_DENOMINATOR*1000]=b;
				if (!b) {
					denominatorAll = false;
				}
				needUpdate=true;			
			} else if (key >= KEY_VOXEL_ENABLE_NUMERATOR*1000) {
				bool b;
				if(stream_cast(b,value))
					return false;
//				if (b && !rsdIncoming->enabledIons[key - KEY_VOXEL_ENABLE_NUMERATOR*1000]) {
//					return false;
//				}
				enabledIons[0][key - KEY_VOXEL_ENABLE_NUMERATOR*1000]=b;
				if (!b) {
					numeratorAll = false;
				}
				needUpdate=true;			
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

std::string  VoxeliseFilter::getErrString(unsigned int code) const
{
	switch(code)
	{
		case VOXEL_ABORT_ERR:
		{
			return std::string("Voxelisation aborted");
		}
		case VOXEL_BOUNDS_INVALID_ERR:
		{
			return std::string("Voxelisation bounds are invalid");
		}
	}	
	
	return std::string("BUG! Should not see this (VoxeliseFilter)");
}

bool VoxeliseFilter::writeState(std::ofstream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<voxelise>" << endl;
			f << tabs(depth+1) << "<userstring value=\"" << userString << "\"/>" << endl;
			f << tabs(depth+1) << "<fixedwidth value=\""<<fixedWidth << "\"/>"  << endl;
			f << tabs(depth+1) << "<nbins values=\""<<nBins[0] << ","<<nBins[1]<<","<<nBins[2] << "\"/>"  << endl;
			f << tabs(depth+1) << "<binwidth values=\""<<binWidth[0] << ","<<binWidth[1]<<","<<binWidth[2] << "\"/>"  << endl;
			f << tabs(depth+1) << "<normalisetype value=\""<<normaliseType << "\"/>"  << endl;
			f << tabs(depth+1) << "<representation value=\""<<representation << "\"/>" << endl;
			f << tabs(depth+1) << "<colour r=\"" <<  r<< "\" g=\"" << g << "\" b=\"" <<b
				<< "\" a=\"" << a << "\"/>" <<endl;
			f << tabs(depth) << "</voxelise>" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}
	
	return true;
}

bool VoxeliseFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
	using std::string;
	string tmpStr;
	xmlChar *xmlString;
	
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
	if(tmpStr == "1") 
		fixedWidth=true;
	else if(tmpStr== "0")
		fixedWidth=false;
	else
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
	if(normaliseType >= VOXEL_NORMALISETYPE_MAX)
		return false;
	
	//Retrieve representation
	if(!XMLGetNextElemAttrib(nodePtr,representation,"representation","value"))
		return false;
	if(representation >=VOXEL_REPRESENT_END)
		return false;


	//Retrieve colour
	//====
	if(XMLHelpFwdToElem(nodePtr,"colour"))
		return false;
	if(!parseXMLColour(nodePtr,r,g,b,a))
		return false;

	//====
	return true;
	
}

void VoxeliseFilter::setPropFromBinding(const SelectionBinding &b)
{
}	

//== Range File Filter == 

RangeFileFilter::RangeFileFilter()
{
	dropUnranged=true;
	assumedFileFormat=RANGE_FORMAT_ORNL;
}


Filter *RangeFileFilter::cloneUncached() const
{
	RangeFileFilter *p=new RangeFileFilter();
	p->rng = rng;
	p->rngName=rngName;
	p->enabledRanges.resize(enabledRanges.size());
	std::copy(enabledRanges.begin(),enabledRanges.end(),
					p->enabledRanges.begin());
	p->enabledIons.resize(enabledIons.size());
	std::copy(enabledIons.begin(),enabledIons.end(),
					p->enabledIons.begin());
	p->assumedFileFormat=assumedFileFormat;
	p->dropUnranged=dropUnranged;

	//We are copying wether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;	
	return p;
}

void RangeFileFilter::initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut)
{
	//Copy any input, except range files to output
	for(size_t ui=0;ui<dataIn.size();ui++)
	{
		if(dataIn[ui]->getStreamType() != STREAM_TYPE_RANGE)
			dataOut.push_back(dataIn[ui]);
	}

	//Create a rangestream data to push through the init phase
	if(rng.getNumIons() && rng.getNumRanges())
	{
		RangeStreamData *rngData=new RangeStreamData;
		rngData->parentFilter=this;
		rngData->rangeFile=&rng;	
		rngData->enabledRanges.resize(enabledRanges.size());	
		std::copy(enabledRanges.begin(),enabledRanges.end(),rngData->enabledRanges.begin());
		rngData->enabledIons.resize(enabledIons.size());	
		std::copy(enabledIons.begin(),enabledIons.end(),rngData->enabledIons.begin());
		rngData->cached=false;

		dataOut.push_back(rngData);
	}
	
}

unsigned int RangeFileFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
		std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(void))
{

	//use the cached copy of the data if we have it.
	if(cacheOK)
	{
		for(unsigned int ui=0;ui<filterOutputs.size(); ui++)
			getOut.push_back(filterOutputs[ui]);

		for(unsigned int ui=0;ui<dataIn.size() ;ui++)
		{
			//We don't cache anything but our modification
			//to the ion stream data types. so we propagate
			//these.
			if(dataIn[ui]->getStreamType() != STREAM_TYPE_IONS)
				getOut.push_back(dataIn[ui]);
		}
			
		return 0;
	}

	vector<IonStreamData *> d;

	//Split the output up into chunks, one for each range, 
	//Extra 1 for unranged ions	
	d.resize(rng.getNumIons()+1);

	//Generate output filter streams. 
	for(unsigned int ui=0;ui<d.size(); ui++)
		d[ui] = new IonStreamData;

	bool haveDefIonColour=false;
	RGB defIonColour;

	//Try to maintain ion size if possible
	bool haveIonSize,sameSize; // have we set the ionSize?
	float ionSize;
	haveIonSize=false;
	sameSize=true;


	vector<size_t> dSizes;
	dSizes.resize(d.size(),0);
	//Do a first sweep to obtain range sizes needed
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS: 
			{

#ifdef _OPENMP
				//Create a unique array for each thread, so they don't try
				//to modify the same data structure
				unsigned int nT =omp_get_max_threads(); 
				vector<size_t> *dSizeArr = new vector<size_t>[nT];
				for(unsigned int uk=0;uk<nT;uk++)
					dSizeArr[uk].resize(dSizes.size(),0);
#endif
				const IonStreamData *src = ((const IonStreamData *)dataIn[ui]);
				#pragma omp parallel for
				for(size_t uj=0; uj<src->data.size();uj++)
				{
#ifdef _OPENMP
					unsigned int thisT=omp_get_thread_num();
#endif
					unsigned int rangeID;
					rangeID=rng.getRangeID(src->data[uj].getMassToCharge());

					//If ion is unranged, then it will have a rangeID of -1
					if(rangeID != (unsigned int)-1 && enabledRanges[rangeID] )
					{
						unsigned int ionID=rng.getIonID(rangeID);

						if(enabledIons[ionID])
						{
							#ifdef _OPENMP
								dSizeArr[thisT][ionID]++;
							#else
								dSizes[ionID]++;
							#endif

						}
					}
				}
#ifdef _OPENMP
				//Merge the arrays back together
				for(unsigned int uk=0;uk<nT;uk++)
				{
					for(unsigned int uj=0;uj<dSizes.size();uj++)
						dSizes[uj] = dSizes[uj]+dSizeArr[uk][uj];
				}
#endif

			}
		}
	}


	//reserve the vector to the exact size we need
	try
	{
		for(size_t ui=0;ui<d.size();ui++)
			d[ui]->data.reserve(dSizes[ui]);
	}
	catch(std::bad_alloc)
	{
		for(size_t ui=0;ui<d.size();ui++)
			delete d[ui];
		return RANGEFILE_BAD_ALLOC;
	}

	dSizes.clear();

	size_t totalSize=numElements(dataIn);
	//Go through each data stream, if it is an ion stream, range it, and keep it for later
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS: 
			{
				//Set the default (unranged) ion colour, by using
				//the first input ion colour.
				if(!haveDefIonColour)
				{
					defIonColour.red =  ((IonStreamData *)dataIn[ui])->r;
					defIonColour.green =  ((IonStreamData *)dataIn[ui])->g;
					defIonColour.blue =  ((IonStreamData *)dataIn[ui])->b;
					haveDefIonColour=true;
				}
			
				//Check for ion size consistency	
				if(haveIonSize)
				{
					sameSize &= (fabs(ionSize-((const IonStreamData *)dataIn[ui])->ionSize) 
									< std::numeric_limits<float>::epsilon());
				}
				else
				{
					ionSize=((const IonStreamData *)dataIn[ui])->ionSize;
					haveIonSize=true;
				}

				unsigned int curProg=NUM_CALLBACK;
				size_t n=0;
				for(vector<IonHit>::const_iterator it=((const IonStreamData *)dataIn[ui])->data.begin();
					       it!=((const IonStreamData *)dataIn[ui])->data.end(); ++it)
				{
					unsigned int ionID, rangeID;
					rangeID=rng.getRangeID(it->getMassToCharge());

					//If ion is unranged, then it will have a rangeID of -1
					if(rangeID != (unsigned int)-1)
						ionID=rng.getIonID(rangeID);

					//If it is unranged, then the rangeID is still -1 (as above).
					//so we won't try to keep it
					if((rangeID != (unsigned int)-1 && enabledRanges[rangeID] && enabledIons[ionID]))
					{
						ASSERT(ionID < enabledRanges.size());
						d[ionID]->data.push_back(*it);
					}
					else if(!dropUnranged && rangeID == -1)
						d[d.size()-1]->data.push_back(*it);

					//update progress every 5000 ions
					if(!curProg--)
					{
						n+=NUM_CALLBACK;
						progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
						curProg=NUM_CALLBACK;

						
						if(!(*callback)())
						{
							for(unsigned int ui=0;ui<d.size();ui++)
								delete d[ui];
							return RANGEFILE_ABORT_FAIL;
						}
					}

				}


			}
				break;
			case STREAM_TYPE_RANGE:
				//Purposely do nothing. This blocks propagation of other ranges
				//i.e. there can only be one in any given node of the tree.
				break;
			default:
				getOut.push_back(dataIn[ui]);
				break;
		}
	}

	//Set the colour of the output ranges
	//and whether to cache.
	for(unsigned int ui=0; ui<rng.getNumIons(); ui++)
	{
		RGB rngCol;
		rngCol = rng.getColour(ui);
		d[ui]->r=rngCol.red;
		d[ui]->g=rngCol.green;
		d[ui]->b=rngCol.blue;
		d[ui]->a=1.0;

	}

	//If all the ions are the same size, then propagate
	//Otherwise use the default ionsize
	if(haveIonSize && sameSize)
	{
		for(unsigned int ui=0;ui<d.size();ui++)
			d[ui]->ionSize=ionSize;
	}
	
	//Set the unranged colour
	if(haveDefIonColour && d.size())
	{
		d[d.size()-1]->r = defIonColour.red;
		d[d.size()-1]->g = defIonColour.green;
		d[d.size()-1]->b = defIonColour.blue;
		d[d.size()-1]->a = 1.0f;
	}


	//Having ranged all streams, merge them back into one ranged stream.
	if(cache)
	{
		for(unsigned int ui=0;ui<d.size(); ui++)
		{
			d[ui]->cached=1; //IMPORTANT: ->cached must be set PRIOR to push back
			filterOutputs.push_back(d[ui]);
		}
	}
	else
	{
		for(unsigned int ui=0;ui<d.size(); ui++)
			d[ui]->cached=0; //IMPORTANT: ->cached must be set PRIOR to push back
		cacheOK=false;
	}
	
	for(unsigned int ui=0;ui<d.size(); ui++)
		getOut.push_back(d[ui]);

	//Put out rangeData
	RangeStreamData *rngData=new RangeStreamData;
	rngData->parentFilter=this;
	rngData->rangeFile=&rng;	
	
	rngData->enabledRanges.resize(enabledRanges.size());	
	std::copy(enabledRanges.begin(),enabledRanges.end(),rngData->enabledRanges.begin());
	rngData->enabledIons.resize(enabledIons.size());	
	std::copy(enabledIons.begin(),enabledIons.end(),rngData->enabledIons.begin());
	
	
	rngData->cached=cache;
	
	if(cache)
		filterOutputs.push_back(rngData);

	getOut.push_back(rngData);
		
	cacheOK=cache;
	return 0;
}

void RangeFileFilter::guessFormat(const std::string &s)
{
	vector<string> sVec;
	splitStrsRef(s.c_str(),'.',sVec);

	if(!sVec.size())
		assumedFileFormat=RANGE_FORMAT_ORNL;
	else if(lowercase(sVec[sVec.size()-1]) == "rrng")
		assumedFileFormat=RANGE_FORMAT_RRNG;
	else if(lowercase(sVec[sVec.size()-1]) == "env")
		assumedFileFormat=RANGE_FORMAT_ENV;
	else
		assumedFileFormat=RANGE_FORMAT_ORNL;
}

unsigned int RangeFileFilter::updateRng()
{
	unsigned int uiErr;	
	if((uiErr = rng.open(rngName.c_str(),assumedFileFormat)))
		return uiErr;

	unsigned int nRng = rng.getNumRanges();
	enabledRanges.resize(nRng);
	unsigned int nIon = rng.getNumIons();
	enabledIons.resize(nIon);
	//Turn all ranges to "on" 
	for(unsigned int ui=0;ui<enabledRanges.size(); ui++)
		enabledRanges[ui]=(char)1;
	//Turn all ions to "on" 
	for(unsigned int ui=0;ui<enabledIons.size(); ui++)
		enabledIons[ui]=(char)1;

	return 0;
}

size_t RangeFileFilter::numBytesForCache(size_t nObjects) const
{
	//The byte requirement is input dependant
	return (nObjects*(size_t)sizeof(IONHIT));
}


void RangeFileFilter::getProperties(FilterProperties &p) const
{
	using std::string;
	p.data.clear();
	p.types.clear();
	p.keys.clear();

	//Ensure that the file is specified
	if(!rngName.size())
		return;

	//Should/be loaded
	ASSERT(enabledRanges.size())
	
	string suffix;
	vector<pair<string, string> > strVec;
	vector<unsigned int> types,keys;

	//SET 0
	strVec.push_back(make_pair("File",rngName));
	types.push_back(PROPERTY_TYPE_STRING);
	keys.push_back(0);

	std::string tmpStr;
	if(dropUnranged)
		tmpStr="1";
	else
		tmpStr="0";

	strVec.push_back(make_pair("Drop unranged",tmpStr));
	types.push_back(PROPERTY_TYPE_BOOL);
	keys.push_back(1);

	p.data.push_back(strVec);
	p.types.push_back(types);
	p.keys.push_back(keys);

	keys.clear();
	types.clear();
	strVec.clear();

	//SET 1
	for(unsigned int ui=0;ui<rng.getNumIons(); ui++)
	{
		//Get the ion ID
		stream_cast(suffix,ui);
		strVec.push_back(make_pair(string("IonID " +suffix),rng.getName(ui)));
		types.push_back(PROPERTY_TYPE_STRING);
		keys.push_back(3*ui);


		string str;	
		if(enabledIons[ui])
			str="1";
		else
			str="0";
		strVec.push_back(make_pair(string("Active Ion ")
						+ suffix,str));
		types.push_back(PROPERTY_TYPE_BOOL);
		keys.push_back(3*ui+1);
		
		RGB col;
		string thisCol;
	
		//Convert the ion colour to a hex string	
		col=rng.getColour(ui);
		genColString((unsigned char)(col.red*255),(unsigned char)(col.green*255),
				(unsigned char)(col.blue*255),255,thisCol);

		strVec.push_back(make_pair(string("colour ") + suffix,thisCol)); 
		types.push_back(PROPERTY_TYPE_COLOUR);
		keys.push_back(3*ui+2);


	}

	p.data.push_back(strVec);
	p.types.push_back(types);
	p.keys.push_back(keys);

	//SET 2->N
	for(unsigned  int ui=0; ui<rng.getNumRanges(); ui++)
	{
		strVec.clear();
		types.clear();
		keys.clear();

		stream_cast(suffix,ui);

		string str;	
		if(enabledRanges[ui])
			str="1";
		else
			str="0";

		strVec.push_back(make_pair(string("Active Rng ")
						+ suffix,str));
		types.push_back(PROPERTY_TYPE_BOOL);
		keys.push_back(KEY_RANGE_ACTIVE);

		strVec.push_back(make_pair(string("Ion ") + suffix, 
					rng.getName(rng.getIonID(ui))));
		types.push_back(PROPERTY_TYPE_STRING);
		keys.push_back(KEY_RANGE_IONID);

		std::pair<float,float > thisRange;
	
		thisRange = rng.getRange(ui);

		string mass;
		stream_cast(mass,thisRange.first);
		strVec.push_back(make_pair(string("Start rng ")+suffix,mass));
		types.push_back(PROPERTY_TYPE_REAL);
		keys.push_back(KEY_RANGE_START);
		
		stream_cast(mass,thisRange.second);
		strVec.push_back(make_pair(string("End rng ")+suffix,mass));
		types.push_back(PROPERTY_TYPE_REAL);
		keys.push_back(KEY_RANGE_END);

		p.types.push_back(types);
		p.data.push_back(strVec);
		p.keys.push_back(keys);
	}

}

bool RangeFileFilter::setProperty(unsigned int set, unsigned int key, 
					const std::string &value, bool &needUpdate)
{
	using std::string;
	needUpdate=false;

	switch(set)
	{
		case 0:
		{
			switch(key)
			{
				case 0:
				{
					//Check to see if the new file can actually be opened
					RangeFile rngTwo;

					if(value != rngName)
					{
						if(!rngTwo.open(value.c_str()))
							return false;
						else
						{
							rng.swap(rngTwo);
							rngName=value;
							needUpdate=true;
						}

					}
					else
						return false;

					break;
				}
				case 1: //Enable/disable unranged dropping
				{
					unsigned int valueInt;
					if(stream_cast(valueInt,value))
						return false;

					if(valueInt ==0 || valueInt == 1)
					{
						if(dropUnranged!= (char)valueInt)
						{
							needUpdate=true;
							dropUnranged=(char)valueInt;
						}
						else
							needUpdate=false;
					}
					else
						return false;		
			
					break;
				}	
				default:
					ASSERT(false);
					break;
			}
			if(needUpdate)
				clearCache();

			break;
		}
		case 1:
		{
			//Each property is stored in the same
			//structured group, each with NUM_ROWS per grouping.
			//The ion ID is simply the row number/NUM_ROWS.
			//similarly the element is given by remainder 
			const unsigned int NUM_ROWS=3;
			unsigned int ionID=key/NUM_ROWS;
			ASSERT(key < NUM_ROWS*rng.getNumIons());
			unsigned int prop = key-ionID*NUM_ROWS;

			switch(prop)
			{
				case 0://Ion name
				{
					
					//only allow english alphabet, upper and lowercase
					for(unsigned int ui=0;ui<value.size();ui++)
					{
						if( value[ui] < 'A'  || value[ui] > 'z' ||
							 (value[ui] > 'Z' && value[ui] < 'a')) 
							return false;
					}
					//TODO: At some point I should work out a 
					//nice way of setting the short and long names.
					rng.setIonShortName(ionID,value);
					rng.setIonLongName(ionID,value);
					needUpdate=true;
					break;
				}
				case 1: //Enable/disable ion
				{
					unsigned int valueInt;
					if(stream_cast(valueInt,value))
						return false;

					if(valueInt ==0 || valueInt == 1)
					{
						if(enabledIons[ionID] != (char)valueInt)
						{
							needUpdate=true;
							enabledIons[ionID]=(char)valueInt;
						}
						else
							needUpdate=false;
					}
					else
						return false;		
			
					break;
				}	
				case 2: //Colour of the ion
				{
					unsigned char r,g,b,a;
					parseColString(value,r,g,b,a);

					RGB newCol;
					newCol.red=(float)r/255.0f;
					newCol.green=(float)g/255.0f;
					newCol.blue=(float)b/255.0f;

					rng.setColour(ionID,newCol);
					needUpdate=true;
					break;
				}
				default:
					ASSERT(false);
			}

			if(needUpdate)
				clearCache();
			break;
		}
		default:
		{
			unsigned int rangeId=set-2;
			switch(key)
			{
				//Range active
				case KEY_RANGE_ACTIVE:
				{
					unsigned int valueInt;
					if(stream_cast(valueInt,value))
						return false;

					if(valueInt ==0 || valueInt == 1)
					{
						if(enabledRanges[rangeId] != (char)valueInt)
						{
							needUpdate=true;
							enabledRanges[rangeId]=(char)valueInt;
						}
						else
							needUpdate=false;
					}
					else
						return false;		
			
					break;
				}	
				case KEY_RANGE_IONID: 
				{
					unsigned int newID;

					if(stream_cast(newID,value))
						return false;

					if(newID == rng.getIonID(rangeId))
						return false;

					if(newID > rng.getNumRanges())
						return false;

					rng.setIonID(rangeId,newID);
					needUpdate=true;
					break;
				}
				//Range start
				case KEY_RANGE_START:
				{

					//Check for valid data type conversion
					float newMass;
					if(stream_cast(newMass,value))
						return false;

					//Ensure that it has actually changed
					if(newMass == rng.getRange(rangeId).first)
						return false;

					//Attempt to move the range to a new position
					if(!rng.moveRange(rangeId,0,newMass))
						return false;

					needUpdate=true;
					
					break;
				}
				//Range end
				case KEY_RANGE_END:
				{

					//Check for valid data type conversion
					float newMass;
					if(stream_cast(newMass,value))
						return false;

					//Ensure that it has actually changed
					if(newMass == rng.getRange(rangeId).second)
						return false;

					//Attempt to move the range to a new position
					if(!rng.moveRange(rangeId,1,newMass))
						return false;

					needUpdate=true;
					
					break;
				}


			}

			if(needUpdate)
				clearCache();
		}


	}
		
	return true;
}


std::string  RangeFileFilter::getErrString(unsigned int code) const
{
	switch(code)
	{
		case RANGEFILE_ABORT_FAIL:
			return std::string("Ranging aborted by user");
		case RANGEFILE_BAD_ALLOC:
			return std::string("Insufficient memory for range");
	}

	return std::string("BUG(range file filter): Shouldn't see this!");
}

void RangeFileFilter::setFormat(unsigned int format) 
{
	ASSERT(format < RANGE_FORMAT_END_OF_ENUM);

	assumedFileFormat=format;
}

bool RangeFileFilter::writeState(std::ofstream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<rangefile>" << endl;
			f << tabs(depth+1) << "<userstring value=\""<<userString << "\"/>"  << endl;

			f << tabs(depth+1) << "<file name=\""<< convertFileStringToCanonical(rngName) << "\"/>"  << endl;
			f << tabs(depth+1) << "<dropunranged value=\""<<(int)dropUnranged<< "\"/>"  << endl;
			f << tabs(depth+1) << "<enabledions>"<< endl;
			for(unsigned int ui=0;ui<enabledIons.size();ui++)
			{
				RGB col;
				string colourString;
				col = rng.getColour(ui);

				genColString((unsigned char)(col.red*255),(unsigned char)(col.green*255),
						(unsigned char)(col.blue*255),255,colourString);
				f<< tabs(depth+2) << "<ion id=\"" << ui << "\" enabled=\"" 
					<< (int)enabledIons[ui] << "\" colour=\"" << colourString << "\"/>" << endl;
			}
			f << tabs(depth+1) << "</enabledions>"<< endl;

			f << tabs(depth+1) << "<enabledranges>"<< endl;
			
			for(unsigned int ui=0;ui<enabledRanges.size();ui++)
			{
				f<< tabs(depth+2) << "<range id=\"" << ui << "\" enabled=\"" 
					<< (int)enabledRanges[ui] << "\"/>" << endl;
			}
			f << tabs(depth+1) << "</enabledranges>"<< endl;
			
			f << tabs(depth) << "</rangefile>" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool RangeFileFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
	
	//Retrieve user string
	//==
	if(XMLHelpFwdToElem(nodePtr,"userstring"))
		return false;

	xmlChar *xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	userString=(char *)xmlString;
	xmlFree(xmlString);
	//==

	//Retrieve file name	
	//==
	//Retrieve file name	
	if(XMLHelpFwdToElem(nodePtr,"file"))
		return false;
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"name");
	if(!xmlString)
		return false;
	rngName=(char *)xmlString;
	xmlFree(xmlString);

	//Override the string, as needed
	if( (stateFileDir.size()) &&
		(rngName.size() > 2 && rngName.substr(0,2) == "./") )
	{
		rngName=stateFileDir + rngName.substr(2);
	}

	rngName=convertFileStringToNative(rngName);

	//try using the extention name of the file to guess format
	guessFormat(rngName);

	//Load range file using guessed format
	if(rng.open(rngName.c_str(),assumedFileFormat))
	{
		//If that failed, go to plan B
		//Brute force try all readers
		bool openOK=false;

		for(unsigned int ui=1;ui<RANGE_FORMAT_END_OF_ENUM; ui++)
		{
			if(!rng.open(rngName.c_str(),ui))
			{
				assumedFileFormat=ui;
				openOK=true;
				break;
			}
		}
	
		if(!openOK)
			return false;
	}
	


		
	//==
	
	std::string tmpStr;
	//Retrieve user string
	//==
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"dropunranged","value"))
		return false;
	
	if(tmpStr=="1")
		dropUnranged=true;
	else if(tmpStr=="0")
		dropUnranged=false;
	else
		return false;

	//==
	

	//Retrieve enabled ions	
	//===
	if(XMLHelpFwdToElem(nodePtr,"enabledions"))
		return false;
	xmlNodePtr tmpNode=nodePtr;

	nodePtr=nodePtr->xmlChildrenNode;

	unsigned int ionID;
	bool enabled;
	//By default, turn ions off, but use state file to turn them on
	enabledIons.resize(rng.getNumIons(),false);
	while(!XMLHelpFwdToElem(nodePtr,"ion"))
	{
		//Get ID value
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"id");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;
		xmlFree(xmlString);

		//Check it is streamable
		if(stream_cast(ionID,tmpStr))
			return false;

		if(ionID>= rng.getNumIons()) 
			return false;
		
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"enabled");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;

		if(tmpStr == "0")
			enabled=false;
		else if(tmpStr == "1")
			enabled=true;
		else
			return false;

		enabledIons[ionID]=enabled;
		xmlFree(xmlString);
		
		
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"colour");
		if(!xmlString)
			return false;


		tmpStr=(char *)xmlString;

		unsigned char r,g,b,a;
		if(!parseColString(tmpStr,r,g,b,a))
			return false;
		
		RGB col;
		col.red=(float)r/255.0f;
		col.green=(float)g/255.0f;
		col.blue=(float)b/255.0f;
		rng.setColour(ionID,col);	
		xmlFree(xmlString);
	}

	//===


	nodePtr=tmpNode;
	//Retrieve enabled ranges
	//===
	if(XMLHelpFwdToElem(nodePtr,"enabledranges"))
		return false;
	tmpNode=nodePtr;

	nodePtr=nodePtr->xmlChildrenNode;

	//By default, turn ranges off (cause there are lots of them), and use state to turn them on
	enabledRanges.resize(rng.getNumRanges(),true);
	unsigned int rngID;
	while(!XMLHelpFwdToElem(nodePtr,"range"))
	{
		//Get ID value
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"id");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;
		xmlFree(xmlString);

		//Check it is streamable
		if(stream_cast(rngID,tmpStr))
			return false;

		if(rngID>= rng.getNumRanges()) 
			return false;
		
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"enabled");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;

		if(tmpStr == "0")
			enabled=false;
		else if(tmpStr == "1")
			enabled=true;
		else
			return false;

		xmlFree(xmlString);
		enabledRanges[rngID]=enabled;
	}
	//===

	return true;
}

void RangeFileFilter::getStateOverrides(std::vector<string> &externalAttribs) const 
{
	externalAttribs.push_back(rngName);
}


void RangeFileFilter::setPropFromRegion(unsigned int method, unsigned int regionID, float newPos)
{
	ASSERT(regionID < rng.getNumRanges());

	unsigned int rangeID = regionID;

	switch(method)
	{
		case REGION_LEFT_EXTEND:
			rng.moveRange(rangeID,false, newPos);
			break;
		case REGION_MOVE:
		{
			std::pair<float,float> limits;
			limits=rng.getRange(rangeID);
			float delta;
			delta = (limits.second-limits.first)/2;
			rng.moveBothRanges(rangeID,newPos-delta,newPos+delta);
			break;
		}
		case REGION_RIGHT_EXTEND:
			rng.moveRange(rangeID,true, newPos);
			break;
		default:
			ASSERT(false);
	}

	clearCache();
}

bool RangeFileFilter::writePackageState(std::ofstream &f, unsigned int format,
			const std::vector<std::string> &valueOverrides, unsigned int depth) const
{
	ASSERT(valueOverrides.size() == 1);

	//Temporarily modify the state of the filter, then call writestate
	string tmpFilename=rngName;


	//override const -- naughty, but we know what we are doing...
	const_cast<RangeFileFilter*>(this)->rngName=valueOverrides[0];
	bool result;
	result=writeState(f,format,depth);
	const_cast<RangeFileFilter*>(this)->rngName=tmpFilename;

	return result;
}

SpectrumPlotFilter::SpectrumPlotFilter()
{
	minPlot=0;
	maxPlot=150;
	autoExtrema=true;	
	binWidth=0.5;
	plotType=0;
	logarithmic=1;

	//Default to blue plot
	r=g=0;
	b=a=1;
}

Filter *SpectrumPlotFilter::cloneUncached() const
{
	SpectrumPlotFilter *p = new SpectrumPlotFilter();

	p->minPlot=minPlot;
	p->maxPlot=maxPlot;
	p->binWidth=binWidth;
	p->autoExtrema=autoExtrema;
	p->r=r;	
	p->g=g;	
	p->b=b;	
	p->a=a;	
	p->plotType=plotType;


	//We are copying wether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

//!Get approx number of bytes for caching output
size_t SpectrumPlotFilter::numBytesForCache(size_t nObjects) const
{
	//Check that we have good plot limits, and bin width. if not, we cannot estmate cache size
	if(minPlot ==std::numeric_limits<float>::max() ||
		maxPlot==-std::numeric_limits<float>::max()  || 
		binWidth < sqrt(std::numeric_limits<float>::epsilon()))
	{
		return (size_t)(-1);
	}

	return (size_t)((float)(maxPlot- minPlot)/(binWidth))*2*sizeof(float);
}

unsigned int SpectrumPlotFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(void))
{


	size_t totalSize=numElements(dataIn);
	

	//Determine min and max of input
	if(autoExtrema)
	{
		progress.maxStep=2;
		progress.step=1;
		progress.stepName="Extrema";
	
		size_t n=0;
		minPlot = std::numeric_limits<float>::max();
		maxPlot =-std::numeric_limits<float>::max();
		//Loop through each type of data
		
		unsigned int curProg=NUM_CALLBACK;	
		for(unsigned int ui=0;ui<dataIn.size() ;ui++)
		{
			//Only process stream_type_ions. Do not propagate anything,
			//except for the spectrum
			if(dataIn[ui]->getStreamType() == STREAM_TYPE_IONS)
			{
				IonStreamData *ions;
				ions = (IonStreamData *)dataIn[ui];
				for(unsigned int uj=0;uj<ions->data.size(); uj++)
				{
					minPlot = std::min(minPlot,
						ions->data[uj].getMassToCharge());
					maxPlot = std::max(maxPlot,
						ions->data[uj].getMassToCharge());


					if(!curProg--)
					{
						n+=NUM_CALLBACK;
						progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
						curProg=NUM_CALLBACK;
						if(!(*callback)())
							return SPECTRUM_ABORT_FAIL;
					}
				}
	
			}
			
		}
	
		//Check that the plot values hvae been set (ie not same as initial values)
		if(minPlot !=std::numeric_limits<float>::max() &&
			maxPlot!=-std::numeric_limits<float>::max() )
		{
			//push them out a bit to make the edges visible
			maxPlot=maxPlot+1;
			minPlot=minPlot-1;
		}

		//Time to move to phase 2	
		progress.step=2;
		progress.stepName="count";
	}



	double delta = ((double)maxPlot - (double)(minPlot))/(double)binWidth;


	//Check that we have good plot limits.
	if(minPlot ==std::numeric_limits<float>::max() || 
		minPlot ==-std::numeric_limits<float>::max() || 
		fabs(delta) >  std::numeric_limits<float>::max() || // Check for out-of-range
		 binWidth < sqrt(std::numeric_limits<float>::epsilon())	)
	{
		//If not, then simply set it to "1".
		minPlot=0; maxPlot=1.0; binWidth=0.1;
	}



	//Estimate number of bins in floating point, and check for potential overflow .
	float tmpNBins = (float)((maxPlot-minPlot)/binWidth);
	if(tmpNBins > SPECTRUM_MAX_BINS)
		tmpNBins=SPECTRUM_MAX_BINS;
	unsigned int nBins = (unsigned int)tmpNBins;

	if (!nBins)
	{
		tmpNBins = 10;
		binWidth = (maxPlot-minPlot)/nBins;
	}

	PlotStreamData *d;
	d=new PlotStreamData;
	try
	{
		d->xyData.resize(nBins);
	}
	catch(std::bad_alloc)
	{

		delete d;
		return SPECTRUM_BAD_ALLOC;
	}
	d->r = r;
	d->g = g;
	d->b = b;
	d->a = a;
	d->logarithmic=logarithmic;
	d->plotType = plotType;
	d->index=0;
	d->parent=this;
	d->dataLabel = getUserString();
	d->yLabel= "Count";

	//Check all the incoming ion data's type name
	//and if it is all the same, use it for the plot X-axis
	std::string valueName;
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS: 
			{
				const IonStreamData *ionD;
				ionD=(const IonStreamData *)dataIn[ui];
				if(!valueName.size())
					valueName=ionD->valueType;
				else
				{
					if(ionD->valueType != valueName)
					{
						valueName="Mixed data";
						break;
					}
				}
			}
		}
	}

	d->xLabel=valueName;


	//Look for any ranges in input stream, and add them to the plot
	//while we are doing that, count the number of ions too
	for(unsigned int ui=0;ui<dataIn.size();ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_RANGE:
			{
				const RangeStreamData *rangeD;
				rangeD=(const RangeStreamData *)dataIn[ui];
				for(unsigned int uj=0;uj<rangeD->rangeFile->getNumRanges();uj++)
				{
					unsigned int ionId;
					ionId=rangeD->rangeFile->getIonID(uj);
					//Only append the region if both the range
					//and the ion are enabled
					if((rangeD->enabledRanges)[uj] && 
						(rangeD->enabledIons)[ionId])
					{
						//save the range as a "region"
						d->regions.push_back(rangeD->rangeFile->getRange(uj));
						d->regionID.push_back(uj);
						d->parent=this;
						d->regionParent=rangeD->parentFilter;

						RGB colour;
						//Use the ionID to set the range colouring
						colour=rangeD->rangeFile->getColour(ionId);

						//push back the range colour
						d->regionR.push_back(colour.red);
						d->regionG.push_back(colour.green);
						d->regionB.push_back(colour.blue);
					}
				}
				break;
			}
			default:
				break;
		}
	}

#pragma omp parallel for
	for(unsigned int ui=0;ui<nBins;ui++)
	{
		d->xyData[ui].first = minPlot + ui*binWidth;
		d->xyData[ui].second=0;
	}	


	//Number of ions currently procesed
	size_t n=0;
	unsigned int curProg=NUM_CALLBACK;
	//Loop through each type of data	
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS: 
			{
				const IonStreamData *ions;
				ions = (const IonStreamData *)dataIn[ui];



				//Sum the data bins as needed
				for(unsigned int uj=0;uj<ions->data.size(); uj++)
				{
					unsigned int bin;
					bin = (unsigned int)((ions->data[uj].getMassToCharge()-minPlot)/binWidth);
					//Dependant upon the bounds,
					//actual data could be anywhere.
					if( bin < d->xyData.size())
						d->xyData[bin].second++;

					//update progress every CALLBACK ions
					if(!curProg--)
					{
						n+=NUM_CALLBACK;
						progress.filterProgress= (unsigned int)(((float)(n)/((float)totalSize))*100.0f);
						curProg=NUM_CALLBACK;
						if(!(*callback)())
						{
							delete d;
							return SPECTRUM_ABORT_FAIL;
						}
					}
				}

			}
			default:
				//Don't propagate any type.
				break;
		}

	}

	if(cache)
	{
		d->cached=1; //IMPORTANT: cached must be set PRIOR to push back
		filterOutputs.push_back(d);
		cacheOK=true;
	}
	else
		d->cached=0;

	getOut.push_back(d);

	return 0;
}

void SpectrumPlotFilter::getProperties(FilterProperties &propertyList) const
{
	propertyList.data.clear();
	propertyList.keys.clear();
	propertyList.types.clear();

	vector<unsigned int> type,keys;
	vector<pair<string,string> > s;

	string str;

	stream_cast(str,binWidth);
	keys.push_back(KEY_SPECTRUM_BINWIDTH);
	s.push_back(make_pair("bin width", str));
	type.push_back(PROPERTY_TYPE_REAL);

	if(autoExtrema)
		str = "1";
	else
		str = "0";

	keys.push_back(KEY_SPECTRUM_AUTOEXTREMA);
	s.push_back(make_pair("Auto Min/max", str));
	type.push_back(PROPERTY_TYPE_BOOL);

	stream_cast(str,minPlot);
	keys.push_back(KEY_SPECTRUM_MIN);
	s.push_back(make_pair("Min", str));
	type.push_back(PROPERTY_TYPE_REAL);

	stream_cast(str,maxPlot);
	keys.push_back(KEY_SPECTRUM_MAX);
	s.push_back(make_pair("Max", str));
	type.push_back(PROPERTY_TYPE_REAL);
	
	if(logarithmic)
		str = "1";
	else
		str = "0";
	keys.push_back(KEY_SPECTRUM_LOGARITHMIC);
	s.push_back(make_pair("Logarithmic", str));
	type.push_back(PROPERTY_TYPE_BOOL);


	//Let the user know what the valid values for plot type are
	string tmpChoice;
	vector<pair<unsigned int,string> > choices;


	string tmpStr;
	tmpStr=plotString(PLOT_TYPE_LINES);
	choices.push_back(make_pair((unsigned int) PLOT_TYPE_LINES,tmpStr));
	tmpStr=plotString(PLOT_TYPE_BARS);
	choices.push_back(make_pair((unsigned int)PLOT_TYPE_BARS,tmpStr));
	tmpStr=plotString(PLOT_TYPE_STEPS);
	choices.push_back(make_pair((unsigned int)PLOT_TYPE_STEPS,tmpStr));
	tmpStr=plotString(PLOT_TYPE_STEM);
	choices.push_back(make_pair((unsigned int)PLOT_TYPE_STEM,tmpStr));


	tmpStr= choiceString(choices,plotType);
	s.push_back(make_pair(string("Plot Type"),tmpStr));
	type.push_back(PROPERTY_TYPE_CHOICE);
	keys.push_back(KEY_SPECTRUM_PLOTTYPE);

	string thisCol;

	//Convert the colour to a hex string
	genColString((unsigned char)(r*255.0),(unsigned char)(g*255.0),
		(unsigned char)(b*255.0),(unsigned char)(a*255.0),thisCol);

	s.push_back(make_pair(string("colour"),thisCol)); 
	type.push_back(PROPERTY_TYPE_COLOUR);
	keys.push_back(KEY_SPECTRUM_COLOUR);

	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);
}

bool SpectrumPlotFilter::setProperty(unsigned int set, unsigned int key, 
					const std::string &value, bool &needUpdate) 
{
	ASSERT(!set);

	needUpdate=false;
	switch(key)
	{
		//Bin width
		case KEY_SPECTRUM_BINWIDTH:
		{
			float newWidth;
			if(stream_cast(newWidth,value))
				return false;

			if(newWidth < std::numeric_limits<float>::epsilon())
				return false;

			//Prevent overflow on next line
			if(maxPlot == std::numeric_limits<float>::max() ||
				minPlot == std::numeric_limits<float>::min())
				return false;

			if(newWidth < 0.0f || newWidth > (maxPlot - minPlot))
				return false;

			needUpdate=true;
			binWidth=newWidth;

			break;
		}
		//Auto min/max
		case KEY_SPECTRUM_AUTOEXTREMA:
		{
			//Only allow valid values
			unsigned int valueInt;
			if(stream_cast(valueInt,value))
				return false;

			//Only update as needed
			if(valueInt ==0 || valueInt == 1)
			{
				if(autoExtrema != valueInt)
				{
					needUpdate=true;
					autoExtrema=valueInt;
				}
				else
					needUpdate=false;

			}
			else
				return false;		
	
			break;

		}
		//Plot min
		case KEY_SPECTRUM_MIN:
		{
			if(autoExtrema)
				return false;

			float newMin;
			if(stream_cast(newMin,value))
				return false;

			if(newMin >= maxPlot)
				return false;

			needUpdate=true;
			minPlot=newMin;

			break;
		}
		//Plot max
		case KEY_SPECTRUM_MAX:
		{
			if(autoExtrema)
				return false;
			float newMax;
			if(stream_cast(newMax,value))
				return false;

			if(newMax <= minPlot)
				return false;

			needUpdate=true;
			maxPlot=newMax;

			break;
		}
		case KEY_SPECTRUM_LOGARITHMIC:
		{
			//Only allow valid values
			unsigned int valueInt;
			if(stream_cast(valueInt,value))
				return false;

			//Only update as needed
			if(valueInt ==0 || valueInt == 1)
			{
				if(logarithmic != valueInt)
				{
					needUpdate=true;
					logarithmic=valueInt;
				}
				else
					needUpdate=false;

			}
			else
				return false;		
	
			break;

		}
		//Plot type
		case KEY_SPECTRUM_PLOTTYPE:
		{
			unsigned int tmpPlotType;

			tmpPlotType=plotID(value);

			if(tmpPlotType >= PLOT_TYPE_ENDOFENUM)
				return false;

			plotType = tmpPlotType;
			needUpdate=true;	
			break;
		}
		case KEY_SPECTRUM_COLOUR:
		{
			unsigned char newR,newG,newB,newA;

			parseColString(value,newR,newG,newB,newA);

			if(newB != b || newR != r ||
				newG !=g || newA != a)
				needUpdate=true;
			r=newR/255.0;
			g=newG/255.0;
			b=newB/255.0;
			a=newA/255.0;
			break;
		}
		default:
			ASSERT(false);
			break;

	}

	return true;
}

std::string  SpectrumPlotFilter::getErrString(unsigned int code) const
{
	switch(code)
	{
		case SPECTRUM_BAD_ALLOC:
			return string("Insufficient memory for spectrum fitler.");
		case SPECTRUM_BAD_BINCOUNT:
			return string("Bad bincount value in spectrum filter.");
	}
	return std::string("BUG: (SpectrumPlotFilter::getErrString) Shouldn't see this!");
}

bool SpectrumPlotFilter::writeState(std::ofstream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<spectrumplot>" << endl;
			f << tabs(depth+1) << "<userstring value=\""<<userString << "\"/>"  << endl;

			f << tabs(depth+1) << "<extrema min=\"" << minPlot << "\" max=\"" <<
					maxPlot  << "\" auto=\"" << autoExtrema << "\"/>" << endl;
			f << tabs(depth+1) << "<binwidth value=\"" << binWidth<< "\"/>" << endl;

			f << tabs(depth+1) << "<colour r=\"" <<  r<< "\" g=\"" << g << "\" b=\"" <<b
				<< "\" a=\"" << a << "\"/>" <<endl;
			
			f << tabs(depth+1) << "<logarithmic value=\"" << logarithmic<< "\"/>" << endl;

			f << tabs(depth+1) << "<plottype value=\"" << plotType<< "\"/>" << endl;
			
			f << tabs(depth) << "</spectrumplot>" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool SpectrumPlotFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
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

	//Retrieve Extrema 
	//===
	float tmpMin,tmpMax;
	if(XMLHelpFwdToElem(nodePtr,"extrema"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"min");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(tmpMin,tmpStr))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"max");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(tmpMax,tmpStr))
		return false;

	xmlFree(xmlString);

	if(tmpMin >=tmpMax)
		return false;

	minPlot=tmpMin;
	maxPlot=tmpMax;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"auto");
	if(!xmlString)
		return false;
	
	tmpStr=(char *)xmlString;
	if(tmpStr == "1") 
		autoExtrema=true;
	else if(tmpStr== "0")
		autoExtrema=false;
	else
	{
		xmlFree(xmlString);
		return false;
	}

	xmlFree(xmlString);
	//===
	
	//Retrieve bin width 
	//====
	//
	if(!XMLGetNextElemAttrib(nodePtr,binWidth,"binwidth","value"))
		return false;
	if(binWidth  <= 0.0)
		return false;

	if(!autoExtrema && binWidth > maxPlot - minPlot)
		return false;
	//====
	//Retrieve colour
	//====
	if(XMLHelpFwdToElem(nodePtr,"colour"))
		return false;
	if(!parseXMLColour(nodePtr,r,g,b,a))
		return false;
	//====
	
	std::string tmpString;
	//Retrieve logarithmic mode
	//====
	if(!XMLGetNextElemAttrib(nodePtr,tmpString,"logarithmic","value"))
		return false;
	if(tmpStr == "0")
		logarithmic=false;
	else if(tmpStr == "1")
		logarithmic=true;
	else
		return false;
	//====

	//Retrieve plot type 
	//====
	if(!XMLGetNextElemAttrib(nodePtr,plotType,"plottype","value"))
		return false;
	if(plotType >= PLOT_TYPE_ENDOFENUM)
	       return false;	
	//====

	return true;
}

Filter *IonClipFilter::cloneUncached() const
{
	IonClipFilter *p = new IonClipFilter;
	p->primitiveType=primitiveType;
	p->invertedClip=invertedClip;
	p->showPrimitive=showPrimitive;
	p->vectorParams.resize(vectorParams.size());
	p->scalarParams.resize(scalarParams.size());
	
	std::copy(vectorParams.begin(),vectorParams.end(),p->vectorParams.begin());
	std::copy(scalarParams.begin(),scalarParams.end(),p->scalarParams.begin());

	p->lockAxisMag = lockAxisMag;

	//We are copying wether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;

	return p;
}

//!Get approx number of bytes for caching output
size_t IonClipFilter::numBytesForCache(size_t nObjects) const
{
	//Without full processing, we cannot tell, so provide upper estimate.
	return nObjects*sizeof(IONHIT);
}

//!update filter
unsigned int IonClipFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
			std::vector<const FilterStreamData *> &getOut, ProgressData &progress, 
								bool (*callback)(void))
{
	//Clear selection devices
	devices.clear();

	if(showPrimitive)
	{
		//construct a new primitive, do not cache
		DrawStreamData *drawData=new DrawStreamData;
		switch(primitiveType)
		{
			case IONCLIP_PRIMITIVE_SPHERE:
			{
				//Add drawable components
				DrawSphere *dS = new DrawSphere;
				dS->setOrigin(vectorParams[0]);
				dS->setRadius(scalarParams[0]);
				//FIXME: Alpha blending is all screwed up. May require more
				//advanced drawing in scene. (front-back drawing).
				//I have set alpha=1 for now.
				dS->setColour(0.5,0.5,0.5,1.0);
				dS->setLatSegments(40);
				dS->setLongSegments(40);
				dS->wantsLight=true;
				drawData->drawables.push_back(dS);

				//Set up selection "device" for user interaction
				//Note the order of s->addBinding is critical,
				//as bindings are selected by first match.
				//====
				//The object is selectable
				dS->canSelect=true;

				SelectionDevice *s = new SelectionDevice(this);
				SelectionBinding b[3];

				//Apple doesn't have right click, so we need
				//to hook up an additional system for them.
				//Don't use ifdefs, as this would be useful for
				//normal laptops and the like.
				b[0].setBinding(SELECT_BUTTON_LEFT,FLAG_CMD,DRAW_SPHERE_BIND_ORIGIN,
							BINDING_SPHERE_ORIGIN,dS->getOrigin(),dS);
				b[0].setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
				s->addBinding(b[0]);

				//Bind the drawable object to the properties we wish
				//to be able to modify
				b[1].setBinding(SELECT_BUTTON_LEFT,0,DRAW_SPHERE_BIND_RADIUS,
					BINDING_SPHERE_RADIUS,dS->getRadius(),dS);
				b[1].setInteractionMode(BIND_MODE_FLOAT_TRANSLATE);
				b[1].setFloatLimits(0,std::numeric_limits<float>::max());
				s->addBinding(b[1]);

				b[2].setBinding(SELECT_BUTTON_RIGHT,0,DRAW_SPHERE_BIND_ORIGIN,
					BINDING_SPHERE_ORIGIN,dS->getOrigin(),dS);	
				b[2].setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
				s->addBinding(b[2]);
					
				devices.push_back(s);
				//=====
				break;
			}
			case IONCLIP_PRIMITIVE_PLANE:
			{
				//Origin + normal
				ASSERT(vectorParams.size() == 2);

				//Add drawable components
				DrawSphere *dS = new DrawSphere;
				dS->setOrigin(vectorParams[0]);
				dS->setRadius(drawScale/10);
				dS->setColour(0.5,0.5,0.5,1.0);
				dS->setLatSegments(40);
				dS->setLongSegments(40);
				dS->wantsLight=true;
				drawData->drawables.push_back(dS);
				
				DrawVector *dV  = new DrawVector;
				dV->setOrigin(vectorParams[0]);
				dV->setVector(vectorParams[1]*drawScale);
				drawData->drawables.push_back(dV);
				
				//Set up selection "device" for user interaction
				//====
				//The object is selectable
				dS->canSelect=true;
				dV->canSelect=true;

				SelectionDevice *s = new SelectionDevice(this);
				SelectionBinding b[2];
				//Bind the drawable object to the properties we wish
				//to be able to modify

				//Bind orientation to vector left click
				b[0].setBinding(SELECT_BUTTON_LEFT,0,DRAW_VECTOR_BIND_ORIENTATION,
					BINDING_PLANE_DIRECTION, dV->getVector(),dV);
				b[0].setInteractionMode(BIND_MODE_POINT3D_ROTATE);
				b[0].setFloatLimits(0,std::numeric_limits<float>::max());
				s->addBinding(b[0]);
				
				//Bind translation to sphere left click
				b[1].setBinding(SELECT_BUTTON_LEFT,0,DRAW_SPHERE_BIND_ORIGIN,
						BINDING_PLANE_ORIGIN,dS->getOrigin(),dS);	
				b[1].setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
				s->addBinding(b[1]);


				devices.push_back(s);
				//=====
				break;
			}
			case IONCLIP_PRIMITIVE_CYLINDER:
			{
				//Origin + normal
				ASSERT(vectorParams.size() == 2);
				//Add drawable components
				DrawCylinder *dC = new DrawCylinder;
				dC->setOrigin(vectorParams[0]);
				dC->setRadius(scalarParams[0]);
				dC->setColour(0.5,0.5,0.5,1.0);
				dC->setSlices(40);
				dC->setLength(sqrt(vectorParams[1].sqrMag()));
				dC->setDirection(vectorParams[1]);
				dC->wantsLight=true;
				drawData->drawables.push_back(dC);
				
				//Set up selection "device" for user interaction
				//====
				//The object is selectable
				dC->canSelect=true;
				//Start and end radii must be the same (not a
				//tapered cylinder)
				dC->lockRadii();

				SelectionDevice *s = new SelectionDevice(this);
				SelectionBinding b;
				//Bind the drawable object to the properties we wish
				//to be able to modify

				//Bind left + command button to move
				b.setBinding(SELECT_BUTTON_LEFT,FLAG_CMD,DRAW_CYLINDER_BIND_ORIGIN,
					BINDING_CYLINDER_ORIGIN,dC->getOrigin(),dC);	
				b.setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
				s->addBinding(b);

				//Bind left + shift to change orientation
				b.setBinding(SELECT_BUTTON_LEFT,FLAG_SHIFT,DRAW_CYLINDER_BIND_DIRECTION,
					BINDING_CYLINDER_DIRECTION,dC->getDirection(),dC);	

				if(lockAxisMag)
					b.setInteractionMode(BIND_MODE_POINT3D_ROTATE_LOCK);
				else
					b.setInteractionMode(BIND_MODE_POINT3D_ROTATE);
				s->addBinding(b);

				//Bind right button to changing position 
				b.setBinding(SELECT_BUTTON_RIGHT,0,DRAW_CYLINDER_BIND_ORIGIN,
					BINDING_CYLINDER_ORIGIN,dC->getOrigin(),dC);	
				b.setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
				s->addBinding(b);
					
				//Bind middle button to changing orientation
				b.setBinding(SELECT_BUTTON_MIDDLE,0,DRAW_CYLINDER_BIND_DIRECTION,
					BINDING_CYLINDER_DIRECTION,dC->getDirection(),dC);	
				if(lockAxisMag)
					b.setInteractionMode(BIND_MODE_POINT3D_ROTATE_LOCK);
				else
					b.setInteractionMode(BIND_MODE_POINT3D_ROTATE);
				s->addBinding(b);
					
				//Bind left button to changing radius
				b.setBinding(SELECT_BUTTON_LEFT,0,DRAW_CYLINDER_BIND_RADIUS,
					BINDING_CYLINDER_RADIUS,dC->getRadius(),dC);
				b.setInteractionMode(BIND_MODE_FLOAT_TRANSLATE);
				b.setFloatLimits(0,std::numeric_limits<float>::max());
				s->addBinding(b); 
				
				devices.push_back(s);
				//=====
				
				break;
			}
			case IONCLIP_PRIMITIVE_AAB:
			{
				//Centre  + corner
				ASSERT(vectorParams.size() == 2);
				ASSERT(scalarParams.size() == 0);

				//Add drawable components
				DrawRectPrism *dR = new DrawRectPrism;
				dR->setAxisAligned(vectorParams[0]-vectorParams[1],
							vectorParams[0] + vectorParams[1]);
				dR->setColour(0.5,0.5,0.5,1.0);
				dR->setDrawMode(DRAW_FLAT);
				dR->wantsLight=true;
				drawData->drawables.push_back(dR);
				
				
				//Set up selection "device" for user interaction
				//====
				//The object is selectable
				dR->canSelect=true;

				SelectionDevice *s = new SelectionDevice(this);
				SelectionBinding b[2];
				//Bind the drawable object to the properties we wish
				//to be able to modify

				//Bind orientation to sphere left click
				b[0].setBinding(SELECT_BUTTON_LEFT,0,DRAW_RECT_BIND_TRANSLATE,
					BINDING_RECT_TRANSLATE, vectorParams[0],dR);
				b[0].setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
				s->addBinding(b[0]);


				b[1].setBinding(SELECT_BUTTON_RIGHT,0,DRAW_RECT_BIND_CORNER_MOVE,
						BINDING_RECT_CORNER_MOVE, vectorParams[1],dR);
				b[1].setInteractionMode(BIND_MODE_POINT3D_SCALE);
				s->addBinding(b[1]);

				devices.push_back(s);
				//=====
				break;
			}
			default:
				ASSERT(false);

		}
	
		drawData->cached=false;	
		getOut.push_back(drawData);
	}

	//use the cached copy of the data if we have it.
	if(cacheOK)
	{
		for(unsigned int ui=0;ui<filterOutputs.size(); ui++)
			getOut.push_back(filterOutputs[ui]);

		for(unsigned int ui=0;ui<dataIn.size() ;ui++)
		{
			//We don't cache anything but our modification
			//to the ion stream data types. so we propagate
			//these.
			if(dataIn[ui]->getStreamType() != STREAM_TYPE_IONS)
				getOut.push_back(dataIn[ui]);
		}
			
		return 0;
	}


	IonStreamData *d=0;
	try
	{
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		size_t n=0;
		size_t totalSize=numElements(dataIn);

		//Loop through each data set
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS:
			{
				d=new IonStreamData;
				switch(primitiveType)
				{
					case IONCLIP_PRIMITIVE_SPHERE:
					{
						//origin + radius
						ASSERT(vectorParams.size() == 1);
						ASSERT(scalarParams.size() == 1);
						ASSERT(scalarParams[0] >= 0.0f);
						float sqrRad = scalarParams[0]*scalarParams[0];

						unsigned int curProg=NUM_CALLBACK;
						//Loop through each ion in the dataset
						for(vector<IonHit>::const_iterator it=((const IonStreamData *)dataIn[ui])->data.begin();
							       it!=((const IonStreamData *)dataIn[ui])->data.end(); ++it)
						{
							//Use XOR operand
							if((inSphere(it->getPosRef(),vectorParams[0],sqrRad)) ^ (invertedClip))
								d->data.push_back(*it);
							
							//update progress every CALLBACK ions
							if(!curProg--)
							{
								n+=NUM_CALLBACK;
								progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
								curProg=NUM_CALLBACK;
								if(!(*callback)())
								{
									delete d;
									return IONCLIP_CALLBACK_FAIL;
								}
							}
						}
						break;
					}
					case IONCLIP_PRIMITIVE_PLANE:
					{
						//Origin + normal
						ASSERT(vectorParams.size() == 2);


						//Loop through each data set
						unsigned int curProg=NUM_CALLBACK;
						//Loop through each ion in the dataset
						for(vector<IonHit>::const_iterator it=((const IonStreamData *)dataIn[ui])->data.begin();
							       it!=((const IonStreamData *)dataIn[ui])->data.end(); ++it)
						{
							//Use XOR operand
							if((inFrontPlane(it->getPosRef(),vectorParams[0],vectorParams[1])) ^ invertedClip)
								d->data.push_back(*it);

							//update progress every CALLBACK ions
							if(!curProg--)
							{
								n+=NUM_CALLBACK;
								progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
								curProg=NUM_CALLBACK;
								if(!(*callback)())
								{
									delete d;
									return IONCLIP_CALLBACK_FAIL;
								}
							}
						}
						break;
					}
					case IONCLIP_PRIMITIVE_CYLINDER:
					{
						//Origin + axis
						ASSERT(vectorParams.size() == 2);
						//Radius perp. to axis
						ASSERT(scalarParams.size() == 1);


						unsigned int curProg=NUM_CALLBACK;
						Point3f rotVec;
						//Cross product desired drection with default
						//direction to produce rotation vector
						Point3D dir(0.0f,0.0f,1.0f),direction;
						direction=vectorParams[1];
						direction.normalise();

						float angle = dir.angle(direction);

						float halfLen=sqrt(vectorParams[1].sqrMag())/2.0f;
						float sqrRad=scalarParams[0]*scalarParams[0];
						//Check that we actually need to rotate, to avoid numerical singularity 
						//when cylinder axis is too close to (or is) z-axis
						if(angle > sqrt(std::numeric_limits<float>::epsilon()))
						{
							dir = dir.crossProd(direction);
							dir.normalise();

							rotVec.fx=dir[0];
							rotVec.fy=dir[1];
							rotVec.fz=dir[2];

							Quaternion q1;

						
							//Generate the rotating quaternions
							quat_get_rot_quat(&rotVec,-angle,&q1);
							//pre-compute cylinder length and radius^2
							//Loop through each ion in the dataset
							for(vector<IonHit>::const_iterator it=((const IonStreamData *)dataIn[ui])->data.begin();
								       it!=((const IonStreamData *)dataIn[ui])->data.end(); ++it)
							{
								Point3f p;
								//Translate to get position w respect to cylinder centre
								Point3D ptmp;
								ptmp=it->getPosRef()-vectorParams[0];
								p.fx=ptmp[0];
								p.fy=ptmp[1];
								p.fz=ptmp[2];
								//rotate ion position into cylindrical coordinates
								quat_rot_apply_quat(&p,&q1);


								//Keep ion if inside cylinder XOR inversion of the clippping (inside vs outside)
								if((p.fz < halfLen && p.fz > -halfLen && p.fx*p.fx+p.fy*p.fy < sqrRad)  ^ invertedClip)
										d->data.push_back(*it);

								//update progress every CALLBACK ions
								if(!curProg--)
								{
									n+=NUM_CALLBACK;
									progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
									curProg=NUM_CALLBACK;
									if(!(*callback)())
									{
										delete d;
										return IONCLIP_CALLBACK_FAIL;
									}
								}
							}
				
						}
						else
						{
							//Too close to the z-axis, rotation vector is unable to be stably computed,
							//and we don't need to rotate anyway
							for(vector<IonHit>::const_iterator it=((const IonStreamData *)dataIn[ui])->data.begin();
								       it!=((const IonStreamData *)dataIn[ui])->data.end(); ++it)
							{
								Point3D ptmp;
								ptmp=it->getPosRef()-vectorParams[0];
								
								//Keep ion if inside cylinder XOR inversion of the clippping (inside vs outside)
								if((ptmp[2] < halfLen && ptmp[2] > -halfLen && ptmp[0]*ptmp[0]+ptmp[1]*ptmp[1] < sqrRad)  ^ invertedClip)
										d->data.push_back(*it);

								//update progress every CALLBACK ions
								if(!curProg--)
								{
									n+=NUM_CALLBACK;
									progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
									curProg=NUM_CALLBACK;
									if(!(*callback)())
									{
										delete d;
										return IONCLIP_CALLBACK_FAIL;
									}
								}
							}
							
						}
						break;
					}
					case IONCLIP_PRIMITIVE_AAB:
					{
						//origin + corner
						ASSERT(vectorParams.size() == 2);
						ASSERT(scalarParams.size() == 0);
						unsigned int curProg=NUM_CALLBACK;

						BoundCube c;
						c.setBounds(vectorParams[0]+vectorParams[1],vectorParams[0]-vectorParams[1]);
						//Loop through each ion in the dataset
						for(vector<IonHit>::const_iterator it=((const IonStreamData *)dataIn[ui])->data.begin();
							       it!=((const IonStreamData *)dataIn[ui])->data.end(); ++it)
						{
							//Use XOR operand
							if((c.containsPt(it->getPosRef())) ^ (invertedClip))
								d->data.push_back(*it);
							
							//update progress every CALLBACK ions
							if(!curProg--)
							{
								n+=NUM_CALLBACK;
								progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
								curProg=NUM_CALLBACK;
								if(!(*callback)())
								{
									delete d;
									return IONCLIP_CALLBACK_FAIL;
								}
							}
						}
						break;
					}


					default:
						ASSERT(false);
						return 1;
				}


				//Copy over other attributes
				d->r = ((IonStreamData *)dataIn[ui])->r;
				d->g = ((IonStreamData *)dataIn[ui])->g;
				d->b =((IonStreamData *)dataIn[ui])->b;
				d->a =((IonStreamData *)dataIn[ui])->a;
				d->ionSize =((IonStreamData *)dataIn[ui])->ionSize;
				d->representationType=((IonStreamData *)dataIn[ui])->representationType;

				//getOut is const, so shouldn't be modified
				if(cache)
				{
					d->cached=1;
					filterOutputs.push_back(d);
					cacheOK=true;
				}
				else
					d->cached=0;
				

				getOut.push_back(d);
				d=0;
				break;
			}
			default:
				//Just copy across the ptr, if we are unfamiliar with this type
				getOut.push_back(dataIn[ui]);	
				break;
		}

	}
	}
	catch(std::bad_alloc)
	{
		if(d)
			delete d;
		return IONCLIP_BAD_ALLOC;
	}
	return 0;

}

//!Get the properties of the filter, in key-value form. First vector is for each output.
void IonClipFilter::getProperties(FilterProperties &propertyList) const
{
	propertyList.data.clear();
	propertyList.keys.clear();
	propertyList.types.clear();

	vector<unsigned int> type,keys;
	vector<pair<string,string> > s;


	string str;
	//Let the user know what the valid values for Primitive type
	string tmpChoice,tmpStr;


	vector<pair<unsigned int,string> > choices;

	choices.push_back(make_pair((unsigned int)PRIMITIVE_SPHERE ,
				primitiveStringFromID(PRIMITIVE_SPHERE)));
	choices.push_back(make_pair((unsigned int)PRIMITIVE_PLANE ,
				primitiveStringFromID(PRIMITIVE_PLANE)));
	choices.push_back(make_pair((unsigned int)PRIMITIVE_CYLINDER ,
				primitiveStringFromID(PRIMITIVE_CYLINDER)));
	choices.push_back(make_pair((unsigned int)PRIMITIVE_AAB,
				primitiveStringFromID(PRIMITIVE_AAB)));

	tmpStr= choiceString(choices,primitiveType);
	s.push_back(make_pair(string("Primitive"),tmpStr));
	type.push_back(PROPERTY_TYPE_CHOICE);
	keys.push_back(KEY_IONCLIP_PRIMITIVE_TYPE);
	
	stream_cast(str,showPrimitive);
	keys.push_back(KEY_IONCLIP_PRIMITIVE_SHOW);
	s.push_back(make_pair("Show Primitive", str));
	type.push_back(PROPERTY_TYPE_BOOL);
	
	stream_cast(str,invertedClip);
	keys.push_back(KEY_IONCLIP_PRIMITIVE_INVERTCLIP);
	s.push_back(make_pair("Invert Clip", str));
	type.push_back(PROPERTY_TYPE_BOOL);

	switch(primitiveType)
	{
		case IONCLIP_PRIMITIVE_SPHERE:
		{
			ASSERT(vectorParams.size() == 1);
			ASSERT(scalarParams.size() == 1);
			stream_cast(str,vectorParams[0]);
			keys.push_back(KEY_IONCLIP_ORIGIN);
			s.push_back(make_pair("Origin", str));
			type.push_back(PROPERTY_TYPE_POINT3D);
			
			stream_cast(str,scalarParams[0]);
			keys.push_back(KEY_IONCLIP_RADIUS);
			s.push_back(make_pair("Radius", str));
			type.push_back(PROPERTY_TYPE_REAL);

			break;
		}
		case IONCLIP_PRIMITIVE_PLANE:
		{
			ASSERT(vectorParams.size() == 2);
			ASSERT(scalarParams.size() == 0);
			stream_cast(str,vectorParams[0]);
			keys.push_back(KEY_IONCLIP_ORIGIN);
			s.push_back(make_pair("Origin", str));
			type.push_back(PROPERTY_TYPE_POINT3D);
			
			stream_cast(str,vectorParams[1]);
			keys.push_back(KEY_IONCLIP_NORMAL);
			s.push_back(make_pair("Plane Normal", str));
			type.push_back(PROPERTY_TYPE_POINT3D);

			break;
		}
		case IONCLIP_PRIMITIVE_CYLINDER:
		{
			ASSERT(vectorParams.size() == 2);
			ASSERT(scalarParams.size() == 1);
			stream_cast(str,vectorParams[0]);
			keys.push_back(KEY_IONCLIP_ORIGIN);
			s.push_back(make_pair("Origin", str));
			type.push_back(PROPERTY_TYPE_POINT3D);
			
			stream_cast(str,vectorParams[1]);
			keys.push_back(KEY_IONCLIP_NORMAL);
			s.push_back(make_pair("Axis", str));
			type.push_back(PROPERTY_TYPE_POINT3D);
			
			if(lockAxisMag)
				str="1";
			else
				str="0";
			keys.push_back(KEY_IONCLIP_AXIS_LOCKMAG);
			s.push_back(make_pair("Lock Axis Mag.", str));
			type.push_back(PROPERTY_TYPE_BOOL);

			stream_cast(str,scalarParams[0]);
			keys.push_back(KEY_IONCLIP_RADIUS);
			s.push_back(make_pair("Radius", str));
			type.push_back(PROPERTY_TYPE_POINT3D);
			break;
		}
		case IONCLIP_PRIMITIVE_AAB:
		{
			ASSERT(vectorParams.size() == 2);
			ASSERT(scalarParams.size() == 0);
			stream_cast(str,vectorParams[0]);
			keys.push_back(KEY_IONCLIP_ORIGIN);
			s.push_back(make_pair("Origin", str));
			type.push_back(PROPERTY_TYPE_POINT3D);
			
			stream_cast(str,vectorParams[1]);
			keys.push_back(KEY_IONCLIP_CORNER);
			s.push_back(make_pair("Corner offset", str));
			type.push_back(PROPERTY_TYPE_POINT3D);
			break;
		}
		default:
			ASSERT(false);
	}
	
	
	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);

	ASSERT(keys.size() == type.size());	
}

//!Set the properties for the nth filter. Returns true if prop set OK
bool IonClipFilter::setProperty(unsigned int set,unsigned int key, 
				const std::string &value, bool &needUpdate)
{

	needUpdate=false;
	ASSERT(set == 0);

	switch(key)
	{
		case KEY_IONCLIP_PRIMITIVE_TYPE:
		{
			unsigned int newPrimitive;

			newPrimitive=primitiveID(value);
			if(newPrimitive == (unsigned int)-1)
				return false;	

			primitiveType=newPrimitive;

			switch(primitiveType)
			{
				//If we are switchign between essentially
				//similar data types, don't reset the data.
				//Otherwise, wipe it and try again
				case IONCLIP_PRIMITIVE_SPHERE:
					if(vectorParams.size() !=1)
					{
						vectorParams.clear();
						vectorParams.push_back(Point3D(0,0,0));
					}
					if(scalarParams.size()!=1);
					{
						scalarParams.clear();
						scalarParams.push_back(10.0f);
					}
					break;
				case IONCLIP_PRIMITIVE_PLANE:
					if(vectorParams.size() >2)
					{
						vectorParams.clear();
						vectorParams.push_back(Point3D(0,0,0));
						vectorParams.push_back(Point3D(0,1,0));
					}
					else if (vectorParams.size() ==2)
					{
						vectorParams[1].normalise();
					}
					else if(vectorParams.size() ==1)
					{
						vectorParams.push_back(Point3D(0,1,0));
					}

					scalarParams.clear();
					break;
				case IONCLIP_PRIMITIVE_CYLINDER:
					if(vectorParams.size() >2)
					{
						vectorParams.clear();
						vectorParams.push_back(Point3D(0,0,0));
						vectorParams.push_back(Point3D(0,1,0));
					}
					else if(vectorParams.size() ==1)
					{
						vectorParams.push_back(Point3D(0,1,0));
					}

					if(scalarParams.size()!=1);
					{
						scalarParams.clear();
						scalarParams.push_back(10.0f);
					}
					break;
				case IONCLIP_PRIMITIVE_AAB:
				
					if(vectorParams.size() >2)
					{
						vectorParams.clear();
						vectorParams.push_back(Point3D(0,0,0));
						vectorParams.push_back(Point3D(1,1,1));
					}
					else if(vectorParams.size() ==1)
					{
						vectorParams.push_back(Point3D(1,1,1));
					}
					//check to see if any components of the
					//corner offset are zero; we disallow a
					//zero, 1 or 2 dimensional box
					for(unsigned int ui=0;ui<3;ui++)
					{
						vectorParams[1][ui]=fabs(vectorParams[1][ui]);
						if(vectorParams[1][ui] <std::numeric_limits<float>::epsilon())
							vectorParams[1][ui] = 1;
					}
					scalarParams.clear();
					break;

				default:
					ASSERT(false);
			}
	
			clearCache();	
			needUpdate=true;	
			return true;	
		}
		case KEY_IONCLIP_ORIGIN:
		{
			ASSERT(vectorParams.size() >= 1);
			Point3D newPt;
			if(!newPt.parse(value))
				return false;

			if(!(vectorParams[0] == newPt ))
			{
				vectorParams[0] = newPt;
				needUpdate=true;
				clearCache();
			}

			return true;
		}
		case KEY_IONCLIP_CORNER:
		{
			ASSERT(vectorParams.size() >= 2);
			Point3D newPt;
			if(!newPt.parse(value))
				return false;

			if(!(vectorParams[1] == newPt ))
			{
				vectorParams[1] = newPt;
				needUpdate=true;
				clearCache();
			}

			return true;
		}
		case KEY_IONCLIP_RADIUS:
		{
			ASSERT(scalarParams.size() >=1);
			float newRad;
			if(stream_cast(newRad,value))
				return false;

			if(scalarParams[0] != newRad )
			{
				scalarParams[0] = newRad;
				needUpdate=true;
				clearCache();
			}
			return true;
		}
		case KEY_IONCLIP_NORMAL:
		{
			ASSERT(vectorParams.size() >=2);
			Point3D newPt;
			if(!newPt.parse(value))
				return false;

			if(!(vectorParams[1] == newPt ))
			{
				vectorParams[1] = newPt;
				needUpdate=true;
				clearCache();
			}
			return true;
		}
		case KEY_IONCLIP_PRIMITIVE_SHOW:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			if(stripped=="1")
				showPrimitive=true;
			else
				showPrimitive=false;

			needUpdate=true;

			break;
		}
		case KEY_IONCLIP_PRIMITIVE_INVERTCLIP:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			bool lastVal=invertedClip;
			if(stripped=="1")
				invertedClip=true;
			else
				invertedClip=false;

			//if the result is different, the
			//cache should be invalidated
			if(lastVal!=invertedClip)
			{
				needUpdate=true;
				clearCache();
			}

			break;
		}

		case KEY_IONCLIP_AXIS_LOCKMAG:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			if(stripped=="1")
				lockAxisMag=true;
			else
				lockAxisMag=false;

			needUpdate=true;

			break;
		}
		default:
			ASSERT(false);
			return false;
	}
	

	return true;
}

//!Get the human readable error string associated with a particular error code during refresh(...)
std::string IonClipFilter::getErrString(unsigned int code) const
{
	switch(code)
	{
		case IONCLIP_BAD_ALLOC:
			return std::string("Insufficient mem. for Ionclip");
		case IONCLIP_CALLBACK_FAIL:
			return std::string("Ionclip Aborted");
	}
	ASSERT(false);
}

bool IonClipFilter::writeState(std::ofstream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<ionclip>" << endl;
			f << tabs(depth+1) << "<userstring value=\""<<userString << "\"/>"  << endl;

			f << tabs(depth+1) << "<primitivetype value=\"" << primitiveType<< "\"/>" << endl;
			f << tabs(depth+1) << "<invertedclip value=\"" << invertedClip << "\"/>" << endl;
			f << tabs(depth+1) << "<showprimitive value=\"" << showPrimitive << "\"/>" << endl;
			f << tabs(depth+1) << "<lockaxismag value=\"" << lockAxisMag<< "\"/>" << endl;
			f << tabs(depth+1) << "<vectorparams>" << endl;
			for(unsigned int ui=0; ui<vectorParams.size(); ui++)
			{
				f << tabs(depth+2) << "<point3d x=\"" << vectorParams[ui][0] << 
					"\" y=\"" << vectorParams[ui][1] << "\" z=\"" << vectorParams[ui][2] << "\"/>" << endl;
			}
			f << tabs(depth+1) << "</vectorparams>" << endl;

			f << tabs(depth+1) << "<scalarparams>" << endl;
			for(unsigned int ui=0; ui<scalarParams.size(); ui++)
				f << tabs(depth+2) << "<scalar value=\"" << scalarParams[0] << "\"/>" << endl; 
			
			f << tabs(depth+1) << "</scalarparams>" << endl;
			f << tabs(depth) << "</ionclip>" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool IonClipFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
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

	std::string tmpStr;	
	//Retrieve primitive type 
	//====
	if(!XMLGetNextElemAttrib(nodePtr,primitiveType,"primitivetype","value"))
		return false;
	if(primitiveType >= IONCLIP_PRIMITIVE_END)
	       return false;	
	//====
	
	std::string tmpString;
	//Retrieve clip inversion
	//====
	//
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"invertedclip","value"))
		return false;
	if(tmpStr == "0")
		invertedClip=false;
	else if(tmpStr == "1")
		invertedClip=true;
	else
		return false;
	//====
	
	//Retrieve primitive visiblity 
	//====
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"showprimitive","value"))
		return false;
	if(tmpStr == "0")
		showPrimitive=false;
	else if(tmpStr == "1")
		showPrimitive=true;
	else
		return false;
	//====
	
	//Retrieve axis lock mode 
	//====
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"lockaxismag","value"))
		return false;
	if(tmpStr == "0")
		lockAxisMag=false;
	else if(tmpStr == "1")
		lockAxisMag=true;
	else
		return false;
	//====
	
	//Retreive vector parameters
	//===
	if(XMLHelpFwdToElem(nodePtr,"vectorparams"))
		return false;
	xmlNodePtr tmpNode=nodePtr;

	nodePtr=nodePtr->xmlChildrenNode;

	vectorParams.clear();
	while(!XMLHelpFwdToElem(nodePtr,"point3d"))
	{
		float x,y,z;
		//--Get X value--
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"x");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;
		xmlFree(xmlString);

		//Check it is streamable
		if(stream_cast(x,tmpStr))
			return false;

		//--Get Z value--
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"y");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;
		xmlFree(xmlString);

		//Check it is streamable
		if(stream_cast(y,tmpStr))
			return false;

		//--Get Y value--
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"z");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;
		xmlFree(xmlString);

		//Check it is streamable
		if(stream_cast(z,tmpStr))
			return false;

		vectorParams.push_back(Point3D(x,y,z));
	}
	//===	

	nodePtr=tmpNode;
	//Retreive scalar parameters
	//===
	if(XMLHelpFwdToElem(nodePtr,"scalarparams"))
		return false;
	
	tmpNode=nodePtr;
	nodePtr=nodePtr->xmlChildrenNode;

	scalarParams.clear();
	while(!XMLHelpFwdToElem(nodePtr,"scalar"))
	{
		float v;
		//Get value
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;
		xmlFree(xmlString);

		//Check it is streamable
		if(stream_cast(v,tmpStr))
			return false;
		scalarParams.push_back(v);
	}
	//===	

	//Check the scalar params match the selected primitive	
	switch(primitiveType)
	{
		case IONCLIP_PRIMITIVE_SPHERE:
			if(vectorParams.size() != 1 || scalarParams.size() !=1)
				return false;
			break;
		case IONCLIP_PRIMITIVE_PLANE:
		case IONCLIP_PRIMITIVE_AAB:
			if(vectorParams.size() != 2 || scalarParams.size() !=0)
				return false;
			break;
		case IONCLIP_PRIMITIVE_CYLINDER:
			if(vectorParams.size() != 2 || scalarParams.size() !=1)
				return false;
			break;

		default:
			ASSERT(false);
			return false;
	}
	return true;
}

void IonClipFilter::setPropFromBinding(const SelectionBinding &b)
{
	switch(b.getID())
	{
		case BINDING_CYLINDER_RADIUS:
		case BINDING_SPHERE_RADIUS:
			b.getValue(scalarParams[0]);
			break;
		case BINDING_CYLINDER_ORIGIN:
		case BINDING_SPHERE_ORIGIN:
		case BINDING_PLANE_ORIGIN:
		case BINDING_RECT_TRANSLATE:
			b.getValue(vectorParams[0]);
			break;
		case BINDING_CYLINDER_DIRECTION:
			b.getValue(vectorParams[1]);
			break;
		case BINDING_PLANE_DIRECTION:
		{
			Point3D p;
			b.getValue(p);
			p.normalise();

			vectorParams[1] =p;
			break;
		}
		case BINDING_RECT_CORNER_MOVE:
		{
			//Prevent the corner offset from acquiring a vector
			//with a negative component.
			Point3D p;
			b.getValue(p);
			for(unsigned int ui=0;ui<3;ui++)
			{
				p[ui]=fabs(p[ui]);
				//Should be positive
				if(p[ui] <std::numeric_limits<float>::epsilon())
					return;
			}

			vectorParams[1]=p;
			break;
		}
		default:
			ASSERT(false);
	}
	clearCache();
}

// == Ion Colouring filter ==

IonColourFilter::IonColourFilter()
{
	colourMap=0;
	showColourBar=true;
	mapBounds[0] = 0.0f;
	mapBounds[1] = 100.0f;
	nColours=MAX_NUM_COLOURS; //Full 8 bit indexed colour. very swish

	cacheOK=false;
	cache=true; //By default, we should cache, but decision is made higher up

}

Filter *IonColourFilter::cloneUncached() const
{
	IonColourFilter *p=new IonColourFilter();
	p->colourMap = colourMap;
	p->mapBounds[0]=mapBounds[0];
	p->mapBounds[1]=mapBounds[1];
	p->nColours =nColours;	
	p->showColourBar =showColourBar;	
	
	//We are copying wether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

size_t IonColourFilter::numBytesForCache(size_t nObjects) const
{
		return (size_t)((float)(nObjects*sizeof(IONHIT)));
}

DrawColourBarOverlay *IonColourFilter::makeColourBar() const
{
	//If we have ions, then we should draw a colour bar
	//Set up the colour bar. Place it in a draw stream type
	DrawColourBarOverlay *dc = new DrawColourBarOverlay;

	vector<float> r,g,b;
	r.resize(nColours);
	g.resize(nColours);
	b.resize(nColours);

	for (unsigned int ui=0;ui<nColours;ui++)
	{
		unsigned char rgb[3]; //RGB array
		float value;
		value = (float)(ui)*(mapBounds[1]-mapBounds[0])/(float)nColours + mapBounds[0];
		//Pick the desired colour map
		colourMapWrap(colourMap,rgb,value,mapBounds[0],mapBounds[1]);
		r[ui]=rgb[0]/255.0f;
		g[ui]=rgb[1]/255.0f;
		b[ui]=rgb[2]/255.0f;
	}

	dc->setColourVec(r,g,b);

	dc->setSize(0.08,0.6);
	dc->setPosition(0.1,0.1);
	dc->setMinMax(mapBounds[0],mapBounds[1]);


	return dc;
}


unsigned int IonColourFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(void))
{
	//use the cached copy if we have it.
	if(cacheOK)
	{
		ASSERT(filterOutputs.size());
		for(unsigned int ui=0;ui<dataIn.size();ui++)
		{
			if(dataIn[ui]->getStreamType() != STREAM_TYPE_IONS)
				getOut.push_back(dataIn[ui]);
		}

		for(unsigned int ui=0;ui<filterOutputs.size();ui++)
			getOut.push_back(filterOutputs[ui]);

		if(filterOutputs.size() && showColourBar)
		{
			DrawStreamData *d = new DrawStreamData;
			d->drawables.push_back(makeColourBar());
			d->cached=false;
			getOut.push_back(d);
		}
		return 0;
	}


	ASSERT(nColours >0 && nColours<=MAX_NUM_COLOURS);
	IonStreamData *d[nColours];
	unsigned char rgb[3]; //RGB array
	//Build the colourmap values, each as a unique filter output
	for(unsigned int ui=0;ui<nColours; ui++)
	{
		d[ui]=new IonStreamData;
		float value;
		value = (float)ui*(mapBounds[1]-mapBounds[0])/(float)nColours + mapBounds[0];
		//Pick the desired colour map
		colourMapWrap(colourMap,rgb,value,mapBounds[0],mapBounds[1]);
	
		d[ui]->r=rgb[0]/255.0f;
		d[ui]->g=rgb[1]/255.0f;
		d[ui]->b=rgb[2]/255.0f;
		d[ui]->a=1.0f;
	}
	
	//Try to maintain ion size if possible
	bool haveIonSize,sameSize; // have we set the ionSize?
	float ionSize;
	haveIonSize=false;
	sameSize=true;

	//Did we find any ions in this pass?
	bool foundIons=false;	
	unsigned int totalSize=numElements(dataIn);
	unsigned int curProg=NUM_CALLBACK;
	size_t n=0;
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS: 
			{
				foundIons=true;

				//Check for ion size consistency	
				if(haveIonSize)
				{
					sameSize &= (fabs(ionSize-((const IonStreamData *)dataIn[ui])->ionSize) 
									< std::numeric_limits<float>::epsilon());
				}
				else
				{
					ionSize=((const IonStreamData *)dataIn[ui])->ionSize;
					haveIonSize=true;
				}
				for(vector<IonHit>::const_iterator it=((const IonStreamData *)dataIn[ui])->data.begin();
					       it!=((const IonStreamData *)dataIn[ui])->data.end(); ++it)
				{
					unsigned int colour;

					float tmp;	
					tmp= (it->getMassToCharge()-mapBounds[0])/(mapBounds[1]-mapBounds[0]);
					tmp = std::max(0.0f,tmp);
					tmp = std::min(tmp,1.0f);
					
					colour=(unsigned int)(tmp*(float)(nColours-1));	
					d[colour]->data.push_back(*it);
				
					//update progress every CALLBACK ions
					if(!curProg--)
					{
						n+=NUM_CALLBACK;
						progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
						curProg=NUM_CALLBACK;
						if(!(*callback)())
						{
							for(unsigned int ui=0;ui<nColours;ui++)
								delete d[ui];
							return IONDOWNSAMPLE_ABORT_ERR;
						}
					}
				}

				
				break;
			}
			default:
				getOut.push_back(dataIn[ui]);

		}
	}

	//create the colour bar as needd
	if(foundIons && showColourBar)
	{
		DrawStreamData *d = new DrawStreamData;
		d->drawables.push_back(makeColourBar());
		d->cached=false;
		getOut.push_back(d);
	}


	//If all the ions are the same size, then propagate
	if(haveIonSize && sameSize)
	{
		for(unsigned int ui=0;ui<nColours;ui++)
			d[ui]->ionSize=ionSize;
	}
	//merge the results as needed
	if(cache)
	{
		for(unsigned int ui=0;ui<nColours;ui++)
		{
			d[ui]->cached=true;
			filterOutputs.push_back(d[ui]);
		}
		cacheOK=true;
	}
	else
	{
		for(unsigned int ui=0;ui<nColours;ui++)
		{
			//NOTE: MUST set cached BEFORE push_back!
			d[ui]->cached=false;
		}
		cacheOK=false;
	}

	//push the colours onto the output. cached or not (their status is set above).
	for(unsigned int ui=0;ui<nColours;ui++)
		getOut.push_back(d[ui]);
	
	return 0;
}


void IonColourFilter::getProperties(FilterProperties &propertyList) const
{
	propertyList.data.clear();
	propertyList.keys.clear();
	propertyList.types.clear();

	vector<unsigned int> type,keys;
	vector<pair<string,string> > s;

	string str,tmpStr;


	stream_cast(str,NUM_COLOURMAPS);
	str =string("ColourMap (0-") + str + ")";
	stream_cast(tmpStr,colourMap);

	s.push_back(make_pair(str,tmpStr));
	keys.push_back(KEY_IONCOLOURFILTER_COLOURMAP);
	type.push_back(PROPERTY_TYPE_INTEGER);

	if(showColourBar)
		tmpStr="1";
	else
		tmpStr="0";
	str =string("Show Bar");
	s.push_back(make_pair(str,tmpStr));
	keys.push_back(KEY_IONCOLOURFILTER_SHOWBAR);
	type.push_back(PROPERTY_TYPE_BOOL);


	str = "Num Colours";	
	stream_cast(tmpStr,nColours);
	s.push_back(make_pair(str,tmpStr));
	keys.push_back(KEY_IONCOLOURFILTER_NCOLOURS);
	type.push_back(PROPERTY_TYPE_INTEGER);

	stream_cast(tmpStr,mapBounds[0]);
	s.push_back(make_pair("Map start", tmpStr));
	keys.push_back(KEY_IONCOLOURFILTER_MAPSTART);
	type.push_back(PROPERTY_TYPE_REAL);

	stream_cast(tmpStr,mapBounds[1]);
	s.push_back(make_pair("Map end", tmpStr));
	keys.push_back(KEY_IONCOLOURFILTER_MAPEND);
	type.push_back(PROPERTY_TYPE_REAL);


	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);
}

bool IonColourFilter::setProperty( unsigned int set, unsigned int key,
					const std::string &value, bool &needUpdate)
{
	ASSERT(!set);

	needUpdate=false;
	switch(key)
	{
		case KEY_IONCOLOURFILTER_COLOURMAP:
		{
			unsigned int tmpMap;
			stream_cast(tmpMap,value);

			if(tmpMap > NUM_COLOURMAPS || tmpMap ==colourMap)
				return false;

			clearCache();
			needUpdate=true;
			colourMap=tmpMap;
			break;
		}
		case KEY_IONCOLOURFILTER_MAPSTART:
		{
			float tmpBound;
			stream_cast(tmpBound,value);
			if(tmpBound >=mapBounds[1])
				return false;

			clearCache();
			needUpdate=true;
			mapBounds[0]=tmpBound;
			break;
		}
		case KEY_IONCOLOURFILTER_MAPEND:
		{
			float tmpBound;
			stream_cast(tmpBound,value);
			if(tmpBound <=mapBounds[0])
				return false;

			clearCache();
			needUpdate=true;
			mapBounds[1]=tmpBound;
			break;
		}
		case KEY_IONCOLOURFILTER_NCOLOURS:
		{
			unsigned int numColours;
			if(stream_cast(numColours,value))
				return false;

			clearCache();
			needUpdate=true;
			//enforce 1->MAX_NUM_COLOURS range
			nColours=std::min(numColours,MAX_NUM_COLOURS);
			if(!nColours)
				nColours=1;
			break;
		}
		case KEY_IONCOLOURFILTER_SHOWBAR:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			bool lastVal=showColourBar;
			if(stripped=="1")
				showColourBar=true;
			else
				showColourBar=false;

			//Only need update if changed
			if(lastVal!=showColourBar)
				needUpdate=true;
			break;
		}	

		default:
			ASSERT(false);
	}	
	return true;
}


std::string  IonColourFilter::getErrString(unsigned int code) const
{
	//Currently the only error is aborting
	return std::string("Aborted");
}


bool IonColourFilter::writeState(std::ofstream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<ioncolour>" << endl;
			f << tabs(depth+1) << "<userstring value=\""<<userString << "\"/>"  << endl;

			f << tabs(depth+1) << "<colourmap value=\"" << colourMap << "\"/>" << endl;
			f << tabs(depth+1) << "<extrema min=\"" << mapBounds[0] << "\" max=\"" 
				<< mapBounds[1] << "\"/>" << endl;
			f << tabs(depth+1) << "<ncolours value=\"" << nColours << "\"/>" << endl;

			string str;
			if(showColourBar)
				str="1";
			else
				str="0";
			f << tabs(depth+1) << "<showcolourbar value=\"" << str << "\"/>" << endl;
			
			f << tabs(depth) << "</ioncolour>" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool IonColourFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
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

	std::string tmpStr;	
	//Retrieve colourmap
	//====
	if(XMLHelpFwdToElem(nodePtr,"colourmap"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(colourMap,tmpStr))
		return false;

	if(colourMap>= NUM_COLOURMAPS)
	       return false;	
	xmlFree(xmlString);
	//====
	
	//Retrieve Extrema 
	//===
	float tmpMin,tmpMax;
	if(XMLHelpFwdToElem(nodePtr,"extrema"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"min");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(tmpMin,tmpStr))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"max");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(tmpMax,tmpStr))
		return false;

	xmlFree(xmlString);

	if(tmpMin > tmpMax)
		return false;

	mapBounds[0]=tmpMin;
	mapBounds[1]=tmpMax;

	//===
	
	//Retrieve num colours 
	//====
	if(XMLHelpFwdToElem(nodePtr,"ncolours"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(nColours,tmpStr))
		return false;

	xmlFree(xmlString);
	//====
	
	//Retrieve num colours 
	//====
	if(XMLHelpFwdToElem(nodePtr,"showcolourbar"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(showColourBar,tmpStr))
		return false;

	xmlFree(xmlString);
	//====
	return true;
}

CompositionProfileFilter::CompositionProfileFilter()
{
	binWidth=0.5;
	plotType=0;
	fixedBins=0;
	nBins=1000;
	normalise=1;
	errMode.mode=PLOT_ERROR_NONE;
	errMode.movingAverageNum=4;
	lockAxisMag=false;
	//Default to blue plot
	r=g=0;
	b=a=1;
	
	primitiveType=COMPPROFILE_PRIMITIVE_CYLINDER;
	vectorParams.push_back(Point3D(0.0,0.0,0.0));
	vectorParams.push_back(Point3D(0,20.0,0.0));
	scalarParams.push_back(5.0);

	showPrimitive=true;
}


void CompositionProfileFilter::binIon(unsigned int targetBin, const RangeStreamData* rng, 
	const map<unsigned int,unsigned int> &ionIDMapping,
	vector<vector<size_t> > &frequencyTable, float massToCharge) const
{
	//if we have no range data, then simply increment its position in a 1D table
	//which will later be used as "count" data (like some kind of density plot)
	if(!rng)
	{
		ASSERT(frequencyTable.size() == 1);
		//There is a really annoying numerical boundary case
		//that makes the target bin equate to the table size. 
		//disallow this.
		if(targetBin < frequencyTable[0].size())
			frequencyTable[0][targetBin]++;
		return;
	}


	//We have range data, we need to use it to classify the ion and then increment
	//the appropriate position in the table
	unsigned int rangeID = rng->rangeFile->getRangeID(massToCharge);

	if(rangeID != (unsigned int)(-1) && rng->enabledRanges[rangeID])
	{
		unsigned int ionID=rng->rangeFile->getIonID(rangeID); 
		unsigned int pos;
		pos = ionIDMapping.find(ionID)->second;
		frequencyTable[pos][targetBin]++;
	}
}


Filter *CompositionProfileFilter::cloneUncached() const
{
	CompositionProfileFilter *p = new CompositionProfileFilter();

	p->primitiveType=primitiveType;
	p->showPrimitive=showPrimitive;
	p->vectorParams.resize(vectorParams.size());
	p->scalarParams.resize(scalarParams.size());

	std::copy(vectorParams.begin(),vectorParams.end(),p->vectorParams.begin());
	std::copy(scalarParams.begin(),scalarParams.end(),p->scalarParams.begin());

	p->normalise=normalise;	
	p->stepMode=stepMode;	
	p->fixedBins=fixedBins;
	p->lockAxisMag=lockAxisMag;

	p->binWidth=binWidth;
	p->nBins = nBins;
	p->r=r;	
	p->g=g;	
	p->b=b;	
	p->a=a;	
	p->plotType=plotType;
	p->errMode=errMode;
	//We are copying wether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

void CompositionProfileFilter::initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut)
{
	//Check for range file parent
	for(unsigned int ui=0;ui<dataIn.size();ui++)
	{
		if(dataIn[ui]->getStreamType() == STREAM_TYPE_RANGE)
		{
			haveRangeParent=true;
			return;
		}
	}
	haveRangeParent=false;
}

unsigned int CompositionProfileFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
			std::vector<const FilterStreamData *> &getOut, ProgressData &progress, 
								bool (*callback)(void))
{
	//Clear selection devices
	devices.clear();
	
	if(showPrimitive)
	{
		//construct a new primitive, do not cache
		DrawStreamData *drawData=new DrawStreamData;
		switch(primitiveType)
		{
			case COMPPROFILE_PRIMITIVE_CYLINDER:
			{
				//Origin + normal
				ASSERT(vectorParams.size() == 2);
				//Add drawable components
				DrawCylinder *dC = new DrawCylinder;
				dC->setOrigin(vectorParams[0]);
				dC->setRadius(scalarParams[0]);
				dC->setColour(0.5,0.5,0.5,0.3);
				dC->setSlices(40);
				dC->setLength(sqrt(vectorParams[1].sqrMag())*2.0f);
				dC->setDirection(vectorParams[1]);
				dC->wantsLight=true;
				drawData->drawables.push_back(dC);
				
					
				//Set up selection "device" for user interaction
				//====
				//The object is selectable
				dC->canSelect=true;
				//Start and end radii must be the same (not a
				//tapered cylinder)
				dC->lockRadii();

				SelectionDevice *s = new SelectionDevice(this);
				SelectionBinding b;
				//Bind the drawable object to the properties we wish
				//to be able to modify

				//Bind left + command button to move
				b.setBinding(SELECT_BUTTON_LEFT,FLAG_CMD,DRAW_CYLINDER_BIND_ORIGIN,
					BINDING_CYLINDER_ORIGIN,dC->getOrigin(),dC);	
				b.setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
				s->addBinding(b);

				//Bind left + shift to change orientation
				b.setBinding(SELECT_BUTTON_LEFT,FLAG_SHIFT,DRAW_CYLINDER_BIND_DIRECTION,
					BINDING_CYLINDER_DIRECTION,dC->getDirection(),dC);	
				if(lockAxisMag)
					b.setInteractionMode(BIND_MODE_POINT3D_ROTATE_LOCK);
				else
					b.setInteractionMode(BIND_MODE_POINT3D_ROTATE);
				s->addBinding(b);

				//Bind right button to changing position 
				b.setBinding(SELECT_BUTTON_RIGHT,0,DRAW_CYLINDER_BIND_ORIGIN,
					BINDING_CYLINDER_ORIGIN,dC->getOrigin(),dC);	
				b.setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
				s->addBinding(b);
					
				//Bind middle button to changing orientation
				b.setBinding(SELECT_BUTTON_MIDDLE,0,DRAW_CYLINDER_BIND_DIRECTION,
					BINDING_CYLINDER_DIRECTION,dC->getDirection(),dC);	
				if(lockAxisMag)
					b.setInteractionMode(BIND_MODE_POINT3D_ROTATE_LOCK);
				else
					b.setInteractionMode(BIND_MODE_POINT3D_ROTATE);
				s->addBinding(b);
					
				//Bind left button to changing radius
				b.setBinding(SELECT_BUTTON_LEFT,0,DRAW_CYLINDER_BIND_RADIUS,
					BINDING_CYLINDER_RADIUS,dC->getRadius(),dC);
				b.setInteractionMode(BIND_MODE_FLOAT_TRANSLATE);
				b.setFloatLimits(0,std::numeric_limits<float>::max());
				s->addBinding(b); 
				
				devices.push_back(s);
				//=====
				
				break;
			}
			default:
				ASSERT(false);
		}
		drawData->cached=false;	
		getOut.push_back(drawData);
	}


	//use the cached copy of the data if we have it.
	if(cacheOK)
	{
		//proagate our cached plot data.
		for(unsigned int ui=0;ui<filterOutputs.size();ui++)
			getOut.push_back(filterOutputs[ui]);

		ASSERT(filterOutputs.back()->getStreamType() == STREAM_TYPE_PLOT);

		//Propagate all the incoming data (including ions)
		for(unsigned int ui=0;ui<dataIn.size() ;ui++)
		{
			if(dataIn[ui]->getStreamType() != STREAM_TYPE_IONS)
				getOut.push_back(dataIn[ui]);
		}
			
		return 0;
	}

	//Ion Frequences (composition specific if rangefile present)
	vector<vector<size_t> > ionFrequencies;
	
	RangeStreamData *rngData=0;
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		if(dataIn[ui]->getStreamType() == STREAM_TYPE_RANGE)
		{
			rngData =((RangeStreamData *)dataIn[ui]);
			break;
		}
	}

	//Number of bins, having determined if we are using
	//fixed bin count or not
	unsigned int numBins;

	if(fixedBins)
		numBins=nBins;
	else
	{
		switch(primitiveType)
		{
			case COMPPROFILE_PRIMITIVE_CYLINDER:
			{
				//length of cylinder (as axis starts in cylinder middle)
				float length;
				length=sqrt(vectorParams[1].sqrMag())*2.0f;

				ASSERT(binWidth > std::numeric_limits<float>::epsilon());

				//Check for possible overflow
				if(length/binWidth > (float)std::numeric_limits<unsigned int>::max())
					return COMPPROFILE_ERR_NUMBINS;

				numBins=(unsigned int)(length/binWidth);
				break;
			}
			default:
				ASSERT(false);
		}
		
	}

	//Indirection vector to convert ionFrequencies position to ionID mapping.
	//Should only be used in conjunction with rngData == true
	std::map<unsigned int,unsigned int> ionIDMapping,inverseIDMapping;
	//Allocate space for the frequency table
	if(rngData)
	{
		ASSERT(rngData->rangeFile);
		unsigned int enabledCount=0;
		for(unsigned int ui=0;ui<rngData->rangeFile->getNumIons();ui++)
		{
			//TODO: Might be nice to detect if an ions ranges
			//are all, disabled then if they are, enter this "if"
			//anyway
			if(rngData->enabledIons[ui])
			{
				//Keep the forwards mapping for binning
				ionIDMapping.insert(make_pair(ui,enabledCount));
				//Keep the inverse mapping for labelling
				inverseIDMapping.insert(make_pair(enabledCount,ui));
				enabledCount++;
			}

		

		}

		//Nothing to do.
		if(!enabledCount)
			return 0;

		try
		{
			ionFrequencies.resize(enabledCount);
			//Allocate and Initialise all elements to zero
			for(unsigned int ui=0;ui<ionFrequencies.size(); ui++)
				ionFrequencies[ui].resize(numBins,0);
		}
		catch(std::bad_alloc)
		{
			return COMPPROFILE_ERR_MEMALLOC;
		}

	}
	else
	{
		try
		{
			ionFrequencies.resize(1);
			ionFrequencies[0].resize(numBins,0);
		}
		catch(std::bad_alloc)
		{
			return COMPPROFILE_ERR_MEMALLOC;
		}
	}


	size_t n=0;
	size_t totalSize=numElements(dataIn);
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		//Loop through each data set
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS:
			{

				switch(primitiveType)
				{
					case COMPPROFILE_PRIMITIVE_CYLINDER:
					{
						//Origin + axis
						ASSERT(vectorParams.size() == 2);
						//Radius perp. to axis
						ASSERT(scalarParams.size() == 1);

						unsigned int curProg=NUM_CALLBACK;
						Point3f rotVec;
						//Cross product desired drection with default
						//direction to produce rotation vector
						Point3D dir(0.0f,0.0f,1.0f),direction;
						direction=vectorParams[1];
						direction.normalise();

						float angle = dir.angle(direction);

						float halfLen=sqrt(vectorParams[1].sqrMag())/2.0f;
						float sqrRad=scalarParams[0]*scalarParams[0];
				
						//Check that we actually need to rotate, to avoid numerical singularity 
						//when cylinder axis is too close to (or is) z-axis
						if(angle > sqrt(std::numeric_limits<float>::epsilon()))
						{
							dir = dir.crossProd(direction);
							dir.normalise();

							rotVec.fx=dir[0];
							rotVec.fy=dir[1];
							rotVec.fz=dir[2];

							Quaternion q1;

							//Generate the rotating quaternions
							quat_get_rot_quat(&rotVec,-angle,&q1);


						
							//pre-compute cylinder length and radius^2
							//Loop through each ion in the dataset
							for(vector<IonHit>::const_iterator it=((const IonStreamData *)dataIn[ui])->data.begin();
								       it!=((const IonStreamData *)dataIn[ui])->data.end(); ++it)
							{
								Point3f p;
								//Translate to get position w respect to cylinder centre
								Point3D ptmp;
								ptmp=it->getPosRef()-vectorParams[0];
								p.fx=ptmp[0];
								p.fy=ptmp[1];
								p.fz=ptmp[2];
								//rotate ion position into cylindrical coordinates
								quat_rot_apply_quat(&p,&q1);

								//Keep ion if inside cylinder XOR inversion of the clippping (inside vs outside)
								if((p.fz < halfLen && p.fz > -halfLen && p.fx*p.fx+p.fy*p.fy < sqrRad))  
								{
									//Figure out where inside the cylinder the 
									//data lies. Then push it into the correct bin.
									unsigned int targetBin;
									targetBin =  (unsigned int)((float)numBins*(float)(p.fz + halfLen)/(2.0f*halfLen));
								
									binIon(targetBin,rngData,ionIDMapping,ionFrequencies,
											it->getMassToCharge());
								}

								//update progress every CALLBACK ions
								if(!curProg--)
								{
									n+=NUM_CALLBACK;
									progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
									curProg=NUM_CALLBACK;
									if(!(*callback)())
										return IONCLIP_CALLBACK_FAIL;
								}
							}
				
						}
						else
						{
							//Too close to the z-axis, rotation vector is unable to be stably computed,
							//and we don't need to rotate anyway
							for(vector<IonHit>::const_iterator it=((const IonStreamData *)dataIn[ui])->data.begin();
								       it!=((const IonStreamData *)dataIn[ui])->data.end(); ++it)
							{
								Point3D ptmp;
								ptmp=it->getPosRef()-vectorParams[0];
								
								//Keep ion if inside cylinder XOR inversion of the clippping (inside vs outside)
								if((ptmp[2] < halfLen && ptmp[2] > -halfLen && ptmp[0]*ptmp[0]+ptmp[1]*ptmp[1] < sqrRad))
								{
									//Figure out where inside the cylinder the 
									//data lies. Then push it into the correct bin.
									unsigned int targetBin;
									targetBin =  (unsigned int)((float)numBins*(float)(ptmp[2]+ halfLen)/(2.0f*halfLen));								
									binIon(targetBin,rngData,ionIDMapping,ionFrequencies,
											it->getMassToCharge());
								}

								//update progress every CALLBACK ions
								if(!curProg--)
								{
									n+=NUM_CALLBACK;
									progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
									curProg=NUM_CALLBACK;
									if(!(*callback)())
										return IONCLIP_CALLBACK_FAIL;
								}
							}
							
						}
						break;
					}
				}
				break;
			}
			default:
				//Do not propagate other types.
				break;
		}
				
	}

	PlotStreamData *plotData[ionFrequencies.size()];
	float length;
	length=sqrt(vectorParams[1].sqrMag())*2.0f;


	float normFactor=1.0f;
	for(unsigned int ui=0;ui<ionFrequencies.size();ui++)
	{
		plotData[ui] = new PlotStreamData;

		plotData[ui]->index=ui;
		plotData[ui]->parent=this;
		plotData[ui]->xLabel= "Distance";
		plotData[ui]->errDat=errMode;
		if(normalise)
		{
			//If we have composition, normalise against 
			//sum composition = 1 otherwise use volume of bin
			//as normalisation factor
			if(rngData)
				plotData[ui]->yLabel= "Fraction";
			else
				plotData[ui]->yLabel= "Density (\\#.len^3)";
		}
		else
			plotData[ui]->yLabel= "Count";

		//Give the plot a title like "Myplot:Mg" (if have range) or "MyPlot" (no range)
		if(rngData)
		{
			unsigned int thisIonID;
			thisIonID = inverseIDMapping.find(ui)->second;
			plotData[ui]->dataLabel = getUserString() + string(":") 
					+ rngData->rangeFile->getName(thisIonID);

		
			//Set the plot colour to the ion colour	
			RGB col;
			col=rngData->rangeFile->getColour(thisIonID);

			plotData[ui]->r =col.red;
			plotData[ui]->g =col.green;
			plotData[ui]->b =col.blue;

		}
		else
		{
			//If it only has one component, then 
			//it's not really a composition profile is it?
			plotData[ui]->dataLabel= "Freq. Profile";
			plotData[ui]->r = r;
			plotData[ui]->g = g;
			plotData[ui]->b = b;
			plotData[ui]->a = a;
		}

		plotData[ui]->xyData.resize(ionFrequencies[ui].size());
		for(unsigned int uj=0;uj<ionFrequencies[ui].size(); uj++)
		{
			float xPos;
			xPos = ((float)uj/(float)ionFrequencies[ui].size())*length;
			if(rngData)
			{
				//Composition profile: do inter bin normalisation
				
				//Recompute normalisation value for this bin
				if(normalise)
				{
					float sum;
					sum=0;

					//Loop across each bin type, summing result
					for(unsigned int uk=0;uk<ionFrequencies.size();uk++)
						sum +=(float)ionFrequencies[uk][uj];

					if(sum)
						normFactor=1.0/sum;
				}

				plotData[ui]->xyData[uj] = std::make_pair(xPos,normFactor*(float)ionFrequencies[ui][uj]);
			}
			else
			{
				if(normalise)
				{
					//This is a frequency profile. Normalising across bins would lead to every value
					//being equal to 1. Let us instead normalise by cylinder section volume)
					normFactor = 1.0/(M_PI*scalarParams[0]*scalarParams[0]*binWidth);


				}
				//Normalise Y value against volume of bin
				plotData[ui]->xyData[uj] = std::make_pair(
					xPos,normFactor*(float)ionFrequencies[ui][uj]);

			}
		}

		if(cache)
		{
			plotData[ui]->cached=1;
			filterOutputs.push_back(plotData[ui]);	
		}
		else
			plotData[ui]->cached=0;

		plotData[ui]->plotType = plotType;
		getOut.push_back(plotData[ui]);
	}

	cacheOK=cache;
	return 0;
}

std::string  CompositionProfileFilter::getErrString(unsigned int code) const
{
	switch(code)
	{
		case COMPPROFILE_ERR_NUMBINS:
			return std::string("Too many bins in comp. profile.");
		case COMPPROFILE_ERR_MEMALLOC:
			return std::string("Note enough memory for comp. profile.");
		case COMPPROFILE_ERR_ABORT:
			return std::string("Aborted composition prof.");
	}
	return std::string("BUG: (CompositionProfileFilter::getErrString) Shouldn't see this!");
}

bool CompositionProfileFilter::setProperty(unsigned int set, unsigned int key, 
					const std::string &value, bool &needUpdate) 
{

	switch(set)
	{
		case 0://Primitive settings
		{
			
			switch(key)
			{
				case KEY_COMPPROFILE_BINWIDTH:
				{
					float newBinWidth;
					if(stream_cast(newBinWidth,value))
						return false;

					if(newBinWidth < sqrt(std::numeric_limits<float>::epsilon()))
						return false;

					binWidth=newBinWidth;
					clearCache();
					needUpdate=true;
					break;
				}
				case KEY_COMPPROFILE_FIXEDBINS:
				{
					unsigned int valueInt;
					if(stream_cast(valueInt,value))
						return false;

					if(valueInt ==0 || valueInt == 1)
					{
						if(fixedBins!= valueInt)
						{
							needUpdate=true;
							fixedBins=valueInt;
						}
						else
							needUpdate=false;
					}
					else
						return false;
					clearCache();
					needUpdate=true;	
					break;	
				}
				case KEY_COMPPROFILE_NORMAL:
				{
					Point3D newPt;
					if(!newPt.parse(value))
						return false;

					if(!(vectorParams[1] == newPt ))
					{
						vectorParams[1] = newPt;
						needUpdate=true;
						clearCache();
					}
					return true;
				}
				case KEY_COMPPROFILE_NUMBINS:
				{
					unsigned int newNumBins;
					if(stream_cast(newNumBins,value))
						return false;

					nBins=newNumBins;

					clearCache();
					needUpdate=true;
					break;
				}
				case KEY_COMPPROFILE_ORIGIN:
				{
					Point3D newPt;
					if(!newPt.parse(value))
						return false;

					if(!(vectorParams[0] == newPt ))
					{
						vectorParams[0] = newPt;
						needUpdate=true;
						clearCache();
					}

					return true;
				}
				case KEY_COMPPROFILE_PRIMITIVETYPE:
				{
					unsigned int newPrimitive;
					if(stream_cast(newPrimitive,value) ||
							newPrimitive >= COMPPROFILE_PRIMITIVE_END)
						return false;
			

					//TODO: Convert the type data as best we can.
					primitiveType=newPrimitive;

					//In leiu of covnersion, just reset the primitive
					//values to some nominal defaults.
					vectorParams.clear();
					scalarParams.clear();
					switch(primitiveType)
					{
						case IONCLIP_PRIMITIVE_CYLINDER:
							vectorParams.push_back(Point3D(0,0,0));
							vectorParams.push_back(Point3D(0,20,0));
							scalarParams.push_back(10.0f);
							break;

						default:
							ASSERT(false);
					}
			
					clearCache();	
					needUpdate=true;	
					return true;	
				}
				case KEY_COMPPROFILE_RADIUS:
				{
					float newRad;
					if(stream_cast(newRad,value))
						return false;

					if(scalarParams[0] != newRad )
					{
						scalarParams[0] = newRad;
						needUpdate=true;
						clearCache();
					}
					return true;
				}
				case KEY_COMPPROFILE_SHOWPRIMITIVE:
				{
					unsigned int valueInt;
					if(stream_cast(valueInt,value))
						return false;

					if(valueInt ==0 || valueInt == 1)
					{
						if(showPrimitive!= valueInt)
						{
							needUpdate=true;
							showPrimitive=valueInt;
						}
						else
							needUpdate=false;
					}
					else
						return false;		
					break;	
				}

				case KEY_COMPPROFILE_NORMALISE:
				{
					unsigned int valueInt;
					if(stream_cast(valueInt,value))
						return false;

					if(valueInt ==0 || valueInt == 1)
					{
						if(normalise!= valueInt)
						{
							needUpdate=true;
							normalise=valueInt;
						}
						else
							needUpdate=false;
					}
					else
						return false;
					clearCache();
					needUpdate=true;	
					break;	
				}
				case KEY_COMPPROFILE_LOCKAXISMAG:
				{
					string stripped=stripWhite(value);

					if(!(stripped == "1"|| stripped == "0"))
						return false;

					if(stripped=="1")
						lockAxisMag=true;
					else
						lockAxisMag=false;

					needUpdate=true;

					break;
				}
			}
		}
		case 1: //Plot settings
		{
			switch(key)
			{
				case KEY_COMPPROFILE_PLOTTYPE:
				{
					unsigned int tmpPlotType;

					tmpPlotType=plotID(value);

					if(tmpPlotType >= PLOT_TYPE_ENDOFENUM)
						return false;

					plotType = tmpPlotType;
					needUpdate=true;	
					break;
				}
				case KEY_COMPPROFILE_COLOUR:
				{
					unsigned char newR,newG,newB,newA;
					parseColString(value,newR,newG,newB,newA);

					r=((float)newR)/255.0f;
					g=((float)newG)/255.0f;
					b=((float)newB)/255.0f;
					a=1.0;

					needUpdate=true;
					break;	
				}
			}
			break;
		}
		case 2: //Error estimation settings	
		{
			switch(key)
			{
				case KEY_COMPPROFILE_ERRMODE:
				{
					unsigned int tmpMode;
					tmpMode=plotErrmodeID(value);

					if(tmpMode >= PLOT_ERROR_ENDOFENUM)
						return false;

					errMode.mode= tmpMode;
					needUpdate=true;

					break;
				}
				case KEY_COMPPROFILE_AVGWINSIZE:
				{
					unsigned int tmpNum;
					stream_cast(tmpNum,value);
					if(tmpNum<=1)
						return 1;

					errMode.movingAverageNum=tmpNum;
					needUpdate=true;
					break;
				}
				default:
					ASSERT(false);
					;
			}
			break;
		}
		default:
			ASSERT(false);	
	}

	if(needUpdate)
		clearCache();

	return true;
}

void CompositionProfileFilter::getProperties(FilterProperties &propertyList) const
{
	propertyList.data.clear();
	propertyList.keys.clear();
	propertyList.types.clear();

	vector<unsigned int> type,keys;
	vector<pair<string,string> > s;

	string str,tmpStr;

	//Allow primitive selection if we have more than one primitive
	if(COMPPROFILE_PRIMITIVE_END > 1)
	{
		stream_cast(str,(int)COMPPROFILE_PRIMITIVE_END-1);
		str =string("Primitive Type (0-") + str + ")";
		stream_cast(tmpStr,primitiveType);
		s.push_back(make_pair(str,tmpStr));
		keys.push_back(KEY_COMPPROFILE_PRIMITIVETYPE);
		type.push_back(PROPERTY_TYPE_INTEGER);
	}

	str = "Show primitive";	
	stream_cast(tmpStr,showPrimitive);
	s.push_back(make_pair(str,tmpStr));
	keys.push_back(KEY_COMPPROFILE_SHOWPRIMITIVE);
	type.push_back(PROPERTY_TYPE_BOOL);

	switch(primitiveType)
	{
		case COMPPROFILE_PRIMITIVE_CYLINDER:
		{
			ASSERT(vectorParams.size() == 2);
			ASSERT(scalarParams.size() == 1);
			stream_cast(str,vectorParams[0]);
			keys.push_back(KEY_COMPPROFILE_ORIGIN);
			s.push_back(make_pair("Origin", str));
			type.push_back(PROPERTY_TYPE_POINT3D);
			
			stream_cast(str,vectorParams[1]);
			keys.push_back(KEY_COMPPROFILE_NORMAL);
			s.push_back(make_pair("Axis", str));
			type.push_back(PROPERTY_TYPE_POINT3D);

			if(lockAxisMag)
				str="1";
			else
				str="0";
			keys.push_back(KEY_COMPPROFILE_LOCKAXISMAG);
			s.push_back(make_pair("Lock Axis Mag.", str));
			type.push_back(PROPERTY_TYPE_BOOL);
			
			stream_cast(str,scalarParams[0]);
			keys.push_back(KEY_COMPPROFILE_RADIUS);
			s.push_back(make_pair("Radius", str));
			type.push_back(PROPERTY_TYPE_POINT3D);



			break;
		}
	}

	keys.push_back(KEY_COMPPROFILE_FIXEDBINS);
	stream_cast(str,fixedBins);
	s.push_back(make_pair("Fixed Bin Num", str));
	type.push_back(PROPERTY_TYPE_BOOL);

	if(fixedBins)
	{
		stream_cast(tmpStr,nBins);
		str = "Num Bins";
		s.push_back(make_pair(str,tmpStr));
		keys.push_back(KEY_COMPPROFILE_NUMBINS);
		type.push_back(PROPERTY_TYPE_INTEGER);
	}
	else
	{
		str = "Bin width";
		stream_cast(tmpStr,binWidth);
		s.push_back(make_pair(str,tmpStr));
		keys.push_back(KEY_COMPPROFILE_BINWIDTH);
		type.push_back(PROPERTY_TYPE_REAL);
	}

	str = "Normalise";	
	stream_cast(tmpStr,normalise);
	s.push_back(make_pair(str,tmpStr));
	keys.push_back(KEY_COMPPROFILE_NORMALISE);
	type.push_back(PROPERTY_TYPE_BOOL);


	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);

	s.clear();
	type.clear();
	keys.clear();
	
	//use set 2 to store the plot properties
	stream_cast(str,plotType);
	//Let the user know what the valid values for plot type are
	string tmpChoice;
	vector<pair<unsigned int,string> > choices;


	tmpStr=plotString(PLOT_TYPE_LINES);
	choices.push_back(make_pair((unsigned int) PLOT_TYPE_LINES,tmpStr));
	tmpStr=plotString(PLOT_TYPE_BARS);
	choices.push_back(make_pair((unsigned int)PLOT_TYPE_BARS,tmpStr));
	tmpStr=plotString(PLOT_TYPE_STEPS);
	choices.push_back(make_pair((unsigned int)PLOT_TYPE_STEPS,tmpStr));
	tmpStr=plotString(PLOT_TYPE_STEM);
	choices.push_back(make_pair((unsigned int)PLOT_TYPE_STEM,tmpStr));

	tmpStr= choiceString(choices,plotType);
	s.push_back(make_pair(string("Plot Type"),tmpStr));
	type.push_back(PROPERTY_TYPE_CHOICE);
	keys.push_back(KEY_COMPPROFILE_PLOTTYPE);
	//Convert the colour to a hex string
	if(!haveRangeParent)
	{
		string thisCol;
		genColString((unsigned char)(r*255.0),(unsigned char)(g*255.0),
		(unsigned char)(b*255.0),(unsigned char)(a*255.0),thisCol);

		s.push_back(make_pair(string("Colour"),thisCol)); 
		type.push_back(PROPERTY_TYPE_COLOUR);
		keys.push_back(KEY_COMPPROFILE_COLOUR);
	}

	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);
	
	s.clear();
	type.clear();
	keys.clear();

	
	choices.clear();
	tmpStr=plotErrmodeString(PLOT_ERROR_NONE);
	choices.push_back(make_pair((unsigned int) PLOT_ERROR_NONE,tmpStr));
	tmpStr=plotErrmodeString(PLOT_ERROR_MOVING_AVERAGE);
	choices.push_back(make_pair((unsigned int) PLOT_ERROR_MOVING_AVERAGE,tmpStr));

	tmpStr= choiceString(choices,errMode.mode);
	s.push_back(make_pair(string("Err. Estimator"),tmpStr));
	type.push_back(PROPERTY_TYPE_CHOICE);
	keys.push_back(KEY_COMPPROFILE_ERRMODE);


	if(errMode.mode == PLOT_ERROR_MOVING_AVERAGE)
	{
		stream_cast(tmpStr,errMode.movingAverageNum);
		s.push_back(make_pair(string("Avg. Window"), tmpStr));
		type.push_back(PROPERTY_TYPE_INTEGER);
		keys.push_back(KEY_COMPPROFILE_AVGWINSIZE);

	}	

	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);
}

//!Get approx number of bytes for caching output
size_t CompositionProfileFilter::numBytesForCache(size_t nObjects) const
{
	//FIXME: IMPLEMEMENT ME
	return (unsigned int)(-1);
}

bool CompositionProfileFilter::writeState(std::ofstream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<compositionprofile>" << endl;
			f << tabs(depth+1) << "<userstring value=\""<<userString << "\"/>"  << endl;

			f << tabs(depth+1) << "<primitivetype value=\"" << primitiveType<< "\"/>" << endl;
			f << tabs(depth+1) << "<showprimitive value=\"" << showPrimitive << "\"/>" << endl;
			f << tabs(depth+1) << "<lockaxismag value=\"" << lockAxisMag<< "\"/>" << endl;
			f << tabs(depth+1) << "<vectorparams>" << endl;
			for(unsigned int ui=0; ui<vectorParams.size(); ui++)
			{
				f << tabs(depth+2) << "<point3d x=\"" << vectorParams[ui][0] << 
					"\" y=\"" << vectorParams[ui][1] << "\" z=\"" << vectorParams[ui][2] << "\"/>" << endl;
			}
			f << tabs(depth+1) << "</vectorparams>" << endl;

			f << tabs(depth+1) << "<scalarparams>" << endl;
			for(unsigned int ui=0; ui<scalarParams.size(); ui++)
				f << tabs(depth+2) << "<scalar value=\"" << scalarParams[0] << "\"/>" << endl; 
			
			f << tabs(depth+1) << "</scalarparams>" << endl;
			f << tabs(depth+1) << "<normalise value=\"" << normalise << "\"/>" << endl;
			f << tabs(depth+1) << "<stepmode value=\"" << stepMode << "\"/>" << endl;
			f << tabs(depth+1) << "<fixedbins value=\"" << (int)fixedBins << "\"/>" << endl;
			f << tabs(depth+1) << "<nbins value=\"" << nBins << "\"/>" << endl;
			f << tabs(depth+1) << "<binwidth value=\"" << binWidth << "\"/>" << endl;
			f << tabs(depth+1) << "<colour r=\"" <<  r<< "\" g=\"" << g << "\" b=\"" <<b
				<< "\" a=\"" << a << "\"/>" <<endl;

			f << tabs(depth+1) << "<plottype value=\"" << plotType << "\"/>" << endl;
			f << tabs(depth) << "</compositionprofile>" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool CompositionProfileFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
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

	std::string tmpStr;	
	//Retrieve primitive type 
	//====
	if(XMLHelpFwdToElem(nodePtr,"primitivetype"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(primitiveType,tmpStr))
		return false;

	if(primitiveType >= COMPPROFILE_PRIMITIVE_END)
	       return false;	
	xmlFree(xmlString);
	//====
	
	//Retrieve primitive visiblity 
	//====
	if(XMLHelpFwdToElem(nodePtr,"showprimitive"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(tmpStr == "0")
		showPrimitive=false;
	else if(tmpStr == "1")
		showPrimitive=true;
	else
		return false;

	xmlFree(xmlString);
	//====
	
	//Retrieve axis lock mode 
	//====
	if(XMLHelpFwdToElem(nodePtr,"lockaxismag"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(tmpStr == "0")
		lockAxisMag=false;
	else if(tmpStr == "1")
		lockAxisMag=true;
	else
		return false;

	xmlFree(xmlString);
	//====
	
	//Retreive vector parameters
	//===
	if(XMLHelpFwdToElem(nodePtr,"vectorparams"))
		return false;
	xmlNodePtr tmpNode=nodePtr;

	nodePtr=nodePtr->xmlChildrenNode;

	vectorParams.clear();
	while(!XMLHelpFwdToElem(nodePtr,"point3d"))
	{
		float x,y,z;
		//--Get X value--
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"x");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;
		xmlFree(xmlString);

		//Check it is streamable
		if(stream_cast(x,tmpStr))
			return false;

		//--Get Z value--
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"y");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;
		xmlFree(xmlString);

		//Check it is streamable
		if(stream_cast(y,tmpStr))
			return false;

		//--Get Y value--
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"z");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;
		xmlFree(xmlString);

		//Check it is streamable
		if(stream_cast(z,tmpStr))
			return false;

		vectorParams.push_back(Point3D(x,y,z));
	}
	//===	

	nodePtr=tmpNode;
	//Retreive scalar parameters
	//===
	if(XMLHelpFwdToElem(nodePtr,"scalarparams"))
		return false;
	
	tmpNode=nodePtr;
	nodePtr=nodePtr->xmlChildrenNode;

	scalarParams.clear();
	while(!XMLHelpFwdToElem(nodePtr,"scalar"))
	{
		float v;
		//Get value
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;
		xmlFree(xmlString);

		//Check it is streamable
		if(stream_cast(v,tmpStr))
			return false;
		scalarParams.push_back(v);
	}
	//===	

	//Check the scalar params match the selected primitive	
	switch(primitiveType)
	{
		case COMPPROFILE_PRIMITIVE_CYLINDER:
			if(vectorParams.size() != 2 || scalarParams.size() !=1)
				return false;
			break;
		default:
			ASSERT(false);
			return false;
	}

	nodePtr=tmpNode;

	//Retrieve normalisation on/off 
	//====
	if(XMLHelpFwdToElem(nodePtr,"normalise"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(tmpStr == "0")
		normalise=false;
	else if(tmpStr == "1")
		normalise=true;
	else
		return false;

	xmlFree(xmlString);
	//====

	//Retrieve step mode 
	//====
	if(XMLHelpFwdToElem(nodePtr,"stepmode"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(stepMode,tmpStr))
		return false;

	xmlFree(xmlString);
	//====


	//Retrieve fixed bins on/off 
	//====
	if(XMLHelpFwdToElem(nodePtr,"fixedbins"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(tmpStr == "0")
		fixedBins=false;
	else if(tmpStr == "1")
		fixedBins=true;
	else
		return false;


	xmlFree(xmlString);
	//====

	//Retrieve num bins
	//====
	if(XMLHelpFwdToElem(nodePtr,"nbins"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(nBins,tmpStr))
		return false;

	xmlFree(xmlString);
	//====

	//Retrieve bin width
	//====
	if(XMLHelpFwdToElem(nodePtr,"binwidth"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(binWidth,tmpStr))
		return false;

	xmlFree(xmlString);
	//====

	//Retrieve colour
	//====
	if(XMLHelpFwdToElem(nodePtr,"colour"))
		return false;
	if(!parseXMLColour(nodePtr,r,g,b,a))
		return false;
	//====
	
	//Retrieve plot type 
	//====
	if(XMLHelpFwdToElem(nodePtr,"plottype"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(plotType,tmpStr))
		return false;

	if(plotType >= PLOT_TYPE_ENDOFENUM)
	       return false;	
	xmlFree(xmlString);
	//====

	return true;
}

void CompositionProfileFilter::setPropFromBinding(const SelectionBinding &b)
{
	switch(b.getID())
	{
		case BINDING_CYLINDER_RADIUS:
			b.getValue(scalarParams[0]);
			break;

		case BINDING_CYLINDER_DIRECTION:
			b.getValue(vectorParams[1]);
			break;

		case BINDING_CYLINDER_ORIGIN:
			b.getValue(vectorParams[0]);
			break;
		default:
			ASSERT(false);
	}

	clearCache();
}

//=== Bounding box filter ==


BoundingBoxFilter::BoundingBoxFilter()
{
	fixedNumTicks=true;
	threeDText=true;
	for(unsigned int ui=0;ui<3;ui++)
	{
		numTicks[ui]=12;
		tickSpacing[ui]=5.0f;
	}
	fontSize=5;

	rLine=gLine=0.0f;
	aLine=bLine=1.0f;
	


	lineWidth=2.0f;
	isVisible=true;

	cacheOK=false;
	cache=false; 
}

Filter *BoundingBoxFilter::cloneUncached() const
{
	BoundingBoxFilter *p=new BoundingBoxFilter();
	p->fixedNumTicks=fixedNumTicks;
	for(unsigned int ui=0;ui<3;ui++)
	{
		p->numTicks[ui]=numTicks[ui];
		p->tickSpacing[ui]=tickSpacing[ui];
	}

	p->isVisible=isVisible;
	p->rLine=rLine;
	p->gLine=gLine;
	p->bLine=bLine;
	p->aLine=aLine;
	p->threeDText=threeDText;	

	p->lineWidth=lineWidth;
	p->fontSize=fontSize;

	//We are copying wether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

size_t BoundingBoxFilter::numBytesForCache(size_t nObjects) const
{
	//Say we don't know, we are not going to cache anyway.
	return (size_t)-1;
}

unsigned int BoundingBoxFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(void))
{

	//Compute the bounding box of the incoming streams
	BoundCube bTotal,bThis;
	bTotal.setInverseLimits();

	size_t totalSize=numElements(dataIn);
	size_t n=0;
	Point3D p[4];
	unsigned int ptCount=0;
	for(unsigned int ui=0;ui<dataIn.size();ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS:
			{
				bThis=bTotal;
				//Grab the first four points to define a volume.
				//then expand that volume using the boundcube functions.
				const IonStreamData *d =(const IonStreamData *) dataIn[ui];
				size_t dataPos=0;
				unsigned int curProg=NUM_CALLBACK;
				while(ptCount < 4 && dataPos < d->data.size())
				{
					for(unsigned int ui=0; ui<d->data.size();ui++)
					{
						p[ptCount]=d->data[ui].getPosRef();
						ptCount++;
						dataPos=ui;
						if(ptCount >=4) 
							break;
					}
				}

				//Ptcount will be 4 if we have >=4 points in dataset
				if(ptCount == 4)
				{
					bThis.setBounds(p,4);
					//Expand the bounding volume
#ifdef _OPENMP
					//Parallel version
					unsigned int nT =omp_get_max_threads(); 

					BoundCube *newBounds= new BoundCube[nT];
					for(unsigned int ui=0;ui<nT;ui++)
						newBounds[ui]=bThis;

					bool spin=false;
					#pragma omp parallel for shared(spin)
					for(unsigned int ui=dataPos;ui<d->data.size();ui++)
					{
						unsigned int thisT=omp_get_thread_num();
						//OpenMP does not allow exiting. Use spin instead
						if(spin)
							continue;

						if(!curProg--)
						{
							#pragma omp critical
							{
							n+=NUM_CALLBACK;
							progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
							}


							if(thisT == 0)
							{
								if(!(*callback)())
									spin=true;
							}
						}

						newBounds[thisT].expand(d->data[ui].getPosRef());
					}
					if(spin)
					{			
						delete d;
						return IONDOWNSAMPLE_ABORT_ERR;
					}

					for(unsigned int ui=0;ui<nT;ui++)
						bThis.expand(newBounds[ui]);

					delete[] newBounds;
#else
					//Single thread version
					for(unsigned int ui=dataPos;ui<d->data.size();ui++)
					{
						bThis.expand(d->data[ui].getPosRef());
						if(!curProg--)
						{
							n+=NUM_CALLBACK;
							progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
							if(!(*callback)())
							{
								delete d;
								return IONDOWNSAMPLE_ABORT_ERR;
							}
						}
					}
#endif
					bTotal.expand(bThis);
				}

				break;
			}
			default:
				break;
		}

		//Copy the input data to the output	
		getOut.push_back(dataIn[ui]);	
	}

	//Append the bounding box if it is valid
	if(bTotal.isValid() && isVisible)
	{
		DrawStreamData *d = new DrawStreamData;

		//Add the rectangle drawable
		DrawRectPrism *dP = new DrawRectPrism;
		dP->setAxisAligned(bTotal);
		dP->setColour(rLine,gLine,bLine,aLine);
		dP->setLineWidth(lineWidth);
		d->drawables.push_back(dP);

		//Add the tick drawables
		Point3D tickOrigin,tickEnd;
		bTotal.getBounds(tickOrigin,tickEnd);

		float tmpTickSpacing[3];
		float tmpTickCount[3];
		if(fixedNumTicks)
		{
			for(unsigned int ui=0; ui<3;ui++)
			{
				ASSERT(numTicks[ui]);
				tmpTickSpacing[ui]=( (tickEnd[ui] - tickOrigin[ui])/(float)numTicks[ui]);
				tmpTickCount[ui]=numTicks[ui];
			}
		}
		else
		{
			for(unsigned int ui=0; ui<3;ui++)
			{
				ASSERT(numTicks[ui]);
				tmpTickSpacing[ui]= tickSpacing[ui];
				tmpTickCount[ui]=(unsigned int)((tickEnd[ui] - tickOrigin[ui])/tickSpacing[ui]);
			}
		}

		//Draw the ticks on the box perimeter.
		for(unsigned int ui=0;ui<3;ui++)
		{
			Point3D tickVector;
			Point3D tickPosition;
			Point3D textVector,upVector;

			tickPosition=tickOrigin;
			switch(ui)
			{
				case 0:
					tickVector=Point3D(0,-1,-1);
					textVector=Point3D(0,1,0);
					break;
				case 1:
					tickVector=Point3D(-1,0,-1);
					textVector=Point3D(1,0,0);
					break;
				case 2:
					tickVector=Point3D(-1,-1,0);
					textVector=Point3D(1,1,0);
					break;
			}

			//TODO: This would be more efficient if we made some kind of 
			//"comb" class?
			DrawVector *dV;
			DrawGLText *dT;
			//Allow up to  128 chars
			char buffer[128];
			for(unsigned int uj=0;uj<tmpTickCount[ui];uj++)
			{
				tickPosition[ui]=tmpTickSpacing[ui]*uj + tickOrigin[ui];
				dV = new DrawVector;
			
				dV->setOrigin(tickPosition);
				dV->setVector(tickVector);
				dV->setColour(rLine,gLine,bLine,aLine);

				d->drawables.push_back(dV);
		

				//Don't draw the 0 value, as this gets repeated. 
				//we will handle this separately
				if(uj)
				{
					//Draw the tick text
					if( threeDText)	
						dT = new DrawGLText(getDefaultFontFile().c_str(),FTGL_POLYGON);
					else
						dT = new DrawGLText(getDefaultFontFile().c_str(),FTGL_BITMAP);
					float f;
					f = tmpTickSpacing[ui]*uj;
					snprintf(buffer,128,"%2.0f",f);
					dT->setString(buffer);
					dT->setSize(fontSize);
					
					dT->setColour(1.0f,1.0f,1.0f,1.0f);
					dT->setOrigin(tickPosition + tickVector*2);	
					dT->setUp(Point3D(0,0,1));	
					dT->setTextDir(textVector);
					dT->setAlignment(DRAWTEXT_ALIGN_RIGHT);
					d->drawables.push_back(dT);
				}
			}

		}

		DrawGLText *dT; 
		if(threeDText)
			dT = new DrawGLText(getDefaultFontFile().c_str(),FTGL_POLYGON);
		else
			dT = new DrawGLText(getDefaultFontFile().c_str(),FTGL_BITMAP);
		//Handle "0" text value
		dT->setString("0");
		
		dT->setColour(1.0f,1.0f,1.0f,1.0f);
		dT->setSize(fontSize);
		dT->setOrigin(tickOrigin+ Point3D(-1,-1,-1));
		dT->setAlignment(DRAWTEXT_ALIGN_RIGHT);
		dT->setUp(Point3D(0,0,1));	
		dT->setTextDir(Point3D(-1,-1,0));
		d->drawables.push_back(dT);
		d->cached=false;
		
		getOut.push_back(d);
	}
	return 0;
}


void BoundingBoxFilter::getProperties(FilterProperties &propertyList) const
{
	propertyList.data.clear();
	propertyList.keys.clear();
	propertyList.types.clear();

	vector<unsigned int> type,keys;
	vector<pair<string,string> > s;

	string tmpStr;
	stream_cast(tmpStr,isVisible);
	s.push_back(std::make_pair("Visible", tmpStr));
	keys.push_back(KEY_BOUNDINGBOX_VISIBLE);
	type.push_back(PROPERTY_TYPE_BOOL);

	
	//Properties are X Y and Z counts on ticks
	stream_cast(tmpStr,fixedNumTicks);
	s.push_back(std::make_pair("Fixed Tick Num", tmpStr));
	keys.push_back(KEY_BOUNDINGBOX_FIXEDOUT);
	type.push_back(PROPERTY_TYPE_BOOL);
	if(fixedNumTicks)
	{
		//Properties are X Y and Z counts on ticks
		stream_cast(tmpStr,numTicks[0]);
		keys.push_back(KEY_BOUNDINGBOX_COUNT_X);
		s.push_back(make_pair("Num X", tmpStr));
		type.push_back(PROPERTY_TYPE_INTEGER);
		
		stream_cast(tmpStr,numTicks[1]);
		keys.push_back(KEY_BOUNDINGBOX_COUNT_Y);
		s.push_back(make_pair("Num Y", tmpStr));
		type.push_back(PROPERTY_TYPE_INTEGER);
		
		stream_cast(tmpStr,numTicks[2]);
		keys.push_back(KEY_BOUNDINGBOX_COUNT_Z);
		s.push_back(make_pair("Num Z", tmpStr));
		type.push_back(PROPERTY_TYPE_INTEGER);
	}
	else
	{
		stream_cast(tmpStr,tickSpacing[0]);
		s.push_back(make_pair("Spacing X", tmpStr));
		keys.push_back(KEY_BOUNDINGBOX_SPACING_X);
		type.push_back(PROPERTY_TYPE_REAL);

		stream_cast(tmpStr,tickSpacing[1]);
		s.push_back(make_pair("Spacing Y", tmpStr));
		keys.push_back(KEY_BOUNDINGBOX_SPACING_Y);
		type.push_back(PROPERTY_TYPE_REAL);

		stream_cast(tmpStr,tickSpacing[2]);
		s.push_back(make_pair("Spacing Z", tmpStr));
		keys.push_back(KEY_BOUNDINGBOX_SPACING_Z);
		type.push_back(PROPERTY_TYPE_REAL);
	}

	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);

	//Second set -- Box Line properties 
	type.clear();
	keys.clear();
	s.clear();

	//Colour
	genColString((unsigned char)(rLine*255.0),(unsigned char)(gLine*255.0),
		(unsigned char)(bLine*255),(unsigned char)(aLine*255),tmpStr);
	s.push_back(std::make_pair("Box Colour", tmpStr));
	keys.push_back(KEY_BOUNDINGBOX_LINECOLOUR);
	type.push_back(PROPERTY_TYPE_COLOUR);


	
	//Line thickness
	stream_cast(tmpStr,lineWidth);
	s.push_back(std::make_pair("Line thickness", tmpStr));
	keys.push_back(KEY_BOUNDINGBOX_LINEWIDTH);
	type.push_back(PROPERTY_TYPE_REAL);

	//Font size	
	stream_cast(tmpStr,fontSize);
	keys.push_back(KEY_BOUNDINGBOX_FONTSIZE);
	s.push_back(make_pair("Font Size", tmpStr));
	type.push_back(PROPERTY_TYPE_INTEGER);


	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);
}

bool BoundingBoxFilter::setProperty( unsigned int set, unsigned int key,
					const std::string &value, bool &needUpdate)
{

	needUpdate=false;
	switch(key)
	{
		case KEY_BOUNDINGBOX_VISIBLE:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			bool lastVal=isVisible;
			if(stripped=="1")
				isVisible=true;
			else
				isVisible=false;

			//if the result is different, the
			//cache should be invalidated
			if(lastVal!=isVisible)
				needUpdate=true;
			break;
		}	
		case KEY_BOUNDINGBOX_FIXEDOUT:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			bool lastVal=fixedNumTicks;
			if(stripped=="1")
				fixedNumTicks=true;
			else
				fixedNumTicks=false;

			//if the result is different, the
			//cache should be invalidated
			if(lastVal!=fixedNumTicks)
				needUpdate=true;
			break;
		}	
		case KEY_BOUNDINGBOX_COUNT_X:
		case KEY_BOUNDINGBOX_COUNT_Y:
		case KEY_BOUNDINGBOX_COUNT_Z:
		{
			ASSERT(fixedNumTicks);
			unsigned int newCount;
			if(stream_cast(newCount,value))
				return false;

			//there is a start and an end tick, at least
			if(newCount < 2)
				return false;

			numTicks[key-KEY_BOUNDINGBOX_COUNT_X]=newCount;
			needUpdate=true;
			break;
		}
		case KEY_BOUNDINGBOX_LINECOLOUR:
		{
			unsigned char newR,newG,newB,newA;

			parseColString(value,newR,newG,newB,newA);

			if(newB != bLine || newR != rLine ||
				newG !=gLine || newA != aLine)
				needUpdate=true;

			rLine=newR/255.0;
			gLine=newG/255.0;
			bLine=newB/255.0;
			aLine=newA/255.0;
			needUpdate=true;
			break;
		}
		case KEY_BOUNDINGBOX_LINEWIDTH:
		{
			float newWidth;
			if(stream_cast(newWidth,value))
				return false;

			if(newWidth <= 0.0f)
				return false;

			lineWidth=newWidth;
			needUpdate=true;
			break;
		}
		case KEY_BOUNDINGBOX_SPACING_X:
		case KEY_BOUNDINGBOX_SPACING_Y:
		case KEY_BOUNDINGBOX_SPACING_Z:
		{
			ASSERT(!fixedNumTicks);
			float newSpacing;
			if(stream_cast(newSpacing,value))
				return false;

			if(newSpacing <= 0.0f)
				return false;

			tickSpacing[key-KEY_BOUNDINGBOX_SPACING_X]=newSpacing;
			needUpdate=true;
			break;
		}
		case KEY_BOUNDINGBOX_FONTSIZE:
		{
			unsigned int newCount;
			if(stream_cast(newCount,value))
				return false;

			fontSize=newCount;
			needUpdate=true;
			break;
		}
		default:
			ASSERT(false);
	}	
	return true;
}


std::string  BoundingBoxFilter::getErrString(unsigned int code) const
{

	//Currently the only error is aborting
	return std::string("Aborted");
}

bool BoundingBoxFilter::writeState(std::ofstream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<boundingbox>" << endl;
			f << tabs(depth+1) << "<userstring value=\""<<userString << "\"/>"  << endl;
			f << tabs(depth+1) << "<visible value=\"" << isVisible << "\"/>" << endl;
			f << tabs(depth+1) << "<fixedticks value=\"" << fixedNumTicks << "\"/>" << endl;
			f << tabs(depth+1) << "<ticknum x=\""<<numTicks[0]<< "\" y=\"" 
				<< numTicks[1] << "\" z=\""<< numTicks[2] <<"\"/>"  << endl;
			f << tabs(depth+1) << "<tickspacing x=\""<<tickSpacing[0]<< "\" y=\"" 
				<< tickSpacing[1] << "\" z=\""<< tickSpacing[2] <<"\"/>"  << endl;
			f << tabs(depth+1) << "<linewidth value=\"" << lineWidth << "\"/>"<<endl;
			f << tabs(depth+1) << "<fontsize value=\"" << fontSize << "\"/>"<<endl;
			f << tabs(depth+1) << "<colour r=\"" <<  rLine<< "\" g=\"" << gLine << "\" b=\"" <<bLine  
								<< "\" a=\"" << aLine << "\"/>" <<endl;
			f << tabs(depth) << "</boundingbox>" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool BoundingBoxFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
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
	std::string tmpStr;

	//Retrieve visiblity 
	//====
	if(XMLHelpFwdToElem(nodePtr,"visible"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(tmpStr == "0")
		isVisible=false;
	else if(tmpStr == "1")
		isVisible=true;
	else
		return false;

	xmlFree(xmlString);
	//====
	
	//Retrieve fixed tick num
	//====
	if(XMLHelpFwdToElem(nodePtr,"fixedticks"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(tmpStr == "0")
		fixedNumTicks=false;
	else if(tmpStr == "1")
		fixedNumTicks=true;
	else
		return false;

	xmlFree(xmlString);
	//====
	
	//Retrieve num ticks
	//====
	if(XMLHelpFwdToElem(nodePtr,"ticknum"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"x");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(numTicks[0],tmpStr))
		return false;

	xmlFree(xmlString);

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"y");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(numTicks[1],tmpStr))
		return false;

	xmlFree(xmlString);

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"z");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(numTicks[2],tmpStr))
		return false;

	xmlFree(xmlString);
	//====
	
	//Retrieve spacing
	//====
	if(XMLHelpFwdToElem(nodePtr,"tickspacing"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"x");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(tickSpacing[0],tmpStr))
		return false;

	if(tickSpacing[0] < 0.0f)
		return false;

	xmlFree(xmlString);

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"y");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(tickSpacing[1],tmpStr))
		return false;
	if(tickSpacing[1] < 0.0f)
		return false;

	xmlFree(xmlString);

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"z");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(tickSpacing[2],tmpStr))
		return false;

	if(tickSpacing[2] < 0.0f)
		return false;
	xmlFree(xmlString);
	//====
	
	//Retrieve line width 
	//====
	if(XMLHelpFwdToElem(nodePtr,"linewidth"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(lineWidth,tmpStr))
		return false;

	if(lineWidth < 0)
	       return false;	
	xmlFree(xmlString);
	//====
	
	//Retrieve font size 
	//====
	if(XMLHelpFwdToElem(nodePtr,"fontsize"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(fontSize,tmpStr))
		return false;

	if(fontSize<  0)
	       return false;	
	xmlFree(xmlString);
	//====

	//Retrieve colour
	//====
	if(XMLHelpFwdToElem(nodePtr,"colour"))
		return false;

	if(!parseXMLColour(nodePtr,rLine,gLine,bLine,aLine))
		return false;
	//====

	return true;	
}


//=== Transform filter === 
TransformFilter::TransformFilter()
{
	transformMode=TRANSFORM_TRANSLATE;
	originMode=TRANSFORM_ORIGINMODE_SELECT;
	//Set up default value
	vectorParams.resize(1);
	vectorParams[0] = Point3D(0,0,0);
	
	showPrimitive=true;
	showOrigin=false;

	cacheOK=false;
	cache=false; 
}

Filter *TransformFilter::cloneUncached() const
{
	TransformFilter *p=new TransformFilter();

	//Copy the values
	p->vectorParams.resize(vectorParams.size());
	p->scalarParams.resize(scalarParams.size());
	
	std::copy(vectorParams.begin(),vectorParams.end(),p->vectorParams.begin());
	std::copy(scalarParams.begin(),scalarParams.end(),p->scalarParams.begin());

	p->showPrimitive=showPrimitive;
	p->originMode=originMode;
	p->transformMode=transformMode;
	p->showOrigin=showOrigin;
	//We are copying wether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

size_t TransformFilter::numBytesForCache(size_t nObjects) const
{
	//Say we don't know, we are not going to cache anyway.
	return (size_t)-1;
}
DrawStreamData* TransformFilter::makeMarkerSphere(SelectionDevice* &s) const
{
	//construct a new primitive, do not cache
	DrawStreamData *drawData=new DrawStreamData;
	//Add drawable components
	DrawSphere *dS = new DrawSphere;
	dS->setOrigin(vectorParams[0]);
	dS->setRadius(1);
	//FIXME: Alpha blending is all screwed up. May require more
	//advanced drawing in scene. (front-back drawing).
	//I have set alpha=1 for now.
	dS->setColour(0.2,0.2,0.8,1.0);
	dS->setLatSegments(40);
	dS->setLongSegments(40);
	dS->wantsLight=true;
	drawData->drawables.push_back(dS);

	s=0;
	//Set up selection "device" for user interaction
	//Note the order of s->addBinding is critical,
	//as bindings are selected by first match.
	//====
	//The object is selectable
	if (originMode == TRANSFORM_ORIGINMODE_SELECT )
	{
		dS->canSelect=true;

		s=new SelectionDevice(this);
		SelectionBinding b;

		b.setBinding(SELECT_BUTTON_LEFT,FLAG_CMD,DRAW_SPHERE_BIND_ORIGIN,
		             BINDING_SPHERE_ORIGIN,dS->getOrigin(),dS);
		b.setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
		s->addBinding(b);

	}
	drawData->cached=false;	
}

unsigned int TransformFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(void))
{
	//Clear selection devices FIXME: Is this a memory leak???
	devices.clear();
	//use the cached copy if we have it.
	if(cacheOK)
	{
		ASSERT(filterOutputs.size());
		for(unsigned int ui=0;ui<dataIn.size();ui++)
		{
			if(dataIn[ui]->getStreamType() != STREAM_TYPE_IONS)
				getOut.push_back(dataIn[ui]);
		}

		for(unsigned int ui=0;ui<filterOutputs.size();ui++)
			getOut.push_back(filterOutputs[ui]);
		
		
		if(filterOutputs.size() && showPrimitive)
		{
			//If the user is using a transform mode that requires origin selection 
			if(showOrigin && (transformMode == TRANSFORM_ROTATE ||
					transformMode == TRANSFORM_SCALE) )
			{
				SelectionDevice *s;
				getOut.push_back(makeMarkerSphere(s));
				if(s)
					devices.push_back(s);
			}
			
		}

		return 0;
	}


	//The user is allowed to choose the mode by which the origin is computed
	//so set the origin variable depending upon this
	switch(originMode)
	{
		case TRANSFORM_ORIGINMODE_CENTREBOUND:
		{
			BoundCube masterB;
			masterB.setInverseLimits();
			#pragma omp parallel for
			for(unsigned int ui=0;ui<dataIn.size() ;ui++)
			{
				BoundCube thisB;

				if(dataIn[ui]->getStreamType() == STREAM_TYPE_IONS)
				{
					const IonStreamData* ions;
					ions = (const IonStreamData*)dataIn[ui];
					if(ions->data.size())
					{
						thisB = getIonDataLimits(ions->data);
						#pragma omp critical
						masterB.expand(thisB);
					}
				}
			}

			if(!masterB.isValid())
				vectorParams[0]=Point3D(0,0,0);
			else
				vectorParams[0]=masterB.getCentroid();
			break;
		}
		case TRANSFORM_ORIGINMODE_MASSCENTRE:
		{
			Point3D massCentre(0,0,0);
			size_t numCentres=0;
			#pragma omp parallel for
			for(unsigned int ui=0;ui<dataIn.size() ;ui++)
			{
				if(dataIn[ui]->getStreamType() == STREAM_TYPE_IONS)
				{
					const IonStreamData* ions;
					ions = (const IonStreamData*)dataIn[ui];

					if(ions->data.size())
					{
						Point3D thisCentre;
						thisCentre=Point3D(0,0,0);
						for(unsigned int uj=0;uj<ions->data.size();uj++)
							thisCentre+=ions->data[uj].getPosRef();
						#pragma omp critical
						massCentre+=thisCentre*1.0/(float)ions->data.size();
						numCentres++;
					}
				}
			}
			vectorParams[0]=massCentre*1.0/(float)numCentres;
			break;

		}
		case TRANSFORM_ORIGINMODE_SELECT:
			break;
		default:
			ASSERT(false);
	}

	//If the user is using a transform mode that requires origin selection 
	if(showOrigin && (transformMode == TRANSFORM_ROTATE ||
			transformMode == TRANSFORM_SCALE) )
	{
		SelectionDevice *s;
		getOut.push_back(makeMarkerSphere(s));
		if(s)
			devices.push_back(s);
	}
			
	//Apply the transformations to the incoming 
	//ion streams, generating new outgoing ion streams with
	//the modified positions
	size_t totalSize=numElements(dataIn);
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		switch(transformMode)
		{
			case TRANSFORM_SCALE:
			{
				//We are going to scale the incoming point data
				//around the specified origin.
				ASSERT(vectorParams.size() == 1);
				ASSERT(scalarParams.size() == 1);
				float scaleFactor=scalarParams[0];
				Point3D origin=vectorParams[0];

				size_t n=0;
				switch(dataIn[ui]->getStreamType())
				{
					case STREAM_TYPE_IONS:
					{
						//Set up scaling output ion stream 
						IonStreamData *d=new IonStreamData;
						const IonStreamData *src = (const IonStreamData *)dataIn[ui];
						d->data.resize(src->data.size());
						d->r = src->r;
						d->g = src->g;
						d->b = src->b;
						d->a = src->a;
						d->ionSize = src->ionSize;
						d->valueType=src->valueType;

						ASSERT(src->data.size() <= totalSize);
						unsigned int curProg=NUM_CALLBACK;
#ifdef _OPENMP
						//Parallel version
						bool spin=false;
						#pragma omp parallel for shared(spin)
						for(unsigned int ui=0;ui<src->data.size();ui++)
						{
							unsigned int thisT=omp_get_thread_num();
							if(spin)
								continue;

							if(!curProg--)
							{
								#pragma omp critical
								{
								n+=NUM_CALLBACK;
								progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
								}


								if(thisT == 0)
								{
									if(!(*callback)())
										spin=true;
								}
							}


							//set the position for the given ion
							d->data[ui].setPos((src->data[ui].getPosRef() - origin)*scaleFactor+origin);
							d->data[ui].setMassToCharge(src->data[ui].getMassToCharge());
						}
						if(spin)
						{			
							delete d;
							return TRANSFORM_CALLBACK_FAIL;
						}

#else
						//Single threaded version
						size_t pos=0;
						//Copy across the ions into the target
						for(vector<IonHit>::const_iterator it=src->data.begin();
							       it!=src->data.end(); ++it)
						{
							//set the position for the given ion
							d->data[pos].setPos((it->getPosRef() - origin)*scaleFactor+origin);
							d->data[pos].setMassToCharge(it->getMassToCharge());
							//update progress every CALLBACK ions
							if(!curProg--)
							{
								n+=NUM_CALLBACK;
								progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
								if(!(*callback)())
								{
									delete d;
									return TRANSFORM_CALLBACK_FAIL;
								}
								curProg=NUM_CALLBACK;
							}
							pos++;
						}

						ASSERT(pos == d->data.size());
#endif
						ASSERT(d->data.size() == src->data.size());

						if(cache)
						{
							d->cached=1;
							filterOutputs.push_back(d);
							cacheOK=true;
						}
						else
							d->cached=0;

						getOut.push_back(d);
						break;
					}
					default:
						//Just copy across the ptr, if we are unfamiliar with this type
						getOut.push_back(dataIn[ui]);	
						break;
				}
				break;
			}
			case TRANSFORM_TRANSLATE:
			{
				//We are going to scale the incoming point data
				//around the specified origin.
				ASSERT(vectorParams.size() == 1);
				ASSERT(scalarParams.size() == 0);
				Point3D origin =vectorParams[0];
				size_t n=0;
				switch(dataIn[ui]->getStreamType())
				{
					case STREAM_TYPE_IONS:
					{
						//Set up scaling output ion stream 
						IonStreamData *d=new IonStreamData;

						const IonStreamData *src = (const IonStreamData *)dataIn[ui];
						d->data.resize(src->data.size());
						d->r = src->r;
						d->g = src->g;
						d->b = src->b;
						d->a = src->a;
						d->ionSize = src->ionSize;
						d->valueType=src->valueType;

						ASSERT(src->data.size() <= totalSize);
						unsigned int curProg=NUM_CALLBACK;
#ifdef _OPENMP
						//Parallel version
						bool spin=false;
						#pragma omp parallel for shared(spin)
						for(unsigned int ui=0;ui<src->data.size();ui++)
						{
							unsigned int thisT=omp_get_thread_num();
							if(spin)
								continue;

							if(!curProg--)
							{
								#pragma omp critical
								{
								n+=NUM_CALLBACK;
								progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
								}


								if(thisT == 0)
								{
									if(!(*callback)())
										spin=true;
								}
							}


							//set the position for the given ion
							d->data[ui].setPos((src->data[ui].getPosRef() - origin));
							d->data[ui].setMassToCharge(src->data[ui].getMassToCharge());
						}
						if(spin)
						{			
							delete d;
							return TRANSFORM_CALLBACK_FAIL;
						}

#else
						//Single threaded version
						size_t pos=0;
						//Copy across the ions into the target
						for(vector<IonHit>::const_iterator it=src->data.begin();
							       it!=src->data.end(); ++it)
						{
							//set the position for the given ion
							d->data[pos].setPos((it->getPosRef() - origin));
							d->data[pos].setMassToCharge(it->getMassToCharge());
							//update progress every CALLBACK ions
							if(!curProg--)
							{
								n+=NUM_CALLBACK;
								progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
								if(!(*callback)())
								{
									delete d;
									return TRANSFORM_CALLBACK_FAIL;
								}
								curProg=NUM_CALLBACK;
							}
							pos++;
						}
						ASSERT(pos == d->data.size());
#endif
						ASSERT(d->data.size() == src->data.size());
						if(cache)
						{
							d->cached=1;
							filterOutputs.push_back(d);
							cacheOK=true;
						}
						else
							d->cached=0;
						
						getOut.push_back(d);
						break;
					}
					default:
						//Just copy across the ptr, if we are unfamiliar with this type
						getOut.push_back(dataIn[ui]);	
						break;
				}
				break;
			}
			case TRANSFORM_ROTATE:
			{
				Point3D origin=vectorParams[0];
				switch(dataIn[ui]->getStreamType())
				{
					case STREAM_TYPE_IONS:
					{
						//Set up scaling output ion stream 
						IonStreamData *d=new IonStreamData;

						const IonStreamData *src = (const IonStreamData *)dataIn[ui];
						d->data.resize(src->data.size());
						d->r = src->r;
						d->g = src->g;
						d->b = src->b;
						d->a = src->a;
						d->ionSize = src->ionSize;
						d->valueType=src->valueType;

						//We are going to rotate the incoming point data
						//around the specified origin.
						ASSERT(vectorParams.size() == 2);
						ASSERT(scalarParams.size() == 1);
						Point3D axis =vectorParams[1];
						axis.normalise();
						float angle=scalarParams[0]*M_PI/180.0f;

						unsigned int curProg=NUM_CALLBACK;
						size_t n=0;

						Point3f rotVec,p;
						rotVec.fx=axis[0];
						rotVec.fy=axis[1];
						rotVec.fz=axis[2];

						Quaternion q1;

						//Generate the rotating quaternion
						quat_get_rot_quat(&rotVec,-angle,&q1);
						ASSERT(src->data.size() <= totalSize);


						size_t pos=0;

						//Copy across the ions into the target
						for(vector<IonHit>::const_iterator it=src->data.begin();
							       it!=src->data.end(); ++it)
						{
							p.fx=it->getPosRef()[0]-origin[0];
							p.fy=it->getPosRef()[1]-origin[1];
							p.fz=it->getPosRef()[2]-origin[2];
							quat_rot_apply_quat(&p,&q1);
							//set the position for the given ion
							d->data[pos].setPos(p.fx+origin[0],
									p.fy+origin[1],p.fz+origin[2]);
							d->data[pos].setMassToCharge(it->getMassToCharge());
							//update progress every CALLBACK ions
							if(!curProg--)
							{
								n+=NUM_CALLBACK;
								progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
								if(!(*callback)())
								{
									delete d;
									return TRANSFORM_CALLBACK_FAIL;
								}
								curProg=NUM_CALLBACK;
							}
							pos++;
						}

						ASSERT(d->data.size() == src->data.size());
						if(cache)
						{
							d->cached=1;
							filterOutputs.push_back(d);
							cacheOK=true;
						}
						else
							d->cached=0;
						
						getOut.push_back(d);
						break;
					}
					default:
						getOut.push_back(dataIn[ui]);
						break;
				}

				break;
			}
		}
	}
	return 0;
}


void TransformFilter::getProperties(FilterProperties &propertyList) const
{
	propertyList.data.clear();
	propertyList.keys.clear();
	propertyList.types.clear();

	vector<unsigned int> type,keys;
	vector<pair<string,string> > s;

	string tmpStr;
	vector<pair<unsigned int,string> > choices;
	choices.push_back(make_pair((unsigned int) TRANSFORM_TRANSLATE,"Translate"));
	choices.push_back(make_pair((unsigned int)TRANSFORM_SCALE,"Scale"));
	choices.push_back(make_pair((unsigned int)TRANSFORM_ROTATE,"Rotate"));
	
	tmpStr= choiceString(choices,transformMode);
	
	s.push_back(make_pair(string("Mode"),tmpStr));
	type.push_back(PROPERTY_TYPE_CHOICE);
	keys.push_back(KEY_TRANSFORM_MODE);
	
	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);
	s.clear();type.clear();keys.clear();

	//non-translation transforms require a user to select an origin	
	if(transformMode != TRANSFORM_TRANSLATE)
	{
		vector<pair<unsigned int,string> > choices;
		for(unsigned int ui=0;ui<TRANSFORM_ORIGINMODE_END;ui++)
			choices.push_back(make_pair(ui,getOriginTypeString(ui)));
		
		tmpStr= choiceString(choices,originMode);

		s.push_back(make_pair(string("Origin mode"),tmpStr));
		type.push_back(PROPERTY_TYPE_CHOICE);
		keys.push_back(KEY_TRANSFORM_ORIGINMODE);
	
		stream_cast(tmpStr,showOrigin);	
		s.push_back(make_pair(string("Show marker"),tmpStr));
		type.push_back(PROPERTY_TYPE_BOOL);
		keys.push_back(KEY_TRANSFORM_SHOWORIGIN);
	}

	

	switch(transformMode)
	{
		case TRANSFORM_TRANSLATE:
		{
			ASSERT(vectorParams.size() == 1);
			ASSERT(scalarParams.size() == 0);
			
			stream_cast(tmpStr,vectorParams[0]);
			keys.push_back(KEY_TRANSFORM_ORIGIN);
			s.push_back(make_pair("Translation", tmpStr));
			type.push_back(PROPERTY_TYPE_POINT3D);
			break;
		}
		case TRANSFORM_SCALE:
		{
			ASSERT(vectorParams.size() == 1);
			ASSERT(scalarParams.size() == 1);
			

			if(originMode == TRANSFORM_ORIGINMODE_SELECT)
			{
				stream_cast(tmpStr,vectorParams[0]);
				keys.push_back(KEY_TRANSFORM_ORIGIN);
				s.push_back(make_pair("Origin", tmpStr));
				type.push_back(PROPERTY_TYPE_POINT3D);
			}

			stream_cast(tmpStr,scalarParams[0]);
			keys.push_back(KEY_TRANSFORM_SCALEFACTOR);
			s.push_back(make_pair("Scale Fact.", tmpStr));
			type.push_back(PROPERTY_TYPE_REAL);
			break;
		}
		case TRANSFORM_ROTATE:
		{
			ASSERT(vectorParams.size() == 2);
			ASSERT(scalarParams.size() == 1);
			if(originMode == TRANSFORM_ORIGINMODE_SELECT)
			{
				stream_cast(tmpStr,vectorParams[0]);
				keys.push_back(KEY_TRANSFORM_ORIGIN);
				s.push_back(make_pair("Origin", tmpStr));
				type.push_back(PROPERTY_TYPE_POINT3D);
			}
			stream_cast(tmpStr,vectorParams[1]);
			keys.push_back(KEY_TRANSFORM_ROTATE_AXIS);
			s.push_back(make_pair("Axis", tmpStr));
			type.push_back(PROPERTY_TYPE_POINT3D);

			stream_cast(tmpStr,scalarParams[0]);
			keys.push_back(KEY_TRANSFORM_ROTATE_ANGLE);
			s.push_back(make_pair("Angle (deg)", tmpStr));
			type.push_back(PROPERTY_TYPE_REAL);
			break;
			break;
		}
	
		default:
			ASSERT(false);
	}

	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);
}

bool TransformFilter::setProperty( unsigned int set, unsigned int key,
					const std::string &value, bool &needUpdate)
{

	needUpdate=false;
	switch(key)
	{
		case KEY_TRANSFORM_MODE:
		{

			if(value == "Translate")
				transformMode= TRANSFORM_TRANSLATE;
			else if ( value == "Scale" )
				transformMode= TRANSFORM_SCALE;
			else if ( value == "Rotate")
				transformMode= TRANSFORM_ROTATE;
			else
				return false;

			vectorParams.clear();
			scalarParams.clear();
			switch(transformMode)
			{
				case TRANSFORM_SCALE:
					vectorParams.push_back(Point3D(0,0,0));
					scalarParams.push_back(1.0f);
					break;
				case TRANSFORM_TRANSLATE:
					vectorParams.push_back(Point3D(0,0,0));
					break;
				case TRANSFORM_ROTATE:
					vectorParams.push_back(Point3D(0,0,0));
					vectorParams.push_back(Point3D(1,0,0));
					scalarParams.push_back(0.0f);
					break;
				default:
					ASSERT(false);
			}
			needUpdate=true;	
			clearCache();
			break;
		}
		//The rotation angle, and the scale factor are both stored
		//in scalaraparams[0]. All we need ot do is set that,
		//as either can take any valid floating pt value
		case KEY_TRANSFORM_ROTATE_ANGLE:
		case KEY_TRANSFORM_SCALEFACTOR:
		{
			float newScale;
			if(stream_cast(newScale,value))
				return false;

			if(scalarParams[0] != newScale )
			{
				scalarParams[0] = newScale;
				needUpdate=true;
				clearCache();
			}
			return true;
		}
		case KEY_TRANSFORM_ORIGIN:
		{
			Point3D newPt;
			if(!newPt.parse(value))
				return false;

			if(!(vectorParams[0] == newPt ))
			{
				vectorParams[0] = newPt;
				needUpdate=true;
				clearCache();
			}

			return true;
		}
		case KEY_TRANSFORM_ROTATE_AXIS:
		{
			ASSERT(vectorParams.size() ==2);
			ASSERT(scalarParams.size() ==1);
			Point3D newPt;
			if(!newPt.parse(value))
				return false;

			if(newPt.sqrMag() < std::numeric_limits<float>::epsilon())
				return false;

			if(!(vectorParams[1] == newPt ))
			{
				vectorParams[1] = newPt;
				needUpdate=true;
				clearCache();
			}

			return true;
		}
			break;
		case KEY_TRANSFORM_ORIGINMODE:
		{
			size_t i;
			for (i = 0; i < TRANSFORM_MODE_ENUM_END; i++)
				if (value == getOriginTypeString(i)) break;
		
			if( i == TRANSFORM_MODE_ENUM_END)
				return false;

			if(originMode != i)
			{
				originMode = i;
				needUpdate=true;
				clearCache();
			}
			return true;
		}
		case KEY_TRANSFORM_SHOWORIGIN:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			if(stripped=="1")
				showOrigin=true;
			else
				showOrigin=false;

			needUpdate=true;

			break;
		}
		default:
			ASSERT(false);
	}	
	return true;
}


std::string  TransformFilter::getErrString(unsigned int code) const
{

	//Currently the only error is aborting
	return std::string("Aborted");
}

bool TransformFilter::writeState(std::ofstream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<transform>" << endl;
			f << tabs(depth+1) << "<userstring value=\""<<userString << "\"/>"  << endl;
			f << tabs(depth+1) << "<transformmode value=\"" << transformMode<< "\"/>"<<endl;
			f << tabs(depth+1) << "<originmode value=\"" << originMode<< "\"/>"<<endl;

			string tmpStr;
			if(showOrigin)
				tmpStr="1";
			else
				tmpStr="0";
			f << tabs(depth+1) << "<showorigin value=\"" << tmpStr <<  "\"/>"<<endl;
				
			f << tabs(depth+1) << "<vectorparams>" << endl;
			for(unsigned int ui=0; ui<vectorParams.size(); ui++)
			{
				f << tabs(depth+2) << "<point3d x=\"" << vectorParams[ui][0] << 
					"\" y=\"" << vectorParams[ui][1] << "\" z=\"" << vectorParams[ui][2] << "\"/>" << endl;
			}
			f << tabs(depth+1) << "</vectorparams>" << endl;

			f << tabs(depth+1) << "<scalarparams>" << endl;
			for(unsigned int ui=0; ui<scalarParams.size(); ui++)
				f << tabs(depth+2) << "<scalar value=\"" << scalarParams[0] << "\"/>" << endl; 
			
			f << tabs(depth+1) << "</scalarparams>" << endl;
			f << tabs(depth) << "</transform>" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool TransformFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
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

	std::string tmpStr;	
	//Retrieve transformation type 
	//====
	if(!XMLGetNextElemAttrib(nodePtr,transformMode,"transformmode","value"))
		return false;
	if(transformMode>= TRANSFORM_MODE_ENUM_END)
	       return false;	
	//====
	
	//Retrieve origination type 
	//====
	if(!XMLGetNextElemAttrib(nodePtr,originMode,"originmode","value"))
		return false;
	if(originMode>= TRANSFORM_ORIGINMODE_END)
	       return false;	
	//====
	
	//Retrieve origination type 
	//====
	if(!XMLGetNextElemAttrib(nodePtr,originMode,"showorigin","value"))
		return false;
	//====
	
	//Retreive vector parameters
	//===
	if(XMLHelpFwdToElem(nodePtr,"vectorparams"))
		return false;
	xmlNodePtr tmpNode=nodePtr;

	nodePtr=nodePtr->xmlChildrenNode;

	vectorParams.clear();
	while(!XMLHelpFwdToElem(nodePtr,"point3d"))
	{
		float x,y,z;
		//--Get X value--
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"x");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;
		xmlFree(xmlString);

		//Check it is streamable
		if(stream_cast(x,tmpStr))
			return false;

		//--Get Z value--
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"y");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;
		xmlFree(xmlString);

		//Check it is streamable
		if(stream_cast(y,tmpStr))
			return false;

		//--Get Y value--
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"z");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;
		xmlFree(xmlString);

		//Check it is streamable
		if(stream_cast(z,tmpStr))
			return false;

		vectorParams.push_back(Point3D(x,y,z));
	}
	//===	

	nodePtr=tmpNode;
	//Retreive scalar parameters
	//===
	if(XMLHelpFwdToElem(nodePtr,"scalarparams"))
		return false;
	
	tmpNode=nodePtr;
	nodePtr=nodePtr->xmlChildrenNode;

	scalarParams.clear();
	while(!XMLHelpFwdToElem(nodePtr,"scalar"))
	{
		float v;
		//Get value
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;
		xmlFree(xmlString);

		//Check it is streamable
		if(stream_cast(v,tmpStr))
			return false;
		scalarParams.push_back(v);
	}
	//===	

	//Check the scalar params match the selected primitive	
	switch(transformMode)
	{
		case TRANSFORM_TRANSLATE:
			if(vectorParams.size() != 1 || scalarParams.size() !=0)
				return false;
			break;
		case TRANSFORM_SCALE:
			if(vectorParams.size() != 1 || scalarParams.size() !=1)
				return false;
			break;
		case TRANSFORM_ROTATE:
			if(vectorParams.size() != 1 || scalarParams.size() !=1)
				return false;
			break;
		default:
			ASSERT(false);
			return false;
	}
	return true;
}


void TransformFilter::setPropFromBinding(const SelectionBinding &b)
{
	switch(b.getID())
	{
		case BINDING_SPHERE_ORIGIN:
			b.getValue(vectorParams[0]);
			break;
		default:
			ASSERT(false);
	}
	clearCache();
}

std::string TransformFilter::getOriginTypeString(unsigned int i) const
{
	switch(i)
	{
		case TRANSFORM_ORIGINMODE_SELECT:
			return std::string("Specify");
		case TRANSFORM_ORIGINMODE_CENTREBOUND:
			return std::string("Boundbox Centre");
		case TRANSFORM_ORIGINMODE_MASSCENTRE:
			return std::string("Mass Centre");
	}
	ASSERT(false);
}


//=== External program filter === 
ExternalProgramFilter::ExternalProgramFilter()
{
	alwaysCache=false;
	cleanInput=true;
	cacheOK=false;
	cache=false; 
}

Filter *ExternalProgramFilter::cloneUncached() const
{
	ExternalProgramFilter *p=new ExternalProgramFilter();

	//Copy the values
	p->workingDir=workingDir;
	p->commandLine=commandLine;
	p->alwaysCache=alwaysCache;
	p->cleanInput=cleanInput;

	//We are copying wether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

size_t ExternalProgramFilter::numBytesForCache(size_t nObjects) const
{
	if(alwaysCache)
		return 0;
	else
		return (size_t)-1; //Say we don't know, we are not going to cache anyway.
}

unsigned int ExternalProgramFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(void))
{
	//use the cached copy if we have it.
	if(cacheOK)
	{
		for(unsigned int ui=0;ui<filterOutputs.size();ui++)
			getOut.push_back(filterOutputs[ui]);

		return 0;
	}

	vector<string> commandLineSplit;

	splitStrsRef(commandLine.c_str(),' ',commandLineSplit);
	//Nothing to do
	if(!commandLineSplit.size())
		return 0;

	vector<string> ionOutputNames,plotOutputNames;

	//Compute the bounding box of the incoming streams
	string s;
	wxString tempDir;
	if(workingDir.size())
		tempDir=(wxStr(workingDir) +_("/inputData"));
	else
		tempDir=(_("inputData"));


	//Create a temporary dir
	if(!wxDirExists(tempDir) )
	{
		//Audacity claims that this can return false even on
		//success (NoiseRemoval.cpp, line 148).
		//I was having problems with this function too;
		//so use their workaround
		wxMkdir(tempDir);

		if(!wxDirExists(tempDir) )	
			return EXTERNALPROG_MAKEDIR_FAIL;

	}
	
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS:
			{
				const IonStreamData *i;
				i = (const IonStreamData * )(dataIn[ui]);

				if(!i->data.size())
					break;
				//Save the data to a file
				wxString tmpStr;

				tmpStr=wxFileName::CreateTempFileName(tempDir+ _("/pointdata"));
				//wxwidgets has no suffix option... annoying.
				wxRemoveFile(wxStr(tmpStr));

				s = stlStr(tmpStr);
				s+=".pos";
				if(IonVectorToPos(i->data,s))
				{
					//Uh-oh problem. Clean up and exit
					return EXTERNALPROG_WRITEPOS_FAIL;
				}

				ionOutputNames.push_back(s);
				break;
			}
			case STREAM_TYPE_PLOT:
			{
				const PlotStreamData *i;
				i = (const PlotStreamData * )(dataIn[ui]);

				if(!i->xyData.size())
					break;
				//Save the data to a file
				wxString tmpStr;

				tmpStr=wxFileName::CreateTempFileName(tempDir + _("/plot"));
				//wxwidgets has no suffix option... annoying.
				wxRemoveFile(wxStr(tmpStr));
				s = stlStr(tmpStr);
			        s+= ".xy";
				if(!writeTextFile(s.c_str(),i->xyData))
				{
					//Uh-oh problem. Clean up and exit
					return EXTERNALPROG_WRITEPLOT_FAIL;
				}

				plotOutputNames.push_back(s);
				break;
			}
			default:
				break;
		}	
	}

	//Nothing to do.
	if(!(plotOutputNames.size() ||
		ionOutputNames.size()))
		return 0;


	//Construct the command, using substitution
	string command;
	unsigned int ionOutputPos,plotOutputPos;
	ionOutputPos=plotOutputPos=0;
	command = commandLineSplit[0];
	for(unsigned int ui=1;ui<commandLineSplit.size();ui++)
	{
		if(commandLineSplit[ui].find('%') == std::string::npos)
		{
			command+= string(" ") + commandLineSplit[ui];
		}
		else
		{
			size_t pos;
			pos=0;

			string thisCommandEntry;
			pos =commandLineSplit[ui].find("%",pos);
			while(pos != string::npos)
			{
				//OK, so we found a % symbol.
				//lets do some substitution
				
				//% must be followed by something otherwise this is an error
				if(pos == commandLineSplit[ui].size())
					return EXTERNALPROG_COMMANDLINE_FAIL;

				char code;
				code = commandLineSplit[ui][pos+1];

				switch(code)
				{
					case '%':
						//Escape '%%' to '%' symbol
						thisCommandEntry+="%";
						pos+=2;
						break;
					case 'i':
					{
						//Substitute '%i' with ion file name
						if(ionOutputPos == ionOutputNames.size())
						{
							//User error; not enough pos files to fill.
							return EXTERNALPROG_SUBSTITUTE_FAIL;
						}

						thisCommandEntry+=ionOutputNames[ionOutputPos];
						ionOutputPos++;
						pos++;
						break;
					}
					case 'I':
					{
						//Substitute '%I' with all ion file names, space separated
						if(ionOutputPos == ionOutputNames.size())
						{
							//User error. not enough pos files to fill
							return EXTERNALPROG_SUBSTITUTE_FAIL;
						}
						for(unsigned int ui=ionOutputPos; ui<ionOutputNames.size();ui++)
							thisCommandEntry+=ionOutputNames[ui] + " ";

						ionOutputPos=ionOutputNames.size();
						pos++;
						break;
					}
					case 'p':
					{
						//Substitute '%p' with all plot file names, space separated
						if(plotOutputPos == plotOutputNames.size())
						{
							//User error. not enough pos files to fill
							return EXTERNALPROG_SUBSTITUTE_FAIL;
						}
						for(unsigned int ui=plotOutputPos; ui<plotOutputNames.size();ui++)
							thisCommandEntry+=plotOutputNames[ui];

						plotOutputPos=plotOutputNames.size();
						pos++;
						break;
					}
					case 'P': 
					{
						//Substitute '%I' with all plot file names, space separated
						if(plotOutputPos == plotOutputNames.size())
						{
							//User error. not enough pos files to fill
							return EXTERNALPROG_SUBSTITUTE_FAIL;
						}
						for(unsigned int ui=plotOutputPos; ui<plotOutputNames.size();ui++)
							thisCommandEntry+=plotOutputNames[ui]+ " ";

						plotOutputPos=plotOutputNames.size();
						pos++;
						break;
					}
					default:
						//Invalid user input string. % must be escaped or recognised.
						return EXTERNALPROG_SUBSTITUTE_FAIL;
				}


				pos =commandLineSplit[ui].find("%",pos);
			}
			
			command+= string(" ") + thisCommandEntry;
		}

	}

	//If we have a specific working dir; use it. Otherwise just use curdir
	wxString origDir = wxGetCwd();
	if(workingDir.size())
	{
		//Set the working directory before launching
		if(!wxSetWorkingDirectory(wxStr(workingDir)))
			return EXTERNALPROG_SETWORKDIR_FAIL;
	}
	else
	{
		if(!wxSetWorkingDirectory(_(".")))
			return EXTERNALPROG_SETWORKDIR_FAIL;
	}

	bool result;
	//Execute the program
	result=wxShell(wxStr(command));

	if(cleanInput)
	{
		//If program returns error, this was a problem
		//delete the input files.
		for(unsigned int ui=0;ui<ionOutputNames.size();ui++)
		{
			//try to delete the file
			wxRemoveFile(wxStr(ionOutputNames[ui]));

			//call the update to be nice
			(*callback)();
		}
		for(unsigned int ui=0;ui<plotOutputNames.size();ui++)
		{
			//try to delete the file
			wxRemoveFile(wxStr(plotOutputNames[ui]));

			//call the update to be nice
			(*callback)();
		}
	}
	wxSetWorkingDirectory(origDir);	
	if(!result)
		return EXTERNALPROG_COMMAND_FAIL; 
	
	wxSetWorkingDirectory(origDir);	

	wxDir *dir = new wxDir;
	wxArrayString *a = new wxArrayString;
	if(workingDir.size())
		dir->GetAllFiles(wxStr(workingDir),a,_("*.pos"),wxDIR_FILES);
	else
		dir->GetAllFiles(wxGetCwd(),a,_("*.pos"),wxDIR_FILES);


	//read the output files, which is assumed to be any "pos" file
	//in the working dir
	for(unsigned int ui=0;ui<a->Count(); ui++)
	{
		wxULongLong size;
		size = wxFileName::GetSize((*a)[ui]);

		if( (size !=0) && size!=wxInvalidSize)
		{
			//Load up the pos file

			string sTmp;
			wxString wxTmpStr;
			wxTmpStr=(*a)[ui];
			sTmp = stlStr(wxTmpStr);
			unsigned int dummy;
			IonStreamData *d = new IonStreamData();
			//TODO: some kind of secondary file for specification of
			//ion attribs?
			d->r = 1.0;
			d->g=0;
			d->b=0;
			d->a=1.0;
			d->ionSize = 2.0;

			int index2[] = {
					0, 1, 2, 3
					};
			if(GenericLoadFloatFile(4, 4, index2, d->data,sTmp.c_str(),dummy,0))
				return EXTERNALPROG_READPOS_FAIL;


			if(alwaysCache)
			{
				d->cached=1;
				filterOutputs.push_back(d);
			}
			else
				d->cached=0;
			getOut.push_back(d);
		}
	}

	a->Clear();
	if(workingDir.size())
		dir->GetAllFiles(wxStr(workingDir),a,_("*.xy"),wxDIR_FILES);
	else
		dir->GetAllFiles(wxGetCwd(),a,_("*.xy"),wxDIR_FILES);

	//read the output files, which is assumed to be any "pos" file
	//in the working dir
	for(unsigned int ui=0;ui<a->Count(); ui++)
	{
		wxULongLong size;
		size = wxFileName::GetSize((*a)[ui]);

		if( (size !=0) && size!=wxInvalidSize)
		{
			string sTmp;
			wxString wxTmpStr;
			wxTmpStr=(*a)[ui];
			sTmp = stlStr(wxTmpStr);

			vector<vector<float> > dataVec;

			vector<string> header;	

			//Possible delimiters to try when loading file
			//try each in turn
			const char *delimString ="\t, ";
			unsigned int uj=0;
			while(delimString[uj])
			{
				if(!loadTextData(sTmp.c_str(),dataVec,header,delimString[uj]))
					break;

				dataVec.clear();
				header.clear();
				uj++;
			}

			//well we ran out of delimiters. bad luck
			if(!delimString[uj] || !dataVec.size())
				return EXTERNALPROG_READPLOT_FAIL;


			//Check that the input has the correct size
			for(unsigned int uj=0;uj<dataVec.size()-1;uj+=2)
			{
				//well the columns don't match
				if(dataVec[uj].size() != dataVec[uj+1].size())
					return EXTERNALPROG_PLOTCOLUMNS_FAIL;
			}

			//Check to see if the header might be able
			//to be matched to the data
			bool applyLabels=false;
			if(header.size() == dataVec.size())
				applyLabels=true;

			for(unsigned int uj=0;uj<dataVec.size()-1;uj+=2)
			{
				//TODO: some kind of secondary file for specification of
				//plot attribs?
				PlotStreamData *d = new PlotStreamData();
				d->r = 0.0;
				d->g=1.0;
				d->b=0;
				d->a=1.0;


				//set the title to the filename (trim the .xy extention
				//and the working directory name)
				string tmpFilename;
				tmpFilename=sTmp.substr(workingDir.size(),sTmp.size()-
								workingDir.size()-3);

				d->dataLabel=getUserString() + string(":") + onlyFilename(tmpFilename);

				if(applyLabels)
				{

					//set the xy-labels to the column headers
					d->xLabel=header[uj];
					d->yLabel=header[uj+1];
				}
				else
				{
					d->xLabel="x";
					d->yLabel="y";
				}

				d->xyData.resize(dataVec[uj].size());

				ASSERT(dataVec[uj].size() == dataVec[uj+1].size());
				for(unsigned int uk=0;uk<dataVec[uj].size(); uk++)
				{
					d->xyData[uk]=make_pair(dataVec[uj][uk], 
								dataVec[uj+1][uk]);
				}
				
				if(alwaysCache)
				{
					d->cached=1;
					filterOutputs.push_back(d);
				}
				else
					d->cached=0;
				
				getOut.push_back(d);
			}
		}
	}

	if(alwaysCache)
		cacheOK=true;

	delete dir;
	delete a;

	return 0;
}


void ExternalProgramFilter::getProperties(FilterProperties &propertyList) const
{
	propertyList.data.clear();
	propertyList.keys.clear();
	propertyList.types.clear();

	vector<unsigned int> type,keys;
	vector<pair<string,string> > s;

	std::string tmpStr;
	
	s.push_back(make_pair("Command", commandLine));
	type.push_back(PROPERTY_TYPE_STRING);
	keys.push_back(KEY_EXTERNALPROGRAM_COMMAND);		
	
	s.push_back(make_pair("Work Dir", workingDir));
	type.push_back(PROPERTY_TYPE_STRING);
	keys.push_back(KEY_EXTERNALPROGRAM_WORKDIR);		
	
	propertyList.types.push_back(type);
	propertyList.data.push_back(s);
	propertyList.keys.push_back(keys);

	type.clear();s.clear();keys.clear();

	if(cleanInput)
		tmpStr="1";
	else
		tmpStr="0";

	s.push_back(make_pair("Cleanup input",tmpStr));
	type.push_back(PROPERTY_TYPE_BOOL);
	keys.push_back(KEY_EXTERNALPROGRAM_CLEANUPINPUT);		
	if(alwaysCache)
		tmpStr="1";
	else
		tmpStr="0";
	
	s.push_back(make_pair("Cache",tmpStr));
	type.push_back(PROPERTY_TYPE_BOOL);
	keys.push_back(KEY_EXTERNALPROGRAM_ALWAYSCACHE);		

	propertyList.types.push_back(type);
	propertyList.data.push_back(s);
	propertyList.keys.push_back(keys);
}

bool ExternalProgramFilter::setProperty( unsigned int set, unsigned int key,
					const std::string &value, bool &needUpdate)
{
	needUpdate=false;
	switch(key)
	{
		case KEY_EXTERNALPROGRAM_COMMAND:
		{
			if(commandLine!=value)
			{
				commandLine=value;
				needUpdate=true;
				clearCache();
			}
			break;
		}
		case KEY_EXTERNALPROGRAM_WORKDIR:
		{
			if(workingDir!=value)
			{
				//Check the directory exists
				if(!wxDirExists(wxStr(value)))
					return false;

				workingDir=value;
				needUpdate=true;
				clearCache();
			}
			break;
		}
		case KEY_EXTERNALPROGRAM_ALWAYSCACHE:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			if(stripped=="1")
				alwaysCache=true;
			else
			{
				alwaysCache=false;

				//If we need to generate a cache, do so
				//otherwise, trash it
				clearCache();
			}

			needUpdate=true;
			break;
		}
		case KEY_EXTERNALPROGRAM_CLEANUPINPUT:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			if(stripped=="1")
				cleanInput=true;
			else
				cleanInput=false;
			
			break;
		}
		default:
			ASSERT(false);

	}

	return true;
}


std::string  ExternalProgramFilter::getErrString(unsigned int code) const
{

	switch(code)
	{
		case EXTERNALPROG_COMMANDLINE_FAIL:
			return std::string("Error processing command line");
		case EXTERNALPROG_SETWORKDIR_FAIL:
			return std::string("Unable to set working directory");
		case EXTERNALPROG_WRITEPOS_FAIL:
			return std::string("Error saving posfile result for external progrma");
		case EXTERNALPROG_WRITEPLOT_FAIL:
			return std::string("Error saving plot result for externalprogrma");
		case EXTERNALPROG_MAKEDIR_FAIL:
			return std::string("Error creating temporary directory");
		case EXTERNALPROG_PLOTCOLUMNS_FAIL:
			return std::string("Detected unusable number of columns in plot");
		case EXTERNALPROG_READPLOT_FAIL:
			return std::string("Unable to parse plot result from external program");
		case EXTERNALPROG_READPOS_FAIL:
			return std::string("Unable to load ions from external program"); 
		case EXTERNALPROG_SUBSTITUTE_FAIL:
			return std::string("Unable to perform commandline substitution");
		case EXTERNALPROG_COMMAND_FAIL: 
			return std::string("Error executing external program");
		default:
			//Currently the only error is aborting
			return std::string("Bug: write me (externalProgramfilter).");
	}
}

bool ExternalProgramFilter::writeState(std::ofstream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{
			f << tabs(depth) << "<externalprog>" << endl;

			f << tabs(depth+1) << "<userstring value=\""<<userString << "\"/>"  << endl;
			f << tabs(depth+1) << "<commandline name=\"" << commandLine << "\"/>" << endl;
			f << tabs(depth+1) << "<workingdir name=\"" << workingDir << "\"/>" << endl;
			f << tabs(depth+1) << "<alwayscache value=\"" << alwaysCache << "\"/>" << endl;
			f << tabs(depth+1) << "<cleaninput value=\"" << cleanInput << "\"/>" << endl;
			f << tabs(depth) << "</externalprog>" << endl;
			break;

		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool ExternalProgramFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
	//Retrieve user string
	if(XMLHelpFwdToElem(nodePtr,"userstring"))
		return false;

	xmlChar *xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	userString=(char *)xmlString;
	xmlFree(xmlString);

	//Retrieve command
	if(XMLHelpFwdToElem(nodePtr,"commandline"))
		return false;
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"name");
	if(!xmlString)
		return false;
	commandLine=(char *)xmlString;
	xmlFree(xmlString);
	
	//Retrieve working dir
	if(XMLHelpFwdToElem(nodePtr,"workingdir"))
		return false;
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"name");
	if(!xmlString)
		return false;
	workingDir=(char *)xmlString;
	xmlFree(xmlString);


	//get should cache
	string tmpStr;
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"alwayscache","value"))
		return false;

	if(tmpStr == "1") 
		alwaysCache=true;
	else if(tmpStr== "0")
		alwaysCache=false;
	else
		return false;

	//check readable 
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"cleaninput","value"))
		return false;

	if(tmpStr == "1") 
		cleanInput=true;
	else if(tmpStr== "0")
		cleanInput=false;
	else
		return false;

	return true;
}



// == NN analysis filter ==

SpatialAnalysisFilter::SpatialAnalysisFilter()
{
	algorithm=SPATIAL_ANALYSIS_DENSITY;
	nnMax=1;
	distMax=1;
	stopMode=SPATIAL_DENSITY_NEIGHBOUR;

	//Default colour is red
	r=a=1.0f;
	g=b=0.0f;

	haveRangeParent=false;
	numBins=100;
	excludeSurface=false;
	reductionDistance=distMax;

	cacheOK=false;
	cache=true; //By default, we should cache, but decision is made higher up

}

Filter *SpatialAnalysisFilter::cloneUncached() const
{
	SpatialAnalysisFilter *p=new SpatialAnalysisFilter;

	p->algorithm=algorithm;
	p->stopMode=stopMode;
	p->nnMax=nnMax;
	p->distMax=distMax;

	//We are copying wether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

size_t SpatialAnalysisFilter::numBytesForCache(size_t nObjects) const
{
	return nObjects*sizeof(IONHIT);
}

void SpatialAnalysisFilter::initFilter(const std::vector<const FilterStreamData *> &dataIn,
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
				//well, things have changed, we didn't have a 
				//range parent before.
				different=true;
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

				ionSourceEnabled.resize(ionNames.size(),true);
				ionTargetEnabled.resize(ionNames.size(),true);
			}

			return;
		}
	}
	haveRangeParent=false;
}

unsigned int SpatialAnalysisFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(void))
{
	//use the cached copy if we have it.
	if(cacheOK)
	{
		for(unsigned int ui=0;ui<dataIn.size();ui++)
		{
			if(dataIn[ui]->getStreamType() != STREAM_TYPE_IONS &&
				dataIn[ui]->getStreamType() !=STREAM_TYPE_RANGE)
				getOut.push_back(dataIn[ui]);
		}
		for(unsigned int ui=0;ui<filterOutputs.size();ui++)
			getOut.push_back(filterOutputs[ui]);
		return 0;
	}


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

	const RangeFile *rngF=0;
	if(haveRangeParent)
	{
		//Check we actually have something to do
		if(!std::count(ionSourceEnabled.begin(),
					ionSourceEnabled.end(),true))
			return 0;
		if(!std::count(ionTargetEnabled.begin(),
					ionTargetEnabled.end(),true))
			return 0;

		rngF=getRangeFile(dataIn);
	}

	//Run the algorithm
	if(algorithm == SPATIAL_ANALYSIS_DENSITY)
	{
		//Build monolithic point set
		//---
		vector<Point3D> p;
		p.resize(totalDataSize);

		size_t dataSize=0;
		size_t n=0;

		progress.step=1;
		progress.stepName="Collate";
		progress.maxStep=3;
		(*callback)();

		for(unsigned int ui=0;ui<dataIn.size() ;ui++)
		{
			switch(dataIn[ui]->getStreamType())
			{
				case STREAM_TYPE_IONS: 
				{
					const IonStreamData *d;
					d=((const IonStreamData *)dataIn[ui]);

					if(extendPointVector(p,d->data,
							callback,progress.filterProgress,
							dataSize))
						return SPATIAL_ANALYSIS_ABORT_ERR;

					dataSize+=d->data.size();
				}
				break;	
				default:
					break;
			}
		}
		//---

		progress.step=2;
		progress.stepName="Build";
		(*callback)();

		BoundCube treeDomain;
		treeDomain.setBounds(p);

		//Build the tree (its roughly nlogn timing, but worst case n^2)
		K3DTree kdTree;
		kdTree.setCallbackMethod(callback);
		kdTree.setProgressPointer(&(progress.filterProgress));
		
		kdTree.buildByRef(p);


		//Update progress & User interface by calling callback
		if(!(*callback)())
			return SPATIAL_ANALYSIS_ABORT_ERR;
		p.clear(); //We don't need pts any more, as tree *is* a copy.


		//Its algorithim time!
		//----
		//Update progress stuff
		n=0;
		progress.step=3;
		progress.stepName="Analyse";
		(*callback)();

		//List of points for which there was a failure
		//first entry is the point Id, second is the 
		//dataset id.
		std::list<std::pair<size_t,size_t> > badPts;
		for(size_t ui=0;ui<dataIn.size() ;ui++)
		{
			switch(dataIn[ui]->getStreamType())
			{
				case STREAM_TYPE_IONS: 
				{
					const IonStreamData *d;
					d=((const IonStreamData *)dataIn[ui]);
					IonStreamData *newD = new IonStreamData;

					//Adjust this number to provide more update thanusual, because we
					//are not doing an o(1) task between updates; yes, it is  hack
					unsigned int curProg=NUM_CALLBACK/(10*nnMax);
					newD->data.resize(d->data.size());
					if(stopMode == SPATIAL_DENSITY_NEIGHBOUR)
					{
						bool spin=false;
						#pragma omp parallel for
						for(size_t uj=0;uj<d->data.size();uj++)
						{
							if(spin)
								continue;
							Point3D r;
							vector<const Point3D *> res;
							r=d->data[uj].getPosRef();
							
							//Assign the mass to charge using nn density estimates
							kdTree.findKNearest(r,treeDomain,nnMax,res);

							if(res.size())
							{	
								float maxSqrRad;

								//Get the radius as the furtherst object
								maxSqrRad= (res[res.size()-1]->sqrDist(r));

								//Set the mass as the volume of sphere * the number of NN
								newD->data[uj].setMassToCharge(res.size()/(4.0/3.0*M_PI*powf(maxSqrRad,3.0/2.0)));
								//Keep original position
								newD->data[uj].setPos(r);
							}
							else
							{
								#pragma omp critical
								badPts.push_back(make_pair(uj,ui));
							}

							res.clear();
							
							//Update callback as needed
							if(!curProg--)
							{
								#pragma omp critical 
								{
								n+=NUM_CALLBACK/(nnMax);
								progress.filterProgress= (unsigned int)(((float)n/(float)totalDataSize)*100.0f);
								if(!(*callback)())
									spin=true;
								curProg=NUM_CALLBACK/(nnMax);
								}
							}
						}

						if(spin)
						{
							delete newD;
							return SPATIAL_ANALYSIS_ABORT_ERR;
						}


					}
					else if(stopMode == SPATIAL_DENSITY_RADIUS)
					{
						float maxSqrRad = distMax*distMax;
						float vol = 4.0/3.0*M_PI*maxSqrRad*distMax; //Sphere volume=4/3 Pi R^3
						for(size_t uj=0;uj<d->data.size();uj++)
						{
							Point3D r;
							const Point3D *res;
							float deadDistSqr;
							unsigned int numInRad;
							
							r=d->data[uj].getPosRef();
							numInRad=0;
							deadDistSqr=0;

							//Assign the mass to charge using nn density estimates
							do
							{
								res=kdTree.findNearest(r,treeDomain,deadDistSqr);

								//Check to see if we found something
								if(!res)
								{
									badPts.push_back(make_pair(uj, ui));
									break;
								}
								
								if(res->sqrDist(r) >maxSqrRad)
									break;
								numInRad++;
								//Advance ever so slightly beyond the next ion
								deadDistSqr = res->sqrDist(r)+std::numeric_limits<float>::epsilon();
								//Update callback as needed
								if(!curProg--)
								{
									progress.filterProgress= (unsigned int)((float)n/(float)totalDataSize*100.0f);
									if(!(*callback)())
									{
										delete newD;
										return SPATIAL_ANALYSIS_ABORT_ERR;
									}
									curProg=NUM_CALLBACK/(10*nnMax);
								}
							}while(true);

							n++;
							//Set the mass as the volume of sphere * the number of NN
							newD->data[uj].setMassToCharge(numInRad/vol);
							//Keep original position
							newD->data[uj].setPos(r);
							
						}

					}
					else
					{
						//Should not get here.
						ASSERT(false);
					}


					//move any bad points from the array to the end, then drop them
					//To do this, we have to reverse sort the array, then
					//swap the output ion vector entries with the end,
					//then do a resize.
					ComparePairFirst cmp;
					badPts.sort(cmp);
					badPts.reverse();

					//Do some swappage
					size_t pos=1;
					for(std::list<std::pair<size_t,size_t> >::iterator it=badPts.begin(); it!=badPts.end();++it)
					{
						newD->data[(*it).first]=newD->data[newD->data.size()-pos];
					}

					//Trim the tail of bad points, leaving only good points
					newD->data.resize(newD->data.size()-badPts.size());


					//Use default colours
					newD->r=d->r;
					newD->g=d->g;
					newD->b=d->b;
					newD->a=d->a;
					newD->ionSize=d->ionSize;
					newD->representationType=d->representationType;
					newD->valueType="Number Density (\\#/Vol^3)";

					//Cache result as needed
					if(cache)
					{
						newD->cached=true;
						filterOutputs.push_back(newD);
						cacheOK=true;
					}
					else
						newD->cached=false;
					getOut.push_back(newD);
				}
				break;	
				case STREAM_TYPE_RANGE: 
					//Do not propagate ranges
				break;
				default:
					getOut.push_back(dataIn[ui]);
					break;
			}
		}
		//If we have bad points, let the user know.
		if(badPts.size())
		{
			std::string sizeStr;
			stream_cast(sizeStr,badPts.size());
			consoleOutput.push_back(std::string("Warning,") + sizeStr + 
					" points were un-analysable. These have been dropped");

			//Print out a list of points if we can

			size_t maxPrintoutSize=std::min(badPts.size(),(size_t)200);
			list<pair<size_t,size_t> >::iterator it;
			it=badPts.begin();
			while(maxPrintoutSize--)
			{
				std::string s;
				const IonStreamData *d;
				d=((const IonStreamData *)dataIn[it->second]);

				Point3D getPos;
				getPos=	d->data[it->first].getPosRef();
				stream_cast(s,getPos);
				consoleOutput.push_back(s);
				++it;
			}

			if(badPts.size() > 200)
			{
				consoleOutput.push_back("And so on...");
			}


		}	
	}
	else if (algorithm == SPATIAL_ANALYSIS_RDF)
	{
		progress.step=1;
		progress.stepName="Collate";
		if(excludeSurface)
			progress.maxStep=4;
		else
			progress.maxStep=3;

		K3DTree kdTree;
		kdTree.setCallbackMethod(callback);
		kdTree.setProgressPointer(&(progress.filterProgress));
		
		//Source points
		vector<Point3D> p;
		bool needSplitting;

		needSplitting=false;
		//We only need to split up the data if we hvae to 
		if(std::count(ionSourceEnabled.begin(),ionSourceEnabled.end(),true)!=ionSourceEnabled.size())
			needSplitting=true;
		if(std::count(ionTargetEnabled.begin(),ionTargetEnabled.end(),true)!=ionTargetEnabled.size())
			needSplitting=true;

		if(haveRangeParent & needSplitting)
		{
			ASSERT(ionNames.size());
			//Build monolithic point sets
			//one for targets, one for sources
			//---
			vector<Point3D> pts[2]; //0 -> Source, 1-> Target
			size_t sizeNeeded[2];
			sizeNeeded[0]=sizeNeeded[1]=0;
			//Presize arrays
			for(unsigned int ui=0;ui<dataIn.size() ;ui++)
			{
				switch(dataIn[ui]->getStreamType())
				{
					case STREAM_TYPE_IONS: 
					{
						unsigned int rangeID;

						const IonStreamData *d;
						d=((const IonStreamData *)dataIn[ui]);
						rangeID=getIonstreamIonID(d,rngF);

						if(rangeID == (unsigned int)-1)
							break;

						if(ionSourceEnabled[rangeID])
							sizeNeeded[0]+=d->data.size();

						if(ionTargetEnabled[rangeID])
							sizeNeeded[1]+=d->data.size();
						break;	
					}
					default:
						break;
				}
			}

			pts[0].resize(sizeNeeded[0]);
			pts[1].resize(sizeNeeded[1]);

			//Fill arrays	
			size_t curPos[2];
			curPos[0]=curPos[1]=0;
			for(unsigned int ui=0;ui<dataIn.size() ;ui++)
			{
				switch(dataIn[ui]->getStreamType())
				{
					case STREAM_TYPE_IONS: 
					{
						unsigned int rangeID;
						const IonStreamData *d;
						d=((const IonStreamData *)dataIn[ui]);
						rangeID=getIonstreamIonID(d,rngF);

						if(rangeID==(unsigned int)(-1))
							break;
						if(ionSourceEnabled[rangeID])
						{
							if(extendPointVector(pts[0],d->data,callback,
									progress.filterProgress,curPos[0]))
								return SPATIAL_ANALYSIS_ABORT_ERR;
							curPos[0]+=d->data.size();
						}

						if(ionTargetEnabled[rangeID])
						{
							if(extendPointVector(pts[1],d->data,callback,
									progress.filterProgress,curPos[1]))
								return SPATIAL_ANALYSIS_ABORT_ERR;

							curPos[1]+=d->data.size();
						}
						break;
					}
					default:
						break;
				}
			}
			//---

			progress.step=2;
			progress.stepName="Build";

			//Build the tree using the target ions
			//(its roughly nlogn timing, but worst case n^2)
			kdTree.buildByRef(pts[1]);
			pts[1].clear();
			
			//Remove surface points from sources if desired
			if(excludeSurface)
			{
				ASSERT(reductionDistance > 0);
				progress.step++;
				progress.stepName="Surface";


				//Take the input points, then use them
				//to compute the convex hull reduced 
				//volume. 
				vector<Point3D> returnPoints;
				GetReducedHullPts(pts[0],reductionDistance,
						returnPoints);

				pts[0].clear();
				//Forget the original points, and use the new ones
				p.swap(returnPoints);
			}
			else
				p.swap(pts[0]);

		}
		else
		{
			//Build monolithic point set
			//---
			p.resize(totalDataSize);

			size_t dataSize=0;

			for(unsigned int ui=0;ui<dataIn.size() ;ui++)
			{
				switch(dataIn[ui]->getStreamType())
				{
					case STREAM_TYPE_IONS: 
					{
						const IonStreamData *d;
						d=((const IonStreamData *)dataIn[ui]);
						if(extendPointVector(p,d->data,callback,
							progress.filterProgress,dataSize))
							return SPATIAL_ANALYSIS_ABORT_ERR;
						dataSize+=d->data.size();
					}
					break;	
					default:
						break;
				}
			}
			//---

			progress.step=2;
			progress.stepName="Build";
			BoundCube treeDomain;
			treeDomain.setBounds(p);

			//Build the tree (its roughly nlogn timing, but worst case n^2)
			kdTree.buildByRef(p);

			//Remove surface points if desired
			if(excludeSurface)
			{
				ASSERT(reductionDistance > 0);
				progress.step++;
				progress.stepName="Surface";


				//Take the input points, then use them
				//to compute the convex hull reduced 
				//volume. 
				vector<Point3D> returnPoints;
				GetReducedHullPts(p,reductionDistance,
						returnPoints);

				//Forget the original points, and use the new ones
				p.swap(returnPoints);

			}
			
		}

		//Let us perform the desired analysis
		progress.step++;
		progress.stepName="Analyse";

		//If there is no data, there is nothing to do.
		if(!p.size() || !kdTree.nodeCount())
			return	0;
		//OK, at this point, the KD tree contains the target points
		//of interest, and the vector "p" contains the source points
		//of interest, whatever they might be.

		if (stopMode == SPATIAL_DENSITY_NEIGHBOUR)
		{
			//User is after an NN histogram analysis

			//Histogram is output as a per-NN histogram of frequency.
			unsigned int *histogram = new unsigned int[numBins*nnMax];
			
			//Bin widths for the NN histograms (each NN hist
			//is scaled separately). The +1 is due to the tail bin
			//being the totals
			float *binWidth = new float[nnMax];

			if(generateNNHist(p,kdTree,nnMax,numBins,histogram,binWidth))
				return SPATIAL_ANALYSIS_INSUFFICIENT_SIZE_ERR;

			//Alright then, we have the histogram in x-{y1,y2,y3...y_n} form
			//lets make some plots shall we?
			PlotStreamData *plotData[nnMax];

			for(unsigned int ui=0;ui<nnMax;ui++)
			{
				plotData[ui] = new PlotStreamData;
				plotData[ui]->index=ui;
				plotData[ui]->parent=this;
				plotData[ui]->xLabel="Radial Distance";
				plotData[ui]->yLabel="Count";
				std::string tmpStr;
				stream_cast(tmpStr,ui+1);
				plotData[ui]->dataLabel=getUserString() + string(" " ) +tmpStr + "NN Freq.";

				//Red plot.
				plotData[ui]->r=r;
				plotData[ui]->g=g;
				plotData[ui]->b=b;
				plotData[ui]->xyData.resize(numBins);

				for(unsigned int uj=0;uj<numBins;uj++)
				{
					float dist;
					dist = (float)uj*binWidth[ui];
					plotData[ui]->xyData[uj] = std::make_pair(dist,
							histogram[uj*nnMax+ui]);
				}

				if(cache)
				{
					plotData[ui]->cached=true;
					filterOutputs.push_back(plotData[ui]);
					cacheOK=true;
				}	
				
				getOut.push_back(plotData[ui]);
			}




		}
		else if(stopMode == SPATIAL_DENSITY_RADIUS)
		{
			unsigned int warnBiasCount=0;
			
			//Histogram is output as a histogram of frequency vs distance
			unsigned int *histogram = new unsigned int[numBins];
			for(unsigned int ui=0;ui<numBins;ui++)
				histogram[ui] =0;
		
			//User is after an RDF analysis. Run it.
			generateDistHist(p,kdTree,histogram,distMax,numBins,
					warnBiasCount);


			if(warnBiasCount)
			{
				string sizeStr;
				stream_cast(sizeStr,warnBiasCount);
			
				consoleOutput.push_back(std::string("Warning, ")
						+ sizeStr + " points were unable to find neighbour points that exceeded the search radius, and thus terminated prematurely");
			}

			PlotStreamData *plotData = new PlotStreamData;

			plotData = new PlotStreamData;
			plotData->index=0;
			plotData->parent=this;
			plotData->xLabel="Radial Distance";
			plotData->yLabel="Count";
			plotData->dataLabel=getUserString() + " RDF";

			//Red plot.
			plotData->r=r;
			plotData->g=g;
			plotData->b=b;
			plotData->xyData.resize(numBins);

			for(unsigned int uj=0;uj<numBins;uj++)
			{
				float dist;
				dist = (float)uj/(float)numBins*distMax;
				plotData->xyData[uj] = std::make_pair(dist,
						histogram[uj]);
			}

			if(cache)
			{
				plotData->cached=true;
				filterOutputs.push_back(plotData);
				cacheOK=true;
			}	
			
			getOut.push_back(plotData);
		
			for(unsigned int ui=0;ui<dataIn.size() ;ui++)
			{
				switch(dataIn[ui]->getStreamType())
				{
					case STREAM_TYPE_IONS:
					case STREAM_TYPE_RANGE: 
						//Do not propagate ranges, or ions
					break;
					default:
						getOut.push_back(dataIn[ui]);
						break;
				}
				
			}

		}
	}



	return 0;
}

void SpatialAnalysisFilter::getProperties(FilterProperties &propertyList) const
{
	propertyList.data.clear();
	propertyList.keys.clear();
	propertyList.types.clear();

	vector<unsigned int> type,keys;
	vector<pair<string,string> > s;

	string tmpStr;
	vector<pair<unsigned int,string> > choices;
	
	tmpStr="Local Density";
	choices.push_back(make_pair((unsigned int)SPATIAL_ANALYSIS_DENSITY,tmpStr));
	tmpStr="Radial Distribution";
	choices.push_back(make_pair((unsigned int)SPATIAL_ANALYSIS_RDF,tmpStr));
	tmpStr= choiceString(choices,algorithm);
	
	s.push_back(make_pair(string("Algorithm"),tmpStr));
	choices.clear();
	type.push_back(PROPERTY_TYPE_CHOICE);
	keys.push_back(KEY_SPATIALANALYSIS_ALGORITHM);


	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);


	s.clear(); type.clear(); keys.clear(); 

	//Get the options for the current algorithm
	if(algorithm ==  SPATIAL_ANALYSIS_RDF
		||  algorithm == SPATIAL_ANALYSIS_DENSITY)
	{
		tmpStr="Fixed Neighbour Count";

		choices.push_back(make_pair((unsigned int)SPATIAL_DENSITY_NEIGHBOUR,tmpStr));
		tmpStr="Fixed Radius";
		choices.push_back(make_pair((unsigned int)SPATIAL_DENSITY_RADIUS,tmpStr));
		tmpStr= choiceString(choices,stopMode);
		s.push_back(make_pair(string("Stop Mode"),tmpStr));
		type.push_back(PROPERTY_TYPE_CHOICE);
		keys.push_back(KEY_SPATIALANALYSIS_STOPMODE);

		if(stopMode == SPATIAL_DENSITY_NEIGHBOUR)
		{
			stream_cast(tmpStr,nnMax);
			s.push_back(make_pair(string("NN Max"),tmpStr));
			type.push_back(PROPERTY_TYPE_INTEGER);
			keys.push_back(KEY_SPATIALANALYSIS_NNMAX);
		}
		else
		{
			stream_cast(tmpStr,distMax);
			s.push_back(make_pair(string("Dist Max"),tmpStr));
			type.push_back(PROPERTY_TYPE_REAL);
			keys.push_back(KEY_SPATIALANALYSIS_DISTMAX);
		}

		//Extra options for RDF
		if(algorithm == SPATIAL_ANALYSIS_RDF)
		{
			stream_cast(tmpStr,numBins);
			s.push_back(make_pair(string("Num Bins"),tmpStr));
			type.push_back(PROPERTY_TYPE_INTEGER);
			keys.push_back(KEY_SPATIALANALYSIS_NUMBINS);

			if(excludeSurface)
				tmpStr="1";
			else
				tmpStr="0";

			s.push_back(make_pair(string("Surface Remove"),tmpStr));
			type.push_back(PROPERTY_TYPE_BOOL);
			keys.push_back(KEY_SPATIALANALYSIS_REMOVAL);
			
			if(excludeSurface)
			{
				stream_cast(tmpStr,reductionDistance);
				s.push_back(make_pair(string("Remove Dist"),tmpStr));
				type.push_back(PROPERTY_TYPE_REAL);
				keys.push_back(KEY_SPATIALANALYSIS_REDUCTIONDIST);

			}
				
			string thisCol;
			//Convert the ion colour to a hex string	
			genColString((unsigned char)(r*255),(unsigned char)(g*255),
					(unsigned char)(b*255),(unsigned char)(a*255),thisCol);

			s.push_back(make_pair(string("Plot colour "),thisCol)); 
			type.push_back(PROPERTY_TYPE_COLOUR);
			keys.push_back(KEY_SPATIALANALYSIS_COLOUR);

			if(haveRangeParent)
			{
				propertyList.data.push_back(s);
				propertyList.types.push_back(type);
				propertyList.keys.push_back(keys);
				
				s.clear();type.clear();keys.clear();

				ASSERT(ionSourceEnabled.size() == ionNames.size());
				ASSERT(ionNames.size() == ionTargetEnabled.size());

				
				string sTmp;

				if(std::count(ionSourceEnabled.begin(),
					ionSourceEnabled.end(),true) == ionSourceEnabled.size())
					sTmp="1";
				else
					sTmp="0";

				s.push_back(make_pair("Source",sTmp));
				type.push_back(PROPERTY_TYPE_BOOL);
				keys.push_back(KEY_SPATIALANALYSIS_ENABLE_SOURCE);

					
				//Loop over the possible incoming ranges,
				//once to set sources, once to set targets
				for(unsigned int ui=0;ui<ionSourceEnabled.size();ui++)
				{
					if(ionSourceEnabled[ui])
						sTmp="1";
					else
						sTmp="0";
					s.push_back(make_pair(ionNames[ui],sTmp));
					type.push_back(PROPERTY_TYPE_BOOL);
					//FIXME: This is a hack...
					keys.push_back(KEY_SPATIALANALYSIS_ENABLE_SOURCE*1000+ui);
				}

				propertyList.data.push_back(s);
				propertyList.types.push_back(type);
				propertyList.keys.push_back(keys);
				
				s.clear();type.clear();keys.clear();
				if(std::count(ionTargetEnabled.begin(),
					ionTargetEnabled.end(),true) == ionTargetEnabled.size())
					sTmp="1";
				else
					sTmp="0";
				
				s.push_back(make_pair("Target",sTmp));
				type.push_back(PROPERTY_TYPE_BOOL);
				keys.push_back(KEY_SPATIALANALYSIS_ENABLE_TARGET);
				
				//Loop over the possible incoming ranges,
				//once to set sources, once to set targets
				for(unsigned int ui=0;ui<ionTargetEnabled.size();ui++)
				{
					if(ionTargetEnabled[ui])
						sTmp="1";
					else
						sTmp="0";
					s.push_back(make_pair(ionNames[ui],sTmp));
					type.push_back(PROPERTY_TYPE_BOOL);
					//FIXME: This is a hack...
					keys.push_back(KEY_SPATIALANALYSIS_ENABLE_TARGET*1000+ui);
				}

			}
		}	
	}

	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);
}

bool SpatialAnalysisFilter::setProperty( unsigned int set, unsigned int key,
					const std::string &value, bool &needUpdate)
{

	needUpdate=false;
	switch(key)
	{
		case KEY_SPATIALANALYSIS_ALGORITHM:
		{
			size_t ltmp=SPATIAL_ANALYSIS_ENUM_END;

			std::string tmp;
			if(value == "Local Density")
				ltmp=SPATIAL_ANALYSIS_DENSITY;
			else if( value == "Radial Distribution")
				ltmp=SPATIAL_ANALYSIS_RDF;
			
			if(ltmp>=SPATIAL_ANALYSIS_ENUM_END)
				return false;
			
			algorithm=ltmp;
			needUpdate=true;
			clearCache();

			break;
		}	
		case KEY_SPATIALANALYSIS_STOPMODE:
		{
			switch(algorithm)
			{
				case SPATIAL_ANALYSIS_DENSITY:
				case SPATIAL_ANALYSIS_RDF:
				{
					size_t ltmp=SPATIAL_DENSITY_ENUM_END;

					std::string tmp;
					if(value == "Fixed Radius")
						ltmp=SPATIAL_DENSITY_RADIUS;
					else if (value == "Fixed Neighbour Count")
						ltmp=SPATIAL_DENSITY_NEIGHBOUR;
					
					if(ltmp>=SPATIAL_DENSITY_ENUM_END)
						return false;
					
					stopMode=ltmp;
					needUpdate=true;
					clearCache();
					break;
				}

			
				default:
					//Should know what algorithm we use.
					ASSERT(false);
				break;
			}
			break;
		}	
		case KEY_SPATIALANALYSIS_DISTMAX:
		{
			std::string tmp;
			float ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp<= 0.0)
				return false;
			
			distMax=ltmp;
			needUpdate=true;
			clearCache();

			break;
		}	
		case KEY_SPATIALANALYSIS_NNMAX:
		{
			std::string tmp;
			unsigned int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp==0)
				return false;
			
			nnMax=ltmp;
			needUpdate=true;
			clearCache();

			break;
		}	
		case KEY_SPATIALANALYSIS_NUMBINS:
		{
			std::string tmp;
			unsigned int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp==0)
				return false;
			
			numBins=ltmp;
			needUpdate=true;
			clearCache();

			break;
		}
		case KEY_SPATIALANALYSIS_REDUCTIONDIST:
		{
			std::string tmp;
			float ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp<= 0.0)
				return false;
			
			reductionDistance=ltmp;
			needUpdate=true;
			clearCache();

			break;
		}	
		case KEY_SPATIALANALYSIS_REMOVAL:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			bool lastVal=excludeSurface;
			if(stripped=="1")
				excludeSurface=true;
			else
				excludeSurface=false;

			//if the result is different, the
			//cache should be invalidated
			if(lastVal!=excludeSurface)
			{
				needUpdate=true;
				clearCache();
			}
			
			break;
		}
		case KEY_SPATIALANALYSIS_COLOUR:
		{
			unsigned char newR,newG,newB,newA;

			parseColString(value,newR,newG,newB,newA);

			if(newB != b || newR != r ||
				newG !=g || newA != a)
			{
				r=newR/255.0;
				g=newG/255.0;
				b=newB/255.0;
				a=newA/255.0;


				clearCache();
				needUpdate=true;
			}


			break;
		}
		case KEY_SPATIALANALYSIS_ENABLE_SOURCE:
		{
			ASSERT(haveRangeParent);
			bool allEnabled=true;
			for(unsigned int ui=0;ui<ionSourceEnabled.size();ui++)
			{
				if(!ionSourceEnabled[ui])
				{
					allEnabled=false;
					break;
				}
			}

			//Invert the result and assign
			allEnabled=!allEnabled;
			for(unsigned int ui=0;ui<ionSourceEnabled.size();ui++)
				ionSourceEnabled[ui]=allEnabled;

			needUpdate=true;
			clearCache();
			break;
		}
		case KEY_SPATIALANALYSIS_ENABLE_TARGET:
		{
			ASSERT(haveRangeParent);
			bool allEnabled=true;
			for(unsigned int ui=0;ui<ionNames.size();ui++)
			{
				if(!ionTargetEnabled[ui])
				{
					allEnabled=false;
					break;
				}
			}

			//Invert the result and assign
			allEnabled=!allEnabled;
			for(unsigned int ui=0;ui<ionNames.size();ui++)
				ionTargetEnabled[ui]=allEnabled;

			needUpdate=true;
			clearCache();
			break;
		}
		default:
		{
			ASSERT(haveRangeParent);
			//The incoming range keys are dynamically allocated to a 
			//position beyond any reasonable key. Its a hack,
			//but it works, and is entirely contained within the filter code.
			if(key >=KEY_SPATIALANALYSIS_ENABLE_SOURCE*1000 &&
				key < KEY_SPATIALANALYSIS_ENABLE_TARGET*1000)
			{
				size_t offset;
				offset = key-KEY_SPATIALANALYSIS_ENABLE_SOURCE*1000;
				
				string stripped=stripWhite(value);

				if(!(stripped == "1"|| stripped == "0"))
					return false;
				bool lastVal = ionSourceEnabled[offset]; 
				

				if(stripped=="1")
					ionSourceEnabled[offset]=true;
				else
					ionSourceEnabled[offset]=false;

				//if the result is different, the
				//cache should be invalidated
				if(lastVal!=ionSourceEnabled[offset])
				{
					needUpdate=true;
					clearCache();
				}

				

			}	
			else if ( key >=KEY_SPATIALANALYSIS_ENABLE_TARGET*1000)
			{
				size_t offset;
				offset = key-KEY_SPATIALANALYSIS_ENABLE_TARGET*1000;
				
				string stripped=stripWhite(value);

				if(!(stripped == "1"|| stripped == "0"))
					return false;
				bool lastVal = ionTargetEnabled[offset]; 
				

				if(stripped=="1")
					ionTargetEnabled[offset]=true;
				else
					ionTargetEnabled[offset]=false;

				//if the result is different, the
				//cache should be invalidated
				if(lastVal!=ionTargetEnabled[offset])
				{
					needUpdate=true;
					clearCache();
				}
			}	
			else
			{
				ASSERT(false);
			}

		}

	}	
	return true;
}


std::string  SpatialAnalysisFilter::getErrString(unsigned int code) const
{
	//Currently the only error is aborting


	switch(code)
	{
		case SPATIAL_ANALYSIS_ABORT_ERR:
			return std::string("Spatial analysis aborted by user");
		case SPATIAL_ANALYSIS_INSUFFICIENT_SIZE_ERR:
			return std::string("Insufficient data to complete analysis.");
		default:
			ASSERT(false);

	}

	return std::string("Bug! (Spatial analysis filter) Shouldn't see this");
}

bool SpatialAnalysisFilter::writeState(std::ofstream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<spatialanalysis>" << endl;
			f << tabs(depth+1) << "<userstring value=\""<<userString << "\"/>"  << endl;
			f << tabs(depth+1) << "<algorithm value=\""<<algorithm<< "\"/>"  << endl;
			f << tabs(depth+1) << "<stopmode value=\""<<stopMode<< "\"/>"  << endl;
			f << tabs(depth+1) << "<nnmax value=\""<<nnMax<< "\"/>"  << endl;
			f << tabs(depth+1) << "<distmax value=\""<<distMax<< "\"/>"  << endl;
			f << tabs(depth+1) << "<numbins value=\""<<numBins<< "\"/>"  << endl;
			f << tabs(depth+1) << "<excludesurface value=\""<<excludeSurface<< "\"/>"  << endl;
			f << tabs(depth+1) << "<reductiondistance value=\""<<reductionDistance<< "\"/>"  << endl;
			f << tabs(depth+1) << "<colour r=\"" <<  r<< "\" g=\"" << g << "\" b=\"" <<b
				<< "\" a=\"" << a << "\"/>" <<endl;
			f << tabs(depth) << "</spatialanalysis>" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool SpatialAnalysisFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
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
	if(algorithm >=SPATIAL_ANALYSIS_ENUM_END)
		return false;
	//===
	
	//Retrieve stop mode 
	//===
	if(!XMLGetNextElemAttrib(nodePtr,stopMode,"stopmode","value"))
		return false;
	if(stopMode >=SPATIAL_DENSITY_ENUM_END)
		return false;
	//===
	
	//Retrieve nnMax val
	//====== 
	if(!XMLGetNextElemAttrib(nodePtr,nnMax,"nnmax","value"))
		return false;
	if(nnMax <=0)
		return false;
	//===
	
	//Retrieve distMax val
	//====== 
	if(!XMLGetNextElemAttrib(nodePtr,distMax,"distmax","value"))
		return false;
	if(distMax <=0.0)
		return false;
	//===
	
	//Retrieve numBins val
	//====== 
	if(!XMLGetNextElemAttrib(nodePtr,numBins,"numbins","value"))
		return false;
	if(!numBins)
		return false;
	//===
	
	//Retreive exclude surface on/off
	//===
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"excludesurface","value"))
		return false;
	//check that new value makes sense 
	if(tmpStr == "1")
		excludeSurface=true;
	else if( tmpStr == "0")
		excludeSurface=false;
	else
		return false;
	//===
	

	//Get reduction distance
	//===
	if(!XMLGetNextElemAttrib(nodePtr,reductionDistance,"reductiondistance","value"))
		return false;
	if(reductionDistance < 0.0f)
		return false;
	//===

	//Retrieve colour
	//====
	if(XMLHelpFwdToElem(nodePtr,"colour"))
		return false;
	if(!parseXMLColour(nodePtr,r,g,b,a))
		return false;
	//====

	return true;
}
