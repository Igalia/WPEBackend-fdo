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

#include <assert.h>
#include "ws.h"
#include "view-backend-exportable-fdo.h"
#include "wpe-fdo/view-backend-exportable-egl.h"
#include "linux-dmabuf/linux-dmabuf.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <list>

namespace {

struct buffer_data {
    struct wl_resource *buffer_resource;
    EGLImageKHR egl_image;
};

class ClientBundleEGL : public ClientBundleBase {
public:
    ClientBundleEGL(struct wpe_view_backend_exportable_fdo_egl_client* _client, void* data,
                    ViewBackend* viewBackend, uint32_t initialWidth, uint32_t initialHeight)
        : ClientBundleBase(data, viewBackend, initialWidth, initialHeight)
        , client(_client)
    {
        m_eglDisplay = WS::Instance::singleton().getEGLDisplay();
        assert (m_eglDisplay);

        eglCreateImage = (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress ("eglCreateImageKHR");
        assert (eglCreateImage);

        eglDestroyImage = (PFNEGLDESTROYIMAGEKHRPROC)
            eglGetProcAddress ("eglDestroyImageKHR");
        assert (eglDestroyImage);
    }

    ~ClientBundleEGL()
    {
        for (auto* buf_data : m_buffers) {
            assert(buf_data->egl_image);
            eglDestroyImage(m_eglDisplay, buf_data->egl_image);

            delete buf_data;
        }
        m_buffers.clear();
    }

    void exportBuffer(struct wl_resource *bufferResource) override
    {
        static const EGLint image_attrs[] = {
            EGL_WAYLAND_PLANE_WL, 0,
            EGL_NONE
        };
        EGLImageKHR image = eglCreateImage (m_eglDisplay,
                                            EGL_NO_CONTEXT,
                                            EGL_WAYLAND_BUFFER_WL,
                                            bufferResource,
                                            image_attrs);
        if (!image)
            return;

        auto* buf_data = new struct buffer_data;
        buf_data->buffer_resource = bufferResource;
        buf_data->egl_image = image;
        m_buffers.push_back(buf_data);

        client->export_egl_image(data, image);
    }

    void exportBuffer(const struct linux_dmabuf_buffer *dmabuf_buffer)
    {
        const struct linux_dmabuf_attributes *buf_attribs =
            linux_dmabuf_get_buffer_attributes(dmabuf_buffer);
        assert(buf_attribs);

        EGLint attribs[50];
        int atti = 0;

        attribs[atti++] = EGL_WIDTH;
        attribs[atti++] = buf_attribs->width;
        attribs[atti++] = EGL_HEIGHT;
        attribs[atti++] = buf_attribs->height;
        attribs[atti++] = EGL_LINUX_DRM_FOURCC_EXT;
        attribs[atti++] = buf_attribs->format;

        for (int i = 0; i < buf_attribs->n_planes; i++) {
            attribs[atti++] = EGL_DMA_BUF_PLANE0_FD_EXT;
            attribs[atti++] = buf_attribs->fd[i];
            attribs[atti++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
            attribs[atti++] = buf_attribs->offset[i];
            attribs[atti++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
            attribs[atti++] = buf_attribs->stride[i];
            attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
            attribs[atti++] = buf_attribs->modifier[i] & 0xFFFFFFFF;
            attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
            attribs[atti++] = buf_attribs->modifier[i] >> 32;
        }

        attribs[atti++] = EGL_NONE;

        EGLImageKHR image = eglCreateImage (m_eglDisplay,
                                            EGL_NO_CONTEXT,
                                            EGL_LINUX_DMA_BUF_EXT,
                                            nullptr,
                                            attribs);
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
                eglDestroyImage(m_eglDisplay, buf_data->egl_image);

                return buf_data;
            }

        return nullptr;
    }

    struct wpe_view_backend_exportable_fdo_egl_client* client;

private:
    EGLDisplay m_eglDisplay;
    PFNEGLCREATEIMAGEKHRPROC eglCreateImage;
    PFNEGLDESTROYIMAGEKHRPROC eglDestroyImage;

    std::list<struct buffer_data*> m_buffers;
};

} // namespace

extern "C" {

__attribute__((visibility("default")))
struct wpe_view_backend_exportable_fdo*
wpe_view_backend_exportable_fdo_egl_create(struct wpe_view_backend_exportable_fdo_egl_client* client, void* data, uint32_t width, uint32_t height)
{
    auto* clientBundle = new ClientBundleEGL(client, data, nullptr, width, height);

    return wpe_view_backend_exportable_fdo_new(clientBundle);
}

__attribute__((visibility("default")))
void
wpe_view_backend_exportable_fdo_egl_dispatch_release_image(struct wpe_view_backend_exportable_fdo* exportable, EGLImageKHR image)
{
    auto* clientBundle = reinterpret_cast<ClientBundleEGL*>
        (wpe_view_backend_exportable_fdo_get_client_bundle(exportable));

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

}
