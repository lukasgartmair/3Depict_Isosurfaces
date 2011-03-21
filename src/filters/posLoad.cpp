#include "posLoad.h"

#include "../xmlHelper.h"
#include "../basics.h"


//Default number of ions to load
const size_t MAX_IONS_LOAD_DEFAULT=5*1024*1024/(4*sizeof(float)); //5 MB worth.

// Tp prevent the dropdown lists from getting too unwieldy, set an artificial maximum
const unsigned int MAX_NUM_FILE_COLS=5000; 
enum
{
	KEY_POSLOAD_FILE,
	KEY_POSLOAD_SIZE,
	KEY_POSLOAD_COLOUR,
	KEY_POSLOAD_IONSIZE,
	KEY_POSLOAD_ENABLED,
	KEY_POSLOAD_SELECTED_COLUMN0,
	KEY_POSLOAD_SELECTED_COLUMN1,
	KEY_POSLOAD_SELECTED_COLUMN2,
	KEY_POSLOAD_SELECTED_COLUMN3,
	KEY_POSLOAD_NUMBER_OF_COLUMNS,
};

// == Pos load filter ==
PosLoadFilter::PosLoadFilter()
{
	cache=true;
	maxIons=MAX_IONS_LOAD_DEFAULT;
	fileType=FILE_TYPE_POS;
	//default ion colour is red.
	r=a=1.0f;
	g=b=0.0f;

	enabled=true;
	volumeRestrict=false;
	bound.setInverseLimits();
	//Ion size (rel. size)..
	ionSize=2.0;

	numColumns = 4;
	for (int i  = 0; i < numColumns; i++) {
		index[i] = i;
	}
}

Filter *PosLoadFilter::cloneUncached() const
{
	PosLoadFilter *p=new PosLoadFilter;
	p->ionFilename=ionFilename;
	p->maxIons=maxIons;
	p->ionSize=ionSize;
	p->r=r;	
	p->g=g;	
	p->b=b;	
	p->a=a;	
	p->bound.setBounds(bound);
	p->volumeRestrict=volumeRestrict;
	p->numColumns=numColumns;
	p->fileType=fileType;
	p->enabled=enabled;

	for(size_t ui=0;ui<INDEX_LENGTH;ui++)
		p->index[ui]=index[ui];

	//We are copying wether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->enabled=enabled;
	p->userString=userString;

	// this is for a pos file
	memcpy(p->index, index, sizeof(int) * 4);
	p->numColumns=numColumns;

	return p;
}

void PosLoadFilter::setFilename(const char *name)
{
	ionFilename = name;
	guessNumColumns();
}

void PosLoadFilter::setFilename(const std::string &name)
{
	ionFilename = name;
	guessNumColumns();
}

void PosLoadFilter::guessNumColumns()
{
	//Test the extention to determine what we will do
	string extension;
	if(ionFilename.size() > 4)
		extension = ionFilename.substr ( ionFilename.size() - 4, 4 );

	//Set extention to lowercase version
	for(size_t ui=0;ui<extension.size();ui++)
		extension[ui] = tolower(extension[ui]);

	if( extension == std::string(".pos")) {
		numColumns = 4;
		fileType = FILE_TYPE_POS;
		return;
	}
	fileType = FILE_TYPE_NULL;
	numColumns = 4;
}

//!Get (approx) number of bytes required for cache
size_t PosLoadFilter::numBytesForCache(size_t nObjects) const 
{
	//Otherwise we hvae to work it out, based upon the file
	size_t result;
	getFilesize(ionFilename.c_str(),result);

	if(maxIons)
		return std::min(maxIons*sizeof(float)*4,result);


	return result;	
}

string PosLoadFilter::getValueLabel()
{
	return std::string("Mass-to-Charge (amu/e)");
}

