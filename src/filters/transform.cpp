#include "transform.h"

#include "../xmlHelper.h"

#include <algorithm>

enum
{
	KEY_TRANSFORM_MODE,
	KEY_TRANSFORM_SCALEFACTOR,
	KEY_TRANSFORM_ORIGIN,
	KEY_TRANSFORM_SHOWORIGIN,
	KEY_TRANSFORM_ORIGINMODE,
	KEY_TRANSFORM_ROTATE_ANGLE,
	KEY_TRANSFORM_ROTATE_AXIS,
};

//Possible transform modes (scaling, rotation etc)
enum
{
	TRANSFORM_TRANSLATE,
	TRANSFORM_SCALE,
	TRANSFORM_ROTATE,
	TRANSFORM_VALUE_SHUFFLE,
	TRANSFORM_MODE_ENUM_END
};

//!Possible mode for selection of origin in transform filter
enum
{
	TRANSFORM_ORIGINMODE_SELECT,
	TRANSFORM_ORIGINMODE_CENTREBOUND,
	TRANSFORM_ORIGINMODE_MASSCENTRE,
	TRANSFORM_ORIGINMODE_END, // Not actually origin mode, just end of enum
};

//!Error codes
enum
{
	TRANSFORM_CALLBACK_FAIL=1,
};

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

DrawStreamData* TransformFilter::makeMarkerSphere(SelectionDevice<Filter>* &s) const
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
		case TRANSFORM_ORIGINMODE_SELECT:
			break;
		default:
			ASSERT(false);
	}

	//If the user is using a transform mode that requires origin selection 
	if(showOrigin && (transformMode == TRANSFORM_ROTATE ||
			transformMode == TRANSFORM_SCALE) )
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
	if( transformMode != TRANSFORM_VALUE_SHUFFLE)
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

							const IonStreamData *src = (const IonStreamData *)dataIn[ui];
							//Set up output ion stream 
							IonStreamData *d=new IonStreamData;
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
	}
	else
	{
		progress.step=1;
		progress.filterProgress=0;
		progress.stepName="Collate";
		progress.maxStep=3;
		//we have to cross the streams (I thought that was bad?) 
		//  - Each dataset is no longer independant, and needs to
		//  be mixed with the other datasets. Bugger; sounds mem. expensive.
		
		//Set up output ion stream 
		IonStreamData *d=new IonStreamData;
		
		//TODO: Better output colouring/size
		//Set up ion metadata
		d->r = 0.5;
		d->g = 0.5;
		d->b = 0.5;
		d->a = 0.5;
		d->ionSize = 2.0;
		d->valueType="Mass-to-Charge";

		size_t n=0;
		size_t curPos=0;
		
		vector<float> ionData;

		//TODO: Ouch. Memory intensive -- could do a better job
		//of this
		ionData.resize(totalSize);
		d->data.resize(totalSize);

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

							if(!(*callback)())
							{
								delete d;
								return TRANSFORM_CALLBACK_FAIL;
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
		progress.stepName="Shuffle";
		if(!(*callback)())
		{
			delete d;
			return TRANSFORM_CALLBACK_FAIL;
		}
		//Shuffle the value data.TODO: callback functor	
		std::random_shuffle(d->data.begin(),d->data.end());	
		if(!(*callback)())
		{
			delete d;
			return TRANSFORM_CALLBACK_FAIL;
		}

		progress.step=2;
		progress.filterProgress=0;
		progress.stepName="Shuffle";
		if(!(*callback)())
		{
			delete d;
			return TRANSFORM_CALLBACK_FAIL;
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

				if(!(*callback)())
				{
					delete d;
					return TRANSFORM_CALLBACK_FAIL;
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
	choices.push_back(make_pair((unsigned int) TRANSFORM_TRANSLATE,"Translate"));
	choices.push_back(make_pair((unsigned int)TRANSFORM_SCALE,"Scale"));
	choices.push_back(make_pair((unsigned int)TRANSFORM_ROTATE,"Rotate"));
	choices.push_back(make_pair((unsigned int)TRANSFORM_VALUE_SHUFFLE,"Value Shuffle"));
	
	tmpStr= choiceString(choices,transformMode);
	
	s.push_back(make_pair(string("Mode"),tmpStr));
	type.push_back(PROPERTY_TYPE_CHOICE);
	keys.push_back(KEY_TRANSFORM_MODE);
	
	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);
	propertyList.keyNames.push_back("Type");
	s.clear();type.clear();keys.clear();
	
	//non-translation transforms require a user to select an origin	
	if(transformMode == TRANSFORM_SCALE || transformMode == TRANSFORM_ROTATE)
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
		}
		case TRANSFORM_VALUE_SHUFFLE:
		{
			//No options...
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
		propertyList.keyNames.push_back("Trans. Params");
	}

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
			else if ( value == "Value Shuffle")
				transformMode= TRANSFORM_VALUE_SHUFFLE;
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
				case TRANSFORM_VALUE_SHUFFLE:
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
		case KEY_TRANSFORM_ROTATE_AXIS:
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
			f << tabs(depth) << "<" << trueName() << ">" << endl;
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
			if(vectorParams.size() != 2 || scalarParams.size() !=1)
				return false;
			break;
		case TRANSFORM_VALUE_SHUFFLE:
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
