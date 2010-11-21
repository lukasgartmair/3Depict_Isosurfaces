 /* Copyright (C) 2010  Daniel Haley
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "rdf.h"
#include "voxels.h"

//QHull library
extern "C"
{
	#include <qhull/qhull_a.h>
}

//!Inline func for calculating a(dot)b
inline float dotProduct(float a1, float a2, float a3, 
			float b1, float b2, float b3)
{
	return a1*b1 + a2*b2 + a3* b3;
}

//A bigger MAX_NN_DISTS is better because you will attempt to grab more ram
//however there is a chance that memory allocation can fail, which currently i do not grab safely
const unsigned int MAX_NN_DISTS = 0x8000000; //96 MB samples at a time 

double det3by3(const double *ptArray)
{
	return (ptArray[0]*(ptArray[4]*ptArray[8]
			      	- ptArray[7]*ptArray[5]) 
		- ptArray[1]*(ptArray[3]*ptArray[8]
		       		- ptArray[6]*ptArray[5]) 
		+ ptArray[2]*(ptArray[3]*ptArray[7] 
				- ptArray[4]*ptArray[6]));
}

//Determines the volume of a quadrilateral pyramid
//input points "planarpts" must be adjacent (connected) by 
//0 <-> 1 <-> 2 <-> 0, all points connected to apex
double pyramidVol(const Point3D *planarPts, const Point3D &apex)
{

	//Array for 3D simplex volumed determination
	//		| (a_x - b_x)   (b_x - c_x)   (c_x - d_x) |
	//v_simplex =1/6| (a_y - b_y)   (b_y - c_y)   (c_y - d_y) |
	//		| (a_z - b_z)   (b_z - c_z)   (c_z - d_z) |
	double simplexA[9];

	//simplex A (a,b,c,apex) is as follows
	//a=planarPts[0] b=planarPts[1] c=planarPts[2]
	
	simplexA[0] = (double)( (planarPts[0])[0] - (planarPts[1])[0] );
	simplexA[1] = (double)( (planarPts[1])[0] - (planarPts[2])[0] );
	simplexA[2] = (double)( (planarPts[2])[0] - (apex)[0] );
	
	simplexA[3] = (double)( (planarPts[0])[1] - (planarPts[1])[1] );
	simplexA[4] = (double)( (planarPts[1])[1] - (planarPts[2])[1] );
	simplexA[5] = (double)( (planarPts[2])[1] - (apex)[1] );
	
	simplexA[6] = (double)( (planarPts[0])[2] - (planarPts[1])[2] );
	simplexA[7] = (double)( (planarPts[1])[2] - (planarPts[2])[2] );
	simplexA[8] = (double)( (planarPts[2])[2] - (apex)[2] );
	
	return 1.0/6.0 * (fabs(det3by3(simplexA)));	
}

//!Check which way vectors attached to two 3D points "point", 
/*! Two vectors may point "together", /__\ "apart" \__/  or 
 *  "In common" /__/ or \__\
 */
unsigned int vectorPointDir(const Point3D &pA, const Point3D &pB, 
				const Point3D &vC, const Point3D &vD)
{
	//Check which way vectors attached to two 3D points "point", 
	// - "together", "apart" or "in common"
	
	//calculate AB.CA, BA.DB
	float dot1  = (pB-pA).dotProd(vC - pA);
	float dot2= (pA - pB).dotProd(vD - pB);

	//We shall somehwat arbitrarily call perpendicular cases "together"
	if(dot1 ==0.0f || dot2 == 0.0f)
		return POINTDIR_TOGETHER;

	//If they have opposite signs, then they are "in common"
	if(( dot1  < 0.0f  && dot2 > 0.0f) || (dot1 > 0.0f && dot2 < 0.0f) )
		return POINTDIR_IN_COMMON;

	if( dot1 < 0.0f && dot2 <0.0f )
		return POINTDIR_APART; 

	if(dot1 > 0.0f && dot2 > 0.0f )
		return POINTDIR_TOGETHER;

	ASSERT(false)
	return 0; 
}

