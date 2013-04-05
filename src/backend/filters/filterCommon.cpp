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




//TODO: Work out where the payoff for this is
//grab size when doing convex hull calculations
const unsigned int HULL_GRAB_SIZE=4096;

using std::ostream;
using std::vector;
using std::endl;
using std::string;

//Wrapper for qhull single-pass run
unsigned int doHull(unsigned int bufferSize, double *buffer, 
			vector<Point3D> &resHull, Point3D &midPoint);

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
	
	while(!XMLHelpFwdToElem(nodePtr,"point3d"))
	{
		std::string tmpStr;
		xmlChar* xmlString;
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


unsigned int computeConvexHull(const vector<const FilterStreamData*> &data, unsigned int *progress,
					bool (*callback)(bool),std::vector<Point3D> &curHull, bool freeHull)
{

	size_t numPts;
	numPts=numElements(data,STREAM_TYPE_IONS);
	//Easy case of no data
	if(numPts < 4)
		return 0;

	double *buffer;
	double *tmp;
	//Use malloc so we can re-alloc
	buffer =(double*) malloc(HULL_GRAB_SIZE*3*sizeof(double));

	if(!buffer)
		return HULL_ERR_NO_MEM;

	size_t bufferOffset=0;

	//Do the convex hull in steps for two reasons
	// 1) qhull chokes on large data
	// 2) we need to run the callback every now and again, so we have to
	//   work in batches.
	Point3D midPoint;
	float maxSqrDist=-1;
	bool doneHull=false;
	size_t progressReduce=PROGRESS_REDUCE;
	size_t n=0;
	for(size_t ui=0; ui<data.size(); ui++)
	{
		if(data[ui]->getStreamType() != STREAM_TYPE_IONS)
			continue;

		const IonStreamData* ions=(const IonStreamData*)data[ui];

		for(size_t uj=0; uj<ions->data.size(); uj++)
		{		//Do contained-in-sphere check
			if(!curHull.size() || midPoint.sqrDist(ions->data[uj].getPos())>= maxSqrDist)
			{
				//Copy point data into hull buffer
				buffer[3*bufferOffset]=ions->data[uj].getPos()[0];
				buffer[3*bufferOffset+1]=ions->data[uj].getPos()[1];
				buffer[3*bufferOffset+2]=ions->data[uj].getPos()[2];
				bufferOffset++;

				//If we have hit the hull grab size, perform a hull

				if(bufferOffset == HULL_GRAB_SIZE)
				{
					bufferOffset+=curHull.size();
					tmp=(double*)realloc(buffer,
							     3*bufferOffset*sizeof(double));
					if(!tmp)
					{
						free(buffer);
						if(doneHull)
							FREE_QHULL();

						return HULL_ERR_NO_MEM;
					}

					buffer=tmp;
					//Copy in the old hull
					for(size_t uk=0; uk<curHull.size(); uk++)
					{
						buffer[3*(HULL_GRAB_SIZE+uk)]=curHull[uk][0];
						buffer[3*(HULL_GRAB_SIZE+uk)+1]=curHull[uk][1];
						buffer[3*(HULL_GRAB_SIZE+uk)+2]=curHull[uk][2];
					}

					unsigned int errCode=0;
					if(doneHull)
						FREE_QHULL();
					
					errCode=doHull(bufferOffset,buffer,curHull,midPoint);
					if(errCode)
						return errCode;

					doneHull=true;

					//Now compute the min sqr distance
					//to the vertex, so we can fast-reject
					maxSqrDist=std::numeric_limits<float>::max();
					for(size_t ui=0; ui<curHull.size(); ui++)
						maxSqrDist=std::min(maxSqrDist,curHull[ui].sqrDist(midPoint));
					//reset buffer size
					bufferOffset=0;
				}

			}
			n++;

			//Update the progress information, and run callback periodically
			if(!progressReduce--)
			{
				if(!(*callback)(false))
				{
					free(buffer);
					if(doneHull)
						FREE_QHULL();
					return HULL_ERR_USER_ABORT;
				}
	
				*progress= (unsigned int)((float)(n)/((float)numPts)*100.0f);

				progressReduce=PROGRESS_REDUCE;
			}
		}
	}

	//If we have actually done a convex hull in our loop,
	// we may still have to clear it
	if(doneHull && freeHull)
		FREE_QHULL();

	//Need at least 4 objects to construct a sufficiently large buffer
	if(bufferOffset + curHull.size() > 4)
	{
		//Re-allocate the buffer to determine the last hull size
		tmp=(double*)realloc(buffer,
		                     3*(bufferOffset+curHull.size())*sizeof(double));
		if(!tmp)
		{
			free(buffer);
			return HULL_ERR_NO_MEM;
		}
		buffer=tmp;

		#pragma omp parallel for
		for(unsigned int ui=0; ui<curHull.size(); ui++)
		{
			buffer[3*(bufferOffset+ui)]=curHull[ui][0];
			buffer[3*(bufferOffset+ui)+1]=curHull[ui][1];
			buffer[3*(bufferOffset+ui)+2]=curHull[ui][2];
		}

		unsigned int errCode=doHull(bufferOffset+curHull.size(),buffer,curHull,midPoint);
		if(freeHull)
			FREE_QHULL();

		if(errCode)
		{
			free(buffer);
			//Free the last convex hull mem
			return errCode;
		}
	}


	free(buffer);
	return 0;
}

unsigned int computeConvexHull(const vector<Point3D> &data, unsigned int *progress,
					bool (*callback)(bool),std::vector<Point3D> &curHull, bool freeHull)
{

	//Easy case of no data
	if(data.size()< 4)
		return 0;

	double *buffer;
	double *tmp;
	//Use malloc so we can re-alloc
	buffer =(double*) malloc(HULL_GRAB_SIZE*3*sizeof(double));

	if(!buffer)
		return HULL_ERR_NO_MEM;

	size_t bufferOffset=0;

	//Do the convex hull in steps for two reasons
	// 1) qhull chokes on large data
	// 2) we need to run the callback every now and again, so we have to
	//   work in batches.
	Point3D midPoint;
	float maxSqrDist=-1;
	bool doneHull=false;


	size_t progressReduce=PROGRESS_REDUCE;

	for(size_t uj=0; uj<data.size(); uj++)
	{
		//Do contained-in-sphere check
		if(!curHull.size() || midPoint.sqrDist(data[uj])>= maxSqrDist)
		{
		
			//Copy point data into hull buffer
			buffer[3*bufferOffset]=data[uj][0];
			buffer[3*bufferOffset+1]=data[uj][1];
			buffer[3*bufferOffset+2]=data[uj][2];
			bufferOffset++;

			//If we have hit the hull grab size, perform a hull

			if(bufferOffset == HULL_GRAB_SIZE)
			{
				bufferOffset+=curHull.size();
				tmp=(double*)realloc(buffer,
						     3*bufferOffset*sizeof(double));
				if(!tmp)
				{
					free(buffer);
					if(doneHull)
						FREE_QHULL();

					return HULL_ERR_NO_MEM;
				}

				buffer=tmp;
				//Copy in the old hull
				for(size_t uk=0; uk<curHull.size(); uk++)
				{
					buffer[3*(HULL_GRAB_SIZE+uk)]=curHull[uk][0];
					buffer[3*(HULL_GRAB_SIZE+uk)+1]=curHull[uk][1];
					buffer[3*(HULL_GRAB_SIZE+uk)+2]=curHull[uk][2];
				}

				unsigned int errCode=0;
				if(doneHull)
					FREE_QHULL();
				
				errCode=doHull(bufferOffset,buffer,curHull,midPoint);
				if(errCode)
					return errCode;

				doneHull=true;

				//Now compute the min sqr distance
				//to the vertex, so we can fast-reject
				maxSqrDist=std::numeric_limits<float>::max();
				for(size_t ui=0; ui<curHull.size(); ui++)
					maxSqrDist=std::min(maxSqrDist,curHull[ui].sqrDist(midPoint));
				//reset buffer size
				bufferOffset=0;
			}
		}

		if(!progressReduce--)
		{
			if(!(*callback)(false))
			{
				free(buffer);
				if(doneHull)
					FREE_QHULL();
				return HULL_ERR_USER_ABORT;
			}
	
			*progress= (unsigned int)((float)(uj)/((float)data.size())*100.0f);

			progressReduce=PROGRESS_REDUCE;
		}
	}

	//If we have actually done a convex hull in our loop,
	// we may still have to clear it
	if(doneHull && freeHull)
		FREE_QHULL();


	//Build the final hull, using the remaining points, and the
	// filtered hull points
	//Need at least 4 objects to construct a sufficiently large buffer
	if(bufferOffset + curHull.size() > 4)
	{
		//Re-allocate the buffer to determine the last hull size
		tmp=(double*)realloc(buffer,
		                     3*(bufferOffset+curHull.size())*sizeof(double));
		if(!tmp)
		{
			free(buffer);
			return HULL_ERR_NO_MEM;
		}
		buffer=tmp;

		#pragma omp parallel for
		for(unsigned int ui=0; ui<curHull.size(); ui++)
		{
			buffer[3*(bufferOffset+ui)]=curHull[ui][0];
			buffer[3*(bufferOffset+ui)+1]=curHull[ui][1];
			buffer[3*(bufferOffset+ui)+2]=curHull[ui][2];
		}

		unsigned int errCode=doHull(bufferOffset+curHull.size(),buffer,curHull,midPoint);
		if(freeHull)
			FREE_QHULL();

		if(errCode)
		{
			free(buffer);
			//Free the last convex hull mem
			return errCode;
		}
	}


	free(buffer);
	return 0;
}

unsigned int doHull(unsigned int bufferSize, double *buffer, 
			vector<Point3D> &resHull, Point3D &midPoint)
{
	const int dim=3;
	//Now compute the new hull
	//Generate the convex hull
	//(result is stored in qh's globals :(  )
	//note that the input is "joggled" to 
	//ensure simplicial facet generation

	qh_new_qhull(	dim,
			bufferSize,
			buffer,
			false,
			(char *)"qhull QJ", //Joggle the output, such that only simplical facets are generated
			NULL,
			NULL);

	unsigned int numPoints=0;
	//count points
	//--	
	//OKay, whilst this may look like invalid syntax,
	//qh is actually a macro from qhull
	//that creates qh. or qh-> as needed
	vertexT *vertex = qh vertex_list;
	while(vertex != qh vertex_tail)
	{
		vertex = vertex->next;
		numPoints++;
	}
	//--	

	//store points in vector
	//--
	vertex= qh vertex_list;
	try
	{
		resHull.resize(numPoints);	
	}
	catch(std::bad_alloc)
	{
		free(buffer);
		return HULL_ERR_NO_MEM;
	}
	//--

	//Compute mean point
	//--
	int curPt=0;
	midPoint=Point3D(0,0,0);	
	while(vertex != qh vertex_tail)
	{
		resHull[curPt]=Point3D(vertex->point[0],
				vertex->point[1],
				vertex->point[2]);
		midPoint+=resHull[curPt];
		curPt++;
		vertex = vertex->next;
	}
	midPoint*=1.0f/(float)numPoints;
	//--

	return 0;
}