unsigned int PosLoadFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(void))
{
	//use the cached copy if we have it.
	if(cacheOK)
	{
		ASSERT(filterOutputs.size());
		for(unsigned int ui=0;ui<dataIn.size();ui++)
			getOut.push_back(dataIn[ui]);

		for(unsigned int ui=0;ui<filterOutputs.size();ui++)
			getOut.push_back(filterOutputs[ui]);

		return 0;
	}

	if(!enabled)
	{
		for(unsigned int ui=0;ui<dataIn.size();ui++)
			getOut.push_back(dataIn[ui]);

		return 0;
	}

	IonStreamData *ionData = new IonStreamData;


	unsigned int uiErr;	
	if(maxIons)
	{
		//Load the pos file, limiting how much you pull from it
		if((uiErr = LimitLoadPosFile(numColumns, INDEX_LENGTH, index, ionData->data, ionFilename.c_str(),
							maxIons,progress.filterProgress,callback,strongRandom)))
		{
			consoleOutput.push_back(string("Error loading file: ") + ionFilename);
			delete ionData;
			return uiErr;
		}
	}
	else
	{
		//Load the pos file
		if((uiErr = GenericLoadFloatFile(numColumns, INDEX_LENGTH, index, ionData->data, ionFilename.c_str(),
							progress.filterProgress,callback)))
		{
			consoleOutput.push_back(string("Error loading file: ") + ionFilename);
			delete ionData;
			return uiErr;
		}
	}

	ionData->r = r;
	ionData->g = g;
	ionData->b = b;
	ionData->a = a;
	ionData->ionSize=ionSize;
	ionData->valueType=getValueLabel();
	
	random_shuffle(ionData->data.begin(),ionData->data.end());


	string s;
	stream_cast(s,ionData->data.size());
	consoleOutput.push_back( string("Loaded ") + s + " Ions" );
	if(cache)
	{
		ionData->cached=1;
		filterOutputs.push_back(ionData);
		cacheOK=true;
	}
	else
		ionData->cached=0;

	for(unsigned int ui=0;ui<dataIn.size();ui++)
		getOut.push_back(dataIn[ui]);

	//Append the ion data 
	getOut.push_back(ionData);

	return 0;
};

void PosLoadFilter::getProperties(FilterProperties &propertyList) const
{
	propertyList.data.clear();
	propertyList.types.clear();
	propertyList.keys.clear();

	vector<pair<string,string> > s;
	vector<unsigned int> type,keys;

	s.push_back(std::make_pair("File", ionFilename));
	type.push_back(PROPERTY_TYPE_STRING);
	keys.push_back(KEY_POSLOAD_FILE);
	
	string colStr;
	stream_cast(colStr,numColumns);
	s.push_back(std::make_pair("Number of columns", colStr));
	keys.push_back(KEY_POSLOAD_NUMBER_OF_COLUMNS);
	type.push_back(PROPERTY_TYPE_INTEGER);

	vector<pair<unsigned int,string> > choices;
	for (int i = 0; i < numColumns; i++) {
		string tmp;
		stream_cast(tmp,i);
		choices.push_back(make_pair(i,tmp));
	}
	
	colStr= choiceString(choices,index[0]);
	s.push_back(std::make_pair("x", colStr));
	keys.push_back(KEY_POSLOAD_SELECTED_COLUMN0);
	type.push_back(PROPERTY_TYPE_CHOICE);
	
	colStr= choiceString(choices,index[1]);
	s.push_back(std::make_pair("y", colStr));
	keys.push_back(KEY_POSLOAD_SELECTED_COLUMN1);
	type.push_back(PROPERTY_TYPE_CHOICE);
	
	colStr= choiceString(choices,index[2]);
	s.push_back(std::make_pair("z", colStr));
	keys.push_back(KEY_POSLOAD_SELECTED_COLUMN2);
	type.push_back(PROPERTY_TYPE_CHOICE);
	
	colStr= choiceString(choices,index[3]);
	s.push_back(std::make_pair("value", colStr));
	keys.push_back(KEY_POSLOAD_SELECTED_COLUMN3);
	type.push_back(PROPERTY_TYPE_CHOICE);
	
	string tmpStr;
	stream_cast(tmpStr,enabled);
	s.push_back(std::make_pair("Enabled", tmpStr));
	keys.push_back(KEY_POSLOAD_ENABLED);
	type.push_back(PROPERTY_TYPE_BOOL);

	if(enabled)
	{
		std::string tmpStr;
		stream_cast(tmpStr,maxIons*sizeof(float)*4/(1024*1024));
		s.push_back(std::make_pair("Load Limit (MB)",tmpStr));
		type.push_back(PROPERTY_TYPE_INTEGER);
		keys.push_back(KEY_POSLOAD_SIZE);
		
		string thisCol;
		//Convert the ion colour to a hex string	
		genColString((unsigned char)(r*255),(unsigned char)(g*255),
				(unsigned char)(b*255),(unsigned char)(a*255),thisCol);

		s.push_back(make_pair(string("Default colour "),thisCol)); 
		type.push_back(PROPERTY_TYPE_COLOUR);
		keys.push_back(KEY_POSLOAD_COLOUR);

		stream_cast(tmpStr,ionSize);
		s.push_back(make_pair(string("Draw Size"),tmpStr)); 
		type.push_back(PROPERTY_TYPE_REAL);
		keys.push_back(KEY_POSLOAD_IONSIZE);
	}

	propertyList.data.push_back(s);
	propertyList.types.push_back(type);
	propertyList.keys.push_back(keys);
}

