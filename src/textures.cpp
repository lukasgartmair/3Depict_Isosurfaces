/*
 *	textures.cpp - texture wrapper class implementation
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

#include "textures.h"

const char *TEST_OVERLAY_PNG[] = { 
				"textures/Left_clicked_mouse.png",
				"textures/Left-Right-arrow.png",
				"textures/Right_clicked_mouse.png",
				"textures/rotateArrow.png", 
				"textures/middle_clicked_mouse.png",
				"textures/scroll_wheel_mouse.png",
				"textures/enlarge.png",
			
				"textures/keyboard-ctrl.png",
				"textures/keyboard-command.png",
				"textures/keyboard-alt.png",
				"textures/keyboard-tab.png",
				"textures/keyboard-shift.png",
			};


bool TexturePool::openTexture(const char *texName,unsigned int &texID, unsigned int &uniqID)
{
	std::string texPath;

	texPath = locateDataFile(texName);
	
	//See if we already have this texture (use abs. name)
	for(unsigned int ui=0;ui<openTextures.size();ui++)
	{
		if(openTextures[ui].first == texPath)
		{
			texID = openTextures[ui].second.name;
			uniqID = texUniqIds.getId(ui);
			return true;
		}
	}


	//Try to load the texture, as we don't have it
	texture tex;	
	if(!pngTexture2D(&tex,texPath.c_str()))
	{
		uniqID=texUniqIds.genId(openTextures.size());
		openTextures.push_back(
			make_pair(texPath,tex));

		texID=tex.name;
		return true;
	}
	else
		return false;
}


void TexturePool::closeTexture(unsigned int texId)
{
	for(unsigned int ui=0;ui<openTextures.size();ui++)
	{
		if(openTextures[ui].second.name== texId)
		{
			texUniqIds.killByPos(ui);	
			delete [] openTextures[ui].second.data;
			openTextures.erase(openTextures.begin()+ui);
			return;
		}
	}
}

void TexturePool::closeAll()
{
	for(unsigned int ui=0;ui<openTextures.size();ui++)
	{
		delete[] openTextures[ui].second.data;
		glDeleteTextures(1,&openTextures[ui].second.name);	
	}

	texUniqIds.clear();
	openTextures.clear();
}


int pngTexture(texture* dest, const char* filename, GLenum type) {
  FILE *fp;
  unsigned int x, y, z;
  png_uint_32 width, height;
  GLint curtex;
  png_bytep *texture_rows;

  if (!check_if_png((char *)filename, &fp, 8)) {
    return(1); /* could not open, or was not a valid .png */
  }

  if (read_png(fp, 8, &texture_rows, &width, &height)) {
    return(2); /* something is wrong with the .png */
  }

  z=0;
  dest->width = width;
  dest->height = height;
  dest->data = (unsigned char *)malloc(4*width*height);
  for (y=0; y<height; y++) {
    for (x=0; x<4*(width); x++) {
      dest->data[z++] = texture_rows[y][x];
    }
    free(texture_rows[y]);
  }
  free(texture_rows);

  if (type == GL_TEXTURE_1D) {
    glGetIntegerv(GL_TEXTURE_BINDING_1D, &curtex);
  } else {
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &curtex);
  }
  glGenTextures(1, &(dest->name));
  glBindTexture(type, dest->name);
  if (type == GL_TEXTURE_1D) {
    glTexImage1D(type, 0, GL_RGBA, dest->width, 0, GL_RGBA, GL_UNSIGNED_BYTE, dest->data);
  } else {
    glTexImage2D(type, 0, GL_RGBA, dest->width, dest->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, dest->data);
  }
  glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_LINEAR); /* other routines should override this later */
  glTexParameteri(type, GL_TEXTURE_MAG_FILTER, GL_LINEAR); /* if they don't want linear filtering */
  glBindTexture(type, curtex);
  return (0);
}

int pngTexture2D(texture* dest, const char* filename) {
  return (pngTexture(dest, filename, GL_TEXTURE_2D));
}

int pngTexture1D(texture* dest, const char* filename) {
  return (pngTexture(dest, filename, GL_TEXTURE_1D));
}
