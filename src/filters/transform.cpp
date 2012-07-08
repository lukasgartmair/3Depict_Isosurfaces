#include "transform.h"

#include "../xmlHelper.h"

#include "../translation.h"

#include <algorithm>

enum
{
	KEY_MODE,
	KEY_SCALEFACTOR,
	KEY_ORIGIN,
	KEY_TRANSFORM_SHOWORIGIN,
	KEY_ORIGINMODE,
	KEY_NOISELEVEL,
	KEY_NOISETYPE,
	KEY_ROTATE_ANGLE,
	KEY_ROTATE_AXIS,
	KEY_ORIGIN_VALUE
};

//Possible transform modes (scaling, rotation etc)
enum
{
	MODE_TRANSLATE,
	MODE_SCALE,
	MODE_ROTATE,
	MODE_VALUE_SHUFFLE,
	MODE_SPATIAL_NOISE,
	MODE_TRANSLATE_VALUE,
	MODE_ENUM_END
};

//!Possible mode for selection of origin in transform filter
enum
{
	ORIGINMODE_SELECT,
	ORIGINMODE_CENTREBOUND,
	ORIGINMODE_MASSCENTRE,
	ORIGINMODE_END, // Not actually origin mode, just end of enum
};

//!Possible noise modes
enum
{
	NOISETYPE_GAUSSIAN,
	NOISETYPE_WHITE,
	NOISETYPE_END
};

//!Error codes
enum
{
	ERR_CALLBACK_FAIL=1,
	ERR_NOMEM
};

const char *TRANSFORM_MODE_STRING[] = { NTRANS("Translate"),
					NTRANS("Scale"),
					NTRANS("Rotate"),
					NTRANS("Value Shuffle"),
					NTRANS("Spatial Noise"),
					NTRANS("Translate Value")
					};

const char *TRANSFORM_ORIGIN_STRING[]={ 
					NTRANS("Specify"),
					NTRANS("Boundbox Centre"),
					NTRANS("Mass Centre")
					};
					
	
	
