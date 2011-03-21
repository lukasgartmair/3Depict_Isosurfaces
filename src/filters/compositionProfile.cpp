#include "compositionProfile.h"
#include "../xmlHelper.h"
#include "../plot.h"

enum
{
	KEY_COMPPROFILE_BINWIDTH=1,
	KEY_COMPPROFILE_FIXEDBINS,
	KEY_COMPPROFILE_NORMAL,
	KEY_COMPPROFILE_NUMBINS,
	KEY_COMPPROFILE_ORIGIN,
	KEY_COMPPROFILE_PLOTTYPE,
	KEY_COMPPROFILE_PRIMITIVETYPE,
	KEY_COMPPROFILE_RADIUS,
	KEY_COMPPROFILE_SHOWPRIMITIVE,
	KEY_COMPPROFILE_NORMALISE,
	KEY_COMPPROFILE_COLOUR,
	KEY_COMPPROFILE_ERRMODE,
	KEY_COMPPROFILE_AVGWINSIZE,
	KEY_COMPPROFILE_LOCKAXISMAG,
};

//!Possible primitive types for composition profiles
enum
{
	COMPPROFILE_PRIMITIVE_CYLINDER,
	COMPPROFILE_PRIMITIVE_END, //Not actually a primitive, just end of enum
};

//!Error codes
enum
{
	COMPPROFILE_ERR_NUMBINS=1,
	COMPPROFILE_ERR_MEMALLOC,
	COMPPROFILE_ERR_ABORT,
};

CompositionProfileFilter::CompositionProfileFilter()
{
	binWidth=0.5;
	plotType=0;
	fixedBins=0;
	nBins=1000;
	normalise=1;
	errMode.mode=PLOT_ERROR_NONE;
	errMode.movingAverageNum=4;
	lockAxisMag=false;
	//Default to blue plot
	r=g=0;
	b=a=1;
	
	primitiveType=COMPPROFILE_PRIMITIVE_CYLINDER;
	vectorParams.push_back(Point3D(0.0,0.0,0.0));
	vectorParams.push_back(Point3D(0,20.0,0.0));
	scalarParams.push_back(5.0);

	showPrimitive=true;
}


void CompositionProfileFilter::binIon(unsigned int targetBin, const RangeStreamData* rng, 
	const map<unsigned int,unsigned int> &ionIDMapping,
	vector<vector<size_t> > &frequencyTable, float massToCharge) const
{
	//if we have no range data, then simply increment its position in a 1D table
	//which will later be used as "count" data (like some kind of density plot)
	if(!rng)
	{
		ASSERT(frequencyTable.size() == 1);
		//There is a really annoying numerical boundary case
		//that makes the target bin equate to the table size. 
		//disallow this.
		if(targetBin < frequencyTable[0].size())
			frequencyTable[0][targetBin]++;
		return;
	}


	//We have range data, we need to use it to classify the ion and then increment
	//the appropriate position in the table
	unsigned int rangeID = rng->rangeFile->getRangeID(massToCharge);

	if(rangeID != (unsigned int)(-1) && rng->enabledRanges[rangeID])
	{
		unsigned int ionID=rng->rangeFile->getIonID(rangeID); 
		unsigned int pos;
		pos = ionIDMapping.find(ionID)->second;
		frequencyTable[pos][targetBin]++;
	}
}


Filter *CompositionProfileFilter::cloneUncached() const
{
	CompositionProfileFilter *p = new CompositionProfileFilter();

	p->primitiveType=primitiveType;
	p->showPrimitive=showPrimitive;
	p->vectorParams.resize(vectorParams.size());
	p->scalarParams.resize(scalarParams.size());

	std::copy(vectorParams.begin(),vectorParams.end(),p->vectorParams.begin());
	std::copy(scalarParams.begin(),scalarParams.end(),p->scalarParams.begin());

	p->normalise=normalise;	
	p->stepMode=stepMode;	
	p->fixedBins=fixedBins;
	p->lockAxisMag=lockAxisMag;

	p->binWidth=binWidth;
	p->nBins = nBins;
	p->r=r;	
	p->g=g;	
	p->b=b;	
	p->a=a;	
	p->plotType=plotType;
	p->errMode=errMode;
	//We are copying wether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

void CompositionProfileFilter::initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut)
{
	//Check for range file parent
	for(unsigned int ui=0;ui<dataIn.size();ui++)
	{
		if(dataIn[ui]->getStreamType() == STREAM_TYPE_RANGE)
		{
			haveRangeParent=true;
			return;
		}
	}
	haveRangeParent=false;
}

