#pragma once

#include <wayland-egl.h>

#include <wpe/renderer-backend-egl.h>
#include <wpe/renderer-host.h>
#include <wpe/view-backend.h>

extern struct wpe_renderer_host_interface fdo_renderer_host;

extern struct wpe_renderer_backend_egl_interface fdo_renderer_backend_egl;
extern struct wpe_renderer_backend_egl_target_interface fdo_renderer_backend_egl_target;
