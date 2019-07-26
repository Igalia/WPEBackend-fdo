# - Try to find WPE.
# Once done, this will define
#
#  WPE_FOUND - system has WPE.
#  WPE_INCLUDE_DIRS - the WPE include directories
#  WPE_LIBRARIES - link these to use WPE.
#
# Copyright (C) 2016 Igalia S.L.
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
pkg_check_modules(WPE IMPORTED_TARGET wpe-1.0)

find_path(WPE_INCLUDE_DIR
    NAMES wpe/wpe.h
    HINTS ${WPE_INCLUDEDIR} ${WPE_INCLUDE_DIRS}
)
find_library(WPE_LIBRARY
    NAMES wpe-1.0
    HINTS ${WPE_LIBDIR} ${WPE_LIBRARY_DIRS}
)
mark_as_advanced(WPE_INCLUDE_DIR WPE_LIBRARY)

# If pkg-config has not found the module but find_path+find_library have
# figured out where the header and library are, create the PkgConfig::WPE
# imported target anyway with the found paths.
#
if (WPE_LIBRARY AND NOT TARGET WPE::libwpe)
    add_library(WPE::libwpe INTERFACE IMPORTED)
    if (TARGET PkgConfig::WPE)
        target_link_libraries(WPE::libwpe INTERFACE PkgConfig::WPE)
    else ()
        set_property(TARGET WPE::libwpe PROPERTY
            INTERFACE_LINK_LIBRARIES "${WPE_LIBRARY}")
        set_property(TARGET WPE::libwpe PROPERTY
            INTERFACE_INCLUDE_DIRECTORIES "${WPE_INCLUDE_DIR}")
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(WPE REQUIRED_VARS WPE_LIBRARY WPE_INCLUDE_DIR)
