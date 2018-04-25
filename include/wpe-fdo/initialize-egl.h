#if !defined(__WPE_FDO_EGL_H_INSIDE__) && !defined(WPE_FDO_COMPILATION)
#error "Only <wpe/fdo-egl.h> can be included directly."
#endif

#ifndef __initialize_egl_h__
#define __initialize_egl_h__

#ifdef __cplusplus
extern "C" {
#endif

#include <EGL/egl.h>

void
wpe_fdo_initialize_for_egl_display(EGLDisplay);

#ifdef __cplusplus
}
#endif

#endif /* __initialize_egl_h__ */
