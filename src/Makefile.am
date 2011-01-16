MSYS_PATH=/c/msys/1.0/local/include/

.rc.o:
	$(WX_RESCOMP) $^ -o $@
%.o : %.rc
	 $(WX_RESCOMP) $^ -o $@

3Depict_LDFLAGS=$(LDFLAGS) $(FT_LDFLAGS)

3Depict_CXXFLAGS=$(CXXFLAGS) $(FT_INCLUDES) $(FTGL_CFLAGS) $(WX_CPPFLAGS) \
	$(GL_FLAGS) $(MGL_CFLAGS) $(XML_CFLAGS) $(PNG_CFLAGS) -DDEBUG 
3Depict_CFLAGS=$(CXXFLAGS) $(FT_INCLUDES) $(FTGL_CFLAGS) \
	       $(XML_CFLAGS)  $(MGL_CFLAGS) $(PNG_CFLAGS) $(QHULL_CFLAGS) -DDEBUG 

3Depict_LDADD=$(LIBS) $(WX_LIBS) $(MGL_LIBS)  $(FTGL_LIBS) \
		 $(FT_LIBS) $(XML_LIBS) $(GSL_LIBS) $(GL_LIBS) $(GLU_LIBS) $(QHULL_LIBS) $(PNG_LIBS) 

bin_PROGRAMS= 3Depict
SOURCE_FILES=quat.c pngread.c rdf.cpp glPane.cpp basics.cpp cameras.cpp drawables.cpp lights.cpp \
		3Depict.cpp scene.cpp mathglPane.cpp plot.cpp APTClasses.cpp filter.cpp \
		viscontrol.cpp mathfuncs.cpp wxcomponents.cpp colourmap.cpp select.cpp common.cpp \
		XMLHelper.cpp ResDialog.cpp ExportPos.cpp ExportRngDialog.cpp StashDialog.cpp textures.cpp \
		configFile.cpp K3DTree.cpp IsoSurface.cpp prefDialog.cpp effect.cpp wxCropPanel.cpp

#Do we have or need windows-XP "resource" files for look and feel?
if HAVE_WINDRES
3Depict_SOURCES=$(SOURCE_FILES)  winconsole.cpp 3Depict.rc
else
3Depict_SOURCES=$(SOURCE_FILES)
endif
