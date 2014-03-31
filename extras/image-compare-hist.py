#!/usr/bin/python

#GPLv3+ permission obtained via email
#https://stackoverflow.com/questions/1927660/compare-two-images-the-python-linux-way
import math, operator
from PIL import Image
def compare(fileSVG, filePNG):
	image1 = Image.open(fileSVG)
	image2 = Image.open(filePNG)
	h1 = image1.histogram()
	h2 = image2.histogram()
	rms = math.sqrt(reduce(operator.add,
	map(lambda a,b: (a-b)**2, h1, h2))/len(h1))
	return rms

if __name__=='__main__':
	import sys
	file1, file2 = sys.argv[1:]
	f=open('img-compare-result-arkd.txt','w');
	s=compare(file1, file2)
	f.write(str(compare))