bool PosLoadFilter::setProperty( unsigned int set, unsigned int key, 
					const std::string &value, bool &needUpdate)
{
	
	needUpdate=false;
	switch(key)
	{
		case KEY_POSLOAD_FILE:
		{
			//ensure that the new file can be found
			//Try to open the file
			std::ifstream f(value.c_str());
			if(!f)
				return false;
			f.close();

			setFilename(value);
			//Invalidate the cache
			clearCache();

			needUpdate=true;
			break;
		}
		case KEY_POSLOAD_ENABLED:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			bool lastVal=enabled;
			if(stripped=="1")
				enabled=true;
			else
				enabled=false;

			//if the result is different, the
			//cache should be invalidated
			if(lastVal!=enabled)
				needUpdate=true;
			
			clearCache();
			break;
		}
		case KEY_POSLOAD_SIZE:
		{
			std::string tmp;
			size_t ltmp;
			if(stream_cast(ltmp,value))
				return false;

			if(ltmp*(1024*1024/(sizeof(float)*4)) != maxIons)
			{
				//Convert from MB to ions.			
				maxIons = ltmp*(1024*1024/(sizeof(float)*4));
				needUpdate=true;
				//Invalidate cache
				clearCache();
			}
			break;
		}
		case KEY_POSLOAD_COLOUR:
		{
			unsigned char newR,newG,newB,newA;

			parseColString(value,newR,newG,newB,newA);

			if(newB != b || newR != r ||
				newG !=g || newA != a)
			{
				r=newR/255.0;
				g=newG/255.0;
				b=newB/255.0;
				a=newA/255.0;


				//Check the cache, updating it if needed
				if(cacheOK)
				{
					for(unsigned int ui=0;ui<filterOutputs.size();ui++)
					{
						if(filterOutputs[ui]->getStreamType() == STREAM_TYPE_IONS)
						{
							IonStreamData *i;
							i=(IonStreamData *)filterOutputs[ui];
							i->r=r;
							i->g=g;
							i->b=b;
							i->a=a;
						}
					}

				}
				needUpdate=true;
			}


			break;
		}
		case KEY_POSLOAD_IONSIZE:
		{
			std::string tmp;
			float ltmp;
			if(stream_cast(ltmp,value))
				return false;

			if(ltmp < 0)
				return false;

			ionSize=ltmp;
			needUpdate=true;

			//Check the cache, updating it if needed
			if(cacheOK)
			{
				for(unsigned int ui=0;ui<filterOutputs.size();ui++)
				{
					if(filterOutputs[ui]->getStreamType() == STREAM_TYPE_IONS)
					{
						IonStreamData *i;
						i=(IonStreamData *)filterOutputs[ui];
						i->ionSize=ionSize;
					}
				}
			}
			needUpdate=true;

			break;
		}
		case KEY_POSLOAD_SELECTED_COLUMN0:
		{
			std::string tmp;
			int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp < 0 || ltmp >= numColumns)
				return false;
			
			index[0]=ltmp;
			needUpdate=true;
			clearCache();
			
			break;
		}
		case KEY_POSLOAD_SELECTED_COLUMN1:
		{
			std::string tmp;
			int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp < 0 || ltmp >= numColumns)
				return false;
			
			index[1]=ltmp;
			needUpdate=true;
			clearCache();
			
			break;
		}
		case KEY_POSLOAD_SELECTED_COLUMN2:
		{
			std::string tmp;
			int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp < 0 || ltmp >= numColumns)
				return false;
			
			index[2]=ltmp;
			needUpdate=true;
			clearCache();
			
			break;
		}
		case KEY_POSLOAD_SELECTED_COLUMN3:
		{
			std::string tmp;
			unsigned int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp < 0 || ltmp >= numColumns)
				return false;
			
			index[3]=ltmp;
			needUpdate=true;
			clearCache();
			
			break;
		}
		case KEY_POSLOAD_NUMBER_OF_COLUMNS:
		{
			std::string tmp;
			int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp <=0 || ltmp >= MAX_NUM_FILE_COLS)
				return false;
			
			numColumns=ltmp;
			for (int i = 0; i < INDEX_LENGTH; i++) {
				index[i] = (index[i] < numColumns? index[i]: numColumns - 1);
			}
			needUpdate=true;
			clearCache();
			
			break;
		}
		default:
			ASSERT(false);
			break;
	}
	return true;
}

