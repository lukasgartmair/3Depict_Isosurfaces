#ifndef GLDEBUG_H
#define GLDEBUG_H


//OpenGL debugging macro
#if DEBUG
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <map>

#define glError() { \
		GLenum err = glGetError(); \
		while (err != GL_NO_ERROR) { \
					fprintf(stderr, "glError: %s caught at %s:%u\n", (char *)gluErrorString(err), __FILE__, __LINE__); \
					err = glGetError(); \
				} \
		std::cerr << "glErr Clean " << __FILE__ << ":" << __LINE__ << std::endl; \
}
	

#define glStackDepths() { \
		int gldepthdebug[3];glGetIntegerv (GL_MODELVIEW_STACK_DEPTH, gldepthdebug);\
	       	glGetIntegerv (GL_PROJECTION_STACK_DEPTH, gldepthdebug+1);\
	       	glGetIntegerv (GL_TEXTURE_STACK_DEPTH, gldepthdebug+2);\
		std::cerr << "OpenGL Stack Depths: ModelV:" << gldepthdebug[0] << " Pr: "\
		 << gldepthdebug[1] << " Tex:" << gldepthdebug[2] << std::endl;}
	

inline void glPrintMatrix(int matrixMode )
{

	ASSERT(matrixMode == GL_PROJECTION_MATRIX ||
			matrixMode == GL_MODELVIEW_MATRIX ||
			matrixMode == GL_TEXTURE_MATRIX );

	//Record old matrix mode,
	// then switch to new stack and retrieve the top
	float f[16];
	{
	GLint oldMode;
	glGetIntegerv( GL_MATRIX_MODE, &oldMode);
	std::map<int,int> remapMode;
	remapMode[GL_PROJECTION_MATRIX ] = GL_PROJECTION;
	remapMode[GL_MODELVIEW_MATRIX ] = GL_MODELVIEW;
	remapMode[GL_TEXTURE_MATRIX] = GL_TEXTURE;
	glMatrixMode (remapMode[matrixMode]);
	//retrieve
	glGetFloatv( matrixMode , f);
	glMatrixMode(oldMode);
	}


	std::cerr << "[ ";
	for(size_t ui=0; ui <4 ; ui++)
	{
		for(size_t uj=0;uj<4; uj++)
		{
			std::cerr << f[ui*4 + uj] << "\t" ;
		}
		if(ui !=3)
			std::cerr << std::endl;
	}
	std::cerr << " ] " << std::endl;
}

#else
	#define glStackDepths()
#define glError()
#endif

#endif