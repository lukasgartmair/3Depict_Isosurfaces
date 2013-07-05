/*
 *	dataLoad.cpp - filter to  load datasets from various source
 *	Copyright (C) 2013, D Haley 

 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.

 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.

 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "dataLoad.h"

//Needed for modification time
#include <wx/file.h>
#include <wx/filename.h>
#include "../../wxcommon.h"

#include "filterCommon.h"


#include "backend/APT/APTFileIO.h"

//Default number of ions to load
const size_t MAX_IONS_LOAD_DEFAULT=5*1024*1024/(4*sizeof(float)); //5 MB worth.

// Tp prevent the dropdown lists from getting too unwieldy, set an artificial maximum
const unsigned int MAX_NUM_FILE_COLS=5000; 

//Allowable text file deliminators
const char *TEXT_DELIMINATORS = "\t ,";

//Supported data types
enum
{
	FILEDATA_TYPE_POS,
	FILEDATA_TYPE_TEXT,
	FILEDATA_TYPE_ENUM_END, // Not a data type, just end of enum
};

const char *AVAILABLE_FILEDATA_TYPES[] = { 	NTRANS("POS Data"),
					NTRANS("Text Data"),
					};
const char *DEFAULT_LABEL="Mass-to-Charge (amu/e)";

// == Pos load filter ==
DataLoadFilter::DataLoadFilter() : fileType(FILEDATA_TYPE_POS), doSample(true), maxIons(MAX_IONS_LOAD_DEFAULT),
	r(1.0f),g(0.0f),b(0.0f),a(1.0f),ionSize(2.0f), numColumns(4), enabled(true),
	volumeRestrict(false), monitorTimestamp(-1),monitorSize((size_t)-1),wantMonitor(false),
	valueLabel(TRANS(DEFAULT_LABEL))
{
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(AVAILABLE_FILEDATA_TYPES) == FILEDATA_TYPE_ENUM_END);
	cache=true;

	bound.setInverseLimits();
	
	for (unsigned int i  = 0; i < numColumns; i++) {
		index[i] = i;
	}

}

Filter *DataLoadFilter::cloneUncached() const
{
	DataLoadFilter *p=new DataLoadFilter;
	p->ionFilename=ionFilename;
	p->doSample=doSample;
	p->maxIons=maxIons;
	p->ionSize=ionSize;
	p->fileType=fileType;
	p->guessType=guessType;
	//Colours
	p->r=r;	
	p->g=g;	
	p->b=b;	
	p->a=a;	
	p->fileType=fileType;
	//Bounding volume
	p->bound.setBounds(bound);
	p->volumeRestrict=volumeRestrict;
	p->numColumns=numColumns;
	p->enabled=enabled;

	for(size_t ui=0;ui<INDEX_LENGTH;ui++)
		p->index[ui]=index[ui];

	//We are copying wether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->enabled=enabled;
	p->userString=userString;

	p->wantMonitor=wantMonitor;
	// this is for a pos file
	memcpy(p->index, index, sizeof(int) * 4);
	p->numColumns=numColumns;

	return p;
}


void DataLoadFilter::setFileMode(unsigned int fileMode)
{
	switch(fileMode)
	{
		case DATALOAD_TEXT_FILE:
			fileType=FILEDATA_TYPE_TEXT;
			break;
		case DATALOAD_FLOAT_FILE:
			fileType=FILEDATA_TYPE_POS;
			break;
		default:
			ASSERT(false);
	}
}


void DataLoadFilter::setFilename(const char *name)
{
	ionFilename = name;
	guessNumColumns();
}

void DataLoadFilter::setFilename(const std::string &name)
{
	ionFilename = name;
	guessNumColumns();
}

void DataLoadFilter::guessNumColumns()
{
	//Test the extension to determine what we will do
	string extension;
	if(ionFilename.size() > 4)
		extension = ionFilename.substr ( ionFilename.size() - 4, 4 );

	//Set extension to lowercase version
	for(size_t ui=0;ui<extension.size();ui++)
		extension[ui] = tolower(extension[ui]);

	if( extension == std::string(".pos")) {
		numColumns = 4;
		return;
	}
	numColumns = 4;
}

//!Get (approx) number of bytes required for cache
size_t DataLoadFilter::numBytesForCache(size_t nObjects) const 
{
	//Otherwise we have to work it out, based upon the file
	size_t result;
	getFilesize(ionFilename.c_str(),result);

	if(doSample)
		return std::min(maxIons*sizeof(float)*4,result);


	return result;	
}

unsigned int DataLoadFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress, bool (*callback)(bool))
{
	errStr="";
	//use the cached copy if we have it.
	if(cacheOK)
	{
		bool doUseCache=true;
		//If we are monitoring the file,
		//the cache is only valid if we have 
		//the same timestamp as on the file.
		if(wantMonitor)
		{
			if(!wxFile::Exists(wxStr(ionFilename)))
			{
				monitorTimestamp=-1;
				monitorSize=-1;
				doUseCache=false;
				clearCache();
			}
			else
			{
				//How can we have a valid cache if we don't
				//have a valid load time?
				ASSERT(monitorTimestamp!=-1 && monitorSize!=(size_t)-1);


				size_t fileSizeVal;
				getFilesize(ionFilename.c_str(),fileSizeVal);
				if(wxFileModificationTime(wxStr(ionFilename)) ==monitorTimestamp
					||  fileSizeVal!= monitorSize)
				{
					doUseCache=false;
					clearCache();
				}
			}
		}

		//Use the cache if it is still OK, otherwise use
		//the full function
		if(doUseCache)
		{
			ASSERT(filterOutputs.size());
			for(unsigned int ui=0;ui<dataIn.size();ui++)
				getOut.push_back(dataIn[ui]);

			for(unsigned int ui=0;ui<filterOutputs.size();ui++)
				getOut.push_back(filterOutputs[ui]);
		
			return 0;
		}
	}

	//If theres no file, then there is not a lot we can do..
	if(!wxFile::Exists(wxStr(ionFilename)))
	{
		wxFileName f(wxStr(ionFilename));
		
		errStr= stlStr(f.GetFullName())  + TRANS(" does not exist");
		return ERR_FILE_OPEN;
	}
	
	//If we have disable the filter, or we are are monitoring and 
	//there is no file
	if(!enabled ||(wantMonitor && !wxFile::Exists(wxStr(ionFilename))) )
	{
		monitorTimestamp=-1;
		monitorSize=-1;
		for(unsigned int ui=0;ui<dataIn.size();ui++)
			getOut.push_back(dataIn[ui]);

		return 0;
	}

	//Update the monitoring timestamp such that we know
	//when the file was last loaded
	monitorTimestamp = wxFileModificationTime(wxStr(ionFilename));
	size_t tmp;
	if(getFilesize(ionFilename.c_str(),tmp))
		monitorSize=tmp;

	IonStreamData *ionData = new IonStreamData;
	ionData->parent=this;	

	unsigned int uiErr;	
	switch(fileType)
	{
		case FILEDATA_TYPE_POS:
		{
			if(doSample)
			{
				//Load the pos file, limiting how much you pull from it
				if((uiErr = LimitLoadPosFile(numColumns, INDEX_LENGTH, index, ionData->data, ionFilename.c_str(),
									maxIons,progress.filterProgress,callback,strongRandom)))
				{
					consoleOutput.push_back(string(TRANS("Error loading file: ")) + ionFilename);
					delete ionData;
					errStr=TRANS(POS_ERR_STRINGS[uiErr]);
					return uiErr;
				}
			}	
			else
			{
				if((uiErr = GenericLoadFloatFile(numColumns, INDEX_LENGTH, index, ionData->data, ionFilename.c_str(),
									progress.filterProgress,callback)))
				{
					consoleOutput.push_back(string(TRANS("Error loading file: ")) + ionFilename);
					delete ionData;
					errStr=TRANS(POS_ERR_STRINGS[uiErr]);
					return uiErr;
				}
			}	
			break;
		}
		case FILEDATA_TYPE_TEXT:
		{

		
			if(doSample)
			{
				//TODO: Migrate to using a generic text data loading routine
				//	rather than an IonHit specific one, to avoid need for separate error strings
				//Load the data from a text file
				if((uiErr = limitLoadTextFile(INDEX_LENGTH,index,ionData->data, ionFilename.c_str(),TEXT_DELIMINATORS,
									maxIons,progress.filterProgress,callback,strongRandom)))
				{
					consoleOutput.push_back(string(TRANS("Error loading file: ")) + ionFilename);
					delete ionData;
					errStr=ION_TEXT_ERR_STRINGS[uiErr];
					return uiErr;
				}
			}
			else
			{
				vector<vector<float> > outDat;
				vector<std::string> headerData;
				if((uiErr=loadTextData(ionFilename.c_str(),outDat,headerData,TEXT_DELIMINATORS)))
				{
					consoleOutput.push_back(string(TRANS("Error loading file: ")) + ionFilename);
					delete ionData;
					errStr=TEXT_LOAD_ERR_STRINGS[uiErr];
					return uiErr;
				}
		
					

				if(outDat.size() !=4)
				{
					std::string sizeStr;
					stream_cast(sizeStr,outDat.size());

					consoleOutput.push_back(
						string(TRANS("Data file contained incorrect number of columns -- should be 4, was ")) + sizeStr );

					errStr=TEXT_LOAD_ERR_STRINGS[ERR_FILE_FORMAT];
					return ERR_FILE_FORMAT;
				}
			
			
				ASSERT(outDat[0].size() == outDat[1].size() && 
					outDat[1].size() == outDat[2].size()
					&& outDat[2].size() == outDat[3].size());

				ionData->data.resize(outDat[0].size());
				#pragma omp parallel for
				for(unsigned int ui=0;ui<outDat[0].size(); ui++)
				{
					//Convert vector to ionhits.	
					ionData->data[ui].setPos(outDat[0][ui],outDat[1][ui],outDat[2][ui]);
					ionData->data[ui].setMassToCharge(outDat[3][ui]);
				}
			}	

			
			break;
		}
		
		default:
			ASSERT(false);
	}


	ionData->r = r;
	ionData->g = g;
	ionData->b = b;
	ionData->a = a;
	ionData->ionSize=ionSize;
	ionData->valueType=valueLabel;


	if(ionData->data.empty())
	{
		//Shouldn't get here...
		ASSERT(false);
		delete ionData;
		return	0;
	}


	BoundCube dataCube;
	IonHit::getBoundCube(ionData->data,dataCube);

	if(dataCube.isNumericallyBig())
	{
		consoleOutput.push_back(
			TRANS("Warning:One or more bounds of the loaded data approaches "
			       "the limits of numerical stability for the internal data type"
			       "(magnitude too large). Consider rescaling data before loading"));
	}

	string s;
	stream_cast(s,ionData->data.size());
	consoleOutput.push_back( string(TRANS("Loaded ") + s + TRANS(" Points")) );
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
}

void DataLoadFilter::getProperties(FilterPropGroup &propertyList) const
{
	FilterProperty p;

	size_t curGroup=0;

	p.type=PROPERTY_TYPE_STRING;
	p.key=DATALOAD_KEY_FILE;
	p.name=TRANS("File");
	p.helpText=TRANS("File from which to load data");
	p.data=ionFilename;

	propertyList.addProperty(p,curGroup);

	vector<pair<unsigned int,string> > choices;

	for(unsigned int ui=0;ui<FILEDATA_TYPE_ENUM_END; ui++)
		choices.push_back(make_pair(ui,AVAILABLE_FILEDATA_TYPES[ui]));	
					
	p.data=choiceString(choices,fileType);
	p.name=TRANS("File type");
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Type of file to be loaded");
	p.key=DATALOAD_KEY_FILETYPE;
	
	propertyList.addProperty(p,curGroup);

	//---------
	curGroup++;	
	
	string colStr;
	switch(fileType)
	{
		case FILEDATA_TYPE_POS:
		{
			stream_cast(colStr,numColumns);
			p.name=TRANS("Entries per point");
			p.helpText=TRANS("Number of decimal values in file per 3D point (normally 4)");
			p.data=colStr;
			p.key=DATALOAD_KEY_NUMBER_OF_COLUMNS;
			p.type=PROPERTY_TYPE_INTEGER;
			propertyList.addProperty(p,curGroup);
			break;
		}
		case FILEDATA_TYPE_TEXT:
		{
			break;
		}
		default:
			ASSERT(false);
			
	}

	choices.clear();
	for (unsigned int i = 0; i < numColumns; i++) {
		string tmp;
		stream_cast(tmp,i);
		choices.push_back(make_pair(i,tmp));
	}
	
	colStr= choiceString(choices,index[0]);
	p.name="X";
	p.data=colStr;
	p.key=DATALOAD_KEY_SELECTED_COLUMN0;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Relative offset of each entry in file for point's X position");
	propertyList.addProperty(p,curGroup);
	
	colStr= choiceString(choices,index[1]);
	p.name="Y";
	p.data=colStr;
	p.key=DATALOAD_KEY_SELECTED_COLUMN1;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Relative offset of each entry in file for point's Y position");
	propertyList.addProperty(p,curGroup);
	
	colStr= choiceString(choices,index[2]);
	p.name="Z";
	p.data=colStr;
	p.key=DATALOAD_KEY_SELECTED_COLUMN2;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Relative offset of each entry in file for point's Z position");
	propertyList.addProperty(p,curGroup);
	
	colStr= choiceString(choices,index[3]);
	p.name=TRANS("Value");
	p.data=colStr;
	p.key=DATALOAD_KEY_SELECTED_COLUMN3;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Relative offset of each entry in file to use for scalar value of 3D point");
	propertyList.addProperty(p,curGroup);
	
	p.name=TRANS("Value Label");
	p.data=valueLabel;
	p.key=DATALOAD_KEY_VALUELABEL;
	p.type=PROPERTY_TYPE_STRING;
	p.helpText=TRANS("Name for the scalar value associated with each point");
	propertyList.addProperty(p,curGroup);
	
	propertyList.setGroupTitle(curGroup,TRANS("Format params."));

	curGroup++;

	string tmpStr;
	stream_cast(tmpStr,enabled);
	p.name=TRANS("Enabled");
	p.data=tmpStr;
	p.key=DATALOAD_KEY_ENABLED;
	p.type=PROPERTY_TYPE_BOOL;
	p.helpText=TRANS("Load this file?");
	propertyList.addProperty(p,curGroup);

	if(enabled)
	{
		std::string tmpStr;
		
		stream_cast(tmpStr,doSample);
		p.name=TRANS("Sample data");
		p.data=tmpStr;
		p.type=PROPERTY_TYPE_BOOL;
		p.helpText=TRANS("Perform random selection on file contents, instead of loading entire file");
		p.key=DATALOAD_KEY_SAMPLE;
		propertyList.addProperty(p,curGroup);

		if(doSample)
		{
			stream_cast(tmpStr,maxIons*sizeof(float)*4/(1024*1024));
			p.name=TRANS("Load Limit (MB)");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_INTEGER;
			p.helpText=TRANS("Limit for size of data to load");
			p.key=DATALOAD_KEY_SIZE;
			propertyList.addProperty(p,curGroup);
		}
	
		stream_cast(tmpStr,wantMonitor);
		p.name=TRANS("Monitor");
		p.data=tmpStr;
		p.key=DATALOAD_KEY_MONITOR;
		p.type=PROPERTY_TYPE_BOOL;
		p.helpText=TRANS("Watch file timestamp to track changes to file contents from other programs");
		propertyList.addProperty(p,curGroup);
		propertyList.setGroupTitle(curGroup,TRANS("Load params."));

		
		curGroup++;

		string thisCol;
		//Convert the ion colour to a hex string	
		genColString((unsigned char)(r*255),(unsigned char)(g*255),
				(unsigned char)(b*255),(unsigned char)(a*255),thisCol);

		p.name=TRANS("Default colour ");
		p.data=thisCol; 
		p.type=PROPERTY_TYPE_COLOUR;
		p.helpText=TRANS("Default colour for points, if not overridden by other filters");
		p.key=DATALOAD_KEY_COLOUR;
		propertyList.addProperty(p,curGroup);

		stream_cast(tmpStr,ionSize);
		p.name=TRANS("Draw Size");
		p.data=tmpStr; 
		p.type=PROPERTY_TYPE_REAL;
		p.helpText=TRANS("Default size for points, if not overridden by other filters");
		p.key=DATALOAD_KEY_IONSIZE;
		propertyList.addProperty(p,curGroup);
		propertyList.setGroupTitle(curGroup,TRANS("Appearance"));
	}

}

bool DataLoadFilter::setProperty(  unsigned int key, 
					const std::string &value, bool &needUpdate)
{
	
	needUpdate=false;
	switch(key)
	{
		case DATALOAD_KEY_FILETYPE:
		{
			unsigned int ltmp;
			ltmp=(unsigned int)-1;
			
			for(unsigned int ui=0;ui<FILEDATA_TYPE_ENUM_END; ui++)
			{
				if(AVAILABLE_FILEDATA_TYPES[ui] == value)
				{
					ltmp=ui;
					break;
				}
			}
			if(ltmp == (unsigned int) -1 || ltmp == fileType)
				return false;

			fileType=ltmp;
			clearCache();
			needUpdate=true;
			break;
		}
		case DATALOAD_KEY_FILE:
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
		case DATALOAD_KEY_ENABLED:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			bool lastVal=enabled;
			enabled=(stripped == "1");
			
			//if the result is different, the
			//cache should be invalidated
			if(lastVal!=enabled)
				needUpdate=true;
			
			clearCache();
			break;
		}
		case DATALOAD_KEY_MONITOR:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			bool lastVal=wantMonitor;
			wantMonitor=(stripped=="1");

			//if the result is different, the
			//cache should be invalidated
			if(lastVal!=wantMonitor)
				needUpdate=true;
			
			clearCache();
			break;
		}
		
		case DATALOAD_KEY_SAMPLE:
		{
			string stripped=stripWhite(value);

			if(!(stripped == "1"|| stripped == "0"))
				return false;

			bool lastVal=doSample;
			doSample=(stripped == "1");
			
			//if the result is different, the
			//cache should be invalidated
			if(lastVal!=doSample)
				needUpdate=true;
			
			clearCache();
			break;
		}
		case DATALOAD_KEY_SIZE:
		{
			size_t ltmp;
			if(stream_cast(ltmp,value))
				return false;

			if(!ltmp)
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
		case DATALOAD_KEY_COLOUR:
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
		case DATALOAD_KEY_IONSIZE:
		{
			float ltmp;
			if(stream_cast(ltmp,value))
				return false;

			if(ltmp < 0)
				return false;

			ionSize=ltmp;

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
		case DATALOAD_KEY_VALUELABEL:
		{
			if(value !=valueLabel)
			{
				valueLabel=value;
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
							i->valueType=valueLabel;
						}
					}
				}

			}

			break;
		}
		case DATALOAD_KEY_SELECTED_COLUMN0:
		{
			unsigned int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp >= numColumns)
				return false;
			
			index[0]=ltmp;
			needUpdate=true;
			clearCache();
			
			break;
		}
		case DATALOAD_KEY_SELECTED_COLUMN1:
		{
			unsigned int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp >= numColumns)
				return false;
			
			index[1]=ltmp;
			needUpdate=true;
			clearCache();
			
			break;
		}
		case DATALOAD_KEY_SELECTED_COLUMN2:
		{
			unsigned int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp >= numColumns)
				return false;
			
			index[2]=ltmp;
			needUpdate=true;
			clearCache();
			
			break;
		}
		case DATALOAD_KEY_SELECTED_COLUMN3:
		{
			unsigned int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp >= numColumns)
				return false;
			
			index[3]=ltmp;
			needUpdate=true;
			clearCache();
			
			break;
		}
		case DATALOAD_KEY_NUMBER_OF_COLUMNS:
		{
			unsigned int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp >= MAX_NUM_FILE_COLS)
				return false;
			
			numColumns=ltmp;
			for (unsigned int i = 0; i < INDEX_LENGTH; i++) {
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

bool DataLoadFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
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

	//retrieve file type (text,pos etc), if needed; default to pos.
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"type");
	if(xmlString)
	{
		int type;
		if(stream_cast(type,xmlString))
			return false;
		
		if(fileType >=FILEDATA_TYPE_ENUM_END)
			return false;
		fileType=type;
		xmlFree(xmlString);
	}
	else
		fileType=FILEDATA_TYPE_POS;


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
	if(numColumns >= MAX_NUM_FILE_COLS)
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
	
	//Retrieve enabled/disabled
	//--
	unsigned int tmpVal;
	if(!XMLGetNextElemAttrib(nodePtr,tmpVal,"enabled","value"))
		return false;
	enabled=tmpVal;
	//--
	
	//Retrieve monitor mode 
	//--
	xmlNodePtr nodeTmp;
	nodeTmp=nodePtr;
	if(XMLGetNextElemAttrib(nodePtr,tmpVal,"monitor","value"))
		wantMonitor=tmpVal;
	else
	{
		nodePtr=nodeTmp;
		wantMonitor=false;
	}
	//--
	
	//Retrieve value type string (eg mass-to-charge, 
	// or whatever the data type is)
	//--
	nodeTmp=nodePtr;
	if(XMLHelpFwdToElem(nodePtr,"valuetype"))
	{
		nodePtr=nodeTmp;
		valueLabel=TRANS(DEFAULT_LABEL);
	}
	else
	{
		xmlChar *xmlString;
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
		if(!xmlString)
			return false;
		valueLabel=(char *)xmlString;
		xmlFree(xmlString);

	}

	//--


	//Get sampling enabled/disabled
	//---
	//TODO: Remove me:
	// Note, in 3Depict-0.0.10 and lower, we did not have this option,
	// so some statefiles will exist without this. In the case it is not
	// found, we need to make it up
	{
	nodeTmp=nodePtr;
	bool needSampleState=false;
	if(!XMLGetNextElemAttrib(nodePtr,doSample,"dosample","value"))
	{
		nodePtr=nodeTmp;
		needSampleState=true;
	}
	//---

	//Get max Ions
	//--
	//TODO: Forbid zero values - don't do it now, as we previously used this
	// for disabling sampling, so some users' XML files will still have this
	if(!XMLGetNextElemAttrib(nodePtr,maxIons,"maxions","value"))
		return false;

	if(needSampleState) 
		doSample=maxIons;

	//FIXME: Compatibility hack.  User preferences from 3Depict <=0.0.10
	
	// could cause == 0 maxIons, causing an assertion error in
	// ::refresh() due to no ions. Override this.
	// In the future, we should reject this as invalid, however
	// it was valid for 3Depict <= 0.0.10
	if(!maxIons)
		maxIons=MAX_IONS_LOAD_DEFAULT;
	//--
	}
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

unsigned int DataLoadFilter::getRefreshBlockMask() const
{
	return 0;
}

unsigned int DataLoadFilter::getRefreshEmitMask() const
{
	return STREAM_TYPE_IONS;
}

unsigned int DataLoadFilter::getRefreshUseMask() const
{
	return 0;
}

std::string  DataLoadFilter::getErrString(unsigned int code) const
{
	ASSERT(errStr.size());
	return errStr;
}

void DataLoadFilter::setPropFromBinding(const SelectionBinding &b) 
{
	ASSERT(false);
}

bool DataLoadFilter::writeState(std::ostream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<" << trueName() << ">" << endl;

			f << tabs(depth+1) << "<userstring value=\""<< escapeXML(userString) << "\"/>"  << endl;
			f << tabs(depth+1) << "<file name=\"" << escapeXML(convertFileStringToCanonical(ionFilename)) << "\" type=\"" << fileType << "\"/>" << endl;
			f << tabs(depth+1) << "<columns value=\"" << numColumns << "\"/>" << endl;
			f << tabs(depth+1) << "<xyzm values=\"" << index[0] << "," << index[1] << "," << index[2] << "," << index[3] << "\"/>" << endl;
			f << tabs(depth+1) << "<enabled value=\"" << enabled<< "\"/>" << endl;
			f << tabs(depth+1) << "<monitor value=\"" << wantMonitor<< "\"/>"<< endl; 
			f << tabs(depth+1) << "<valuetype value=\"" << escapeXML(valueLabel)<< "\"/>"<< endl; 
			f << tabs(depth+1) << "<dosample value=\"" << doSample << "\"/>" << endl;
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

bool DataLoadFilter::writePackageState(std::ostream &f, unsigned int format,
			const std::vector<std::string> &valueOverrides, unsigned int depth) const
{
	ASSERT(valueOverrides.size() == 1);

	//Temporarily modify the state of the filter, then call writestate
	string tmpIonFilename=ionFilename;


	//override const -- naughty, but we know what we are doing...
	const_cast<DataLoadFilter*>(this)->ionFilename=valueOverrides[0];
	bool result;
	result=writeState(f,format,depth);

	const_cast<DataLoadFilter*>(this)->ionFilename=tmpIonFilename;

	return result;
}

void DataLoadFilter::getStateOverrides(std::vector<string> &externalAttribs) const 
{
	externalAttribs.push_back(ionFilename);

}

bool DataLoadFilter::monitorNeedsRefresh() const
{
	//We can only actually effect an update if the
	// filter is enabled (and we actually want to monitor).
	// otherwise, we don't need to refresh
	if(enabled && wantMonitor)
	{
		//Check to see that the file exists, if
		// not fall back to the cache.
		if(!wxFile::Exists(wxStr(ionFilename)))
			return cacheOK;


		size_t sizeVal;
		getFilesize(ionFilename.c_str(),sizeVal);
		if(sizeVal != monitorSize)
			return true;

		return( wxFileModificationTime(wxStr(ionFilename))
						!=monitorTimestamp);
		
		
	}


	return false;
}


#ifdef DEBUG


bool posFileTest();
bool textFileTest();


bool DataLoadFilter::runUnitTests()
{
	if(!posFileTest())
		return false;

	if(!textFileTest())
		return false;

	return true;
}

bool posFileTest()
{
	//Synthesise data, then *save* it.

	const unsigned int NUM_PTS=133;
	vector<IonHit> hits;
	hits.resize(NUM_PTS);
	for(unsigned int ui=0; ui<NUM_PTS; ui++)
	{
		hits[ui].setPos(Point3D(ui,ui,ui));
		hits[ui].setMassToCharge(ui);
	}

	//TODO: Make more robust
	const char *posName="testAFNEUEA1754.pos";
	//see if we can open the file for input. If so, it must exist
	std::ifstream file(posName,std::ios::binary);

	if(file)
	{
		std::string s;
		s="Unwilling to execute file test, will not overwrite file :";
		s+=posName;
		s+=". Test is indeterminate";
		WARN(false,s.c_str());

		return true;
	}

	if(IonHit::makePos(hits,posName))
	{
		WARN(false,"Unable to create test output file. Unit test was indeterminate. Requires write access to excution path");
		return true;
	}

	//-----------

	//Create the data load filter, load it, then check we loaded the same data
	//---------
	DataLoadFilter* d= new DataLoadFilter;
	d->setCaching(false);

	bool needUp;
	TEST(d->setProperty(DATALOAD_KEY_FILE,posName,needUp),"Set prop");
	TEST(d->setProperty(DATALOAD_KEY_SAMPLE,"0",needUp),"Set prop");
	//---------

	vector<const FilterStreamData*> streamIn,streamOut;
	ProgressData prog;
	TEST(!d->refresh(streamIn,streamOut,prog,dummyCallback),"Refresh error code");
	delete d;


	TEST(streamOut.size() == 1, "Stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS, "Stream type");

	TEST(streamOut[0]->getNumBasicObjects() == hits.size(), "Stream count");
	
	
#if defined(__LINUX__) || defined(__APPLE__)
	//Hackish method to delete file
	std::string s;
	s=string("rm -f ") + string(posName);
	system(s.c_str());
#endif

	delete streamOut[0];
	return true;
}

bool textFileTest()
{
	//write some random data
	// with a fixed seed value
	RandNumGen r;
	r.initialise(232635); 
	const unsigned int NUM_PTS=1000;

	//TODO: do better than this
	const char *FILENAME="test-3mdfuneaascn.txt";
	//see if we can open the file for input. If so, it must exist,
	//and thus we don't want to overwite it, as it may contain useful data.
	std::ifstream inFile(FILENAME);
	if(inFile)
	{
		std::string s;
		s="Unwilling to execute file test, will not overwrite file :";
		s+=FILENAME;
		s+=". Test is indeterminate";
		WARN(false,s.c_str());

		return true;
	}
		
	std::ofstream outFile(FILENAME);

	if(!outFile)
	{
		WARN(false,"Unable to create test output file. Unit test was indeterminate. Requires write access to excution path");
		return true;
	}

	vector<IonHit> hitVec;
	hitVec.resize(NUM_PTS);
	
	//Write out the file
	outFile << "x y\tz\tValues" << endl;
	for(unsigned int ui=0;ui<NUM_PTS;ui++)
	{
		Point3D p;
		p.setValue(r.genUniformDev(),r.genUniformDev(),r.genUniformDev());
		hitVec[ui].setPos(p);
		hitVec[ui].setMassToCharge(r.genUniformDev());
		outFile << p[0] << " " << p[1] << "\t" << p[2] << "\t" << hitVec[ui].getMassToCharge() << endl;
	}

	outFile.close();

	//Create the data load filter, load it, then check we loaded the same data
	//---------
	DataLoadFilter* d=new DataLoadFilter;
	d->setCaching(false);

	bool needUp;
	TEST(d->setProperty(DATALOAD_KEY_FILE,FILENAME,needUp),"Set prop");
	TEST(d->setProperty(DATALOAD_KEY_SAMPLE,"0",needUp),"Set prop"); //load all data
	//Load data as text file
	TEST(d->setProperty(DATALOAD_KEY_FILETYPE,
			AVAILABLE_FILEDATA_TYPES[FILEDATA_TYPE_TEXT],needUp),"Set prop"); 
	//---------


	vector<const FilterStreamData*> streamIn,streamOut;
	ProgressData prog;
	TEST(!d->refresh(streamIn,streamOut,prog,dummyCallback),"Refresh error code");
	delete d;


	TEST(streamOut.size() == 1, "Stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS, "Stream type");

	TEST(streamOut[0]->getNumBasicObjects() == NUM_PTS,"Stream count");

#if defined(__LINUX__) || defined(__APPLE__)
	//Hackish mathod to delete file
	std::string s;
	s=string("rm -f ") + string(FILENAME);
	system(s.c_str());
#endif

	delete streamOut[0];
	return true;
}

#endif
