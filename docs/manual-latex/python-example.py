#!/usr/bin/python

import sys
import os 


#Append the contents of one file to another
def appendFile(sourceFile,targetFile):
	try : 
		fileSrc = open(sourceFile,"rb")
		fileTarget = open(targetFile,"ab")
	
		#Extremely inefficient!!
		byte = fileSrc.read(1)
		while byte != "" :
			fileTarget.write(byte)
			byte=fileSrc.read(1)

	except IOError:
		return 1

	return 0

def main():
	argv = sys.argv
	#Name of file that we will dump our results to
	OUTPUT_POSFILE="output.pos"

	#Remove any old files from previous runs
	if os.path.isfile(OUTPUT_POSFILE) :
		os.remove(OUTPUT_POSFILE)


	# do nothing if we have no arguments
	if(len(argv) < 2) :
		return 0;

	#Loop over all our inputs, then for .pos files, 
	# create one big file with all data merged 
	for i in argv[1:] : 
		print "given file :" + i

		fileExt = i[-3:];
		if fileExt  == "pos" :
			if appendFile(i,OUTPUT_POSFILE):
				return 1; #Output to file failed, for some reason
			else : 
				print "appended file to " + OUTPUT_POSFILE

		else :
			#Filename did not end in .pos, lets ignore it.
			print "File :" + i + " does not appear to be a pos file";


        return 0

if __name__ == "__main__":
    sys.exit(main())



