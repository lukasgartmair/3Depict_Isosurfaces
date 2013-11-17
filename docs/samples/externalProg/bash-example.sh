#!/bin/bash

BYTES_PER_RECORD=16
echo "Num args : "$#
echo "Working Directory:"  `pwd`

for (( i=1; i<=$#; i++ )); 
do
	eval arg=\$$i
	echo "Input file: $arg"
	echo "File size:" `filesize $arg` " Bytes"
	NUM_IONS=$(expr $(filesize $arg) / $BYTES_PER_RECORD) 
	echo "Num Ions:" $NUM_IONS 

	#Copy the output into the working directory, so that 3Depict's scanning of the working directory
	# for .pos files will find it
	cp $arg script-output-3Depict-input.pos
done 

exit 0
