/*
 *	drawables.cpp - opengl drawable objects cpp file
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

//Do we have the HPMC Real-time on-gpu isosurface library?
//Note, this define is repeated in the header
//to avoid exposing glew.h, which complains bitterly about header orders.
//#define HPMC_GPU_ISOSURFACE 

#ifdef HPMC_GPU_ISOSURFACE
	//HPC headers
	#include <hpmc/hpmc.h>
#endif

#include "drawables.h"

#include "colourmap.h"

#include "isoSurface.h"

#include <limits>
#include "mathfuncs.h"

//OpenGL debugging macro
#if DEBUG
#include <iostream>
#include <cstdlib>
#define glError() { \
		GLenum err = glGetError(); \
		while (err != GL_NO_ERROR) { \
					fprintf(stderr, "glError: %s caught at %s:%u\n", (char *)gluErrorString(err), __FILE__, __LINE__); \
					err = glGetError(); \
				} \
		std::cerr << "glErr Clean " << __FILE__ << ":" << __LINE__ << std::endl; \
}
#else
#define glError()
#endif

//Static class variables
//====
const Camera *DrawGLText::curCamera = 0;
const Camera *DrawField3D::curCam = 0;
//==


using std::vector;

DrawableObj::DrawableObj() : active(true), wantsLight(false)
{
	canSelect=false;
}

DrawableObj::~DrawableObj()
{
}

//=====

DrawPoint::DrawPoint() : origin(0.0f,0.0f,0.0f), r(1.0f), g(1.0f), b(1.0f), a(1.0f)
{
}

DrawPoint::DrawPoint(float x, float y, float z) : origin(x,y,z), r(1.0f), g(1.0f), b(1.0f)
{
}

DrawPoint::~DrawPoint()
{
}




void DrawPoint::setColour(float rnew, float gnew, float bnew, float anew)
{
	r=rnew;
	g=gnew;
	b=bnew;
	a=anew;
}



void DrawPoint::setOrigin(const Point3D &pt)
{
	origin = pt;
}


void DrawPoint::draw() const
{
	glColor4f(r,g,b,a);
	glBegin(GL_POINT);
	glVertex3f(origin[0],origin[1],origin[2]);
	glEnd();
}

DrawVector::DrawVector() : origin(0.0f,0.0f,0.0f), vector(0.0f,0.0f,1.0f),arrowSize(1.0f),scaleArrow(true),
			r(1.0f), g(1.0f), b(1.0f), a(1.0f)
{
}

DrawVector::~DrawVector()
{
}


void DrawVector::getBoundingBox(BoundCube &b) const 
{
	b.setBounds(origin,vector+origin);
}

void DrawVector::setColour(float rnew, float gnew, float bnew, float anew)
{
	r=rnew;
	g=gnew;
	b=bnew;
	a=anew;
}

void DrawVector::setOrigin(const Point3D &pt)
{
	origin = pt;
}

void DrawVector::setVector(const Point3D &pt)
{
	vector= pt;
}

void DrawVector::draw() const
{
	glColor3f(r,g,b);
	//FIXME: Arrow head calculations?
	glBegin(GL_LINES);
		glVertex3f(origin[0],origin[1],origin[2]);
		glVertex3f(vector[0]+origin[0],vector[1]+origin[1],vector[2]+origin[2]);
	glEnd();
}

void DrawVector::recomputeParams(const std::vector<Point3D> &vecs, 
			const std::vector<float> &scalars, unsigned int mode)
{
	switch(mode)
	{
		case DRAW_VECTOR_BIND_ORIENTATION:
			ASSERT(vecs.size() ==1 && scalars.size() ==0);
			vector=vecs[0];
			break;
		case DRAW_VECTOR_BIND_ORIGIN:
			ASSERT(scalars.size() == 1 && vecs.size()==0);
			origin=vecs[0];
			break;
		default:
			ASSERT(false);
	}
}
DrawTriangle::DrawTriangle() : r(1.0f), g(1.0f),b(1.0f),a(1.0f)
{
}

DrawTriangle::~DrawTriangle()
{
}

void DrawTriangle::setVertex(unsigned int ui, const Point3D &pt)
{
	ASSERT(ui < 3);
	vertices[ui] = pt;
}

void DrawTriangle::setColour(float rnew, float gnew, float bnew, float anew)
{
	r=rnew;
	g=gnew;
	b=bnew;
	a=anew;
}

void DrawTriangle::draw() const
{
	glColor4f(r,g,b,a);
	glBegin(GL_TRIANGLES);
		glVertex3f((vertices[0])[0],
			(vertices[0])[1], (vertices[0])[2]);
		glVertex3f((vertices[1])[0],
			(vertices[1])[1], (vertices[1])[2]);
		glVertex3f((vertices[2])[0],
			(vertices[2])[1], (vertices[2])[2]);
	glEnd();
}


DrawSphere::DrawSphere() : radius(1.0f), latSegments(8),longSegments(8)
{
	q=gluNewQuadric();
}

DrawSphere::~DrawSphere()
{
	gluDeleteQuadric(q);
}



void DrawSphere::getBoundingBox(BoundCube &b) const
{
	for(unsigned int ui=0;ui<3;ui++)
	{
		b.setBound(ui,0,origin[ui] - radius);
		b.setBound(ui,1,origin[ui] + radius);
	}
}

void DrawSphere::setOrigin(const Point3D &p)
{
	origin = p;
}

void DrawSphere::setLatSegments(unsigned int ui)
{
	latSegments = ui;
}

void DrawSphere::setLongSegments(unsigned int ui)
{
	longSegments = ui;
}

void DrawSphere::setRadius(float rad)
{
	radius=rad;
}

void DrawSphere::setColour(float rnew, float gnew, float bnew, float anew)
{
	r=rnew;
	g=gnew;
	b=bnew;
	a=anew;
}

void DrawSphere::draw() const 
{
	glPushMatrix();
		glTranslatef(origin[0],origin[1],origin[2]);
		glColor4f(r,g,b,a);
		gluSphere(q,radius,latSegments,longSegments);
	glPopMatrix();
}


void DrawSphere::recomputeParams(const vector<Point3D> &vecs, 
			const vector<float> &scalars, unsigned int mode)
{
	switch(mode)
	{
		case DRAW_SPHERE_BIND_ORIGIN:
			ASSERT(vecs.size() ==1 && scalars.size() ==0);
			origin=vecs[0];
			break;
		case DRAW_SPHERE_BIND_RADIUS:
			ASSERT(scalars.size() == 1 && vecs.size()==0);
			radius=scalars[0];
			break;
		default:
			ASSERT(false);
	}
}
//===========

DrawCylinder::DrawCylinder() : radius(1.0f), 
		origin(0.0f,0.0f,0.0f), direction(0.0f,0.0f,1.0f), slices(4),stacks(4)
{
	q= gluNewQuadric();
	qCap[0]= gluNewQuadric();
	gluQuadricOrientation(qCap[0],GLU_INSIDE);	
	qCap[1]= gluNewQuadric();
	gluQuadricOrientation(qCap[1],GLU_OUTSIDE);	
	radiiLocked=false;
}

DrawCylinder::~DrawCylinder()
{
	gluDeleteQuadric(q);
	gluDeleteQuadric(qCap[0]);
	gluDeleteQuadric(qCap[1]);
}


void DrawCylinder::setOrigin(const Point3D& pt)
{
	origin=pt;
}


void DrawCylinder::setDirection(const Point3D &p)
{
	direction=p;
}


void DrawCylinder::draw() const
{
	//Cross product desired drection with default
	//direction to produce rotation vector
	Point3D dir(0.0f,0.0f,1.0f);

	glPushMatrix();
	glTranslatef(origin[0],origin[1],origin[2]);

	Point3D dirNormal(direction);
	dirNormal.normalise();

	float angle = dir.angle(dirNormal);
	dir = dir.crossProd(dirNormal);
	float length=sqrtf(direction.sqrMag());

	glRotatef(angle*180.0f/M_PI,dir[0],dir[1],dir[2]);

	//OpenGL defined cylinder starting at 0 and going to lenght. I want it starting at 0 and going to+-l/2
	glTranslatef(0,0,-length/2.0f);
	glColor4f(r,g,b,a);
	
	//Draw the end cap at z=0
	if(radiiLocked)
	{
		gluDisk(qCap[0],0,radius,slices,1);
		gluCylinder(q,radius,radius, length,slices,stacks);

		//Draw the start cap at z=l	
		glTranslatef(0,0,length);
		gluDisk(qCap[1],0,radius,slices,1);
	}
	else
	{
		gluDisk(qCap[0],0,radius,slices,1);
		gluCylinder(q,radius,radius, length,slices,stacks);

		//Draw the end cap at z=l	
		glTranslatef(0,0,length);
		gluDisk(qCap[1],0,radius,slices,1);
	}

	glPopMatrix();
}

void DrawCylinder::setSlices(unsigned int i)
{
	slices=i;
}

void DrawCylinder::setStacks(unsigned int i)
{
	stacks=i;
}

void DrawCylinder::setRadius(float rad)
{
	radius=rad;
}

void DrawCylinder::recomputeParams(const vector<Point3D> &vecs, 
			const vector<float> &scalars, unsigned int mode)
{
	switch(mode)
	{
		case DRAW_CYLINDER_BIND_ORIGIN:
			ASSERT(vecs.size() ==1 && scalars.size() ==0);
			origin=vecs[0];
			break;

		case DRAW_CYLINDER_BIND_DIRECTION:
			ASSERT(vecs.size() ==1 && scalars.size() ==0);
			direction=vecs[0];
			break;
		case DRAW_CYLINDER_BIND_RADIUS:
			ASSERT(scalars.size() == 1 && vecs.size()==0);
			radius=scalars[0];
			break;
		default:
			ASSERT(false);
	}
}


void DrawCylinder::setLength(float len)
{
	direction=direction.normalise()*len;
}

void DrawCylinder::setColour(float rnew, float gnew, float bnew, float anew)
{
	r=rnew;
	g=gnew;
	b=bnew;
	a=anew;
}

void DrawCylinder::getBoundingBox(BoundCube &b) const
{

	float tmp;

	Point3D normAxis(direction);
	normAxis.normalise();
	
	//Height offset for ending circles. 
	//The joint bounding box of these two is the 
	//overall bounding box
	Point3D offset;



	//X component
	tmp=sin(acos(normAxis.dotProd(Point3D(1,0,0))));
	offset[0] = radius*tmp;

	//Y component
	tmp=sin(acos(normAxis.dotProd(Point3D(0,1,0))));
	offset[1] = radius*tmp;

	//Z component
	tmp=sin(acos(normAxis.dotProd(Point3D(0,0,1))));
	offset[2] = radius*tmp;


	vector<Point3D> p;
	p.resize(4);
	p[0]= offset+direction*0.5+origin;
	p[1]= -offset+direction*0.5+origin;
	p[2]= offset-direction*0.5+origin;
	p[3]= -offset-direction*0.5+origin;


	b.setBounds(p);
}


//======

DrawManyPoints::DrawManyPoints() : r(1.0f),g(1.0f),b(1.0f),a(1.0f), size(1.0f)
{
	wantsLight=false;
}

DrawManyPoints::~DrawManyPoints() 
{
	wantsLight=false;
	haveCachedBounds=false;
}

void DrawManyPoints::getBoundingBox(BoundCube &b) const
{

	//Update the cache as needed
	if(!haveCachedBounds)
	{
		haveCachedBounds=true;
		cachedBounds.setBounds(pts);
	}

	b=cachedBounds;
	return;
}


void DrawManyPoints::clear()
{
	pts.clear();
}

void DrawManyPoints::addPoints(const vector<Point3D> &vp)
{
	pts.reserve(pts.size()+vp.size());
	std::copy(vp.begin(),vp.end(),pts.begin());
}

/*
void DrawManyPoints::addPoints(const vector<IonHit> &vp)
{
	pts.reserve(pts.size()+vp.size());
	for(size_t ui=0; ui<vp.size(); ui++)
		pts.push_back(vp[ui].getPos());
	haveCachedBounds=false;
}
*/
void DrawManyPoints::shuffle()
{
	std::random_shuffle(pts.begin(),pts.end());
}


