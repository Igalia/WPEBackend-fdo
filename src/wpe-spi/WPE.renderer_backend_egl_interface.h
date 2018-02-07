#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wpe_renderer_backend_egl_interface {
    void* (*create)(int);
    void (*destroy)(void*);

    EGLNativeDisplayType (*get_native_display)(void*);
};

struct wpe_renderer_backend_egl_target;

struct wpe_renderer_backend_egl_target_interface {
    void* (*create)(struct wpe_renderer_backend_egl_target*, int);
    void (*destroy)(void*);

    void (*initialize)(void*, void*, uint32_t, uint32_t);
    EGLNativeWindowType (*get_native_window)(void*);
    void (*resize)(void*, uint32_t, uint32_t);
    void (*frame_will_render)(void*);
    void (*frame_rendered)(void*);
};

struct wpe_renderer_backend_egl_offscreen_target_interface {
    void* (*create)();
    void (*destroy)(void*);

    void (*initialize)(void*, void*);
    EGLNativeWindowType (*get_native_window)(void*);
};

#ifdef __cplusplus
}
#endif
