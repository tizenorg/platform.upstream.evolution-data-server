%define baseline 3.9

%define USE_EVOLDAP 0
%define with_introspection 1
%define enable_goa no
%define enable_uoa no
%define enable_gtk no
%define enable_gdata no
%define enable_weather no
%define enable_email no

# should match configure.ac
%define so_edataserver 17
%define so_ecal 15
%define so_edata_cal 20
%define so_edata_book 17
%define so_ebook 14
%define so_camel 43
%define so_ebackend 6
%define _evo_version 3.9


Name:           evolution-data-server
Version:        3.9.90
Release:        0
Summary:        Evolution Data Server
License:        LGPL-2.0+
Group:          Development/Libraries
Url:            http://www.gnome.org
Source0:        http://download.gnome.org/sources/evolution-data-server/%{baseline}/%{name}-%{version}.tar.xz
Source98:       baselibs.conf
BuildRequires:  db4-devel
BuildRequires:  fdupes
BuildRequires:  gcc-c++
BuildRequires:  gettext-tools
BuildRequires:  glibc-locale
BuildRequires:  gnome-common
BuildRequires:  gperf
BuildRequires:  gtk-doc
BuildRequires:  intltool
BuildRequires:  vala
BuildRequires:  pkgconfig(icu-i18n)
BuildRequires:  pkgconfig(gcr-base-3) >= 3.4
%if %{?enable_goa} != no
BuildRequires:  pkgconfig(goa-1.0) >= 3.2
%endif
BuildRequires:  pkgconfig(gobject-introspection-1.0)
%if %{?enable_gtk} != no
BuildRequires:  pkgconfig(gtk+-3.0)
%endif
%if %{?enable_weather} != no
BuildRequires:  pkgconfig(gweather-3.0) >= 3.5.0
%endif
# Not sure what this is for. Not checked by current configure.ac?
# BuildRequires:  pkgconfig(libIDL-2.0)
%if %{?enable_gdata} != no
BuildRequires:  pkgconfig(libgdata) >= 0.10
BuildRequires:  pkgconfig(oauth)
%endif
BuildRequires:  pkgconfig(libical) >= 0.43
BuildRequires:  pkgconfig(libsecret-unstable) >= 0.5
BuildRequires:  pkgconfig(libsoup-2.4) >= 2.40.3
BuildRequires:  pkgconfig(nss)
BuildRequires:  pkgconfig(python-2.7)
BuildRequires:  pkgconfig(sqlite3) >= 3.5

Recommends:     %{name}-locale = %{version}
%ifarch  %ix86
Obsoletes:      evolution-data-server-32bit
%endif
Requires(post): glib2-tools
Requires(postun): glib2-tools

%description
Evolution Data Server provides a central location for your address book
and calendar in the GNOME Desktop.


%package -n libcamel
Summary:        Evolution Data Server - Messaging Library
Group:          System/Libraries

%description -n libcamel
Evolution Data Server provides a central location for your address book
and calendar in the GNOME Desktop.

This package contains a shared system library for messaging.


%package -n libebackend
Summary:        Evolution Data Server - Backend Utilities Library
Group:          System/Libraries

%description -n libebackend
Evolution Data Server provides a central location for your address book
and calendar in the GNOME Desktop.

This package contains a shared system library for backends.


%package -n libebook
Summary:        Evolution Data Server - Address Book Client Library
Group:          System/Libraries

%description -n libebook
Evolution Data Server provides a central location for your address book
and calendar in the GNOME Desktop.

This package contains a shared system library to access address books.


%package -n libebook-contacts
Summary:        Evolution Data Server - Address Book Client Library
Group:          System/Libraries

%description -n libebook-contacts
Evolution Data Server provides a central location for your address book
and calendar in the GNOME Desktop.

This package contains a shared system library to access address books.


%if %{?with_introspection}

%package -n typelib-EBookContacts
Summary:        Evolution Data Server - Address Book Backend Library, Introspection bindings
Group:          System/Libraries

%description -n typelib-EBookContacts
Evolution Data Server provides a central location for your address book
and calendar in the GNOME Desktop.

