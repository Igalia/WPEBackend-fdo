#include <dlfcn.h>
#include <stdlib.h>

#include <stdio.h>

static void* s_spi_library_handle = 0;

static void* wpe_spi_load_entrypoint(const char* entrypoint)
{
    if (!s_spi_library_handle) {
        s_spi_library_handle = dlopen("libWPEBackendSPI.so", RTLD_NOW);
        if (!s_spi_library_handle)
            abort();
    }
    return dlsym(s_spi_library_handle, entrypoint);
}

typedef void*(*PFN_WPE_SPI_WHAT)();

#define GENERATE_WPE_ENTRYPOINT_LOADER(whut) \
    PFN_WPE_SPI_WHAT __wpe_spi_client_ ## whut; \
    void* __wpe_spi_client_ ## whut ## _resolve() \
    { \
        void* entrypoint = wpe_spi_load_entrypoint("__wpe_spi_entrypoint__wpe_" #whut); \
        __wpe_spi_client_ ## whut = (PFN_WPE_SPI_WHAT)entrypoint; \
        return ((PFN_WPE_SPI_WHAT)entrypoint)(); \
    } \
    PFN_WPE_SPI_WHAT __wpe_spi_client_ ## whut = __wpe_spi_client_ ## whut ## _resolve;

GENERATE_WPE_ENTRYPOINT_LOADER(renderer_host_create);
GENERATE_WPE_ENTRYPOINT_LOADER(renderer_host_create_client);

GENERATE_WPE_ENTRYPOINT_LOADER(view_backend_create);
GENERATE_WPE_ENTRYPOINT_LOADER(view_backend_create_with_backend_interface);
GENERATE_WPE_ENTRYPOINT_LOADER(view_backend_destroy);
GENERATE_WPE_ENTRYPOINT_LOADER(view_backend_set_backend_client);
GENERATE_WPE_ENTRYPOINT_LOADER(view_backend_set_input_client);
GENERATE_WPE_ENTRYPOINT_LOADER(view_backend_initialize);
GENERATE_WPE_ENTRYPOINT_LOADER(view_backend_get_renderer_host_fd);
GENERATE_WPE_ENTRYPOINT_LOADER(view_backend_dispatch_set_size);
GENERATE_WPE_ENTRYPOINT_LOADER(view_backend_dispatch_frame_displayed);
GENERATE_WPE_ENTRYPOINT_LOADER(view_backend_dispatch_pointer_event);
GENERATE_WPE_ENTRYPOINT_LOADER(view_backend_dispatch_keyboard_event);
GENERATE_WPE_ENTRYPOINT_LOADER(view_backend_dispatch_axis_event);
GENERATE_WPE_ENTRYPOINT_LOADER(view_backend_dispatch_touch_event);

GENERATE_WPE_ENTRYPOINT_LOADER(renderer_backend_egl_create);
GENERATE_WPE_ENTRYPOINT_LOADER(renderer_backend_egl_destroy);
GENERATE_WPE_ENTRYPOINT_LOADER(renderer_backend_egl_get_native_display);

GENERATE_WPE_ENTRYPOINT_LOADER(renderer_backend_egl_target_create);
GENERATE_WPE_ENTRYPOINT_LOADER(renderer_backend_egl_target_destroy);
GENERATE_WPE_ENTRYPOINT_LOADER(renderer_backend_egl_target_set_client);
GENERATE_WPE_ENTRYPOINT_LOADER(renderer_backend_egl_target_initialize);
GENERATE_WPE_ENTRYPOINT_LOADER(renderer_backend_egl_target_get_native_window);
GENERATE_WPE_ENTRYPOINT_LOADER(renderer_backend_egl_target_resize);
GENERATE_WPE_ENTRYPOINT_LOADER(renderer_backend_egl_target_frame_rendered);
GENERATE_WPE_ENTRYPOINT_LOADER(renderer_backend_egl_target_frame_will_render);
GENERATE_WPE_ENTRYPOINT_LOADER(renderer_backend_egl_target_dispatch_frame_complete);

GENERATE_WPE_ENTRYPOINT_LOADER(renderer_backend_egl_offscreen_target_create);
GENERATE_WPE_ENTRYPOINT_LOADER(renderer_backend_egl_offscreen_target_destroy);
GENERATE_WPE_ENTRYPOINT_LOADER(renderer_backend_egl_offscreen_target_initialize);
GENERATE_WPE_ENTRYPOINT_LOADER(renderer_backend_egl_offscreen_target_get_native_window);

GENERATE_WPE_ENTRYPOINT_LOADER(input_key_mapper_create);
GENERATE_WPE_ENTRYPOINT_LOADER(input_key_mapper_identifier_for_key_event);
GENERATE_WPE_ENTRYPOINT_LOADER(input_key_mapper_windows_key_code_for_key_event);
GENERATE_WPE_ENTRYPOINT_LOADER(input_key_mapper_single_character_for_key_event);
