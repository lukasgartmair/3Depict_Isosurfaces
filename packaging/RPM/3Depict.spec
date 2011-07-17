Name:		3Depict
Version:	0.0.6
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
BuildRequires: wxGTK-devel
#PDF latex build
BuildRequires: tex(latex)

#Required for surface removal algorithms 
BuildRequires: qhull-devel

#Fedora specific PDF dir.
Patch0: 3Depict-0.0.6-manual-pdf-loc.patch
#Fedora specific font dir
Patch1: 3Depict-0.0.6-font-path.patch

%description
This program is designed to help users visualize and analyze 3D point clouds
with an associated real value, in a fast and flexible fashion. It is 
specifically targeted to atom probe tomography applications, but may be 
useful for general scalar valued point data purposes.

%prep

%setup -q 

%patch0
%patch1

%build
#--enable-openmp-parallel does not work -- there is a bug in the
# tr1 headers with -D_GLIBCXX_PARALLEL . Lets drop that and only use
# -fopenmp
export CFLAGS="$RPM_OPT_FLAGS -fopenmp"
export CXXFLAGS="$RPM_OPT_FLAGS -fopenmp"
%configure --disable-debug-checks
make %{?_smp_mflags}

pushd docs/manual-latex
pdflatex manual.tex
bibtex manual
pdflatex manual.tex
popd


%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

# Install the textures
mkdir -p %{buildroot}%{_datadir}/%{name}/textures
cp -p src/textures/* %{buildroot}%{_datadir}/%{name}/textures


#Install the manpage
install -Dp -m 644 packaging/manpage/%{name}.1 %{buildroot}%{_mandir}/man1/%{name}.1

desktop-file-install \
		--dir %{buildroot}%{_datadir}/applications \
		packaging/%{name}.desktop
mkdir -p %{buildroot}%{_datadir}/pixmaps/
install -Dp -m 644 src/tex-source/3Depict-icon.svg %{buildroot}%{_datadir}/pixmaps/3Depict.svg

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
* Fri May 20 2011 D Haley <mycae(a!t)yahoo.com> - 0.0.6-1
- Update to 0.0.6

* Mon Mar 27 2011 D Haley <mycae(a!t)yahoo.com> - 0.0.5-1
- New upstream release

* Sat Mar 13 2011 D Haley <mycae(a!t)yahoo.com> - 0.0.4-3
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

* Sat Aug 08 2010 D Haley <mycae(a!t)yahoo.com> - 0.0.1-1
- Initial package
