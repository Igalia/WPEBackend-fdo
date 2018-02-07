#pragma once

extern "C" {

typedef void*(*PFN_WPE_SPI_WHAT)();

#define PFN_WPE_SPI_RENDERER_BACKEND_EGL_CREATE (struct wpe_renderer_backend_egl*(*)(int))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_renderer_backend_egl_create;

static inline struct wpe_renderer_backend_egl*
wpe_renderer_backend_egl_create(int fd)
{
    return (PFN_WPE_SPI_RENDERER_BACKEND_EGL_CREATE(__wpe_spi_client_renderer_backend_egl_create()))(fd);
}

#define PFN_WPE_SPI_RENDERER_BACKEND_EGL_DESTROY (void(*)(struct wpe_renderer_backend_egl*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_renderer_backend_egl_destroy;

static inline void
wpe_renderer_backend_egl_destroy(struct wpe_renderer_backend_egl* backend)
{
    (PFN_WPE_SPI_RENDERER_BACKEND_EGL_DESTROY(__wpe_spi_client_renderer_backend_egl_destroy()))(backend);
}

#define PFN_WPE_SPI_RENDERER_BACKEND_EGL_GET_NATIVE_DISPLAY (EGLNativeDisplayType(*)(struct wpe_renderer_backend_egl*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_renderer_backend_egl_get_native_display;

static inline EGLNativeDisplayType
wpe_renderer_backend_egl_get_native_display(struct wpe_renderer_backend_egl* backend)
{
    return (PFN_WPE_SPI_RENDERER_BACKEND_EGL_GET_NATIVE_DISPLAY(__wpe_spi_client_renderer_backend_egl_get_native_display()))(backend);
}


#define PFN_WPE_SPI_RENDERER_BACKEND_EGL_TARGET_CREATE (struct wpe_renderer_backend_egl_target*(*)(int))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_renderer_backend_egl_target_create;

static inline struct wpe_renderer_backend_egl_target*
wpe_renderer_backend_egl_target_create(int fd)
{
    return (PFN_WPE_SPI_RENDERER_BACKEND_EGL_TARGET_CREATE(__wpe_spi_client_renderer_backend_egl_target_create()))(fd);
}

#define PFN_WPE_SPI_RENDERER_BACKEND_EGL_TARGET_DESTROY (void(*)(struct wpe_renderer_backend_egl_target*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_renderer_backend_egl_target_destroy;

static inline void
wpe_renderer_backend_egl_target_destroy(struct wpe_renderer_backend_egl_target* target)
{
    (PFN_WPE_SPI_RENDERER_BACKEND_EGL_TARGET_DESTROY(__wpe_spi_client_renderer_backend_egl_target_destroy()))(target);
}

#define PFN_WPE_SPI_RENDERER_BACKEND_EGL_TARGET_SET_CLIENT (void(*)(struct wpe_renderer_backend_egl_target*, struct wpe_renderer_backend_egl_target_client*, void*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_renderer_backend_egl_target_set_client;

static inline void
wpe_renderer_backend_egl_target_set_client(struct wpe_renderer_backend_egl_target* target, struct wpe_renderer_backend_egl_target_client* client, void* client_data)
{
    (PFN_WPE_SPI_RENDERER_BACKEND_EGL_TARGET_SET_CLIENT(__wpe_spi_client_renderer_backend_egl_target_set_client()))(target, client, client_data);
}

#define PFN_WPE_SPI_RENDERER_BACKEND_EGL_TARGET_INITIALIZE (void(*)(struct wpe_renderer_backend_egl_target*, struct wpe_renderer_backend_egl*, uint32_t, uint32_t))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_renderer_backend_egl_target_initialize;

static inline void
wpe_renderer_backend_egl_target_initialize(struct wpe_renderer_backend_egl_target* target, struct wpe_renderer_backend_egl* backend, uint32_t width, uint32_t height)
{
    (PFN_WPE_SPI_RENDERER_BACKEND_EGL_TARGET_INITIALIZE(__wpe_spi_client_renderer_backend_egl_target_initialize()))(target, backend, width, height);
}

#define PFN_WPE_SPI_RENDERER_BACKEND_EGL_TARGET_GET_NATIVE_WINDOW (EGLNativeWindowType(*)(struct wpe_renderer_backend_egl_target*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_renderer_backend_egl_target_get_native_window;

static inline EGLNativeWindowType
wpe_renderer_backend_egl_target_get_native_window(struct wpe_renderer_backend_egl_target* target)
{
    return (PFN_WPE_SPI_RENDERER_BACKEND_EGL_TARGET_GET_NATIVE_WINDOW(__wpe_spi_client_renderer_backend_egl_target_get_native_window()))(target);
}

#define PFN_WPE_SPI_RENDERER_BACKEND_EGL_TARGET_RESIZE (void(*)(struct wpe_renderer_backend_egl_target*, uint32_t, uint32_t))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_renderer_backend_egl_target_resize;

static inline void
wpe_renderer_backend_egl_target_resize(struct wpe_renderer_backend_egl_target* target, uint32_t width, uint32_t height)
{
    (PFN_WPE_SPI_RENDERER_BACKEND_EGL_TARGET_RESIZE(__wpe_spi_client_renderer_backend_egl_target_resize()))(target, width, height);
}

#define PFN_WPE_SPI_RENDERER_BACKEND_EGL_TARGET_FRAME_WILL_RENDER (void(*)(struct wpe_renderer_backend_egl_target*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_renderer_backend_egl_target_frame_will_render;

static inline void
wpe_renderer_backend_egl_target_frame_will_render(struct wpe_renderer_backend_egl_target* target)
{
    (PFN_WPE_SPI_RENDERER_BACKEND_EGL_TARGET_FRAME_WILL_RENDER(__wpe_spi_client_renderer_backend_egl_target_frame_will_render()))(target);
}

#define PFN_WPE_SPI_RENDERER_BACKEND_EGL_TARGET_FRAME_RENDERED (void(*)(struct wpe_renderer_backend_egl_target*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_renderer_backend_egl_target_frame_rendered;

static inline void
wpe_renderer_backend_egl_target_frame_rendered(struct wpe_renderer_backend_egl_target* target)
{
    (PFN_WPE_SPI_RENDERER_BACKEND_EGL_TARGET_FRAME_RENDERED(__wpe_spi_client_renderer_backend_egl_target_frame_rendered()))(target);
}

#define PFN_WPE_SPI_RENDERER_BACKEND_EGL_TARGET_DISPATCH_FRAME_COMPLETE (void(*)(struct wpe_renderer_backend_egl_target*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_renderer_backend_egl_target_dispatch_frame_complete;

static inline void wpe_renderer_backend_egl_target_dispatch_frame_complete(struct wpe_renderer_backend_egl_target* object)
{
    (PFN_WPE_SPI_RENDERER_BACKEND_EGL_TARGET_DISPATCH_FRAME_COMPLETE(__wpe_spi_client_renderer_backend_egl_target_dispatch_frame_complete()))(object);
}


#define PFN_WPE_SPI_RENDERER_BACKEND_EGL_OFFSCREEN_TARGET_CREATE (struct wpe_renderer_backend_egl_offscreen_target*(*)())
extern PFN_WPE_SPI_WHAT __wpe_spi_client_renderer_backend_egl_offscreen_target_create;

static inline struct wpe_renderer_backend_egl_offscreen_target*
wpe_renderer_backend_egl_offscreen_target_create()
{
    return (PFN_WPE_SPI_RENDERER_BACKEND_EGL_OFFSCREEN_TARGET_CREATE(__wpe_spi_client_renderer_backend_egl_offscreen_target_create))();
}

#define PFN_WPE_SPI_RENDERER_BACKEND_EGL_OFFSCREEN_TARGET_DESTROY (void(*)(struct wpe_renderer_backend_egl_offscreen_target*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_renderer_backend_egl_offscreen_target_destroy;

static inline void
wpe_renderer_backend_egl_offscreen_target_destroy(struct wpe_renderer_backend_egl_offscreen_target* target)
{
    (PFN_WPE_SPI_RENDERER_BACKEND_EGL_OFFSCREEN_TARGET_DESTROY(__wpe_spi_client_renderer_backend_egl_offscreen_target_destroy))(target);
}

#define PFN_WPE_SPI_RENDERER_BACKEND_EGL_OFFSCREEN_TARGET_INITIALIZE (void(*)(struct wpe_renderer_backend_egl_offscreen_target*, struct wpe_renderer_backend_egl*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_renderer_backend_egl_offscreen_target_initialize;

static inline void
wpe_renderer_backend_egl_offscreen_target_initialize(struct wpe_renderer_backend_egl_offscreen_target* target, struct wpe_renderer_backend_egl* backend)
{
    (PFN_WPE_SPI_RENDERER_BACKEND_EGL_OFFSCREEN_TARGET_INITIALIZE(__wpe_spi_client_renderer_backend_egl_offscreen_target_initialize))(target, backend);
}

#define PFN_WPE_SPI_RENDERER_BACKEND_EGL_OFFSCREEN_TARGET_GET_NATIVE_WINDOW (EGLNativeWindowType(*)(struct wpe_renderer_backend_egl_offscreen_target*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_renderer_backend_egl_offscreen_target_get_native_window;

static inline EGLNativeWindowType
wpe_renderer_backend_egl_offscreen_target_get_native_window(struct wpe_renderer_backend_egl_offscreen_target* target)
{
    return (PFN_WPE_SPI_RENDERER_BACKEND_EGL_OFFSCREEN_TARGET_GET_NATIVE_WINDOW(__wpe_spi_client_renderer_backend_egl_offscreen_target_get_native_window))(target);
}

}
