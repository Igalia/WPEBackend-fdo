cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-exceptions -fno-rtti")

find_package(EGL REQUIRED)
find_package(GLIB REQUIRED COMPONENTS gio)
find_package(Libxkbcommon 0.4.0 REQUIRED)
find_package(Wayland REQUIRED)
find_package(WaylandEGL REQUIRED)

if (EGL_DEFINITIONS)
	add_definitions(${EGL_DEFINITIONS})
endif ()

set(WPEBACKEND_FDO_INCLUDE_DIRECTORIES
    "include"
    "src"
    "src/external"
    ${EGL_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${LIBXKBCOMMON_INCLUDE_DIRS}
    ${WAYLAND_INCLUDE_DIRS}
    ${WAYLAND_EGL_INCLUDE_DIRS}
)

set(WPEBACKEND_FDO_LIBRARIES
    ${EGL_LIBRARIES}
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${LIBXKBCOMMON_LIBRARIES}
    ${WAYLAND_LIBRARIES}
    ${WAYLAND_EGL_LIBRARIES}
)

set(WPEBACKEND_FDO_PUBLIC_HEADERS
    include/wpe-fdo/initialize-egl.h
    include/wpe-fdo/view-backend-exportable.h
)

set(WPEBACKEND_FDO_SOURCES
    src/fdo.cpp
    src/initialize-egl.cpp
    src/input-key-mapper-libxkbcommon.cpp
    src/renderer-backend-egl.cpp
    src/renderer-host.cpp
    src/view-backend-exportable-fdo.cpp
    src/ws.cpp

    src/wpe-spi/WPE.spi_integration.c
)

add_library(WPEBackend-fdo SHARED ${WPEBACKEND_FDO_SOURCES})
target_include_directories(WPEBackend-fdo PRIVATE ${WPEBACKEND_FDO_INCLUDE_DIRECTORIES})
target_link_libraries(WPEBackend-fdo ${WPEBACKEND_FDO_LIBRARIES})

set(WPEBACKEND_FDO_VERSION_MAJOR 0)
set(WPEBACKEND_FDO_VERSION_MINOR 1)
set(WPEBACKEND_FDO_VERSION ${WPEBACKEND_FDO_VERSION_MAJOR}.${WPEBACKEND_FDO_VERSION_MINOR})

set_target_properties(WPEBackend-fdo
    PROPERTIES
    VERSION ${WPEBACKEND_FDO_VERSION}
    SOVERSION ${WPEBACKEND_FDO_VERSION_MAJOR}
)

install(
    TARGETS WPEBackend-fdo
    EXPORT WPEBackend-fdoTargets
    LIBRARY DESTINATION lib
)
install(
    FILES ${WPEBACKEND_FDO_PUBLIC_HEADERS}
    DESTINATION include/wpe-fdo
)

configure_file(wpebackend-fdo.pc.in wpebackend-fdo.pc @ONLY)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/wpebackend-fdo.pc"
    DESTINATION lib/pkgconfig
)
