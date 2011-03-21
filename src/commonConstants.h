#ifndef COMMONCONSTANTS_H
#define COMMONCONSTANTS_H
//Plot styles
enum
{
  PLOT_TYPE_LINES=0,
  PLOT_TYPE_BARS,
  PLOT_TYPE_STEPS,
  PLOT_TYPE_STEM,
  PLOT_TYPE_ENDOFENUM,
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
};

//!Plotting region ID codes
enum{
	REGION_LEFT_EXTEND=1,
	REGION_MOVE,
	REGION_RIGHT_EXTEND
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
