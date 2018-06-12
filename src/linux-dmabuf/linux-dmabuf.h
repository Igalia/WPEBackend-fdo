/*
 * Adopted from the Weston project <https://cgit.freedesktop.org/wayland/weston>
 * along with its license.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <wayland-server.h>

#define MAX_DMABUF_PLANES 4
#ifndef DRM_FORMAT_MOD_INVALID
#define DRM_FORMAT_MOD_INVALID ((1ULL<<56) - 1)
#endif
#ifndef DRM_FORMAT_MOD_LINEAR
#define DRM_FORMAT_MOD_LINEAR 0
#endif

typedef void* EGLDisplay;

struct linux_dmabuf_attributes {
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t flags; /* enum zlinux_buffer_params_flags */
    int8_t n_planes;
    int32_t fd[MAX_DMABUF_PLANES];
    uint32_t offset[MAX_DMABUF_PLANES];
    uint32_t stride[MAX_DMABUF_PLANES];
    uint64_t modifier[MAX_DMABUF_PLANES];
};

struct linux_dmabuf_buffer;

bool
linux_dmabuf_setup(struct wl_display *wl_display, EGLDisplay egl_display);

void
linux_dmabuf_teardown(void);

const struct linux_dmabuf_buffer *
linux_dmabuf_get_buffer(struct wl_resource *buffer_resource);

const struct linux_dmabuf_attributes *
linux_dmabuf_get_buffer_attributes(const struct linux_dmabuf_buffer *buffer);

#ifdef __cplusplus
}
#endif
