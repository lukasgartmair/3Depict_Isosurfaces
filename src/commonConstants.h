/*
 * commonConstants.h  - Common constants used across program
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
#ifndef COMMONCONSTANTS_H
#define COMMONCONSTANTS_H
//Plot styles
enum
{
  PLOT_TRACE_LINES=0,
  PLOT_TRACE_BARS,
  PLOT_TRACE_STEPS,
  PLOT_TRACE_STEM,
  PLOT_TRACE_POINTS,
  PLOT_TRACE_ENDOFENUM
};

//Plot types
enum
{
	PLOT_MODE_1D,
	PLOT_MODE_2D,
	PLOT_MODE_ENUM_END
};


//Plot error types
enum
{
	PLOT_ERROR_NONE,
	PLOT_ERROR_MOVING_AVERAGE,
	PLOT_ERROR_ENDOFENUM
};

//!State file output formats
enum
{
	STATE_FORMAT_XML=1
};

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
	PROPERTY_TYPE_ENUM_END //Not a prop, just end of enum
};


//!Movement types for plot
enum
{
	REGION_MOVE_EXTEND_XMINUS,
	REGION_MOVE_TRANSLATE_X,
	REGION_MOVE_EXTEND_XPLUS
};

//!Data holder for colour as float
typedef struct RGBf
{
	float red;
	float green;
	float blue;
} RGBf;

//!Structure to handle error bar drawing in plot
struct PLOT_ERROR
{
	//!Plot data estimator mode
	unsigned int mode;
	//!Number of data points for moving average
	unsigned int movingAverageNum;
	//!Edge mode
	unsigned int edgeMode;
};

#endif
