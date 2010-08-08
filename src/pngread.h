/*
 * 	Copyright (C) 2002 Thomas Schumm <pansi@phong.org>
 * 	MOdifications 2010 D Haley
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

#ifdef __cplusplus 
	extern "C" { 
#endif

#ifndef PNGREAD_H
#define PNGREAD_H


#if defined(__WIN32) || defined(__WIN64) || defined(__CYGWIN__)
	#include <libpng/png.h>
#else
	#include <png.h>

#endif

#include <stdlib.h>

#ifndef PNG_LIBPNG_VER
#error Requires libpng!
#endif

#if PNG_LIBPNG_VER < 10200
#error Requires libpng version 1.2.0 or greater!
#endif

int check_if_png(const char*, FILE**, unsigned int);
int read_png(FILE*, unsigned int, png_bytep**, png_uint_32*, png_uint_32*);


#ifdef __cplusplus 
	}; 
#endif
#endif