void DrawManyPoints::addPoint(const Point3D &p)
{
	pts.push_back(p);
	haveCachedBounds=false;
}

void DrawManyPoints::setColour(float rnew, float gnew, float bnew, float anew)
{
	r=rnew;
	g=gnew;
	b=bnew;
	a=anew;
}

void DrawManyPoints::setSize(float f)
{
	size=f;
}

void DrawManyPoints::draw() const
{
	const Point3D *p;
	glPointSize(size); 
	glBegin(GL_POINTS);
		glColor4f(r,g,b,a);
		//TODO: Consider Vertex buffer objects. would be faster, but less portable.
		for(unsigned int ui=0; ui<pts.size(); ui++)
		{
			p=&(pts[ui]);
			glVertex3fv(p->getValueArr());
		}
	glEnd();
}

//======

DrawDispList::DrawDispList() : listNum(0),listActive(false)
{
}

DrawDispList::~DrawDispList()
{
	if(listNum)
	{
		ASSERT(!listActive);
		ASSERT(glIsList(listNum));
		glDeleteLists(listNum,1);
	}

}

bool DrawDispList::startList(bool execute)
{
	//Ensure that the user has appropriately closed the list
	ASSERT(!listActive);
	boundBox.setInverseLimits();
	
	//If the list is already genned, clear it
	if(listNum)
		glDeleteLists(listNum,1);

	//Create the display list (ask for one)
	listNum=glGenLists(1);

	if(listNum)
	{
		glNewList(listNum,GL_COMPILE);
		listActive=true;
	}
	return (listNum!=0);
}

