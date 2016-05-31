MSYS_PATH=/c/msys/1.0/local/include/

%.rc.o: 
	$(WX_RESCOMP) $^ -o $@ 3Depict.rc

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

#------- Common header files for all sub-modules
COMMON_SOURCE_FILES = common/pngread.c common/stringFuncs.cpp common/constants.cpp common/xmlHelper.cpp\
			 common/colourmap.cpp common/voxels.cpp common/mathfuncs.cpp common/basics.cpp common/assertion.cpp \
			common/mesh.cpp common/gsl_helper.cpp
COMMON_HEADER_FILES = common/pngread.h common/stringFuncs.h  common/constants.h  common/xmlHelper.h common/colourmap.h \
		      	common/mathfuncs.h common/basics.h common/translation.h common/endianTest.h common/assertion.h common/voxels.h \
			common/array2D.h common/mesh.h common/gsl_helper.h

#-----------

#------- "Backend" calculation files (non-ui) ----------
FILTER_FILES = backend/filters/allFilter.cpp backend/filters/filterCommon.cpp \
	        backend/filters/dataLoad.cpp backend/filters/ionDownsample.cpp \
		backend/filters/rangeFile.cpp backend/filters/voxelise.cpp \
		backend/filters/spectrumPlot.cpp backend/filters/transform.cpp \
		backend/filters/externalProgram.cpp backend/filters/ionClip.cpp \
		backend/filters/ionColour.cpp backend/filters/boundingBox.cpp  \
		backend/filters/profile.cpp backend/filters/spatialAnalysis.cpp \
		backend/filters/clusterAnalysis.cpp backend/filters/ionInfo.cpp \
		backend/filters/annotation.cpp backend/filters/geometryHelpers.cpp \
		backend/filters/algorithms/binomial.cpp  backend/filters/algorithms/mass.cpp  

FILTER_HEADER_FILES = backend/filters/allFilter.h backend/filters/filterCommon.h \
		backend/filters/dataLoad.h backend/filters/ionDownsample.h \
		backend/filters/rangeFile.h backend/filters/voxelise.h backend/filters/spectrumPlot.h \
	       	backend/filters/transform.h backend/filters/externalProgram.h backend/filters/ionClip.h \
		backend/filters/ionColour.h backend/filters/boundingBox.h  \
		backend/filters/profile.h backend/filters/spatialAnalysis.h \
		backend/filters/clusterAnalysis.h backend/filters/ionInfo.h \
		backend/filters/annotation.h backend/filters/geometryHelpers.h \
		backend/filters/algorithms/binomial.h backend/filters/algorithms/mass.h

BACKEND_SOURCE_FILES = backend/animator.cpp backend/filtertreeAnalyse.cpp backend/filtertree.cpp \
		     	backend/APT/ionhit.cpp backend/APT/APTFileIO.cpp backend/APT/APTRanges.cpp backend/APT/abundanceParser.cpp \
			backend/APT/vtk.cpp \
			backend/filters/algorithms/K3DTree.cpp backend/filters/algorithms/K3DTree-mk2.cpp\
			backend/filter.cpp backend/filters/algorithms/rdf.cpp \
		       backend/viscontrol.cpp backend/state.cpp backend/plot.cpp  backend/configFile.cpp 

BACKEND_HEADER_FILES =  backend/animator.h backend/filtertreeAnalyse.h backend/filtertree.h\
			backend/APT/ionhit.h backend/APT/APTFileIO.h backend/APT/APTRanges.h backend/APT/abundanceParser.h \
			backend/filters/vtk.h backend/filters/algorithms/K3DTree.h backend/filters/algorithms/K3DTree-mk2.h \
			backend/filter.h backend/filters/algorithms/rdf.h \
			backend/viscontrol.h backend/state.h backend/plot.h backend/configFile.h \
		        backend/tree.hh

#------------

