#!/bin/bash

#!nasty hack script to fix some spelling errors in comments
#Usage:
#---
# grep '^\s*//' *{cpp,h} > comments #Rudely extract (some) comments
# cat comments | aspell list  > badwords #get a list of words with spelling errors
#Manually slice down list of bad words
# grep  -f badwords comments > errs
#Manually slice down errs file, then correct spelling errs in that file
# - then run this script
#---
SAVEIFS=$IFS

IFS=$(echo -en "\n\b")
for i in `cat errs`
do
	tre-agrep -4 --line-number $i *.{cpp,h} > tmpRes;

	if [ `wc -l tmpRes | awk '{print $1}'` -eq 0 ] ; then
		echo "no match for :" $i
		continue
	fi

	#Extract only the first occurrence
	FILENAME=`cat tmpRes | awk  -F: '{print $1}' | head -n 1 `
	LINE=`cat tmpRes | awk -F: '{print $2}' | head -n 1`

	echo $FILENAME 
	echo $LINE
	awk -v line=$LINE -v new_content="$i" '{
		if (NR == line) {
			print new_content;
		} else {
			print $0;
		}
		}' $FILENAME  > tmpV
	mv tmpV "$FILENAME"
done


IFS=$SAVEIFS