void DrawDispList::addDrawable(const DrawableObj *d)
{
	ASSERT(listActive);
	BoundCube b;
	d->getBoundingBox(b);
	boundBox.expand(b);
	d->draw();
}

bool DrawDispList::endList()
{
	glEndList();

	ASSERT(boundBox.isValid());
	listActive=false;	
	return (glGetError() ==0);
}

void DrawDispList::draw() const
{
	ASSERT(!listActive);

	//Cannot select display list objects,
	//as we cannot modify them without a "do-over".
	ASSERT(!canSelect);

	ASSERT(glIsList(listNum));
	//Execute the list	
	glPushMatrix();
	glCallList(listNum);
	glPopMatrix();
}

//========


DrawGLText::DrawGLText(std::string fontFile, unsigned int mode) : curFontMode(mode), origin(0.0f,0.0f,0.0f), r(0.0),g(0.0),b(0.0),a(1.0), up(0.0f,1.0f,0.0f),  textDir(1.0f,0.0f,0.0f), readDir(0.0f,0.0f,1.0f), isOK(true),ensureReadFromNorm(true) 
{
	fontString = fontFile;

	font=0;
	switch(mode)
	{
		case FTGL_BITMAP:
			font = new FTGLBitmapFont(fontFile.c_str());
			break;
		case FTGL_PIXMAP:
			font = new FTGLPixmapFont(fontFile.c_str());
			break;
		case FTGL_OUTLINE:
			font = new FTGLOutlineFont(fontFile.c_str());
			break;
		case FTGL_POLYGON:
			font = new FTGLPolygonFont(fontFile.c_str());
			break;
		case FTGL_EXTRUDE:
			font = new FTGLExtrdFont(fontFile.c_str());
			break;
		case FTGL_TEXTURE:
			font = new FTGLTextureFont(fontFile.c_str());
			break;
		default:
			//Don't do this. Use valid font numbers
			ASSERT(false);
			font=0; 
	}

	//In case of allocation failure or invalid font num
	if(!font || font->Error())
	{
		isOK=false;
		return;
	}

	//Try to make it 100 point
	font->FaceSize(5);
	font->Depth(20);

	//Use unicode
	font->CharMap(ft_encoding_unicode);

	alignMode = DRAWTEXT_ALIGN_LEFT;
}

void DrawGLText::draw() const
{
	if(!isOK)
		return;

	//Translate the drawing position to the origin
	Point3D offsetVec=textDir;
	float advance=font->Advance(strText.c_str());
	switch(alignMode)
	{
		case DRAWTEXT_ALIGN_LEFT:
			break;
		case DRAWTEXT_ALIGN_CENTRE:
			offsetVec=offsetVec*advance/2.0f;
			break;
		case DRAWTEXT_ALIGN_RIGHT:
			offsetVec=offsetVec*advance;
			break;
		default:
			ASSERT(false);
	}
	

	glPushMatrix();

	if(curFontMode !=FTGL_BITMAP)
	{
		offsetVec=origin-offsetVec;
		glTranslatef(offsetVec[0],offsetVec[1],offsetVec[2]);

		//Rotate such that the new X-Y plane is set to the
		//desired text orientation. (ie. we want to draw the text in the 
		//specified combination of updir-textdir, rather than in the X-y plane)

		//---	
		//Textdir and updir MUST be normal to one another
		ASSERT(textDir.dotProd(up) < sqrtf(std::numeric_limits<float>::epsilon()));

		//rotate around textdir cross X, if the two are not the same
		Point3D rotateAxis;
		Point3D newUp=up;
		float angle=textDir.angle(Point3D(1,0,0) );
		if(angle > sqrtf(std::numeric_limits<float>::epsilon()))
		{
			rotateAxis = textDir.crossProd(Point3D(-1,0,0));
			rotateAxis.normalise();
			
			Point3f tmp,axis;
			tmp.fx=up[0];
			tmp.fy=up[1];
			tmp.fz=up[2];

			axis.fx=rotateAxis[0];
			axis.fy=rotateAxis[1];
			axis.fz=rotateAxis[2];


			glRotatef(angle*180/M_PI,rotateAxis[0],rotateAxis[1],rotateAxis[2]);
			quat_rot(&tmp,&axis,angle*180/M_PI);

			newUp[0]=tmp.fx;
			newUp[1]=tmp.fy;
			newUp[2]=tmp.fz;
		}

		//rotate new up direction into y around x axis
		angle = newUp.angle(Point3D(0,1,0));
		if(angle > sqrtf(std::numeric_limits<float>::epsilon()))
		{
			rotateAxis = newUp.crossProd(Point3D(0,-1,0));
			rotateAxis.normalise();
			glRotatef(angle*180/M_PI,rotateAxis[0],rotateAxis[1],rotateAxis[2]);
		}

		//Ensure that the text is not back-culled (i.e. if the
		//text normal is pointing away from the camera, it does not
		//get drawn). Here we have to flip the normal, by spinning the 
		//text by 180 around its up direction (which has been modified
		//by above code to coincide with the y axis.
		if(curCamera)
		{
			//This is not *quite* right in perspective mode
			//but is right in orthogonal

			Point3D textNormal,camVec;
			textNormal = up.crossProd(textDir);
			textNormal.normalise();

			camVec = origin - curCamera->getOrigin();
		
			camVec.normalise();

			if(camVec.dotProd(textNormal) < 0)
			{
				//move halfway along text, noting that 
				//the text direction is now the x-axis
				glTranslatef(advance/2.0f,0,0);
				//spin text around its up direction 180 degrees
				glRotatef(180,0,1,0);
				//restore back to original position
				glTranslatef(-advance/2.0f,0,0);
			}
		
			camVec=curCamera->getUpDirection();	
			if(camVec.dotProd(up) < 0)
			{
				//move halfway along text, noting that 
				//the text direction is now the x-axis
				glTranslatef(advance/2.0f,0,0);
				//spin text around its front direction 180 degrees
				//no need to trnaslate as text sits at its baseline
				glRotatef(180,0,0,1);
				//move halfway along text, noting that 
				//the text direction is now the x-axis
				glTranslatef(-advance/2.0f,0,0);
			}

		}
	}
	else
	{
		//FIXME: The text ends up in a wierd location
		//2D coordinate storage for bitmap text
		double xWin,yWin,zWin;
		//Compute the 2D coordinates
		double model_view[16];
		glGetDoublev(GL_MODELVIEW_MATRIX, model_view);

		double projection[16];
		glGetDoublev(GL_PROJECTION_MATRIX, projection);

		int viewport[4];
		glGetIntegerv(GL_VIEWPORT, viewport);

		//Apply the openGL coordinate transformation pipleine to the
		//specified coords
		gluProject(offsetVec[0],offsetVec[1],offsetVec[2]
				,model_view,projection,viewport,
					&xWin,&yWin,&zWin);

		glRasterPos3f(xWin,yWin,zWin);

	}
	//---

	//Draw text
	if(curFontMode == FTGL_TEXTURE)
	{
		glPushAttrib(GL_ENABLE_BIT);
		glEnable(GL_TEXTURE_2D);
	
		font->Render(strText.c_str());
		glPopAttrib();
	}
	else
		font->Render(strText.c_str());

	
	glPopMatrix();
}

