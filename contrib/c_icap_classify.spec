Name:           c_icap_classify
Version:        20180416
Release:        1%{?dist}
Summary:        Classification module for c-icap.

Group:          Servers
License:        LGPLv3+
URL:            http://c-icap.sourceforge.net/
Source0:        c_icap_classify-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:  c_icap-devel
#BuildRequires:	opencv-devel
BuildRequires:  tre-devel
BuildRequires:  libicu-devel
Requires:	c_icap_modules
#Requires:	opencv
Requires:	tre
Requires:       c_icap
Requires:       libicu

%description
This module can be setup to classify text & HTML pages as well as images. These
classifications are according to FastHyperSpace files or Intel OpenCV files
supplied by the user to recognize various image characteristics.

%package -n	%{name}-training
Summary:	Programs to train FHS (Fast HyperSpace) or FNB (Fast Naive Bayes) files
Group:		Admin Tools
Provides:       %{name}-training fhs_learn fhs_judge fhs_makepreload fnb_learn fnb_judge fnb_makepreload
Requires:	c_icap_classify

%description -n %{name}-training
Programs to train FHS (Fast HyperSpace) or FNB (Fast Naive Bayes) files for use by c_icap_classify.

%prep
%setup -q
chmod 700 RECONF
./RECONF


%build
%configure
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
# Make some directories
install -d %{buildroot}%{_sysconfdir}

# Now do install
make install DESTDIR=$RPM_BUILD_ROOT

# nuke unwanted devel files
rm -f %{buildroot}%{_libdir}/c_icap/srv_*.a
rm -f %{buildroot}%{_libdir}/c_icap/srv_*.la
rm -f %{buildroot}%{_libdir}/c_icap/sys_logger.la

%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
%{_libdir}/c_icap/srv_classify.so


%files training
%{_bindir}/fhs_judge
%{_bindir}/fhs_learn
%{_bindir}/fhs_makepreload
%{_bindir}/fhs_findtolearn
%{_bindir}/fnb_judge
%{_bindir}/fnb_learn
%{_bindir}/fnb_makepreload
%{_bindir}/fnb_findtolearn
%attr(0644,root,root) %{_mandir}/man8/fhs*
%attr(0644,root,root) %{_mandir}/man8/fnb*

%changelog
