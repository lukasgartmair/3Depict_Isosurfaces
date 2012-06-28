#include "compositionProfile.h"
#include "../xmlHelper.h"
#include "../plot.h"

#include "../translation.h"


//!Possible primitive types for composition profiles
enum
{
	PRIMITIVE_CYLINDER,
	PRIMITIVE_END, //Not actually a primitive, just end of enum
};

//!Error codes
enum
{
	ERR_NUMBINS=1,
	ERR_MEMALLOC,
	ERR_ABORT
};

CompositionProfileFilter::CompositionProfileFilter() : primitiveType(PRIMITIVE_CYLINDER),
	showPrimitive(true), lockAxisMag(false),normalise(true), fixedBins(0),
	nBins(1000), binWidth(0.5f), r(0.0f),g(0.0f),b(1.0f),a(1.0f), plotStyle(0)
{
	errMode.mode=PLOT_ERROR_NONE;
	errMode.movingAverageNum=4;
	
	vectorParams.push_back(Point3D(0.0,0.0,0.0));
	vectorParams.push_back(Point3D(0,20.0,0.0));
	scalarParams.push_back(5.0);

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
	p->fixedBins=fixedBins;
	p->lockAxisMag=lockAxisMag;

	p->binWidth=binWidth;
	p->nBins = nBins;
	p->r=r;	
	p->g=g;	
	p->b=b;	
	p->a=a;	
	p->plotStyle=plotStyle;
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
								bool (*callback)(bool))
{
	//Clear selection devices
	devices.clear();
	
	if(showPrimitive)
	{
		//construct a new primitive, do not cache
		DrawStreamData *drawData=new DrawStreamData;
		drawData->parent=this;
		switch(primitiveType)
		{
			case PRIMITIVE_CYLINDER:
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
			case PRIMITIVE_CYLINDER:
			{
				//length of cylinder (as axis starts in cylinder middle)
				float length;
				length=sqrt(vectorParams[1].sqrMag())*2.0f;

				ASSERT(binWidth > std::numeric_limits<float>::epsilon());

				//Check for possible overflow
				if(length/binWidth > (float)std::numeric_limits<unsigned int>::max())
					return ERR_NUMBINS;

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
			return ERR_MEMALLOC;
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
			return ERR_MEMALLOC;
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
					case PRIMITIVE_CYLINDER:
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

						float halfLen=sqrt(vectorParams[1].sqrMag());
						float sqrRad=scalarParams[0]*scalarParams[0];
				
						//Check that we actually need to rotate, to avoid numerical singularity 
						//when cylinder axis is too close to (or is) z-axis
						if(angle > sqrt(std::numeric_limits<float>::epsilon()))
						{
							if(angle < M_PI-sqrt(std::numeric_limits<float>::epsilon()))
							{
								dir = dir.crossProd(direction);
								dir.normalise();
							}
							else
							{
								//Any old nomral in XY will do, due to rotational symmetry
								dir=Point3D(1,0,0);

							}

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

								//Keep ion if inside cylinder 
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
									if(!(*callback)(false))
										return ERR_ABORT;
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
								
								//Keep ion if inside cylinder 
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
									if(!(*callback)(false))
										return ERR_ABORT;
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
		plotData[ui]->xLabel= TRANS("Distance");
		plotData[ui]->errDat=errMode;
		if(normalise)
		{
			//If we have composition, normalise against 
			//sum composition = 1 otherwise use volume of bin
			//as normalisation factor
			if(rngData)
				plotData[ui]->yLabel= TRANS("Fraction");
			else
				plotData[ui]->yLabel= TRANS("Density (\\#.len^3)");
		}
		else
			plotData[ui]->yLabel= TRANS("Count");

		//Give the plot a title like TRANS("Myplot:Mg" (if have range) or "MyPlot") (no range)
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
			plotData[ui]->dataLabel= TRANS("Freq. Profile");
			plotData[ui]->r = r;
			plotData[ui]->g = g;
			plotData[ui]->b = b;
			plotData[ui]->a = a;
		}

		plotData[ui]->xyData.resize(ionFrequencies[ui].size());
	
		//Density profiles (non-ranged plots) have a fixed normalisation factor
		if(!rngData && normalise)
		{
			if(fixedBins)
				normFactor = 1.0/(M_PI*scalarParams[0]*scalarParams[0]*(length/(float)numBins));
			else
				normFactor = 1.0/(M_PI*scalarParams[0]*scalarParams[0]*binWidth);
		}	

		//Go through each bin, then perform the appropriate normalisation
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
				//This is a frequency profile (factor ==1), or density profile (factor computed above).
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

		plotData[ui]->plotStyle = plotStyle;
		plotData[ui]->plotMode=PLOT_MODE_1D;
		getOut.push_back(plotData[ui]);
	}

	cacheOK=cache;
	return 0;
}

std::string  CompositionProfileFilter::getErrString(unsigned int code) const
{
	switch(code)
	{
		case ERR_NUMBINS:
			return std::string(TRANS("Too many bins in comp. profile."));
		case ERR_MEMALLOC:
			return std::string(TRANS("Not enough memory for comp. profile."));
		case ERR_ABORT:
			return std::string(TRANS("Aborted composition prof."));
	}
	return std::string("BUG: (CompositionProfileFilter::getErrString) Shouldn't see this!");
}

bool CompositionProfileFilter::setProperty(unsigned int set, unsigned int key, 
					const std::string &value, bool &needUpdate) 
{

			
	switch(key)
	{
		case COMPOSITION_KEY_BINWIDTH:
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
		case COMPOSITION_KEY_FIXEDBINS:
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
		case COMPOSITION_KEY_NORMAL:
		{
			Point3D newPt;
			if(!parsePointStr(value,newPt))
				return false;

			if(primitiveType == PRIMITIVE_CYLINDER)
			{
				if(lockAxisMag && 
					newPt.sqrMag() > sqrt(std::numeric_limits<float>::epsilon()))
				{
					newPt.normalise();
					newPt*=sqrt(vectorParams[1].sqrMag());
				}
			}
			
			if(!(vectorParams[1] == newPt ))
			{
				vectorParams[1] = newPt;
				needUpdate=true;
				clearCache();
			}
			return true;
		}
		case COMPOSITION_KEY_NUMBINS:
		{
			unsigned int newNumBins;
			if(stream_cast(newNumBins,value))
				return false;

			//zero bins disallowed
			if(!newNumBins)
				return false;

			nBins=newNumBins;

			clearCache();
			needUpdate=true;
			break;
		}
		case COMPOSITION_KEY_ORIGIN:
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
		case COMPOSITION_KEY_PRIMITIVETYPE:
		{
			unsigned int newPrimitive;
			if(stream_cast(newPrimitive,value) ||
					newPrimitive >= PRIMITIVE_END)
				return false;
	

			//TODO: Convert the type data as best we can.
			primitiveType=newPrimitive;

			//In leiu of covnersion, just reset the primitive
			//values to some nominal defaults.
			vectorParams.clear();
			scalarParams.clear();
			switch(primitiveType)
			{
				case PRIMITIVE_CYLINDER:
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
		case COMPOSITION_KEY_RADIUS:
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
		case COMPOSITION_KEY_SHOWPRIMITIVE:
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

		case COMPOSITION_KEY_NORMALISE:
		{
			unsigned int valueInt;
			if(stream_cast(valueInt,value))
				return false;

			if(!(valueInt ==0 || valueInt == 1))
				return false;
			
			if(normalise!= valueInt)
			{
				needUpdate=true;
				normalise=valueInt;
			}
			else
				needUpdate=false;
		
			clearCache();
			needUpdate=true;	
			break;	
		}
		case COMPOSITION_KEY_LOCKAXISMAG:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			lockAxisMag=(stripped=="1");

			needUpdate=true;

			break;
		}

		case COMPOSITION_KEY_PLOTTYPE:
		{
			unsigned int tmpPlotType;

			tmpPlotType=plotID(value);

			if(tmpPlotType >= PLOT_TRACE_ENDOFENUM)
				return false;

			plotStyle = tmpPlotType;
			needUpdate=true;	
			break;
		}
		case COMPOSITION_KEY_COLOUR:
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
		case COMPOSITION_KEY_ERRMODE:
		{
			unsigned int tmpMode;
			tmpMode=plotErrmodeID(value);

			if(tmpMode >= PLOT_ERROR_ENDOFENUM)
				return false;

			errMode.mode= tmpMode;
			needUpdate=true;

			break;
		}
		case COMPOSITION_KEY_AVGWINSIZE:
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
	if(PRIMITIVE_END > 1)
	{
		stream_cast(str,(int)PRIMITIVE_END-1);
		str =string(TRANS("Primitive Type (0-") + str + ")");
		stream_cast(tmpStr,primitiveType);
		s.push_back(make_pair(str,tmpStr));
		keys.push_back(COMPOSITION_KEY_PRIMITIVETYPE);
		type.push_back(PROPERTY_TYPE_INTEGER);
	}

	str = TRANS("Show Primitive");	
	stream_cast(tmpStr,showPrimitive);
	s.push_back(make_pair(str,tmpStr));
	keys.push_back(COMPOSITION_KEY_SHOWPRIMITIVE);
	type.push_back(PROPERTY_TYPE_BOOL);

	switch(primitiveType)
	{
		case PRIMITIVE_CYLINDER:
		{
			ASSERT(vectorParams.size() == 2);
			ASSERT(scalarParams.size() == 1);
			stream_cast(str,vectorParams[0]);
			keys.push_back(COMPOSITION_KEY_ORIGIN);
			s.push_back(make_pair(TRANS("Origin"), str));
			type.push_back(PROPERTY_TYPE_POINT3D);
			
			stream_cast(str,vectorParams[1]);
			keys.push_back(COMPOSITION_KEY_NORMAL);
			s.push_back(make_pair(TRANS("Axis"), str));
			type.push_back(PROPERTY_TYPE_POINT3D);

			if(lockAxisMag)
				str="1";
			else
				str="0";
			keys.push_back(COMPOSITION_KEY_LOCKAXISMAG);
			s.push_back(make_pair(TRANS("Lock Axis Mag."), str));
			type.push_back(PROPERTY_TYPE_BOOL);
			
			stream_cast(str,scalarParams[0]);
			keys.push_back(COMPOSITION_KEY_RADIUS);
			s.push_back(make_pair(TRANS("Radius"), str));
			type.push_back(PROPERTY_TYPE_POINT3D);



			break;
		}
	}

	keys.push_back(COMPOSITION_KEY_FIXEDBINS);
	stream_cast(str,fixedBins);
	s.push_back(make_pair(TRANS("Fixed Bin Num"), str));
	type.push_back(PROPERTY_TYPE_BOOL);

	if(fixedBins)
	{
		stream_cast(tmpStr,nBins);
		str = TRANS("Num Bins");
		s.push_back(make_pair(str,tmpStr));
		keys.push_back(COMPOSITION_KEY_NUMBINS);
		type.push_back(PROPERTY_TYPE_INTEGER);
	}
	else
	{
		str = TRANS("Bin width");
		stream_cast(tmpStr,binWidth);
		s.push_back(make_pair(str,tmpStr));
		keys.push_back(COMPOSITION_KEY_BINWIDTH);
		type.push_back(PROPERTY_TYPE_REAL);
	}

	str = TRANS("Normalise");	
	stream_cast(tmpStr,normalise);
	s.push_back(make_pair(str,tmpStr));
	keys.push_back(COMPOSITION_KEY_NORMALISE);
	type.push_back(PROPERTY_TYPE_BOOL);


	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);

	s.clear();
	type.clear();
	keys.clear();
	
	//use set 2 to store the plot properties
	stream_cast(str,plotStyle);
	//Let the user know what the valid values for plot type are
	string tmpChoice;
	vector<pair<unsigned int,string> > choices;


	tmpStr=plotString(PLOT_TRACE_LINES);
	choices.push_back(make_pair((unsigned int) PLOT_TRACE_LINES,tmpStr));
	tmpStr=plotString(PLOT_TRACE_BARS);
	choices.push_back(make_pair((unsigned int)PLOT_TRACE_BARS,tmpStr));
	tmpStr=plotString(PLOT_TRACE_STEPS);
	choices.push_back(make_pair((unsigned int)PLOT_TRACE_STEPS,tmpStr));
	tmpStr=plotString(PLOT_TRACE_STEM);
	choices.push_back(make_pair((unsigned int)PLOT_TRACE_STEM,tmpStr));

	tmpStr= choiceString(choices,plotStyle);
	s.push_back(make_pair(string(TRANS("Plot Type")),tmpStr));
	type.push_back(PROPERTY_TYPE_CHOICE);
	keys.push_back(COMPOSITION_KEY_PLOTTYPE);
	//Convert the colour to a hex string
	if(!haveRangeParent)
	{
		string thisCol;
		genColString((unsigned char)(r*255.0),(unsigned char)(g*255.0),
		(unsigned char)(b*255.0),(unsigned char)(a*255.0),thisCol);

		s.push_back(make_pair(string(TRANS("Colour")),thisCol)); 
		type.push_back(PROPERTY_TYPE_COLOUR);
		keys.push_back(COMPOSITION_KEY_COLOUR);
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
	s.push_back(make_pair(string(TRANS("Err. Estimator")),tmpStr));
	type.push_back(PROPERTY_TYPE_CHOICE);
	keys.push_back(COMPOSITION_KEY_ERRMODE);


	if(errMode.mode == PLOT_ERROR_MOVING_AVERAGE)
	{
		stream_cast(tmpStr,errMode.movingAverageNum);
		s.push_back(make_pair(string(TRANS("Avg. Window")), tmpStr));
		type.push_back(PROPERTY_TYPE_INTEGER);
		keys.push_back(COMPOSITION_KEY_AVGWINSIZE);

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
			f << tabs(depth+1) << "<userstring value=\""<< escapeXML(userString) << "\"/>"  << endl;

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
			f << tabs(depth+1) << "<fixedbins value=\"" << (int)fixedBins << "\"/>" << endl;
			f << tabs(depth+1) << "<nbins value=\"" << nBins << "\"/>" << endl;
			f << tabs(depth+1) << "<binwidth value=\"" << binWidth << "\"/>" << endl;
			f << tabs(depth+1) << "<colour r=\"" <<  r<< "\" g=\"" << g << "\" b=\"" <<b
				<< "\" a=\"" << a << "\"/>" <<endl;

			f << tabs(depth+1) << "<plottype value=\"" << plotStyle << "\"/>" << endl;
			f << tabs(depth) << "</" << trueName()  << " >" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}


void CompositionProfileFilter::setUserString(const std::string &str)
{
	if(userString != str)
	{
		userString=str;
		clearCache();
	}	
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

	if(primitiveType >= PRIMITIVE_END)
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
		case PRIMITIVE_CYLINDER:
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
	if(stream_cast(plotStyle,tmpStr))
		return false;

	if(plotStyle >= PLOT_TRACE_ENDOFENUM)
	       return false;	
	xmlFree(xmlString);
	//====

	return true;
}

unsigned int CompositionProfileFilter::getRefreshBlockMask() const
{
	//Absolutely anything can go through this filter.
	return 0;
}

unsigned int CompositionProfileFilter::getRefreshEmitMask() const
{
	if(showPrimitive)
		return STREAM_TYPE_PLOT | STREAM_TYPE_DRAW;
	else
		return STREAM_TYPE_PLOT;
}

unsigned int CompositionProfileFilter::getRefreshUseMask() const
{
	return STREAM_TYPE_IONS | STREAM_TYPE_RANGE;
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

#ifdef DEBUG

bool testDensityCylinder();
bool testCompositionCylinder();
void synthComposition(const vector<pair<float,float> > &compositionData,
			vector<IonHit> &h);
IonStreamData *synthLinearProfile(const Point3D &start, const Point3D &end,
					float radialSpread,unsigned int numPts);

bool CompositionProfileFilter::runUnitTests()
{
	if(!testDensityCylinder())
		return false;

	if(!testCompositionCylinder())
		return false;

	return true;
}

bool testCompositionCylinder()
{
	IonStreamData *d;
	const size_t NUM_PTS=10000;

	//Create a cylinder of data, forming a linear profile
	Point3D startPt(-1.0f,-1.0f,-1.0f),endPt(1.0f,1.0f,1.0f);
	d= synthLinearProfile(startPt,endPt,
			0.5f, NUM_PTS);

	//Generate two compositions for the test dataset
	{
	vector<std::pair<float,float>  > vecCompositions;
	vecCompositions.push_back(make_pair(2.0f,0.5f));
	vecCompositions.push_back(make_pair(3.0f,0.5f));
	synthComposition(vecCompositions,d->data);
	}

	//Build a faux rangestream
	RangeStreamData *rngStream;
	rngStream = new RangeStreamData;
	rngStream->rangeFile = new RangeFile;
	
	RGBf rgb; rgb.red=rgb.green=rgb.blue=1.0f;

	unsigned int aIon,bIon;
	std::string tmpStr;
	tmpStr="A";
	aIon=rngStream->rangeFile->addIon(tmpStr,tmpStr,rgb);
	tmpStr="B";
	bIon=rngStream->rangeFile->addIon(tmpStr,tmpStr,rgb);
	rngStream->rangeFile->addRange(1.5,2.5,aIon);
	rngStream->rangeFile->addRange(2.5,3.5,bIon);
	rngStream->enabledIons.resize(2,true);
	rngStream->enabledRanges.resize(2,true);

	//Construct the composition filter
	CompositionProfileFilter *f = new CompositionProfileFilter;

	//Build some points to pass to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	
	bool needUp; std::string s;
	stream_cast(s,Point3D((startPt+endPt)*0.5f));
	TEST(f->setProperty(0,COMPOSITION_KEY_ORIGIN,s,needUp),"set origin");
	
	stream_cast(s,Point3D((endPt-startPt)*0.5f));
	TEST(f->setProperty(0,COMPOSITION_KEY_NORMAL,s,needUp),"set direction");
	TEST(f->setProperty(0,COMPOSITION_KEY_SHOWPRIMITIVE,"1",needUp),"Set cylinder visibility");
	TEST(f->setProperty(0,COMPOSITION_KEY_NORMALISE,"1",needUp),"Disable normalisation");
	TEST(f->setProperty(0,COMPOSITION_KEY_RADIUS,"5",needUp),"Set radius");
	
	//Inform the filter about the range stream
	streamIn.push_back(rngStream);
	f->initFilter(streamIn,streamOut);
	
	streamIn.push_back(d);
	f->setCaching(false);


	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p,dummyCallback),"Refresh error code");

	TEST(streamOut.size() == 3, "output stream count");

	delete d;

	std::map<unsigned int, unsigned int> countMap;
	countMap[STREAM_TYPE_PLOT] = 0;
	countMap[STREAM_TYPE_DRAW] = 0;

	for(unsigned int ui=0;ui<streamOut.size();ui++)
	{
		ASSERT(countMap.find(streamOut[ui]->getStreamType()) != countMap.end());
		countMap[streamOut[ui]->getStreamType()]++;
	}

	TEST(countMap[STREAM_TYPE_PLOT] == 2,"Plot count");
	TEST(countMap[STREAM_TYPE_DRAW] == 1,"Draw count");
	
	const PlotStreamData* plotData=0;
	for(unsigned int ui=0;ui<streamOut.size();ui++)
	{
		if(streamOut[ui]->getStreamType() == STREAM_TYPE_PLOT)
		{
			plotData = (const PlotStreamData *)streamOut[ui];
			break;
		}
	}

	TEST(plotData->xyData.size(),"Plot data size");

	for(size_t ui=0;ui<plotData->xyData.size(); ui++)
	{
		TEST(plotData->xyData[ui].second <= 1.0f && 
			plotData->xyData[ui].second >=0.0f,"normalised data range test"); 
	}

	for(unsigned int ui=0;ui<streamOut.size();ui++)
		delete streamOut[ui];


	delete f;
	delete rngStream->rangeFile;
	delete rngStream;

	return true;
}

bool testDensityCylinder()
{
	IonStreamData *d;
	const size_t NUM_PTS=10000;

	//Create a cylinder of data, forming a linear profile
	Point3D startPt(-1.0f,-1.0f,-1.0f),endPt(1.0f,1.0f,1.0f);
	d= synthLinearProfile(startPt,endPt,
			0.5f, NUM_PTS);

	//Generate two compositions for the test dataset
	{
	vector<std::pair<float,float>  > vecCompositions;
	vecCompositions.push_back(make_pair(2.0f,0.5f));
	vecCompositions.push_back(make_pair(3.0f,0.5f));
	synthComposition(vecCompositions,d->data);
	}

	CompositionProfileFilter *f = new CompositionProfileFilter;
	f->setCaching(false);

	//Build some points to pass to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	streamIn.push_back(d);
	
	bool needUp; std::string s;
	stream_cast(s,Point3D((startPt+endPt)*0.5f));
	TEST(f->setProperty(0,COMPOSITION_KEY_ORIGIN,s,needUp),"set origin");
	
	stream_cast(s,Point3D((endPt-startPt)*0.5f));
	TEST(f->setProperty(0,COMPOSITION_KEY_NORMAL,s,needUp),"set direction");
	
	TEST(f->setProperty(0,COMPOSITION_KEY_SHOWPRIMITIVE,"1",needUp),"Set cylinder visibility");

	TEST(f->setProperty(0,COMPOSITION_KEY_NORMALISE,"0",needUp),"Disable normalisation");
	TEST(f->setProperty(0,COMPOSITION_KEY_RADIUS,"5",needUp),"Set radius");

	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p,dummyCallback),"Refresh error code");
	delete f;
	delete d;


	TEST(streamOut.size() == 2, "output stream count");

	std::map<unsigned int, unsigned int> countMap;
	countMap[STREAM_TYPE_PLOT] = 0;
	countMap[STREAM_TYPE_DRAW] = 0;

	for(unsigned int ui=0;ui<streamOut.size();ui++)
	{
		ASSERT(countMap.find(streamOut[ui]->getStreamType()) != countMap.end());
		countMap[streamOut[ui]->getStreamType()]++;
	}

	TEST(countMap[STREAM_TYPE_PLOT] == 1,"Plot count");
	TEST(countMap[STREAM_TYPE_DRAW] == 1,"Draw count");

	
	const PlotStreamData* plotData=0;
	for(unsigned int ui=0;ui<streamOut.size();ui++)
	{
		if(streamOut[ui]->getStreamType() == STREAM_TYPE_PLOT)
		{
			plotData = (const PlotStreamData *)streamOut[ui];
			break;
		}
	}

	float sum=0;
	for(size_t ui=0;ui<plotData->xyData.size(); ui++)
		sum+=plotData->xyData[ui].second;


	TEST(sum > NUM_PTS/1.2f,"Number points roughly OK");
	TEST(sum <= NUM_PTS,"No overcounting");
	
	for(unsigned int ui=0;ui<streamOut.size();ui++)
		delete streamOut[ui];

	return true;
}


//first value in pair is target mass, second value is target composition
void synthComposition(const vector<std::pair<float,float> > &compositionData,
			vector<IonHit> &h)
{
	float fractionSum=0;
	for(size_t ui=0;ui<compositionData.size(); ui++)
		fractionSum+=compositionData[ui].second;

	//build the spactings between 0 and 1, so we can
	//randomly select ions by uniform deviates
	vector<std::pair<float,float> > ionCuts;
	ionCuts.resize(compositionData.size());
	//ionCuts.resize[compositionData.size()];
	float runningSum=0;
	for(size_t ui=0;ui<ionCuts.size(); ui++)
	{
		runningSum+=compositionData[ui].second;
		ionCuts[ui]=make_pair(compositionData[ui].first, 
				runningSum/fractionSum);
	}

	RandNumGen rngHere;
	rngHere.initTimer();
	for(size_t ui=0;ui<h.size();ui++)
	{

		float newMass;
		bool haveSetMass;
		
		//keep generating random selections until we hit something.
		// This is to prevent any fp fallthrough
		do
		{
			float uniformDeviate;
			uniformDeviate=rngHere.genUniformDev();

			haveSetMass=false;
			//This is not efficient -- data is sorted,
			//so binary search would work, but whatever.
			for(size_t uj=0;uj<ionCuts.size();uj++)	
			{
				if(uniformDeviate >=ionCuts[uj].second)
				{
					newMass=ionCuts[uj].first;
					haveSetMass=true;
					break;
				}
			}
		}while(!haveSetMass);


		h[ui].setMassToCharge(newMass);
	}
}


//Create a line of points of fixed mass (1), with a top-hat radial spread function
// so we end up with a cylinder of unit mass data along some start-end axis
//you must free the returned value by calling "delete"
IonStreamData *synthLinearProfile(const Point3D &start, const Point3D &end,
					float radialSpread,unsigned int numPts)
{

	ASSERT((start-end).sqrMag() > std::numeric_limits<float>::epsilon());
	IonStreamData *d = new IonStreamData;

	IonHit h;
	h.setMassToCharge(1.0f);

	Point3D delta; 
	delta=(end-start)*1.0f/(float)numPts;

	RandNumGen rngAxial;
	rngAxial.initTimer();
	
	Point3D unitDelta;
	unitDelta=delta;
	unitDelta.normalise();
	
	
	d->data.resize(numPts);
	for(size_t ui=0;ui<numPts;ui++)
	{
		//generate a random offset vector
		//that is normal to the axis of the simulation
		Point3D randomVector;
		do
		{
			randomVector=Point3D(rngAxial.genUniformDev(),
					rngAxial.genUniformDev(),
					rngAxial.genUniformDev());
		}while(randomVector.sqrMag() < std::numeric_limits<float>::epsilon() &&
			randomVector.angle(delta) < std::numeric_limits<float>::epsilon());

		
		randomVector=randomVector.crossProd(unitDelta);
		randomVector.normalise();

		//create the point
		Point3D pt;
		pt=delta*(float)ui + start; //true location
		pt+=randomVector*radialSpread;
		h.setPos(pt);
		d->data[ui] =h;
	}

	return d;
}
#endif
