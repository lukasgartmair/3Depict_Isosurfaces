#ifndef WXCOMMON_H
#define WXCOMMON_H
#include <wx/wx.h>
#include <wx/treectrl.h>

#define wxCStr(a) wxString(a,*wxConvCurrent)
#define wxStr(a) wxString(a.c_str(),*wxConvCurrent)

#define stlStr(a) (const char *)a.mb_str()

#include <string>
std::string locateDataFile(const char *name);
#endif
