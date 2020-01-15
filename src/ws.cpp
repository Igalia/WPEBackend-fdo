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

#include "ws.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglmesaext.h>
#include "linux-dmabuf/linux-dmabuf.h"
#include "bridge/wpe-bridge-server-protocol.h"
#include <cassert>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#ifndef EGL_WL_bind_wayland_display
#define EGL_WAYLAND_BUFFER_WL 0x31D5
typedef EGLBoolean (EGLAPIENTRYP PFNEGLBINDWAYLANDDISPLAYWL) (EGLDisplay dpy, struct wl_display *display);
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
static PFNEGLCREATEIMAGEKHRPROC s_eglCreateImageKHR;
static PFNEGLDESTROYIMAGEKHRPROC s_eglDestroyImageKHR;
static PFNEGLQUERYDMABUFFORMATSEXTPROC s_eglQueryDmaBufFormatsEXT;
static PFNEGLQUERYDMABUFMODIFIERSEXTPROC s_eglQueryDmaBufModifiersEXT;

namespace WS {

struct ServerSource {
    static GSourceFuncs s_sourceFuncs;

    GSource source;
    GPollFD pfd;
    struct wl_display* display;
};

GSourceFuncs ServerSource::s_sourceFuncs = {
    // prepare
    [](GSource* base, gint* timeout) -> gboolean
    {
        auto& source = *reinterpret_cast<ServerSource*>(base);
        *timeout = -1;
        wl_display_flush_clients(source.display);
        return FALSE;
    },
    // check
    [](GSource* base) -> gboolean
    {
        auto& source = *reinterpret_cast<ServerSource*>(base);
        return !!source.pfd.revents;
    },
    // dispatch
    [](GSource* base, GSourceFunc, gpointer) -> gboolean
    {
        auto& source = *reinterpret_cast<ServerSource*>(base);

        if (source.pfd.revents & G_IO_IN) {
            struct wl_event_loop* eventLoop = wl_display_get_event_loop(source.display);
            wl_event_loop_dispatch(eventLoop, -1);
            wl_display_flush_clients(source.display);
        }

        if (source.pfd.revents & (G_IO_ERR | G_IO_HUP))
            return FALSE;

        source.pfd.revents = 0;
        return TRUE;
    },
    nullptr, // finalize
    nullptr, // closure_callback
    nullptr, // closure_marshall
};

struct Surface {
    uint32_t id { 0 };
    struct wl_client* client { nullptr };

    ExportableClient* exportableClient { nullptr };