//! Returns the shortest distance between a line segment and a given point
/* The inputs are the ends of the line segment and the point. Uses the formula that 
 * \f$ 
 * D = \abs{\vec{PE}}\f$
 * \f[ 
 * \mathrm{~if~} \vec{PA} \cdot \vec{AB} > 0 
 * \rightarrow \vec{PE} = \vec{A} 
 * \f]
 * \f[
 * \mathrm{~if~} \vec{AB} \cdot \vec{PB} > 0 ~\&~ \vec{PA} \cdot \vec{AB} < 0
 * \rightarrow \vec{PB} \cdot \frac{\vec{AB}}{\abs{\vec{AB}}} 
 * \f]
 * \f[
 * \mathrm{~if~} \vec{PB} \cdot \vec{AB} < 0 
 * \rightarrow \vec{B} 
 * \f]
 */
float distanceToSegment(const Point3D &fA, const Point3D &fB, const Point3D &p)
{

	//If the vectors ar pointing "together" then use  point-line formula
	if(vectorPointDir(fA,fB,p,p) == POINTDIR_TOGETHER)
	{
		Point3D closestPt;
		Point3D vAB= fB-fA;

		//Use formula d^2 = |(B-A)(cross)(A-P)|^2/|B-A|^2
		return sqrt( (vAB.crossProd(fA-p)).sqrMag()/(vAB.sqrMag()));
	}

	return sqrt( std::min(fB.sqrDist(p), fA.sqrDist(p)) );
}

//!Find the distance between a point, and a triangular facet -- may be positive or negative
/* The inputs are the facet points (ABC) and the point P.
 * distance is shortest using standard plane version 
 * \f$ D = \vec{AB} \cdot \vec{n} \f$ 
 * iff dot products to each combination of \f$ \left( AP,BP,CP \right) \leq 0 \f$
 * otherwise closest point is on the boundary of the simplex.
 * tested by shortest distance to each line segment (E is shortest pt. AB is line segement)
 * \f$ \vec{E} = \frac{\vec{AB}}{|\vec{AB}|} 
 *  ( \vec{PB} \cdot \vec{AB})\f$
 */
float distanceToFacet(const Point3D &fA, const Point3D &fB, 
			const Point3D &fC, const Point3D &p, const Point3D &normal)
{

	//This will check the magnitude of the incoming normal
	ASSERT( fabs(sqrt(normal.sqrMag()) - 1.0f) < 2.0* std::numeric_limits<float>::epsilon());
	unsigned int pointDir[3];
	pointDir[0] = vectorPointDir(fA,fB,p,p);
	pointDir[1] = vectorPointDir(fA,fC,p,p);
	pointDir[2] = vectorPointDir(fB,fC,p,p);

	//They can never be "APART" if the
	//vectors point to the same pt
	ASSERT(pointDir[0] != POINTDIR_APART);
	ASSERT(pointDir[1] != POINTDIR_APART);
	ASSERT(pointDir[2] != POINTDIR_APART);

	//Check to see if any of them are "in common"
	if(pointDir[0]  > 0 ||  pointDir[1] >0 || pointDir[2] > 0)
	{
		//if so, we have to check each edge for its closest point
		//then pick the best
		float bestDist[3];
		bestDist[0] = distanceToSegment(fA,fB,p);
		bestDist[1] = distanceToSegment(fA,fC,p);
		bestDist[2] = distanceToSegment(fB,fC,p);
	

		return std::min(bestDist[0],std::min(bestDist[1],bestDist[2]));
	}

	float temp;

	temp = fabs((p-fA).dotProd(normal));

	//check that the other points were not better than this!
	ASSERT(sqrt(fA.sqrDist(p)) >= temp - std::numeric_limits<float>::epsilon());
	ASSERT(sqrt(fB.sqrDist(p)) >= temp - std::numeric_limits<float>::epsilon());
	ASSERT(sqrt(fC.sqrDist(p)) >= temp - std::numeric_limits<float>::epsilon());

	//Point lies above/below facet, use plane formula
	return temp; 
}

