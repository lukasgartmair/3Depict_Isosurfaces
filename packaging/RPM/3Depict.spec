Name:		3Depict
Version:	0.0.16
Release:	1%{?dist}
Summary:	Valued 3D point cloud visualization and analysis
Group:		Applications/Engineering


License:	GPLv3+
URL:		http://threedepict.sourceforge.net
Source0:	http://downloads.sourceforge.net/threedepict/%{name}-%{version}.tar.gz


BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

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
#Desktop file utils for installing desktop file
BuildRequires: desktop-file-utils
#WX widgets
BuildRequires: wxGTK3-devel
#PDF latex build
#BuildRequires: tex(latex)

#Required for surface removal algorithms 
BuildRequires: qhull-devel

#Fedora specific PDF dir.
Patch0: %{name}-%{version}-manual-pdf-loc.patch
#Fedora specific font dir
Patch1: %{name}-%{version}-font-path.patch
#Upstream patches from 0.0.15 release tarball
Patch2: %{name}-0.0.15-upstream.patch

%description
This software is designed to help users visualize and analyze 3D point clouds
with an associated real value, in a fast and flexible fashion. It is 
specifically targeted to atom probe tomography applications, but may be 
useful for general scalar valued point data purposes.

%prep

%setup -q 

%patch0
%patch1
%patch2

%build
%configure --disable-debug-checks --enable-openmp-parallel --enable-mgl2
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

# Install the textures
mkdir -p %{buildroot}%{_datadir}/%{name}/textures
cp -p data/textures/*png %{buildroot}%{_datadir}/%{name}/textures/


#Install the manpage
install -Dp -m 644 packaging/manpage/%{name}.1 %{buildroot}%{_mandir}/man1/%{name}.1

desktop-file-install \
		--dir %{buildroot}%{_datadir}/applications \
		packaging/%{name}.desktop
mkdir -p %{buildroot}%{_datadir}/pixmaps/
install -Dp -m 644 data/textures/tex-source/%{name}-icon.svg %{buildroot}%{_datadir}/pixmaps/%{name}.svg

#install language files
#--
#Remap locale names
mv locales/de_DE/ locales/de/

mkdir -p %{buildroot}/%{_datadir}/locale/
cp -R locales/* %{buildroot}/%{_datadir}/locale/

#Restore the internal build's locale naming
mv locales/de/ locales/de_DE/
#--


#Move the documentation such that it is picked up by the doc macro
mv docs/manual-latex/manual.pdf %{name}-%{version}-manual.pdf

#Locale stuff
%find_lang %{name}


%clean
rm -rf %{buildroot}


%files -f %{name}.lang
%defattr(-,root,root,-)
%doc COPYING AUTHORS ChangeLog README TODO %{name}-%{version}-manual.pdf
%{_bindir}/%{name}
%dir %{_datadir}/%{name}/
%dir %{_datadir}/%{name}/textures
%{_datadir}/%{name}/textures/*.png
%{_datadir}/applications/%{name}.desktop
%{_mandir}/man1/%{name}.1.*
%{_datadir}/pixmaps/*.svg


%changelog
* Sun Apr 06 2014 D Haley <mycae(a!t)gmx.com> - 0.0.16-1
- Update to 0.0.16

* Wed Feb 12 2014 D Haley <mycae(a!t)gmx.com> - 0.0.15-4
- Rebuild for mgl

* Wed Feb 05 2014 D Haley <mycae(a!t)gmx.com> - 0.0.15-3
- Rebuild for new mgl
- Add upstream patches 

* Sun Jan 26 2014 D Haley <mycae(a!t)gmx.com> - 0.0.15-2
- Rebuild for new mgl

* Sun Dec 01 2013 D Haley <mycae(a!t)gmx.com> - 0.0.15-1
- Update to 0.0.15

* Fri Aug 02 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.0.14-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_20_Mass_Rebuild

* Sat Jul 20 2013 D Haley <mycae(a!t)gmx.com> - 0.0.14-1
- Update to 0.0.14

* Tue Jun 25 2013 D Haley <mycae(a!t)gmx.com> - 0.0.13-2
- Enable mathgl2

* Fri Apr 12 2013 D Haley <mycae(a!t)gmx.com> - 0.0.13-1
- Update to 0.0.13

* Sat Mar 23 2013 D Haley <mycae(a!t)gmx.com> - 0.0.12-4
- Add aarch 64 patch for bug 924960, until next version

* Wed Feb 13 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.0.12-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_19_Mass_Rebuild

* Sun Dec 9 2012 D Haley <mycae(a!t)yahoo.com> - 0.0.12-2
- Import bugfixes from upstream for plot UI and crash fixes

* Sun Nov 25 2012 D Haley <mycae(a!t)yahoo.com> - 0.0.12-1
- Update to 0.0.12

* Mon Apr 2 2012 D Haley <mycae(a!t)yahoo.com> - 0.0.10-1
- Update to 0.0.10

* Tue Feb 28 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.0.9-4
- Rebuilt for c++ ABI breakage

* Thu Jan 12 2012 D Haley <mycae(a!t)yahoo.com> - 0.0.9-3
- Patch to fix FTFBS for gcc 4.7

* Thu Jan 12 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.0.9-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_17_Mass_Rebuild

* Sat Dec 17 2011 D Haley <mycae(a!t)yahoo.com> - 0.0.9-1
- Update to 0.0.9

* Tue Dec 06 2011 Adam Jackson <ajax@redhat.com> - 0.0.8-3
- Rebuild for new libpng

* Sat Oct 29 2011 D Haley <mycae(a!t)yahoo.com> - 0.0.8-2
- Post release fixes for various crash bugs

* Sun Oct 23 2011 D Haley <mycae(a!t)yahoo.com> - 0.0.8-1
- Update to 0.0.8

* Sun Aug 14 2011 D Haley <mycae(a!t)yahoo.com> - 0.0.7-1
- Update to 0.0.7

* Fri May 20 2011 D Haley <mycae(a!t)yahoo.com> - 0.0.6-1
- Update to 0.0.6

* Sun Mar 27 2011 D Haley <mycae(a!t)yahoo.com> - 0.0.5-1
- New upstream release

* Sun Mar 13 2011 D Haley <mycae(a!t)yahoo.com> - 0.0.4-3
- Patch opengl startup code -- peek at gl context. Possible fix for bug 684390

* Sat Feb 12 2011 D Haley <mycae(a!t)yahoo.com> - 0.0.4-2
- Fix bug 677016 - 3Depict no built with rpm opt flags

* Sat Jan 22 2011 D Haley <mycae(a!t)yahoo.com> - 0.0.4-1
- Update to 0.0.4

* Fri Nov 26 2010 D Haley <mycae(a!t)yahoo.com> - 0.0.3-1
- Update to 0.0.3

* Tue Oct 5 2010 D Haley <mycae(a!t)yahoo.com> - 0.0.2-3
- Use tex(latex) virtual package in preference to texlive-latex

* Mon Oct 4 2010 D Haley <mycae(a!t)yahoo.com> - 0.0.2-2
- Add latex build for manual

* Sat Sep 25 2010 D Haley <mycae(a!t)yahoo.com> - 0.0.2-1
- Update to 0.0.2
- Address comments in package review 

* Sun Aug 08 2010 D Haley <mycae(a!t)yahoo.com> - 0.0.1-1
- Initial package
