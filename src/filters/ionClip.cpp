#include "ionClip.h"

#include "../xmlHelper.h"

//!Error codes
enum 
{
	IONCLIP_CALLBACK_FAIL=1,
	IONCLIP_BAD_ALLOC,
};

//FIXME: make member functions.
bool inSphere(const Point3D &testPt, const Point3D &origin, float sqrRadius)
{
	return testPt.sqrDist(origin)< sqrRadius;
}

bool inFrontPlane(const Point3D &testPt, const Point3D &origin, const Point3D &planeVec)
{
	return ((testPt-origin).dotProd(planeVec) > 0.0f);
}

unsigned int primitiveID(const std::string &str)
{
	if(str == "Sphere")
		return PRIMITIVE_SPHERE;
	if(str == "Plane")
		return PRIMITIVE_PLANE;
	if(str == "Cylinder")
		return PRIMITIVE_CYLINDER;
	if(str == "Aligned box")
		return PRIMITIVE_AAB;

	ASSERT(false);
	return (unsigned int)-1;
}

std::string primitiveStringFromID(unsigned int id)
{
	switch(id)
	{
		case PRIMITIVE_SPHERE:
			return string("Sphere");
		case PRIMITIVE_PLANE:
			return string("Plane");
		case PRIMITIVE_CYLINDER:
			return string("Cylinder");
	 	case PRIMITIVE_AAB:
			return string("Aligned box");
		default:
			ASSERT(false);
	}
	return string("");
}
enum {
	KEY_IONCLIP_ORIGIN=1,
	KEY_IONCLIP_PRIMITIVE_TYPE,
	KEY_IONCLIP_RADIUS,
	KEY_IONCLIP_PRIMITIVE_SHOW,
	KEY_IONCLIP_PRIMITIVE_INVERTCLIP,
	KEY_IONCLIP_NORMAL,
	KEY_IONCLIP_CORNER,
	KEY_IONCLIP_AXIS_LOCKMAG,
};

Filter *IonClipFilter::cloneUncached() const
{
	IonClipFilter *p = new IonClipFilter;
	p->primitiveType=primitiveType;
	p->invertedClip=invertedClip;
	p->showPrimitive=showPrimitive;
	p->vectorParams.resize(vectorParams.size());
	p->scalarParams.resize(scalarParams.size());
	
	std::copy(vectorParams.begin(),vectorParams.end(),p->vectorParams.begin());
	std::copy(scalarParams.begin(),scalarParams.end(),p->scalarParams.begin());

	p->lockAxisMag = lockAxisMag;

	//We are copying wether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;

	return p;
}

//!Get approx number of bytes for caching output
size_t IonClipFilter::numBytesForCache(size_t nObjects) const
{
	//Without full processing, we cannot tell, so provide upper estimate.
	return nObjects*IONDATA_SIZE;
}

