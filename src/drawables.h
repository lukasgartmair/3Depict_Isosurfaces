/*
 *	drawables.h - Opengl drawable objects header
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

#ifndef DRAWABLES_H
#define DRAWABLES_H

//Do we have the HPMC Real-time on-gpu isosurface library?
//Note, this define is repeated in drawables.cpp
//to avoid exposing glew.h in this header,
//which complains bitterly about header orders.
//#define HPMC_GPU_ISOSURFACE

#include "textures.h"
#include "cameras.h"
#include "voxels.h"
#include "isoSurface.h"
#include "basics.h"

//STL includes
#include <vector>
#include <string>




//MacOS is "special" and puts it elsewhere
#ifdef __APPLE__ 
	#include <OpenGL/glu.h>
#else
	#include <GL/glu.h>
#endif

//TODO: Work out if there is any way of obtaining the maximum 
//number of items that can be drawn in an opengl context
//For now Max it out at 10 million (~120MB of vertex data)
const size_t MAX_NUM_DRAWABLE_POINTS=10;


//OK, the new FTGL is fucked up. It actually uses defines from
//freetype  as arguments to #includes. Wierd. So this sequence is important
#include <ft2build.h>
#include <FTGL/ftgl.h>

enum
{
	FTGL_BITMAP=0,
	FTGL_PIXMAP,
	FTGL_OUTLINE,
	FTGL_POLYGON,
	FTGL_EXTRUDE,
	FTGL_TEXTURE
};

//!Text aligment modes for DrawGLText
enum
{
	DRAWTEXT_ALIGN_LEFT,
	DRAWTEXT_ALIGN_CENTRE,
	DRAWTEXT_ALIGN_RIGHT,
	DRAWTEXT_ALIGN_ENUM_END
};


//!Primitve drawing mode. (wireframe/solid)
enum
{
	DRAW_WIREFRAME,
	DRAW_FLAT,
	DRAW_SMOOTH,
	DRAW_END_ENUM //Not a mode, just a marker to catch end-of-enum
};

//!Axis styles
enum
{
	AXIS_IN_SPACE
};


//!Binding enums. Needed to bind drawable selection
//to internal modification actions inside the drawable
enum
{
	DRAW_SPHERE_BIND_ORIGIN,
	DRAW_SPHERE_BIND_RADIUS,
	DRAW_VECTOR_BIND_ORIENTATION,
	DRAW_VECTOR_BIND_ORIGIN,
	DRAW_CYLINDER_BIND_ORIGIN,
	DRAW_CYLINDER_BIND_DIRECTION,
	DRAW_CYLINDER_BIND_RADIUS,
	DRAW_RECT_BIND_TRANSLATE,
	DRAW_RECT_BIND_CORNER_MOVE,
	DRAW_BIND_ENUM_END
};



//!An abstract bas class for drawing primitives
class DrawableObj
{
	protected:
		//!Is the drawable active?
		bool active;

		//!Is the object changed since last set?
		bool haveChanged;
		//!Pointer to current scene camera
		static const Camera *curCamera;	
		
	public: 
		//!Can be selected from openGL viewport interactively?
		bool canSelect;

		//!Wants lighting calculations active during render?
		bool wantsLight;

		//!Is this an overlay? By default, no
		virtual bool isOverlay() const { return false;}

		//!Constructor
		DrawableObj();

		//!Do we need to do element based depth sorting?
		virtual bool needsDepthSorting() const { return false; } ;


		//!Can we break this object down into constituent elements?
		virtual bool isExplodable() const { return false;};

		//!Break object down into simple elements
		virtual void explode(std::vector<DrawableObj *> &simpleObjects){ ASSERT(isExplodable()); };

		virtual bool hasChanged() const { return haveChanged; }


		//!Set the active state of the object
		void setActive(bool active);
		//!Pure virtual funciton - draw the object
		virtual void draw() const =0;

		//!Set if user can interact with object, needed for opengl hit testing
		void setInteract(bool canAct){canSelect=canAct;};

		virtual void getBoundingBox(BoundCube &b) const = 0;
		//!Drawable destructor
		virtual ~DrawableObj();

		//!If we offer any kind of external pointer interface; use this to do a recomputation as needed. This is needed for selection binding behaviour
		virtual void recomputeParams(const vector<Point3D> &vecs, const vector<float> &scalars, unsigned int mode) {};
		
		//!Set the current camera
		static void setCurCamera(const Camera *c){curCamera=c;};

		//!Get the centre of the object. Only valid if object is simple
		virtual Point3D getCentroid() const {ASSERT(!isExplodable());} ;
};

//A single point drawing class 
class DrawPoint : public DrawableObj
{
	protected:
		//!Point origin
		Point3D origin;
		//!Point colour (r,g,b,a) range: [0.0f,1.0f]
		float r,g,b,a;
	public:
		//!Constructor
		DrawPoint();
		//!Constructor that takes in positional argments
		DrawPoint(float,float,float);
		//!Destructor
		virtual ~DrawPoint();
	
		//!Sets the color of the point to be drawn
		void setColour(float r, float g, float b, float alpha);
		//!Draws the points
		void draw() const;
		//!Sets the location of the poitns
		void setOrigin(const Point3D &);

		void getBoundingBox(BoundCube &b) const { b.setInvalid();};

		Point3D getCentroid() const{ return origin;}
};

//!A point drawing class - for many points of same size & colour
class DrawManyPoints : public DrawableObj
{
	protected:
		//!Vector of points to draw
		std::vector<Point3D> pts;
		//!Point colours (r,g,b,a) range: [0.0f,1.0f]
		float r,g,b,a;
		//!Size of the point
		float size;

		mutable bool haveCachedBounds;
		mutable BoundCube cachedBounds;
	public:
		//!Constructor
		DrawManyPoints();
		//!Destructor
		virtual ~DrawManyPoints();
		//!Swap out the internal vector with an extenal one
		void swap(std::vector<Point3D> &);
		//!Remove all points
		void clear();
		//!Add points into the drawing vector
		void addPoints(const std::vector<Point3D> &);
		//!Add a single point into the drawing vector
		void addPoint(const Point3D &);
		//! set the color of the points to be drawn
		void setColour(float r, float g, float b, float alpha);
		//!Set the display size of the drawn poitns
		void setSize(float);
		//!Draw the points
		void draw() const;

		//!Shuffle the points to remove anisotropic drawing effects. Thus must be done prior to draw call.
		void shuffle();
		//!Get the bounding box that encapuslates this object
		void getBoundingBox(BoundCube &b) const; 
		
		//!return number of points
		size_t getNumPts() { return pts.size();};

		//!This object is explodable
		bool isExplodable() { return true;}

		//!Explode object into simple point drawables
		void explode(vector<DrawableObj*> &simpleObjects);
};

//!Draw a vector
class DrawVector: public DrawableObj
{
	protected:
		//!Vector origin
		Point3D origin;
		Point3D vector;

		//!Arrow head size 
		float arrowSize;

		//!Scale arrow head by vector size
		bool scaleArrow;

		//!Vector colour (r,g,b,a) range: [0.0f,1.0f]
		float r,g,b,a;
	public:
		//!Constructor
		DrawVector();
		//!Destructor
		virtual ~DrawVector();
	
		
		//!Sets the color of the point to be drawn
		void setColour(float r, float g, float b, float alpha);
		//!Draws the points
		void draw() const;
		//!Sets the location of the poitns
		void setOrigin(const Point3D &);
		//!Sets the location of the poitns
		void setVector(const Point3D &);
		//!Gets the cylinder axis direction
		Point3D getVector(){ return vector;};

		void getBoundingBox(BoundCube &b) const; 
		//!Recompute the internal parameters using the input vector information
		void recomputeParams(const std::vector<Point3D> &vecs, 
					const std::vector<float> &scalars, unsigned int mode);

};

//! A single colour triangle
class DrawTriangle : public DrawableObj
{
	protected:
		//!The vertices of the triangle
		Point3D vertices[3];
		Point3D vertNorm[3];
		//!Colour data - red, green, blue, alpha
		float r,g,b,a;
	public:
		//!Constructor
		DrawTriangle();
		//!Destructor
		virtual ~DrawTriangle();

		//!Set one of three vertices (0-2) locations
		void setVertex(unsigned int, const Point3D &);
		//!Set the vertex normals
		void setVertexNorm(unsigned int, const Point3D &);
		//!Set the colour of the triangle
		void setColour(float r, float g, float b, float a);
		//!Draw the triangle
		void draw() const;
		//!Get bounding cube
		void getBoundingBox(BoundCube &b) const { b.setBounds(vertices,3);}
};

//!A smooth coloured quad
/* According to openGL, the quad's vertices need not be coplanar, 
 * but they must be convex
 */
