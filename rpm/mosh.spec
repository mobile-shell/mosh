Name:		mosh
Version:	1.3.2
Release:	1%{?dist}
Summary:	Mobile shell that supports roaming and intelligent local echo

License:	GPLv3+
Group:		Applications/Internet
URL:		https://mosh.org/
%undefine       _disable_source_fetch
Source0:	https://github.com/mobile-shell/mosh/archive/refs/tags/%{name}-%{version}.tar.gz

BuildRequires:	protobuf-compiler
BuildRequires:	protobuf-devel
BuildRequires:	libutempter-devel
BuildRequires:	zlib-devel
BuildRequires:	ncurses-devel
BuildRequires:	openssl11-devel
Requires:	openssh-clients
Requires:	openssl11
Requires:	perl-IO-Socket-IP

%description
Mosh is a remote terminal application that supports:
  - intermittent network connectivity,
  - roaming to different IP address without dropping the connection, and
  - intelligent local echo and line editing to reduce the effects
    of "network lag" on high-latency connections.


%prep
# Validate we're using the tarball we expect.
SHA256="e7f69404816f1ca5b44762d010e406a56d18e133d31efd0783970de8d6c80ed4"
test $(sha256sum ../SOURCES/%{name}-%{version}.tar.gz |cut -d " " -f 1) == "$SHA256"
# Name is duplicated in release tag for 1.3.2
%setup -q -n %{name}-%{name}-%{version}


%build
# Use upstream's more aggressive hardening instead of Fedora's defaults
export CFLAGS="-g -O2" CXXFLAGS="-g -O2"
./autogen.sh
%configure --enable-compile-warnings=error
make %{?_smp_mflags}


%install
make install DESTDIR=$RPM_BUILD_ROOT


%files
%doc README.md COPYING ChangeLog
%{_bindir}/mosh
%{_bindir}/mosh-client
%{_bindir}/mosh-server
%{_mandir}/man1/mosh.1.gz
%{_mandir}/man1/mosh-client.1.gz
%{_mandir}/man1/mosh-server.1.gz


%changelog
* Tue Oct 26 2021 Doug Chapman <dougch@amazon.com> - 1.3.2-1
- Update to mosh 1.3.2
- Use OpenSSL 1.1

* Sun Jul 12 2015 John Hood <cgull@glup.org> - 1.2.5-1
- Update to mosh 1.2.5

* Fri Jun 26 2015 John Hood <cgull@glup.org> - 1.2.4.95rc2-1
- Update to mosh 1.2.4.95rc2

* Mon Jun 08 2015 John Hood <cgull@glup.org> - 1.2.4.95rc1-1
- Update to mosh 1.2.4.95rc1

* Wed Mar 27 2013 Alexander Chernyakhovsky <achernya@mit.edu> - 1.2.4-1
- Update to mosh 1.2.4

* Sun Mar 10 2013 Alexander Chernyakhovsky <achernya@mit.edu> - 1.2.3-3
- Rebuilt for Protobuf API change from 2.4.1 to 2.5.0

* Thu Feb 14 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.2.3-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_19_Mass_Rebuild

* Fri Oct 19 2012 Alexander Chernyakhovsky <achernya@mit.edu> - 1.2.3-1
- Update to mosh 1.2.3

* Fri Jul 20 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.2.2-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_18_Mass_Rebuild

* Wed Jun 13 2012 Alexander Chernyakhovsky <achernya@mit.edu> - 1.2.2-1
- Update to mosh 1.2.2

* Sat Apr 28 2012 Alexander Chernyakhovsky <achernya@mit.edu> - 1.2-2
- Add -g and -O2 CFLAGS

* Fri Apr 27 2012 Alexander Chernyakhovsky <achernya@mit.edu> - 1.2-1
- Update to mosh 1.2.

* Mon Mar 26 2012 Alexander Chernyakhovsky <achernya@mit.edu> - 1.1.1-1
- Update to mosh 1.1.1.

* Wed Mar 21 2012 Alexander Chernyakhovsky <achernya@mit.edu> - 1.1-1
- Initial packaging for mosh.
