/*
 * assertion.h  - Program assertion header
 * Copyright (C) 2011  D Haley
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ASSERTION_H
#define ASSERTION_H

#ifdef DEBUG
	#include "eventlogger.h"
	#include <iostream>
	#include <cstdlib>


	void dh_assert(const char * const filename, const unsigned int lineNumber); 
	void dh_warn(const char * const filename, const unsigned int lineNumber,
							const char *message);

	#ifndef ASSERT
	#define ASSERT(f) if(!(f)) {dh_assert(__FILE__,__LINE__);}
	#endif

	#ifndef WARN
	#define WARN(f,g) if(!(f)) { dh_warn(__FILE__,__LINE__,g);}
	#endif
	
	inline void dh_assert(const char * const filename, const unsigned int lineNumber) 
	{
		extern EventLogger evtlog;
		evtlog.dump();
		std::cerr << "ASSERTION ERROR!" << std::endl;
		std::cerr << "Filename: " << filename << std::endl;
		std::cerr << "Line number: " << lineNumber << std::endl;

		std::cerr << "Do you wish to continue?(y/n)";
		char y = 'a';
		while (y != 'n' && y != 'y')
			std::cin >> y;

		if (y != 'y')
			exit(1);
	}

	inline void dh_warn(const char * const filename, const unsigned int lineNumber,const char *message) 
	{
		std::cerr << "Warning to programmer." << std::endl;
		std::cerr << "Filename: " << filename << std::endl;
		std::cerr << "Line number: " << lineNumber << std::endl;
		std::cerr << message << std::endl;
	}

	//Debug timing routines
	#define DEBUG_TIME_START timeval TIME_DEBUG_t; gettimeofday(&TIME_DEBUG_t,NULL);
	#define DEBUG_TIME_END timeval TIME_DEBUG_tend; gettimeofday(&TIME_DEBUG_tend,NULL); \
		cerr << (TIME_DEBUG_tend.tv_sec - TIME_DEBUG_t.tv_sec) + ((float)TIME_DEBUG_tend.tv_usec-(float)TIME_DEBUG_t.tv_usec)/1.0e6;

	//OpenGL debugging macro
	#define glError() { \
		GLenum err = glGetError(); \
		while (err != GL_NO_ERROR) { \
					fprintf(stderr, "glError: %s caught at %s:%u\n", (char *)gluErrorString(err), __FILE__, __LINE__); \
					err = glGetError(); \
				} \
		std::cerr << "glErr Clean " << __FILE__ << ":" << __LINE__ << std::endl; \
		}

	#ifndef TEST
	#define TEST(f,g) if(!(f)) { cerr << "Test fail :" << __FILE__ << ":" << __LINE__ << "\t"<< g << endl;return false;}
	#endif

	//A hack to generate compile time asserts (thanks Internet).
	//This causes gcc to give "duplicate case value", if the predicate is false
	#define COMPILE_ASSERT(pred)            \
	    switch(0){case 0:case pred:;}


#else
	#define ASSERT(f)
	#define COMPILE_ASSERT(f)
	#define WARN(f,g) 
	#define glError()
#endif

#endif
