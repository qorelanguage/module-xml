%global mod_ver 2.0.0

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

%if %suse_version
%global dist .opensuse%{os_maj}_%{os_min}
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
License: MIT
Group: Development/Languages/Other
URL: http://qore.org
Source: http://prdownloads.sourceforge.net/qore/%{name}-%{version}.tar.bz2
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
Requires: /usr/bin/env
Requires: qore-module(abi)%{?_isa} = %{module_api}
%if 0%{?el7}
BuildRequires: devtoolset-7-gcc-c++
%endif
BuildRequires: cmake >= 3.5
BuildRequires: gcc-c++
BuildRequires: qore-devel >= 2.0
BuildRequires: qore-stdlib >= 2.0
BuildRequires: qore >= 2.0
BuildRequires: libxml2-devel
BuildRequires: openssl-devel
BuildRequires: fdupes
BuildRequires: doxygen
%if 0%{?suse_version} || 0%{?sles_version}
BuildRequires: timezone
%else
BuildRequires: tzdata
%endif

%description
This package contains the xml module for the Qore Programming Language.

XML is a markup language for encoding information.

%if 0%{?suse_version}
%debug_package
%endif

%package doc
Summary: Documentation and examples for the Qore xml module
Group: Development/Languages/Other

%description doc
This package contains the HTML documentation and example programs for the Qore
xml module.

%files doc
%defattr(-,root,root,-)
%doc docs/xml docs/XmlRpcHandler docs/SalesforceSoapClient docs/SaxDataProvider docs/SoapClient docs/SoapDataProvider docs/SoapHandler docs/WSDL docs/XmlRpcConnection test examples

%prep
%setup -q

%build
%if 0%{?el7}
# enable devtoolset7
. /opt/rh/devtoolset-7/enable
%endif
export CXXFLAGS="%{?optflags}"
cmake -DCMAKE_INSTALL_PREFIX=%{_prefix} -DCMAKE_BUILD_TYPE=RELWITHDEBINFO -DCMAKE_SKIP_RPATH=1 -DCMAKE_SKIP_INSTALL_RPATH=1 -DCMAKE_SKIP_BUILD_RPATH=1 -DCMAKE_PREFIX_PATH=${_prefix}/lib64/cmake/Qore .
make %{?_smp_mflags}
%{__make}
%{__make} docs
sed -i 's/#!\/usr\/bin\/env qore/#!\/usr\/bin\/qore/' test/*.qtest

%install
make DESTDIR=%{buildroot} install %{?_smp_mflags}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{module_dir}
%{user_module_dir}
%{_bindir}/soaputil
%{_bindir}/webdav-server
%doc COPYING.LGPL COPYING.MIT README RELEASE-NOTES AUTHORS

%check
export QORE_MODULE_DIR=$QORE_MODULE_DIR:qlib
qore -l ./xml-api-%{module_api}.qmod test/InputStreamSaxIterator.qtest -v
qore -l ./xml-api-%{module_api}.qmod test/Salesforce.com.qtest -v
qore -l ./xml-api-%{module_api}.qmod test/SoapClient.qtest -v
qore -l ./xml-api-%{module_api}.qmod test/SoapHandler.qtest -v
qore -l ./xml-api-%{module_api}.qmod test/XmlRpcClient.qtest -v
qore -l ./xml-api-%{module_api}.qmod test/XmlRpcHandler.qtest -v
qore -l ./xml-api-%{module_api}.qmod test/soap.qtest -v
qore -l ./xml-api-%{module_api}.qmod test/webdav_DummyWebDavHandler.qtest -v
qore -l ./xml-api-%{module_api}.qmod test/webdav_FsWebDavHandler.qtest -v
#qore -l ./xml-api-%{module_api}.qmod test/webdav_FsWebDavHandler_litmus.qtest -v
qore -l ./xml-api-%{module_api}.qmod test/xml.qtest -v

%changelog
* Sun Oct 1 2023 David Nichols <david@qore.org> - 2.0.0
- added support for the DataProvider app/action catalog

* Thu Jun 15 2023 David Nichols <david@qore.org> - 1.8.1
- allow wsdl to be chosen as a file in the SoapClient connection

* Mon Dec 19 2022 David Nichols <david@qore.org> - 1.8.0
- use cmake for build

* Sun Nov 6 2022 David Nichols <david@qore.org> - 1.8.0
- updated to version 1.8.0

* Fri Oct 14 2022 David Nichols <david@qore.org> - 1.7.1
- updated to version 1.7.1

* Wed Sep 14 2022 David Nichols <david@qore.org> - 1.7.0
- updated to version 1.7.0

* Thu Jul 7 2022 David Nichols <david@qore.org> - 1.6.1
- updated to version 1.6.1

* Fri Apr 1 2022 David Nichols <david@qore.org> - 1.6.0
- updated to version 1.6.0

* Fri Feb 18 2022 David Nichols <david@qore.org> - 1.5.3
- updated to version 1.5.3

* Sat Feb 12 2022 David Nichols <david@qore.org> - 1.5.2
- updated to version 1.5.2

* Tue Jan 25 2022 David Nichols <david@qore.org> - 1.5.1
- updated to version 1.5.1

* Tue Jun 19 2018 David Nichols <david@qore.org> - 1.5
- updated to version 1.5

* Tue May 15 2018 David Nichols <david@qore.org> - 1.4.2
- updated to version 1.4.2

* Fri Apr 13 2018 David Nichols <david@qore.org> - 1.4.1
- updated to version 1.4.1

* Thu Feb 2 2017 David Nichols <david@qore.org> - 1.4
- updated to version 1.4

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
