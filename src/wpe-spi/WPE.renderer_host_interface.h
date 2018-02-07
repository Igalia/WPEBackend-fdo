#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct wpe_renderer_host_interface {
    void* (*create)();
    void (*destroy)(void*);

    int (*create_client)(void*);
};

#ifdef __cplusplus
}
#endif