class DrawQuad : public DrawableObj
{
	private:
		//!Vertices of the quad
		Point3D vertices[4];

		//!Colour data for the quad
		//!The lighting normal of the triangle 
		/*! Lighting for this class is per triangle only no
		 * per vertex lighting */
		Point3D normal;
		//!Colours of the vertices (rgba colour model)
		float r[4],g[4],b[4],a[4];
	public:
		//!Constructor
		DrawQuad();
		//!Destructor
		virtual ~DrawQuad();
		//!sets the vertices to defautl colours (r g b and white ) for each vertex respectively
		void colourVerticies();
		//!Set vertex's location
		void setVertex(unsigned int, const Point3D &);
		//!Set the colour of a vertex
		void setColour(unsigned int, float r, float g, float b, float a);
		//!Update the normal to the surface from vertices
		/*!Uses the first 3 vertices to calculate the normal.
		 */
		void calcNormal();
		//!Draw the triangle
		void draw() const;
		//!Get bounding cube
		void getBoundingBox(BoundCube &b) const { return b.setBounds(vertices,4);}
};

//!A sphere drawing 
class DrawSphere : public DrawableObj
{
	protected:
	
		//!Pointer to the GLU quadric doohicker
		GLUquadricObj *q;
		//!Origin of the object
		Point3D origin;
		//!Colour data - rgba
		float r,g,b,a;
		//!Sphere radius
		float radius;
		//!Number of lateral and longitudinal segments 
		unsigned int latSegments,longSegments;
	public:
		//!Default Constructor
		DrawSphere();
		//! Destructor
		virtual ~DrawSphere();

