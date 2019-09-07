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
    struct BufferResource {
        struct wl_resource* resource;
        EGLImageKHR image;

        struct wl_list link;
        struct wl_listener destroyListener;

        static void destroyNotify(struct wl_listener*, void*);
    };

    ClientBundleEGLDeprecated(const struct wpe_view_backend_exportable_fdo_egl_client* _client, void* data,
                              ViewBackend* viewBackend, uint32_t initialWidth, uint32_t initialHeight)
        : ClientBundle(data, viewBackend, initialWidth, initialHeight)
        , client(_client)
    {
        wl_list_init(&bufferResources);
    }

    virtual ~ClientBundleEGLDeprecated()
    {
        BufferResource* resource;
        BufferResource* next;
        wl_list_for_each_safe(resource, next, &bufferResources, link) {
            WS::Instance::singleton().destroyImage(resource->image);
            viewBackend->releaseBuffer(resource->resource);

            wl_list_remove(&resource->link);
            wl_list_remove(&resource->destroyListener.link);
            delete resource;
        }
        wl_list_init(&bufferResources);
    }

    void exportBuffer(struct wl_resource *buffer) override
    {
        EGLImageKHR image = WS::Instance::singleton().createImage(buffer);
        if (!image)
            return;

        auto* resource = new BufferResource;
        resource->resource = buffer;
        resource->image = image;
        resource->destroyListener.notify = BufferResource::destroyNotify;

        wl_resource_add_destroy_listener(buffer, &resource->destroyListener);
        wl_list_insert(&bufferResources, &resource->link);

        client->export_egl_image(data, image);
    }

    void exportBuffer(const struct linux_dmabuf_buffer *dmabuf_buffer) override
    {
        EGLImageKHR image = WS::Instance::singleton().createImage(dmabuf_buffer);
        if (!image)
            return;

        auto* resource = new BufferResource;
        resource->resource = dmabuf_buffer->buffer_resource;
        resource->image = image;
        resource->destroyListener.notify = BufferResource::destroyNotify;

        wl_resource_add_destroy_listener(dmabuf_buffer->buffer_resource, &resource->destroyListener);
        wl_list_insert(&bufferResources, &resource->link);

        client->export_egl_image(data, image);
    }

    void releaseImage(EGLImageKHR image)
    {
        BufferResource* matchingResource = nullptr;
        BufferResource* resource;
        wl_list_for_each(resource, &bufferResources, link) {
            if (resource->image == image) {
                matchingResource = resource;
                break;
            }
        }

        WS::Instance::singleton().destroyImage(image);

        if (matchingResource) {
            viewBackend->releaseBuffer(matchingResource->resource);

            wl_list_remove(&matchingResource->link);
            wl_list_remove(&matchingResource->destroyListener.link);
            delete matchingResource;
        }
    }

    const struct wpe_view_backend_exportable_fdo_egl_client* client;

    struct wl_list bufferResources;
};

void ClientBundleEGLDeprecated::BufferResource::destroyNotify(struct wl_listener* listener, void*)
{
    BufferResource* resource;
    resource = wl_container_of(listener, resource, destroyListener);

    wl_list_remove(&resource->link);
    delete resource;
}

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

    void releaseImage(struct wpe_fdo_egl_exported_image* image)
    {
        image->exported = false;

        if (image->bufferResource)
            viewBackend->releaseBuffer(image->bufferResource);
        else
            deleteImage(image);
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
        image->exported = true;
        client->export_fdo_egl_image(data, image);
    }

    static void deleteImage(struct wpe_fdo_egl_exported_image* image)
    {
        assert(image->eglImage);
        WS::Instance::singleton().destroyImage(image->eglImage);

        delete image;
    }

    static void bufferDestroyListenerCallback(struct wl_listener* listener, void*)
    {
        struct wpe_fdo_egl_exported_image* image;
        image = wl_container_of(listener, image, bufferDestroyListener);

        image->bufferResource = nullptr;

        if (!image->exported)
            deleteImage(image);
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
    static_cast<ClientBundleEGLDeprecated*>(exportable->clientBundle)->releaseImage(image);
}

__attribute__((visibility("default")))
void
wpe_view_backend_exportable_fdo_egl_dispatch_release_exported_image(struct wpe_view_backend_exportable_fdo* exportable, struct wpe_fdo_egl_exported_image* image)
{
    static_cast<ClientBundleEGL*>(exportable->clientBundle)->releaseImage(image);
}

}
