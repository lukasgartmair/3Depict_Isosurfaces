#include "boundingBox.h"
#include "../xmlHelper.h"

#include "../translation.h"

enum
{
	KEY_BOUNDINGBOX_VISIBLE=1,
	KEY_BOUNDINGBOX_COUNT_X,
	KEY_BOUNDINGBOX_COUNT_Y,
	KEY_BOUNDINGBOX_COUNT_Z,
	KEY_BOUNDINGBOX_FONTSIZE,
	KEY_BOUNDINGBOX_FONTCOLOUR,
	KEY_BOUNDINGBOX_FIXEDOUT,
	KEY_BOUNDINGBOX_LINECOLOUR,
	KEY_BOUNDINGBOX_LINEWIDTH,
	KEY_BOUNDINGBOX_SPACING_X,
	KEY_BOUNDINGBOX_SPACING_Y,
	KEY_BOUNDINGBOX_SPACING_Z
};

enum
{
	BOUNDINGBOX_ABORT_ERR,
};
//=== Bounding box filter ==


BoundingBoxFilter::BoundingBoxFilter()
{
	fixedNumTicks=true;
	threeDText=true;
	for(unsigned int ui=0;ui<3;ui++)
	{
		numTicks[ui]=12;
		tickSpacing[ui]=5.0f;
	}
	fontSize=5;

	rLine=gLine=0.0f;
	aLine=bLine=1.0f;
	


	lineWidth=2.0f;
	isVisible=true;

	cacheOK=false;
	cache=false; 
}