		//!Sets the location of the sphere's origin
		void setOrigin(const Point3D &p);
		//!Gets the location of the sphere's origin
		Point3D getOrigin() const { return origin;};
		//!Set the number of lateral segments
		void setLatSegments(unsigned int);
		//!Set the number of longitudinal segments
		void setLongSegments(unsigned int);
		//!Set the radius
		void setRadius(float);
		//!get the radius
		float getRadius() const { return radius;};
		//!Set the colour (rgba) of the object
		void setColour(float r,float g,float b,float a);
		//!Draw the sphere
		void draw() const;
		//!Get the bounding box that encapuslates this object
		void getBoundingBox(BoundCube &b) const ;

		//!Recompute the internal parameters using the input vector information
		void recomputeParams(const vector<Point3D> &vecs, const vector<float> &scalars, unsigned int mode);

};

//!A tapered cylinder drawing class
class DrawCylinder : public DrawableObj
{
	protected:
		//!Pointer to quadric, required for glu. Note the caps are defined as well
		GLUquadricObj *q,*qCap[2];
		//!Colours for cylinder
		float r,g,b,a;
		//!Cylinder start and end radii
		float radius;
		//!Length of the cylinder
		float length;

		//!Origin of base and direction of cylinder
		Point3D origin, direction;
		//!number of sectors (pie slices) 
		unsigned int slices;
		//!number of vertical slices (should be 1 if endradius equals start radius
		unsigned int stacks;

		//!Do we lock the drawing to only use the first radius?
		bool radiiLocked;
	public:
		//!Constructor
		DrawCylinder();
		//!Destructor
		virtual ~DrawCylinder();

		//!Set the location of the base of the cylinder
		void setOrigin(const Point3D &pt);
		//!Number of cuts perpendicular to axis - ie disks
		void setSlices(unsigned int i);
		//!Number of cuts  along axis - ie segments
		void setStacks(unsigned int i);

