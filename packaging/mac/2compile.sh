#!/bin/bash
cp makeMacOSXApp ../..
cp -R 3Depict.app ../..
cd ../..
time ./makeMacOSXApp --update-config=no > out.txt 2>&1
if [ $? -eq 0 ] ; then
	echo "Finished compiling"
else
	echo "Failed"
	vi out.txt
	exit 1
fi
time ./3Depict.app/Contents/MacOS/3Depict -t > out2.txt 2>&1
if [ $? -eq 0 ] ; then
        echo "Finished testing"
else
        echo "Failed"
        vi out2.txt
	exit 1
fi