Filter *BoundingBoxFilter::cloneUncached() const
{
	BoundingBoxFilter *p=new BoundingBoxFilter();
	p->fixedNumTicks=fixedNumTicks;
	for(unsigned int ui=0;ui<3;ui++)
	{
		p->numTicks[ui]=numTicks[ui];
		p->tickSpacing[ui]=tickSpacing[ui];
	}

	p->isVisible=isVisible;
	p->rLine=rLine;
	p->gLine=gLine;
	p->bLine=bLine;
	p->aLine=aLine;
	p->threeDText=threeDText;	

	p->lineWidth=lineWidth;
	p->fontSize=fontSize;

	//We are copying wether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

size_t BoundingBoxFilter::numBytesForCache(size_t nObjects) const
{
	//Say we don't know, we are not going to cache anyway.
	return (size_t)-1;
}

unsigned int BoundingBoxFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(void))
{

	//Compute the bounding box of the incoming streams
	BoundCube bTotal,bThis;
	bTotal.setInverseLimits();

	size_t totalSize=numElements(dataIn);
	size_t n=0;
	Point3D p[4];
	unsigned int ptCount=0;
	for(unsigned int ui=0;ui<dataIn.size();ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS:
			{
				bThis=bTotal;
				//Grab the first four points to define a volume.
				//then expand that volume using the boundcube functions.
				const IonStreamData *d =(const IonStreamData *) dataIn[ui];
				size_t dataPos=0;
				unsigned int curProg=NUM_CALLBACK;
				while(ptCount < 4 && dataPos < d->data.size())
				{
					for(unsigned int ui=0; ui<d->data.size();ui++)
					{
						p[ptCount]=d->data[ui].getPosRef();
						ptCount++;
						dataPos=ui;
						if(ptCount >=4) 
							break;
					}
				}

				//Ptcount will be 4 if we have >=4 points in dataset
				if(ptCount == 4)
				{
					bThis.setBounds(p,4);
					//Expand the bounding volume
#ifdef _OPENMP
					//Parallel version
					unsigned int nT =omp_get_max_threads(); 

					BoundCube *newBounds= new BoundCube[nT];
					for(unsigned int ui=0;ui<nT;ui++)
						newBounds[ui]=bThis;

					bool spin=false;
					#pragma omp parallel for shared(spin)
					for(unsigned int ui=dataPos;ui<d->data.size();ui++)
					{
						unsigned int thisT=omp_get_thread_num();
						//OpenMP does not allow exiting. Use spin instead
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

						newBounds[thisT].expand(d->data[ui].getPosRef());
					}
					if(spin)
					{			
						delete d;
						return BOUNDINGBOX_ABORT_ERR;
					}

					for(unsigned int ui=0;ui<nT;ui++)
						bThis.expand(newBounds[ui]);

					delete[] newBounds;
#else
					//Single thread version
					for(unsigned int ui=dataPos;ui<d->data.size();ui++)
					{
						bThis.expand(d->data[ui].getPosRef());
						if(!curProg--)
						{
							n+=NUM_CALLBACK;
							progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
							if(!(*callback)())
							{
								delete d;
								return BOUNDINGBOX_ABORT_ERR;
							}
						}
					}
#endif
					bTotal.expand(bThis);
				}

				break;
			}
			default:
				break;
		}

		//Copy the input data to the output	
		getOut.push_back(dataIn[ui]);	
	}

	//Append the bounding box if it is valid
	if(bTotal.isValid() && isVisible)
	{
		DrawStreamData *d = new DrawStreamData;
		d->parent=this;

		//Add the rectangle drawable
		DrawRectPrism *dP = new DrawRectPrism;
		dP->setAxisAligned(bTotal);
		dP->setColour(rLine,gLine,bLine,aLine);
		dP->setLineWidth(lineWidth);
		d->drawables.push_back(dP);

		//Add the tick drawables
		Point3D tickOrigin,tickEnd;
		bTotal.getBounds(tickOrigin,tickEnd);

		float tmpTickSpacing[3];
		float tmpTickCount[3];
		if(fixedNumTicks)
		{
			for(unsigned int ui=0; ui<3;ui++)
			{
				ASSERT(numTicks[ui]);
				tmpTickSpacing[ui]=( (tickEnd[ui] - tickOrigin[ui])/(float)numTicks[ui]);
				tmpTickCount[ui]=numTicks[ui];
			}
		}
		else
		{
			for(unsigned int ui=0; ui<3;ui++)
			{
				ASSERT(numTicks[ui]);
				tmpTickSpacing[ui]= tickSpacing[ui];
				tmpTickCount[ui]=(unsigned int)((tickEnd[ui] - tickOrigin[ui])/tickSpacing[ui])+1;
			}
		}

		//Draw the ticks on the box perimeter.
		for(unsigned int ui=0;ui<3;ui++)
		{
			Point3D tickVector;
			Point3D tickPosition;
			Point3D textVector,upVector;

			tickPosition=tickOrigin;
			switch(ui)
			{
				case 0:
					tickVector=Point3D(0,-1,-1);
					textVector=Point3D(0,1,0);
					break;
				case 1:
					tickVector=Point3D(-1,0,-1);
					textVector=Point3D(1,0,0);
					break;
				case 2:
					tickVector=Point3D(-1,-1,0);
					textVector=Point3D(1,1,0);
					break;
			}

			//TODO: This would be more efficient if we made some kind of 
			//"comb" class?
			DrawVector *dV;
			DrawGLText *dT;
			//Allow up to  128 chars
			char buffer[128];
			for(unsigned int uj=0;uj<tmpTickCount[ui];uj++)
			{
				tickPosition[ui]=tmpTickSpacing[ui]*uj + tickOrigin[ui];
				dV = new DrawVector;
			
				dV->setOrigin(tickPosition);
				dV->setVector(tickVector);
				dV->setColour(rLine,gLine,bLine,aLine);

				d->drawables.push_back(dV);
		

				//Don't draw the 0 value, as this gets repeated. 
				//we will handle this separately
				if(uj)
				{
					//Draw the tick text
					if( threeDText)	
						dT = new DrawGLText(getDefaultFontFile().c_str(),FTGL_POLYGON);
					else
						dT = new DrawGLText(getDefaultFontFile().c_str(),FTGL_BITMAP);
					float f;
					f = tmpTickSpacing[ui]*uj;
					snprintf(buffer,127,"%2.0f",f);
					dT->setString(buffer);
					dT->setSize(fontSize);
					
					dT->setColour(rLine,gLine,bLine,aLine);
					dT->setOrigin(tickPosition + tickVector*2);	
					dT->setUp(Point3D(0,0,1));	
					dT->setTextDir(textVector);
					dT->setAlignment(DRAWTEXT_ALIGN_RIGHT);

					d->drawables.push_back(dT);
				}
			}

		}

		DrawGLText *dT; 
		if(threeDText)
			dT = new DrawGLText(getDefaultFontFile().c_str(),FTGL_POLYGON);
		else
			dT = new DrawGLText(getDefaultFontFile().c_str(),FTGL_BITMAP);
		//Handle "0" text value
		dT->setString("0");
		
		dT->setColour(rLine,gLine,bLine,aLine);
		dT->setSize(fontSize);
		dT->setOrigin(tickOrigin+ Point3D(-1,-1,-1));
		dT->setAlignment(DRAWTEXT_ALIGN_RIGHT);
		dT->setUp(Point3D(0,0,1));	
		dT->setTextDir(Point3D(-1,-1,0));
		d->drawables.push_back(dT);
		d->cached=0;
		
		getOut.push_back(d);
	}
	return 0;
}


