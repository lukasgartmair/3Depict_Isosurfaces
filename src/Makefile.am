MSYS_PATH=/c/msys/1.0/local/include/

.rc.o:
	$(WX_RESCOMP) $^ -o $@
%.o : %.rc
	 $(WX_RESCOMP) $^ -o $@

3Depict_LDFLAGS=$(LDFLAGS) $(FT_LDFLAGS) $(GSL_LIBS) $(MGL_LIBS)

3Depict_CXXFLAGS=$(CXXFLAGS) $(FT_INCLUDES) $(FTGL_CFLAGS) $(WX_CPPFLAGS) \
	$(GL_FLAGS) $(GSL_CFLAGS) $(MGL_CFLAGS) $(XML_CFLAGS) $(PNG_CFLAGS)  \
	$(OPENMP_FLAGS) $(DEBUG_FLAGS) -pipe 

3Depict_CFLAGS=$(CFLAGS) $(FT_INCLUDES) $(FTGL_CFLAGS) \
	       $(XML_CFLAGS) $(GSL_CFLAGS)  $(MGL_CFLAGS) $(PNG_CFLAGS) $(QHULL_CFLAGS) \
	      $(OPENMP_FLAGS) $(DEBUG_FLAGS) -pipe


3Depict_LDADD=$(LIBS) $(GETTEXT_LIBS) $(WX_LIBS) $(MGL_LIBS)  $(FTGL_LIBS) \
		 $(FT_LIBS) $(XML_LIBS) $(GSL_LIBS) $(GL_LIBS) $(GLU_LIBS) $(QHULL_LIBS) $(PNG_LIBS) 

bin_PROGRAMS= 3Depict

FILTER_FILES = filters/allFilter.cpp filters/dataLoad.cpp filters/ionDownsample.cpp \
		filters/rangeFile.cpp filters/voxelise.cpp filters/spectrumPlot.cpp \
	       	filters/transform.cpp filters/externalProgram.cpp filters/ionClip.cpp \
		filters/ionColour.cpp filters/boundingBox.cpp  \
		filters/compositionProfile.cpp filters/spatialAnalysis.cpp \
		filters/clusterAnalysis.cpp filters/ionInfo.cpp \
		filters/annotation.cpp

FILTER_HEADER_FILES = filters/allFilter.h filters/dataLoad.h filters/ionDownsample.h \
		filters/rangeFile.h filters/voxelise.h filters/spectrumPlot.h \
	       	filters/transform.h filters/externalProgram.h filters/ionClip.h \
		filters/ionColour.h filters/boundingBox.h  \
		filters/compositionProfile.h filters/spatialAnalysis.h \
		filters/clusterAnalysis.h filters/ionInfo.h \
		filters/annotation.h

DIALOG_SOURCE_FILES = dialogs/ExportPos.cpp  dialogs/ExportRngDialog.cpp dialogs/prefDialog.cpp  \
		dialogs/resolutionDialog.cpp  dialogs/StashDialog.cpp \
		dialogs/autosaveDialog.cpp dialogs/filterErrorDialog.cpp \
		dialogs/animateFilterDialog.cpp \
		dialogs/animateSubDialogs/colourKeyFrameDialog.cpp \
		dialogs/animateSubDialogs/stringKeyFrameDialog.cpp \
		dialogs/animateSubDialogs/choiceKeyFrameDialog.cpp


DIALOG_HEADER_FILES = dialogs/ExportPos.h dialogs/ExportRngDialog.h dialogs/prefDialog.h \
		dialogs/StashDialog.h dialogs/resolutionDialog.h \
		dialogs/autosaveDialog.h dialogs/filterErrorDialog.h \
		dialogs/animateFilterDialog.h \
		dialogs/animateSubDialogs/realKeyFrameDialog.h \
		dialogs/animateSubDialogs/colourKeyFrameDialog.h \
		dialogs/animateSubDialogs/stringKeyFrameDialog.h \
		dialogs/animateSubDialogs/choiceKeyFrameDialog.h

#These source files should have heirachical couplings only; no cross-coupling
BASE_SOURCE_FILES=  animator.cpp eventlogger.cpp filtertreeAnalyse.cpp filtertree.cpp mathglPane.cpp \
	  	     plot.cpp configFile.cpp filter.cpp \
		     APTClasses.cpp select.cpp \
		     drawables.cpp effect.cpp textures.cpp rdf.cpp \
		     cameras.cpp isoSurface.cpp K3DTree.cpp K3DTree-mk2.cpp\
			 xmlHelper.cpp cropPanel.cpp colourmap.cpp \
			pngread.c mathfuncs.cpp wxcomponents.cpp wxcommon.cpp 

BASE_HEADER_FILES=   animator.h eventlogger.h filtertreeAnalyse.h filtertree.h mathglPane.h plot.h \
		     configFile.h filter.h APTClasses.h select.h \
		     drawables.h effect.h textures.h rdf.h translation.h winconsole.h \
		     cameras.h isoSurface.h K3DTree.h K3DTree-mk2.h\
			xmlHelper.h cropPanel.h colourmap.h \
			pngread.h mathfuncs.h wxcomponents.h wxcommon.h voxels.h \
		       endianTest.h assertion.h commonConstants.h  art.h tree.hh 3Depict.xpm


#These files are inter-dependant to some extent (mostly viscontrol)
CROSS_DEP_SOURCE_FILES=testing.cpp 3Depict.cpp basics.cpp glPane.cpp \
		       scene.cpp viscontrol.cpp 
CROSS_DEP_HEADER_FILES=testing.h 3Depict.h basics.h glPane.h \
			scene.h viscontrol.h 

SOURCE_FILES=  $(BASE_SOURCE_FILES) $(BASE_HEADER_FILES) $(DIALOG_SOURCE_FILES) $(DIALOG_HEADER_FILES) $(FILTER_FILES) $(FILTER_HEADER_FILES) \
	      $(CROSS_DEP_SOURCE_FILES) $(CROSS_DEP_HEADER_FILES) 


#Do we have or need windows-XP "resource" files for look and feel?
if HAVE_WINDRES
3Depict_SOURCES=$(SOURCE_FILES)  winconsole.cpp 3Depict.rc
else
3Depict_SOURCES=$(SOURCE_FILES)
endif

#Tarball options
EXTRA_DIST = textures/ tex-source/ glade-skeleton/ myAppIcon.ico
 

if USE_PRECOMPILED_HEADERS

#Precompiled headers
BUILT_SOURCES=   mathglPane.h.gch plot.h.gch configFile.h.gch filter.h.gch \
		     APTClasses.h.gch resDialog.h.gch select.h.gch \
		     drawables.h.gch effect.h.gch textures.h.gch rdf.h.gch \
		     cameras.h.gch isoSurface.h.gch K3DTree.h.gch K3DTree-mk2.h.gch\
			basics.h.gch xmlHelper.h.gch cropPanel.h.gch colourmap.h.gch \
			pngread.h.gch mathfuncs.h.gch wxcomponents.h.gch wxcommon.h.gch voxels.h.gch \
		       endianTest.h.gch assertion.h.gch commonConstants.h.gch  art.h.gch tree.hh.gch
%.hh.gch: %.hh
	        $(CXXCOMPILE) $(3Depict_CXXFLAGS)-c $< 

%.h.gch: %.h
	        $(CXXCOMPILE) $(3Depict_CXXFLAGS) -c $< 
endif