//!update filter
unsigned int IonClipFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
			std::vector<const FilterStreamData *> &getOut, ProgressData &progress, 
								bool (*callback)(void))
{
	ASSERT(vectorParams.size() || scalarParams.size());	
	//Clear selection devices
	devices.clear();

	if(showPrimitive)
	{
		//construct a new primitive, do not cache
		DrawStreamData *drawData=new DrawStreamData;
		switch(primitiveType)
		{
			case IONCLIP_PRIMITIVE_SPHERE:
			{
				//Add drawable components
				DrawSphere *dS = new DrawSphere;
				dS->setOrigin(vectorParams[0]);
				dS->setRadius(scalarParams[0]);
				//FIXME: Alpha blending is all screwed up. May require more
				//advanced drawing in scene. (front-back drawing).
				//I have set alpha=1 for now.
				dS->setColour(0.5,0.5,0.5,1.0);
				dS->setLatSegments(40);
				dS->setLongSegments(40);
				dS->wantsLight=true;
				drawData->drawables.push_back(dS);

				//Set up selection "device" for user interaction
				//Note the order of s->addBinding is critical,
				//as bindings are selected by first match.
				//====
				//The object is selectable
				dS->canSelect=true;

				SelectionDevice<Filter> *s = new SelectionDevice<Filter>(this);
				SelectionBinding b[3];

				//Apple doesn't have right click, so we need
				//to hook up an additional system for them.
				//Don't use ifdefs, as this would be useful for
				//normal laptops and the like.
				b[0].setBinding(SELECT_BUTTON_LEFT,FLAG_CMD,DRAW_SPHERE_BIND_ORIGIN,
							BINDING_SPHERE_ORIGIN,dS->getOrigin(),dS);
				b[0].setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
				s->addBinding(b[0]);

				//Bind the drawable object to the properties we wish
				//to be able to modify
				b[1].setBinding(SELECT_BUTTON_LEFT,0,DRAW_SPHERE_BIND_RADIUS,
					BINDING_SPHERE_RADIUS,dS->getRadius(),dS);
				b[1].setInteractionMode(BIND_MODE_FLOAT_TRANSLATE);
				b[1].setFloatLimits(0,std::numeric_limits<float>::max());
				s->addBinding(b[1]);

				b[2].setBinding(SELECT_BUTTON_RIGHT,0,DRAW_SPHERE_BIND_ORIGIN,
					BINDING_SPHERE_ORIGIN,dS->getOrigin(),dS);	
				b[2].setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
				s->addBinding(b[2]);
					
				devices.push_back(s);
				//=====
				break;
			}
			case IONCLIP_PRIMITIVE_PLANE:
			{
				//Origin + normal
				ASSERT(vectorParams.size() == 2);

				//Add drawable components
				DrawSphere *dS = new DrawSphere;
				dS->setOrigin(vectorParams[0]);
				dS->setRadius(drawScale/10);
				dS->setColour(0.5,0.5,0.5,1.0);
				dS->setLatSegments(40);
				dS->setLongSegments(40);
				dS->wantsLight=true;
				drawData->drawables.push_back(dS);
				
				DrawVector *dV  = new DrawVector;
				dV->setOrigin(vectorParams[0]);
				dV->setVector(vectorParams[1]*drawScale);
				drawData->drawables.push_back(dV);
				
				//Set up selection "device" for user interaction
				//====
				//The object is selectable
				dS->canSelect=true;
				dV->canSelect=true;

				SelectionDevice<Filter> *s = new SelectionDevice<Filter>(this);
				SelectionBinding b[2];
				//Bind the drawable object to the properties we wish
				//to be able to modify

				//Bind orientation to vector left click
				b[0].setBinding(SELECT_BUTTON_LEFT,0,DRAW_VECTOR_BIND_ORIENTATION,
					BINDING_PLANE_DIRECTION, dV->getVector(),dV);
				b[0].setInteractionMode(BIND_MODE_POINT3D_ROTATE);
				b[0].setFloatLimits(0,std::numeric_limits<float>::max());
				s->addBinding(b[0]);
				
				//Bind translation to sphere left click
				b[1].setBinding(SELECT_BUTTON_LEFT,0,DRAW_SPHERE_BIND_ORIGIN,
						BINDING_PLANE_ORIGIN,dS->getOrigin(),dS);	
				b[1].setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
				s->addBinding(b[1]);


				devices.push_back(s);
				//=====
				break;
			}
			case IONCLIP_PRIMITIVE_CYLINDER:
			{
				//Origin + normal
				ASSERT(vectorParams.size() == 2);
				//Add drawable components
				DrawCylinder *dC = new DrawCylinder;
				dC->setOrigin(vectorParams[0]);
				dC->setRadius(scalarParams[0]);
				dC->setColour(0.5,0.5,0.5,1.0);
				dC->setSlices(40);
				dC->setLength(sqrt(vectorParams[1].sqrMag()));
				dC->setDirection(vectorParams[1]);
				dC->wantsLight=true;
				drawData->drawables.push_back(dC);
				
				//Set up selection "device" for user interaction
				//====
				//The object is selectable
				dC->canSelect=true;
				//Start and end radii must be the same (not a
				//tapered cylinder)
				dC->lockRadii();

				SelectionDevice<Filter> *s = new SelectionDevice<Filter>(this);
				SelectionBinding b;
				//Bind the drawable object to the properties we wish
				//to be able to modify

				//Bind left + command button to move
				b.setBinding(SELECT_BUTTON_LEFT,FLAG_CMD,DRAW_CYLINDER_BIND_ORIGIN,
					BINDING_CYLINDER_ORIGIN,dC->getOrigin(),dC);	
				b.setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
				s->addBinding(b);

				//Bind left + shift to change orientation
				b.setBinding(SELECT_BUTTON_LEFT,FLAG_SHIFT,DRAW_CYLINDER_BIND_DIRECTION,
					BINDING_CYLINDER_DIRECTION,dC->getDirection(),dC);	

				if(lockAxisMag)
					b.setInteractionMode(BIND_MODE_POINT3D_ROTATE_LOCK);
				else
					b.setInteractionMode(BIND_MODE_POINT3D_ROTATE);
				s->addBinding(b);

				//Bind right button to changing position 
				b.setBinding(SELECT_BUTTON_RIGHT,0,DRAW_CYLINDER_BIND_ORIGIN,
					BINDING_CYLINDER_ORIGIN,dC->getOrigin(),dC);	
				b.setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
				s->addBinding(b);
					
				//Bind middle button to changing orientation
				b.setBinding(SELECT_BUTTON_MIDDLE,0,DRAW_CYLINDER_BIND_DIRECTION,
					BINDING_CYLINDER_DIRECTION,dC->getDirection(),dC);	
				if(lockAxisMag)
					b.setInteractionMode(BIND_MODE_POINT3D_ROTATE_LOCK);
				else
					b.setInteractionMode(BIND_MODE_POINT3D_ROTATE);
				s->addBinding(b);
					
				//Bind left button to changing radius
				b.setBinding(SELECT_BUTTON_LEFT,0,DRAW_CYLINDER_BIND_RADIUS,
					BINDING_CYLINDER_RADIUS,dC->getRadius(),dC);
				b.setInteractionMode(BIND_MODE_FLOAT_TRANSLATE);
				b.setFloatLimits(0,std::numeric_limits<float>::max());
				s->addBinding(b); 
				
				devices.push_back(s);
				//=====
				
				break;
			}
			case IONCLIP_PRIMITIVE_AAB:
			{
				//Centre  + corner
				ASSERT(vectorParams.size() == 2);
				ASSERT(scalarParams.size() == 0);

				//Add drawable components
				DrawRectPrism *dR = new DrawRectPrism;
				dR->setAxisAligned(vectorParams[0]-vectorParams[1],
							vectorParams[0] + vectorParams[1]);
				dR->setColour(0.5,0.5,0.5,1.0);
				dR->setDrawMode(DRAW_FLAT);
				dR->wantsLight=true;
				drawData->drawables.push_back(dR);
				
				
				//Set up selection "device" for user interaction
				//====
				//The object is selectable
				dR->canSelect=true;

				SelectionDevice<Filter> *s = new SelectionDevice<Filter>(this);
				SelectionBinding b[2];
				//Bind the drawable object to the properties we wish
				//to be able to modify

				//Bind orientation to sphere left click
				b[0].setBinding(SELECT_BUTTON_LEFT,0,DRAW_RECT_BIND_TRANSLATE,
					BINDING_RECT_TRANSLATE, vectorParams[0],dR);
				b[0].setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
				s->addBinding(b[0]);


				b[1].setBinding(SELECT_BUTTON_RIGHT,0,DRAW_RECT_BIND_CORNER_MOVE,
						BINDING_RECT_CORNER_MOVE, vectorParams[1],dR);
				b[1].setInteractionMode(BIND_MODE_POINT3D_SCALE);
				s->addBinding(b[1]);

				devices.push_back(s);
				//=====
				break;
			}
			default:
				ASSERT(false);

		}
	
		drawData->cached=0;	
		getOut.push_back(drawData);
	}

	//use the cached copy of the data if we have it.
	if(cacheOK)
	{
		for(unsigned int ui=0;ui<filterOutputs.size(); ui++)
			getOut.push_back(filterOutputs[ui]);

		for(unsigned int ui=0;ui<dataIn.size() ;ui++)
		{
			//We don't cache anything but our modification
			//to the ion stream data types. so we propagate
			//these.
			if(dataIn[ui]->getStreamType() != STREAM_TYPE_IONS)
				getOut.push_back(dataIn[ui]);
		}
			
		return 0;
	}


	IonStreamData *d=0;
	try
	{
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		size_t n=0;
		size_t totalSize=numElements(dataIn);

		//Loop through each data set
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS:
			{
				d=new IonStreamData;
				switch(primitiveType)
				{
					case IONCLIP_PRIMITIVE_SPHERE:
					{
						//origin + radius
						ASSERT(vectorParams.size() == 1);
						ASSERT(scalarParams.size() == 1);
						ASSERT(scalarParams[0] >= 0.0f);
						float sqrRad = scalarParams[0]*scalarParams[0];

						unsigned int curProg=NUM_CALLBACK;
						//Loop through each ion in the dataset
						for(vector<IonHit>::const_iterator it=((const IonStreamData *)dataIn[ui])->data.begin();
							       it!=((const IonStreamData *)dataIn[ui])->data.end(); ++it)
						{
							//Use XOR operand
							if((inSphere(it->getPosRef(),vectorParams[0],sqrRad)) ^ (invertedClip))
								d->data.push_back(*it);
							
							//update progress every CALLBACK ions
							if(!curProg--)
							{
								n+=NUM_CALLBACK;
								progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
								curProg=NUM_CALLBACK;
								if(!(*callback)())
								{
									delete d;
									return IONCLIP_CALLBACK_FAIL;
								}
							}
						}
						break;
					}
					case IONCLIP_PRIMITIVE_PLANE:
					{
						//Origin + normal
						ASSERT(vectorParams.size() == 2);


						//Loop through each data set
						unsigned int curProg=NUM_CALLBACK;
						//Loop through each ion in the dataset
						for(vector<IonHit>::const_iterator it=((const IonStreamData *)dataIn[ui])->data.begin();
							       it!=((const IonStreamData *)dataIn[ui])->data.end(); ++it)
						{
							//Use XOR operand
							if((inFrontPlane(it->getPosRef(),vectorParams[0],vectorParams[1])) ^ invertedClip)
								d->data.push_back(*it);

							//update progress every CALLBACK ions
							if(!curProg--)
							{
								n+=NUM_CALLBACK;
								progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
								curProg=NUM_CALLBACK;
								if(!(*callback)())
								{
									delete d;
									return IONCLIP_CALLBACK_FAIL;
								}
							}
						}
						break;
					}
					case IONCLIP_PRIMITIVE_CYLINDER:
					{
						//Origin + axis
						ASSERT(vectorParams.size() == 2);
						//Radius perp. to axis
						ASSERT(scalarParams.size() == 1);


						unsigned int curProg=NUM_CALLBACK;
						Point3f rotVec;
						//Cross product desired drection with default
						//direction to produce rotation vector
						Point3D dir(0.0f,0.0f,1.0f),direction;
						direction=vectorParams[1];
						direction.normalise();

						float angle = dir.angle(direction);

						float halfLen=sqrt(vectorParams[1].sqrMag())/2.0f;
						float sqrRad=scalarParams[0]*scalarParams[0];
						//Check that we actually need to rotate, to avoid numerical singularity 
						//when cylinder axis is too close to (or is) z-axis
						if(angle > sqrt(std::numeric_limits<float>::epsilon()))
						{
							dir = dir.crossProd(direction);
							dir.normalise();

							rotVec.fx=dir[0];
							rotVec.fy=dir[1];
							rotVec.fz=dir[2];

							Quaternion q1;

						
							//Generate the rotating quaternions
							quat_get_rot_quat(&rotVec,-angle,&q1);
							//pre-compute cylinder length and radius^2
							//Loop through each ion in the dataset
							for(vector<IonHit>::const_iterator it=((const IonStreamData *)dataIn[ui])->data.begin();
								       it!=((const IonStreamData *)dataIn[ui])->data.end(); ++it)
							{
								Point3f p;
								//Translate to get position w respect to cylinder centre
								Point3D ptmp;
								ptmp=it->getPosRef()-vectorParams[0];
								p.fx=ptmp[0];
								p.fy=ptmp[1];
								p.fz=ptmp[2];
								//rotate ion position into cylindrical coordinates
								quat_rot_apply_quat(&p,&q1);


								//Keep ion if inside cylinder XOR inversion of the clippping (inside vs outside)
								if((p.fz < halfLen && p.fz > -halfLen && p.fx*p.fx+p.fy*p.fy < sqrRad)  ^ invertedClip)
										d->data.push_back(*it);

								//update progress every CALLBACK ions
								if(!curProg--)
								{
									n+=NUM_CALLBACK;
									progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
									curProg=NUM_CALLBACK;
									if(!(*callback)())
									{
										delete d;
										return IONCLIP_CALLBACK_FAIL;
									}
								}
							}
				
						}
						else
						{
							//Too close to the z-axis, rotation vector is unable to be stably computed,
							//and we don't need to rotate anyway
							for(vector<IonHit>::const_iterator it=((const IonStreamData *)dataIn[ui])->data.begin();
								       it!=((const IonStreamData *)dataIn[ui])->data.end(); ++it)
							{
								Point3D ptmp;
								ptmp=it->getPosRef()-vectorParams[0];
								
								//Keep ion if inside cylinder XOR inversion of the clippping (inside vs outside)
								if((ptmp[2] < halfLen && ptmp[2] > -halfLen && ptmp[0]*ptmp[0]+ptmp[1]*ptmp[1] < sqrRad)  ^ invertedClip)
										d->data.push_back(*it);

								//update progress every CALLBACK ions
								if(!curProg--)
								{
									n+=NUM_CALLBACK;
									progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
									curProg=NUM_CALLBACK;
									if(!(*callback)())
									{
										delete d;
										return IONCLIP_CALLBACK_FAIL;
									}
								}
							}
							
						}
						break;
					}
					case IONCLIP_PRIMITIVE_AAB:
					{
						//origin + corner
						ASSERT(vectorParams.size() == 2);
						ASSERT(scalarParams.size() == 0);
						unsigned int curProg=NUM_CALLBACK;

						BoundCube c;
						c.setBounds(vectorParams[0]+vectorParams[1],vectorParams[0]-vectorParams[1]);
						//Loop through each ion in the dataset
						for(vector<IonHit>::const_iterator it=((const IonStreamData *)dataIn[ui])->data.begin();
							       it!=((const IonStreamData *)dataIn[ui])->data.end(); ++it)
						{
							//Use XOR operand
							if((c.containsPt(it->getPosRef())) ^ (invertedClip))
								d->data.push_back(*it);
							
							//update progress every CALLBACK ions
							if(!curProg--)
							{
								n+=NUM_CALLBACK;
								progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
								curProg=NUM_CALLBACK;
								if(!(*callback)())
								{
									delete d;
									return IONCLIP_CALLBACK_FAIL;
								}
							}
						}
						break;
					}


					default:
						ASSERT(false);
						return 1;
				}


				//Copy over other attributes
				d->r = ((IonStreamData *)dataIn[ui])->r;
				d->g = ((IonStreamData *)dataIn[ui])->g;
				d->b =((IonStreamData *)dataIn[ui])->b;
				d->a =((IonStreamData *)dataIn[ui])->a;
				d->ionSize =((IonStreamData *)dataIn[ui])->ionSize;
				d->representationType=((IonStreamData *)dataIn[ui])->representationType;

				//getOut is const, so shouldn't be modified
				if(cache)
				{
					d->cached=1;
					filterOutputs.push_back(d);
					cacheOK=true;
				}
				else
					d->cached=0;
				

				getOut.push_back(d);
				d=0;
				break;
			}
			default:
				//Just copy across the ptr, if we are unfamiliar with this type
				getOut.push_back(dataIn[ui]);	
				break;
		}

	}
	}
	catch(std::bad_alloc)
	{
		if(d)
			delete d;
		return IONCLIP_BAD_ALLOC;
	}
	return 0;

}

