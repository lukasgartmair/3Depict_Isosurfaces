#include "allFilter.h"


//Filter "factory" type function. If this gets too big, 
//we could use a pre-populated hashtable in 
//a static class to speed things up.
//returns null pointer if string is invalid
Filter *makeFilter(const std::string &s)
{
	Filter *f;
	unsigned int type=(unsigned int)-1;
	for(unsigned int ui=0;ui<FILTER_TYPE_ENUM_END; ui++)
	{
		if( s == FILTER_NAMES[ui])
			type=ui;
	}

	switch(type)
	{
		
		case FILTER_TYPE_POSLOAD:
			f=new PosLoadFilter;
			break;
		case FILTER_TYPE_IONDOWNSAMPLE:
			f=new IonDownsampleFilter;
			break;
		case FILTER_TYPE_RANGEFILE:
			f=new RangeFileFilter;
			break;
		case FILTER_TYPE_SPECTRUMPLOT:
			f=new SpectrumPlotFilter;
			break;
		case FILTER_TYPE_IONCLIP:
			f=new IonClipFilter;
			break;
		case FILTER_TYPE_IONCOLOURFILTER:
			f=new IonColourFilter;
			break;
		case FILTER_TYPE_COMPOSITION:
			f=new CompositionProfileFilter;
			break;
		case FILTER_TYPE_BOUNDBOX:
			f = new BoundingBoxFilter;
			break;
		case FILTER_TYPE_TRANSFORM:
			f= new TransformFilter;
			break;
		case FILTER_TYPE_EXTERNALPROC:
			f= new ExternalProgramFilter;
			break;
		case FILTER_TYPE_SPATIAL_ANALYSIS:
			f = new SpatialAnalysisFilter;
			break;
		case FILTER_TYPE_CLUSTER_ANALYSIS:
			f = new ClusterAnalysisFilter;
			break;
		case FILTER_TYPE_VOXELS:
			f = new VoxeliseFilter;
			break;
		
		default:
			f=0;

	}

	WARN(f,"Should have only got here with invalid input. Might be worth double checking"); 
#ifdef DEBUG
	//Should have set filter
	//type string should match
	if(f)
		ASSERT(f->trueName() ==  s);
#endif
	return  f;
}

Filter *makeFilter(unsigned int ui)
{
	Filter *f;

	switch(ui)
	{
		case FILTER_TYPE_POSLOAD:
			f=new PosLoadFilter;
			break;
		case FILTER_TYPE_IONDOWNSAMPLE:
			f=new IonDownsampleFilter;
			break;
		case FILTER_TYPE_RANGEFILE:
			f=new RangeFileFilter;
			break;
		case FILTER_TYPE_SPECTRUMPLOT:
			f=new SpectrumPlotFilter;
			break;
		case FILTER_TYPE_IONCLIP:
			f=new IonClipFilter;
			break;
		case FILTER_TYPE_IONCOLOURFILTER:
			f=new IonColourFilter;
			break;
		case FILTER_TYPE_COMPOSITION:
			f=new CompositionProfileFilter;
			break;
		case FILTER_TYPE_BOUNDBOX:
			f = new BoundingBoxFilter;
			break;
		case FILTER_TYPE_TRANSFORM:
			f= new TransformFilter;
			break;
		case FILTER_TYPE_EXTERNALPROC:
			f= new ExternalProgramFilter;
			break;
		case FILTER_TYPE_SPATIAL_ANALYSIS:
			f = new SpatialAnalysisFilter;
			break;
		case FILTER_TYPE_CLUSTER_ANALYSIS:
			f = new ClusterAnalysisFilter;
			break;
		case FILTER_TYPE_VOXELS:
			f = new VoxeliseFilter;
			break;
			
		default:
			ASSERT(false);
	}

	return  f;
}

Filter *makeFilterFromDefUserString(const std::string &s)
{
	//This is a bit of a hack. Build each object, then retrieve its string.
	//Could probably use static functions and type casts to improve this
	for(unsigned int ui=0;ui<FILTER_TYPE_ENUM_END; ui++)
	{
		Filter *t;
		t = makeFilter(ui);
		if( s == t->typeString())
			return t;

		delete t;
	}

	ASSERT(false);
}