		//!Gets the location of the origin
		Point3D getOrigin(){ return origin;};
		//!Gets the cylinder axis direction
		Point3D getDirection(){ return direction;};
		//!Set end radius
		void setRadius(float val);
		//!get the radius
		float getRadius() const { return radius;};
		//!Set the orientation of cylinder
		void setDirection(const Point3D &pt);
		//!Set the length of cylinder
		void setLength(float len);
		//!Set the color of the cylinder
		void setColour(float r,float g,float b,float a);
	
		//!Draw the clyinder
		void draw() const;
		//!Get the bounding box that encapuslates this object
		void getBoundingBox(BoundCube &b) const ;

		//!Recompute the internal parameters using the input vector information
		void recomputeParams(const vector<Point3D> &vecs, const vector<float> &scalars, unsigned int mode);

		virtual bool needsDepthSorting() const;



		//!Lock (or unlock) the radius to the start radius (i.e. synch the two)
		void lockRadii(bool doLock=true) {radiiLocked=doLock;};
};


//FIXME: It seemed like a good idea at the time to make this a drawable. Now I don't know why.
//!Special class for holding references to other objects, where we
//need to depth sort them
class DrawDepthSorted: public DrawableObj
{
	private:
		bool haveLastDist;
		//Objects that need to be depth sorted
		std::vector<std::pair<const DrawableObj *,std::vector<DrawableObj *> > > depthObjects;

		//Vector that tells us where in the depthObjects array to go to when displaying sorted elements
		mutable vector<std::pair<size_t,size_t> > depthJumpKeys;
		
		//!camera location at last draw
		mutable Point3D lastCamLoc;
	public:
		DrawDepthSorted() { haveLastDist=false;};
		void addObjectsAsNeeded(const DrawableObj *);
		void draw() const; 
		void getBoundingBox(BoundCube &b) const { ASSERT(false);};
};

//!Drawing mode enumeration for scalar field
enum
{
	VOLUME_POINTS=0
};

//!A display list generating class
/*! This class allows for the creation of display lists for openGL objects
 *  You can use this class to create a display list which will allow you to
 *  reference cached openGL calls already stored in the video card.
 *  This can be  used to reduce the overhead in the drawing routines
 */
class DrawDispList : public DrawableObj
{
	private:
		//!Static variable for the next list number to generate
		unsigned int listNum;
		//!True if the list is active (collecting/accumulating)
		bool listActive;
		//!Bounding box of objects in display list
		BoundCube boundBox;
	public:
		//!Constructor
		DrawDispList();
		//!Destructor
		virtual ~DrawDispList();

		//!Execute the display list
		void draw() const;		

		//!Set it such that many openGL calls map to the display list.
		/*!If "execute" is true, the commands will be excuted after
		 * accumulating the display list buffer
		 * For a complete list of which calls map to the dispaly list,
		 * see the openGL spec, "Display lists"
		 */
		bool startList(bool execute);
	
		//!Add a drawable object - list MUST be active
		/* If the list is not active, this will simply exectue
		 * the draw function of the drawable
		 */
		void addDrawable(const DrawableObj *);
			
		//!Close the list - this *will* clear the gl error system
		bool endList();
		//!Get bounding cube
		void getBoundingBox(BoundCube &b) const { b=boundBox;}

};

//!A class to draw text obects using FTGL
/*May not be the best way to do this.
 * MIght be better to have static instances
 * of each possible type of font, then
 * render the text appropriately
 */
class DrawGLText : public DrawableObj
{

	private:

		//!FTGL font instance
		FTFont *font;

		//!Font string
		std::string fontString;
		//!Current font mode
		unsigned int curFontMode;
		
		//!Text string
		std::string strText;
		//!Origin of text
		Point3D origin;
		//!Alignment mode
		unsigned int alignMode;

		//!Colours for text 
		float r,g,b,a;

		//Two vectors required to define 
		//these should always give a dot prod of
		//zero

		//!Up direction for text
		Point3D up;

		//!Text flow direction
		Point3D textDir;
	
