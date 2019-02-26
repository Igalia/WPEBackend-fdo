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

#include "linux-dmabuf/linux-dmabuf.h"
#include "view-backend-exportable-private.h"
#include "ws.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <cassert>
#include <list>
#include <wpe-fdo/view-backend-exportable-egl.h>

namespace {

struct buffer_data {
    struct wl_resource *buffer_resource;
    EGLImageKHR egl_image;
    uint32_t width;
    uint32_t height;
};

class ClientBundleEGL final : public ClientBundle {
public:
    ClientBundleEGL(const struct wpe_view_backend_exportable_fdo_egl_client* _client, void* data,
                    ViewBackend* viewBackend, uint32_t initialWidth, uint32_t initialHeight)
        : ClientBundle(data, viewBackend, initialWidth, initialHeight)
        , client(_client)
    {
    }

    virtual ~ClientBundleEGL()
    {
        for (auto* buf_data : m_buffers) {
            assert(buf_data->egl_image);
            WS::Instance::singleton().destroyImage(buf_data->egl_image);

            delete buf_data;
        }
        m_buffers.clear();
    }

    void exportBuffer(struct wl_resource *bufferResource, uint32_t width, uint32_t height) override
    {
        EGLImageKHR image = WS::Instance::singleton().createImage(bufferResource);
        if (!image)
            return;

        auto* buf_data = new struct buffer_data;
        buf_data->buffer_resource = bufferResource;
        buf_data->egl_image = image;
        buf_data->width = width;
        buf_data->height = height;
        m_buffers.push_back(buf_data);

        client->export_egl_image(data, image);
    }

    void exportBuffer(const struct linux_dmabuf_buffer *dmabuf_buffer, uint32_t width, uint32_t height) override
    {
        EGLImageKHR image = WS::Instance::singleton().createImage(dmabuf_buffer);
        if (!image)
            return;

        auto* buf_data = new struct buffer_data;
        buf_data->buffer_resource = nullptr;
        buf_data->egl_image = image;
        buf_data->width = width;
        buf_data->height = height;
        m_buffers.push_back(buf_data);

        client->export_egl_image(data, image);
    }

    struct buffer_data* bufferData(EGLImageKHR image)
    {
        for (auto* buf_data : m_buffers)
            if (buf_data->egl_image == image)
                return buf_data;

        return nullptr;
    }

    struct wl_resource* releaseImage(EGLImageKHR image)
    {
        auto* buffer_data = bufferData(image);
        if (!buffer_data)
            return nullptr;

        m_buffers.remove(buffer_data);
        WS::Instance::singleton().destroyImage(buffer_data->egl_image);
        auto* buffer_resource = buffer_data->buffer_resource;
        delete buffer_data;

        return buffer_resource;
    }

    const struct wpe_view_backend_exportable_fdo_egl_client* client;

private:
    std::list<struct buffer_data*> m_buffers;
};

} // namespace

extern "C" {

__attribute__((visibility("default")))
struct wpe_view_backend_exportable_fdo*
wpe_view_backend_exportable_fdo_egl_create(const struct wpe_view_backend_exportable_fdo_egl_client* client, void* data, uint32_t width, uint32_t height)
{
    auto* clientBundle = new ClientBundleEGL(client, data, nullptr, width, height);

    struct wpe_view_backend* backend = wpe_view_backend_create_with_backend_interface(&view_backend_exportable_fdo_interface, clientBundle);

    auto* exportable = new struct wpe_view_backend_exportable_fdo;
    exportable->clientBundle = clientBundle;
    exportable->backend = backend;

    return exportable;
}

__attribute__((visibility("default")))
void
wpe_view_backend_exportable_fdo_egl_dispatch_release_image(struct wpe_view_backend_exportable_fdo* exportable, EGLImageKHR image)
{
    auto* clientBundle = reinterpret_cast<ClientBundleEGL*>(exportable->clientBundle);

    auto* buffer_resource = clientBundle->releaseImage(image);
    if (buffer_resource)
        wpe_view_backend_exportable_fdo_dispatch_release_buffer(exportable, buffer_resource);
}

__attribute__((visibility("default")))
bool
wpe_view_backend_exportable_fdo_egl_dispatch_query_image_size(struct wpe_view_backend_exportable_fdo* exportable, EGLImageKHR image, uint32_t* width, uint32_t* height)
{
    auto* clientBundle = reinterpret_cast<ClientBundleEGL*>(exportable->clientBundle);

    auto* buffer_data = clientBundle->bufferData(image);
    if (!buffer_data)
        return false;

    if (width)
        *width = buffer_data->width;
    if (height)
        *height = buffer_data->height;

    return true;
}

}
