/*
 * Copyright (C) 2019 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if !defined(__WPE_FDO_EGL_H_INSIDE__) && !defined(WPE_FDO_COMPILATION)
#error "Only <wpe/fdo-egl.h> can be included directly."
#endif

#ifndef __exported_image_egl_h__
#define __exported_image_egl_h__

#ifdef __cplusplus
extern "C" {
#endif

#include <wpe/wpe.h>

typedef void* EGLImageKHR;

struct wpe_fdo_egl_exported_image;

typedef void (*wpe_fdo_egl_exported_image_destroy_notify_t)(void *data, struct wpe_fdo_egl_exported_image *image);

void
wpe_fdo_egl_exported_image_set_destroy_notify(struct wpe_fdo_egl_exported_image*, wpe_fdo_egl_exported_image_destroy_notify_t, void*);

uint32_t
wpe_fdo_egl_exported_image_get_width(struct wpe_fdo_egl_exported_image*);

uint32_t
wpe_fdo_egl_exported_image_get_height(struct wpe_fdo_egl_exported_image*);

EGLImageKHR
wpe_fdo_egl_exported_image_get_egl_image(struct wpe_fdo_egl_exported_image*);

#ifdef __cplusplus
}
#endif

#endif /* __exported_image_egl_h___ */