		//!Direction for which text should be legible
		/*! This will ensure that the text is legible from 
		 * the direction being pointed to by normal. It is
		 * not the true normal of the quad. as the origin and the
		 * up direction specify some of that data already
		 */
		Point3D readDir;
		
		//!True if no erro
		bool isOK;

		//!Ensure that the text is never mirrored from view direction
		bool ensureReadFromNorm;
	public:
		//!Constructor (font file & text mode)
		/* Valid font types are FTGL_x where x is
		 * 	BITMAP
		 * 	PIXMAP
		 * 	OUTLINE
		 * 	POLYGON
		 * 	EXTRUDE
		 * 	 TEXTURE
		 */
		DrawGLText(std::string fontFile,
					unsigned int ftglTextMode);

		//!Destructor
		virtual ~DrawGLText();

		//!Set the size of the text (in points (which may be GL units,
		//unsure))
		inline void setSize(unsigned int size)
			{if(isOK){font->FaceSize(size);}};
		//!Set the depth of the text (in points, may be GL units, unsure)
		inline void setDepth(unsigned int depth)
			{if(isOK){font->Depth(depth);}};
		//!Returs true if the class data is good
		inline bool ok() const
			{return isOK;};

		//!Set the text string to be displayed
		inline void setString(std::string str)
			{strText=str;};

		//!Render the text string
		void draw() const;

		//!Set the up direction for the text
		/*!Note that this must be orthogonal to
		 * the reading direction
		 */
		inline void setUp(const Point3D &p) 
			{ up=p;up.normalise();};
		//!Set the origin
		inline void setOrigin(const Point3D &p) 
			{ origin=p;};
		//!Set the reading direction
		/*!The reading direction is the direction
		 * from which the text should be legible
		 * This direction is important only if ensureReadFromNorm
		 * is set
		 */
		inline void setReadDir(const Point3D &p) 
			{ readDir=p;}; 

		//!Set the text flow direction
		/*! This *must* be orthogonal to the up vector
		 * i.e. forms a right angle with
		 */
		inline void setTextDir(const Point3D &p) 
			{textDir=p;textDir.normalise();}
		//!Return the location of the lower-left of the text
		inline Point3D getOrigin() const 
			{return origin;};

		inline void setReadFromNorm(bool b)
			{ensureReadFromNorm=b;}

		//!Set the colour (rgba) of the object
		void setColour(float r,float g,float b,float a);
		
		//!Get the bounding box for the text
		void getBoundingBox(BoundCube &b) const; 

		//!Set the text alignment (default is left)
		void setAlignment(unsigned int mode);
};




//!A class to draw rectangluar prisms
/* TODO: extend to non-axis aligned version 
 */
class DrawRectPrism  : public DrawableObj
{
	private:
		//!Drawing mode, (line or surface);
		unsigned int drawMode;
		//!Colours for prism
		float r,g,b,a;
		//!Lower left and upper right of box
		Point3D pMin, pMax;
		//!Thickness of line
		float lineWidth;
	public:
		DrawRectPrism();
		~DrawRectPrism();

		//!Draw object
		void draw() const;

		//!Set the draw mode
		void setDrawMode(unsigned int n) { drawMode=n;};
		//!Set colour of box
		void setColour(float rnew, float gnew, float bnew, float anew);
		//!Set thickness of box
		void setLineWidth(float lineWidth);
		//!Set up box as axis-aligned rectangle using two points
		void setAxisAligned(const Point3D &p1,const Point3D &p2);
		//!Set up box as axis-aligned rectangle using bounding box 
		void setAxisAligned(const BoundCube &b);

		void getBoundingBox(BoundCube &b) const;
		
		//!Recompute the internal parameters using the input vector information
		void recomputeParams(const vector<Point3D> &vecs, const vector<float> &scalars, unsigned int mode);
};

struct RGBFloat
{
	float v[3];
};

class DrawColourBarOverlay : public DrawableObj
{
	private:
		FTFont *font;

		//!Colours for each element
		vector<RGBFloat> rgb;
		//alpha (transparancy) value
		float a;
		//!Minimum and maximum values for the colour bar (for ticks)
		float min,max;
		//!Height and width of bar (total)
		float height,width;
		//!top left of bar
		float tlX,tlY;