void BoundingBoxFilter::getProperties(FilterProperties &propertyList) const
{
	propertyList.data.clear();
	propertyList.keys.clear();
	propertyList.types.clear();

	vector<unsigned int> type,keys;
	vector<pair<string,string> > s;

	string tmpStr;
	stream_cast(tmpStr,isVisible);
	s.push_back(std::make_pair(TRANS("Visible"), tmpStr));
	keys.push_back(KEY_BOUNDINGBOX_VISIBLE);
	type.push_back(PROPERTY_TYPE_BOOL);

	
	//Properties are X Y and Z counts on ticks
	stream_cast(tmpStr,fixedNumTicks);
	s.push_back(std::make_pair(TRANS("Fixed Tick Num"), tmpStr));
	keys.push_back(KEY_BOUNDINGBOX_FIXEDOUT);
	type.push_back(PROPERTY_TYPE_BOOL);
	if(fixedNumTicks)
	{
		//Properties are X Y and Z counts on ticks
		stream_cast(tmpStr,numTicks[0]);
		keys.push_back(KEY_BOUNDINGBOX_COUNT_X);
		s.push_back(make_pair(TRANS("Num X"), tmpStr));
		type.push_back(PROPERTY_TYPE_INTEGER);
		
		stream_cast(tmpStr,numTicks[1]);
		keys.push_back(KEY_BOUNDINGBOX_COUNT_Y);
		s.push_back(make_pair(TRANS("Num Y"), tmpStr));
		type.push_back(PROPERTY_TYPE_INTEGER);
		
		stream_cast(tmpStr,numTicks[2]);
		keys.push_back(KEY_BOUNDINGBOX_COUNT_Z);
		s.push_back(make_pair(TRANS("Num Z"), tmpStr));
		type.push_back(PROPERTY_TYPE_INTEGER);
	}
	else
	{
		stream_cast(tmpStr,tickSpacing[0]);
		s.push_back(make_pair(TRANS("Spacing X"), tmpStr));
		keys.push_back(KEY_BOUNDINGBOX_SPACING_X);
		type.push_back(PROPERTY_TYPE_REAL);

		stream_cast(tmpStr,tickSpacing[1]);
		s.push_back(make_pair(TRANS("Spacing Y"), tmpStr));
		keys.push_back(KEY_BOUNDINGBOX_SPACING_Y);
		type.push_back(PROPERTY_TYPE_REAL);

		stream_cast(tmpStr,tickSpacing[2]);
		s.push_back(make_pair(TRANS("Spacing Z"), tmpStr));
		keys.push_back(KEY_BOUNDINGBOX_SPACING_Z);
		type.push_back(PROPERTY_TYPE_REAL);
	}

	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);

	//Second set -- Box Line properties 
	type.clear();
	keys.clear();
	s.clear();

	//Colour
	genColString((unsigned char)(rLine*255.0),(unsigned char)(gLine*255.0),
		(unsigned char)(bLine*255),(unsigned char)(aLine*255),tmpStr);
	s.push_back(std::make_pair(TRANS("Box Colour"), tmpStr));
	keys.push_back(KEY_BOUNDINGBOX_LINECOLOUR);
	type.push_back(PROPERTY_TYPE_COLOUR);


	
	//Line thickness
	stream_cast(tmpStr,lineWidth);
	s.push_back(std::make_pair(TRANS("Line thickness"), tmpStr));
	keys.push_back(KEY_BOUNDINGBOX_LINEWIDTH);
	type.push_back(PROPERTY_TYPE_REAL);

	//Font size	
	stream_cast(tmpStr,fontSize);
	keys.push_back(KEY_BOUNDINGBOX_FONTSIZE);
	s.push_back(make_pair(TRANS("Font Size"), tmpStr));
	type.push_back(PROPERTY_TYPE_INTEGER);


	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);
}

