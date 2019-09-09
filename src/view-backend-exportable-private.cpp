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

#include "view-backend-exportable-private.h"
#include <sys/types.h>
#include <sys/socket.h>

ViewBackend::ViewBackend(ClientBundle* clientBundle, struct wpe_view_backend* backend)
    : m_clientBundle(clientBundle)
    , m_backend(backend)
{
    m_clientBundle->viewBackend = this;

    wl_list_init(&m_frameCallbacks);
}

ViewBackend::~ViewBackend()
{
    clearFrameCallbacks();

    WS::Instance::singleton().unregisterViewBackend(m_id);

    if (m_clientFd != -1)
        close(m_clientFd);

    if (m_source) {
        g_source_destroy(m_source);
        g_source_unref(m_source);
    }
    if (m_socket)
        g_object_unref(m_socket);
}

void ViewBackend::initialize()
{
    int sockets[2];
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
    if (ret == -1)
        return;

    m_socket = g_socket_new_from_fd(sockets[0], nullptr);
    if (!m_socket) {
        close(sockets[0]);
        close(sockets[1]);
        return;
    }

    g_socket_set_blocking(m_socket, FALSE);

    m_source = g_socket_create_source(m_socket, G_IO_IN, nullptr);
    g_source_set_callback(m_source, reinterpret_cast<GSourceFunc>(s_socketCallback), this, nullptr);
    g_source_set_priority(m_source, -70);
    g_source_set_can_recurse(m_source, TRUE);
    g_source_attach(m_source, g_main_context_get_thread_default());

    m_clientFd = sockets[1];

    wpe_view_backend_dispatch_set_size(m_backend,
                                       m_clientBundle->initialWidth,
                                       m_clientBundle->initialHeight);
}

int ViewBackend::clientFd()
{
    return dup(m_clientFd);
}

void ViewBackend::frameCallback(struct wl_resource* callback)
{
    auto* resource = new FrameCallbackResource;
    resource->resource = callback;
    resource->destroyListener.notify = FrameCallbackResource::destroyNotify;
    wl_resource_add_destroy_listener(callback, &resource->destroyListener);
    wl_list_insert(&m_frameCallbacks, &resource->link);
}

void ViewBackend::exportBufferResource(struct wl_resource* bufferResource)
{
    m_clientBundle->exportBuffer(bufferResource);
}

void ViewBackend::exportLinuxDmabuf(const struct linux_dmabuf_buffer *dmabuf_buffer)
{
    m_clientBundle->exportBuffer(dmabuf_buffer);
}

void ViewBackend::dispatchFrameCallbacks()
{
    FrameCallbackResource* resource;
    wl_list_for_each(resource, &m_frameCallbacks, link) {
        wl_callback_send_done(resource->resource, 0);
    }
    clearFrameCallbacks();

    wl_client_flush(m_client);
    wpe_view_backend_dispatch_frame_displayed(m_backend);
}

void ViewBackend::releaseBuffer(struct wl_resource* buffer_resource)
{
    wl_buffer_send_release(buffer_resource);
    wl_client_flush(m_client);
}

gboolean ViewBackend::s_socketCallback(GSocket* socket, GIOCondition condition, gpointer data)
{
    if (!(condition & G_IO_IN))
        return TRUE;

    uint32_t message[2];
    gssize len = g_socket_receive(socket, reinterpret_cast<gchar*>(message), sizeof(uint32_t) * 2,
        nullptr, nullptr);
    if (len == -1)
        return FALSE;

    if (len == sizeof(uint32_t) * 2 && message[0] == 0x42) {
        auto& viewBackend = *static_cast<ViewBackend*>(data);
        viewBackend.m_id = message[1];
        viewBackend.m_client = WS::Instance::singleton().registerViewBackend(viewBackend.m_id, viewBackend);
    }

    return TRUE;
}

void ViewBackend::clearFrameCallbacks()
{
    FrameCallbackResource* resource;
    FrameCallbackResource* next;
    wl_list_for_each_safe(resource, next, &m_frameCallbacks, link) {
        wl_list_remove(&resource->link);
        wl_list_remove(&resource->destroyListener.link);
        wl_resource_destroy(resource->resource);
        delete resource;
    }
    wl_list_init(&m_frameCallbacks);
}

void ViewBackend::FrameCallbackResource::destroyNotify(struct wl_listener* listener, void*)
{
    FrameCallbackResource* resource;
    resource = wl_container_of(listener, resource, destroyListener);

    wl_list_remove(&resource->link);
    delete resource;
}