DrawGLText::~DrawGLText()
{
	if(font)
		delete font;
}

void DrawGLText::setColour(float rnew, float gnew, float bnew, float anew)
{
	r=rnew;
	g=gnew;
	b=bnew;
	a=anew;
}

void DrawGLText::getBoundingBox(BoundCube &b) const
{

	if(isOK)
	{
		float minX,minY,minZ;
		float maxX,maxY,maxZ;
		font->BBox(strText.c_str(),minX,minY,minZ,maxX,maxY,maxZ);
		b.setBounds(minX+origin[0],minY+origin[1],minZ+origin[2],
				maxX+origin[0],maxY+origin[1],maxZ+origin[2]);
	}
	else
		b.setInverseLimits();	
	
}

void DrawGLText::setAlignment(unsigned int newMode)
{
	ASSERT(newMode < DRAWTEXT_ALIGN_ENUM_END);
	alignMode=newMode;
}

void DrawGLText::setCurCamera(const Camera *p)
{
	curCamera=p;
}


DrawRectPrism::DrawRectPrism()
{
	r=g=b=a=1.0f;
	drawMode=DRAW_WIREFRAME;
	lineWidth=1.0f;
}
DrawRectPrism::~DrawRectPrism()
{
}

void DrawRectPrism::getBoundingBox(BoundCube &b) const
{
	b.setBounds(pMin[0],pMin[1],pMin[2],
			pMax[0],pMax[1],pMax[2]);
}

void DrawRectPrism::draw() const
{
	ASSERT(r <=1.0f && g<=1.0f && b <=1.0f && a <=1.0f);
	ASSERT(r >=0.0f && g>=0.0f && b >=0.0f && a >=0.0f);

	if(!active)
		return;
 
	switch(drawMode)
	{
		case DRAW_WIREFRAME:
		{
			//TODO: Could speedup with LINE_STRIP/LOOP. This is 
			//not a bottleneck atm though.
			glLineWidth(lineWidth);	
			glBegin(GL_LINES);
				glColor4f(r,g,b,a);
				//Bottom corner out (three lines from corner)
				glVertex3f(pMin[0],pMin[1],pMin[2]);
				glVertex3f(pMax[0],pMin[1],pMin[2]);
				
				glVertex3f(pMin[0],pMin[1],pMin[2]);
				glVertex3f(pMin[0],pMax[1],pMin[2]);

				glVertex3f(pMin[0],pMin[1],pMin[2]);
				glVertex3f(pMin[0],pMin[1],pMax[2]);
				
				//Top Corner out (three lines from corner)
				glVertex3f(pMax[0],pMax[1],pMax[2]);
				glVertex3f(pMin[0],pMax[1],pMax[2]);
			
				glVertex3f(pMax[0],pMax[1],pMax[2]);
				glVertex3f(pMax[0],pMin[1],pMax[2]);
				
				glVertex3f(pMax[0],pMax[1],pMax[2]);
				glVertex3f(pMax[0],pMax[1],pMin[2]);

				//Missing pieces - in an "across-down-across" shape
				glVertex3f(pMin[0],pMax[1],pMin[2]);
				glVertex3f(pMax[0],pMax[1],pMin[2]);
				
				glVertex3f(pMax[0],pMax[1],pMin[2]);
				glVertex3f(pMax[0],pMin[1],pMin[2]);

				glVertex3f(pMax[0],pMin[1],pMin[2]);
				glVertex3f(pMax[0],pMin[1],pMax[2]);
				
				glVertex3f(pMax[0],pMin[1],pMax[2]);
				glVertex3f(pMin[0],pMin[1],pMax[2]);
				
				glVertex3f(pMin[0],pMin[1],pMax[2]);
				glVertex3f(pMin[0],pMax[1],pMax[2]);

				glVertex3f(pMin[0],pMax[1],pMax[2]);
				glVertex3f(pMin[0],pMax[1],pMin[2]);
			glEnd();
			break;
		}
		case DRAW_FLAT:
		{
			glBegin(GL_QUADS);
				glColor4f(r,g,b,a);
			
				glNormal3f(0,0,-1);
				//Along the bottom
				glVertex3f(pMin[0],pMin[1],pMin[2]);
				glVertex3f(pMin[0],pMax[1],pMin[2]);
				glVertex3f(pMax[0],pMax[1],pMin[2]);
				glVertex3f(pMax[0],pMin[1],pMin[2]);
				//Up the side
				glNormal3f(1,0,0);
				glVertex3f(pMax[0],pMax[1],pMax[2]);
				glVertex3f(pMax[0],pMin[1],pMax[2]);
				glVertex3f(pMax[0],pMin[1],pMin[2]);
				glVertex3f(pMax[0],pMax[1],pMin[2]);
				//Over the top
				glNormal3f(0,0,1);
				glVertex3f(pMax[0],pMin[1],pMax[2]);
				glVertex3f(pMax[0],pMax[1],pMax[2]);
				glVertex3f(pMin[0],pMax[1],pMax[2]);
				glVertex3f(pMin[0],pMin[1],pMax[2]);

				//and back down
				glNormal3f(-1,0,0);
				glVertex3f(pMin[0],pMax[1],pMin[2]);
				glVertex3f(pMin[0],pMin[1],pMin[2]);
				glVertex3f(pMin[0],pMin[1],pMax[2]);
				glVertex3f(pMin[0],pMax[1],pMax[2]);

				//Now the other two sides
				glNormal3f(0,-1,0);
				glVertex3f(pMax[0],pMin[1],pMax[2]);
				glVertex3f(pMin[0],pMin[1],pMax[2]);
				glVertex3f(pMin[0],pMin[1],pMin[2]);
				glVertex3f(pMax[0],pMin[1],pMin[2]);
				
				glNormal3f(0,1,0);
				glVertex3f(pMax[0],pMax[1],pMax[2]);
				glVertex3f(pMax[0],pMax[1],pMin[2]);
				glVertex3f(pMin[0],pMax[1],pMin[2]);
				glVertex3f(pMin[0],pMax[1],pMax[2]);

			glEnd();

			break;

		}
		default:
			ASSERT(false);
	}		


}

void DrawRectPrism::setAxisAligned( const Point3D &p1, const Point3D &p2)
{
	for(unsigned int ui=0; ui<3; ui++)
	{
		pMin[ui]=std::min(p1[ui],p2[ui]);
		pMax[ui]=std::max(p1[ui],p2[ui]);
	}

}

void DrawRectPrism::setAxisAligned( const BoundCube &b)
{
	b.getBounds(pMin,pMax);
}

void DrawRectPrism::setColour(float rnew, float gnew, float bnew, float anew)
{
	r=rnew;
	g=gnew;
	b=bnew;
	a=anew;
}

