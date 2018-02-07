#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_resource;
struct wpe_view_backend;
struct wpe_view_backend_exportable_fdo;

struct wpe_view_backend_exportable_fdo_client {
    void (*export_buffer_resource)(void* data, struct wl_resource* buffer_resource);
};

struct wpe_view_backend_exportable_fdo*
wpe_view_backend_exportable_fdo_create(struct wpe_view_backend_exportable_fdo_client*, void*, uint32_t width, uint32_t height);

void
wpe_view_backend_exportable_fdo_destroy(struct wpe_view_backend_exportable_fdo*);

struct wpe_view_backend*
wpe_view_backend_exportable_fdo_get_view_backend(struct wpe_view_backend_exportable_fdo*);

void
wpe_view_backend_exportable_fdo_dispatch_frame_complete(struct wpe_view_backend_exportable_fdo*);

void
wpe_view_backend_exportable_fdo_dispatch_release_buffer(struct wpe_view_backend_exportable_fdo*, struct wl_resource*);

#ifdef __cplusplus
}
#endif