//!Get the properties of the filter, in key-value form. First vector is for each output.
void IonClipFilter::getProperties(FilterProperties &propertyList) const
{
	ASSERT(vectorParams.size() || scalarParams.size());	
	propertyList.data.clear();
	propertyList.keys.clear();
	propertyList.types.clear();

	vector<unsigned int> type,keys;
	vector<pair<string,string> > s;


	string str;
	//Let the user know what the valid values for Primitive type
	string tmpChoice,tmpStr;


	vector<pair<unsigned int,string> > choices;

	choices.push_back(make_pair((unsigned int)PRIMITIVE_SPHERE ,
				primitiveStringFromID(PRIMITIVE_SPHERE)));
	choices.push_back(make_pair((unsigned int)PRIMITIVE_PLANE ,
				primitiveStringFromID(PRIMITIVE_PLANE)));
	choices.push_back(make_pair((unsigned int)PRIMITIVE_CYLINDER ,
				primitiveStringFromID(PRIMITIVE_CYLINDER)));
	choices.push_back(make_pair((unsigned int)PRIMITIVE_AAB,
				primitiveStringFromID(PRIMITIVE_AAB)));

	tmpStr= choiceString(choices,primitiveType);
	s.push_back(make_pair(string("Primitive"),tmpStr));
	type.push_back(PROPERTY_TYPE_CHOICE);
	keys.push_back(KEY_IONCLIP_PRIMITIVE_TYPE);
	
	stream_cast(str,showPrimitive);
	keys.push_back(KEY_IONCLIP_PRIMITIVE_SHOW);
	s.push_back(make_pair("Show Primitive", str));
	type.push_back(PROPERTY_TYPE_BOOL);
	
	stream_cast(str,invertedClip);
	keys.push_back(KEY_IONCLIP_PRIMITIVE_INVERTCLIP);
	s.push_back(make_pair("Invert Clip", str));
	type.push_back(PROPERTY_TYPE_BOOL);

	switch(primitiveType)
	{
		case IONCLIP_PRIMITIVE_SPHERE:
		{
			ASSERT(vectorParams.size() == 1);
			ASSERT(scalarParams.size() == 1);
			stream_cast(str,vectorParams[0]);
			keys.push_back(KEY_IONCLIP_ORIGIN);
			s.push_back(make_pair("Origin", str));
			type.push_back(PROPERTY_TYPE_POINT3D);
			
			stream_cast(str,scalarParams[0]);
			keys.push_back(KEY_IONCLIP_RADIUS);
			s.push_back(make_pair("Radius", str));
			type.push_back(PROPERTY_TYPE_REAL);

			break;
		}
		case IONCLIP_PRIMITIVE_PLANE:
		{
			ASSERT(vectorParams.size() == 2);
			ASSERT(scalarParams.size() == 0);
			stream_cast(str,vectorParams[0]);
			keys.push_back(KEY_IONCLIP_ORIGIN);
			s.push_back(make_pair("Origin", str));
			type.push_back(PROPERTY_TYPE_POINT3D);
			
			stream_cast(str,vectorParams[1]);
			keys.push_back(KEY_IONCLIP_NORMAL);
			s.push_back(make_pair("Plane Normal", str));
			type.push_back(PROPERTY_TYPE_POINT3D);

			break;
		}
		case IONCLIP_PRIMITIVE_CYLINDER:
		{
			ASSERT(vectorParams.size() == 2);
			ASSERT(scalarParams.size() == 1);
			stream_cast(str,vectorParams[0]);
			keys.push_back(KEY_IONCLIP_ORIGIN);
			s.push_back(make_pair("Origin", str));
			type.push_back(PROPERTY_TYPE_POINT3D);
			
			stream_cast(str,vectorParams[1]);
			keys.push_back(KEY_IONCLIP_NORMAL);
			s.push_back(make_pair("Axis", str));
			type.push_back(PROPERTY_TYPE_POINT3D);
			
			if(lockAxisMag)
				str="1";
			else
				str="0";
			keys.push_back(KEY_IONCLIP_AXIS_LOCKMAG);
			s.push_back(make_pair("Lock Axis Mag.", str));
			type.push_back(PROPERTY_TYPE_BOOL);

			stream_cast(str,scalarParams[0]);
			keys.push_back(KEY_IONCLIP_RADIUS);
			s.push_back(make_pair("Radius", str));
			type.push_back(PROPERTY_TYPE_POINT3D);
			break;
		}
		case IONCLIP_PRIMITIVE_AAB:
		{
			ASSERT(vectorParams.size() == 2);
			ASSERT(scalarParams.size() == 0);
			stream_cast(str,vectorParams[0]);
			keys.push_back(KEY_IONCLIP_ORIGIN);
			s.push_back(make_pair("Origin", str));
			type.push_back(PROPERTY_TYPE_POINT3D);
			
			stream_cast(str,vectorParams[1]);
			keys.push_back(KEY_IONCLIP_CORNER);
			s.push_back(make_pair("Corner offset", str));
			type.push_back(PROPERTY_TYPE_POINT3D);
			break;
		}
		default:
			ASSERT(false);
	}
	
	
	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);

	ASSERT(keys.size() == type.size());	
}