    struct wl_resource* bufferResource { nullptr };
    const struct linux_dmabuf_buffer* dmabufBuffer { nullptr };
    struct wl_shm_buffer* shmBuffer { nullptr };
};

static const struct wl_surface_interface s_surfaceInterface = {
    // destroy
    [](struct wl_client*, struct wl_resource*) { },
    // attach
    [](struct wl_client*, struct wl_resource* surfaceResource, struct wl_resource* bufferResource, int32_t, int32_t)
    {
        auto& surface = *static_cast<Surface*>(wl_resource_get_user_data(surfaceResource));

        surface.dmabufBuffer = Instance::singleton().getDmaBufBuffer(bufferResource);
        surface.shmBuffer = wl_shm_buffer_get(bufferResource);

        if (surface.bufferResource)
            wl_buffer_send_release(surface.bufferResource);
        surface.bufferResource = bufferResource;
    },
    // damage
    [](struct wl_client*, struct wl_resource*, int32_t, int32_t, int32_t, int32_t) { },
    // frame
    [](struct wl_client* client, struct wl_resource* surfaceResource, uint32_t callback)
    {
        auto& surface = *static_cast<Surface*>(wl_resource_get_user_data(surfaceResource));
        if (!surface.exportableClient)
            return;

        struct wl_resource* callbackResource = wl_resource_create(client, &wl_callback_interface, 1, callback);
        if (!callbackResource) {
            wl_resource_post_no_memory(surfaceResource);
            return;
        }

        wl_resource_set_implementation(callbackResource, nullptr, nullptr, nullptr);
        surface.exportableClient->frameCallback(callbackResource);
    },
    // set_opaque_region
    [](struct wl_client*, struct wl_resource*, struct wl_resource*) { },
    // set_input_region
    [](struct wl_client*, struct wl_resource*, struct wl_resource*) { },
    // commit
    [](struct wl_client*, struct wl_resource* surfaceResource)
    {
        auto& surface = *static_cast<Surface*>(wl_resource_get_user_data(surfaceResource));
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
    },
    // set_buffer_transform
    [](struct wl_client*, struct wl_resource*, int32_t) { },
    // set_buffer_scale
    [](struct wl_client*, struct wl_resource*, int32_t) { },
#if (WAYLAND_VERSION_MAJOR > 1) || (WAYLAND_VERSION_MAJOR == 1 && WAYLAND_VERSION_MINOR >= 10)
    // damage_buffer
    [](struct wl_client*, struct wl_resource*, int32_t, int32_t, int32_t, int32_t) { },
#endif
};

static const struct wl_compositor_interface s_compositorInterface = {
    // create_surface
    [](struct wl_client* client, struct wl_resource* resource, uint32_t id)
    {
        struct wl_resource* surfaceResource = wl_resource_create(client, &wl_surface_interface,
            wl_resource_get_version(resource), id);
        if (!surfaceResource) {
            wl_resource_post_no_memory(resource);
            return;
        }

        auto* surface = new Surface;
        surface->client = client;
        surface->id = id;
        wl_resource_set_implementation(surfaceResource, &s_surfaceInterface, surface,
            [](struct wl_resource* resource)
            {
                auto* surface = static_cast<Surface*>(wl_resource_get_user_data(resource));
                delete surface;
            });
    },
    // create_region
    [](struct wl_client*, struct wl_resource*, uint32_t) { },
};

static const struct wpe_bridge_interface s_wpeBridgeInterface = {
    // connect
    [](struct wl_client*, struct wl_resource* resource, struct wl_resource* surfaceResource)
    {
        auto* surface = static_cast<Surface*>(wl_resource_get_user_data(surfaceResource));
        if (!surface)
            return;

        static uint32_t bridgeID = 0;
        ++bridgeID;
        wpe_bridge_send_connected(resource, bridgeID);
        Instance::singleton().registerSurface(bridgeID, surface);
    },
};

Instance& Instance::singleton()
{
    static Instance* s_singleton;
    if (!s_singleton)
        s_singleton = new Instance;
    return *s_singleton;
}

Instance::Instance()
    : m_display(wl_display_create())
    , m_source(g_source_new(&ServerSource::s_sourceFuncs, sizeof(ServerSource)))
    , m_eglDisplay(EGL_NO_DISPLAY)
{
    m_compositor = wl_global_create(m_display, &wl_compositor_interface, 3, this,
        [](struct wl_client* client, void*, uint32_t version, uint32_t id)
        {
            struct wl_resource* resource = wl_resource_create(client, &wl_compositor_interface, version, id);
            if (!resource) {
                wl_client_post_no_memory(client);
                return;
            }

            wl_resource_set_implementation(resource, &s_compositorInterface, nullptr, nullptr);
        });
    m_wpeBridge = wl_global_create(m_display, &wpe_bridge_interface, 1, this,
        [](struct wl_client* client, void*, uint32_t version, uint32_t id)
        {
            struct wl_resource* resource = wl_resource_create(client, &wpe_bridge_interface, version, id);
            if (!resource) {
                wl_client_post_no_memory(client);
                return;
            }

            wl_resource_set_implementation(resource, &s_wpeBridgeInterface, nullptr, nullptr);
        });

    wl_list_init(&m_dmabufBuffers);

    auto& source = *reinterpret_cast<ServerSource*>(m_source);

    struct wl_event_loop* eventLoop = wl_display_get_event_loop(m_display);
    source.pfd.fd = wl_event_loop_get_fd(eventLoop);
    source.pfd.events = G_IO_IN | G_IO_ERR | G_IO_HUP;
    source.pfd.revents = 0;
    source.display = m_display;

    g_source_add_poll(m_source, &source.pfd);
    g_source_set_name(m_source, "WPEBackend-fdo::Host");
    g_source_set_can_recurse(m_source, TRUE);
    g_source_attach(m_source, g_main_context_get_thread_default());
}

Instance::~Instance()
{
    if (m_source) {
        g_source_destroy(m_source);
        g_source_unref(m_source);
    }

    if (m_compositor)
        wl_global_destroy(m_compositor);

    if (m_wpeBridge)
        wl_global_destroy(m_wpeBridge);

    if (m_linuxDmabuf) {
        struct linux_dmabuf_buffer *buffer;
        struct linux_dmabuf_buffer *tmp;
        wl_list_for_each_safe(buffer, tmp, &m_dmabufBuffers, link) {
            assert(buffer);

            wl_list_remove(&buffer->link);
            linux_dmabuf_buffer_destroy(buffer);
        }
        wl_global_destroy(m_linuxDmabuf);
    }

    if (m_display)
        wl_display_destroy(m_display);
}

static bool isEGLExtensionSupported(const char* extensionList, const char* extension)
{
    if (!extensionList)
        return false;

    int extensionLen = strlen(extension);
    const char* extensionListPtr = extensionList;
    while ((extensionListPtr = strstr(extensionListPtr, extension))) {
        if (extensionListPtr[extensionLen] == ' ' || extensionListPtr[extensionLen] == '\0')
            return true;
	extensionListPtr += extensionLen;
    }
    return false;
}

bool Instance::initialize(EGLDisplay eglDisplay)
{
    if (m_eglDisplay == eglDisplay)
        return true;

    if (m_eglDisplay != EGL_NO_DISPLAY) {
        g_warning("Multiple EGL displays are not supported.\n");
        return false;
    }

    // wl_display_init_shm() returns `0` on success.
    if (wl_display_init_shm(m_display) != 0)
        return false;

    const char* extensions = eglQueryString(eglDisplay, EGL_EXTENSIONS);
    if (isEGLExtensionSupported(extensions, "EGL_WL_bind_wayland_display")) {
        s_eglBindWaylandDisplayWL = reinterpret_cast<PFNEGLBINDWAYLANDDISPLAYWL>(eglGetProcAddress("eglBindWaylandDisplayWL"));
        assert(s_eglBindWaylandDisplayWL);
        s_eglQueryWaylandBufferWL = reinterpret_cast<PFNEGLQUERYWAYLANDBUFFERWL>(eglGetProcAddress("eglQueryWaylandBufferWL"));
        assert(s_eglQueryWaylandBufferWL);
    }

    if (isEGLExtensionSupported(extensions, "EGL_KHR_image_base")) {
        s_eglCreateImageKHR = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
        assert(s_eglCreateImageKHR);
        s_eglDestroyImageKHR = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));
        assert(s_eglDestroyImageKHR);
    }

    if (s_eglBindWaylandDisplayWL && s_eglQueryWaylandBufferWL) {
        if (!s_eglCreateImageKHR || !s_eglDestroyImageKHR)
            return false;
        if (!s_eglBindWaylandDisplayWL(eglDisplay, m_display))
            return false;
    }

    m_eglDisplay = eglDisplay;

    /* Initialize Linux dmabuf subsystem. */
    if (isEGLExtensionSupported(extensions, "EGL_EXT_image_dma_buf_import")
        && isEGLExtensionSupported(extensions, "EGL_EXT_image_dma_buf_import_modifiers")) {
        s_eglQueryDmaBufFormatsEXT = reinterpret_cast<PFNEGLQUERYDMABUFFORMATSEXTPROC>(eglGetProcAddress("eglQueryDmaBufFormatsEXT"));
        assert(s_eglQueryDmaBufFormatsEXT);
        s_eglQueryDmaBufModifiersEXT = reinterpret_cast<PFNEGLQUERYDMABUFMODIFIERSEXTPROC>(eglGetProcAddress("eglQueryDmaBufModifiersEXT"));
        assert(s_eglQueryDmaBufModifiersEXT);
    }

    if (s_eglQueryDmaBufFormatsEXT && s_eglQueryDmaBufModifiersEXT) {
        if (m_linuxDmabuf)
            assert(!"Linux-dmabuf has already been initialized");
        m_linuxDmabuf = linux_dmabuf_setup(m_display);
    }

    return true;
}

