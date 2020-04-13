/*
 * Copyright (C) 2020 Igalia S.L.
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

#include "ws-egl.h"

#include "linux-dmabuf/linux-dmabuf.h"
#include <epoxy/egl.h>
#include <cassert>
#include <cstring>

#ifndef EGL_WL_bind_wayland_display
#define EGL_WAYLAND_BUFFER_WL 0x31D5
typedef EGLBoolean (EGLAPIENTRYP PFNEGLBINDWAYLANDDISPLAYWL) (EGLDisplay dpy, struct wl_display *display);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYWAYLANDBUFFERWL) (EGLDisplay dpy, struct wl_resource *buffer, EGLint attribute, EGLint *value);
#endif

#ifndef EGL_EXT_image_dma_buf_import_modifiers
typedef EGLBoolean (* PFNEGLQUERYDMABUFFORMATSEXTPROC) (EGLDisplay dpy, EGLint max_formats, EGLint *formats, EGLint *num_formats);
typedef EGLBoolean (* PFNEGLQUERYDMABUFMODIFIERSEXTPROC) (EGLDisplay dpy, EGLint format, EGLint max_modifiers, EGLuint64KHR *modifiers, EGLBoolean *external_only, EGLint *num_modifiers);
#endif /* EGL_EXT_image_dma_buf_import_modifiers */

#ifndef EGL_EXT_image_dma_buf_import
#define EGL_LINUX_DMA_BUF_EXT             0x3270
#define EGL_DMA_BUF_PLANE0_FD_EXT         0x3272
#define EGL_DMA_BUF_PLANE0_OFFSET_EXT     0x3273
#define EGL_DMA_BUF_PLANE0_PITCH_EXT      0x3274
#define EGL_DMA_BUF_PLANE1_FD_EXT         0x3275
#define EGL_DMA_BUF_PLANE1_OFFSET_EXT     0x3276
#define EGL_DMA_BUF_PLANE1_PITCH_EXT      0x3277
#define EGL_DMA_BUF_PLANE2_FD_EXT         0x3278
#define EGL_DMA_BUF_PLANE2_OFFSET_EXT     0x3279
#define EGL_DMA_BUF_PLANE2_PITCH_EXT      0x327A
#endif /* EGL_EXT_image_dma_buf_import */

#ifndef EGL_EXT_image_dma_buf_import_modifiers
#define EGL_DMA_BUF_PLANE3_FD_EXT         0x3440
#define EGL_DMA_BUF_PLANE3_OFFSET_EXT     0x3441
#define EGL_DMA_BUF_PLANE3_PITCH_EXT      0x3442
#define EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT 0x3443
#define EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT 0x3444
#define EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT 0x3445
#define EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT 0x3446
#define EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT 0x3447
#define EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT 0x3448
#define EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT 0x3449
#define EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT 0x344A
#endif /* EGL_EXT_image_dma_buf_import_modifiers */

static PFNEGLBINDWAYLANDDISPLAYWL s_eglBindWaylandDisplayWL;
static PFNEGLQUERYWAYLANDBUFFERWL s_eglQueryWaylandBufferWL;
static PFNEGLQUERYDMABUFFORMATSEXTPROC s_eglQueryDmaBufFormatsEXT;
static PFNEGLQUERYDMABUFMODIFIERSEXTPROC s_eglQueryDmaBufModifiersEXT;

