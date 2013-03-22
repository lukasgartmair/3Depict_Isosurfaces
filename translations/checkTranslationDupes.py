#!/usr/bin/python

import sys
import re

filename="3Depict_de_DE.po"
f = open(filename)

if not f:
	sys.exit("Unable to open file. Exiting")

curLine=0

while True: 
	curLine=curLine+1;
	s=f.readline();
	if(not s):
		break;

	if re.match("msgid.*",s):
		msgid=s[6:];
	elif re.match("msgstr.*",s):
		msgstr=s[7:];

		if(msgid == msgstr and re.match(".*[A-z].*",msgid)):
				print "Duplicate id at "  + str(curLine)