int Instance::createClient()
{
    if (m_eglDisplay == EGL_NO_DISPLAY)
        return -1;

    int pair[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, pair) < 0)
        return -1;

    int clientFd = dup(pair[1]);
    close(pair[1]);

    wl_client_create(m_display, pair[0]);
    return clientFd;
}

void Instance::registerSurface(uint32_t id, Surface* surface)
{
    m_viewBackendMap.insert({ id, surface });
}

EGLImageKHR Instance::createImage(struct wl_resource* resourceBuffer)
{
    if (m_eglDisplay == EGL_NO_DISPLAY)
        return EGL_NO_IMAGE_KHR;
    return s_eglCreateImageKHR(m_eglDisplay, EGL_NO_CONTEXT, EGL_WAYLAND_BUFFER_WL, resourceBuffer, nullptr);
}

EGLImageKHR Instance::createImage(const struct linux_dmabuf_buffer* dmabufBuffer)
{
    if (!m_linuxDmabuf)
        return EGL_NO_IMAGE_KHR;

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

    return s_eglCreateImageKHR(m_eglDisplay, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attribs);
}

void Instance::destroyImage(EGLImageKHR image)
{
    if (m_eglDisplay == EGL_NO_DISPLAY)
        return;
    s_eglDestroyImageKHR(m_eglDisplay, image);
}

