Name:    n2kafka
Summary: Get data events via SNMP or scripting and send results in json over kafka.
Version: %{__version}
Release: %{__release}%{?dist}

License: GNU AGPLv3
URL: https://gitlab.redborder.lan/dfernandez.ext/n2kafka/repository/archive.tar.gz?ref=redborder
Source0: %{name}-%{version}.tar.gz

BuildRequires: gcc librd-devel json-c-devel librdkafka-devel libev-devel libmicrohttpd-devel jansson-devel >= 2.7 yajl-devel >= 2.1.0 libcurl-devel >= 7.48

Summary: Non-blocking high-level wrapper for libcurl
Group:   Development/Libraries/C and C++
Requires: librd0 json-c librdkafka1 libev libmicrohttpd jansson >= 2.7 yajl >= 2.1.0 libcurl >= 7.48
%description
%{summary}

%prep
%setup -qn %{name}-%{version}

%build
./configure --prefix=/usr
make

%install
DESTDIR=%{buildroot} make install

%clean
rm -rf %{buildroot}

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%defattr(755,root,root)
/usr/bin/n2kafka

%changelog
* Wed May 11 2016 Juan J. Prieto <jjprieto@redborder.com> - 1.0-1
- first spec version


