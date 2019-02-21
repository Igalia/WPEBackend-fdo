/*
 * Copyright (C) 2017, 2018 Igalia S.L.
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

#include <glib.h>
#include <unordered_map>
#include <wayland-server.h>
#include "linux-dmabuf/linux-dmabuf.h"

typedef void *EGLDisplay;
typedef void *EGLImageKHR;

namespace WS {

struct ExportableClient {
    virtual void frameCallback(struct wl_resource*) = 0;
    virtual void exportBufferResource(struct wl_resource*) = 0;
    virtual void exportLinuxDmabuf(const struct linux_dmabuf_buffer *dmabuf_buffer) = 0;
};

struct Surface;

class Instance {
public:
    static Instance& singleton();
    ~Instance();

    bool initialize(EGLDisplay);

    int createClient();

    void createSurface(uint32_t, Surface*);
    struct wl_client* registerViewBackend(uint32_t, ExportableClient&);
    void unregisterViewBackend(uint32_t);

    EGLDisplay getEGLDisplay()
    {
        return m_eglDisplay;
    }

    EGLImageKHR createImage(struct wl_resource*);
    EGLImageKHR createImage(const struct linux_dmabuf_buffer*);
    void destroyImage(EGLImageKHR);

private:
    Instance();

    struct wl_display* m_display;
    struct wl_global* m_compositor;
    struct wl_global* m_wpeBridge;
    GSource* m_source;

    std::unordered_map<uint32_t, Surface*> m_viewBackendMap;

    EGLDisplay m_eglDisplay;
};

} // namespace WS
