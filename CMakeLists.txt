cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-exceptions -fno-rtti")

find_package(EGL REQUIRED)
find_package(GLIB REQUIRED COMPONENTS gio)
find_package(Wayland REQUIRED)
find_package(WaylandEGL REQUIRED)
find_package(WPEBackend REQUIRED)

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
    include/wpe-fdo/view-backend-exportable.h
)

set(WPEBACKEND_FDO_SOURCES
    src/fdo.cpp
    src/renderer-backend-egl.cpp
    src/renderer-host.cpp
    src/view-backend-exportable-fdo.cpp
    src/ws.cpp
)

add_library(WPEBackend-fdo SHARED ${WPEBACKEND_FDO_SOURCES})
target_include_directories(WPEBackend-fdo PRIVATE ${WPEBACKEND_FDO_INCLUDE_DIRECTORIES})
target_link_libraries(WPEBackend-fdo ${WPEBACKEND_FDO_LIBRARIES})

install(
    TARGETS WPEBackend-fdo
    EXPORT WPEBackend-fdoTargets
    LIBRARY DESTINATION lib
)
install(
    FILES ${WPEBACKEND_FDO_PUBLIC_HEADERS}
    DESTINATION include/wpe-fdo
)
