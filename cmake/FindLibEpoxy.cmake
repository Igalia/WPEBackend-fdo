# - Try to find libepoxy.
# Once done, this will define
#
#  EPOXY_FOUND - system has libepoxy
#  EPOXY_INCLUDE_DIRS - the libepoxy include directories
#  EPOXY_LIBRARIES - link these to use libepoxy.
#
# Copyright (C) 2020 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND ITS CONTRIBUTORS ``AS
# IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR ITS
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

find_package(PkgConfig)
pkg_check_modules(EPOXY IMPORTED_TARGET epoxy)

find_path(EPOXY_INCLUDE_DIR
    NAMES epoxy/egl.h
    HINTS ${EPOXY_INCLUDEDIR} ${EPOXY_INCLUDE_DIRS}
)
find_library(EPOXY_LIBRARY
    NAMES epoxy
    HINTS ${EPOXY_LIBDIR} ${EPOXY_LIBRARY_DIRS}
)
mark_as_advanced(EPOXY_INCLUDE_DIR EPOXY_LIBRARY)

# If pkg-config has not found the module but find_path+find_library have
# figured out where the header and library are, create the PkgConfig::Epoxy
# imported target anyway with the found paths.
#
if (EPOXY_LIBRARIES AND NOT TARGET Epoxy::libepoxy)
    add_library(Epoxy::libepoxy INTERFACE IMPORTED)
    if (TARGET PkgConfig::EPOXY)
        set_property(TARGET Epoxy::libepoxy PROPERTY
            INTERFACE_LINK_LIBRARIES PkgConfig::EPOXY)
    else ()
        set_property(TARGET Epoxy::libepoxy PROPERTY
            INTERFACE_LINK_LIBRARIES ${EPOXY_LIBRARY})
        set_property(TARGET Epoxy::libepoxy PROPERTY
            INTERFACE_INCLUDE_DIRECTORIES ${EPOXY_INCLUDE_DIR})
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(EPOXY REQUIRED_VARS EPOXY_LIBRARY EPOXY_INCLUDE_DIR)