void DrawRectPrism::setLineWidth(float newLineWidth)
{
	ASSERT(newLineWidth > 0.0f);
	lineWidth=newLineWidth;
}

void DrawRectPrism::recomputeParams(const vector<Point3D> &vecs, 
			const vector<float> &scalars, unsigned int mode)
{
	switch(mode)
	{
		case DRAW_RECT_BIND_TRANSLATE:
		{
			ASSERT(vecs.size() ==1);
			Point3D delta;
			delta = (pMax - pMin)*0.5;
			//Object has been translated
			pMin = vecs[0]-delta;
			pMax = vecs[0]+delta;
			break;
		}
		case DRAW_RECT_BIND_CORNER_MOVE:
		{
			ASSERT(vecs.size() ==1);
			//Delta has changed, but origin shoudl stay the same
			Point3D mean, corner;
			mean  = (pMin + pMax)*0.5;

			//Prevent negative offset values, otherwise we can
			//get inside out boxes
			corner=vecs[0];
			for(unsigned int ui=0;ui<3;ui++)
				corner[ui]= fabs(corner[ui]);

			pMin = mean-corner;
			pMax = mean+corner;
			break;
		}
		default:
			ASSERT(false);
	}
}

DrawTexturedQuadOverlay::DrawTexturedQuadOverlay()
{
	texPool=0;
}

DrawTexturedQuadOverlay::~DrawTexturedQuadOverlay()
{
	textureOK=false;
}

void DrawTexturedQuadOverlay::setSize(float s)
{
	length=s;
}


void DrawTexturedQuadOverlay::draw() const
{
	if(!textureOK)
		return;

	ASSERT(glIsTexture(textureId));
	
	glMatrixMode(GL_PROJECTION);	
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0, winX, winY, 0);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D,textureId);
	
	// Draw overlay quad 
	glColor3f(1.0f,1.0f,1.0f);
	glBegin(GL_QUADS);
		glTexCoord2f(0.0f,0.0f);
		glVertex3f(position[0]-length/2.0,position[1]-length/2.0,0.0);
		glTexCoord2f(0.0f,1.0f);
		glVertex3f(position[0]-length/2.0,position[1]+length/2.0,0.0);
		glTexCoord2f(1.0f,1.0f);
		glVertex3f(position[0]+length/2.0,position[1]+length/2.0,0.0);
		glTexCoord2f(1.0f,0.0f);
		glVertex3f(position[0]+length/2.0,position[1]-length/2.0,0.0);
	glEnd();

	glDisable(GL_TEXTURE_2D);	
	/* draw stuff */

	glPopMatrix(); //Pop modelview matrix

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);
}


bool DrawTexturedQuadOverlay::setTexture(const char *textureFile)
{
	ASSERT(texPool);	
	unsigned int dummy;

	textureOK= texPool->openTexture(textureFile,textureId,dummy);
	return textureOK;
}

void DrawTexturedQuadOverlay::getBoundingBox(BoundCube &b) const
{
	b.setInvalid();
}


DrawColourBarOverlay::DrawColourBarOverlay() 
{
	a=1.0;
	string f;
	f=getDefaultFontFile();
	font = new FTGLPolygonFont(f.c_str());
};


void DrawColourBarOverlay::draw() const
{
	//Draw quads
	float elemHeight;
	//80% of bar width is for the actual colour bar itself.
	float barWidth=0.8*width;
	elemHeight=height/(float)rgb.size();
	glBegin(GL_QUADS);
	for(unsigned int ui=0;ui<rgb.size();ui++)
	{
		//Set the quad colour for bar element
		glColor4f(rgb[rgb.size()-(ui+1)].v[0],
				rgb[rgb.size()-(ui+1)].v[1],
				rgb[rgb.size()-(ui+1)].v[2],1.0);

		//draw this quad (bar element)
		glVertex3f(tlX,tlY+(float)ui*elemHeight,0);
		glVertex3f(tlX,tlY+(float)(ui+1)*elemHeight,0);
		glVertex3f(tlX+barWidth,tlY+(float)(ui+1)*elemHeight,0);
		glVertex3f(tlX+barWidth,tlY+(float)(ui)*elemHeight,0);
	}

	glEnd();

	//Draw ticks on colour bar
	glBegin(GL_LINES);
		glColor4f(1.0,1.0,1.0f,1.0f);
		//Top tick
		glVertex3f(tlX,tlY,0);
		glVertex3f(tlX+width,tlY,0);
		//Bottom tick
		glVertex3f(tlX,tlY+height,0);
		glVertex3f(tlX+width,tlY+height,0);
	glEnd();




	if(!font || font->Error())
	{
#ifdef DEBUG
		std::cerr << "Ah bugger. No font!" << std::endl;
#endif
		return;
	}



	//FTGL units are a pain; The devs could not decide
	//whether to implement them in opengl coords or real coords
	//so they did neither, and implemented them in "points".
	//here we assume that we can transform 1 ftgl unit
	//to 1 opengl unit by inversion 
	const float FTGL_DEFAULT_UNIT_SCALE=1.0/72.0;

	glColor3f(1.0f,1.0f,1.0f);	
	font->FaceSize(3);
	glDisable(GL_CULL_FACE);
	glPushMatrix();
	glTranslatef(tlX+width,tlY,0);
	string s;
	stream_cast(s,max);
	//Note negative sign to flip from y-down screen (opengl) to text dir
	//(y up)
	glScaled(FTGL_DEFAULT_UNIT_SCALE,
			-FTGL_DEFAULT_UNIT_SCALE,FTGL_DEFAULT_UNIT_SCALE);
	font->Render(s.c_str());
	glPopMatrix();

	glPushMatrix();
	glTranslatef(tlX+width,tlY+height,0);
	stream_cast(s,min);
	//Note negative sign to flip from y-down screen (opengl) to text dir
	//(y up)
	glScaled(FTGL_DEFAULT_UNIT_SCALE,
			-FTGL_DEFAULT_UNIT_SCALE,FTGL_DEFAULT_UNIT_SCALE);
	font->Render(s.c_str());
	glPopMatrix();
	glEnable(GL_CULL_FACE);



}

void DrawColourBarOverlay::setColourVec(const vector<float> &r,
					const vector<float> &g,
					const vector<float> &b)
{
	ASSERT(r.size() == g.size());
	ASSERT(g.size() == b.size());
	rgb.resize(r.size());
	for(unsigned int ui=0;ui<r.size();ui++)
	{
		rgb[ui].v[0]=r[ui];
		rgb[ui].v[1]=g[ui];
		rgb[ui].v[2]=b[ui];
	}


}

void DrawColourBarOverlay::getBoundingBox(BoundCube &b) const
{
	b.setInvalid();
}


DrawField3D::DrawField3D() : alphaVal(0.2f), pointSize(1.0f), drawBoundBox(true),volumeGrid(false), volumeRenderMode(0), field(0) 
{
	boxColour.r = boxColour.g = boxColour.b = boxColour.a = 1.0f;
	ptsCacheOK=false;
}

