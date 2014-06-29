#!/usr/bin/python
#
#	test.py - Automated UI testing for 3depict
#	Copyright (C) 2014, D Haley 

#	This program is free software: you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation, either version 3 of the License, or
#	(at your option) any later version.

#	This program is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.

#	You should have received a copy of the GNU General Public License
#	along with this program.  If not, see <http://www.gnu.org/licenses/>.
#


import os
from os import environ

import traceback
import subprocess
import time
import tempfile
import struct
import array
import random

#Dogtail imports
import dogtail
from dogtail.config import config
from dogtail import dump
from dogtail import rawinput
from dogtail import sessions 
from dogtail.procedural import *
from dogtail.utils import screenshot
from dogtail.predicate import GenericPredicate

#Python magick - wrapper for imagemagick (no docs, afaik)
import PythonMagick as Magick

#HELPER FUNCTIONS
#---
#Create a predicate from name/role
def element(name,role,description=""):
	return predicate.GenericPredicate(name,role,description)

#launch the colour dialog from the main window
def loadColDlg(app3depict):
	viewMenu=app3depict.menu('View');
	viewMenu.click();
	backItem = viewMenu.menuItem("Background Colour...")
	backItem.click()

	colDlg = app3depict.findChild(element("Choose colour","color chooser"))
	return colDlg

def sameImage(fileA,fileB):
	imA=Magick.Image(fileA);
	imB=Magick.Image(fileB);

	return  imA == imB

def appShot(application,ycrop=0):
	time.sleep(0.3) # Wait a fraction of second, in case image is not very static
	shot=screenshot()
	mainFrame=application.findChild(element("","frame"))
	xyStart=mainFrame.position;
	xySize=mainFrame.size
	
	im=Magick.Image(shot);
	im.crop(Magick.Geometry(xySize[0],xySize[1]-ycrop,xyStart[0],xyStart[1]))
	im.write(shot)

	return shot
def find(target, someList):
	for item in someList:
		if target == item: 
			return True
	return False


#---

#Close the program
def closeProgram(app3depict):
	fileMenu=app3depict.menu('File')
	fileMenu.click();
	exitMenu=app3depict.menuItem('Exit');

	exitMenu.click()	

	try:
		reqDialog=app3depict.findChild(element("","alert"),retry=False)
		#For some reason, pressing the dialog contents using .button(...) does nothing
		# manually use keycombo to quit
		if reqDialog.name == "Error" : 
			dogtail.rawinput.keyCombo("<alt>o")

	except:
		reqDialog=None	

#Try executing/aborting the file->open menu
def openTest(app3depict):
	global overrideImage	
	
	shotBefore=appShot(app3depict)

	fileMenu=app3depict.menu('File')
	fileMenu.click();
	openMenu=fileMenu.menuItem('Open...')
	openMenu.click();

	saveas = app3depict.dialog('Select Data or State File...')

	
	tmp=tempfile.NamedTemporaryFile(delete=False)
	tmp.close()
	filename=tmp.name+'.pos'
	writeRandomPos(filename,100)

	#Force gnome's annoying dialog to actually display a
	# place to type
	hddButton=saveas.findChild(element("File System","table cell"));
	hddButton.click()

	fileText=saveas.findChild(element("","text"));
	fileText.typeText(filename)

	saveas.button('Open').click()
	
	shotAfter=appShot(app3depict)

	#Point data should have loaded
	if(sameImage(shotBefore,shotAfter) and ( not overrideImage)):
		return False
	
	

	#FIXME: 3Depict bug! shouldn't need to tap space
	# after loading file - should auto-snap to bounds
	#Press space to tap 
	dogtail.rawinput.pressKey("space")
	return True

