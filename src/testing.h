#ifndef TESTING_H
#define TESTING_H

#ifdef DEBUG
#include "filtertree.h"


//Run all the built-in unit tests.
bool runUnitTests();

//Run the particular specified filter tree
bool testFilterTree(FilterTree f);

#endif

#endif