//=== Transform filter === 
TransformFilter::TransformFilter()
{
	COMPILE_ASSERT(ARRAYSIZE(TRANSFORM_MODE_STRING) == MODE_ENUM_END);
	COMPILE_ASSERT(ARRAYSIZE(TRANSFORM_ORIGIN_STRING) == ORIGINMODE_END);

	randGen.initTimer();
	transformMode=MODE_TRANSLATE;
	originMode=ORIGINMODE_SELECT;
	noiseType=NOISETYPE_WHITE;
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
	p->noiseType=noiseType;
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

DrawStreamData* TransformFilter::makeMarkerSphere(SelectionDevice<Filter>* &s) const
{
	//construct a new primitive, do not cache
	DrawStreamData *drawData=new DrawStreamData;
	drawData->parent=this;
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
	if (originMode == ORIGINMODE_SELECT )
	{
		dS->canSelect=true;

		s=new SelectionDevice<Filter>(this);
		SelectionBinding b;

		b.setBinding(SELECT_BUTTON_LEFT,FLAG_CMD,DRAW_SPHERE_BIND_ORIGIN,
		             BINDING_SPHERE_ORIGIN,dS->getOrigin(),dS);
		b.setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
		s->addBinding(b);

	}
	drawData->cached=0;	

	return drawData;
}

unsigned int TransformFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(bool))
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
			if(showOrigin && (transformMode == MODE_ROTATE ||
					transformMode == MODE_SCALE) )
			{
				SelectionDevice<Filter> *s;
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
		case ORIGINMODE_CENTREBOUND:
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
		case ORIGINMODE_MASSCENTRE:
		{
			Point3D massCentre(0,0,0);
			size_t numCentres=0;
			#pragma omp parallel for
			for(unsigned int ui=0;ui<dataIn.size() ;ui++)
			{
				Point3D massContrib;
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
						massContrib=thisCentre*1.0/(float)ions->data.size();
						#pragma omp critical
						massCentre+=massContrib;
						numCentres++;
					}
				}
			}
			vectorParams[0]=massCentre*1.0/(float)numCentres;
			break;

		}
		case ORIGINMODE_SELECT:
			break;
		default:
			ASSERT(false);
	}

	//If the user is using a transform mode that requires origin selection 
	if(showOrigin && (transformMode == MODE_ROTATE ||
			transformMode == MODE_SCALE) )
	{
		SelectionDevice<Filter> *s;
		getOut.push_back(makeMarkerSphere(s));
		if(s)
			devices.push_back(s);
	}
			
	//Apply the transformations to the incoming 
	//ion streams, generating new outgoing ion streams with
	//the modified positions
	size_t totalSize=numElements(dataIn);
	if( transformMode != MODE_VALUE_SHUFFLE)
	{
		//Dont cross the streams. Why? It would be bad.
		//  - Im fuzzy on the whole good-bad thing, what do you mean bad?"
		//  - Every ion in the data body can be operated on independantly.
		//
		//  OK, important safety tip.
		for(unsigned int ui=0;ui<dataIn.size() ;ui++)
		{
			switch(transformMode)
			{
				case MODE_SCALE:
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
							d->parent=this;
							const IonStreamData *src = (const IonStreamData *)dataIn[ui];

							try
							{
								d->data.resize(src->data.size());
							}
							catch(std::bad_alloc)
							{
								delete d;
								return ERR_NOMEM;
							}
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
										if(!(*callback)(false))
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
								return ERR_CALLBACK_FAIL;
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
									if(!(*callback)(false))
									{
										delete d;
										return ERR_CALLBACK_FAIL;
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
				case MODE_TRANSLATE:
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
							d->parent=this;
							
							const IonStreamData *src = (const IonStreamData *)dataIn[ui];
							try
							{
								d->data.resize(src->data.size());
							}
							catch(std::bad_alloc)
							{
								delete d;
								return ERR_NOMEM;
							}
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
										if(!(*callback)(false))
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
								return ERR_CALLBACK_FAIL;
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
									if(!(*callback)(false))
									{
										delete d;
										return ERR_CALLBACK_FAIL;
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
				case MODE_TRANSLATE_VALUE:
				{
					//We are going to scale the incoming point data
					//around the specified origin.
					ASSERT(vectorParams.size() == 0);
					ASSERT(scalarParams.size() == 1);
					float origin =scalarParams[0];
					size_t n=0;
					switch(dataIn[ui]->getStreamType())
					{
						case STREAM_TYPE_IONS:
						{
							//Set up scaling output ion stream 
							IonStreamData *d=new IonStreamData;
							d->parent=this;
							
							const IonStreamData *src = (const IonStreamData *)dataIn[ui];
							try
							{
								d->data.resize(src->data.size());
							}
							catch(std::bad_alloc)
							{
								delete d;
								return ERR_NOMEM;
							}
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
										if(!(*callback)(false))
											spin=true;
									}
								}
								
								
								//set the position for the given ion
								d->data[ui].setPos((src->data[ui].getPosRef()));
								d->data[ui].setMassToCharge(src->data[ui].getMassToCharge()+origin);
							}
							if(spin)
							{			
								delete d;
								return ERR_CALLBACK_FAIL;
							}
							
#else
							//Single threaded version
							size_t pos=0;
							//Copy across the ions into the target
							for(vector<IonHit>::const_iterator it=src->data.begin();
								it!=src->data.end(); ++it)
							{
								//set the position for the given ion
								d->data[pos].setPos((it->getPosRef()));
								d->data[pos].setMassToCharge(it->getMassToCharge() + origin);
								//update progress every CALLBACK ions
								if(!curProg--)
								{
									n+=NUM_CALLBACK;
									progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
									if(!(*callback)(false))
									{
										delete d;
										return ERR_CALLBACK_FAIL;
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
				case MODE_ROTATE:
				{
					Point3D origin=vectorParams[0];
					switch(dataIn[ui]->getStreamType())
					{
						case STREAM_TYPE_IONS:
						{

							const IonStreamData *src = (const IonStreamData *)dataIn[ui];
							//Set up output ion stream 
							IonStreamData *d=new IonStreamData;
							d->parent=this;
							try
							{
								d->data.resize(src->data.size());
							}
							catch(std::bad_alloc)
							{
								delete d;
								return ERR_NOMEM;
							}
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

							//TODO: Parallelise rotation
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
									if(!(*callback)(false))
									{
										delete d;
										return ERR_CALLBACK_FAIL;
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
				case MODE_SPATIAL_NOISE:
				{
					ASSERT(scalarParams.size() ==1 &&
							vectorParams.size()==0);
					switch(dataIn[ui]->getStreamType())
					{
						case STREAM_TYPE_IONS:
						{
							//Set up scaling output ion stream 
							IonStreamData *d=new IonStreamData;
							d->parent=this;

							const IonStreamData *src = (const IonStreamData *)dataIn[ui];
							try
							{
								d->data.resize(src->data.size());
							}
							catch(std::bad_alloc)
							{
								delete d;
								return ERR_NOMEM;
							}
							d->r = src->r;
							d->g = src->g;
							d->b = src->b;
							d->a = src->a;
							d->ionSize = src->ionSize;
							d->valueType=src->valueType;

							float scaleFactor=scalarParams[0];
							ASSERT(src->data.size() <= totalSize);
							unsigned int curProg=NUM_CALLBACK;

							//NOTE: This *cannot* be parallelised without parallelising the random
							// number generator safely. If using multiple random number generators,
							// one would need to ensure sufficient entropy in EACH generator. This
							// is not trivial to prove, and so has not been done here. Bootstrapping
							// each random number generator using non-random seeds could be problematic
							// same as feeding back a random number into other rng instances 
							//
							// One solution is to use the unix /dev/urandom interface or the windows
							// cryptographic API, alternatively use the TR1 header's mersenne twister with
							// multi-seeding:
							//  http://theo.phys.sci.hiroshima-u.ac.jp/~ishikawa/PRNG/mt_stream_en.html
							switch(noiseType)
							{
								case NOISETYPE_WHITE:
								{
									for(size_t ui=0;ui<src->data.size();ui++)
									{
										Point3D pt;

										pt.setValue(0,randGen.genUniformDev()-0.5f);
										pt.setValue(1,randGen.genUniformDev()-0.5f);
										pt.setValue(2,randGen.genUniformDev()-0.5f);

										pt*=scaleFactor;

										//set the position for the given ion
										d->data[ui].setPos(src->data[ui].getPosRef() + pt);
										d->data[ui].setMassToCharge(src->data[ui].getMassToCharge());
										
										
										if(!curProg--)
										{
											curProg=NUM_CALLBACK;
											progress.filterProgress= (unsigned int)((float)(ui)/((float)totalSize)*100.0f);
											if(!(*callback)(false))
											{
												delete d;
												return ERR_CALLBACK_FAIL;
											}
										}
									}
									break;
								}
								case NOISETYPE_GAUSSIAN:
								{
									for(size_t ui=0;ui<src->data.size();ui++)
									{
										Point3D pt;

										pt.setValue(0,randGen.genGaussDev());
										pt.setValue(1,randGen.genGaussDev());
										pt.setValue(2,randGen.genGaussDev());

										pt*=scaleFactor;

										//set the position for the given ion
										d->data[ui].setPos(src->data[ui].getPosRef() + pt);
										d->data[ui].setMassToCharge(src->data[ui].getMassToCharge());
										
										
										if(!curProg--)
										{
											curProg=NUM_CALLBACK;
											progress.filterProgress= (unsigned int)((float)(ui)/((float)totalSize)*100.0f);
											if(!(*callback)(false))
											{
												delete d;
												return ERR_CALLBACK_FAIL;
											}
										}
									}

									break;
								}
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
	}
	else
	{
		progress.step=1;
		progress.filterProgress=0;
		progress.stepName=TRANS("Collate");
		progress.maxStep=3;
		if(!(*callback)(true))
			return ERR_CALLBACK_FAIL;
		//we have to cross the streams (I thought that was bad?) 
		//  - Each dataset is no longer independant, and needs to
		//  be mixed with the other datasets. Bugger; sounds mem. expensive.
		
		//Set up output ion stream 
		IonStreamData *d=new IonStreamData;
		d->parent=this;
		
		//TODO: Better output colouring/size
		//Set up ion metadata
		d->r = 0.5;
		d->g = 0.5;
		d->b = 0.5;
		d->a = 0.5;
		d->ionSize = 2.0;
		d->valueType=TRANS("Mass-to-Charge (amu/e)");

		size_t n=0;
		size_t curPos=0;
		
		vector<float> ionData;

		//TODO: Ouch. Memory intensive -- could do a better job
		//of this?
		try
		{
			ionData.resize(totalSize);
			d->data.resize(totalSize);
		}
		catch(std::bad_alloc)
		{
			return ERR_NOMEM; 
		}

		//merge the datasets
		for(size_t ui=0;ui<dataIn.size() ;ui++)
		{
			switch(dataIn[ui]->getStreamType())
			{
				case STREAM_TYPE_IONS:
				{
		
					const IonStreamData *src = (const IonStreamData *)dataIn[ui];

					unsigned int curProg=NUM_CALLBACK;
					//Loop through the ions in this stream, and copy its data value
					for(size_t uj=0;uj<src->data.size();uj++)
					{
						ionData[uj+curPos] = src->data[uj].getMassToCharge();
						d->data[uj+curPos].setPos(src->data[uj].getPos());

						if(!curProg--)
						{
							n+=NUM_CALLBACK;
							progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);

							if(!(*callback)(false))
							{
								delete d;
								return ERR_CALLBACK_FAIL;
							}
						}

					}

					curPos+=src->data.size();
					break;
				}
				default:
					getOut.push_back(dataIn[ui]);
					break;
			}
		}

		n=0;
	
		progress.step=1;
		progress.filterProgress=0;
		progress.stepName=TRANS("Shuffle");
		if(!(*callback)(true))
		{
			delete d;
			return ERR_CALLBACK_FAIL;
		}
		//Shuffle the value data.TODO: callback functor	
		std::random_shuffle(d->data.begin(),d->data.end());	
		if(!(*callback)(false))
		{
			delete d;
			return ERR_CALLBACK_FAIL;
		}

		progress.step=2;
		progress.filterProgress=0;
		progress.stepName=TRANS("Splice");
		if(!(*callback)(true))
		{
			delete d;
			return ERR_CALLBACK_FAIL;
		}
		
		

		//Set the output data by splicing together the
		//shuffled values and the original position info
		n=0;
		unsigned int curProg=NUM_CALLBACK;
		for(size_t uj=0;uj<totalSize;uj++)
		{
			d->data[uj].setMassToCharge(ionData[uj]);
			if(!curProg--)
			{
				n+=NUM_CALLBACK;
				progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);

				if(!(*callback)(false))
				{
					delete d;
					return ERR_CALLBACK_FAIL;
				}
			}

		}

		if(cache)
		{
			d->cached=1;
			filterOutputs.push_back(d);
			cacheOK=true;
		}
		else
			d->cached=0;

		getOut.push_back(d);
		
	}
	return 0;
}


void TransformFilter::getProperties(FilterProperties &propertyList) const
{
	propertyList.data.clear();
	propertyList.keys.clear();
	propertyList.types.clear();
	propertyList.keyNames.clear();

	vector<unsigned int> type,keys;
	vector<pair<string,string> > s;

	string tmpStr;
	vector<pair<unsigned int,string> > choices;
	for(unsigned int ui=0;ui<MODE_ENUM_END; ui++)
		choices.push_back(make_pair(ui,TRANS(TRANSFORM_MODE_STRING[ui])));
	
	tmpStr= choiceString(choices,transformMode);
	choices.clear();
	
	s.push_back(make_pair(string(TRANS("Mode")),tmpStr));
	type.push_back(PROPERTY_TYPE_CHOICE);
	keys.push_back(KEY_MODE);
	
	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);
	propertyList.keyNames.push_back(TRANS("Type"));
	s.clear();type.clear();keys.clear();
	
	//non-translation transforms require a user to select an origin	
	if(transformMode == MODE_SCALE || transformMode == MODE_ROTATE)
	{
		vector<pair<unsigned int,string> > choices;
		for(unsigned int ui=0;ui<ORIGINMODE_END;ui++)
			choices.push_back(make_pair(ui,getOriginTypeString(ui)));
		
		tmpStr= choiceString(choices,originMode);

		s.push_back(make_pair(string(TRANS("Origin mode")),tmpStr));
		type.push_back(PROPERTY_TYPE_CHOICE);
		keys.push_back(KEY_ORIGINMODE);
	
		stream_cast(tmpStr,showOrigin);	
		s.push_back(make_pair(string(TRANS("Show marker")),tmpStr));
		type.push_back(PROPERTY_TYPE_BOOL);
		keys.push_back(KEY_TRANSFORM_SHOWORIGIN);
	}

	

	switch(transformMode)
	{
		case MODE_TRANSLATE:
		{
			ASSERT(vectorParams.size() == 1);
			ASSERT(scalarParams.size() == 0);
			
			stream_cast(tmpStr,vectorParams[0]);
			keys.push_back(KEY_ORIGIN);
			s.push_back(make_pair(TRANS("Translation"), tmpStr));
			type.push_back(PROPERTY_TYPE_POINT3D);
			break;
		}
		case MODE_TRANSLATE_VALUE:
		{
			ASSERT(vectorParams.size() == 0);
			ASSERT(scalarParams.size() == 1);
			
			
			if(originMode == ORIGINMODE_SELECT)
			{
				stream_cast(tmpStr,scalarParams[0]);
				keys.push_back(KEY_ORIGIN_VALUE);
				s.push_back(make_pair(TRANS("Origin"), tmpStr));
				type.push_back(PROPERTY_TYPE_REAL);
			}
			break;
		}
		case MODE_SCALE:
		{
			ASSERT(vectorParams.size() == 1);
			ASSERT(scalarParams.size() == 1);
			
			
			if(originMode == ORIGINMODE_SELECT)
			{
				stream_cast(tmpStr,vectorParams[0]);
				keys.push_back(KEY_ORIGIN);
				s.push_back(make_pair(TRANS("Origin"), tmpStr));
				type.push_back(PROPERTY_TYPE_POINT3D);
			}
			
			stream_cast(tmpStr,scalarParams[0]);
			keys.push_back(KEY_SCALEFACTOR);
			s.push_back(make_pair(TRANS("Scale Fact."), tmpStr));
			type.push_back(PROPERTY_TYPE_REAL);
			break;
		}
		case MODE_ROTATE:
		{
			ASSERT(vectorParams.size() == 2);
			ASSERT(scalarParams.size() == 1);
			if(originMode == ORIGINMODE_SELECT)
			{
				stream_cast(tmpStr,vectorParams[0]);
				keys.push_back(KEY_ORIGIN);
				s.push_back(make_pair(TRANS("Origin"), tmpStr));
				type.push_back(PROPERTY_TYPE_POINT3D);
			}
			stream_cast(tmpStr,vectorParams[1]);
			keys.push_back(KEY_ROTATE_AXIS);
			s.push_back(make_pair(TRANS("Axis"), tmpStr));
			type.push_back(PROPERTY_TYPE_POINT3D);

			stream_cast(tmpStr,scalarParams[0]);
			keys.push_back(KEY_ROTATE_ANGLE);
			s.push_back(make_pair(TRANS("Angle (deg)"), tmpStr));
			type.push_back(PROPERTY_TYPE_REAL);
			break;
		}
		case MODE_VALUE_SHUFFLE:
		{
			//No options...
			break;	
		}
		case MODE_SPATIAL_NOISE:
		{
			for(unsigned int ui=0;ui<NOISETYPE_END;ui++)
				choices.push_back(make_pair(ui,getNoiseTypeString(ui)));
			tmpStr= choiceString(choices,noiseType);
			choices.clear();
			
			s.push_back(make_pair(string(TRANS("Noise Type")),tmpStr));
			type.push_back(PROPERTY_TYPE_CHOICE);
			keys.push_back(KEY_NOISETYPE);


			stream_cast(tmpStr,scalarParams[0]);
			keys.push_back(KEY_NOISELEVEL);
			if(noiseType == NOISETYPE_WHITE)
				s.push_back(make_pair(TRANS("Noise level"), tmpStr));
			else if(noiseType == NOISETYPE_GAUSSIAN)
				s.push_back(make_pair(TRANS("Standard dev."), tmpStr));
			else
			{
				ASSERT(false);
			}

			type.push_back(PROPERTY_TYPE_REAL);


			break;	
		}
		default:
			ASSERT(false);
	}

	if(s.size() )
	{
		ASSERT(s.size() == type.size());
		ASSERT(type.size() == keys.size());
		propertyList.data.push_back(s);
		propertyList.types.push_back(type);
		propertyList.keys.push_back(keys);
		propertyList.keyNames.push_back(TRANS("Transform Params"));
	}

}

bool TransformFilter::setProperty( unsigned int set, unsigned int key,
					const std::string &value, bool &needUpdate)
{

	needUpdate=false;
	switch(key)
	{
		case KEY_MODE:
		{

			//TODO: Mkove these into an array,
			//so they are synced with ::getProperties 
			// (it wont work if these are not synced)
			if(value == TRANS("Translate"))
				transformMode= MODE_TRANSLATE;
			else if ( value == TRANS("Translate Value") )
				transformMode= MODE_TRANSLATE_VALUE;
			else if ( value == TRANS("Scale") )
				transformMode= MODE_SCALE;
			else if ( value == TRANS("Rotate"))
				transformMode= MODE_ROTATE;
			else if ( value == TRANS("Value Shuffle"))
				transformMode= MODE_VALUE_SHUFFLE;
			else if ( value == TRANS("Spatial Noise"))
				transformMode= MODE_SPATIAL_NOISE;
			else
			{
				ASSERT(false);
				return false;
			}

			vectorParams.clear();
			scalarParams.clear();
			switch(transformMode)
			{
				case MODE_SCALE:
					vectorParams.push_back(Point3D(0,0,0));
					scalarParams.push_back(1.0f);
					break;
				case MODE_TRANSLATE:
					vectorParams.push_back(Point3D(0,0,0));
					break;
				case MODE_TRANSLATE_VALUE:
					scalarParams.push_back(100.0f);
					break;
				case MODE_ROTATE:
					vectorParams.push_back(Point3D(0,0,0));
					vectorParams.push_back(Point3D(1,0,0));
					scalarParams.push_back(0.0f);
					break;
				case MODE_VALUE_SHUFFLE:
					break;
				case MODE_SPATIAL_NOISE:
					scalarParams.push_back(0.1f);
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
		case KEY_ROTATE_ANGLE:
		case KEY_SCALEFACTOR:
		case KEY_NOISELEVEL:
		case KEY_ORIGIN_VALUE:
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
		case KEY_ORIGIN:
		{
			Point3D newPt;
			if(!parsePointStr(value,newPt))
				return false;

			if(!(vectorParams[0] == newPt ))
			{
				vectorParams[0] = newPt;
				needUpdate=true;
				clearCache();
			}

			return true;
		}
		case KEY_ROTATE_AXIS:
		{
			ASSERT(vectorParams.size() ==2);
			ASSERT(scalarParams.size() ==1);
			Point3D newPt;
			if(!parsePointStr(value,newPt))
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
		case KEY_ORIGINMODE:
		{
			size_t i;
			for (i = 0; i < MODE_ENUM_END; i++)
				if (value == getOriginTypeString(i)) break;
		
			if( i == MODE_ENUM_END)
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

			showOrigin=(stripped=="1");

			needUpdate=true;

			break;
		}
		case KEY_NOISETYPE:
		{
			size_t i;
			for (i = 0; i < NOISETYPE_END; i++)
				if (value == getNoiseTypeString(i)) break;
		
			if( i == NOISETYPE_END)
				return false;

			if(noiseType != i)
			{
				noiseType = i;
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


std::string  TransformFilter::getErrString(unsigned int code) const
{

	switch(code)
	{
		//User aborted in a callback
		case ERR_CALLBACK_FAIL:
			return std::string(TRANS("Aborted"));
		//Caught a memory issue
		case ERR_NOMEM:
			return std::string(TRANS("Unable to allocate memory"));
	}
	ASSERT(false);
}

bool TransformFilter::writeState(std::ofstream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<" << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\""<< escapeXML(userString) << "\"/>"  << endl;
			f << tabs(depth+1) << "<transformmode value=\"" << transformMode<< "\"/>"<<endl;
			f << tabs(depth+1) << "<originmode value=\"" << originMode<< "\"/>"<<endl;
			
			f << tabs(depth+1) << "<noisetype value=\"" << noiseType<< "\"/>"<<endl;

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
			f << tabs(depth) << "</" << trueName() << ">" << endl;
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
	if(transformMode>= MODE_ENUM_END)
	       return false;	
	//====
	
	//Retrieve origination type 
	//====
	if(!XMLGetNextElemAttrib(nodePtr,originMode,"originmode","value"))
		return false;
	if(originMode>= ORIGINMODE_END)
	       return false;	
	//====
	
	//Retrieve origination type 
	//====
	if(!XMLGetNextElemAttrib(nodePtr,originMode,"noisetype","value"))
		return false;
	if(noiseType>= NOISETYPE_END)
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
		case MODE_TRANSLATE:
			if(vectorParams.size() != 1 || scalarParams.size() !=0)
				return false;
			break;
		case MODE_SCALE:
			if(vectorParams.size() != 1 || scalarParams.size() !=1)
				return false;
			break;
		case MODE_ROTATE:
			if(vectorParams.size() != 2 || scalarParams.size() !=1)
				return false;
			break;
		case MODE_TRANSLATE_VALUE:
			if(vectorParams.size() != 0 || scalarParams.size() !=1)
				return false;
			break;
		case MODE_VALUE_SHUFFLE:
		case MODE_SPATIAL_NOISE:
			break;
		default:
			ASSERT(false);
			return false;
	}
	return true;
}


unsigned int TransformFilter::getRefreshBlockMask() const
{
	//Only ions cannot go through this filter.
	return STREAM_TYPE_IONS;
}

unsigned int TransformFilter::getRefreshEmitMask() const
{
	if(showPrimitive)
		return STREAM_TYPE_IONS | STREAM_TYPE_DRAW;
	else
		return STREAM_TYPE_IONS;
}

unsigned int TransformFilter::getRefreshUseMask() const
{
	return STREAM_TYPE_IONS;
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
	ASSERT(i<ORIGINMODE_END);
	return TRANSFORM_ORIGIN_STRING[i];
}

std::string TransformFilter::getNoiseTypeString(unsigned int i) const
{
	switch(i)
	{
		case NOISETYPE_WHITE:
			return std::string(TRANS("White"));
		case NOISETYPE_GAUSSIAN:
			return std::string(TRANS("Gaussian"));
	}
	ASSERT(false);
}


#ifdef DEBUG

//Generate some synthetic data points, that lie within 0->span.
//span must be  a 3-wide array, and numPts will be generated.
//each entry in the array should be coprime for optimal results.
//filter pointer must be deleted.
IonStreamData *synthDataPoints(unsigned int span[],unsigned int numPts);
bool rotateTest();
bool translateTest();
bool scaleTest();
bool shuffleTest();


class MassCompare
{
	public:
		inline bool operator()(const IonHit &h1,const IonHit &h2) const
		{return h1.getMassToCharge()<h2.getMassToCharge();};
};

bool TransformFilter::runUnitTests()
{
	if(!rotateTest())
		return false;

	if(!translateTest())
		return false;

	if(!scaleTest())
		return false;

	if(!shuffleTest())
		return false;

	return true;
}

IonStreamData *synthDataPoints(unsigned int span[], unsigned int numPts)
{
	IonStreamData *d = new IonStreamData;
	
	for(unsigned int ui=0;ui<numPts;ui++)
	{
		IonHit h;
		h.setPos(Point3D(ui%span[0],
			ui%span[1],ui%span[2]));
		h.setMassToCharge(ui);
		d->data.push_back(h);
	}

	return d;
}

bool rotateTest()
{
	//Simulate some data to send to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	IonStreamData *d= new IonStreamData;

	RandNumGen rng;
	rng.initTimer();

	const unsigned int NUM_PTS=10000;

	
	//Build a  sphere of data points
	//by rejection method
	d->data.reserve(NUM_PTS/2.0);	
	for(unsigned int ui=0;ui<NUM_PTS;ui++)
	{
		Point3D tmp;
		tmp=Point3D(rng.genUniformDev()-0.5f,
				rng.genUniformDev()-0.5f,
				rng.genUniformDev()-0.5f);

		if(tmp.sqrMag() < 1.0f)
		{
			IonHit h;
			h.setPos(tmp);
			h.setMassToCharge(1);
			d->data.push_back(h);
		}
	}
		
	streamIn.push_back(d);

	//Set up the filter itself
	//---
	TransformFilter *f=new TransformFilter;
	f->setCaching(false);


	bool needUp;
	string s;
	f->setProperty(0,KEY_MODE,TRANS(TRANSFORM_MODE_STRING[MODE_ROTATE]),needUp);
	float tmpVal;
	tmpVal=rng.genUniformDev()*M_PI*2.0;
	stream_cast(s,tmpVal);
	f->setProperty(0,KEY_ROTATE_ANGLE,s,needUp);
	Point3D tmpPt;

	//NOTE: Technically there is a nonzero chance of this failing.
	tmpPt=Point3D(rng.genUniformDev()-0.5f,
			rng.genUniformDev()-0.5f,
			rng.genUniformDev()-0.5f);
	stream_cast(s,tmpPt);
	f->setProperty(0,KEY_ROTATE_AXIS,s,needUp);
	
	f->setProperty(0,KEY_ORIGINMODE,TRANSFORM_ORIGIN_STRING[ORIGINMODE_MASSCENTRE],needUp);
	f->setProperty(0,KEY_TRANSFORM_SHOWORIGIN,"0",needUp);
	//---


	//OK, so now do the rotation
	//Do the refresh
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p,dummyCallback),"refresh error code");
	delete f;

	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS,"stream type");
	TEST(streamOut[0]->getNumBasicObjects() == d->data.size(),"Ion count invariance");

	const IonStreamData *outData=(IonStreamData*)streamOut[0];

	Point3D massCentre[2];
	massCentre[0]=massCentre[1]=Point3D(0,0,0);
	//Now check that the mass centre has not moved
	for(unsigned int ui=0;ui<d->data.size();ui++)
		massCentre[0]+=d->data[ui].getPos();

	for(unsigned int ui=0;ui<outData->data.size();ui++)
		massCentre[1]+=outData->data[ui].getPos();


	TEST((massCentre[0]-massCentre[1]).sqrMag() < 
			2.0*sqrt(std::numeric_limits<float>::epsilon()),"mass centre invariance");

	//Rotating a sphere around its centre of mass
	// should not massively change the bounding box
	// however we don't quite have  a sphere, so we could have (at the most extreme,
	// a cube)
	BoundCube bc[2];
	bc[0]=getIonDataLimits(d->data);
	bc[1]=getIonDataLimits(outData->data);

	float volumeRat;
	volumeRat = bc[0].volume()/bc[1].volume();

	TEST(volumeRat > 0.5f && volumeRat < 2.0f, "volume ratio test");

	delete streamOut[0];
	delete d;
	return true;
}

bool translateTest()
{
	RandNumGen rng;
	rng.initTimer();
	
	//Simulate some data to send to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	IonStreamData *d;
	const unsigned int NUM_PTS=10000;

	unsigned int span[]={ 
			5, 7, 9
			};	
	d=synthDataPoints(span,NUM_PTS);
	streamIn.push_back(d);

	Point3D offsetPt;

	//Set up the filter itself
	//---
	TransformFilter *f=new TransformFilter;

	bool needUp;
	string s;
	f->setProperty(0,KEY_MODE,TRANSFORM_MODE_STRING[MODE_TRANSLATE],needUp);

	//NOTE: Technically there is a nonzero chance of this failing.
	offsetPt=Point3D(rng.genUniformDev()-0.5f,
			rng.genUniformDev()-0.5f,
			rng.genUniformDev()-0.5f);
	offsetPt[0]*=span[0];
	offsetPt[1]*=span[1];
	offsetPt[2]*=span[2];

	stream_cast(s,offsetPt);
	f->setProperty(0,KEY_ORIGIN,s,needUp);
	f->setProperty(0,KEY_TRANSFORM_SHOWORIGIN,"0",needUp);
	//---
	
	//Do the refresh
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p,dummyCallback),"Refresh error code");
	delete f;
	
	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS,"stream type");
	TEST(streamOut[0]->getNumBasicObjects() == d->data.size(),"Ion count invariance");

	const IonStreamData *outData=(IonStreamData*)streamOut[0];

	//Bound cube should move exactly as per the translation
	BoundCube bc[2];
	bc[0]=getIonDataLimits(d->data);
	bc[1]=getIonDataLimits(outData->data);

	for(unsigned int ui=0;ui<3;ui++)
	{
		for(unsigned int uj=0;uj<2;uj++)
		{
			float f;
			f=bc[0].getBound(ui,uj) -bc[1].getBound(ui,uj);
			TEST(fabs(f-offsetPt[ui]) < sqrt(std::numeric_limits<float>::epsilon()), "bound translation");
		}
	}

	delete d;
	delete streamOut[0];

	return true;
}


bool scaleTest() 
{
	//Simulate some data to send to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	IonStreamData *d;

	RandNumGen rng;
	rng.initTimer();

	const unsigned int NUM_PTS=10000;

	unsigned int span[]={ 
			5, 7, 9
			};	
	d=synthDataPoints(span,NUM_PTS);
	streamIn.push_back(d);

	//Set up the filter itself
	//---
	TransformFilter *f=new TransformFilter;

	bool needUp;
	string s;
	//Switch to scale mode
	f->setProperty(0,KEY_MODE,
			TRANS(TRANSFORM_MODE_STRING[MODE_SCALE]),needUp);


	//Switch to mass-centre origin
	f->setProperty(0,KEY_ORIGINMODE,
		TRANS(TRANSFORM_ORIGIN_STRING[ORIGINMODE_MASSCENTRE]),needUp);

	float scaleFact;
	//Pick some scale, both positive and negative.
	if(rng.genUniformDev() > 0.5)
		scaleFact=rng.genUniformDev()*10;
	else
		scaleFact=0.1f/(0.1f+rng.genUniformDev());

	stream_cast(s,scaleFact);

	f->setProperty(0,KEY_SCALEFACTOR,s,needUp);
	//Don't show origin marker
	f->setProperty(0,KEY_TRANSFORM_SHOWORIGIN,"0",needUp);
	//---


	//OK, so now do the rotation
	//Do the refresh
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p,dummyCallback),"refresh error code");
	delete f;

	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS,"stream type");
	TEST(streamOut[0]->getNumBasicObjects() == d->data.size(),"Ion count invariance");

	const IonStreamData *outData=(IonStreamData*)streamOut[0];

	//Scaling around its centre of mass
	// should scale the bounding box by the cube of the scale factor
	BoundCube bc[2];
	bc[0]=getIonDataLimits(d->data);
	bc[1]=getIonDataLimits(outData->data);

	float cubeOfScale=scaleFact*scaleFact*scaleFact;

	float volumeDelta;
	volumeDelta=fabs(bc[1].volume()/cubeOfScale - bc[0].volume() );

	TEST(volumeDelta < 100.0f*sqrt(std::numeric_limits<float>::epsilon()), "scaled volume test");

	delete streamOut[0];
	delete d;
	return true;
}

bool shuffleTest() 
{
	//Simulate some data to send to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	IonStreamData *d;

	RandNumGen rng;
	rng.initTimer();

	const unsigned int NUM_PTS=1000;

	unsigned int span[]={ 
			5, 7, 9
			};	
	d=synthDataPoints(span,NUM_PTS);
	streamIn.push_back(d);

	//Set up the filter itself
	//---
	TransformFilter *f=new TransformFilter;

	bool needUp;
	//Switch to shuffle mode
	TEST(f->setProperty(0,KEY_MODE,
			TRANS(TRANSFORM_MODE_STRING[MODE_VALUE_SHUFFLE]),needUp),"refresh error code");
	//---


	//OK, so now run the shuffle 
	//Do the refresh
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p,dummyCallback),"refresh error code");
	delete f;

	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS,"stream type");
	TEST(streamOut[0]->getNumBasicObjects() == d->data.size(),"Ion count invariance");

	TEST(streamOut[0]->getNumBasicObjects() == d->data.size(),"Ion count invariance");

	IonStreamData *outData=(IonStreamData*)streamOut[0];

	//Check to see that the output masses each exist in the input,
	//but are not in  the same sequence
	//---
	

	bool sequenceDifferent=false;	
	for(size_t ui=0;ui<d->data.size();ui++)
	{
		if(d->data[ui].getMassToCharge() == outData->data[ui].getMassToCharge())
		{
			sequenceDifferent=true;
			break;
		}
	}
	TEST(sequenceDifferent,
		"Should be shuffled - Prob. of sequence being identical in both orig & shuffled cases is very low");
	//Sort masses
	MassCompare cmp;
	std::sort(outData->data.begin(),outData->data.end(),cmp);
	std::sort(d->data.begin(),d->data.end(),cmp);


	for(size_t ui=0;ui<d->data.size();ui++)
	{
		TEST(d->data[ui].getMassToCharge() == outData->data[ui].getMassToCharge(),"Shuffle + Sort mass should be the same");
	
	}



	delete streamOut[0];
	delete d;
	return true;
}

#endif
