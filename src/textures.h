/*
 * 	textures.h - Texture control classes header
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


#ifndef TEXTURES_H
#define TEXTURES_H

#ifdef __APPLE__ 
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif
#include <cstdlib>
#include <vector>
#include <utility>
#include <string>

#include "basics.h"
#include "pngread.h"


//Named Textures
enum
{
	TEXTURE_LEFT_CLICK=0,
	TEXTURE_TRANSLATE,
	TEXTURE_RIGHT_CLICK,
	TEXTURE_ROTATE,
	TEXTURE_MIDDLE_CLICK,
	TEXTURE_SCROLL_WHEEL,
	TEXTURE_ENLARGE,

	TEXTURE_CTRL,
	TEXTURE_COMMAND, 
	TEXTURE_ALT,
	TEXTURE_TAB,
	TEXTURE_SHIFT,
};

//Paths to named textures
extern const char *TEST_OVERLAY_PNG[]; 

typedef struct {
GLuint name; /* OpenGL name assigned by the thingy */
GLuint width;
GLuint height;
unsigned char *data;
} texture;

class TexturePool
{
private:
		UniqueIDHandler texUniqIds;
		std::vector<std::pair<std::string,texture> > openTextures;

	public:
		//Open the texture specified by the following file, and
		//then return the texture ID; or just return the texture 
		//if already loaded. Return true on success.
		bool openTexture(const char *texName,unsigned int &texID, unsigned int &uniqID);

		//Close the specified texture, using its unique ID
		void closeTexture(unsigned int texID);

		//Close all textures
		void closeAll();
			
};

//!Type can be GL_TEXTURE_1D or GL_TEXTURE_2D
int pngTexture(texture* dest, const char* filename, GLenum type);
int pngTexture2D(texture*, const char*);
int pngTexture1D(texture*, const char*);


#endif