unsigned int CompositionProfileFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
			std::vector<const FilterStreamData *> &getOut, ProgressData &progress, 
								bool (*callback)(void))
{
	//Clear selection devices
	devices.clear();
	
	if(showPrimitive)
	{
		//construct a new primitive, do not cache
		DrawStreamData *drawData=new DrawStreamData;
		switch(primitiveType)
		{
			case COMPPROFILE_PRIMITIVE_CYLINDER:
			{
				//Origin + normal
				ASSERT(vectorParams.size() == 2);
				//Add drawable components
				DrawCylinder *dC = new DrawCylinder;
				dC->setOrigin(vectorParams[0]);
				dC->setRadius(scalarParams[0]);
				dC->setColour(0.5,0.5,0.5,0.3);
				dC->setSlices(40);
				dC->setLength(sqrt(vectorParams[1].sqrMag())*2.0f);
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
			default:
				ASSERT(false);
		}
		drawData->cached=0;	
		getOut.push_back(drawData);
	}


	//use the cached copy of the data if we have it.
	if(cacheOK)
	{
		//proagate our cached plot data.
		for(unsigned int ui=0;ui<filterOutputs.size();ui++)
			getOut.push_back(filterOutputs[ui]);

		ASSERT(filterOutputs.back()->getStreamType() == STREAM_TYPE_PLOT);

		//Propagate all the incoming data (including ions)
		for(unsigned int ui=0;ui<dataIn.size() ;ui++)
		{
			if(dataIn[ui]->getStreamType() != STREAM_TYPE_IONS)
				getOut.push_back(dataIn[ui]);
		}
			
		return 0;
	}

	//Ion Frequences (composition specific if rangefile present)
	vector<vector<size_t> > ionFrequencies;
	
	RangeStreamData *rngData=0;
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		if(dataIn[ui]->getStreamType() == STREAM_TYPE_RANGE)
		{
			rngData =((RangeStreamData *)dataIn[ui]);
			break;
		}
	}

	//Number of bins, having determined if we are using
	//fixed bin count or not
	unsigned int numBins;

	if(fixedBins)
		numBins=nBins;
	else
	{
		switch(primitiveType)
		{
			case COMPPROFILE_PRIMITIVE_CYLINDER:
			{
				//length of cylinder (as axis starts in cylinder middle)
				float length;
				length=sqrt(vectorParams[1].sqrMag())*2.0f;

				ASSERT(binWidth > std::numeric_limits<float>::epsilon());

				//Check for possible overflow
				if(length/binWidth > (float)std::numeric_limits<unsigned int>::max())
					return COMPPROFILE_ERR_NUMBINS;

				numBins=(unsigned int)(length/binWidth);
				break;
			}
			default:
				ASSERT(false);
		}
		
	}

	//Indirection vector to convert ionFrequencies position to ionID mapping.
	//Should only be used in conjunction with rngData == true
	std::map<unsigned int,unsigned int> ionIDMapping,inverseIDMapping;
	//Allocate space for the frequency table
	if(rngData)
	{
		ASSERT(rngData->rangeFile);
		unsigned int enabledCount=0;
		for(unsigned int ui=0;ui<rngData->rangeFile->getNumIons();ui++)
		{
			//TODO: Might be nice to detect if an ions ranges
			//are all, disabled then if they are, enter this "if"
			//anyway
			if(rngData->enabledIons[ui])
			{
				//Keep the forwards mapping for binning
				ionIDMapping.insert(make_pair(ui,enabledCount));
				//Keep the inverse mapping for labelling
				inverseIDMapping.insert(make_pair(enabledCount,ui));
				enabledCount++;
			}

		

		}

		//Nothing to do.
		if(!enabledCount)
			return 0;

		try
		{
			ionFrequencies.resize(enabledCount);
			//Allocate and Initialise all elements to zero
			#pragma omp parallel for
			for(unsigned int ui=0;ui<ionFrequencies.size(); ui++)
				ionFrequencies[ui].resize(numBins,0);
		}
		catch(std::bad_alloc)
		{
			return COMPPROFILE_ERR_MEMALLOC;
		}

	}
	else
	{
		try
		{
			ionFrequencies.resize(1);
			ionFrequencies[0].resize(numBins,0);
		}
		catch(std::bad_alloc)
		{
			return COMPPROFILE_ERR_MEMALLOC;
		}
	}


	size_t n=0;
	size_t totalSize=numElements(dataIn);
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		//Loop through each data set
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS:
			{

				switch(primitiveType)
				{
					case COMPPROFILE_PRIMITIVE_CYLINDER:
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
								if((p.fz < halfLen && p.fz > -halfLen && p.fx*p.fx+p.fy*p.fy < sqrRad))  
								{
									//Figure out where inside the cylinder the 
									//data lies. Then push it into the correct bin.
									unsigned int targetBin;
									targetBin =  (unsigned int)((float)numBins*(float)(p.fz + halfLen)/(2.0f*halfLen));
								
									binIon(targetBin,rngData,ionIDMapping,ionFrequencies,
											it->getMassToCharge());
								}

								//update progress every CALLBACK ions
								if(!curProg--)
								{
									n+=NUM_CALLBACK;
									progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
									curProg=NUM_CALLBACK;
									if(!(*callback)())
										return COMPPROFILE_ERR_ABORT;
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
								if((ptmp[2] < halfLen && ptmp[2] > -halfLen && ptmp[0]*ptmp[0]+ptmp[1]*ptmp[1] < sqrRad))
								{
									//Figure out where inside the cylinder the 
									//data lies. Then push it into the correct bin.
									unsigned int targetBin;
									targetBin =  (unsigned int)((float)numBins*(float)(ptmp[2]+ halfLen)/(2.0f*halfLen));								
									binIon(targetBin,rngData,ionIDMapping,ionFrequencies,
											it->getMassToCharge());
								}

								//update progress every CALLBACK ions
								if(!curProg--)
								{
									n+=NUM_CALLBACK;
									progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
									curProg=NUM_CALLBACK;
									if(!(*callback)())
										return COMPPROFILE_ERR_ABORT;
								}
							}
							
						}
						break;
					}
				}
				break;
			}
			default:
				//Do not propagate other types.
				break;
		}
				
	}

	PlotStreamData *plotData[ionFrequencies.size()];
	float length;
	length=sqrt(vectorParams[1].sqrMag())*2.0f;


	float normFactor=1.0f;
	for(unsigned int ui=0;ui<ionFrequencies.size();ui++)
	{
		plotData[ui] = new PlotStreamData;

		plotData[ui]->index=ui;
		plotData[ui]->parent=this;
		plotData[ui]->xLabel= "Distance";
		plotData[ui]->errDat=errMode;
		if(normalise)
		{
			//If we have composition, normalise against 
			//sum composition = 1 otherwise use volume of bin
			//as normalisation factor
			if(rngData)
				plotData[ui]->yLabel= "Fraction";
			else
				plotData[ui]->yLabel= "Density (\\#.len^3)";
		}
		else
			plotData[ui]->yLabel= "Count";

		//Give the plot a title like "Myplot:Mg" (if have range) or "MyPlot" (no range)
		if(rngData)
		{
			unsigned int thisIonID;
			thisIonID = inverseIDMapping.find(ui)->second;
			plotData[ui]->dataLabel = getUserString() + string(":") 
					+ rngData->rangeFile->getName(thisIonID);

		
			//Set the plot colour to the ion colour	
			RGBf col;
			col=rngData->rangeFile->getColour(thisIonID);

			plotData[ui]->r =col.red;
			plotData[ui]->g =col.green;
			plotData[ui]->b =col.blue;

		}
		else
		{
			//If it only has one component, then 
			//it's not really a composition profile is it?
			plotData[ui]->dataLabel= "Freq. Profile";
			plotData[ui]->r = r;
			plotData[ui]->g = g;
			plotData[ui]->b = b;
			plotData[ui]->a = a;
		}

		plotData[ui]->xyData.resize(ionFrequencies[ui].size());
		for(unsigned int uj=0;uj<ionFrequencies[ui].size(); uj++)
		{
			float xPos;
			xPos = ((float)uj/(float)ionFrequencies[ui].size())*length;
			if(rngData)
			{
				//Composition profile: do inter bin normalisation
				//Recompute normalisation value for this bin
				if(normalise)
				{
					float sum;
					sum=0;

					//Loop across each bin type, summing result
					for(unsigned int uk=0;uk<ionFrequencies.size();uk++)
						sum +=(float)ionFrequencies[uk][uj];

					if(sum)
						normFactor=1.0/sum;
				}

				plotData[ui]->xyData[uj] = std::make_pair(xPos,normFactor*(float)ionFrequencies[ui][uj]);
			}
			else
			{
				if(normalise)
				{
					//This is a frequency profile. Normalising across bins would lead to every value
					//being equal to 1. Let us instead normalise by cylinder section volume)
					normFactor = 1.0/(M_PI*scalarParams[0]*scalarParams[0]*binWidth);


				}
				//Normalise Y value against volume of bin
				plotData[ui]->xyData[uj] = std::make_pair(
					xPos,normFactor*(float)ionFrequencies[ui][uj]);

			}
		}

		if(cache)
		{
			plotData[ui]->cached=1;
			filterOutputs.push_back(plotData[ui]);	
		}
		else
			plotData[ui]->cached=0;

		plotData[ui]->plotType = plotType;
		getOut.push_back(plotData[ui]);
	}

	cacheOK=cache;
	return 0;
}

