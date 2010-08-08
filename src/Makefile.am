.rc.o:
	windres $^ -o $@
%.o : %.rc
	windres $^ -o $@

3Depict_LDFLAGS=$(LDFLAGS) $(FT_LDFLAGS)

3Depict_CXXFLAGS=$(CXXFLAGS) $(FT_INCLUDES) $(FTGL_CFLAGS) $(WX_CPPFLAGS) \
		 	$(MGL_CFLAGS) $(XML_CFLAGS) $(PNG_CFLAGS) -DDEBUG 
3Depict_CFLAGS=$(CXXFLAGS) $(FT_INCLUDES) $(FTGL_CFLAGS) \
	       $(XML_CFLAGS)  $(MGL_CFLAGS) $(PNG_CFLAGS) -DDEBUG 

3Depict_LDADD=$(LIBS) $(FT_LIBS) $(FT_LIBS) $(GL_LIBS) \
		$(FTGL_LIBS) $(MGL_LIBS) $(WX_LIBS) $(XML_LIBS) $(PNG_LIBS)
bin_PROGRAMS= 3Depict
SOURCE_FILES=quat.c pngread.c glPane.cpp basics.cpp cameras.cpp drawables.cpp lights.cpp \
		3Depict.cpp scene.cpp mathglPane.cpp plot.cpp APTClasses.cpp filter.cpp \
		viscontrol.cpp mathfuncs.cpp wxcomponents.cpp colourmap.cpp select.cpp common.cpp \
		XMLHelper.cpp ResDialog.cpp ExportPos.cpp ExportRngDialog.cpp StashDialog.cpp textures.cpp \
	        configFile.cpp K3DTree.cpp	

#Do we have or need windows-XP "resource" files for look and feel?
if HAVE_WINDRES
3Depict_SOURCES=$(SOURCE_FILES)  winconsole.cpp quick3d.rc
else
3Depict_SOURCES=$(SOURCE_FILES)
endif