DrawField3D::~DrawField3D()
{
	if(field)
		delete field;
}


void DrawField3D::getBoundingBox(BoundCube &b) const
{
	ASSERT(field)
	b.setBounds(field->getMinBounds(),field->getMaxBounds());
}


void DrawField3D::setField(const Voxels<float> *newField)
{
	field=newField;
}

void DrawField3D::setRenderMode(unsigned int mode)
{
	volumeRenderMode=mode;
}

void DrawField3D::setColourMinMax()
{
	colourMapBound[0]=field->min();
	colourMapBound[1]=field->max();

	ASSERT(colourMapBound[0] <=colourMapBound[1]);
}

			
void DrawField3D::draw() const
{

	ASSERT(field);

	//Depend upon the render mode
	switch(volumeRenderMode)
	{
		case VOLUME_POINTS:
		{
			size_t fieldSizeX,fieldSizeY,fieldSizeZ;
			Point3D p;

			field->getSize(fieldSizeX,fieldSizeY, fieldSizeZ);

			//We need to generate some points, then sort them by distance
			//from eye (back to front), otherwise they will not blend properly
			std::vector<std::pair<float,unsigned int >  > eyeDists;

			Point3D camOrigin = curCam->getOrigin();

			if(!ptsCacheOK)
			{
				ptsCache.clear();
				for(unsigned int uiX=0; uiX<fieldSizeX; uiX++)
				{
					for(unsigned int uiY=0; uiY<fieldSizeY; uiY++)
					{
						for(unsigned int uiZ=0; uiZ<fieldSizeZ; uiZ++)
						{
							float v;
							v=field->getData(uiX,uiY,uiZ);
							if(v > std::numeric_limits<float>::epsilon())
							{
								RGBThis rgb;
								//Set colour and point loc
								colourMapWrap(colourMapID,rgb.v,
										field->getData(uiX,uiY,uiZ), 
										colourMapBound[0],colourMapBound[1]);
								
								ptsCache.push_back(make_pair(field->getPoint(uiX,uiY,uiZ),rgb));
							}
						}
					}
				}
					

				ptsCacheOK=true;
			}
			eyeDists.resize(ptsCache.size());
			
			//Set up an original index for the eye distances
			#pragma omp parallel for
			for(unsigned int ui=0;ui<ptsCache.size();ui++)
			{
				eyeDists[ui].first=ptsCache[ui].first.sqrDist(camOrigin);
				eyeDists[ui].second=ui;
			}

			ComparePairFirstReverse cmp;
			std::sort(eyeDists.begin(),eyeDists.end(),cmp);	

			//render each element in the field as a point
			//the colour of the point is determined by its scalar value
			glPointSize(pointSize);
			glBegin(GL_POINTS);
			for(unsigned int ui=0;ui<ptsCache.size();ui++)
			{
				unsigned int idx;
				idx=eyeDists[ui].second;
				//Tell openGL about it
				glColor4f(((float)(ptsCache[idx].second.v[0]))/255.0f, 
						((float)(ptsCache[idx].second.v[1]))/255.0f,
						((float)(ptsCache[idx].second.v[2]))/255.0f, 
						alphaVal);
				glVertex3f(ptsCache[idx].first[0],ptsCache[idx].first[1],ptsCache[idx].first[2]);
			}
			glEnd();
			break;
		}

		default:
			//Not implemented
			ASSERT(false); 
	}

	//Draw the bounding box as required
	if(drawBoundBox)
	{
		Point3D pMin,pMax;
		pMin=field->getMinBounds();
		pMax=field->getMaxBounds();
	

		glColor4f(boxColour.r, boxColour.g,boxColour.b,boxColour.a);
		//Draw lines between field min and max
		glBegin(GL_LINES);
			//Bottom corner out
			glVertex3f(pMin[0],pMin[1],pMin[2]);
			glVertex3f(pMax[0],pMin[1],pMin[2]);
			
			glVertex3f(pMin[0],pMin[1],pMin[2]);
			glVertex3f(pMin[0],pMax[1],pMin[2]);

			glVertex3f(pMin[0],pMin[1],pMin[2]);
			glVertex3f(pMin[0],pMin[1],pMax[2]);
			
			//Top Corner out
			glVertex3f(pMax[0],pMax[1],pMax[2]);
			glVertex3f(pMin[0],pMax[1],pMax[2]);
		
			glVertex3f(pMax[0],pMax[1],pMax[2]);
			glVertex3f(pMax[0],pMin[1],pMax[2]);
			
			glVertex3f(pMax[0],pMax[1],pMax[2]);
			glVertex3f(pMax[0],pMax[1],pMin[2]);

			//Missing pieces - in a across down across shape
			glVertex3f(pMin[0],pMax[1],pMin[2]);
			glVertex3f(pMax[0],pMax[1],pMin[2]);
			
			glVertex3f(pMax[0],pMax[1],pMin[2]);
			glVertex3f(pMax[0],pMin[1],pMin[2]);

			glVertex3f(pMax[0],pMin[1],pMin[2]);
			glVertex3f(pMax[0],pMin[1],pMax[2]);
			
			glVertex3f(pMax[0],pMin[1],pMax[2]);
			glVertex3f(pMin[0],pMin[1],pMax[2]);
			
			glVertex3f(pMin[0],pMin[1],pMax[2]);
			glVertex3f(pMin[0],pMax[1],pMax[2]);

			glVertex3f(pMin[0],pMax[1],pMax[2]);
			glVertex3f(pMin[0],pMax[1],pMin[2]);
		glEnd();
	}
	//Draw the projections
}

void DrawField3D::setAlpha(float newAlpha)
{
	alphaVal=newAlpha;
}

void DrawField3D::setPointSize(float size)
{
	pointSize=size;
}

void DrawField3D::setMapColours(unsigned int mapID)
{
	ASSERT(mapID < NUM_COLOURMAPS);
	colourMapID= mapID;
}

void DrawField3D::setBoxColours(float rNew, float gNew, float bNew, float aNew)
{
	boxColour.r = rNew;
	boxColour.g = gNew;
	boxColour.b = bNew;
	boxColour.a = aNew;

}


DrawIsoSurface::DrawIsoSurface()
{
	cacheOK=false;
	drawMode=DRAW_SMOOTH;
	threshold=0.5;
	voxels=0;
	
	r=0.5;
	g=0.5;
	b=0.5;
	a=1.0;	
}

DrawIsoSurface::~DrawIsoSurface()
{
	if(voxels)
		delete voxels;
}

void DrawIsoSurface::swapVoxels(Voxels<float> *f)
{
	std::swap(f,voxels);
	cacheOK=false;
	mesh.clear();
}


void DrawIsoSurface::updateMesh() const
{

	mesh.clear();
	marchingCubes(*voxels, threshold,mesh);

	cacheOK=true;

}

void DrawIsoSurface::getBoundingBox(BoundCube &b) const
{
	if(voxels)
	{
		b.setBounds(voxels->getMinBounds(),
				voxels->getMaxBounds());
	}
	else
		b.setInverseLimits();
}