//obtains all the input points from ions that lie inside the convex hull after
//it has been shrunk such that the closest distance from the hull to the original data
//is reductionDim 
unsigned int GetReducedHullPts(const vector<Point3D> &points, float reductionDim, vector<Point3D> &pointResult)
{
	const int dim=3;

	//TODO: This could be made to use a fixed amount of ram, by
	//partitioning the input points, and then
	//computing multiple hulls.
	//Take the resultant hull points, then hull again. This would be
	//much more space efficient.
	//Alternately, compute a for a randoms K set of points, and reject
	//points that lie in the hull from further computation


	//First we must construct the hull
	double *buffer = new double[points.size()*3];
	Point3D tempPt;
	for(unsigned int ui=points.size(); ui--;)
	{
		tempPt = points[ui];
		buffer[ui*dim] = tempPt[0];
		buffer[ui*dim + 1] = tempPt[1];
		buffer[ui*dim + 2] = tempPt[2];
	}

	//Generate the convex hull
	//(result is stored in qh's globals :(  )
	//note that the input is "joggled" to 
	//ensure simplicial facet generation
	qh_new_qhull(	dim,
			points.size(),
			buffer,
			false,
			(char *)"qhull QJ", //Joggle the output, such that only simplical facets are generated
			NULL,
			NULL);

#ifdef DEBUG
	//The input gets j
	cerr << "Qhull input Joggle max: " << qh JOGGLEmax << std::endl;
#endif

	//OKay, whilst this may look like invalid syntax,
	//qh is actually a macro from qhull
	//that creates qh. or qh-> as needed
	vertexT *vertex = qh vertex_list;

	//obtain convex hull points & mass centroid
	Point3D midPoint(0.0f,0.0f,0.0f);
	unsigned int numPoints=0;
	while(vertex != qh vertex_tail)
	{
		//aggregate points
		midPoint+=Point3D(vertex->point[0],
					vertex->point[1],
					vertex->point[2]);
		vertex = vertex->next;
		numPoints++;
	}

	//do division to obtain average (midpoint)
	midPoint *= 1.0f/(float)numPoints;
#ifdef DEBUG
	cerr << "MidPoint " << midPoint << endl;
#endif	
	//Now we will find the mass/volume centroid of the hull
	//by constructing sets of pyramids
	//where the faces of the hull are the bases of 
	//pyramids, and the midpoint is the apex
	Point3D hullCentroid(0.0f,0.0f,0.0f);
	float massPyramids=0.0f;
	//Run through the faced list
	facetT *curFac = qh facet_list;
	
	while(curFac != qh facet_tail)
	{
		vertexT *vertex;
		Point3D pyramidCentroid;
		unsigned int ui;
		Point3D ptArray[3];
		float vol;

		//find the pyramid volume + centroid
		pyramidCentroid = midPoint;
		ui=0;
		
		//This assertion fails, some more processing is needed to be done to break
		//the facet into something simplical
		ASSERT(curFac->simplicial);
		vertex  = (vertexT *)curFac->vertices->e[ui].p;
		while(vertex)
		{	//copy the vertex info into the pt array
			(ptArray[ui])[0]  = vertex->point[0];
			(ptArray[ui])[1]  = vertex->point[1];
			(ptArray[ui])[2]  = vertex->point[2];
		
			//aggregate pyramidal points	
			pyramidCentroid += ptArray[ui];
		
			//increment before updating vertex
			//to allow checking for NULL termination	
			ui++;
			vertex  = (vertexT *)curFac->vertices->e[ui].p;
		
		}
		
		//note that this counter has been post incremented. 
		ASSERT(ui ==3);
		vol = pyramidVol(ptArray,midPoint);
		
		ASSERT(vol>=0);
		
		//Find the midpoint of the pyramid, this will be the 
		//same as its centre of mass.
		pyramidCentroid*= 0.25f;
		hullCentroid = hullCentroid + (pyramidCentroid*vol);
		massPyramids+=vol;

		curFac=curFac->next;
	}
#ifdef DEBUG
	cerr << "Total \"Mass\" of pyramids:" << massPyramids << endl;
#endif

	hullCentroid *= 1.0f/massPyramids;
#ifdef DEBUG
	cerr << "Mass Centroid :" << hullCentroid[0] << "," << hullCentroid[1] << "," <<
			hullCentroid[2] << endl;
#endif
	float minDist=std::numeric_limits<float>::max();
	//find the smallest distance between the centroid and the
	//convex hull
       	curFac=qh facet_list;
	while(curFac != qh facet_tail)
	{
		float temp;
		vertexT *vertex;
		Point3D vertexPt[3];
		
		//The shortest distance from the plane to the point
		//is the dot product of the UNIT normal with 
		//A-B, where B is on plane, A is point in question
		for(unsigned int ui=0; ui<3; ui++)
		{
			//grab vertex
			vertex  = ((vertexT *)curFac->vertices->e[ui].p);
			vertexPt[ui] = Point3D(vertex->point[0],vertex->point[1],vertex->point[2]);
		}

		//Find the distance between hullcentroid and a given facet
		temp = distanceToFacet(vertexPt[0],vertexPt[1],vertexPt[2],hullCentroid,
					Point3D(curFac->normal[0],curFac->normal[1],curFac->normal[2]));

		if(temp < minDist)
			minDist = temp;
	
		curFac=curFac->next;
	}

	//shrink the convex hull such that it lies at
	//least reductionDim from the original surface of
	//the convex hull
	//The scale factor that we want is 
	//given by s = 1 -(reductionDim/maxDist)
	float scaleFactor;
	scaleFactor = 1  - reductionDim/ minDist;

	if(scaleFactor < 0.0f)
	{
		cerr << "Scale factor negative. Aborting." << endl;
		return RDF_ERR_NEGATIVE_SCALE_FACT;
	}

	//update mindist such that it is still valid after scaling
	minDist=minDist*scaleFactor;
	
#ifdef DEBUG
	cerr << "Scale Factor : " << scaleFactor << endl;
#endif	
	
	//now scan through the input points and see if they
	//lie in the reduced convex hull
	vertex = qh vertex_list;

	//find the bounding cube for the scaled convex hull such that we can quickly
	//discount points without examining the entire hull
	vector<Point3D> vecHullPts;
	vecHullPts.resize(qh num_vertices);
	unsigned int ui=0;
	while(vertex !=qh vertex_tail)
	{
		//Translate around hullCentroid before scaling, 
		//then undo translation after scale
		//Modify the vertex data such that it is scaled around the hullCentroid
		vertex->point[0] = (vertex->point[0] - hullCentroid[0])*scaleFactor + hullCentroid[0];
		vertex->point[1] = (vertex->point[1] - hullCentroid[1])*scaleFactor + hullCentroid[1];
		vertex->point[2] = (vertex->point[2] - hullCentroid[2])*scaleFactor + hullCentroid[2];
	
		vecHullPts[ui]=Point3D(vertex->point[0],
					vertex->point[1],
					vertex->point[2]);

		vertex = vertex->next;
		ui++;
	}

	vecHullPts.clear();
#ifdef DEBUG
	cerr << "Num Facets: " << qh num_facets << endl;
#endif	
	//if the dot product of the normal with the point vector of the
	//considered point P, to any vertex on all of the facets of the 
	//convex hull F1, F2, ... , Fn is negative,
	//then P does NOT lie inside the convex hull.
	pointResult.reserve(points.size()/2);
	curFac = qh facet_list;
	
	//minimum distance from centroid to convex hull
	for(unsigned int ui=points.size(); ui--;)
	{
		float fX,fY,fZ;
		double *ptArr,*normalArr;
		fX =points[ui][0];
		fY = points[ui][1];
		fZ = points[ui][2];
		
		//loop through the facets
		curFac = qh facet_list;
		while(curFac != qh facet_tail)
		{
			//Dont ask. It just grabs the first coords of the vertex
			//associated with this facet
			ptArr = ((vertexT *)curFac->vertices->e[0].p)->point;
			
			normalArr = curFac->normal;
			//if the dotproduct is negative, then the point vector from the 
			//point in queston to the surface is in opposite to the outwards facing
			//normal, which means the point lies outside the hull	
			if (dotProduct( (float)ptArr[0]  - fX,
					(float)ptArr[1] - fY,
					(float)ptArr[2] - fZ,
					normalArr[0], normalArr[1],
					normalArr[2]) >= 0)
			{
				curFac=curFac->next;
				continue;
			}
			goto reduced_loop_next;
		}
		//we passed all tests, point is inside convex hull
		pointResult.push_back(points[ui]);
		
reduced_loop_next:
	;
	}
	
	return 0;
}

