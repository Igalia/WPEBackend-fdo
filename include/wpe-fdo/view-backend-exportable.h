#if !defined(__WPE_FDO_H_INSIDE__) && !defined(WPE_FDO_COMPILATION)
#error "Only <wpe/fdo.h> can be included directly."
#endif

#ifndef __view_backend_exportable_h__
#define __view_backend_exportable_h__

#ifdef __cplusplus
extern "C" {
#endif

#include <wpe/wpe.h>

struct wl_resource;
struct wpe_view_backend_exportable_fdo;

struct wpe_view_backend_exportable_fdo_client {
    void (*export_buffer_resource)(void* data, struct wl_resource* buffer_resource);
    void (*export_linux_dmabuf)(void* data, uint32_t width, uint32_t height,
                                uint32_t format, uint32_t flags,
                                uint32_t num_planes, const int32_t* fds,
                                const uint32_t* strides, const uint32_t* offsets,
                                const uint64_t* modifiers);
    void (*_wpe_reserved0)(void);
    void (*_wpe_reserved1)(void);
    void (*_wpe_reserved2)(void);
    void (*_wpe_reserved3)(void);
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

#endif /* __view_backend_exportable_h___ */
