Name:		3Depict
Version:	0.0.3
Release:	1%{?dist}
Summary:	Valued 3D point cloud visualization and analysis
Group:		Applications/Engineering


License:	GPLv3+
URL:		http://threedepict.sourceforge.net
Source0:	http://downloads.sourceforge.net/threedepict/%{name}-%{version}.tar.gz

BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

#Fedora specific texture path install location
Patch0:	%{name}-%{version}-texture-path.patch

#Mathgl for plotting
BuildRequires:	mathgl-devel 
#Mesa for GLU
BuildRequires:	libGL-devel 
#Libxml2 for file parsing
BuildRequires:	libxml2-devel 
#FTGL for 3d fonts
BuildRequires:	ftgl-devel 
#libpng for textures
BuildRequires: libpng-devel
#WX widgets
BuildRequires: wxGTK-devel
#Required for surface removal algorithms 
BuildRequires: libqhull-devel

#Desktop file utils for installing desktop file
BuildRequires: desktop-file-utils

%description
This program is designed to help users visualize and analyze 3D point clouds
with an associated real value, in a fast and flexible fashion. It is 
specifically targeted to atom probe tomography applications, but may be 
useful for general scalar valued point data purposes.

%prep
%setup -q 
%patch0

%build
%configure 
make %{?_smp_mflags}


%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

# Install the textures
mkdir -p %{buildroot}%{_datadir}/%{name}/textures
cp -p src/textures/* %{buildroot}%{_datadir}/%{name}/textures


#Installl the manpage
install -Dp -m 644 packaging/manpage/%{name}.1 %{buildroot}%{_mandir}/man1/%{name}.1

desktop-file-install \
		--dir %{buildroot}%{_datadir}/applications \
		packaging/%{name}.desktop
mkdir -p %{buildroot}%{_datadir}/pixmaps/
install -Dp -m 644 src/tex-source/3Depict-icon.svg %{buildroot}%{_datadir}/pixmaps/
%clean
rm -rf %{buildroot}


%files
%defattr(-,root,root,-)
%doc COPYING AUTHORS ChangeLog README TODO
%{_bindir}/%{name}
%dir %{_datadir}/%{name}/
%dir %{_datadir}/%{name}/textures
%{_datadir}/%{name}/textures/*.png
%{_datadir}/applications/%{name}.desktop
%{_mandir}/man1/%{name}.1.*
%{_datadir}/pixmaps/*


%changelog
* Nov 21 2008 D Haley <mycae(a!t)yahoo.com> - 0.0.3-1
- Update to 0.0.3

* Tue Sep 21 2010 D Haley <mycae(a!t)yahoo.com> - 0.0.2-1
- Update to 0.0.2
- Address comments in package review 

* Sat Aug 08 2010 D Haley <mycae(a!t)yahoo.com> - 0.0.1-1
- Initial package