namespace WS {

ImplEGL::ImplEGL()
{
    m_egl.display = EGL_NO_DISPLAY;

    wl_list_init(&m_dmabuf.buffers);
}

ImplEGL::~ImplEGL()
{
    if (m_dmabuf.global) {
        struct linux_dmabuf_buffer *buffer;
        struct linux_dmabuf_buffer *tmp;
        wl_list_for_each_safe(buffer, tmp, &m_dmabuf.buffers, link) {
            assert(buffer);

            wl_list_remove(&buffer->link);
            linux_dmabuf_buffer_destroy(buffer);
        }
        wl_global_destroy(m_dmabuf.global);
    }
}

void ImplEGL::surfaceAttach(Surface& surface, struct wl_resource* bufferResource)
{
    surface.dmabufBuffer = getDmaBufBuffer(bufferResource);
    surface.shmBuffer = wl_shm_buffer_get(bufferResource);

    if (surface.bufferResource)
        wl_buffer_send_release(surface.bufferResource);
    surface.bufferResource = bufferResource;
}

void ImplEGL::surfaceCommit(Surface& surface)
{
    if (!surface.exportableClient)
        return;

    struct wl_resource* bufferResource = surface.bufferResource;
    surface.bufferResource = nullptr;

    if (surface.dmabufBuffer)
        surface.exportableClient->exportLinuxDmabuf(surface.dmabufBuffer);
    else if (surface.shmBuffer)
        surface.exportableClient->exportShmBuffer(bufferResource, surface.shmBuffer);
    else
        surface.exportableClient->exportBufferResource(bufferResource);
}

bool ImplEGL::initialize(EGLDisplay eglDisplay)
{
    if (m_egl.display == eglDisplay)
        return true;

    if (m_egl.display != EGL_NO_DISPLAY) {
        g_warning("Multiple EGL displays are not supported.\n");
        return false;
    }

    m_egl.WL_bind_wayland_display = epoxy_has_egl_extension(m_egl.display, "EGL_WL_bind_wayland_display");
    m_egl.KHR_image_base = epoxy_has_egl_extension(m_egl.display, "EGL_KHR_image_base");
    m_egl.EXT_image_dma_buf_import = epoxy_has_egl_extension(m_egl.display, "EGL_EXT_image_dma_buf_import");
    m_egl.EXT_image_dma_buf_import_modifiers = epoxy_has_egl_extension(m_egl.display, "EGL_EXT_image_dma_buf_import_modifiers");

    // wl_display_init_shm() returns `0` on success.
    if (wl_display_init_shm(display()) != 0)
        return false;

    if (m_egl.WL_bind_wayland_display) {
        s_eglBindWaylandDisplayWL = reinterpret_cast<PFNEGLBINDWAYLANDDISPLAYWL>(eglGetProcAddress("eglBindWaylandDisplayWL"));
        assert(s_eglBindWaylandDisplayWL);
        s_eglQueryWaylandBufferWL = reinterpret_cast<PFNEGLQUERYWAYLANDBUFFERWL>(eglGetProcAddress("eglQueryWaylandBufferWL"));
        assert(s_eglQueryWaylandBufferWL);
    }

    if (s_eglBindWaylandDisplayWL && s_eglQueryWaylandBufferWL) {
        // Bail if EGL_KHR_image_base is not present, which is needed to create EGLImages from the received wl_resources.
        // TODO: this is not really accurate -- we can still export raw wl_resources without having to spawn EGLImages.
        if (!m_egl.KHR_image_base)
            return false;

        if (!s_eglBindWaylandDisplayWL(eglDisplay, display()))
            return false;
    }

    m_initialized = true;
    m_egl.display = eglDisplay;

    /* Initialize Linux dmabuf subsystem. */
    if (m_egl.EXT_image_dma_buf_import && m_egl.EXT_image_dma_buf_import_modifiers) {
        s_eglQueryDmaBufFormatsEXT = reinterpret_cast<PFNEGLQUERYDMABUFFORMATSEXTPROC>(eglGetProcAddress("eglQueryDmaBufFormatsEXT"));
        assert(s_eglQueryDmaBufFormatsEXT);
        s_eglQueryDmaBufModifiersEXT = reinterpret_cast<PFNEGLQUERYDMABUFMODIFIERSEXTPROC>(eglGetProcAddress("eglQueryDmaBufModifiersEXT"));
        assert(s_eglQueryDmaBufModifiersEXT);
    }

    if (s_eglQueryDmaBufFormatsEXT && s_eglQueryDmaBufModifiersEXT) {
        if (m_dmabuf.global)
            assert(!"Linux-dmabuf has already been initialized");
        m_dmabuf.global = linux_dmabuf_setup(display());
    }

    return true;
}

EGLImageKHR ImplEGL::createImage(struct wl_resource* resourceBuffer)
{
    if (m_egl.display == EGL_NO_DISPLAY)
        return EGL_NO_IMAGE_KHR;

    assert(m_egl.KHR_image_base);
    return eglCreateImageKHR(m_egl.display, EGL_NO_CONTEXT, EGL_WAYLAND_BUFFER_WL, resourceBuffer, nullptr);
}

EGLImageKHR ImplEGL::createImage(const struct linux_dmabuf_buffer* dmabufBuffer)
{
    static const struct {
        EGLint fd;
        EGLint offset;
        EGLint pitch;
        EGLint modifierLo;
        EGLint modifierHi;
    } planeEnums[4] = {
        {EGL_DMA_BUF_PLANE0_FD_EXT,
         EGL_DMA_BUF_PLANE0_OFFSET_EXT,
         EGL_DMA_BUF_PLANE0_PITCH_EXT,
         EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT,
         EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT},
        {EGL_DMA_BUF_PLANE1_FD_EXT,
         EGL_DMA_BUF_PLANE1_OFFSET_EXT,
         EGL_DMA_BUF_PLANE1_PITCH_EXT,
         EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
         EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT},
        {EGL_DMA_BUF_PLANE2_FD_EXT,
         EGL_DMA_BUF_PLANE2_OFFSET_EXT,
         EGL_DMA_BUF_PLANE2_PITCH_EXT,
         EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT,
         EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT},
        {EGL_DMA_BUF_PLANE3_FD_EXT,
         EGL_DMA_BUF_PLANE3_OFFSET_EXT,
         EGL_DMA_BUF_PLANE3_PITCH_EXT,
         EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT,
         EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT},
    };

    EGLint attribs[50];
    int atti = 0;
    attribs[atti++] = EGL_WIDTH;
    attribs[atti++] = dmabufBuffer->attributes.width;
    attribs[atti++] = EGL_HEIGHT;
    attribs[atti++] = dmabufBuffer->attributes.height;
    attribs[atti++] = EGL_LINUX_DRM_FOURCC_EXT;
    attribs[atti++] = dmabufBuffer->attributes.format;

    for (int i = 0; i < dmabufBuffer->attributes.n_planes; i++) {
        attribs[atti++] = planeEnums[i].fd;
        attribs[atti++] = dmabufBuffer->attributes.fd[i];
        attribs[atti++] = planeEnums[i].offset;
        attribs[atti++] = dmabufBuffer->attributes.offset[i];
        attribs[atti++] = planeEnums[i].pitch;
        attribs[atti++] = dmabufBuffer->attributes.stride[i];
        attribs[atti++] = planeEnums[i].modifierLo;
        attribs[atti++] = dmabufBuffer->attributes.modifier[i] & 0xFFFFFFFF;
        attribs[atti++] = planeEnums[i].modifierHi;
        attribs[atti++] = dmabufBuffer->attributes.modifier[i] >> 32;
    }

    attribs[atti++] = EGL_NONE;

    assert(m_egl.KHR_image_base);
    return eglCreateImageKHR(m_egl.display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attribs);
}

void ImplEGL::destroyImage(EGLImageKHR image)
{
    if (m_egl.display == EGL_NO_DISPLAY)
        return;

    assert(m_egl.KHR_image_base);
    eglDestroyImageKHR(m_egl.display, image);
}

void ImplEGL::queryBufferSize(struct wl_resource* bufferResource, uint32_t* width, uint32_t* height)
{
    if (m_egl.display == EGL_NO_DISPLAY) {
        if (width)
            *width = 0;
        if (height)
            *height = 0;
        return;
    }

    if (width) {
        int w;
        s_eglQueryWaylandBufferWL(m_egl.display, bufferResource, EGL_WIDTH, &w);
        *width = w;
    }

    if (height) {
        int h;
        s_eglQueryWaylandBufferWL(m_egl.display, bufferResource, EGL_HEIGHT, &h);
        *height = h;
    }
}

void ImplEGL::importDmaBufBuffer(struct linux_dmabuf_buffer* buffer)
{
    if (!m_dmabuf.global)
        return;
    wl_list_insert(&m_dmabuf.buffers, &buffer->link);
}

const struct linux_dmabuf_buffer* ImplEGL::getDmaBufBuffer(struct wl_resource* bufferResource) const
{
    if (!m_dmabuf.global || !bufferResource)
        return nullptr;

    if (!linux_dmabuf_buffer_implements_resource(bufferResource))
        return nullptr;

    struct linux_dmabuf_buffer* buffer;
    wl_list_for_each(buffer, &m_dmabuf.buffers, link) {
        if (buffer->buffer_resource == bufferResource)
            return buffer;
    }

    return NULL;
}

void ImplEGL::foreachDmaBufModifier(std::function<void (int format, uint64_t modifier)> callback)
{
    if (m_egl.display == EGL_NO_DISPLAY)
        return;

    EGLint formats[128];
    EGLint numFormats;
    if (!s_eglQueryDmaBufFormatsEXT(m_egl.display, 128, formats, &numFormats))
        assert(!"Linux-dmabuf: Failed to query formats");

    for (int i = 0; i < numFormats; i++) {
        uint64_t modifiers[64];
        EGLint numModifiers;
        if (!s_eglQueryDmaBufModifiersEXT(m_egl.display, formats[i], 64, modifiers, NULL, &numModifiers))
            assert(!"Linux-dmabuf: Failed to query modifiers of a format");

        /* Send DRM_FORMAT_MOD_INVALID token when no modifiers are supported
         * for this format.
         */
        if (numModifiers == 0) {
            numModifiers = 1;
            modifiers[0] = DRM_FORMAT_MOD_INVALID;
        }

        for (int j = 0; j < numModifiers; j++)
            callback(formats[i], modifiers[j]);
    }
}

} // namespace WS
