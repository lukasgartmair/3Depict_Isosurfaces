Name:		3Depict
Version:	0.0.1
Release:	1%{?dist}
Summary:	Valued 3D point cloud visualization and analysis
Group:		Applications/Engineering


License:	GPLv3+
URL:		http://3Depict.sourceforge.net
Source0:	http://3Depict.sourceforge.net/path-to-file/3Depict-0.0.1.tar.gz
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

Patch0: %{name}-texture-path.patch

#Mathgl for plotting
BuildRequires:	mathgl-devel 
#Mesa for GLU
BuildRequires:	libGL-devel 
#Libxml2 for file parsing
BuildRequires:	libxml2-devel 
#FTGL for 3d fonts
BuildRequires:	ftgl-devel 
#Desktop file utils for installing desktop file
BuildRequires: desktop-file-utils
#WX widgets
BuildRequires: wxGTK-devel
#FIXME: mathgl needs an update; should Require this in -devel
BuildRequires: gsl-devel

%description
This program is designed to help users visualize and analyze 3D point clouds
with an associated real value, in a fast and flexible fashion. It is 
specifically targeted to atom probe tomography applications, but may be 
useful for general scalar valued point data purposes.

%prep
%setup -q 
%patch0

%build
export LDFLAGS="-lGL -lpng"
%configure 
make %{?_smp_mflags}


%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

# Install the textures
mkdir -p %{buildroot}/%{_datadir}/%{name}/textures
cp -p src/textures/* %{buildroot}/%{_datadir}/%{name}/textures



desktop-file-install \
		--dir $RPM_BUILD_ROOT%{_datadir}/applications \
		%{name}.desktop
	

%post -p /sbin/ldconfig


%postun -p /sbin/ldconfig


%clean
rm -rf %{buildroot}


%files
%defattr(-,root,root,-)
%doc COPYING AUTHORS ChangeLog README TODO
/%{_bindir}/%{name}
/%{_datadir}/%{name}/
%{_datadir}/applications/%{name}.desktop


%changelog
* Sat Jul 24 2010 D Haley <mycae(a!t)yahoo.com> - 0.0.1-1
- Initial package
