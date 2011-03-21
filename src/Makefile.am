MSYS_PATH=/c/msys/1.0/local/include/

.rc.o:
	$(WX_RESCOMP) $^ -o $@
%.o : %.rc
	 $(WX_RESCOMP) $^ -o $@

3Depict_LDFLAGS=$(LDFLAGS) $(FT_LDFLAGS)

3Depict_CXXFLAGS=$(CXXFLAGS) $(FT_INCLUDES) $(FTGL_CFLAGS) $(WX_CPPFLAGS) \
	$(GL_FLAGS) $(MGL_CFLAGS) $(XML_CFLAGS) $(PNG_CFLAGS)  \
	$(OPENMP_FLAGS) $(DEBUG_FLAGS)
3Depict_CFLAGS=$(CFLAGS) $(FT_INCLUDES) $(FTGL_CFLAGS) \
	       $(XML_CFLAGS)  $(MGL_CFLAGS) $(PNG_CFLAGS) $(QHULL_CFLAGS) \
	      $(OPENMP_FLAGS) $(DEBUG_FLAGS) 

3Depict_LDADD=$(LIBS) $(WX_LIBS) $(MGL_LIBS)  $(FTGL_LIBS) \
		 $(FT_LIBS) $(XML_LIBS) $(GSL_LIBS) $(GL_LIBS) $(GLU_LIBS) $(QHULL_LIBS) $(PNG_LIBS) 

bin_PROGRAMS= 3Depict

FILTER_FILES = filters/allFilter.cpp filters/posLoad.cpp filters/ionDownsample.cpp \
		filters/rangeFile.cpp filters/voxelise.cpp filters/spectrumPlot.cpp \
	       	filters/transform.cpp filters/externalProgram.cpp filters/ionClip.cpp \
		filters/ionColour.cpp filters/boundingBox.cpp  \
		filters/compositionProfile.cpp filters/spatialAnalysis.cpp \
		filters/clusterAnalysis.cpp



#These source files should have heirachical couplings only; no cross-coupling
BASE_SOURCE_FILES=  mathglPane.cpp plot.cpp prefDialog.cpp configFile.cpp filter.cpp \
		     APTClasses.cpp resDialog.cpp select.cpp \
		     drawables.cpp effect.cpp textures.cpp rdf.cpp \
		     cameras.cpp isoSurface.cpp K3DTree.cpp K3DTree-mk2.cpp\
			basics.cpp lights.cpp xmlHelper.cpp cropPanel.cpp colourmap.cpp \
			pngread.c mathfuncs.cpp wxcomponents.cpp wxcommon.cpp

#These files are inter-dependant to some extent (mostly viscontrol)
CROSS_DEP_SOURCE_FILE=3Depict.cpp ExportPos.cpp ExportRngDialog.cpp glPane.cpp scene.cpp StashDialog.cpp viscontrol.cpp


SOURCE_FILES= $(BASE_SOURCE_FILES)  $(FILTER_FILES) $(CROSS_DEP_SOURCE_FILE)
#Do we have or need windows-XP "resource" files for look and feel?
if HAVE_WINDRES
3Depict_SOURCES=$(SOURCE_FILES)  winconsole.cpp 3Depict.rc
else
3Depict_SOURCES=$(SOURCE_FILES)
endif
