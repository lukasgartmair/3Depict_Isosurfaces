/*
 *	animateFilterDialog.cpp - Framewise animation of filter properties
 *	Copyright (C) 2013, D Haley 

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
// -*- C++ -*- generated by wxGlade b48da20df8f4++ on Mon May  7 22:48:14 2012

#include "animateFilterDialog.h"
#include "resolutionDialog.h"


#include "./animateSubDialogs/realKeyFrameDialog.h"
#include "./animateSubDialogs/colourKeyFrameDialog.h"
#include "./animateSubDialogs/stringKeyFrameDialog.h"
#include "./animateSubDialogs/choiceKeyFrameDialog.h"
// begin wxGlade: ::extracode

// end wxGlade

#include <wx/filename.h>
#include "common/translation.h"
#include "common/stringFuncs.h"
#include "wx/wxcomponents.h"

enum
{
	ID_FILTER_TREE_CTRL,
	ID_PROPERTY_GRID,
	ID_ANIMATION_GRID_CTRL,
	ID_BUTTON_FRAME_REMOVE,
	ID_TEXTBOX_WORKDIR,
	ID_BUTTON_WORKDIR,
	ID_CHECK_IMAGE_OUT,
	ID_TEXTBOX_IMAGEPREFIX,
	ID_TEXTBOX_IMAGESIZE,
	ID_BUTTON_IMAGE_RES,
	ID_CHECK_ONLYDATACHANGE,
	ID_CHECK_POINT_OUT,
	ID_CHECK_PLOT_OUT,
	ID_CHECK_VOXEL_OUT,
	ID_CHECK_RANGE_OUT,
	ID_COMBO_RANGE_TYPE,
	ID_SPLIT_FILTERVIEW,
	ID_FRAME_SLIDER,
	ID_FRAME_TEXTBOX,
	ID_BTN_OK,
	ID_BTN_CANCEL,
	ID_FILTER_PROPERTY_VALUE_GRID
};
enum
{
	CELL_FILTERNAME,
	CELL_PROPERTYNAME,
	CELL_KEYINTERPMODE,
	CELL_STARTFRAME,
	CELL_ENDFRAME
};

enum
{
	FRAME_CELL_FILTERNAME,
	FRAME_CELL_PROPNAME,
	FRAME_CELL_VALUE
};

const size_t RANGE_FORMAT_NUM_OPTIONS=3;


//TODO: This should be merged into aptclasses?
const char *extension[RANGE_FORMAT_NUM_OPTIONS] =
{
	"rng",
	"rrng",
	"env"
};

const char * comboRange_choices[RANGE_FORMAT_NUM_OPTIONS] =
{
	NTRANS("Oak-Ridge RNG"),
	NTRANS("Cameca/Ametek RRNG"),
	NTRANS("Cameca/Ametek ENV")
};

using std::string;
using std::cout;
using std::endl;


template<class T>
bool getRealKeyFrame(FrameProperties &frameProp,
		FilterProperty &filterProp, RealKeyFrameDialog<T> *r) 
{
	if( r->ShowModal() != wxID_OK)
	{
		r->Destroy();
		return false;
	}

	//Copy out the data obtained from the dialog
	size_t transitionMode;
	T value;
	transitionMode=r->getTransitionMode();
	frameProp.setInterpMode(transitionMode);

	value=r->getStartValue();
	stream_cast(filterProp.data,value);

	frameProp.addKeyFrame(r->getStartFrame(),filterProp);

	//Add end value as needed
	switch(transitionMode)
	{
		case TRANSITION_STEP:
			break;
		case TRANSITION_INTERP:
			value=r->getEndValue();
			stream_cast(filterProp.data,value);
			frameProp.addKeyFrame(r->getEndFrame(),filterProp);
			break;
		default:
			ASSERT(false);
	}

	r->Destroy();

	return true;
}

ExportAnimationDialog::ExportAnimationDialog(wxWindow* parent, int id, const wxString& title, const wxPoint& pos, const wxSize& size, long style):
    wxDialog(parent, id, title, pos, size, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER|wxMAXIMIZE_BOX|wxMINIMIZE_BOX)
{
    // begin wxGlade: ExportAnimationDialog::ExportAnimationDialog
    viewNotebook = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0);
    frameViewPane = new wxPanel(viewNotebook, wxID_ANY);
    filterViewPane = new wxPanel(viewNotebook, wxID_ANY);
    splitPaneFilter = new wxSplitterWindow(filterViewPane, ID_SPLIT_FILTERVIEW, wxDefaultPosition, wxDefaultSize, wxSP_3D|wxSP_BORDER);
    filterRightPane = new wxPanel(splitPaneFilter, wxID_ANY);
    filterLeftPane = new wxPanel(splitPaneFilter, wxID_ANY);
    keyFramesSizer_staticbox = new wxStaticBox(filterRightPane, -1, wxTRANS("Key frames"));
    outputDataSizer_staticbox = new wxStaticBox(frameViewPane, -1, wxTRANS("Output Data"));
    filterPropertySizer_staticbox = new wxStaticBox(filterLeftPane, -1, wxTRANS("Filters and properties"));
    filterTreeCtrl =new wxTreeCtrl(filterLeftPane,ID_FILTER_TREE_CTRL , wxDefaultPosition, wxDefaultSize, wxTR_HAS_BUTTONS|wxTR_NO_LINES|wxTR_HIDE_ROOT|wxTR_DEFAULT_STYLE|wxSUNKEN_BORDER|wxTR_EDIT_LABELS);

    propertyGrid = new wxPropertyGrid(filterLeftPane, ID_PROPERTY_GRID);
    animationGrid = new wxGrid(filterRightPane, ID_ANIMATION_GRID_CTRL);
    keyFrameRemoveButton = new wxButton(filterRightPane, wxID_REMOVE, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    labelWorkDir = new wxStaticText(frameViewPane, wxID_ANY, wxTRANS("Dir : "));
    textWorkDir = new wxTextCtrl(frameViewPane, ID_TEXTBOX_WORKDIR, wxEmptyString);
    buttonWorkDir = new wxButton(frameViewPane, wxID_OPEN, wxEmptyString);
    checkOutOnlyChanged = new wxCheckBox(frameViewPane, ID_CHECK_ONLYDATACHANGE, wxTRANS("Output only when refresh required"));
    outputDataSepLine = new wxStaticLine(frameViewPane, wxID_ANY);
    labelDataType = new wxStaticText(frameViewPane, wxID_ANY, wxTRANS("Data Types:"));
    checkImageOutput = new wxCheckBox(frameViewPane, ID_CHECK_IMAGE_OUT, wxTRANS("3D Images"));
    lblImageName = new wxStaticText(frameViewPane, wxID_ANY, wxTRANS("File Prefix: "));
    textImageName = new wxTextCtrl(frameViewPane, ID_TEXTBOX_IMAGEPREFIX, wxEmptyString);
    labelImageSize = new wxStaticText(frameViewPane, wxID_ANY, wxTRANS("Size : "));
    textImageSize = new wxTextCtrl(frameViewPane, ID_TEXTBOX_IMAGESIZE, wxEmptyString);
    buttonImageSize = new wxButton(frameViewPane, ID_BUTTON_IMAGE_RES, wxTRANS("..."));
    checkPoints = new wxCheckBox(frameViewPane, ID_CHECK_POINT_OUT, wxTRANS("Point data"));
    checkPlotData = new wxCheckBox(frameViewPane, ID_CHECK_PLOT_OUT, wxTRANS("Plots"));
    checkVoxelData = new wxCheckBox(frameViewPane, ID_CHECK_VOXEL_OUT, wxTRANS("Voxel data"));
    checkRangeData = new wxCheckBox(frameViewPane, ID_CHECK_RANGE_OUT, wxTRANS("Range files"));
    labelRangeFormat = new wxStaticText(frameViewPane, wxID_ANY, wxTRANS("Format"));
    
    //Workaround for wx bug http://trac.wxwidgets.org/ticket/4398
    wxSortedArrayString rangeNames;
    for(unsigned int ui=0;ui<RANGE_FORMAT_NUM_OPTIONS; ui++)
    {
	const char * str = comboRange_choices[ui];

	//construct translation->comboRange_choices offset.
	rangeMap[TRANS(str)] = ui;
	//Add to filter name wxArray
	wxString wxStrTrans = wxTRANS(str);
	rangeNames.Add(wxStrTrans);
    }
    comboRangeFormat = new wxChoice(frameViewPane, ID_COMBO_RANGE_TYPE, wxDefaultPosition, wxDefaultSize, rangeNames);
    static_line_1 = new wxStaticLine(frameViewPane, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL);
    labelFrame = new wxStaticText(frameViewPane, wxID_ANY, wxTRANS("Frame"));
    frameSlider = new wxSlider(frameViewPane, ID_FRAME_SLIDER, 1, 1, 1);
    textFrame = new wxTextCtrl(frameViewPane, ID_FRAME_TEXTBOX, wxEmptyString);
    framePropGrid = new wxGrid(frameViewPane, ID_FILTER_PROPERTY_VALUE_GRID);
    cancelButton = new wxButton(this, wxID_CANCEL, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    okButton = new wxButton(this, wxID_OK, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);


    set_properties();
    do_layout();
    // end wxGlade
 

#if wxCHECK_VERSION(2,9,0)
    //Manually tuned splitter parameters
    splitPaneFilter->SetMinimumPaneSize(220);
    int w, h;
    GetClientSize(&w,&h);

    float sashFrac=0.4;

    splitPaneFilter->SetSashPosition((int)(sashFrac*w));
#endif

    programmaticEvent=true;

    //-- set up the default properties for dialog back-end data
    imageWidth=640;
    imageHeight=480;
    imageSizeOK=true;

    //Plot check status
    wantPlotOutput=checkPlotData->IsChecked();
    wantImageOutput=checkImageOutput->IsChecked();
    wantIonOutput=checkPoints->IsChecked();
    wantPlotOutput=checkPlotData->IsChecked();
    wantVoxelOutput=checkVoxelData->IsChecked();
    wantRangeOutput=checkRangeData->IsChecked();
    wantOnlyChanges=checkOutOnlyChanged->IsChecked();

    string sFirst,sSecond;
    stream_cast(sFirst,imageWidth);
    stream_cast(sSecond,imageHeight);
    textImageSize->SetValue(wxStr(string(sFirst+string("x")+sSecond)));
    textImageSize->SetBackgroundColour(wxNullColour);

    comboRangeFormat->Enable(checkRangeData->IsChecked());

    currentFrame=0;
    existsConflicts=false;
    //---

    programmaticEvent=false;
}

ExportAnimationDialog::~ExportAnimationDialog()
{
	filterTree=0;
}

BEGIN_EVENT_TABLE(ExportAnimationDialog, wxDialog)
    // begin wxGlade: ExportAnimationDialog::event_table
    EVT_TREE_SEL_CHANGED(ID_FILTER_TREE_CTRL, ExportAnimationDialog::OnFilterTreeCtrlSelChanged)
    EVT_GRID_CMD_EDITOR_SHOWN(ID_PROPERTY_GRID, ExportAnimationDialog::OnFilterGridCellEditorShow)
    EVT_GRID_CMD_EDITOR_SHOWN(ID_ANIMATION_GRID_CTRL, ExportAnimationDialog::OnFrameGridCellEditorShow)
    EVT_GRID_CMD_EDITOR_SHOWN(ID_FILTER_TREE_CTRL, ExportAnimationDialog::OnAnimateGridCellEditorShow)
    EVT_SPLITTER_UNSPLIT(ID_SPLIT_FILTERVIEW, ExportAnimationDialog::OnFilterViewUnsplit) 
    EVT_BUTTON(wxID_REMOVE, ExportAnimationDialog::OnButtonKeyFrameRemove)
    EVT_TEXT(ID_TEXTBOX_WORKDIR, ExportAnimationDialog::OnOutputDirText)
    EVT_BUTTON(wxID_OPEN, ExportAnimationDialog::OnButtonWorkDir)
    EVT_CHECKBOX(ID_CHECK_ONLYDATACHANGE, ExportAnimationDialog::OnCheckOutDataChange)
    EVT_CHECKBOX(ID_CHECK_IMAGE_OUT, ExportAnimationDialog::OnCheckImageOutput)
    EVT_TEXT(ID_TEXTBOX_IMAGEPREFIX, ExportAnimationDialog::OnImageFilePrefix)
    EVT_TEXT(ID_TEXTBOX_IMAGESIZE, ExportAnimationDialog::OnTextImageSize)
    EVT_BUTTON(ID_BUTTON_IMAGE_RES, ExportAnimationDialog::OnBtnResolution)
    EVT_CHECKBOX(ID_CHECK_POINT_OUT, ExportAnimationDialog::OnCheckPointOutput)
    EVT_CHECKBOX(ID_CHECK_PLOT_OUT, ExportAnimationDialog::OnCheckPlotOutput)
    EVT_CHECKBOX(ID_CHECK_VOXEL_OUT, ExportAnimationDialog::OnCheckVoxelOutput)
    EVT_CHECKBOX(ID_CHECK_RANGE_OUT, ExportAnimationDialog::OnCheckRangeOutput)
    EVT_CHOICE(ID_COMBO_RANGE_TYPE, ExportAnimationDialog::OnRangeTypeCombo)
    EVT_COMMAND_SCROLL(ID_FRAME_SLIDER, ExportAnimationDialog::OnFrameViewSlider)
    EVT_TEXT(ID_FRAME_TEXTBOX, ExportAnimationDialog::OnTextFrame)
    EVT_BUTTON(ID_BTN_CANCEL, ExportAnimationDialog::OnButtonCancel)
    EVT_BUTTON(ID_BTN_OK, ExportAnimationDialog::OnButtonOK)
    // end wxGlade
END_EVENT_TABLE();

    
bool ExportAnimationDialog::getModifiedTree(size_t frame, FilterTree &t,bool &needsUp) const
{
	vector<FrameProperties> propsAtFrame;
	vector<size_t> propIds;

	propertyAnimator.getPropertiesAtFrame(frame,propIds,propsAtFrame);

	needsUp=false;
	for(size_t ui=0;ui<propsAtFrame.size();ui++)
	{
		//Get the animated value for the current property
		// we are modifying
		std::string animatedValue;
		animatedValue=propertyAnimator.getInterpolatedFilterData(
					propsAtFrame[ui].getFilterId(),
					propsAtFrame[ui].getPropertyKey(),frame);

		//Set that property in the filter, aborting if unable to 
		// perform the set
		//-------
		//Obtain pointer to filter that we wish to modify
		Filter *filterPtr;
		filterPtr= filterMap.at(propsAtFrame[ui].getFilterId());
		
		bool needUpThisFrame;
		if(!t.setFilterProperty(filterPtr,propsAtFrame[ui].getPropertyKey(),
					animatedValue,needUpThisFrame))
			return false;

		needsUp|=needUpThisFrame;
		//-------
	}

	return true;
}

std::string ExportAnimationDialog::getFilename(unsigned int frame,
		unsigned int type,unsigned int number) const 
{
	std::string s; 
	stream_cast(s,frame); 

	s=workDir+stlStr(wxFileName::GetPathSeparator())+s + string("-");
	switch(type)
	{
		case FILENAME_IMAGE:
			s+=imagePrefix;
			s+=std::string(".png");
			break;
		case FILENAME_IONS:
			s+=std::string(".pos");
			break;
		case FILENAME_RANGE:
		{
			ASSERT(rangeExportMode<RANGE_FORMAT_NUM_OPTIONS);
			
			std::string tmp;
			stream_cast(tmp,number);
			s+=tmp + string(".");
			s+=extension[rangeExportMode];
			break;
		}
		case FILENAME_PLOT:
		{
			std::string tmp;
			stream_cast(tmp,number);
			s+="-" + tmp + ".txt";
			break;
		}
		case FILENAME_VOXEL:
			s+=std::string(".raw");
			break;
		default:
			ASSERT(false);
	}

	return s;
}

void ExportAnimationDialog::prepare() 
{
	vector<const Filter*> dummyVec;
	upWxTreeCtrl(*filterTree,filterTreeCtrl,filterMap,
			dummyVec,NULL);

	update();
}

void ExportAnimationDialog::updateFilterViewGrid()
{
	//Empty the grid
	animationGrid->BeginBatch();
	if(animationGrid->GetNumberRows())
		animationGrid->DeleteRows(0,animationGrid->GetNumberRows());
	animationGrid->EndBatch();


	animationGrid->AppendRows(propertyAnimator.getNumProps());
	for(size_t ui=0;ui<propertyAnimator.getNumProps(); ui++)
	{
		FrameProperties frameProps;
		propertyAnimator.getNthKeyFrame(ui,frameProps);
	
		//Obtain pointer to filter
		const Filter *filterPtr;
		filterPtr= filterMap[frameProps.getFilterId()];

		//Get (generic) properties from that filter
		FilterPropGroup filtPropGroup;
		filterPtr->getProperties(filtPropGroup);
		FilterProperty filtProp;
		filtProp=filtPropGroup.getPropValue(frameProps.getPropertyKey());

		animationGrid->SetCellValue(ui,CELL_FILTERNAME, 
				wxStr(filterPtr->getUserString()));
		animationGrid->SetCellValue(ui,CELL_PROPERTYNAME, 
				wxStr(filtProp.name));
		animationGrid->SetCellValue(ui,CELL_KEYINTERPMODE, 
				wxCStr(INTERP_NAME[frameProps.getInterpMode()]));
		
		string str;
		stream_cast(str,frameProps.getMinFrame());
		animationGrid->SetCellValue(ui,CELL_STARTFRAME, wxStr(str));
		stream_cast(str,frameProps.getMaxFrame());
		animationGrid->SetCellValue(ui,CELL_ENDFRAME, wxStr(str));
	}

	//Check for conflicting rows in the animation dialog,
	// and highlight them in colour
	set<size_t> conflictRows;
	if(!propertyAnimator.checkSelfConsistent(conflictRows))
	{
		existsConflicts=true;
		for(std::set<size_t>::const_iterator it=conflictRows.begin();
				it!=conflictRows.end();++it)
		{
			wxGridCellAttr *colourRowAttr=new wxGridCellAttr;
			colourRowAttr->SetBackgroundColour(wxColour(*wxCYAN));
			animationGrid->SetRowAttr(*it,colourRowAttr);
		}
	}
}

void ExportAnimationDialog::updateFrameViewGrid()
{
	ASSERT(currentFrame <= frameSlider->GetMax());
	
	//Empty the grid
	framePropGrid->BeginBatch();
	if(framePropGrid->GetNumberRows())
		framePropGrid->DeleteRows(0,animationGrid->GetNumberRows());
	framePropGrid->EndBatch();

	std::set<size_t> conflictProps;
	if(!propertyAnimator.checkSelfConsistent(conflictProps))
		return;

	vector<FrameProperties> alteredProperties;
	vector<size_t> propertyIds;
	//Grab the properties that have been modified from their initial value 
	// and then refill the grid with this data
	propertyAnimator.getPropertiesAtFrame(currentFrame, 
				propertyIds,alteredProperties);
	
	framePropGrid->AppendRows(alteredProperties.size());

	for(size_t ui=0; ui<alteredProperties.size();ui++)
	{
		//Obtain filter and property name for the current property
		//--
		std::string filterName,propertyName;
		
		//Get name
		size_t idFilter;
		idFilter=alteredProperties[ui].getFilterId();
		filterName= filterMap[idFilter]->getUserString();

		
		FilterPropGroup p;
		FilterProperty prop;
		//obtain filter properties 
		filterMap.at(idFilter)->getProperties(p);
		prop=p.getPropValue(alteredProperties[ui].getPropertyKey());
		propertyName=prop.name;


		std::string animatedValue;
		animatedValue=propertyAnimator.getInterpolatedFilterData(
				alteredProperties[ui].getFilterId(),prop.key,currentFrame);
		//--

		//Modify the grid properties with the appropriate data
		framePropGrid->SetCellValue(ui,FRAME_CELL_FILTERNAME, 
				wxStr(filterName));
		framePropGrid->SetCellValue(ui,FRAME_CELL_PROPNAME, 
				wxStr(propertyName));
		framePropGrid->SetCellValue(ui,FRAME_CELL_VALUE, 
				wxStr(animatedValue));
	}

}

void ExportAnimationDialog::updateFrameViewSlider()
{

	programmaticEvent=true;

	//reset the range on the frame slider
	size_t maxFrameVal;
	maxFrameVal=propertyAnimator.getMaxFrame();
	frameSlider->SetMin(0);
	frameSlider->SetMax(maxFrameVal);

	//Update the textbox
	std::string textCurrent,textMax;
	stream_cast(textCurrent,frameSlider->GetValue());
	stream_cast(textMax,propertyAnimator.getMaxFrame());

	textCurrent= (textCurrent + std::string("/") + textMax);
	textFrame->SetValue( wxStr(textCurrent));

	programmaticEvent=false;

}

void ExportAnimationDialog::OnTextFrame(wxCommandEvent &event)
{
	if(programmaticEvent)
		return;

	string s;
	s=stlStr(textFrame->GetValue());
	
	bool parseOK=true;
	size_t pos;
	size_t frameCur;
	pos = s.find('/'); 
	if(pos==string::npos)
		parseOK=false;
	else
	{
		string first,last;
		first = s.substr(0,pos);
		last=s.substr(pos+1);


		if(stream_cast(frameCur,first))
			parseOK=false;

	
		size_t frameEnd;	
		if(stream_cast(frameEnd,last))
			parseOK=false;
	}

	if(!parseOK)
		textFrame->SetBackgroundColour(*wxCYAN);
	else
	{
		textFrame->SetBackgroundColour(wxNullColour);

		currentFrame=frameCur;

		updateFrameViewGrid();
	}
    	imageSizeOK=parseOK;

	update();
}

void ExportAnimationDialog::OnFrameViewSlider(wxScrollEvent &event)
{
	programmaticEvent=true;
	size_t sliderVal=frameSlider->GetValue();


	std::string frameText,tmp;
	stream_cast(frameText,sliderVal);
	frameText+= "/";
	stream_cast(tmp,frameSlider->GetMax());
	frameText+=tmp;

	textFrame->SetValue( wxStr(frameText));

	currentFrame=sliderVal;
	updateFrameViewGrid();
	programmaticEvent=false;
}

void ExportAnimationDialog::OnButtonCancel(wxCommandEvent &event)
{
    event.Skip();
}
 
void ExportAnimationDialog::update()
{
	programmaticEvent=true;
	updateFilterViewGrid();
	updateFrameViewGrid();
	updateFrameViewSlider();
	updateOKButton();
	programmaticEvent=false;
}

void ExportAnimationDialog::OnFilterTreeCtrlSelChanged(wxTreeEvent &event)
{
	//Get the parent filter pointer
	wxTreeItemId id=filterTreeCtrl->GetSelection();;

	if(id !=filterTreeCtrl->GetRootItem() && id.IsOk())
	{
		wxTreeItemData *parentData=filterTreeCtrl->GetItemData(id);
		updateFilterPropertyGrid(propertyGrid, 
				filterMap[((wxTreeUint *)parentData)->value]);
	}

	event.Skip();
}


void ExportAnimationDialog::OnFilterGridCellEditorShow(wxGridEvent &event)
{
	event.Veto();
	wxTreeItemId id=filterTreeCtrl->GetSelection();;

	if(id ==filterTreeCtrl->GetRootItem() || !id.IsOk())
		return;


	unsigned int key;
	key=propertyGrid->getKeyFromRow(event.GetRow());

	//Get the filter ID value 
	size_t filterId;
	wxTreeItemId tId = filterTreeCtrl->GetSelection();
	if(!tId.IsOk())
		return;

	wxTreeItemData *tData=filterTreeCtrl->GetItemData(tId);
	filterId = ((wxTreeUint *)tData)->value;

	const Filter *f;
	f=filterMap.at(filterId);
	
	//Obtain the filter property that was selected by the user 
	//--
	FilterPropGroup propGroup;
	f->getProperties(propGroup);

	FilterProperty filterProp;
	filterProp=propGroup.getPropValue(key);
	//--

	//Create a property entry for this, and get values from user
	FrameProperties frameProp(filterId,key);
	switch(filterProp.type)
	{
		case PROPERTY_TYPE_BOOL:
		{
			wxTextEntryDialog *teD = new wxTextEntryDialog(this,wxTRANS("transition frame"),wxTRANS("Frame count"),
						wxT("0"),(long int)wxOK|wxCANCEL);
	
			std::string s;
			size_t frameValue;
			do
			{
				if(teD->ShowModal() == wxID_CANCEL)
				{
					teD->Destroy();
					return;
				}

				s=stlStr(teD->GetValue());
			}
			while(stream_cast(frameValue,s));
			ASSERT(filterProp.data == "1" || filterProp.data == "0");


			//Find the last property for the filter from the animator
			// if available
			std::string sData;
			sData=propertyAnimator.getInterpolatedFilterData(filterId,
					key,std::min(frameValue,
						propertyAnimator.getMaxFrame()));

			if(!sData.empty())
			{
				ASSERT(sData == "0" || sData == "1");
				filterProp.data=sData;
			}
		
			//Flip the data
			if(filterProp.data == "1")
				filterProp.data="0";
			else
				filterProp.data="1";

			frameProp.setInterpMode(INTERP_STEP);
			frameProp.addKeyFrame(frameValue,filterProp);
			
			teD->Destroy();
			break;
		}
		case PROPERTY_TYPE_CHOICE:
		{
			vector<string> choiceVec;
			unsigned int activeChoice;
			choiceStringToVector(filterProp.data,choiceVec,activeChoice);

			ChoiceKeyFrameDialog *cD = new ChoiceKeyFrameDialog(this,
								wxID_ANY,wxT(""));
			cD->setChoices(choiceVec);

			if( cD->ShowModal() != wxID_OK)
			{
				cD->Destroy();
				return;
			}

			filterProp.data=cD->getChoice();
			frameProp.setInterpMode(INTERP_STEP);
			frameProp.addKeyFrame(cD->getStartFrame(),filterProp);

			cD->Destroy();


			break;
		}
		case PROPERTY_TYPE_COLOUR:
		{
			ColourKeyFrameDialog *colDlg = new ColourKeyFrameDialog(this,
					wxID_ANY,wxTRANS("Key frame : Colour")) ;
			if( colDlg->ShowModal() != wxID_OK)
			{
				colDlg->Destroy();
				return;
			}

			//Copy out the data obtained from the dialog
			size_t transitionMode;
			transitionMode=colDlg->getTransitionMode();
			
			frameProp.setInterpMode(transitionMode);
			filterProp.data=colDlg->getStartValue();

			frameProp.addKeyFrame(colDlg->getStartFrame(),filterProp);
			//Add end value as needed
			switch(transitionMode)
			{
				case TRANSITION_STEP:
					break;
				case TRANSITION_INTERP:
					filterProp.data=colDlg->getEndValue();
					frameProp.setInterpMode(INTERP_LINEAR_COLOUR);
					frameProp.addKeyFrame(colDlg->getEndFrame(),filterProp);
					break;
				default:
					ASSERT(false);
			}
			colDlg->Destroy();

			break;
		}
		case PROPERTY_TYPE_STRING:
		{
			//Create and show the string keyframe input dialog
			StringKeyFrameDialog *sd = new StringKeyFrameDialog(this,
					wxID_ANY,wxT(""));

			if(sd->ShowModal() != wxID_OK)
			{
				sd->Destroy();
				return;
			}

			//set the interpolator to step-by-step interp
			frameProp.setInterpMode(INTERP_LIST);

			//Grab the data vector we need to insert the keyframes
			vector<string> dataVec;
			if(!sd->getStrings(dataVec))
			{
				sd->Destroy();
				wxMessageDialog *wxD  =new wxMessageDialog(this,
						wxTRANS("File existed, but was unable to read or interpret file contents."), 
						wxTRANS("String load failed"),wxICON_ERROR|wxOK);

				wxD->ShowModal();
				wxD->Destroy();
				return;
			}
			
			for(size_t ui=0;ui<dataVec.size();ui++)
			{
				filterProp.data=dataVec[ui];
				frameProp.addKeyFrame(sd->getStartFrame()+ui,filterProp);
			}

			sd->Destroy();


			break;
		}
		case PROPERTY_TYPE_REAL:
		{
			RealKeyFrameDialog<float> *r = new RealKeyFrameDialog<float>(this,
					wxID_ANY,wxTRANS("Keyframe : decimal"));
			if(!getRealKeyFrame(frameProp,filterProp,r))
				return;
			frameProp.setInterpMode(r->getTransitionMode());
			break;
		}
		case PROPERTY_TYPE_INTEGER:
		{
			RealKeyFrameDialog<int> *r = new RealKeyFrameDialog<int>(this,
					wxID_ANY,wxTRANS("Keyframe : integer"));
			if(!getRealKeyFrame(frameProp,filterProp,r))
				return;
			frameProp.setInterpMode(r->getTransitionMode());
			break;
		}
		case PROPERTY_TYPE_POINT3D:
		{
			RealKeyFrameDialog<Point3D> *r = new RealKeyFrameDialog<Point3D>(this,
					wxID_ANY,wxTRANS("Keyframe : 3D Point"));
			if(!getRealKeyFrame(frameProp,filterProp,r))
				return;
			//Animator needs special Linear ramping code, so select that
			// if user chooses a linear ramp
			if(frameProp.getInterpMode()==INTERP_LINEAR_FLOAT)
				frameProp.setInterpMode(INTERP_LINEAR_POINT3D);
			else
				frameProp.setInterpMode(r->getTransitionMode());

			break;
		}
		default:
			ASSERT(false); // that should cover all data types...
	}

	//Add property to animator
	propertyAnimator.addProp(frameProp);

	//update the user interface controls
	update();

}

void ExportAnimationDialog::OnFrameGridCellEditorShow(wxGridEvent &event)
{
	event.Veto();
}

void ExportAnimationDialog::OnFilterViewUnsplit(wxSplitterEvent &evt)
{
	evt.Veto();
}

void ExportAnimationDialog::OnAnimateGridCellEditorShow(wxGridEvent &event)
{
	event.Veto();
}

void ExportAnimationDialog::OnButtonKeyFrameRemove(wxCommandEvent &event)
{
	if(!animationGrid->GetNumberRows())
		return;
	//Obtain the IDs of the selected rows, or partially selected rows

	
	//Rectangular selection
	// This is an undocumented class AFAIK. :(
	wxGridCellCoordsArray arrayTL(animationGrid->GetSelectionBlockTopLeft());
	wxGridCellCoordsArray arrayBR(animationGrid->GetSelectionBlockBottomRight());

	//Row prefix or header selection
	const wxArrayInt& selectedRows(animationGrid->GetSelectedRows());
	const wxGridCellCoordsArray& cells(animationGrid->GetSelectedCells());

	vector<size_t> rowsToKill;
	
	if(arrayTL.Count() && arrayBR.Count())
	{
		wxGridCellCoords coordTL = arrayTL.Item(0);
		wxGridCellCoords coordBR = arrayBR.Item(0);

		size_t rows = coordBR.GetRow() - coordTL.GetRow() +1;

		rowsToKill.resize(rows);

		for(size_t r=0; r<rows; r++)
			rowsToKill[r]= coordTL.GetRow() + r;

	}
	else if(selectedRows.size()) //Selection from table row prefix
	{
		rowsToKill.resize(selectedRows.size());

		for(size_t ui=0;ui<selectedRows.size();ui++)
			rowsToKill[ui]=selectedRows[ui];
	}
	else
		rowsToKill.push_back(animationGrid->GetGridCursorRow());
	
	propertyAnimator.removeKeyFrames(rowsToKill);
	//update user interface
	update();

}

void ExportAnimationDialog::updateOKButton()
{
	bool badStatus=false;
	
	badStatus|=workDir.empty();
	badStatus|=filterMap.empty();
	badStatus|=(imagePrefix.empty() && wantImageOutput);
	badStatus|=(propertyAnimator.getNumProps() == 0);

	//Ensure that there were no inconsistent properties in
	// the animation
	std::set<size_t> inconsistentProps;
	badStatus|=!propertyAnimator.checkSelfConsistent(inconsistentProps);
	okButton->Enable(!badStatus);
}

void ExportAnimationDialog::OnOutputDirText(wxCommandEvent &event)
{
	if(programmaticEvent)
		return;
	
	if(!wxDirExists(textWorkDir->GetValue()))
	{
		textWorkDir->SetBackgroundColour(*wxCYAN);
		workDir.clear();
	}
	else 
	{
		textWorkDir->SetBackgroundColour(wxNullColour);
		workDir=stlStr(textWorkDir->GetValue());
	}
	
	//update the user interface controls
	update();
}


void ExportAnimationDialog::OnButtonWorkDir(wxCommandEvent &event)
{
	//Pop up a directory dialog, to choose the base path for the new folder
	wxDirDialog *wxD = new wxDirDialog(this, wxTRANS("Select or create new folder"),
	wxFileSelectorDefaultWildcardStr, wxFD_SAVE);

	unsigned int res;
	res = wxD->ShowModal();

	while(res != wxID_CANCEL)
	{
		//If dir exists, exit
		if(wxDirExists(wxD->GetPath()))
			break;

		res=wxD->ShowModal();
	}

	//User aborted directory choice.
	if(res==wxID_CANCEL)
	{
		wxD->Destroy();
		return;
	}

	textWorkDir->SetValue(wxD->GetPath());
	workDir=stlStr(textWorkDir->GetValue());
	wxD->Destroy();
	
	//update the user interface controls
	update();
}

void ExportAnimationDialog::OnCheckOutDataChange(wxCommandEvent &event)
{
	if(programmaticEvent)
		return;

	wantOnlyChanges=checkOutOnlyChanged->IsChecked();
}

void ExportAnimationDialog::OnCheckImageOutput(wxCommandEvent &event)
{
	if(programmaticEvent)
		return;

	wantImageOutput=checkImageOutput->IsChecked();
	//update UI (eg OK button)
	update();
}


void ExportAnimationDialog::OnImageFilePrefix(wxCommandEvent &event)
{
	if(programmaticEvent)
		return;
	
	imagePrefix=stlStr(textImageName->GetValue());

	//update UI (eg OK button)
	update();
}


void ExportAnimationDialog::OnTextImageSize(wxCommandEvent &event)
{

	if(programmaticEvent)
		return;

	string s;
	s=stlStr(textImageSize->GetValue());
	
	bool parseOK=true;
	size_t pos;
	pos = s.find('x'); 
	if(pos==string::npos)
		parseOK=false;
	else
	{
		string first,last;
		first = s.substr(0,pos);
		last=s.substr(pos+1);


		size_t w,h;
		if(stream_cast(w,first))
			parseOK&=false;

		
		if(stream_cast(h,last))
			parseOK&=false;
	}

	if(!parseOK)
		textImageSize->SetBackgroundColour(*wxCYAN);
	else
		textImageSize->SetBackgroundColour(wxNullColour);
	
	//update UI (eg OK button)
	update();

}

void ExportAnimationDialog::OnBtnResolution(wxCommandEvent &event)
{
	ResolutionDialog *r = new ResolutionDialog(this,wxID_ANY,wxT("Choose Resolution"));

	r->setRes(imageWidth,imageHeight,true);

	if(r->ShowModal() != wxID_OK)
	{
		r->Destroy();
		return;
	}

	imageWidth=r->getWidth();
	imageHeight=r->getHeight();

	string sWidth,sHeight;
	stream_cast(sWidth,imageWidth);
	stream_cast(sHeight,imageHeight);
	
	string s;
	s=sWidth+"x" + sHeight;
	textImageSize->SetValue(wxStr(s));

	r->Destroy();
}


void ExportAnimationDialog::OnCheckPointOutput(wxCommandEvent &event)
{
	wantIonOutput=checkPoints->IsChecked();
}


void ExportAnimationDialog::OnCheckPlotOutput(wxCommandEvent &event)
{
	wantPlotOutput=checkPlotData->IsChecked();
}


void ExportAnimationDialog::OnCheckVoxelOutput(wxCommandEvent &event)
{
	wantVoxelOutput=checkVoxelData->IsChecked();
}


void ExportAnimationDialog::OnCheckRangeOutput(wxCommandEvent &event)
{
	wantRangeOutput=checkRangeData->IsChecked();

	comboRangeFormat->Enable(checkRangeData->IsChecked());
}

void ExportAnimationDialog::OnRangeTypeCombo(wxCommandEvent &event)
{
	rangeExportMode=event.GetSelection();
}


void ExportAnimationDialog::OnButtonOK(wxCommandEvent &event)
{
    event.Skip();
}

size_t ExportAnimationDialog::getRangeFormat() const
{
	return rangeExportMode;	

}

// wxGlade: add ExportAnimationDialog event handlers


void ExportAnimationDialog::set_properties()
{
    // begin wxGlade: ExportAnimationDialog::set_properties
    SetTitle(wxTRANS("Export Animation"));
    filterTreeCtrl->SetToolTip(wxTRANS("Select filter"));
    propertyGrid->CreateGrid(0, 2);
    propertyGrid->SetColLabelValue(0, wxTRANS("Property"));
    propertyGrid->SetColLabelValue(1, wxTRANS("Value"));
    propertyGrid->SetToolTip(wxTRANS("Select property"));
    animationGrid->CreateGrid(0, 5);
    animationGrid->SetColLabelValue(0, wxTRANS("Filter"));
    animationGrid->SetColLabelValue(1, wxTRANS("Property"));
    animationGrid->SetColLabelValue(2, wxTRANS("Mode"));
    animationGrid->SetColLabelValue(3, wxTRANS("Start Frame"));
    animationGrid->SetColLabelValue(4, wxTRANS("End Frame"));
    animationGrid->SetToolTip(wxTRANS("Keyframe table"));
    keyFrameRemoveButton->SetToolTip(wxTRANS("Remove the selected keyframe from the table"));
    textWorkDir->SetToolTip(wxTRANS("Enter where the animation frames will be exported to"));
    buttonWorkDir->SetToolTip(wxTRANS("Browse to directory where the animation frames will be exported to"));
    checkImageOutput->SetValue(1);
    textImageName->SetToolTip(wxTRANS("Enter a descriptive name for output files"));
    textImageSize->SetToolTip(wxTRANS("Enter the target resoltuion (image size)"));
    comboRangeFormat->SetSelection(-1);
    frameSlider->SetToolTip(wxTRANS("Select frame for property display"));
    textFrame->SetToolTip(wxTRANS("Enter frame number to change frame (eg 1/20)"));
    checkPoints->SetToolTip(wxTRANS("Save point data (POS files) in output folder?"));
    checkPlotData->SetToolTip(wxTRANS("Save plots (as text files) in output folder?"));
    checkVoxelData->SetToolTip(wxTRANS("Save voxel data (raw files) in output folder?"));
    checkRangeData->SetToolTip(wxTRANS("Save range files  in output folder?"));
    framePropGrid->CreateGrid(0, 3);
    framePropGrid->SetColLabelValue(0, wxTRANS("Filter"));
    framePropGrid->SetColLabelValue(1, wxTRANS("Property"));
    framePropGrid->SetColLabelValue(2, wxTRANS("Value"));
    framePropGrid->SetToolTip(wxTRANS("Animation parameters for current frame"));
    cancelButton->SetToolTip(wxTRANS("Abort animation"));
    okButton->SetToolTip(wxTRANS("Run Animation"));
    // end wxGlade
}


void ExportAnimationDialog::do_layout()
{
    // begin wxGlade: ExportAnimationDialog::do_layout
    wxBoxSizer* animateSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* globalButtonSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* frameViewSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* propGridSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* frameSliderSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticBoxSizer* outputDataSizer = new wxStaticBoxSizer(outputDataSizer_staticbox, wxVERTICAL);
    wxBoxSizer* rangeFileDropDownSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* outputImageOptionsSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* imageSizeSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* filePrefixSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizer_1 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* outputDirHorizSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* filterViewPaneSizerH = new wxBoxSizer(wxHORIZONTAL);
    wxStaticBoxSizer* keyFramesSizer = new wxStaticBoxSizer(keyFramesSizer_staticbox, wxVERTICAL);
    wxBoxSizer* keyFrameButtonSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* animationGridSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticBoxSizer* filterPropertySizer = new wxStaticBoxSizer(filterPropertySizer_staticbox, wxVERTICAL);
    filterPropertySizer->Add(filterTreeCtrl, 1, wxALL|wxEXPAND, 3);
    filterPropertySizer->Add(propertyGrid, 1, wxALL|wxEXPAND, 3);
    filterLeftPane->SetSizer(filterPropertySizer);
    animationGridSizer->Add(animationGrid, 1, wxALL|wxEXPAND, 3);
    keyFramesSizer->Add(animationGridSizer, 1, wxEXPAND, 0);
    keyFrameButtonSizer->Add(keyFrameRemoveButton, 0, wxALIGN_CENTER_HORIZONTAL, 0);
    keyFramesSizer->Add(keyFrameButtonSizer, 0, wxALL|wxEXPAND, 3);
    filterRightPane->SetSizer(keyFramesSizer);
    splitPaneFilter->SplitVertically(filterLeftPane, filterRightPane);
    filterViewPaneSizerH->Add(splitPaneFilter, 1, wxEXPAND, 0);
    filterViewPane->SetSizer(filterViewPaneSizerH);
    outputDirHorizSizer->Add(labelWorkDir, 0, wxLEFT|wxRIGHT|wxALIGN_CENTER_VERTICAL, 3);
    outputDirHorizSizer->Add(textWorkDir, 1, wxEXPAND, 0);
    outputDirHorizSizer->Add(buttonWorkDir, 0, wxLEFT|wxRIGHT, 2);
    outputDataSizer->Add(outputDirHorizSizer, 0, wxALL|wxEXPAND, 4);
    sizer_1->Add(20, 20, 0, 0, 0);
    sizer_1->Add(checkOutOnlyChanged, 0, 0, 0);
    outputDataSizer->Add(sizer_1, 0, wxBOTTOM|wxEXPAND, 5);
    outputDataSizer->Add(outputDataSepLine, 0, wxTOP|wxBOTTOM|wxEXPAND, 3);
    outputDataSizer->Add(labelDataType, 0, wxTOP|wxBOTTOM, 4);
    outputDataSizer->Add(checkImageOutput, 0, wxLEFT|wxTOP, 3);
    filePrefixSizer->Add(lblImageName, 0, wxLEFT|wxALIGN_CENTER_VERTICAL, 3);
    filePrefixSizer->Add(textImageName, 1, wxALL|wxEXPAND, 4);
    outputImageOptionsSizer->Add(filePrefixSizer, 0, wxALL|wxEXPAND, 3);
    imageSizeSizer->Add(labelImageSize, 0, wxLEFT|wxALIGN_CENTER_VERTICAL, 3);
    imageSizeSizer->Add(textImageSize, 1, wxALL|wxEXPAND, 4);
    imageSizeSizer->Add(buttonImageSize, 0, wxALL, 4);
    outputImageOptionsSizer->Add(imageSizeSizer, 0, wxALL|wxEXPAND, 3);
    outputDataSizer->Add(outputImageOptionsSizer, 0, wxEXPAND, 0);
    outputDataSizer->Add(checkPoints, 1, wxLEFT|wxBOTTOM, 3);
    outputDataSizer->Add(checkPlotData, 1, wxLEFT|wxTOP, 3);
    outputDataSizer->Add(checkVoxelData, 1, wxLEFT|wxTOP|wxBOTTOM, 3);
    outputDataSizer->Add(checkRangeData, 1, wxALL, 3);
    rangeFileDropDownSizer->Add(labelRangeFormat, 0, wxLEFT|wxRIGHT|wxALIGN_CENTER_VERTICAL, 3);
    rangeFileDropDownSizer->Add(comboRangeFormat, 1, wxLEFT|wxRIGHT|wxEXPAND, 3);
    outputDataSizer->Add(rangeFileDropDownSizer, 0, wxLEFT|wxBOTTOM|wxEXPAND, 5);
    outputDataSizer->Add(20, 20, 2, 0, 0);
    frameViewSizer->Add(outputDataSizer, 1, wxEXPAND, 0);
    frameViewSizer->Add(static_line_1, 0, wxLEFT|wxRIGHT|wxEXPAND, 5);
    frameSliderSizer->Add(labelFrame, 0, wxALIGN_CENTER_VERTICAL, 0);
    frameSliderSizer->Add(frameSlider, 1, wxLEFT|wxRIGHT|wxALIGN_CENTER_VERTICAL, 5);
    frameSliderSizer->Add(textFrame, 0, wxALIGN_CENTER_VERTICAL, 0);
    propGridSizer->Add(frameSliderSizer, 0, wxALL|wxEXPAND, 3);
    propGridSizer->Add(framePropGrid, 1, wxEXPAND, 0);
    frameViewSizer->Add(propGridSizer, 2, wxALL|wxEXPAND, 3);
    frameViewPane->SetSizer(frameViewSizer);
    viewNotebook->AddPage(filterViewPane, wxTRANS("Filter view"));
    viewNotebook->AddPage(frameViewPane, wxTRANS("Frame view"));
    animateSizer->Add(viewNotebook, 1, wxEXPAND, 0);
    globalButtonSizer->Add(20, 1, 1, wxEXPAND, 0);
    globalButtonSizer->Add(cancelButton, 0, wxALL|wxALIGN_BOTTOM, 3);
    globalButtonSizer->Add(okButton, 0, wxALL|wxALIGN_BOTTOM, 3);
    animateSizer->Add(globalButtonSizer, 0, wxEXPAND, 0);
    SetSizer(animateSizer);
    animateSizer->Fit(this);
    Layout();
    // end wxGlade
}

