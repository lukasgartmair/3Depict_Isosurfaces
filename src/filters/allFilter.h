#ifndef ALLFILTER_H
#define ALLFILTER_H
#include "boundingBox.h"
#include "compositionProfile.h"
#include "externalProgram.h"
#include "ionClip.h"
#include "ionColour.h"
#include "ionDownsample.h"
#include "posLoad.h"
#include "rangeFile.h"
#include "clusterAnalysis.h"
#include "spatialAnalysis.h"
#include "spectrumPlot.h"
#include "transform.h"
#include "voxelise.h"
#include "ionInfo.h"
#include "annotation.h"

//!Create a "true default" filter from its true name string
Filter *makeFilter(const string  &s) ;
//!Create a true default filter from its enum value FILTER_TYPE_*
Filter *makeFilter(unsigned int ui) ;

//!Create a true default filter from its type string
Filter *makeFilterFromDefUserString(const string &s) ;

#endif