std::string  CompositionProfileFilter::getErrString(unsigned int code) const
{
	switch(code)
	{
		case COMPPROFILE_ERR_NUMBINS:
			return std::string("Too many bins in comp. profile.");
		case COMPPROFILE_ERR_MEMALLOC:
			return std::string("Note enough memory for comp. profile.");
		case COMPPROFILE_ERR_ABORT:
			return std::string("Aborted composition prof.");
	}
	return std::string("BUG: (CompositionProfileFilter::getErrString) Shouldn't see this!");
}

bool CompositionProfileFilter::setProperty(unsigned int set, unsigned int key, 
					const std::string &value, bool &needUpdate) 
{

			
	switch(key)
	{
		case KEY_COMPPROFILE_BINWIDTH:
		{
			float newBinWidth;
			if(stream_cast(newBinWidth,value))
				return false;

			if(newBinWidth < sqrt(std::numeric_limits<float>::epsilon()))
				return false;

			binWidth=newBinWidth;
			clearCache();
			needUpdate=true;
			break;
		}
		case KEY_COMPPROFILE_FIXEDBINS:
		{
			unsigned int valueInt;
			if(stream_cast(valueInt,value))
				return false;

			if(valueInt ==0 || valueInt == 1)
			{
				if(fixedBins!= valueInt)
				{
					needUpdate=true;
					fixedBins=valueInt;
				}
				else
					needUpdate=false;
			}
			else
				return false;
			clearCache();
			needUpdate=true;	
			break;	
		}
		case KEY_COMPPROFILE_NORMAL:
		{
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
		case KEY_COMPPROFILE_NUMBINS:
		{
			unsigned int newNumBins;
			if(stream_cast(newNumBins,value))
				return false;

			nBins=newNumBins;

			clearCache();
			needUpdate=true;
			break;
		}
		case KEY_COMPPROFILE_ORIGIN:
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
		case KEY_COMPPROFILE_PRIMITIVETYPE:
		{
			unsigned int newPrimitive;
			if(stream_cast(newPrimitive,value) ||
					newPrimitive >= COMPPROFILE_PRIMITIVE_END)
				return false;
	

			//TODO: Convert the type data as best we can.
			primitiveType=newPrimitive;

			//In leiu of covnersion, just reset the primitive
			//values to some nominal defaults.
			vectorParams.clear();
			scalarParams.clear();
			switch(primitiveType)
			{
				case IONCLIP_PRIMITIVE_CYLINDER:
					vectorParams.push_back(Point3D(0,0,0));
					vectorParams.push_back(Point3D(0,20,0));
					scalarParams.push_back(10.0f);
					break;

				default:
					ASSERT(false);
			}
	
			clearCache();	
			needUpdate=true;	
			return true;	
		}
		case KEY_COMPPROFILE_RADIUS:
		{
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
		case KEY_COMPPROFILE_SHOWPRIMITIVE:
		{
			unsigned int valueInt;
			if(stream_cast(valueInt,value))
				return false;

			if(valueInt ==0 || valueInt == 1)
			{
				if(showPrimitive!= valueInt)
				{
					needUpdate=true;
					showPrimitive=valueInt;
				}
				else
					needUpdate=false;
			}
			else
				return false;		
			break;	
		}

		case KEY_COMPPROFILE_NORMALISE:
		{
			unsigned int valueInt;
			if(stream_cast(valueInt,value))
				return false;

			if(valueInt ==0 || valueInt == 1)
			{
				if(normalise!= valueInt)
				{
					needUpdate=true;
					normalise=valueInt;
				}
				else
					needUpdate=false;
			}
			else
				return false;
			clearCache();
			needUpdate=true;	
			break;	
		}
		case KEY_COMPPROFILE_LOCKAXISMAG:
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

		case KEY_COMPPROFILE_PLOTTYPE:
		{
			unsigned int tmpPlotType;

			tmpPlotType=plotID(value);

			if(tmpPlotType >= PLOT_TYPE_ENDOFENUM)
				return false;

			plotType = tmpPlotType;
			needUpdate=true;	
			break;
		}
		case KEY_COMPPROFILE_COLOUR:
		{
			unsigned char newR,newG,newB,newA;
			parseColString(value,newR,newG,newB,newA);

			r=((float)newR)/255.0f;
			g=((float)newG)/255.0f;
			b=((float)newB)/255.0f;
			a=1.0;

			needUpdate=true;
			break;	
		}
		case KEY_COMPPROFILE_ERRMODE:
		{
			unsigned int tmpMode;
			tmpMode=plotErrmodeID(value);

			if(tmpMode >= PLOT_ERROR_ENDOFENUM)
				return false;

			errMode.mode= tmpMode;
			needUpdate=true;

			break;
		}
		case KEY_COMPPROFILE_AVGWINSIZE:
		{
			unsigned int tmpNum;
			stream_cast(tmpNum,value);
			if(tmpNum<=1)
				return 1;

			errMode.movingAverageNum=tmpNum;
			needUpdate=true;
			break;
		}
		default:
			ASSERT(false);	
	}

	if(needUpdate)
		clearCache();

	return true;
}

void CompositionProfileFilter::getProperties(FilterProperties &propertyList) const
{
	propertyList.data.clear();
	propertyList.keys.clear();
	propertyList.types.clear();

	vector<unsigned int> type,keys;
	vector<pair<string,string> > s;

	string str,tmpStr;

	//Allow primitive selection if we have more than one primitive
	if(COMPPROFILE_PRIMITIVE_END > 1)
	{
		stream_cast(str,(int)COMPPROFILE_PRIMITIVE_END-1);
		str =string("Primitive Type (0-") + str + ")";
		stream_cast(tmpStr,primitiveType);
		s.push_back(make_pair(str,tmpStr));
		keys.push_back(KEY_COMPPROFILE_PRIMITIVETYPE);
		type.push_back(PROPERTY_TYPE_INTEGER);
	}

	str = "Show primitive";	
	stream_cast(tmpStr,showPrimitive);
	s.push_back(make_pair(str,tmpStr));
	keys.push_back(KEY_COMPPROFILE_SHOWPRIMITIVE);
	type.push_back(PROPERTY_TYPE_BOOL);

	switch(primitiveType)
	{
		case COMPPROFILE_PRIMITIVE_CYLINDER:
		{
			ASSERT(vectorParams.size() == 2);
			ASSERT(scalarParams.size() == 1);
			stream_cast(str,vectorParams[0]);
			keys.push_back(KEY_COMPPROFILE_ORIGIN);
			s.push_back(make_pair("Origin", str));
			type.push_back(PROPERTY_TYPE_POINT3D);
			
			stream_cast(str,vectorParams[1]);
			keys.push_back(KEY_COMPPROFILE_NORMAL);
			s.push_back(make_pair("Axis", str));
			type.push_back(PROPERTY_TYPE_POINT3D);

			if(lockAxisMag)
				str="1";
			else
				str="0";
			keys.push_back(KEY_COMPPROFILE_LOCKAXISMAG);
			s.push_back(make_pair("Lock Axis Mag.", str));
			type.push_back(PROPERTY_TYPE_BOOL);
			
			stream_cast(str,scalarParams[0]);
			keys.push_back(KEY_COMPPROFILE_RADIUS);
			s.push_back(make_pair("Radius", str));
			type.push_back(PROPERTY_TYPE_POINT3D);



			break;
		}
	}

	keys.push_back(KEY_COMPPROFILE_FIXEDBINS);
	stream_cast(str,fixedBins);
	s.push_back(make_pair("Fixed Bin Num", str));
	type.push_back(PROPERTY_TYPE_BOOL);

	if(fixedBins)
	{
		stream_cast(tmpStr,nBins);
		str = "Num Bins";
		s.push_back(make_pair(str,tmpStr));
		keys.push_back(KEY_COMPPROFILE_NUMBINS);
		type.push_back(PROPERTY_TYPE_INTEGER);
	}
	else
	{
		str = "Bin width";
		stream_cast(tmpStr,binWidth);
		s.push_back(make_pair(str,tmpStr));
		keys.push_back(KEY_COMPPROFILE_BINWIDTH);
		type.push_back(PROPERTY_TYPE_REAL);
	}

	str = "Normalise";	
	stream_cast(tmpStr,normalise);
	s.push_back(make_pair(str,tmpStr));
	keys.push_back(KEY_COMPPROFILE_NORMALISE);
	type.push_back(PROPERTY_TYPE_BOOL);


	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);

	s.clear();
	type.clear();
	keys.clear();
	
	//use set 2 to store the plot properties
	stream_cast(str,plotType);
	//Let the user know what the valid values for plot type are
	string tmpChoice;
	vector<pair<unsigned int,string> > choices;


	tmpStr=plotString(PLOT_TYPE_LINES);
	choices.push_back(make_pair((unsigned int) PLOT_TYPE_LINES,tmpStr));
	tmpStr=plotString(PLOT_TYPE_BARS);
	choices.push_back(make_pair((unsigned int)PLOT_TYPE_BARS,tmpStr));
	tmpStr=plotString(PLOT_TYPE_STEPS);
	choices.push_back(make_pair((unsigned int)PLOT_TYPE_STEPS,tmpStr));
	tmpStr=plotString(PLOT_TYPE_STEM);
	choices.push_back(make_pair((unsigned int)PLOT_TYPE_STEM,tmpStr));

	tmpStr= choiceString(choices,plotType);
	s.push_back(make_pair(string("Plot Type"),tmpStr));
	type.push_back(PROPERTY_TYPE_CHOICE);
	keys.push_back(KEY_COMPPROFILE_PLOTTYPE);
	//Convert the colour to a hex string
	if(!haveRangeParent)
	{
		string thisCol;
		genColString((unsigned char)(r*255.0),(unsigned char)(g*255.0),
		(unsigned char)(b*255.0),(unsigned char)(a*255.0),thisCol);

		s.push_back(make_pair(string("Colour"),thisCol)); 
		type.push_back(PROPERTY_TYPE_COLOUR);
		keys.push_back(KEY_COMPPROFILE_COLOUR);
	}

	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);
	
	s.clear();
	type.clear();
	keys.clear();

	
	choices.clear();
	tmpStr=plotErrmodeString(PLOT_ERROR_NONE);
	choices.push_back(make_pair((unsigned int) PLOT_ERROR_NONE,tmpStr));
	tmpStr=plotErrmodeString(PLOT_ERROR_MOVING_AVERAGE);
	choices.push_back(make_pair((unsigned int) PLOT_ERROR_MOVING_AVERAGE,tmpStr));

	tmpStr= choiceString(choices,errMode.mode);
	s.push_back(make_pair(string("Err. Estimator"),tmpStr));
	type.push_back(PROPERTY_TYPE_CHOICE);
	keys.push_back(KEY_COMPPROFILE_ERRMODE);


	if(errMode.mode == PLOT_ERROR_MOVING_AVERAGE)
	{
		stream_cast(tmpStr,errMode.movingAverageNum);
		s.push_back(make_pair(string("Avg. Window"), tmpStr));
		type.push_back(PROPERTY_TYPE_INTEGER);
		keys.push_back(KEY_COMPPROFILE_AVGWINSIZE);

	}	

	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);
}

