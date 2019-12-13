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

#pragma once

#ifndef __video_plane_display_dmabuf_source_h__
#define __video_plane_display_dmabuf_source_h__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wpe_renderer_backend_egl;
struct wpe_video_plane_display_dmabuf_source;

struct wpe_video_plane_display_dmabuf_source*
wpe_video_plane_display_dmabuf_source_create(struct wpe_renderer_backend_egl*);

void
wpe_video_plane_display_dmabuf_source_destroy(struct wpe_video_plane_display_dmabuf_source*);

typedef void (*wpe_video_plane_display_dmabuf_source_update_release_notify_t)(void *data);

void
wpe_video_plane_display_dmabuf_source_update(struct wpe_video_plane_display_dmabuf_source*, int fd, int32_t x, int32_t y, int32_t width, int32_t height, uint32_t stride,
    wpe_video_plane_display_dmabuf_source_update_release_notify_t notify, void* notify_data);

#ifdef __cplusplus
}
#endif

#endif // __video_plane_display_dmabuf_source_h__