//!Generate an NN histogram using NN-max cutoffs 
unsigned int generateNNHist( const vector<Point3D> &pointList, 
			const K3DTree &tree,unsigned int nnMax, unsigned int numBins,
		       	unsigned int *histogram, float *binWidth )
{
	if(pointList.size() <=nnMax)
		return RDF_ERR_INSUFFICIENT_INPUT_POINTS;
	
	//Disallow exact matching for NNs
	float deadDistSqr;
	deadDistSqr= std::numeric_limits<float>::epsilon();
	
	//calclate NNs
	BoundCube cube;
	cube.setBounds(pointList);

	vector<pair<unsigned int,float> > nnDists;
	vector<const Point3D *> nnPoints;	
	
	//Dont allocate too much ram!
	if((float)pointList.size() *(float)nnMax <= (float)MAX_NN_DISTS)
		nnDists.reserve(pointList.size()*nnMax);
	else
		nnDists.reserve(MAX_NN_DISTS);

	//Allocate and assign the initial max distances
	float maxSqrDist[nnMax];
	for(unsigned int ui=0; ui<nnMax; ui++)
		maxSqrDist[ui] = std::numeric_limits<float>::min();
	
	unsigned int maxStoredPt=0;

	//do NN search
	for(unsigned int ui=0; ui<pointList.size(); ui++)
	{
		tree.findKNearest(pointList[ui],cube,
					nnMax,nnPoints,deadDistSqr);


		
		if(nnDists.size() > MAX_NN_DISTS)
		{
			//oh no! ram problems, better stop storing the result
			//just update the max distance as needed
			for(unsigned int uj=0; uj<nnPoints.size(); uj++)
			{
				float temp;
				temp = nnPoints[uj]->sqrDist(pointList[ui]);	
				if(temp > maxSqrDist[uj])
					maxSqrDist[uj] = temp;
			}
			
		}
		else
		{
			//we've got heaps of ram left, just keep storing		
			for(unsigned int uj=0; uj<nnPoints.size(); uj++)
			{
				nnDists.push_back(make_pair(uj,
						nnPoints[uj]->sqrDist(pointList[ui])));	
			}
	
			maxStoredPt=ui;
		}
               

	}
	//Scan for the max sizes, then use number of bins to
	//generate histogram
	float maxDist[nnMax];
	for(unsigned int ui=nnMax; ui--; )
		maxDist[ui]= std::numeric_limits<float>::min();


	//find the maximum distances so we can properly histogram
	for(unsigned int ui=0; ui<nnDists.size(); ui++)
	{
		if(maxDist[nnDists[ui].first] < nnDists[ui].second )
			maxDist[nnDists[ui].first]=nnDists[ui].second;
	}

	float maxOfMaxDists=std::numeric_limits<float>::min();
	for(unsigned int ui=0; ui<nnMax; ui++)
	{
		if(maxOfMaxDists < maxDist[ui])
			maxOfMaxDists = maxDist[ui];
	}	
	
	//check to see if the non-stored points had a larger distance
	for(unsigned int ui=0; ui<nnMax; ui++)
	{
		if(maxSqrDist[ui] > maxDist[ui])
		{
			if(maxOfMaxDists < maxSqrDist[ui])
				maxOfMaxDists=maxSqrDist[ui];
			maxDist[ui] = maxSqrDist[ui];
		}	
		//convert maxima from sqrDistance 
		//to normal =distance	
		maxDist[ui] =sqrt(maxDist[ui]);
	}	

	maxOfMaxDists=sqrt(maxOfMaxDists);
	maxDist[nnMax] = maxOfMaxDists;	

	//Cacluate the bin widths, allowing an extra 5% for the tail bin
	for(unsigned int ui=0; ui<nnMax; ui++)
		binWidth[ui]= 1.05f*maxDist[ui]/(float)numBins;
	
	//convert stored points  from sqr dist to normal distance
	for(unsigned int ui=0; ui<nnDists.size(); ui++)
		nnDists[ui].second = sqrt(nnDists[ui].second);

	
	//Generate histogram for what we have already
	//zero out memory
	for(unsigned int ui=numBins*nnMax; ui--;)
		*(histogram + ui) = 0;

	//Tally the histogram -- so far
	unsigned int row;
	for(unsigned int ui=nnDists.size(); ui--;)
	{	
		//nndists[].first is the NN number. second is the distance
		//so this is the 
		row = (unsigned int)(nnDists[ui].second/binWidth[nnDists[ui].first]);
		//Prevent an exact division from creating an entry on the incorrect row
		if(row == numBins)
			row--;
		
		histogram[row*nnMax + nnDists[ui].first]++;
	}
	
	//we know the bin that things will fall into now, so we can scan 
	//remaining points and place into the histogram on the fly now
	
	//Free up some ram
	nnDists.clear();
	for(unsigned int ui=maxStoredPt+1; ui<pointList.size(); ui++)
	{
		unsigned int offsetTemp;
		float temp;
		tree.findKNearest(pointList[ui],cube,
					nnMax, nnPoints);

		for(unsigned int uj=0; uj<nnPoints.size(); uj++)
		{
			temp=sqrt(nnPoints[uj]->sqrDist(pointList[ui]));
			offsetTemp = (unsigned int)(temp/binWidth[uj]);
			
			//Prevent overflow due to temp/binWidth exceeding array dimension 
			//as (temp is <= binwidth, not < binWidth)
			if(offsetTemp == numBins)
				offsetTemp--;
			
			 histogram [ offsetTemp + uj*numBins]++;
		}
	}


       return 0;
}