//!Set the properties for the nth filter. Returns true if prop set OK
bool IonClipFilter::setProperty(unsigned int set,unsigned int key, 
				const std::string &value, bool &needUpdate)
{

	needUpdate=false;
	ASSERT(set == 0);

	switch(key)
	{
		case KEY_IONCLIP_PRIMITIVE_TYPE:
		{
			unsigned int newPrimitive;

			newPrimitive=primitiveID(value);
			if(newPrimitive == (unsigned int)-1)
				return false;	

			primitiveType=newPrimitive;

			switch(primitiveType)
			{
				//If we are switchign between essentially
				//similar data types, don't reset the data.
				//Otherwise, wipe it and try again
				case IONCLIP_PRIMITIVE_SPHERE:
					if(vectorParams.size() !=1)
					{
						vectorParams.clear();
						vectorParams.push_back(Point3D(0,0,0));
					}
					if(scalarParams.size()!=1);
					{
						scalarParams.clear();
						scalarParams.push_back(10.0f);
					}
					break;
				case IONCLIP_PRIMITIVE_PLANE:
					if(vectorParams.size() >2)
					{
						vectorParams.clear();
						vectorParams.push_back(Point3D(0,0,0));
						vectorParams.push_back(Point3D(0,1,0));
					}
					else if (vectorParams.size() ==2)
					{
						vectorParams[1].normalise();
					}
					else if(vectorParams.size() ==1)
					{
						vectorParams.push_back(Point3D(0,1,0));
					}

					scalarParams.clear();
					break;
				case IONCLIP_PRIMITIVE_CYLINDER:
					if(vectorParams.size()>2)
					{
						vectorParams.resize(2);
					}
					else if(vectorParams.size() ==1)
					{
						vectorParams.push_back(Point3D(0,1,0));
					}
					else if(!vectorParams.size())
					{
						vectorParams.push_back(Point3D(0,0,0));
						vectorParams.push_back(Point3D(0,1,0));

					}

					if(scalarParams.size()!=1);
					{
						scalarParams.clear();
						scalarParams.push_back(10.0f);
					}
					break;
				case IONCLIP_PRIMITIVE_AAB:
				
					if(vectorParams.size() >2)
					{
						vectorParams.clear();
						vectorParams.push_back(Point3D(0,0,0));
						vectorParams.push_back(Point3D(1,1,1));
					}
					else if(vectorParams.size() ==1)
					{
						vectorParams.push_back(Point3D(1,1,1));
					}
					//check to see if any components of the
					//corner offset are zero; we disallow a
					//zero, 1 or 2 dimensional box
					for(unsigned int ui=0;ui<3;ui++)
					{
						vectorParams[1][ui]=fabs(vectorParams[1][ui]);
						if(vectorParams[1][ui] <std::numeric_limits<float>::epsilon())
							vectorParams[1][ui] = 1;
					}
					scalarParams.clear();
					break;

				default:
					ASSERT(false);
			}
	
			clearCache();	
			needUpdate=true;	
			return true;	
		}
		case KEY_IONCLIP_ORIGIN:
		{
			ASSERT(vectorParams.size() >= 1);
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
		case KEY_IONCLIP_CORNER:
		{
			ASSERT(vectorParams.size() >= 2);
			Point3D newPt;
			if(!parsePointStr(value,newPt))
				return false;

			if(!(vectorParams[1] == newPt ))
			{
				vectorParams[1] = newPt;
				needUpdate=true;
				clearCache();
			}

			return true;
		}
		case KEY_IONCLIP_RADIUS:
		{
			ASSERT(scalarParams.size() >=1);
			float newRad;
			if(stream_cast(newRad,value))
				return false;

			if(scalarParams[0] != newRad )
			{
				scalarParams[0] = newRad;
				needUpdate=true;
				clearCache();
			}
			return true;
		}
		case KEY_IONCLIP_NORMAL:
		{
			ASSERT(vectorParams.size() >=2);
			Point3D newPt;
			if(!parsePointStr(value,newPt))
				return false;

			if(!(vectorParams[1] == newPt ))
			{
				vectorParams[1] = newPt;
				needUpdate=true;
				clearCache();
			}
			return true;
		}
		case KEY_IONCLIP_PRIMITIVE_SHOW:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			if(stripped=="1")
				showPrimitive=true;
			else
				showPrimitive=false;

			needUpdate=true;

			break;
		}
		case KEY_IONCLIP_PRIMITIVE_INVERTCLIP:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			bool lastVal=invertedClip;
			if(stripped=="1")
				invertedClip=true;
			else
				invertedClip=false;

			//if the result is different, the
			//cache should be invalidated
			if(lastVal!=invertedClip)
			{
				needUpdate=true;
				clearCache();
			}

			break;
		}

		case KEY_IONCLIP_AXIS_LOCKMAG:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			if(stripped=="1")
				lockAxisMag=true;
			else
				lockAxisMag=false;

			needUpdate=true;

			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	ASSERT(vectorParams.size() || scalarParams.size());	

	return true;
}

