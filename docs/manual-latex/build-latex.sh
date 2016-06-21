#!/bin/bash
LATEXBIN=pdflatex
BIBTEXBIN=bibtex
if [ x`which $LATEXBIN` == x"" ] ; then
	echo "no $LATEXBIN" 
	exit 1
fi

if [ x`which $BIBTEXBIN` == x"" ] ; then
	echo "no $BIBTEXBIN" 
	exit 1
fi

MANUAL=manual.tex

#check to see if the manual and its bib file are around
if [ ! -f $MANUAL  ] ; then
	echo "$MANUAL is missing"
	exit 1
fi

#Remove the old manul
rm -f manual.pdf



#run the multi-pass latex build. 
$LATEXBIN $MANUAL || { echo "failed latex build (pass 1 of 2)"; exit 1 ; }
$BIBTEXBIN `basename -s .tex $MANUAL` || { echo "failed bibtex build"; exit 1 ; }
$LATEXBIN $MANUAL || { echo "failed latex build (pass 2 of 2)"; exit 1 ; }


if [ ! -f manual.pdf ] ; then
	echo "latex ran, but somehow we did not output the PDF...."
	exit 1
fi