void DrawIsoSurface::draw() const
{

	if(!cacheOK)
	{
		//Hmm, we don't have a cached copy of the isosurface mesh.
		//we will need to compute one, it would seem.
		updateMesh();
	}

	//This could be optimised by using triangle strips
	//rather than direct triangles.
	glColor4f(r,g,b,a);
	glPushAttrib(GL_CULL_FACE);
	glDisable(GL_CULL_FACE);	
	glBegin(GL_TRIANGLES);	
	for(unsigned int ui=0;ui<mesh.size();ui++)
	{
		glNormal3fv(mesh[ui].normal[0].getValueArr());
		glVertex3fv(mesh[ui].p[0].getValueArr());
		glNormal3fv(mesh[ui].normal[1].getValueArr());
		glVertex3fv(mesh[ui].p[1].getValueArr()),
		glNormal3fv(mesh[ui].normal[2].getValueArr());
		glVertex3fv(mesh[ui].p[2].getValueArr());
	}
	glPopAttrib();
	glEnd();
}


#ifdef HPMC_GPU_ISOSURFACE

//HPMC on GPU Isosurface code is GPL
// See class definition for licence 


DrawIsoSurfaceWithShader::DrawIsoSurfaceWithShader()
{
	dataset=0;
	shadersOK=false;
	wireframe=false;
		
	//TODO: Make this static, once-only?? Have some global drawinit code?
	GLenum err = glewInit();
	shadersOK= (GLEW_OK == err) &&(GLEW_ARB_vertex_program);
	
	//Create shaders
	 shaded_vertex_shader =
		"varying vec3 normal;\n"
		"void\n"
		"main()\n"
		"{\n"
		"    vec3 p, n;\n"
		"    extractVertex( p, n );\n"
		"    gl_Position = gl_ModelViewProjectionMatrix * vec4( p, 1.0 );\n"
		"    normal = gl_NormalMatrix * n;\n"
		"    gl_FrontColor = gl_Color;\n"
		"}\n";
	 shaded_fragment_shader =
		"varying vec3 normal;\n"
		"void\n"
		"main()\n"
		"{\n"
		"    const vec3 v = vec3( 0.0, 0.0, 1.0 );\n"
		"    vec3 l = normalize( vec3( 1.0, 1.0, 1.0 ) );\n"
		"    vec3 h = normalize( v+l );\n"
		"    vec3 n = normalize( normal );\n"
		"    float diff = max( 0.1, dot( n, l ) );\n"
		"    float spec = pow( max( 0.0, dot(n, h)), 20.0);\n"
		"    gl_FragColor = diff * gl_Color +\n"
		"                   spec * vec4(1.0);\n"
		"}\n";

	 flat_vertex_shader =
		"void\n"
		"main()\n"
		"{\n"
		"    vec3 p, n;\n"
		"    extractVertex( p, n );\n"
		"    gl_Position = gl_ModelViewProjectionMatrix * vec4( p, 1.0 );\n"
		"    gl_FrontColor = gl_Color;\n"
		"}\n";


}


DrawIsoSurfaceWithShader::~DrawIsoSurfaceWithShader()
{
	if(dataset)
		delete[] dataset;
}



void DrawIsoSurfaceWithShader::getBoundingBox(BoundCube &b) const
{
	b=bounds;
}

void DrawIsoSurfaceWithShader::setScalarThresh(float thresh)
{
	threshold=(float)(thresh/trueMax*255.0f);
}


bool DrawIsoSurfaceWithShader::init(const Voxels<float> &f)
{
	if(!shadersOK)
		return false;

	unsigned long long nX,nY,nZ;

	f.getSize(nX,nY,nZ);
	try
	{
		dataset = new char[nX*nY*nZ];
	}
	catch(std::bad_alloc)
	{
		return false;
	}

	//This algorithm does not handle voxel fields with negative values
	ASSERT(f.min() >= 0.0f);
	trueMax=f.max();

	bounds.setBounds(f.getMinBounds(),f.getMaxBounds());

	for(unsigned int ui=0;ui<nX*nY*nZ; ui++)
		dataset[ui]=(float)(f.getData(ui)/trueMax*255.0f);

	volume_size_x=nX;
	volume_size_y=nY;
	volume_size_z=nZ;

	// --- upload volume ------------------------------------------------------

	GLint alignment;
	glGetIntegerv( GL_UNPACK_ALIGNMENT, &alignment );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	glGenTextures( 1, &volume_tex );
	glBindTexture( GL_TEXTURE_3D, volume_tex );
	glTexImage3D( GL_TEXTURE_3D, 0, GL_ALPHA,
		      volume_size_x, volume_size_y, volume_size_z, 0,
		      GL_ALPHA, GL_UNSIGNED_BYTE, dataset );
	glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP);
	glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glBindTexture( GL_TEXTURE_3D, 0 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, alignment );

	// --- create HistoPyramid -------------------------------------------------
	hpmc_c = HPMCcreateConstants();
	hpmc_h = HPMCcreateHistoPyramid( hpmc_c );

	HPMCsetLatticeSize( hpmc_h,
			    volume_size_x,
			    volume_size_y,
			    volume_size_z );

	HPMCsetGridSize( hpmc_h,
			 volume_size_x-1,
			 volume_size_y-1,
			 volume_size_z-1 );

	float max_size = std::max( volume_size_x, std::max( volume_size_y, volume_size_z ) );
	HPMCsetGridExtent( hpmc_h,
			   volume_size_x / max_size,
			   volume_size_y / max_size,
			   volume_size_z / max_size );

	HPMCsetFieldTexture3D( hpmc_h,
			       volume_tex,
			       GL_FALSE );

	// --- create traversal vertex shader --------------------------------------
	hpmc_th_shaded = HPMCcreateTraversalHandle( hpmc_h );

	char *traversal_code = HPMCgetTraversalShaderFunctions( hpmc_th_shaded );
	const char* shaded_vsrc[2] =
	{
		traversal_code,
		shaded_vertex_shader.c_str()
	};
	shaded_v = glCreateShader( GL_VERTEX_SHADER );
	glShaderSource( shaded_v, 2, &shaded_vsrc[0], NULL );
	compileShader( shaded_v, "shaded vertex shader" );
	free( traversal_code );

	const char* shaded_fsrc[1] =
	{
		shaded_fragment_shader.c_str()
	};
	shaded_f = glCreateShader( GL_FRAGMENT_SHADER );
	glShaderSource( shaded_f, 1, &shaded_fsrc[0], NULL );
	compileShader( shaded_f, "shaded fragment shader" );

	// link program
	shaded_p = glCreateProgram();
	glAttachShader( shaded_p, shaded_v );
	glAttachShader( shaded_p, shaded_f );
	linkProgram( shaded_p, "shaded program" );

	// associate program with traversal handle
	HPMCsetTraversalHandleProgram( hpmc_th_shaded,
				       shaded_p,
				       0, 1, 2 );

	hpmc_th_flat = HPMCcreateTraversalHandle( hpmc_h );

	traversal_code = HPMCgetTraversalShaderFunctions( hpmc_th_flat );
	const char* flat_src[2] =
	{
		traversal_code,
		flat_vertex_shader.c_str()
	};
	flat_v = glCreateShader( GL_VERTEX_SHADER );
	glShaderSource( flat_v, 2, &flat_src[0], NULL );
	compileShader( flat_v, "flat vertex shader" );
	free( traversal_code );

	// link program
	flat_p = glCreateProgram();
	glAttachShader( flat_p, flat_v );
	linkProgram( flat_p, "flat program" );

	// associate program with traversal handle
	HPMCsetTraversalHandleProgram( hpmc_th_flat,
				       flat_p,
				       0, 1, 2 );


	glPolygonOffset( 1.0, 1.0 );
	return true;
}


