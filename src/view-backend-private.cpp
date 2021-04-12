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

#include "ipc-messages.h"
#include "view-backend-private.h"
#include <cassert>
#include <sys/types.h>
#include <sys/socket.h>

ViewBackend::ViewBackend(ClientBundle* clientBundle, struct wpe_view_backend* backend)
    : m_clientBundle(clientBundle)
    , m_backend(backend)
{
    m_clientBundle->viewBackend = this;

    wl_list_init(&m_frameCallbacks);
    wl_list_init(&m_clientDestroy.link);
}

ViewBackend::~ViewBackend()
{
    unregisterSurface(m_bridgeId);
    if (m_clientFd != -1)
        close(m_clientFd);
}

void ViewBackend::initialize()
{
    int sockets[2];
    int ret = socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets);
    if (ret == -1)
        return;

    m_socket = FdoIPC::Connection::create(sockets[0], this);
    if (!m_socket) {
        close(sockets[0]);
        close(sockets[1]);
        return;
    }

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

void ViewBackend::exportShmBuffer(struct wl_resource* bufferResource, struct wl_shm_buffer* shmBuffer)
{
    m_clientBundle->exportBuffer(bufferResource, shmBuffer);
}

void ViewBackend::exportEGLStreamProducer(struct wl_resource* bufferResource)
{
    m_clientBundle->exportEGLStreamProducer(bufferResource);
}

void ViewBackend::dispatchFrameCallbacks()
{
    if (G_UNLIKELY(!m_client))
        return;

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
    if (G_UNLIKELY(!m_client))
        return;

    wl_buffer_send_release(buffer_resource);
    wl_client_flush(m_client);
}

void ViewBackend::clientDestroyNotify(struct wl_listener* listener, void*)
{
    ViewBackend* self = wl_container_of(listener, self, m_clientDestroy);

    self->clearFrameCallbacks();
    WS::Instance::singleton().unregisterViewBackend(self->m_bridgeId);
    self->m_client = nullptr;
    self->m_bridgeId = 0;

    wl_list_remove(&self->m_clientDestroy.link);
}

void ViewBackend::registerSurface(uint32_t bridgeId)
{
    if (m_bridgeId == bridgeId)
        return;

    unregisterSurface(m_bridgeId);

    m_bridgeId = bridgeId;
    m_client = WS::Instance::singleton().registerViewBackend(m_bridgeId, *this);
    wl_client_add_destroy_listener(m_client, &m_clientDestroy);
}

void ViewBackend::unregisterSurface(uint32_t bridgeId)
{
    if (!bridgeId || m_bridgeId != bridgeId)
        return;

    // If the surfaceId is valid, we cannot have an invalid wl_client.
    g_assert(m_client != nullptr);

    // Destroying the client triggers the m_clientDestroy callback,
    // the rest of the teardown is done from there.
    wl_client_destroy(m_client);

    // After destroying the client, none of these can be valid.
    g_assert(m_client == nullptr);
    g_assert(m_bridgeId == 0);
}

void ViewBackend::didReceiveMessage(uint32_t messageId, uint32_t messageBody)
{
    switch (messageId) {
    case FdoIPC::Messages::RegisterSurface:
        registerSurface(messageBody);
        break;
    case FdoIPC::Messages::UnregisterSurface:
        unregisterSurface(messageBody);
        break;
    default:
        assert(!"WPE fdo received an invalid IPC message");
    }
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
