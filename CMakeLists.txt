cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-exceptions -fno-rtti")

find_package(EGL REQUIRED)
find_package(GLIB REQUIRED COMPONENTS gio)
find_package(Wayland REQUIRED)
find_package(WaylandEGL REQUIRED)
find_package(WPEBackend REQUIRED)

if (EGL_DEFINITIONS)
	add_definitions(${EGL_DEFINITIONS})
endif ()

add_definitions(-DWPE_FDO_COMPILATION)

set(WPEBACKEND_FDO_INCLUDE_DIRECTORIES
    "include"
    ${EGL_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${WAYLAND_INCLUDE_DIRS}
    ${WAYLAND_EGL_INCLUDE_DIRS}
    ${WPEBACKEND_INCLUDE_DIRS}
)

set(WPEBACKEND_FDO_LIBRARIES
    ${EGL_LIBRARIES}
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${WAYLAND_LIBRARIES}
    ${WAYLAND_EGL_LIBRARIES}
    ${WPEBACKEND_LIBRARIES}
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
    src/input-libxkbcommon.cpp
    src/ws.cpp
    src/linux-dmabuf/linux-dmabuf.c
)

add_library(WPEBackend-fdo SHARED ${WPEBACKEND_FDO_SOURCES})
target_include_directories(WPEBackend-fdo PRIVATE ${WPEBACKEND_FDO_INCLUDE_DIRECTORIES})
target_link_libraries(WPEBackend-fdo ${WPEBACKEND_FDO_LIBRARIES})

set(WPEBACKEND_FDO_VERSION_MAJOR 0)
set(WPEBACKEND_FDO_VERSION_MINOR 1)
set(WPEBACKEND_FDO_VERSION ${WPEBACKEND_FDO_VERSION_MAJOR}.${WPEBACKEND_FDO_VERSION_MINOR})

set(WPEBACKEND_FDO_API_VERSION 0.1)

set_target_properties(WPEBackend-fdo
    PROPERTIES
    OUTPUT_NAME WPEBackend-fdo-${WPEBACKEND_FDO_API_VERSION}
    VERSION ${WPEBACKEND_FDO_VERSION}
    SOVERSION ${WPEBACKEND_FDO_VERSION_MAJOR}
)

install(
    TARGETS WPEBackend-fdo
    LIBRARY DESTINATION lib
)
install(
    FILES ${WPEBACKEND_FDO_PUBLIC_HEADERS}
    DESTINATION include/wpe-fdo-${WPEBACKEND_FDO_API_VERSION}/wpe
)

configure_file(wpebackend-fdo.pc.in wpebackend-fdo-${WPEBACKEND_FDO_API_VERSION}.pc @ONLY)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/wpebackend-fdo-${WPEBACKEND_FDO_API_VERSION}.pc"
    DESTINATION lib/pkgconfig
)
