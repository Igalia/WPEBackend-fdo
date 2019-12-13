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

#ifndef __video_plane_display_dmabuf_receiver_h__
#define __video_plane_display_dmabuf_receiver_h__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wpe_video_plane_display_dmabuf_export;

struct wpe_video_plane_display_dmabuf_receiver {
    void (*handle_dmabuf)(void* data, struct wpe_video_plane_display_dmabuf_export*, uint32_t id, int fd, int32_t x, int32_t y, int32_t width, int32_t height, uint32_t stride);
};

void
wpe_video_plane_display_dmabuf_register_receiver(const struct wpe_video_plane_display_dmabuf_receiver*, void* data);

void
wpe_video_plane_display_dmabuf_export_release(struct wpe_video_plane_display_dmabuf_export*);

#ifdef __cplusplus
}
#endif

#endif // __video_plane_display_dmabuf_receiver_h__