#------------ OpenGL interface files
OPENGL_HEADER_FILES = gl/scene.h gl/drawables.h gl/effect.h gl/textures.h gl/select.h gl/cameras.h gl/isoSurface.h gl/tr.h gl/glDebug.h 
OPENGL_SOURCE_FILES =  gl/scene.cpp gl/drawables.cpp gl/effect.cpp gl/textures.cpp gl/select.cpp gl/cameras.cpp gl/isoSurface.cpp gl/tr.cpp 
#------------



#------------ Frontend (linked to UI in some way) files ---
DIALOG_SOURCE_FILES = gui/dialogs/ExportPos.cpp  gui/dialogs/ExportRngDialog.cpp gui/dialogs/prefDialog.cpp  \
		gui/dialogs/resolutionDialog.cpp  gui/dialogs/StashDialog.cpp \
		gui/dialogs/autosaveDialog.cpp gui/dialogs/filterErrorDialog.cpp \
		gui/dialogs/animateFilterDialog.cpp \
		gui/dialogs/animateSubDialogs/colourKeyFrameDialog.cpp \
		gui/dialogs/animateSubDialogs/stringKeyFrameDialog.cpp \
		gui/dialogs/animateSubDialogs/choiceKeyFrameDialog.cpp \
		gui/dialogs/rangeEditDialog.cpp


DIALOG_HEADER_FILES = gui/dialogs/ExportPos.h gui/dialogs/ExportRngDialog.h gui/dialogs/prefDialog.h \
		gui/dialogs/StashDialog.h gui/dialogs/resolutionDialog.h \
		gui/dialogs/autosaveDialog.h gui/dialogs/filterErrorDialog.h \
		gui/dialogs/animateFilterDialog.h \
		gui/dialogs/animateSubDialogs/realKeyFrameDialog.h \
		gui/dialogs/animateSubDialogs/colourKeyFrameDialog.h \
		gui/dialogs/animateSubDialogs/stringKeyFrameDialog.h \
		gui/dialogs/animateSubDialogs/choiceKeyFrameDialog.h \
		gui/dialogs/rangeEditDialog.h

GUI_SOURCE_FILES=gui/mainFrame.cpp gui/mathglPane.cpp gui/cropPanel.cpp gui/glPane.cpp  $(DIALOG_SOURCE_FILES)
GUI_HEADER_FILES=gui/mainFrame.h gui/mathglPane.h gui/cropPanel.h gui/art.h gui/glPane.h $(DIALOG_HEADER_FILES)

BASE_SOURCE_FILES=   3Depict.cpp testing/testing.cpp wx/wxcommon.cpp  wx/wxcomponents.cpp winconsole.cpp  wx/propertyGridUpdater.cpp
BASE_HEADER_FILES=   testing/testing.h  wx/wxcommon.h  wx/wxcomponents.h   winconsole.h wx/propertyGridUpdater.h

TEST_SOURCE_FILES = testing/mglTesting.cpp
TEST_HEADER_FILES = testing/mglTesting.h
#-----------



SOURCE_FILES=  $(BASE_SOURCE_FILES) $(BASE_HEADER_FILES) $(GUI_SOURCE_FILES) $(GUI_HEADER_FILES) \
	       $(FILTER_FILES) $(FILTER_HEADER_FILES) \
	       ${BACKEND_SOURCE_FILES} ${BACKEND_HEADER_FILES} $(OPENGL_SOURCE_FILES) $(OPENGL_HEADER_FILES) \
	       $(COMMON_SOURCE_FILES) $(COMMON_HEADER_FILES) $(TEST_SOURCE_FILES) $(TEST_HEADER_FILES)

3Depict_SOURCES=$(SOURCE_FILES)

#Do we have or need windows-XP "resource" files for look and feel?
if HAVE_WINDRES
3Depict_SOURCES+=3Depict.rc
3Depict_LDADD+= 3Depict.rc.o
endif

#Tarball options
EXTRA_DIST = gui/glade-skeleton myAppIcon.ico testing/filtertesting.cpp 
 

