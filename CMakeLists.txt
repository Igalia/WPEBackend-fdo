cmake_minimum_required(VERSION 3.3)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(VersioningUtils)

SET_PROJECT_VERSION(1 3 1)
set(WPEBACKEND_FDO_API_VERSION 1.0)

# Before making a release, the LT_VERSION string should be modified.
# The string is of the form C:R:A.
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to C+1:0:A+1
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to C+1:0:0
# - If the interface is the same as the previous version, change to C:R+1:A
CALCULATE_LIBRARY_VERSIONS_FROM_LIBTOOL_TRIPLE(LIBWPEBACKEND_FDO 4 0 3)

project(wpebackend-fdo VERSION "${PROJECT_VERSION}")

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include(DistTargets)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 99)

foreach (cxxflag -fno-exceptions -fno-rtti -Wall)
    check_cxx_compiler_flag("${cxxflag}" CXX_HAS_FLAG_${cxxflag})
    if (CXX_HAS_FLAG_${cxxflag})
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${cxxflag}")
    endif ()
endforeach ()

foreach (cflag -Wall)
    check_c_compiler_flag("${cflag}" CC_HAS_FLAG_${cflag})
    if (CC_HAS_FLAG_${cflag})
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${cflag}")
    endif ()
endforeach ()

find_package(EGL REQUIRED)
find_package(GLIB REQUIRED COMPONENTS gio gobject)
find_package(Wayland REQUIRED client server egl)
find_package(WaylandScanner REQUIRED)
find_package(WPE REQUIRED)

add_definitions(-DWPE_FDO_COMPILATION)

configure_file(include/wpe-fdo/version.h.cmake "${CMAKE_BINARY_DIR}/wpe-fdo/version.h" @ONLY)

set(WPEBACKEND_FDO_INCLUDE_DIRECTORIES
    "include"
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${GLIB_INCLUDE_DIRS}
)

set(WPEBACKEND_FDO_LIBRARIES
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
    GL::egl
    Wayland::client
    Wayland::server
    Wayland::egl
    WPE::libwpe
)

set(WPEBACKEND_FDO_PUBLIC_HEADERS
    ${CMAKE_BINARY_DIR}/wpe-fdo/version.h
    include/wpe-fdo/exported-image-egl.h
    include/wpe-fdo/initialize-egl.h
    include/wpe-fdo/view-backend-exportable.h
    include/wpe-fdo/fdo-egl.h
    include/wpe-fdo/fdo.h
)

set(WPEBACKEND_FDO_GENERATED_SOURCES
    ${CMAKE_BINARY_DIR}/bridge/wpe-bridge-protocol.c
)

set(WPEBACKEND_FDO_SOURCES
    ${WPEBACKEND_FDO_GENERATED_SOURCES}

    src/exported-image-egl.cpp
    src/fdo.cpp
    src/initialize-egl.cpp
    src/ipc.cpp
    src/renderer-backend-egl.cpp
    src/renderer-host.cpp
    src/view-backend-exportable-fdo.cpp
    src/view-backend-exportable-private.cpp
    src/ws.cpp
    src/ws-client.cpp

    src/linux-dmabuf/linux-dmabuf.cpp
    src/linux-dmabuf/linux-dmabuf-protocol.c
)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bridge)
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/bridge/wpe-bridge-protocol.c
           ${CMAKE_BINARY_DIR}/bridge/wpe-bridge-client-protocol.h
           ${CMAKE_BINARY_DIR}/bridge/wpe-bridge-server-protocol.h
    DEPENDS ${CMAKE_SOURCE_DIR}/src/bridge/wpe-bridge.xml
    COMMAND ${WAYLAND_SCANNER} ${WAYLAND_SCANNER_CODE_ARG} < ${CMAKE_SOURCE_DIR}/src/bridge/wpe-bridge.xml > ${CMAKE_BINARY_DIR}/bridge/wpe-bridge-protocol.c
    COMMAND ${WAYLAND_SCANNER} client-header < ${CMAKE_SOURCE_DIR}/src/bridge/wpe-bridge.xml > ${CMAKE_BINARY_DIR}/bridge/wpe-bridge-client-protocol.h
    COMMAND ${WAYLAND_SCANNER} server-header < ${CMAKE_SOURCE_DIR}/src/bridge/wpe-bridge.xml > ${CMAKE_BINARY_DIR}/bridge/wpe-bridge-server-protocol.h
    VERBATIM
)

option(EXPORTABLE_EGL "Enable the exportable EGL interface" ON)

if (EXPORTABLE_EGL)
    list(APPEND WPEBACKEND_FDO_PUBLIC_HEADERS include/wpe-fdo/view-backend-exportable-egl.h)
    list(APPEND WPEBACKEND_FDO_SOURCES src/view-backend-exportable-fdo-egl.cpp)
endif ()

add_library(WPEBackend-fdo SHARED ${WPEBACKEND_FDO_SOURCES})
target_include_directories(WPEBackend-fdo PRIVATE ${WPEBACKEND_FDO_INCLUDE_DIRECTORIES})
target_link_libraries(WPEBackend-fdo ${WPEBACKEND_FDO_LIBRARIES})

set_target_properties(WPEBackend-fdo
    PROPERTIES
    OUTPUT_NAME WPEBackend-fdo-${WPEBACKEND_FDO_API_VERSION}
    VERSION ${LIBWPEBACKEND_FDO_VERSION}
    SOVERSION ${LIBWPEBACKEND_FDO_VERSION_MAJOR}
)

install(
    TARGETS WPEBackend-fdo
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(
    FILES ${WPEBACKEND_FDO_PUBLIC_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/wpe-fdo-${WPEBACKEND_FDO_API_VERSION}/wpe
)

configure_file(wpebackend-fdo.pc.in wpebackend-fdo-${WPEBACKEND_FDO_API_VERSION}.pc @ONLY)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/wpebackend-fdo-${WPEBACKEND_FDO_API_VERSION}.pc"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

option(BUILD_DOCS "Build the documentation" OFF)
if (BUILD_DOCS)
    find_program(HOTDOC hotdoc)
    if (NOT HOTDOC)
        message(FATAL_ERROR "hotdoc not found!")
    endif ()

    execute_process(
        COMMAND ${HOTDOC} --has-extension c-extension
        RESULT_VARIABLE HAS_HOTDOC_C_EXTENSION
    )
    if ("${HAS_HOTDOC_C_EXTENSION}" EQUAL 0)
        add_custom_target(Documentation ALL
            ${HOTDOC} run
                --project-name WPEBackend-fdo
                --project-version ${PROJECT_VERSION}
                --sitemap ${CMAKE_SOURCE_DIR}/docs/sitemap.txt
                --output ${CMAKE_CURRENT_BINARY_DIR}/Documentation/
                --c-smart-index
                --extra-c-flags=-DWPE_FDO_COMPILATION
                --c-sources
                    ${CMAKE_SOURCE_DIR}/include/wpe-fdo/*.h
                --c-include-directories
                    ${CMAKE_SOURCE_DIR}/include
                    ${WPEBACKEND_FDO_INCLUDE_DIRECTORIES}
        )
    else ()
        message(FATAL_ERROR "Hotdoc C extension not found, can't build documentation.")
    endif ()
endif ()