def filterTests(app3depict):
	global overrideImage
	
	#Fudge the bounding box of the
	# cropping region to exclude stuff like statusbar
	# - autosave, progress etc can affect this
	CROP_FUDGE=60
	
	#Locate the data tab
	dataTab=app3depict.findChild(element("Data","page tab"))

	#locate listbox with filters inside
	filterText=dataTab.findChild(element("","text","List of available filters"))
	filterBox=filterText.findAncestor(element("","combo box"))

	xy=filterBox.position

	#move 5px beyond the box so we can click the down arrow	
	xPlus=xy[0]+filterBox.size[0] -3 
	yPlus=xy[1] + filterBox.size[1]/2

	#locate filter tree
	filterTree=dataTab.findChild(element("","panel","Tree - drag to move items, hold Ctrl for copy. Tap delete to remove items."));
	
	#We can't interact with the tree using dogtail's
	# accessibility stuff - the tree doens't appear.
	# So just use the dropdown with undo
	

	dropdownBox=filterBox.menu("")
	dropItems=dropdownBox.findChildren(element("","menu item"))

	editMenu=app3depict.menu("Edit");
	undoMenuItem=editMenu.menuItem("Undo");

	#Single item on each 
	for i in dropItems:
		#Range filter pops up a dialog, 
		# needs special processing
		if i.name == "Range File":
			continue

		#Node is named "pos data", press "p" to activate it
		filterTree.click()
		dogtail.rawinput.pressKey("p")
		dogtail.rawinput.pressKey("space")

		dogtail.rawinput.click(xPlus,yPlus)
		shotBefore=appShot(app3depict,CROP_FUDGE)
		i.click()

		filterTree.click()
		dogtail.rawinput.pressKey("space")
		shotAfter=appShot(app3depict,CROP_FUDGE)
		
		if(sameImage(shotBefore,shotAfter) and not (overrideImage) ):
			print("Image didn't change before/after filter")
			return False

		#Tap the refresh shortcut, then refocus dialog to
		# kill tooltip
		dogtail.rawinput.pressKey("F5")
		
		time.sleep(2) #Wait for refresh to complete 
		dogtail.rawinput.pressKey("space") #Tap "neutral key" to kill tooltip
		shotAfterRefresh=appShot(app3depict,CROP_FUDGE);

		if not sameImage(shotAfterRefresh,shotAfter) and not (overrideImage):
			print("Image Changed when refresh from cache")
			return False
	
		editMenu.click()
		undoMenuItem.click()


	SHOT_BLACKLIST=["Annotation" ]
	#Select the pos data item again
	filterTree.click()
	dogtail.rawinput.pressKey("p")
	for i in dropItems:
		
		shotBefore=appShot(app3depict,CROP_FUDGE)
		#Range filter pops up a dialog, 
		# needs special processing
		if i.name == "Range File":
			continue

		dogtail.rawinput.click(xPlus,yPlus)
		i.click()

		shotAfter=appShot(app3depict,CROP_FUDGE)
		
		#For most filters, there should be a visible change
		if not find(i.name,SHOT_BLACKLIST) :
			if(sameImage(shotBefore, shotAfter) and not overrideImage):
				return False

		#Tap the refresh shortcut
		dogtail.rawinput.pressKey("F5")
		time.sleep(5) #Wait for refresh to complete 
		dogtail.rawinput.pressKey("space") #Tap "neutral key" to kill tooltip
		shotAfterRefresh=appShot(app3depict,CROP_FUDGE);

		#Pressing refresh should do nothing
		if not sameImage(shotAfter,shotAfter) and not overrideImage:
			return False

	
	filterTree.click()
	dogtail.rawinput.pressKey("p")
	
	dogtail.rawinput.pressKey("del")
	
	return True


#Try opening and closing panels with the menu,
# check for change in GUI
def panelTests(app3depict):
	global overrideImage
	viewMenu=app3depict.menu('View');

	PANEL_TOGGLES=["Control Pane", "Raw Data Pane"]
	for i in PANEL_TOGGLES:
		shotBefore=appShot(app3depict);
		viewMenu.click();
		viewMenu.menuItem(i).click();
		shotAfter=appShot(app3depict);
		
		viewMenu.click();
		viewMenu.menuItem(i).click();

		if sameImage(shotBefore,shotAfter) and not overrideImage:
			print ('UI did not change, but should have:')
			return False
	return True

#Try opening preferences menu, and activating a few items
def prefTests(app3depict):
	prefMenu=app3depict.menu("Edit");
	prefMenu.click();
	editMenu=prefMenu.menuItem("Preferences");
	editMenu.click();

	prefDialog=app3depict.findChild(element("Preferences","frame"));

	#Get defaults tab
	tabFilterDefaults=prefDialog.findChild(element("Filt. Default","page tab",));
	#Walk through all the filter entries in the list, clicking each one
	listFilters=tabFilterDefaults.findChild(element("","table"))
	listFilterEntries=tabFilterDefaults.findChildren(element("","table cell"))	

	tmpDelay=dogtail.config.actionDelay;
	dogtail.config.actionDelay=0.05;
	for i in listFilterEntries:
		i.click()
	tmpDelay=tmpDelay;


	prefDialog.button("Cancel").click()

	return True


