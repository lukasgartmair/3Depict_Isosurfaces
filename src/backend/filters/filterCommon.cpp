/*
 *	filterCommon.cpp - Helper routines for filter classes
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

#include "filterCommon.h"


using std::ostream;
using std::vector;
using std::endl;
using std::string;

void writeVectorsXML(ostream &f,const char *containerName,
		const vector<Point3D> &vectorParams, unsigned int depth)
{
	f << tabs(depth+1) << "<" << containerName << ">" << endl;
	for(unsigned int ui=0; ui<vectorParams.size(); ui++)
	{
		f << tabs(depth+2) << "<point3d x=\"" << vectorParams[ui][0] << 
			"\" y=\"" << vectorParams[ui][1] << "\" z=\"" << vectorParams[ui][2] << "\"/>" << endl;
	}
	f << tabs(depth+1) << "</" << containerName << ">" << endl;
}

void writeIonsEnabledXML(ostream &f, const char *containerName, 
		const vector<bool> &enabledState, const vector<string> &names, 
			unsigned int depth)
{
	f << tabs(depth) << "<" << containerName << ">"  << endl;
	for(size_t ui=0;ui<enabledState.size();ui++)
	{
		f<< tabs(depth+1) << "<ion enabled=\"" << (int)enabledState[ui] 
			<< "\" name=\"" << names[ui] << "\"/>" <<  std::endl; 
	}
	f << tabs(depth) << "</" << containerName << ">"  << endl;
}

bool readVectorsXML(xmlNodePtr nodePtr,	std::vector<Point3D> &vectorParams) 
{
	nodePtr=nodePtr->xmlChildrenNode;
	vectorParams.clear();
	
	
	xmlChar* xmlString;
	std::string tmpStr;
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

	return true;
}

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
	xmlFree(xmlString);


	//--green--
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"g");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(g,tmpStr))
	{
		xmlFree(xmlString);
		return false;
	}
	
	xmlFree(xmlString);

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
	{
		xmlFree(xmlString);
		return false;
	}
	xmlFree(xmlString);
	
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
	{
		xmlFree(xmlString);
		return false;
	}
	xmlFree(xmlString);

	//disallow negative or values gt 1.
	if(a < 0.0f || a > 1.0f)
		return false;

	return true;
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
	if(d->data.empty())
		return (unsigned int)-1;

	unsigned int tentativeRange;

	tentativeRange=r->getIonID(d->data[0].getMassToCharge());


	//TODO: Currently, we have no choice but to brute force it.
	//In the future, it might be worth storing some data inside the IonStreamData itself
	//and to use that first, rather than try to brute force the result
#ifdef _OPENMP
	bool spin=false;
	#pragma omp parallel for shared(spin)
	for(size_t ui=1;ui<d->data.size();ui++)
	{
		if(spin)
			continue;
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
				bool (*callback)(bool),unsigned int &progress, size_t offset)
{
	unsigned int curProg=NUM_CALLBACK;
	unsigned int n =offset;
#ifdef _OPENMP
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
				if(!(*callback)(false))
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
			if(!(*callback)(false))
				return 1;
		}

	}
#endif


	return 0;
}

