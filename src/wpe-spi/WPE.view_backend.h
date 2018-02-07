#pragma once

extern "C" {

typedef void*(*PFN_WPE_SPI_WHAT)();

#define PFN_WPE_SPI_VIEW_BACKEND_CREATE (struct wpe_view_backend*(*)())
extern PFN_WPE_SPI_WHAT __wpe_spi_client_view_backend_create;

static inline struct wpe_view_backend*
wpe_view_backend_create()
{
    return (PFN_WPE_SPI_VIEW_BACKEND_CREATE(__wpe_spi_client_view_backend_create()))();
}

#define PFN_WPE_SPI_VIEW_BACKEND_CREATE_WITH_BACKEND_INTERFACE (struct wpe_view_backend*(*)(struct wpe_view_backend_interface*, void*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_view_backend_create_with_backend_interface;

static inline struct wpe_view_backend*
wpe_view_backend_create_with_backend_interface(struct wpe_view_backend_interface* interface, void* interface_data)
{
    return (PFN_WPE_SPI_VIEW_BACKEND_CREATE_WITH_BACKEND_INTERFACE(__wpe_spi_client_view_backend_create_with_backend_interface()))(interface, interface_data);
}

#define PFN_WPE_SPI_VIEW_BACKEND_DESTROY (void(*)(struct wpe_view_backend*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_view_backend_destroy;

static inline void
wpe_view_backend_destroy(struct wpe_view_backend* backend)
{
    (PFN_WPE_SPI_VIEW_BACKEND_DESTROY(__wpe_spi_client_view_backend_destroy()))(backend);
}

#define PFN_WPE_SPI_VIEW_BACKEND_SET_BACKEND_CLIENT (void(*)(struct wpe_view_backend*, struct wpe_view_backend_client*, void*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_view_backend_set_backend_client;

static inline void
wpe_view_backend_set_backend_client(struct wpe_view_backend* backend, struct wpe_view_backend_client* backend_client, void* backend_client_data)
{
    (PFN_WPE_SPI_VIEW_BACKEND_SET_BACKEND_CLIENT(__wpe_spi_client_view_backend_set_backend_client()))(backend, backend_client, backend_client_data);
}

#define PFN_WPE_SPI_VIEW_BACKEND_SET_INPUT_CLIENT (void(*)(struct wpe_view_backend*, struct wpe_view_backend_input_client*, void*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_view_backend_set_input_client;

static inline void
wpe_view_backend_set_input_client(struct wpe_view_backend* backend, struct wpe_view_backend_input_client* input_client, void* input_client_data)
{
    (PFN_WPE_SPI_VIEW_BACKEND_SET_INPUT_CLIENT(__wpe_spi_client_view_backend_set_input_client()))(backend, input_client, input_client_data);
}

#define PFN_WPE_SPI_VIEW_BACKEND_INITIALIZE (void(*)(struct wpe_view_backend*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_view_backend_initialize;

static inline void
wpe_view_backend_initialize(struct wpe_view_backend* backend)
{
    (PFN_WPE_SPI_VIEW_BACKEND_INITIALIZE(__wpe_spi_client_view_backend_initialize()))(backend);
}

#define PFN_WPE_SPI_VIEW_BACKEND_GET_RENDERER_HOST_FD (int(*)(struct wpe_view_backend*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_view_backend_get_renderer_host_fd;

static inline int
wpe_view_backend_get_renderer_host_fd(struct wpe_view_backend* backend)
{
    return (PFN_WPE_SPI_VIEW_BACKEND_GET_RENDERER_HOST_FD(__wpe_spi_client_view_backend_get_renderer_host_fd()))(backend);
}


#define PFN_WPE_SPI_VIEW_BACKEND_DISPATCH_SET_SIZE (void(*)(struct wpe_view_backend*, uint32_t, uint32_t))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_view_backend_dispatch_set_size;

static inline void
wpe_view_backend_dispatch_set_size(struct wpe_view_backend* backend, uint32_t width, uint32_t height)
{
    (PFN_WPE_SPI_VIEW_BACKEND_DISPATCH_SET_SIZE(__wpe_spi_client_view_backend_dispatch_set_size()))(backend, width, height);
}

#define PFN_WPE_SPI_VIEW_BACKEND_DISPATCH_FRAME_DISPLAYED (void(*)(struct wpe_view_backend*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_view_backend_dispatch_frame_displayed;

static inline void
wpe_view_backend_dispatch_frame_displayed(struct wpe_view_backend* backend)
{
    (PFN_WPE_SPI_VIEW_BACKEND_DISPATCH_FRAME_DISPLAYED(__wpe_spi_client_view_backend_dispatch_frame_displayed()))(backend);
}

#define PFN_WPE_SPI_VIEW_BACKEND_DISPATCH_POINTER_EVENT (void(*)(struct wpe_view_backend*, struct wpe_input_pointer_event*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_view_backend_dispatch_pointer_event;

static inline void
wpe_view_backend_dispatch_pointer_event(struct wpe_view_backend* backend, struct wpe_input_pointer_event* event)
{
    (PFN_WPE_SPI_VIEW_BACKEND_DISPATCH_POINTER_EVENT(__wpe_spi_client_view_backend_dispatch_pointer_event()))(backend, event);
}

#define PFN_WPE_SPI_VIEW_BACKEND_DISPATCH_KEYBOARD_EVENT (void(*)(struct wpe_view_backend*, struct wpe_input_keyboard_event*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_view_backend_dispatch_keyboard_event;

static inline void
wpe_view_backend_dispatch_keyboard_event(struct wpe_view_backend* backend, struct wpe_input_keyboard_event* event)
{
    (PFN_WPE_SPI_VIEW_BACKEND_DISPATCH_KEYBOARD_EVENT(__wpe_spi_client_view_backend_dispatch_keyboard_event()))(backend, event);
}

#define PFN_WPE_SPI_VIEW_BACKEND_DISPATCH_AXIS_EVENT (void(*)(struct wpe_view_backend*, struct wpe_input_axis_event*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_view_backend_dispatch_axis_event;

static inline void
wpe_view_backend_dispatch_axis_event(struct wpe_view_backend* backend, struct wpe_input_axis_event* event)
{
    (PFN_WPE_SPI_VIEW_BACKEND_DISPATCH_AXIS_EVENT(__wpe_spi_client_view_backend_dispatch_axis_event()))(backend, event);
}

#define PFN_WPE_SPI_VIEW_BACKEND_DISPATCH_TOUCH_EVENT (void(*)(struct wpe_view_backend*, struct wpe_input_touch_event*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_view_backend_dispatch_touch_event;

static inline void
wpe_view_backend_dispatch_touch_event(struct wpe_view_backend* backend, struct wpe_input_touch_event* event)
{
    (PFN_WPE_SPI_VIEW_BACKEND_DISPATCH_TOUCH_EVENT(__wpe_spi_client_view_backend_dispatch_touch_event()))(backend, event);
}

}