bool BoundingBoxFilter::setProperty( unsigned int set, unsigned int key,
					const std::string &value, bool &needUpdate)
{

	needUpdate=false;
	switch(key)
	{
		case KEY_BOUNDINGBOX_VISIBLE:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			bool lastVal=isVisible;
			if(stripped=="1")
				isVisible=true;
			else
				isVisible=false;

			//if the result is different, the
			//cache should be invalidated
			if(lastVal!=isVisible)
				needUpdate=true;
			break;
		}	
		case KEY_BOUNDINGBOX_FIXEDOUT:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			bool lastVal=fixedNumTicks;
			if(stripped=="1")
				fixedNumTicks=true;
			else
				fixedNumTicks=false;

			//if the result is different, the
			//cache should be invalidated
			if(lastVal!=fixedNumTicks)
				needUpdate=true;
			break;
		}	
		case KEY_BOUNDINGBOX_COUNT_X:
		case KEY_BOUNDINGBOX_COUNT_Y:
		case KEY_BOUNDINGBOX_COUNT_Z:
		{
			ASSERT(fixedNumTicks);
			unsigned int newCount;
			if(stream_cast(newCount,value))
				return false;

			//there is a start and an end tick, at least
			if(newCount < 2)
				return false;

			numTicks[key-KEY_BOUNDINGBOX_COUNT_X]=newCount;
			needUpdate=true;
			break;
		}
		case KEY_BOUNDINGBOX_LINECOLOUR:
		{
			unsigned char newR,newG,newB,newA;

			parseColString(value,newR,newG,newB,newA);

			if(newB != bLine || newR != rLine ||
				newG !=gLine || newA != aLine)
				needUpdate=true;

			rLine=newR/255.0;
			gLine=newG/255.0;
			bLine=newB/255.0;
			aLine=newA/255.0;
			needUpdate=true;
			break;
		}
		case KEY_BOUNDINGBOX_LINEWIDTH:
		{
			float newWidth;
			if(stream_cast(newWidth,value))
				return false;

			if(newWidth <= 0.0f)
				return false;

			lineWidth=newWidth;
			needUpdate=true;
			break;
		}
		case KEY_BOUNDINGBOX_SPACING_X:
		case KEY_BOUNDINGBOX_SPACING_Y:
		case KEY_BOUNDINGBOX_SPACING_Z:
		{
			ASSERT(!fixedNumTicks);
			float newSpacing;
			if(stream_cast(newSpacing,value))
				return false;

			if(newSpacing <= 0.0f)
				return false;

			tickSpacing[key-KEY_BOUNDINGBOX_SPACING_X]=newSpacing;
			needUpdate=true;
			break;
		}
		case KEY_BOUNDINGBOX_FONTSIZE:
		{
			unsigned int newCount;
			if(stream_cast(newCount,value))
				return false;

			fontSize=newCount;
			needUpdate=true;
			break;
		}
		default:
			ASSERT(false);
	}	
	return true;
}