void Instance::queryBufferSize(struct wl_resource* bufferResource, uint32_t* width, uint32_t* height)
{
    if (width) {
        int w;
        s_eglQueryWaylandBufferWL(m_eglDisplay, bufferResource, EGL_WIDTH, &w);
        *width = w;
    }

    if (height) {
        int h;
        s_eglQueryWaylandBufferWL(m_eglDisplay, bufferResource, EGL_HEIGHT, &h);
        *height = h;
    }
}

void Instance::importDmaBufBuffer(struct linux_dmabuf_buffer* buffer)
{
    if (!m_linuxDmabuf)
        return;
    wl_list_insert(&m_dmabufBuffers, &buffer->link);
}

const struct linux_dmabuf_buffer* Instance::getDmaBufBuffer(struct wl_resource* bufferResource) const
{
    if (!m_linuxDmabuf || !bufferResource)
        return nullptr;

    if (!linux_dmabuf_buffer_implements_resource(bufferResource))
        return nullptr;

    struct linux_dmabuf_buffer* buffer;
    wl_list_for_each(buffer, &m_dmabufBuffers, link) {
        if (buffer->buffer_resource == bufferResource)
            return buffer;
    }

    return NULL;
}

void Instance::foreachDmaBufModifier(std::function<void (int format, uint64_t modifier)> callback)
{
    if (m_eglDisplay == EGL_NO_DISPLAY)
        return;

    EGLint formats[128];
    EGLint numFormats;
    if (!s_eglQueryDmaBufFormatsEXT(m_eglDisplay, 128, formats, &numFormats))
        assert(!"Linux-dmabuf: Failed to query formats");

    for (int i = 0; i < numFormats; i++) {
        uint64_t modifiers[64];
        EGLint numModifiers;
        if (!s_eglQueryDmaBufModifiersEXT(m_eglDisplay, formats[i], 64, modifiers, NULL, &numModifiers))
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

struct wl_client* Instance::registerViewBackend(uint32_t surfaceId, ExportableClient& exportableClient)
{
    auto it = m_viewBackendMap.find(surfaceId);
    if (it == m_viewBackendMap.end())
        g_error("Instance::registerViewBackend(): " "Cannot find surface %" PRIu32 " in view backend map.", surfaceId);

    it->second->exportableClient = &exportableClient;
    return it->second->client;
}

void Instance::unregisterViewBackend(uint32_t surfaceId)
{
    auto it = m_viewBackendMap.find(surfaceId);
    if (it != m_viewBackendMap.end()) {
        it->second->exportableClient = nullptr;
        m_viewBackendMap.erase(it);
    }
}

} // namespace WS
