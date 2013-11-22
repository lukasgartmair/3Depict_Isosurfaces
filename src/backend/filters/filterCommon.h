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

#include "backend/APT/APTRanges.h"

//QHull library
#ifdef __POWERPC__
	#pragma push_macro("__POWERPC__")
	#define __POWERPC__ 1
#endif
extern "C"
{
	#include <qhull/qhull_a.h>
}
#ifdef __POWERPC__
	#pragma pop_macro("__POWERPC__")
#endif
#define FREE_QHULL() { qh_freeqhull(!qh_ALL); int curlong, totlong; \
			qh_memfreeshort(&curlong, &totlong);}


enum
{
	HULL_ERR_NO_MEM=1,
	HULL_ERR_USER_ABORT,
	HULL_ERR_ENUM_END
};

const size_t PROGRESS_REDUCE=5000;

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

//Compute the convex hull of a set of input points from fiilterstream data
unsigned int computeConvexHull(const std::vector<const FilterStreamData*> &data, 
			unsigned int *progress, bool (*callback)(bool), 
			std::vector<Point3D> &hullPts, bool freeHull=true);
//Compute the convex hull of a set of input points
unsigned int computeConvexHull(const std::vector<Point3D> &data, 
			unsigned int *progress, bool (*callback)(bool), 
			std::vector<Point3D> &hullPts, bool freeHull=true);

//Draw a colour bar
DrawColourBarOverlay *makeColourBar(float minV, float maxV,size_t nColours,size_t colourMap, bool reverseMap=false) ;

#endif
