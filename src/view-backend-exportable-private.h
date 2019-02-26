/*
 * Copyright (C) 2018 Igalia S.L.
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

#include "ws.h"
#include <gio/gio.h>
#include <vector>
#include <wpe-fdo/view-backend-exportable.h>

class ViewBackend;

class ClientBundle {
public:
    ClientBundle(void* _data, ViewBackend* _viewBackend, uint32_t _initialWidth, uint32_t _initialHeight)
        : data(_data)
        , viewBackend(_viewBackend)
        , initialWidth(_initialWidth)
        , initialHeight(_initialHeight)
    {
    }

    virtual ~ClientBundle() = default;

    virtual void exportBuffer(struct wl_resource*, uint32_t, uint32_t) = 0;
    virtual void exportBuffer(const struct linux_dmabuf_buffer*, uint32_t, uint32_t) = 0;

    void* data;
    ViewBackend* viewBackend;
    uint32_t initialWidth;
    uint32_t initialHeight;
};

class ViewBackend : public WS::ExportableClient {
public:
    ViewBackend(ClientBundle* clientBundle, struct wpe_view_backend* backend);
    ~ViewBackend();

    void initialize();
    int clientFd();
    void frameCallback(struct wl_resource* callbackResource) override;
    void exportBufferResource(struct wl_resource*, uint32_t, uint32_t) override;
    void exportLinuxDmabuf(const struct linux_dmabuf_buffer*, uint32_t, uint32_t) override;
    void dispatchFrameCallback();
    void releaseBuffer(struct wl_resource* buffer_resource);

private:
    static gboolean s_socketCallback(GSocket*, GIOCondition, gpointer);

    uint32_t m_id { 0 };
    struct wl_client* m_client { nullptr };

    ClientBundle* m_clientBundle;
    struct wpe_view_backend* m_backend;

    std::vector<struct wl_resource*> m_callbackResources;

    GSocket* m_socket;
    GSource* m_source;
    int m_clientFd { -1 };
};

struct wpe_view_backend_exportable_fdo {
    ClientBundle* clientBundle;
    struct wpe_view_backend* backend;
};

static struct wpe_view_backend_interface view_backend_exportable_fdo_interface = {
    // create
    [](void* data, struct wpe_view_backend* backend) -> void*
    {
        auto* clientBundle = reinterpret_cast<ClientBundle*>(data);
        return new ViewBackend(clientBundle, backend);
    },
    // destroy
    [](void* data)
    {
        auto* backend = reinterpret_cast<ViewBackend*>(data);
        delete backend;
    },
    // initialize
    [](void* data)
    {
        auto& backend = *reinterpret_cast<ViewBackend*>(data);
        backend.initialize();
    },
    // get_renderer_host_fd
    [](void* data) -> int
    {
        auto& backend = *reinterpret_cast<ViewBackend*>(data);
        return backend.clientFd();
    }
};
