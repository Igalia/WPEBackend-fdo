/*
 * Copyright (C) 2018, 2019 Igalia S.L.
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

#include "view-backend-exportable-fdo-egl-private.h"
#include "view-backend-exportable-private.h"
#include "ws.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <cassert>
#include <list>
#include <wpe-fdo/view-backend-exportable-egl.h>

namespace {

class ClientBundleEGLDeprecated final : public ClientBundle {
public:
    struct buffer_data {
        struct wl_resource *buffer_resource;
        EGLImageKHR egl_image;
    };

    ClientBundleEGLDeprecated(const struct wpe_view_backend_exportable_fdo_egl_client* _client, void* data,
                              ViewBackend* viewBackend, uint32_t initialWidth, uint32_t initialHeight)
        : ClientBundle(data, viewBackend, initialWidth, initialHeight)
        , client(_client)
    {
    }

    virtual ~ClientBundleEGLDeprecated()
    {
        for (auto* buf_data : m_buffers) {
            assert(buf_data->egl_image);
            WS::Instance::singleton().destroyImage(buf_data->egl_image);

            delete buf_data;
        }
        m_buffers.clear();
    }

    void exportBuffer(struct wl_resource *bufferResource) override
    {
        EGLImageKHR image = WS::Instance::singleton().createImage(bufferResource);
        if (!image)
            return;

        auto* buf_data = new struct buffer_data;
        buf_data->buffer_resource = bufferResource;
        buf_data->egl_image = image;
        m_buffers.push_back(buf_data);

        client->export_egl_image(data, image);
    }

    void exportBuffer(const struct linux_dmabuf_buffer *dmabuf_buffer) override
    {
        EGLImageKHR image = WS::Instance::singleton().createImage(dmabuf_buffer);
        if (!image)
            return;

        auto* buf_data = new struct buffer_data;
        buf_data->buffer_resource = nullptr;
        buf_data->egl_image = image;
        m_buffers.push_back(buf_data);

        client->export_egl_image(data, image);
    }

    struct buffer_data* releaseImage(EGLImageKHR image)
    {
        for (auto* buf_data : m_buffers)
            if (buf_data->egl_image == image) {
                m_buffers.remove(buf_data);
                WS::Instance::singleton().destroyImage(buf_data->egl_image);

                return buf_data;
            }

        return nullptr;
    }

    const struct wpe_view_backend_exportable_fdo_egl_client* client;

private:
    std::list<struct buffer_data*> m_buffers;
};

class ClientBundleEGL final : public ClientBundle {
public:
    ClientBundleEGL(const struct wpe_view_backend_exportable_fdo_egl_client* _client, void* data,
                    ViewBackend* viewBackend, uint32_t initialWidth, uint32_t initialHeight)
        : ClientBundle(data, viewBackend, initialWidth, initialHeight)
        , client(_client)
    {
    }

    virtual ~ClientBundleEGL() = default;

    void exportBuffer(struct wl_resource* bufferResource) override
    {
        if (auto* image = findImage(bufferResource)) {
            exportImage(image);
            return;
        }

        EGLImageKHR eglImage = WS::Instance::singleton().createImage(bufferResource);
        if (!eglImage)
            return;

        auto* image = new struct wpe_fdo_egl_exported_image;
        image->eglImage = eglImage;
        image->bufferResource = bufferResource;
        WS::Instance::singleton().queryBufferSize(bufferResource, &image->width, &image->height);
        wl_list_init(&image->bufferDestroyListener.link);
        image->bufferDestroyListener.notify = bufferDestroyListenerCallback;
        wl_resource_add_destroy_listener(bufferResource, &image->bufferDestroyListener);

        exportImage(image);
    }

    void exportBuffer(const struct linux_dmabuf_buffer* dmabufBuffer) override
    {
        if (auto* image = findImage(dmabufBuffer->buffer_resource)) {
            exportImage(image);
            return;
        }

        EGLImageKHR eglImage = WS::Instance::singleton().createImage(dmabufBuffer);
        if (!eglImage)
            return;

        auto* image = new struct wpe_fdo_egl_exported_image;
        image->eglImage = eglImage;
        image->bufferResource = dmabufBuffer->buffer_resource;
        image->width = dmabufBuffer->attributes.width;
        image->height = dmabufBuffer->attributes.height;
        wl_list_init(&image->bufferDestroyListener.link);
        image->bufferDestroyListener.notify = bufferDestroyListenerCallback;
        wl_resource_add_destroy_listener(dmabufBuffer->buffer_resource, &image->bufferDestroyListener);

        exportImage(image);
    }

    const struct wpe_view_backend_exportable_fdo_egl_client* client;

private:

    struct wpe_fdo_egl_exported_image* findImage(struct wl_resource* bufferResource)
    {
        if (auto* listener = wl_resource_get_destroy_listener(bufferResource, bufferDestroyListenerCallback)) {
            struct wpe_fdo_egl_exported_image* image;
            return wl_container_of(listener, image, bufferDestroyListener);
        }

        return nullptr;
    }

    void exportImage(struct wpe_fdo_egl_exported_image* image)
    {
        image->locked = true;
        client->export_fdo_egl_image(data, image);
    }

    static void bufferDestroyListenerCallback(struct wl_listener* listener, void*)
    {
        struct wpe_fdo_egl_exported_image* image;
        image = wl_container_of(listener, image, bufferDestroyListener);
        if (!image->locked)
            wpe_fdo_egl_exported_image_destroy(image);
        else
            image->locked = false;
    }
};

} // namespace

extern "C" {

__attribute__((visibility("default")))
struct wpe_view_backend_exportable_fdo*
wpe_view_backend_exportable_fdo_egl_create(const struct wpe_view_backend_exportable_fdo_egl_client* client, void* data, uint32_t width, uint32_t height)
{
    ClientBundle* clientBundle;
    if (client->export_fdo_egl_image)
        clientBundle = new ClientBundleEGL(client, data, nullptr, width, height);
    else
        clientBundle = new ClientBundleEGLDeprecated(client, data, nullptr, width, height);

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
    auto* clientBundle = reinterpret_cast<ClientBundleEGLDeprecated*>(exportable->clientBundle);

    auto* buffer_data = clientBundle->releaseImage(image);
    if (!buffer_data)
        return;

    /* the EGL image has already been destroyed by ClientBundleEGL */

    if (buffer_data->buffer_resource) {
        wpe_view_backend_exportable_fdo_dispatch_release_buffer(exportable,
                                                                buffer_data->buffer_resource);
    }

    delete buffer_data;
}

__attribute__((visibility("default")))
void
wpe_view_backend_exportable_fdo_egl_dispatch_release_exported_image(struct wpe_view_backend_exportable_fdo* exportable, struct wpe_fdo_egl_exported_image* image)
{
    if (image->locked) {
        image->locked = false;
        if (image->bufferResource)
            wpe_view_backend_exportable_fdo_dispatch_release_buffer(exportable, image->bufferResource);
        return;
    }

    wpe_fdo_egl_exported_image_destroy(image);
}

}
