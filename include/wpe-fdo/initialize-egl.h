#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <EGL/egl.h>

void
wpe_fdo_initialize_for_egl_display(EGLDisplay);

#ifdef __cplusplus
}
#endif
