/*
 *	endianttest.h - Platform endian testing
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

#ifndef _ENDIAN_TEST_H_
#define _ENDIAN_TEST_H_
#if defined (_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#define __LITTLE_ENDIAN__
#else
#ifdef __linux__
#include <endian.h>
#endif
#endif

#ifdef __BYTE_ORDER
//if both are not defined it is TRUE!
#if __BYTE_ORDER == __BIG_ENDIAN
#define __BIG_ENDIAN__
#elif __BYTE_ORDER == __LITTLE_ENDIAN
#define __LITTLE_ENDIAN__
#elif __BYTE_ORDER == __PDP_ENDIAN
#define __ARM_ENDIAN__
#else
#error "Endian determination failed"
#endif
#endif

const int ENDIAN_TEST=1;
//Run-time detection
inline int is_bigendian() { return (*(char*)&ENDIAN_TEST) == 0 ;}

inline int is_littleendian() { return (*(char*)&ENDIAN_TEST) == 1 ;}


inline void floatSwapBytes(float *inFloat)
{
	char floatBytes[4];
	floatBytes[0]=*((char *)inFloat+3);
	floatBytes[1]=*((char *)inFloat+2);
	floatBytes[2]=*((char *)inFloat+1);
	floatBytes[3]=*((char *)inFloat);

	*inFloat=*((float *)floatBytes);
}

#endif
