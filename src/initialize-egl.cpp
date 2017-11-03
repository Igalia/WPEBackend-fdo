#include <wpe-fdo/initialize-egl.h>

#include "ws.h"

__attribute__((visibility("default")))
void
wpe_fdo_initialize_for_egl_display(EGLDisplay display)
{
    WS::Instance::singleton().initialize(display);
}
