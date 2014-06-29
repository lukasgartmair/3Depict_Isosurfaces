/*
MusicXML Library
Copyright (C) Grame 2006-2013

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.

Grame Research Laboratory, 11, cours de Verdun Gensoul 69002 Lyon - France
research@grame.fr
*/

#ifndef __bimap__
#define __bimap__

#include <map>
#include <iterator>
#include <utility>

/*!
\brief implements a bijective map

A bijective map is a map where values and keys are interchangeable.
To operate correctly, typenames T1 and T2 must be different.
*/

template <typename T1, typename T2>
class bimap {
public:
	bimap() {}
	bimap(const T1 tbl1[], const T2 tbl2[], int n);
	virtual ~bimap() {}

//! returns the second type value indexed by the first type
	const T2 operator[] (const T1 key) {
		return fT1Map[key];
	}
	//! returns the first type value indexed by the second type
	const T1 operator[] (const T2 key) {
		return fT2Map[key];
	}
	//! returns the map size
	long size() {
		return fT1Map.size();
	}

	//! adds a pair of values
	bimap& add (const T1& v1, const T2& v2)
	{
		fT1Map[v1]=v2;
		fT2Map[v2]=v1;
		return *this;
	}


private:
	map<T1, T2> fT1Map;
	map<T2, T1> fT2Map;
};

template <typename T1, typename T2>
bimap<T1, T2>::bimap(const T1 tbl1[], const T2 tbl2[], int n)
{
	for (int i=0; i<n; i++) {
		add(tbl1[i], tbl2[i]);
	}
}

#endif
