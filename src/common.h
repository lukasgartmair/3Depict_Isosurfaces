/*
 *	common.h - Common functionality header 
 *	Copyright (C) 2010, D Haley 

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
#ifndef COMMON_H
#define COMMON_H

#include "wxPreprec.h"
#include <wx/treectrl.h>

//TODO: split this between wx and non-wx stuff

#include <string>
#include <vector>
#include <fstream>
#include <list>
#include <algorithm>
#include <utility>

#include "basics.h"
#include "mathfuncs.h"

#define wxCStr(a) wxString(a,*wxConvCurrent)
#define wxStr(a) wxString(a.c_str(),*wxConvCurrent)

#define stlStr(a) (const char *)a.mb_str()


extern const char *DTD_NAME;
extern const char *PROGRAM_NAME;
extern const char *PROGRAM_VERSION;
extern const char *FONT_FILE;


//OK, this is a bit tricky. We override the operators to call
//a callback, so the UI updates keep happening, even inside the STL function
//----
template<class T>
class GreaterWithCallback 
{
	private:
		bool (*callback)(void);
		//!Reduction frequency (use callback every k its)
		unsigned int redMax;
		//!Current reduction counter
		unsigned int reduction;
		//!pointer to progress value
		unsigned int *prgPtr;
	public:
		//!Second argument is a "reduction" value to set the number of calls
		//to the random functor before initiating a callback
		GreaterWithCallback( bool (*ptr)(void),unsigned int red)
			{ callback=ptr; reduction=redMax=red;};

		bool operator()(const T &a, const T &b) 
		{
			if(!reduction--)
			{
				reduction=redMax;
				//Execute callback
				(*callback)();
			}

			return a < b;
		}
};


template<class T>
class EqualWithCallback 
{
	private:
		bool (*callback)(void);
		//!Reduction frequency (use callback every k its)
		unsigned int redMax;
		//!Current reduction counter
		unsigned int reduction;
		//!pointer to progress value
		unsigned int *prgPtr;
	public:
		//!Second argument is a "reduction" value to set the number of calls
		//to the random functor before initiating a callback
		EqualWithCallback( bool (*ptr)(void),unsigned int red)
			{ callback=ptr; reduction=redMax=red;};

		bool operator()(const T &a, const T &b) 
		{
			if(!reduction--)
			{
				reduction=redMax;
				//Execute callback
				(*callback)();
			}

			return a ==b;
		}
};
//----

//C file peek function
inline int fpeek(FILE *stream)
{
	int c;

	c = fgetc(stream);
	ungetc(c, stream);

	return c;
}


//!Property types for wxPropertyGrid
enum
{
	PROPERTY_TYPE_BOOL=1,
	PROPERTY_TYPE_INTEGER,
	PROPERTY_TYPE_REAL,
	PROPERTY_TYPE_COLOUR,
	PROPERTY_TYPE_STRING,
	PROPERTY_TYPE_POINT3D,
	PROPERTY_TYPE_CHOICE,
};

//!Allowable export ion formats
enum
{
	IONFORMAT_POS=1,
};

//Randomly select subset. Subset will be (somewhat) sorted on output
template<class T> size_t randomSelect(std::vector<T> &result, const std::vector<T> &source, 
							RandNumGen &rng, size_t num,unsigned int &progress,bool (*callback)(), bool strongRandom=false)
{
	//If there are not enough points, just copy it across in whole
	if(source.size() <= num)
	{
		num=source.size();
		result.resize(source.size());
		for(size_t ui=0; ui<num; ui++)
			result[ui] = source[ui]; 
	
		return num;
	}

	result.resize(num);

	if(strongRandom)
	{

		size_t numTicksNeeded;
		//If the number of items is larger than half the source size,
		//switch to tracking vacancies, rather than data
		if(num < source.size()/2)
			numTicksNeeded=num;
		else
			numTicksNeeded=source.size()-num;

		//Randomly selected items 
		//---------
		std::vector<size_t> ticks;
		ticks.resize(numTicksNeeded);

		//Create an array of numTicksNeededbers and fill 
		for(size_t ui=0; ui<numTicksNeeded; ui++)
			ticks[ui]=(size_t)(rng.genUniformDev()*(source.size()-1));

		//Remove duplicates. Intersperse some callbacks to be nice
		GreaterWithCallback<size_t> gFunctor(callback,50000);
		std::sort(ticks.begin(),ticks.end(),gFunctor);
		EqualWithCallback<size_t> eqFunctor(callback,50000);
		std::vector<size_t>::iterator newLast;
		newLast=std::unique(ticks.begin(),ticks.end());	
		ticks.erase(newLast,ticks.end());

		std::vector<size_t> moreTicks;
		//Top up with unique entries
		while(ticks.size() +moreTicks.size() < numTicksNeeded)
		{
			size_t index;

			//This is actually not too bad. the collision probability is at most 50%
			//due the switching behaviour above, for any large number of items 
			//So this is at worst case nlog(n) (I think)
			index =rng.genUniformDev()*(source.size()-1);
			if(!binary_search(ticks.begin(),ticks.end(),index) &&
				std::find(moreTicks.begin(),moreTicks.end(),index) ==moreTicks.end())
				moreTicks.push_back(index);

		}

		ticks.reserve(numTicksNeeded);
		for(size_t ui=0;ui<moreTicks.size();ui++)
			ticks.push_back(moreTicks[ui]);

		moreTicks.clear();

		ASSERT(ticks.size() == numTicksNeeded);
		//---------
		
		size_t pos=0;
		//Transfer the output
		unsigned int curProg=70000;

		if(num < source.size()/2)
		{
			for(std::vector<size_t>::iterator it=ticks.begin();it!=ticks.end();it++)
			{

				result[pos]=source[*it];
				pos++;
				if(!curProg--)
				{
					progress= (unsigned int)((float)(curProg)/((float)num)*100.0f);
					(*callback)();
					curProg=70000;
				}
			}
		}
		else
		{
			//Sort the ticks properly (mostly sorted anyway..)
			std::sort(ticks.begin(),ticks.end(),gFunctor);

			unsigned int curTick=0;
			for(size_t ui=0;ui<source.size(); ui++)
			{
				//Don't copy if this is marked
				if(ui == ticks[curTick])
					curTick++;
				else
				{
					ASSERT(result.size() > (ui-curTick));
					result[ui-curTick]=source[ui];
				}
				
				if(!curProg--)
				{
					progress= (unsigned int)((float)(curProg)/((float)source.size())*100.0f);
					(*callback)();
					curProg=70000;
				}
			}
		}

		ticks.clear();
	}	
	else
	{
		//Use a weak randomisation
		LinearFeedbackShiftReg l;

		//work out the mask level we need to use
		size_t i=1;
		unsigned int j=0;
		while(i < (source.size()<<1))
		{
			i=i<<1;
			j++;
		}

		//linear shift table starts at 3.
		if(j<3) {
			j=3;
			i = 1 << j;
		}

		size_t start;
		//start at a random position  in the linear state
		start =(size_t)(rng.genUniformDev()*i);
		l.setMaskPeriod(j);
		l.setState(start);

		size_t ui=0;	
		//generate unique weak random numbers.
		while(ui<num)
		{
			size_t res;
			res= l.clock();
			
			//use the source if it lies within range.
			//drop it silently if it is out of range
			if(res< source.size())
			{
				result[ui] =source[res];
				ui++;
			}
		}

	}

	return num;
}

//Randomly select subset. Subset will be (somewhat) sorted on output
template<class T> size_t randomDigitSelection(std::vector<T> &result, const size_t max,
			RandNumGen &rng, size_t num,unsigned int &progress,bool (*callback)(),
			bool strongRandom=false)
{
	//If there are not enough points, just copy it across in whole
	if(max < num)
	{
		num=max;
		result.resize(max);
		for(size_t ui=0; ui<num; ui++)
			result[ui] = ui; 
	
		return num;
	}

	result.resize(num);

	if(strongRandom)
	{

		size_t numTicksNeeded;
		//If the number of items is larger than half the source size,
		//switch to tracking vacancies, rather than data
		if(num < max/2)
			numTicksNeeded=num;
		else
			numTicksNeeded=max-num;

		//Randomly selected items 
		//---------
		std::vector<size_t> ticks;
		ticks.resize(numTicksNeeded);

		std::cerr << "Generating some unique numbers:" << std::endl;
		//Create an array of numTicksNeededbers and fill 
		for(size_t ui=0; ui<numTicksNeeded; ui++)
			ticks[ui]=(size_t)(rng.genUniformDev()*(max-1));

		//Remove duplicates. Intersperse some callbacks to be nice
		GreaterWithCallback<size_t> gFunctor(callback,50000);
		std::sort(ticks.begin(),ticks.end(),gFunctor);
		EqualWithCallback<size_t> eqFunctor(callback,50000);
		
		std::vector<size_t>::iterator itLast;
		itLast=std::unique(ticks.begin(),ticks.end(),eqFunctor);	
		ticks.erase(itLast,ticks.end());
		std::vector<size_t> moreTicks;
		//Top up with unique entries
		std::cerr << "Topping up" << std::endl;
		while(ticks.size() +moreTicks.size() < numTicksNeeded)
		{
			size_t index;

			//This is actually not too bad. the collision probability is at most 50%
			//due the switching behaviour above, for any large number of items 
			//So this is at worst case nlog(n) (I think)
			index =rng.genUniformDev()*(max-1);
			if(!binary_search(ticks.begin(),ticks.end(),index) &&
				std::find(moreTicks.begin(),moreTicks.end(),index) ==moreTicks.end())
				moreTicks.push_back(index);

		}
		std::cerr << "Finished topup" << std::endl;

		ticks.reserve(numTicksNeeded);
		for(size_t ui=0;ui<moreTicks.size();ui++)
			ticks.push_back(moreTicks[ui]);

		moreTicks.clear();

		ASSERT(ticks.size() == numTicksNeeded);
		//---------
		
		size_t pos=0;
		//Transfer the output
		unsigned int curProg=70000;

		if(num < max/2)
		{
			for(std::vector<size_t>::iterator it=ticks.begin();it!=ticks.end();it++)
			{

				result[pos]=*it;
				pos++;
				if(!curProg--)
				{
					progress= (unsigned int)((float)(curProg)/((float)num)*100.0f);
					(*callback)();
					curProg=70000;
				}
			}
		}
		else
		{
			//Sort the ticks properly (mostly sorted anyway..)
			std::sort(ticks.begin(),ticks.end(),gFunctor);
			
			unsigned int curTick=0;
			for(size_t ui=0;ui<numTicksNeeded; ui++)
			{
				//Don't copy if this is marked
				if(ui == ticks[curTick])
					curTick++;
				else
					result[ui-curTick]=ui;
				
				if(!curProg--)
				{
					progress= (unsigned int)((float)(curProg)/((float)num)*100.0f);
					(*callback)();
					curProg=70000;
				}
			}
		}

		ticks.clear();
	}
	else	
	{
		//Use a weak randomisation
		LinearFeedbackShiftReg l;

		//work out the mask level we need to use
		size_t i=1;
		unsigned int j=0;
		while(i < (max<<1))
		{
			i=i<<1;
			j++;
		}
		
		size_t start;
		//start at a random position  in the linear state
		start =(size_t)(rng.genUniformDev()*i);
		l.setMaskPeriod(j);
		l.setState(start);

		size_t ui=0;	
		//generate unique weak random numbers.
		while(ui<num)
		{
			size_t res;
			res= l.clock();
			
			//use the source if it lies within range.
			//drop it silently if it is out of range
			if(res<max) 
			{
				result[ui] =res;
				ui++;
			}
		}
	}
	return num;
}


#endif
