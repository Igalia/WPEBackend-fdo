#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct wpe_view_backend;

struct wpe_view_backend_interface {
    void* (*create)(void*, struct wpe_view_backend*);
    void (*destroy)(void*);

    void (*initialize)(void*);
    int (*get_renderer_host_fd)(void*);
};

#ifdef __cplusplus
}
#endif