//!Get the human readable error string associated with a particular error code during refresh(...)
std::string IonClipFilter::getErrString(unsigned int code) const
{
	switch(code)
	{
		case IONCLIP_BAD_ALLOC:
			return std::string("Insufficient mem. for Ionclip");
		case IONCLIP_CALLBACK_FAIL:
			return std::string("Ionclip Aborted");
	}
	ASSERT(false);
}

bool IonClipFilter::writeState(std::ofstream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<"<< trueName() <<  ">" << endl;
			f << tabs(depth+1) << "<userstring value=\""<<userString << "\"/>"  << endl;

			f << tabs(depth+1) << "<primitivetype value=\"" << primitiveType<< "\"/>" << endl;
			f << tabs(depth+1) << "<invertedclip value=\"" << invertedClip << "\"/>" << endl;
			f << tabs(depth+1) << "<showprimitive value=\"" << showPrimitive << "\"/>" << endl;
			f << tabs(depth+1) << "<lockaxismag value=\"" << lockAxisMag<< "\"/>" << endl;
			f << tabs(depth+1) << "<vectorparams>" << endl;
			for(unsigned int ui=0; ui<vectorParams.size(); ui++)
			{
				f << tabs(depth+2) << "<point3d x=\"" << vectorParams[ui][0] << 
					"\" y=\"" << vectorParams[ui][1] << "\" z=\"" << vectorParams[ui][2] << "\"/>" << endl;
			}
			f << tabs(depth+1) << "</vectorparams>" << endl;

			f << tabs(depth+1) << "<scalarparams>" << endl;
			for(unsigned int ui=0; ui<scalarParams.size(); ui++)
				f << tabs(depth+2) << "<scalar value=\"" << scalarParams[ui] << "\"/>" << endl; 
			
			f << tabs(depth+1) << "</scalarparams>" << endl;
			f << tabs(depth) << "</ionclip>" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool IonClipFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
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
	//Retrieve primitive type 
	//====
	if(!XMLGetNextElemAttrib(nodePtr,primitiveType,"primitivetype","value"))
		return false;
	if(primitiveType >= IONCLIP_PRIMITIVE_END)
	       return false;	
	//====
	
	std::string tmpString;
	//Retrieve clip inversion
	//====
	//
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"invertedclip","value"))
		return false;
	if(tmpStr == "0")
		invertedClip=false;
	else if(tmpStr == "1")
		invertedClip=true;
	else
		return false;
	//====
	
	//Retrieve primitive visiblity 
	//====
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"showprimitive","value"))
		return false;
	if(tmpStr == "0")
		showPrimitive=false;
	else if(tmpStr == "1")
		showPrimitive=true;
	else
		return false;
	//====
	
	//Retrieve axis lock mode 
	//====
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"lockaxismag","value"))
		return false;
	if(tmpStr == "0")
		lockAxisMag=false;
	else if(tmpStr == "1")
		lockAxisMag=true;
	else
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
	switch(primitiveType)
	{
		case IONCLIP_PRIMITIVE_SPHERE:
			if(vectorParams.size() != 1 || scalarParams.size() !=1)
				return false;
			break;
		case IONCLIP_PRIMITIVE_PLANE:
		case IONCLIP_PRIMITIVE_AAB:
			if(vectorParams.size() != 2 || scalarParams.size() !=0)
				return false;
			break;
		case IONCLIP_PRIMITIVE_CYLINDER:
			if(vectorParams.size() != 2 || scalarParams.size() !=1)
				return false;
			break;

		default:
			ASSERT(false);
			return false;
	}

	ASSERT(vectorParams.size() || scalarParams.size());	
	return true;
}