	public:
	
		DrawColourBarOverlay();
		~DrawColourBarOverlay(){delete font;};
		void getBoundingBox(BoundCube &b) const ;

		//!This is an overlay
		bool isOverlay() const {return true;};
		void setColourVec(const vector<float> &r,
					const vector<float> &g,
					const vector<float> &b);
		//!Draw object
		void draw() const;

		void setAlpha(float alpha) { a=alpha;};
		void setSize(float widthN, float heightN) {height=heightN, width=widthN;} 
		void setPosition(float newTLX,float newTLY) { tlX=newTLX; tlY=newTLY;}
		void setMinMax(float minNew,float maxNew) { min=minNew;max=maxNew;};
		
};

//!A class to hande textures to draw
class DrawTexturedQuadOverlay : public DrawableObj
{
	private:
		TexturePool *texPool;
		unsigned int textureId;
		float position[2];
		float length;
		unsigned int winX,winY;

		bool textureOK;
	public:
		DrawTexturedQuadOverlay();
		~DrawTexturedQuadOverlay();

		//!This is an overlay
		bool isOverlay() const {return true;};
	
		void setWindowSize(unsigned int x, unsigned int y){winX=x;winY=y;};	

		//!Set the texture position in XY (z is ignored)
		void setPos(float xp,float yp){position[0]=xp; position[1]=yp;};

		void setSize(float size);

		//!Set the texture by name
		bool setTexture(const char *textureFile);

		//!Set the texture pool pointer
		void setTexturePool(TexturePool *p) { texPool =p;};

		//!Draw object
		void draw() const;

		void getBoundingBox(BoundCube &b) const ;
};


struct RGBThis
{
	unsigned char v[3];
};

//!This class allows for the visualisation of 3D scalar fields
class DrawField3D : public DrawableObj
{
	private:
		mutable std::vector<std::pair<Point3D,RGBThis> > ptsCache;
		mutable bool ptsCacheOK;
	protected:
		//!Alpha transparancy of objects in field
		float alphaVal;

		//!Size of points in the field -
		//only meaningful if the render mode is set to alpha blended points
		float pointSize;

		//!True if the scalar field's bounding box is to be drawn
		bool drawBoundBox;

		//!Colours for the bounding boxes
		float boxColourR,boxColourG,boxColourB,boxColourA;
		
		//!True if volume grid is enabled
		bool volumeGrid;

		//!Colour map lower and upper bounds
		float colourMapBound[2];

		//!Which colourmap to use
		unsigned int colourMapID;

		//!Sets the render mode for the 3D volume 
		/* Possible modes
		 * 0: Alpha blended points
		 */
		unsigned int volumeRenderMode;
		//!The scalar field - used to store data values
		const Voxels<float> *field;
	public:
		//!Default Constructor
		DrawField3D();
		//!Destructor
		virtual ~DrawField3D();


		//!Get the bounding box for this object
		void getBoundingBox(BoundCube &b) const;
		
		//!Set the render mode (see volumeRenderMode variable for details)
		void setRenderMode(unsigned int);
		
		//!Set the field pointer 
		void setField(const Voxels<float> *field); 

		//!Set the alpha value for elemnts
		void setAlpha(float alpha);

		//!Set the colour bar minima and maxima from current field values
		void setColourMinMax();

		//!Set the colourMap ID
		void setColourMapID(unsigned int i){ colourMapID=i;};

		//!Render the field
		void draw() const;

		//!Set the size of points
		void setPointSize(float size);
		
		//!Set the colours that ar ebeing used in the tempMap
		void setMapColours(unsigned int map);

		//!Set the coour of the bounding box
		void setBoxColours(float r, float g, float b, float a);

};

class DrawIsoSurface: public DrawableObj
{
private:

	mutable bool cacheOK;

	//!should we draw the thing 
	//	- in wireframe
	//	-Flat
	//	-Smooth
	//
	unsigned int drawMode;

	//!Isosurface scalar threshold
	float threshold;

	Voxels<float> *voxels;	

