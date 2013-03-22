/*
 *	filterCommon.h - Helper routines for filter classes
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

#ifndef FILTERCOMMON_H
#define FILTERCOMMON_H

#include "../filter.h"

#include "common/stringFuncs.h"
#include "common/xmlHelper.h"

#include "backend/APT/APTClasses.h"
#include "backend/APT/APTRanges.h"

//serialise 3D std::vectors to specified output stream in XML format
void writeVectorsXML(std::ostream &f, const char *containerName,
		const std::vector<Point3D> &vectorParams, unsigned int depth);

//Serialise out "enabled" ions as XML
void writeIonsEnabledXML(std::ostream &f, const char *containerName, 
		const std::vector<bool> &enabledState, const std::vector<std::string> &names, 
			unsigned int depth);

//serialise 3D scalars to specified output stream in XML format
// - depth is tab indentation depth
// - container name for : <container> (newline) <scalar .../><scalar ... /> </container>
template<class T>
void writeScalarsXML(std::ostream &f, const char *containerName,
		const std::vector<T> &scalarParams, unsigned int depth)
{
	f << tabs(depth) << "<"  << containerName << ">" << std::endl;
	for(unsigned int ui=0; ui<scalarParams.size(); ui++)
		f << tabs(depth+1) << "<scalar value=\"" << scalarParams[0] << "\"/>" << std::endl; 
	
	f << tabs(depth) << "</" << containerName << ">" << std::endl;
}

//Nodeptr must be pointing at container node
template<class T>
bool readScalarsXML(xmlNodePtr nodePtr,std::vector<T> &scalarParams)
{
	std::string tmpStr;
	nodePtr=nodePtr->xmlChildrenNode;

	scalarParams.clear();
	while(!XMLHelpFwdToElem(nodePtr,"scalar"))
	{
		xmlChar *xmlString;
		T v;
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
	return true;
}

//serialise 3D std::vectors to specified output stream in XML format
bool readVectorsXML(xmlNodePtr nodePtr,
	std::vector<Point3D> &vectorParams);


//Parse a "colour" node, extracting rgba data
bool parseXMLColour(xmlNodePtr &nodePtr, 
		float &r, float&g, float&b, float&a);

//Returns the ion stream's range ID from the rangefile, if and only if it every ion in input
// is ranged tht way. Otherwise returns -1.
unsigned int getIonstreamIonID(const IonStreamData *d, const RangeFile *r);

//!Extend a point data vector using some ion data
unsigned int extendPointVector(std::vector<Point3D> &dest, const std::vector<IonHit> &vIonData,
				bool (*callback)(bool),unsigned int &progress, size_t offset);

const RangeFile *getRangeFile(const std::vector<const FilterStreamData*> &dataIn);

#endif
