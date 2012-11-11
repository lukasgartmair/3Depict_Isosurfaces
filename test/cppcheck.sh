#!/bin/bash
#How to use:
#	* Download latest cppcheck git source
#	* Build and install cppcheck
#	* Run this script - you may need to customise it to your system


if [ x`which cppcheck` == x"" ] ; then
	echo "No cppcheck found. Please install into path."
	exit 1
fi

#Default to 2 processes, otherwise
# check system for number of processors
# and thus use that as the number of processes
NUM_PROCS=2
if [ `uname` == Linux ] ; then
	NUM_PROCS=`cat /proc/cpuinfo | grep processor |  wc -l`
fi

HG_ROOT=`hg root`

cppcheck -q -j $NUM_PROCS --enable=all --inconclusive ${HG_ROOT}/src/. 2> ${HG_ROOT}/cpp-res
cat ${HG_ROOT}/cpp-res | sort -n > tmp;

#Strip output that is overly noisy/unhelpful to us
echo 'does not have a constructor' > tmp-ignore
echo '(information)' >> tmp-ignore
echo 'is not initialized in the constructor' >> tmp-ignore
echo 'C-style pointer casting' >> tmp-ignore
grep -v -f tmp-ignore tmp  > ${HG_ROOT}/cpp-res


#Use the per-error message ignore file to filter the last of it
if [ -f ${HG_ROOT}/cpp-ignore ] ; then
	grep -v -f cpp-ignore ${HG_ROOT} cpp-res >tmp;
	mv tmp ${HG_ROOT} > cpp-res
	echo "Wrote cpp-res and cpp-res-delta (result - ignore) to ${HG_ROOT}."
else
	echo "No ignore file (cpp-ignore) found - please check entire cppcheck output"
fi
