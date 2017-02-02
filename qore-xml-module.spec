%global mod_ver 1.3.2

%{?_datarootdir: %global mydatarootdir %_datarootdir}
%{!?_datarootdir: %global mydatarootdir /usr/share}

%global module_api %(qore --latest-module-api 2>/dev/null)
%global module_dir %{_libdir}/qore-modules
%global user_module_dir %{mydatarootdir}/qore-modules/

%if 0%{?sles_version}

%global dist .sles%{?sles_version}

%else
%if 0%{?suse_version}

# get *suse release major version
%global os_maj %(echo %suse_version|rev|cut -b3-|rev)
# get *suse release minor version without trailing zeros
%global os_min %(echo %suse_version|rev|cut -b-2|rev|sed s/0*$//)

%if %suse_version > 1010
%global dist .opensuse%{os_maj}_%{os_min}
%else
%global dist .suse%{os_maj}_%{os_min}
%endif

%endif
%endif

# see if we can determine the distribution type
%if 0%{!?dist:1}
%global rh_dist %(if [ -f /etc/redhat-release ];then cat /etc/redhat-release|sed "s/[^0-9.]*//"|cut -f1 -d.;fi)
%if 0%{?rh_dist}
%global dist .rhel%{rh_dist}
%else
%global dist .unknown
%endif
%endif

Summary: XML module for Qore
Name: qore-xml-module
Version: %{mod_ver}
Release: 1%{dist}
License: LGPL
Group: Development/Languages/Other
URL: http://qore.org
Source: http://prdownloads.sourceforge.net/qore/%{name}-%{version}.tar.bz2
#Source0: %{name}-%{version}.tar.bz2
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
Requires: /usr/bin/env
Requires: qore-module-api-%{module_api}
BuildRequires: gcc-c++
BuildRequires: qore-devel >= 0.8.12
BuildRequires: libxml2-devel
BuildRequires: openssl-devel
BuildRequires: qore

%description
This package contains the xml module for the Qore Programming Language.

XML is a markup language for encoding information.

%if 0%{?suse_version}
%debug_package
%endif

%package doc
Summary: Documentation and examples for the Qore xml module
Group: Development/Languages

%description doc
This package contains the HTML documentation and example programs for the Qore
xml module.

%files doc
%defattr(-,root,root,-)
%doc docs/xml docs/XmlRpcHandler test examples

%prep
%setup -q
./configure RPM_OPT_FLAGS="$RPM_OPT_FLAGS" --prefix=/usr --disable-debug

%build
%{__make}
find test -type f|xargs chmod 644
find docs -type f|xargs chmod 644

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%{module_dir}
mkdir -p $RPM_BUILD_ROOT/%{user_module_dir}
mkdir -p $RPM_BUILD_ROOT/usr/share/doc/qore-xml-module
make install DESTDIR=$RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{module_dir}
%{user_module_dir}
%doc COPYING.LGPL COPYING.MIT README RELEASE-NOTES AUTHORS

%changelog
* Thu Feb 2 2017 David Nichols <david@qore.org> - 1.3.2
- updated to version 1.3.2

* Mon Sep 5 2016 David Nichols <david@qore.org> - 1.3.1
- updated to version 1.3.1

* Sat Jan 4 2014 David Nichols <david@qore.org> - 1.3
- updated to version 1.3

* Mon Nov 12 2012 David Nichols <david@qore.org> - 1.2
- updated to version 1.2

* Thu May 31 2012 David Nichols <david@qore.org> - 1.1
- updated to qpp

* Thu Oct 20 2011 Petr Vanek <petr.vanek@qoretechnologies.com> - 1.1
- 1.1 release

* Tue Dec 28 2010 David Nichols <david@qore.org> - 1.1
- updated to version 1.1

* Fri Dec 17 2010 David Nichols <david@qore.org> - 1.0
- initial spec file for xml module
