#include "interfaces.h"
#include "ws.h"

struct wpe_renderer_host_interface fdo_renderer_host = {
    // create
    []() -> void*
    {
        return nullptr;
    },
    // destroy
    [](void* data)
    {
    },
    // create_client
    [](void* data) -> int
    {
        return WS::Instance::singleton().createClient();
    },
};

extern "C" {

__attribute__((visibility("default")))
void
wpe_renderer_host_exportable_fdo_initialize(EGLDisplay eglDisplay)
{
    WS::Instance::singleton().initialize(eglDisplay);
}

}
