#include "ws.h"
#include <wpe-spi/WPE.renderer_host_interface.h>

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