//!Get approx number of bytes for caching output
size_t CompositionProfileFilter::numBytesForCache(size_t nObjects) const
{
	//FIXME: IMPLEMEMENT ME
	return (unsigned int)(-1);
}

bool CompositionProfileFilter::writeState(std::ofstream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<" << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\""<<userString << "\"/>"  << endl;

			f << tabs(depth+1) << "<primitivetype value=\"" << primitiveType<< "\"/>" << endl;
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
				f << tabs(depth+2) << "<scalar value=\"" << scalarParams[0] << "\"/>" << endl; 
			
			f << tabs(depth+1) << "</scalarparams>" << endl;
			f << tabs(depth+1) << "<normalise value=\"" << normalise << "\"/>" << endl;
			f << tabs(depth+1) << "<stepmode value=\"" << stepMode << "\"/>" << endl;
			f << tabs(depth+1) << "<fixedbins value=\"" << (int)fixedBins << "\"/>" << endl;
			f << tabs(depth+1) << "<nbins value=\"" << nBins << "\"/>" << endl;
			f << tabs(depth+1) << "<binwidth value=\"" << binWidth << "\"/>" << endl;
			f << tabs(depth+1) << "<colour r=\"" <<  r<< "\" g=\"" << g << "\" b=\"" <<b
				<< "\" a=\"" << a << "\"/>" <<endl;

			f << tabs(depth+1) << "<plottype value=\"" << plotType << "\"/>" << endl;
			f << tabs(depth) << "</" << trueName()  << " >" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool CompositionProfileFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
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
	if(XMLHelpFwdToElem(nodePtr,"primitivetype"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(primitiveType,tmpStr))
		return false;

	if(primitiveType >= COMPPROFILE_PRIMITIVE_END)
	       return false;	
	xmlFree(xmlString);
	//====
	
	//Retrieve primitive visiblity 
	//====
	if(XMLHelpFwdToElem(nodePtr,"showprimitive"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(tmpStr == "0")
		showPrimitive=false;
	else if(tmpStr == "1")
		showPrimitive=true;
	else
		return false;

	xmlFree(xmlString);
	//====
	
	//Retrieve axis lock mode 
	//====
	if(XMLHelpFwdToElem(nodePtr,"lockaxismag"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(tmpStr == "0")
		lockAxisMag=false;
	else if(tmpStr == "1")
		lockAxisMag=true;
	else
		return false;

	xmlFree(xmlString);
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
		case COMPPROFILE_PRIMITIVE_CYLINDER:
			if(vectorParams.size() != 2 || scalarParams.size() !=1)
				return false;
			break;
		default:
			ASSERT(false);
			return false;
	}

	nodePtr=tmpNode;

	//Retrieve normalisation on/off 
	//====
	if(XMLHelpFwdToElem(nodePtr,"normalise"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(tmpStr == "0")
		normalise=false;
	else if(tmpStr == "1")
		normalise=true;
	else
		return false;

	xmlFree(xmlString);
	//====

	//Retrieve step mode 
	//====
	if(XMLHelpFwdToElem(nodePtr,"stepmode"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(stepMode,tmpStr))
		return false;

	xmlFree(xmlString);
	//====


	//Retrieve fixed bins on/off 
	//====
	if(XMLHelpFwdToElem(nodePtr,"fixedbins"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(tmpStr == "0")
		fixedBins=false;
	else if(tmpStr == "1")
		fixedBins=true;
	else
		return false;


	xmlFree(xmlString);
	//====

	//Retrieve num bins
	//====
	if(XMLHelpFwdToElem(nodePtr,"nbins"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(nBins,tmpStr))
		return false;

	xmlFree(xmlString);
	//====

	//Retrieve bin width
	//====
	if(XMLHelpFwdToElem(nodePtr,"binwidth"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(binWidth,tmpStr))
		return false;

	xmlFree(xmlString);
	//====

	//Retrieve colour
	//====
	if(XMLHelpFwdToElem(nodePtr,"colour"))
		return false;
	if(!parseXMLColour(nodePtr,r,g,b,a))
		return false;
	//====
	
	//Retrieve plot type 
	//====
	if(XMLHelpFwdToElem(nodePtr,"plottype"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(plotType,tmpStr))
		return false;

	if(plotType >= PLOT_TYPE_ENDOFENUM)
	       return false;	
	xmlFree(xmlString);
	//====

	return true;
}

void CompositionProfileFilter::setPropFromBinding(const SelectionBinding &b)
{
	switch(b.getID())
	{
		case BINDING_CYLINDER_RADIUS:
			b.getValue(scalarParams[0]);
			break;

		case BINDING_CYLINDER_DIRECTION:
			b.getValue(vectorParams[1]);
			break;

		case BINDING_CYLINDER_ORIGIN:
			b.getValue(vectorParams[0]);
			break;
		default:
			ASSERT(false);
	}

	clearCache();
}