void DrawIsoSurfaceWithShader::linkProgram( GLuint program, const std::string& what )
{
	ASSERT(shadersOK);
	glLinkProgram( program );

	GLint linkstatus;
	glGetProgramiv( program, GL_LINK_STATUS, &linkstatus );
	if ( linkstatus != GL_TRUE ) {

		GLint logsize;
		glGetProgramiv( program, GL_INFO_LOG_LENGTH, &logsize );

		if ( logsize > 0 ) {
			GLchar *infolog = new char[ logsize+1 ];
			glGetProgramInfoLog( program, logsize, NULL, infolog );
		}
		shadersOK=false;
	}
}


void DrawIsoSurfaceWithShader::draw() const
{
	ASSERT(shadersOK);
	HPMCbuildHistopyramid( hpmc_h, threshold );

	//TODO: FIXME:  Does not work??! Go back to examples and look again
		
	// --- render surface ------------------------------------------------------
	if ( !wireframe ) {
		HPMCextractVertices( hpmc_th_shaded );
	}
	else {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glEnable( GL_POLYGON_OFFSET_FILL );
		HPMCextractVertices( hpmc_th_flat );
		glDisable( GL_POLYGON_OFFSET_FILL );

		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE );
		HPMCextractVertices( hpmc_th_flat );
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
}

void DrawIsoSurfaceWithShader::compileShader( GLuint shader, const std::string& what )
{
	ASSERT(shadersOK);
	glCompileShader( shader );

	GLint compile_status;
	glGetShaderiv( shader, GL_COMPILE_STATUS, &compile_status );
	if ( compile_status != GL_TRUE ) {

		GLint logsize;
		glGetShaderiv( shader, GL_INFO_LOG_LENGTH, &logsize );

		if ( logsize > 0 ) {
			GLchar *infolog = new char[ logsize+1 ];
			glGetProgramInfoLog( shader, logsize, NULL, infolog );
		}
		else {
		}
		shadersOK=false;
	}
}
#endif


DrawAxis::DrawAxis()
{
}

DrawAxis::~DrawAxis()
{
}

void DrawAxis::setStyle(unsigned int s)
{
	style=s;
}

void DrawAxis::setSize(float s)
{
	size=s;
}

void DrawAxis::setPosition(const Point3D &p)
{
	position=p;
}

void DrawAxis::draw() const

{
	float halfSize=size/2.0f;
	glPushAttrib(GL_LIGHTING_BIT);
	glDisable(GL_LIGHTING);
	glBegin(GL_LINES);
	//Draw lines
	glColor3f(1.0f,0.0f,0.0f);
	glVertex3f(position[0]-halfSize,
	           position[1],position[2]);
	glVertex3f(position[0]+halfSize,
	           position[1],position[2]);

	glColor3f(0.0f,1.0f,0.0f);
	glVertex3f(position[0],
	           position[1]-halfSize,position[2]);
	glVertex3f(position[0],
	           position[1]+halfSize,position[2]);

	glColor3f(0.0f,0.0f,1.0f);
	glVertex3f(position[0],
	           position[1],position[2]-halfSize);
	glVertex3f(position[0],
	           position[1],position[2]+halfSize);
	glEnd();
	glPopAttrib();



	float numSections=20.0f;
	float twoPi = 2.0f *M_PI;
	float radius = 0.1*halfSize;
	//Draw axis cones

	//+x
	glPushMatrix();
	glTranslatef(position[0]+halfSize,position[1],position[2]);

	glColor3f(1.0f,0.0f,0.0f);
	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(radius,0,0);
	glNormal3f(1,0,0);
	for (unsigned int i = 0; i<=numSections; i++)
	{
		glVertex3f(0,radius * cos(i *  twoPi / numSections),
		           radius* sin(i * twoPi / numSections));
		glNormal3f(0,cos(i*twoPi/numSections),sin(i*twoPi/numSections));
	}
	glEnd();
	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0,0,0);
	glNormal3f(-1,0,0);
	for (unsigned int i = 0; i<=numSections; i++)
	{
		glVertex3f(0,-radius * cos(i *  twoPi / numSections),
		           radius* sin(i * twoPi / numSections));
		glNormal3f(-1,0,0);
	}
	glEnd();
	glPopMatrix();

	//+y
	glColor3f(0.0f,1.0f,0.0f);
	glPushMatrix();
	glTranslatef(position[0],position[1]+halfSize,position[2]);
	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0,radius,0);
	glNormal3f(0,1,0);
	for (unsigned int i = 0; i<=numSections; i++)
	{
		glVertex3f(radius * sin(i *  twoPi / numSections),0,
		           radius* cos(i * twoPi / numSections));
		glNormal3f(sin(i*twoPi/numSections),0,cos(i*twoPi/numSections));
	}
	glEnd();

	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0,0,0);
	glNormal3f(0,-1,0);
	for (unsigned int i = 0; i<=numSections; i++)
	{
		glVertex3f(radius * cos(i *  twoPi / numSections),0,
		           radius* sin(i * twoPi / numSections));
		glNormal3f(0,-1,0);
	}
	glEnd();

	glPopMatrix();



	//+z
	glColor3f(0.0f,0.0f,1.0f);
	glPushMatrix();
	glTranslatef(position[0],position[1],position[2]+halfSize);
	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0,0,radius);
	glNormal3f(0,0,1);
	for (unsigned int i = 0; i<=numSections; i++)
	{
		glVertex3f(radius * cos(i *  twoPi / numSections),
		           radius* sin(i * twoPi / numSections),0);
		glNormal3f(cos(i*twoPi/numSections),sin(i*twoPi/numSections),0);
	}
	glEnd();

	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0,0,0);
	glNormal3f(0,0,-1);
	for (unsigned int i = 0; i<=numSections; i++)
	{
		glVertex3f(-radius * cos(i *  twoPi / numSections),
		           radius* sin(i * twoPi / numSections),0);
		glNormal3f(0,0,-1);
	}
	glEnd();
	glPopMatrix();
}

void DrawAxis::getBoundingBox(BoundCube &b) const
{
	b.setInvalid();
}

