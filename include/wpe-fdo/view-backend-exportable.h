#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <wpe/view-backend.h>

struct wl_resource;
struct wpe_view_backend_exportable_fdo;

struct wpe_view_backend_exportable_fdo_client {
    void (*export_buffer_resource)(void* data, struct wl_resource* buffer_resource);
    void (*export_linux_dmabuf)(void* data, uint32_t width, uint32_t height,
                                uint32_t format, uint32_t flags,
                                uint32_t num_planes, const int32_t* fds,
                                const uint32_t* strides, const uint32_t* offsets,
                                const uint64_t* modifiers);
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