#Try changing some of the view options
def viewTests(app3depict):
	global overrideImage
	#Show colour dialog
	colDlg = loadColDlg(app3depict) 

	#Set the colour to black, remembering orig
	#--------
	#The first text field is the colour hex code field
	hexText=colDlg.findChild(element("","text"))
	origHex=hexText.text
	hexText.typeText("#000000")
	colDlg.button("OK").click()
	#--------
	
	#Take screenshot
	shotBefore= appShot(app3depict)

	#Show dialog
	colDlg = loadColDlg(app3depict) 

	#Set colour to green
	#------
	hexText=colDlg.findChild(element("","text"))
	hexText.typeText("#22aa22")
	colDlg.button("OK").click()

	shotAfter = appShot(app3depict)

	#Colour should have changed
	if sameImage(shotAfter, shotBefore ) and not overrideImage:
		return False
	
	#Set the colour back to orig
	colDlg=loadColDlg(app3depict)
	hexText=colDlg.findChild(element("","text"))
	hexText.typeText(origHex)
	colDlg.button("OK").click()


	#Toggle axis
	shotBefore=appShot(app3depict)
	
	viewMenu=app3depict.menu('View');
	viewMenu.click()
	axisItem=viewMenu.menuItem("Axis")
	axisItem.click()
	
	shotAfter=appShot(app3depict)

	#Screenshot should have changed
	if sameImage(shotBefore, shotAfter) and not overrideImage:
		return False

	#Restore axis
	viewMenu.click()
	axisItem.click()


	#Play with fullscreen
	shotBefore=appShot(app3depict)
	viewMenu.click()
	fullscrItem=viewMenu.menuItem("Fullscreen mode")
	fullscrItem.click()

	shotAfter=appShot(app3depict)

	if(sameImage(shotBefore,shotAfter)) and not overrideImage:
		return False
	

	#Leave fullscreen
	dogtail.rawinput.pressKey("F11")
	shotAfter=appShot(app3depict)
	if(sameImage(shotAfter,shotBefore)) and not overrideImage:
		return False

	return True

#Try saving an image, and see if the file turns up
def imageTest(app3depict):
	dogtail.rawinput.keyCombo('<Control>i')
	
	saveas = app3depict.dialog('Save Image...')
	saveLocText=saveas.findChild(element("","text"))

	tmp=tempfile.NamedTemporaryFile(delete=False)
	tmp.close()
	strDogSave=tmp.name + "-shot.png"
	saveLocText.typeText(strDogSave)
	
	saveas.button("Save").click()
	
	
	ressel= app3depict.findChild(element('Resolution Selection','frame'))
	ressel.button("OK").click()

	if not os.path.isfile(strDogSave) :
		return False

	return True

def writeRandomPos(filename,numpoints):
	f=open(filename,'wb')

	#Inefficiently generate a whole bunch of random
	# floats in -100:100 
	randomData=[]
	for i in range(numpoints*4):
		randomData.append(random.uniform(-100,100))

	s=struct.pack('>'+'f'*len(randomData),*randomData)
	f.write(s)

	

def runDogtailTests():
	environ['LANG']='en'
	# Start  program
	run('3Depict')
	app3depict = tree.root.application('3Depict')

	#Tap escape twice, once for startuptips,
	# once to remove any autosave dialog questions
	dogtail.rawinput.pressKey("esc")	
	dogtail.rawinput.pressKey("esc")	
	#Run various tests
	#--------
	if not panelTests(app3depict):
		return False;
	prefTests(app3depict)
	if not viewTests(app3depict):
		return False
	if not imageTest(app3depict):
		return False

	#This test does not return 3depict to
	# normal state, leaves file open
	if not openTest(app3depict):
		return False
	
	if not filterTests(app3depict):
		return False
	##--------

	closeProgram(app3depict)

	return True

if __name__=='__main__':
	import sys

	global overrideImage
	overrideImage=True

	#Check for existing 3depict instances
	data=subprocess.Popen(['pidof', '3Depict'],stdout=subprocess.PIPE).communicate()[0]
	pids=data.split(' ');
	if not (len(pids) == 1 and len(pids[0]) == 0)  :
		print("Cannot start - multiple program instances cannot be run with dogtail")
		exit(1)

	#Setup some rate parameters
	dogtail.config.defaultDelay=0.15;
	dogtail.config.actionDelay=0.15;
	dogtail.config.typingDelay=0.15;
	dogtail.config.searchCutoffCount=5


	#Just complain if anything goes wrong
	try:
		testsOK =runDogtailTests()
	except :
		traceback.print_exc(limit=2)
		print('Test failed with exception.')
		exit(1);

	if not testsOK:
		print('Test failed.')
		exit(1)

	exit(0)
