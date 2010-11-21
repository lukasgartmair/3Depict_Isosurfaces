/* Copyright (C) 2008  D. Haley
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

#ifndef RDF_H
#define RDF_H

#include "basics.h"
#include "K3DTree.h"

#include <vector>
#include <string>

//RDF error codes
enum
{
	RDF_ERR_NEGATIVE_SCALE_FACT,
	RDF_ERR_INSUFFICIENT_INPUT_POINTS,
	RDF_FILE_OPEN_FAIL,
};

//!Generate the NN histogram specified up to a given NN
unsigned int generateNNHist( const vector<Point3D> &pointList, 
			const K3DTree &tree,unsigned int nnMax, unsigned int numBins,
		       	unsigned int *histogram, float *binWidth );

//!Generate an NN histogram using distance max cutoffs. Input histogram must be zeroed,
//if a voxelsname is given, a 3D RDF will be recorded. in this case voxelBins must be nonzero
unsigned int generateDistHist(const vector<Point3D> &pointList, const K3DTree &tree,
			unsigned int *histogram, float distMax,
			unsigned int numBins, unsigned int &warnBiasCount,
			std::string voxelsName=std::string(""),unsigned int voxelBins=0);

//!Returns a subset of points guaranteed to lie at least reductionDim inside hull of input points
/*! Calculates the hull of the input ions and then scales the hull such that the 
 * smallest distance between the scaled hull and the original hull is  exactly
 * reductionDim
 */
unsigned int GetReducedHullPts(const std::vector<Point3D> &pts, float reductionDim,std::vector<Point3D> &returnIons );

#endif
