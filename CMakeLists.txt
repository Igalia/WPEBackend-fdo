cmake_minimum_required(VERSION 3.0)
cmake_policy(VERSION 3.0)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(VersioningUtils)

SET_PROJECT_VERSION(1 1 0)
set(WPEBACKEND_FDO_API_VERSION 0.1)

# Before making a release, the LT_VERSION string should be modified.
# The string is of the form C:R:A.
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to C+1:0:A+1
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to C+1:0:0
# - If the interface is the same as the previous version, change to C:R+1:A
CALCULATE_LIBRARY_VERSIONS_FROM_LIBTOOL_TRIPLE(LIBWPEBACKEND_FDO 2 0 1)

project(wpebackend-fdo VERSION "${PROJECT_VERSION}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-exceptions -fno-rtti")

include(DistTargets)
include(GNUInstallDirs)

find_package(EGL REQUIRED)
find_package(GLIB REQUIRED COMPONENTS gio)
find_package(Wayland REQUIRED)
find_package(WaylandEGL REQUIRED)
find_package(WPE REQUIRED)

if (EGL_DEFINITIONS)
	add_definitions(${EGL_DEFINITIONS})
endif ()

add_definitions(-DWPE_FDO_COMPILATION)

if (NOT (WAYLAND_wayland-client_VERSION VERSION_LESS 1.10))
    add_definitions(-DWAYLAND_1_10_OR_GREATER)
endif ()

set(WPEBACKEND_FDO_INCLUDE_DIRECTORIES
    "include"
    ${EGL_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${WAYLAND_INCLUDE_DIRS}
    ${WAYLAND_EGL_INCLUDE_DIRS}
    ${WPE_INCLUDE_DIRS}
)

set(WPEBACKEND_FDO_LIBRARIES
    ${EGL_LIBRARIES}
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${WAYLAND_LIBRARIES}
    ${WAYLAND_EGL_LIBRARIES}
    ${WPE_LIBRARIES}
)

set(WPEBACKEND_FDO_PUBLIC_HEADERS
    include/wpe-fdo/initialize-egl.h
    include/wpe-fdo/view-backend-exportable.h
    include/wpe-fdo/fdo-egl.h
    include/wpe-fdo/fdo.h
)

set(WPEBACKEND_FDO_SOURCES
    src/fdo.cpp
    src/initialize-egl.cpp
    src/renderer-backend-egl.cpp
    src/renderer-host.cpp
    src/view-backend-exportable-fdo.cpp
    src/view-backend-exportable-private.cpp
    src/ws.cpp
    src/linux-dmabuf/linux-dmabuf.c
    src/linux-dmabuf/linux-dmabuf-protocol.c
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
