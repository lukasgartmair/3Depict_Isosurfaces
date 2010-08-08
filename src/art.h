#ifndef ART_H
#define ART_H

#include <wx/artprov.h>

// include *your* xpm icons here:
#include "3Depict.xpm"

class MyArtProvider : public wxArtProvider
{
	public:
		MyArtProvider() {}
		virtual ~MyArtProvider() {}

	protected:
		wxBitmap CreateBitmap(const wxArtID& id,
							  const wxArtClient& client,
							  const wxSize& size)
		{
			if (id == _T("MY_ART_ID_ICON"))
				return wxBitmap(_Depict);
		}
};

#endif // ART_H