This package provides the GObject Introspection bindings for the library
for address book contacts.
%endif


%package -n libecal
Summary:        Evolution Data Server - Calendar Client Library
Group:          System/Libraries

%description -n libecal
Evolution Data Server provides a central location for your address book
and calendar in the GNOME Desktop.

This package contains a shared system library to access calendars.

%package -n libedata-book
Summary:        Evolution Data Server - Address Book Backend Library
Group:          System/Libraries

%description -n libedata-book
Evolution Data Server provides a central location for your address book
and calendar in the GNOME Desktop.

This package contains a shared system library for address book backends.


%if %{?with_introspection}

%package -n typelib-EBook
Summary:        Evolution Data Server - Address Book Backend Library, Introspection bindings
Group:          System/Libraries

%description -n typelib-EBook
Evolution Data Server provides a central location for your address book
and calendar in the GNOME Desktop.

This package provides the GObject Introspection bindings for the library
for address book backends.
%endif


%package -n libedata-cal
Summary:        Evolution Data Server - Calendar Backend Library
Group:          System/Libraries

%description -n libedata-cal
Evolution Data Server provides a central location for your address book
and calendar in the GNOME Desktop.

This package contains a shared system library for calendar backends.


%package -n libedataserver
Summary:        Evolution Data Server - Utilities Library
Group:          System/Libraries

%description -n libedataserver
Evolution Data Server provides a central location for your address book
and calendar in the GNOME Desktop.

This package contains a shared system library.


%if %{?with_introspection}

%package -n typelib-EDataServer
Summary:        Evolution Data Server - Utilities Library, Introspection bindings
Group:          System/Libraries

%description -n typelib-EDataServer
Evolution Data Server provides a central location for your address book
and calendar in the GNOME Desktop.

This package provides the GObject Introspection bindings for the
libedataserver library.

%endif


%package devel
Summary:        Evolution Data Server - Development Files
Group:          Development/Libraries
Requires:       evolution-data-server = %{?epoch:}%{version}
Requires:       libcamel = %{version}
Requires:       libebackend = %{version}
Requires:       libebook = %{version}
Requires:       libecal = %{version}
Requires:       libedata-book = %{version}
Requires:       libedata-cal = %{version}
Requires:       libedataserver = %{version}
%if %{?with_introspection}
Requires:       typelib-EBook = %{version}
Requires:       typelib-EDataServer = %{version}
%endif
Requires(post):   /sbin/ldconfig
Requires(postun): /sbin/ldconfig

%description devel
The Evolution Data Server development files provide the necessary
libraries, headers, and other files for developing applications which
use the Evolution Data Server for storing contact and calendar
information.


%package doc
Summary:        Evolution Data Server - Developer Documentation
Group:          Documentation
Requires:       %{name} = %{version}

%description doc
Evolution Data Server provides a central location for your address book
and calendar in the GNOME Desktop.

This package contains developer documentation.

%prep
%setup -q

%build

# "maintainer mode" depends on GTK and is not needed
# for packaging, so disable it.

%autogen \
 --libexecdir=%{_libexecdir}/evolution-data-server \
 --disable-maintainer-mode \
 --enable-ipv6=%{?enable_ipv6} \
 --enable-smime=%{?enable_smime} \
 --enable-nntp=%{?enable_nntp} \
 --disable-static \
 --disable-uoa \
 --enable-goa=%{?enable_goa} \
 --enable-weather=%{?enable_weather} \
 --enable-gtk=%{?enable_gtk} \
 --enable-google=%{?enable_gdata} \
%if %{?with_introspection}
 --enable-vala-bindings \
 --enable-introspection \
%else
 --disable-vala-bindings \
 --disable-introspection \
%endif
 # end of configure line

make %{?_smp_mflags} V=1

%install
%make_install
mkdir -p %{buildroot}/%{_datadir}/help
%find_lang evolution-data-server-%{_evo_version}
mv evolution-data-server-%{_evo_version}.lang evolution-data-server.lang
%fdupes %{buildroot}