//!Generate an NN histogram using distance max cutoffs. Input histogram must be zeroed,
unsigned int generateDistHist(const vector<Point3D> &pointList, const K3DTree &tree,
			unsigned int *histogram, float distMax,
			unsigned int numBins, unsigned int &warnBiasCount,
			std::string voxelsName, unsigned int voxelBins)
{


	if(!pointList.size())
		return 0;

	BoundCube cube;
	cube.setBounds(pointList);
	//We dont know how much ram we will need
	//one could estimate an upper bound by 
	//tree.numverticies*pointlist.size()
	//but i dont have  a tree.numvertcies
	float maxSqrDist = distMax*distMax;

	vector<const Point3D *> pts;

	bool haveVox=false;
	Voxels<unsigned int> voxelRDF; 
	//Initialise the 3D rdf as required
	if(voxelsName.size())
	{
		ASSERT(voxelBins)
		ofstream f(voxelsName.c_str());
		if(!f)
			return RDF_FILE_OPEN_FAIL;
		f.close();	
		voxelRDF.resize(2*voxelBins+1,2*voxelBins+1,2*voxelBins+1,
				Point3D(-distMax,-distMax,-distMax),Point3D(distMax,distMax,distMax));
		haveVox=true;
	}

	warnBiasCount=0;
	
	//Main r-max searching routine
#pragma omp parallel for  
	for(unsigned int ui=0; ui<pointList.size(); ui++)
	{
		pair<unsigned int,unsigned int> thisPair;	
		float sqrDist,deadDistSqr;
		Point3D sourcePoint;
		const Point3D *nearPt=0;
		//Go through each point and grab up to the maximum distance
		//that we need
		
		thisPair = std::make_pair(0,0);
		
		//disable exact matching, by requiring d^2 > epsilon
		deadDistSqr=std::numeric_limits<float>::epsilon();
		sqrDist=0;
		sourcePoint=pointList[ui];
		while(deadDistSqr < maxSqrDist)
		{

			//Grab the nearest point
			nearPt = tree.findNearest(sourcePoint, cube,
							 deadDistSqr);

			if(nearPt)
			{
				//Cacluate the sq of the distance to the poitn
				sqrDist = nearPt->sqrDist(sourcePoint);
				
				//if sqrDist is = maxSqrdist then this will cause
				//the histogram indexing to trash alternate memory
				//- this is bad - prevent this please.	
				if(sqrDist < maxSqrDist)
				{
					if(haveVox)
					{
						unsigned long long xV,yV,zV;
						//Create the lower-left bounded point
						//for the voxel data, then increment that position
						Point3D p;
						p=*nearPt-sourcePoint;
						voxelRDF.getIndex(xV,yV,zV,p);

#pragma omp critical
						voxelRDF.increment(xV,yV,zV);
					}

					//Add the point to the histogram
					unsigned int binNum;
					binNum = ((unsigned int) ((sqrt(sqrDist/maxSqrDist)*(float)numBins)));

#pragma omp critical	
					histogram[binNum]++;
				}

				//increase the dead distance to the last distance
				deadDistSqr = sqrDist+std::numeric_limits<float>::epsilon();
			}
			else
			{		
				//Oh no, we had a problem, somehow we couldn't find enough
				warnBiasCount++;
				break;
			}
		}

	}


	if(haveVox)
		voxelRDF.writeFile(voxelsName.c_str());
	
	//Calculations complete!
	return 0;
}
