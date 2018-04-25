#pragma once

#include <wayland-egl.h>

#include <wpe/wpe-egl.h>
#include <wpe/wpe.h>

extern struct wpe_renderer_host_interface fdo_renderer_host;

extern struct wpe_renderer_backend_egl_interface fdo_renderer_backend_egl;
extern struct wpe_renderer_backend_egl_target_interface fdo_renderer_backend_egl_target;

extern struct wpe_input_key_mapper_interface libxkbcommon_input_key_mapper_interface;