%lang_package

%post
%glib2_gsettings_schema_post

%postun
%glib2_gsettings_schema_postun

%post -n libcamel -p /sbin/ldconfig

%postun -n libcamel -p /sbin/ldconfig

%post -n libebackend -p /sbin/ldconfig

%postun -n libebackend -p /sbin/ldconfig

%post -n libebook -p /sbin/ldconfig

%postun -n libebook -p /sbin/ldconfig

%post -n libebook-contacts -p /sbin/ldconfig

%postun -n libebook-contacts -p /sbin/ldconfig

%post -n  libecal -p /sbin/ldconfig

%postun -n  libecal -p /sbin/ldconfig

%post -n libedata-book -p /sbin/ldconfig

%postun -n libedata-book -p /sbin/ldconfig

%post -n libedata-cal -p /sbin/ldconfig

%postun -n libedata-cal -p /sbin/ldconfig

%post -n libedataserver -p /sbin/ldconfig

%postun -n libedataserver -p /sbin/ldconfig


%files
%defattr(-,root,root)
%license COPYING
%{_datadir}/GConf/gsettings/evolution-data-server.convert
%{_datadir}/GConf/gsettings/libedataserver.convert
%{_datadir}/glib-2.0/schemas/org.gnome.Evolution.DefaultSources.gschema.xml
%{_datadir}/glib-2.0/schemas/org.gnome.evolution.eds-shell.gschema.xml
%{_datadir}/glib-2.0/schemas/org.gnome.evolution.shell.network-config.gschema.xml
%{_datadir}/glib-2.0/schemas/org.gnome.evolution-data-server.addressbook.gschema.xml
%{_datadir}/glib-2.0/schemas/org.gnome.evolution-data-server.calendar.gschema.xml
%{_datadir}/pixmaps/evolution-data-server/
%{_datadir}/dbus-1/services/org.gnome.evolution.dataserver.*.service
%{_libdir}/evolution-data-server/
%if "%{_libdir}" != "%{_libexecdir}"
%{_libexecdir}/evolution-data-server/
%endif


%files -n libcamel
%defattr(-, root, root)
%{_libdir}/libcamel-1.2.so.%{so_camel}*


%files -n libebackend
%defattr(-, root, root)
%{_libdir}/libebackend-1.2.so.%{so_ebackend}*


%files -n libebook
%defattr(-, root, root)
%{_libdir}/libebook-1.2.so.%{so_ebook}*


%if %{?with_introspection}
%files -n typelib-EBook
%defattr(-, root, root)
%{_libdir}/girepository-1.0/EBook-1.2.typelib
%endif


%files -n libebook-contacts
%defattr(-, root, root)
%{_libdir}/libebook-contacts-1.2.so.0*


%if %{?with_introspection}
%files -n typelib-EBookContacts
%defattr(-, root, root)
%{_libdir}/girepository-1.0/EBookContacts-1.2.typelib
%endif


%files -n libecal
%defattr(-, root, root)
%{_libdir}/libecal-1.2.so.%{so_ecal}*


%files -n libedata-book
%defattr(-, root, root)
%{_libdir}/libedata-book-1.2.so.%{so_edata_book}*


%files -n libedata-cal
%defattr(-, root, root)
%{_libdir}/libedata-cal-1.2.so.%{so_edata_cal}*


%files -n libedataserver
%defattr(-, root, root)
%{_libdir}/libedataserver-1.2.so.%{so_edataserver}*


%if %{?with_introspection}
%files -n typelib-EDataServer
%defattr(-, root, root)
%{_libdir}/girepository-1.0/EDataServer-1.2.typelib
%endif


%files devel
%defattr(-, root, root)
%{_includedir}/evolution-data-server/
%{_libdir}/*.so
%{_libdir}/pkgconfig/*.pc
%if %{?with_introspection}
%{_datadir}/gir-1.0/*.gir
%dir %{_datadir}/vala
%dir %{_datadir}/vala/vapi
%{_datadir}/vala/vapi/*.deps
%{_datadir}/vala/vapi/*.vapi
%endif
