#!/bin/bash
time ./makeMacOSXApp --update-config=no > out.txt 2>&1
if [ x"`tail -1 out.txt`" == x"Done" ] ; then
	echo "Finished compiling"
else
	echo "Failed"
	vi out.txt
	exit 1
fi
time ./3Depict.app/Contents/MacOS/3Depict -t > out2.txt 2>&1
if [ x"`tail -1 out2.txt`" == x"Unit tests succeeded!" ] ; then
        echo "Finished testing"
else
        echo "Failed"
        vi out2.txt
	exit 1
fi