void IonClipFilter::setPropFromBinding(const SelectionBinding &b)
{
	switch(b.getID())
	{
		case BINDING_CYLINDER_RADIUS:
		case BINDING_SPHERE_RADIUS:
			b.getValue(scalarParams[0]);
			break;
		case BINDING_CYLINDER_ORIGIN:
		case BINDING_SPHERE_ORIGIN:
		case BINDING_PLANE_ORIGIN:
		case BINDING_RECT_TRANSLATE:
			b.getValue(vectorParams[0]);
			break;
		case BINDING_CYLINDER_DIRECTION:
			b.getValue(vectorParams[1]);
			break;
		case BINDING_PLANE_DIRECTION:
		{
			Point3D p;
			b.getValue(p);
			p.normalise();

			vectorParams[1] =p;
			break;
		}
		case BINDING_RECT_CORNER_MOVE:
		{
			//Prevent the corner offset from acquiring a vector
			//with a negative component.
			Point3D p;
			b.getValue(p);
			for(unsigned int ui=0;ui<3;ui++)
			{
				p[ui]=fabs(p[ui]);
				//Should be positive
				if(p[ui] <std::numeric_limits<float>::epsilon())
					return;
			}

			vectorParams[1]=p;
			break;
		}
		default:
			ASSERT(false);
	}
	clearCache();
}
