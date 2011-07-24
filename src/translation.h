#ifndef TRANSLATION_H
#define TRANSLATION_H

#include <locale>


#if defined(__APPLE__) || defined(__WIN32__) || defined(__WIN64__)
#include <libintl.h>
#endif

//!Gettext translation macro
#define TRANS(x) (gettext(x))

//!Gettext null-translation macro (mark for translation, but do nothing)
#define NTRANS(x) (x)

//!Wx friendly gettext translation macro
#define wxTRANS(x) (wxString(gettext(x),*wxConvCurrent))

#define wxNTRANS(x) wxT(x)

#endif