	mutable vector<TriangleWithVertexNorm> mesh;

	//!Warning. Although I declare this as const, I do some naughty mutating to the cache.
	void updateMesh() const;	
	
	//!Point colour (r,g,b,a) range: [0.0f,1.0f]
	float r,g,b,a;
public:

	DrawIsoSurface();
	~DrawIsoSurface();

	//!Transfer ownership of data pointer to class
	void swapVoxels(Voxels<float> *v);

	//!Set the drawing method

	//Draw
	void draw() const;

	//!Set the isosurface value
	void setScalarThresh(float thresh) { threshold=thresh;cacheOK=false;mesh.clear();};

	//!Get the bouding box (of the entire scalar field)	
	void getBoundingBox(BoundCube &b) const ;
		
	//!Sets the color of the point to be drawn
	void setColour(float rP, float gP, float bP, float alpha) { r=rP;g=gP;b=bP;a=alpha;} ;
	
	//!Do we need depth sorting?
	bool needsDepthSorting() const;
};

#ifdef HPMC_GPU_ISOSURFACE
//!A class to use GPU shaders to draw isosurfaces
// **********************************************************************
// Adapted from 
// http://www.sintef.no/Projectweb/Heterogeneous-Computing/
// Research-Topics/Marching-Cubes-using-Histogram-Pyramids/
//
// Reference:
//  High-speed Marching Cubes using Histogram Pyramids
//  Christopher Dyken, Gernot Ziegler, Christian Theobalt and Hans-Peter Seidel
//
//
// Original File: texture3d.cpp
//
// Authors: Christopher Dyken <christopher.dyken@sintef.no>
//
//Licence:
// Copyright (C) 2009 by SINTEF.  All rights reserved.
//   
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// ("GPL") version 2 as published by the Free Software Foundation.
// See the file LICENSE.GPL at the root directory of this source
// distribution for additional information about the GNU GPL.
// 
// SINTEF, Pb 124 Blindern, N-0314 Oslo, Norway
// http://www.sintef.no
// ********************************************************************/
class DrawIsoSurfaceWithShader: public DrawableObj
{
private:
	//can we use shaders?
	bool shadersOK;
	//should we draw the thing in wireframe?
	bool wireframe;
	//Pointer to data
	char *dataset;


	struct HPMCConstants* hpmc_c;
	struct HPMCHistoPyramid* hpmc_h;
	struct HPMCTraversalHandle* hpmc_th_shaded;

	//Shader handles
	GLuint shaded_v;
	GLuint shaded_f;
	GLuint shaded_p;
	//Texture handles
	GLuint volume_tex;

	//On-card volume size
	int volume_size_x;
	int volume_size_y;
	int volume_size_z;

	struct HPMCTraversalHandle* hpmc_th_flat;

	//Shader functions, for dynamic compilation
	std::string shaded_vertex_shader;
	std::string shaded_fragment_shader;
	std::string flat_vertex_shader;

	GLuint flat_v;
	GLuint flat_p;

	//Isosurface scalar threshold (true value)
	float threshold;

	//true data maximum
	float trueMax;

	//Voxel data Bounding box
	BoundCube bounds;
	
	//Compile shader for video card	
	void compileShader( GLuint shader, const std::string& what );
	//Link shader
	void linkProgram( GLuint program, const std::string& what );
public:

	DrawIsoSurfaceWithShader();
	~DrawIsoSurfaceWithShader();

	//initialise dataset and shaders
	bool init(const Voxels<float> &v);
	//Draw
	void draw() const;

	//Can the shader run?
	bool canRun() const{return shadersOK;};

	void setScalarThresh(float thresh) ;

	void getBoundingBox(BoundCube &b) const;
};
#endif

class DrawAxis : public DrawableObj
{
	private:
		//!Drawing style
		unsigned int style;
		Point3D position;
		//!size
		float size;
	public:
		DrawAxis();
		~DrawAxis();

		//!Draw object
		void draw() const;


		void setStyle(unsigned int style);
		void setSize(float newSize);
		void setPosition(const Point3D &p);

		void getBoundingBox(BoundCube &b) const;
};
#endif