bool PosLoadFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
	//Retrieve user string
	//===
	if(XMLHelpFwdToElem(nodePtr,"userstring"))
		return false;

	xmlChar *xmlString;
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	userString=(char *)xmlString;
	xmlFree(xmlString);
	//===

	//Retrieve file name	
	if(XMLHelpFwdToElem(nodePtr,"file"))
		return false;
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"name");
	if(!xmlString)
		return false;
	ionFilename=(char *)xmlString;
	xmlFree(xmlString);

	//Override the string, as needed
	if( (stateFileDir.size()) &&
		(ionFilename.size() > 2 && ionFilename.substr(0,2) == "./") )
	{
		ionFilename=stateFileDir + ionFilename.substr(2);
	}

	//Filenames need to be converted from unix format (which I make canonical on disk) into native format 
	ionFilename=convertFileStringToNative(ionFilename);

	//Retrieve number of columns	
	if(!XMLGetNextElemAttrib(nodePtr,numColumns,"columns","value"))
		return false;
	if(numColumns <=0 || numColumns >= MAX_NUM_FILE_COLS)
		return false;
	
	//Retrieve index	
	if(XMLHelpFwdToElem(nodePtr,"xyzm"))
		return false;
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"values");
	if(!xmlString)
		return false;
	std::vector<string> v;
	splitStrsRef((char *)xmlString,',',v);
	for (unsigned int i = 0; i < INDEX_LENGTH && i < v.size(); i++)
	{
		if(stream_cast(index[i],v[i]))
			return false;

		if(index[i] >=numColumns)
			return false;
	}
	xmlFree(xmlString);
	
	//Retreive enabled/disabled
	//--
	unsigned int tmpVal;
	if(!XMLGetNextElemAttrib(nodePtr,tmpVal,"enabled","value"))
		return false;
	enabled=tmpVal;
	//--

	//Get max Ions
	//--
	if(!XMLGetNextElemAttrib(nodePtr,maxIons,"maxions","value"))
		return false;
	//--
	
	std::string tmpStr;
	//Retrieve colour
	//====
	if(XMLHelpFwdToElem(nodePtr,"colour"))
		return false;

	if(!parseXMLColour(nodePtr,r,g,b,a))
		return false;	
	//====

	//Retrieve drawing size value
	//--
	if(!XMLGetNextElemAttrib(nodePtr,ionSize,"ionsize","value"))
		return false;
	//check positive or zero
	if(ionSize <=0)
		return false;
	//--


	return true;
}

std::string  PosLoadFilter::getErrString(unsigned int code) const
{
	ASSERT(code < POS_ERR_FINAL);
	return posErrStrings[code];
}

bool PosLoadFilter::writeState(std::ofstream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<" << trueName() << ">" << endl;

			f << tabs(depth+1) << "<userstring value=\""<<userString << "\"/>"  << endl;
			f << tabs(depth+1) << "<file name=\"" << convertFileStringToCanonical(ionFilename) << "\"/>" << endl;
			f << tabs(depth+1) << "<columns value=\"" << numColumns << "\"/>" << endl;
			f << tabs(depth+1) << "<xyzm values=\"" << index[0] << "," << index[1] << "," << index[2] << "," << index[3] << "\"/>" << endl;
			f << tabs(depth+1) << "<enabled value=\"" << enabled<< "\"/>" << endl;
			f << tabs(depth+1) << "<maxions value=\"" << maxIons << "\"/>" << endl;

			f << tabs(depth+1) << "<colour r=\"" <<  r<< "\" g=\"" << g << "\" b=\"" <<b
				<< "\" a=\"" << a << "\"/>" <<endl;
			f << tabs(depth+1) << "<ionsize value=\"" << ionSize << "\"/>" << endl;
			f << tabs(depth) << "</" << trueName() << ">" << endl;
			break;
		}
		default:
			//Shouldn't get here, unhandled format string 
			ASSERT(false);
			return false;
	}

	return true;

}

bool PosLoadFilter::writePackageState(std::ofstream &f, unsigned int format,
			const std::vector<std::string> &valueOverrides, unsigned int depth) const
{
	ASSERT(valueOverrides.size() == 1);

	//Temporarily modify the state of the filter, then call writestate
	string tmpIonFilename=ionFilename;


	//override const -- naughty, but we know what we are doing...
	const_cast<PosLoadFilter*>(this)->ionFilename=valueOverrides[0];
	bool result;
	result=writeState(f,format,depth);

	const_cast<PosLoadFilter*>(this)->ionFilename=tmpIonFilename;

	return result;
}

void PosLoadFilter::getStateOverrides(std::vector<string> &externalAttribs) const 
{
	externalAttribs.push_back(ionFilename);
}