std::string  BoundingBoxFilter::getErrString(unsigned int code) const
{

	//Currently the only error is aborting
	return std::string("Aborted");
}

bool BoundingBoxFilter::writeState(std::ofstream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<" << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\""<<userString << "\"/>"  << endl;
			f << tabs(depth+1) << "<visible value=\"" << isVisible << "\"/>" << endl;
			f << tabs(depth+1) << "<fixedticks value=\"" << fixedNumTicks << "\"/>" << endl;
			f << tabs(depth+1) << "<ticknum x=\""<<numTicks[0]<< "\" y=\"" 
				<< numTicks[1] << "\" z=\""<< numTicks[2] <<"\"/>"  << endl;
			f << tabs(depth+1) << "<tickspacing x=\""<<tickSpacing[0]<< "\" y=\"" 
				<< tickSpacing[1] << "\" z=\""<< tickSpacing[2] <<"\"/>"  << endl;
			f << tabs(depth+1) << "<linewidth value=\"" << lineWidth << "\"/>"<<endl;
			f << tabs(depth+1) << "<fontsize value=\"" << fontSize << "\"/>"<<endl;
			f << tabs(depth+1) << "<colour r=\"" <<  rLine<< "\" g=\"" << gLine << "\" b=\"" <<bLine  
								<< "\" a=\"" << aLine << "\"/>" <<endl;
			f << tabs(depth) << "</" <<trueName()<< ">" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool BoundingBoxFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
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

	//Retrieve visiblity 
	//====
	if(XMLHelpFwdToElem(nodePtr,"visible"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(tmpStr == "0")
		isVisible=false;
	else if(tmpStr == "1")
		isVisible=true;
	else
		return false;

	xmlFree(xmlString);
	//====
	
	//Retrieve fixed tick num
	//====
	if(XMLHelpFwdToElem(nodePtr,"fixedticks"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(tmpStr == "0")
		fixedNumTicks=false;
	else if(tmpStr == "1")
		fixedNumTicks=true;
	else
		return false;

	xmlFree(xmlString);
	//====
	
	//Retrieve num ticks
	//====
	if(XMLHelpFwdToElem(nodePtr,"ticknum"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"x");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(numTicks[0],tmpStr))
		return false;

	xmlFree(xmlString);

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"y");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(numTicks[1],tmpStr))
		return false;

	xmlFree(xmlString);

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"z");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(numTicks[2],tmpStr))
		return false;

	xmlFree(xmlString);
	//====
	
	//Retrieve spacing
	//====
	if(XMLHelpFwdToElem(nodePtr,"tickspacing"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"x");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(tickSpacing[0],tmpStr))
		return false;

	if(tickSpacing[0] < 0.0f)
		return false;

	xmlFree(xmlString);

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"y");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(tickSpacing[1],tmpStr))
		return false;
	if(tickSpacing[1] < 0.0f)
		return false;

	xmlFree(xmlString);

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"z");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(tickSpacing[2],tmpStr))
		return false;

	if(tickSpacing[2] < 0.0f)
		return false;
	xmlFree(xmlString);
	//====
	
	//Retrieve line width 
	//====
	if(XMLHelpFwdToElem(nodePtr,"linewidth"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(lineWidth,tmpStr))
		return false;

	if(lineWidth < 0)
	       return false;	
	xmlFree(xmlString);
	//====
	
	//Retrieve font size 
	//====
	if(XMLHelpFwdToElem(nodePtr,"fontsize"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(fontSize,tmpStr))
		return false;

	xmlFree(xmlString);
	//====

	//Retrieve colour
	//====
	if(XMLHelpFwdToElem(nodePtr,"colour"))
		return false;

	if(!parseXMLColour(nodePtr,rLine,gLine,bLine,aLine))
		return false;
	//====

	return true;	
}

int BoundingBoxFilter::getRefreshBlockMask() const
{
	//Everything goes through this filter
	return 0;
}

int BoundingBoxFilter::getRefreshEmitMask() const
{
	return  STREAM_TYPE_DRAW;
}
