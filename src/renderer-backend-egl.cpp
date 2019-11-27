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

// Should be included early to force through the Wayland EGL platform
//#include <wayland-egl.h>

#include <gbm.h>
#include <wpe/wpe-egl.h>

#include "interfaces.h"
#include "protocols/wpe-gbm-client-protocol.h"
#include "ws-client.h"
#include <cassert>
#include <cstring>
#include <fcntl.h>
#include <xf86drm.h>

namespace {

class Backend final : public WS::BaseBackend {
public:
    Backend(int hostFD)
        : WS::BaseBackend(hostFD)
    {
        struct wl_display* display = this->display();
        m_wl.eventQueue = wl_display_create_queue(display);

        struct wl_registry* registry = wl_display_get_registry(display);
        wl_proxy_set_queue(reinterpret_cast<struct wl_proxy*>(registry), m_wl.eventQueue);
        wl_registry_add_listener(registry, &s_registryListener, this);
        wl_display_roundtrip_queue(display, m_wl.eventQueue);
        wl_registry_destroy(registry);

        wl_proxy_set_queue(reinterpret_cast<struct wl_proxy*>(m_wl.wpeGBM), m_wl.eventQueue);
        wpe_gbm_add_listener(m_wl.wpeGBM, &s_wpeGBMListener, this);
        wpe_gbm_request_device(m_wl.wpeGBM);
        wl_display_roundtrip_queue(display, m_wl.eventQueue);
    }
    ~Backend() = default;

    using WS::BaseBackend::display;
    struct gbm_device* gbmDevice() const { return m_gbmDevice; }

private:
    static const struct wl_registry_listener s_registryListener;
    static const struct wpe_gbm_listener s_wpeGBMListener;

    struct gbm_device* m_gbmDevice;

    struct {
        struct wl_event_queue* eventQueue;
        struct wpe_gbm* wpeGBM;
    } m_wl;
};

const struct wl_registry_listener Backend::s_registryListener = {
    // global
    [](void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t)
    {
        auto& backend = *reinterpret_cast<Backend*>(data);
        if (!std::strcmp(interface, "wpe_gbm"))
            backend.m_wl.wpeGBM = static_cast<struct wpe_gbm*>(wl_registry_bind(registry, name, &wpe_gbm_interface, 1));
    },
    // global_remove
    [](void*, struct wl_registry*, uint32_t) { },
};

const struct wpe_gbm_listener Backend::s_wpeGBMListener = {
    // device_fd
	[](void* data, struct wpe_gbm* wpe_gbm, int fd)
    {
        auto& backend = *reinterpret_cast<Backend*>(data);
        int ffd = open("/dev/dri/card0", DRM_RDWR | DRM_CLOEXEC);

        drm_magic_t magic;
        int ret = drmGetMagic(ffd, &magic);
        fprintf(stderr, "s_wpeGBMListener::dev(): got magic %u, ret %d\n", magic, ret);

        wpe_gbm_authenticate(backend.m_wl.wpeGBM, magic);
        wl_display_roundtrip(backend.display());

        drmVersionPtr v = drmGetVersion(ffd);
        fprintf(stderr, "\t%d.%d.%d, name %s date %s desc %s\n",
            v->version_major, v->version_minor, v->version_patchlevel,
            v->name, v->date, v->desc);

        backend.m_gbmDevice = gbm_create_device(ffd);
        fprintf(stderr, "s_wpeGBMListener::device_fd() %d dev %p its fd %d\n",
            fd, backend.m_gbmDevice, gbm_device_get_fd(backend.m_gbmDevice));
    },
};

class Target final : public WS::BaseTarget, public WS::BaseTarget::Impl {
public:
    Target(struct wpe_renderer_backend_egl_target* target, int hostFD)
        : WS::BaseTarget(hostFD, *this)
        , m_target(target)
    {
        wl_list_init(&m_cachedBuffers);
    }

    ~Target()
    {
        m_target = nullptr;
    }

    void initialize(Backend& backend, uint32_t width, uint32_t height)
    {
        WS::BaseTarget::initialize(backend.display());

        {
            struct wl_display* display = backend.display();
            struct wl_registry* registry = wl_display_get_registry(display);
            wl_proxy_set_queue(reinterpret_cast<struct wl_proxy*>(registry), m_wl.eventQueue);
            wl_registry_add_listener(registry, &s_registryListener, this);
            wl_display_roundtrip_queue(display, m_wl.eventQueue);
            wl_registry_destroy(registry);
        }

        m_gbm.surface = gbm_surface_create(backend.gbmDevice(), width, height,
            GBM_FORMAT_RGB565, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
        fprintf(stderr, "surface: %p\n", m_gbm.surface);
    }

    using WS::BaseTarget::requestFrame;

    void frameRendered()
    {
        //fprintf(stderr, "frameRendered(): locking ...\n");
        struct gbm_bo* bo = gbm_surface_lock_front_buffer(m_gbm.surface);
        //fprintf(stderr, "\tlocked %p\n", bo);
        assert(bo);

        uint32_t handle = gbm_bo_get_handle(bo).u32;
        auto* cachedBuffer = static_cast<CachedBuffer*>(gbm_bo_get_user_data(bo));
        if (!cachedBuffer) {
            struct wl_buffer* buffer = wpe_gbm_create_buffer(m_wpeGBM, gbm_bo_get_fd(bo),
                gbm_bo_get_width(bo), gbm_bo_get_height(bo),
                gbm_bo_get_stride(bo), gbm_bo_get_format(bo));
            wl_proxy_set_queue(reinterpret_cast<struct wl_proxy*>(buffer), m_wl.eventQueue);
            wl_buffer_add_listener(buffer, &s_bufferListener, this);

            cachedBuffer = new CachedBuffer;
            wl_list_insert(&m_cachedBuffers, &cachedBuffer->link);
            cachedBuffer->handle = handle;
            cachedBuffer->buffer = buffer;
            cachedBuffer->bo = bo;

            gbm_bo_set_user_data(bo, cachedBuffer, nullptr);
        }

        //fprintf(stderr, "frameRendered(): bo %p handle %u, cachedBuffer %p, buffer %p\n",
        //    bo, handle, cachedBuffer, cachedBuffer->buffer);
        assert(handle == cachedBuffer->handle);

        wl_surface_attach(surface(), cachedBuffer->buffer, 0, 0);
        wl_surface_commit(surface());

        //fprintf(stderr, "\n");
    }

    struct gbm_surface* gbmSurface() const { return m_gbm.surface; }

private:
    static const struct wl_registry_listener s_registryListener;
    static const struct wl_buffer_listener s_bufferListener;

    struct CachedBuffer {
        struct wl_list link;

        uint32_t handle;
        struct wl_buffer* buffer;
        struct gbm_bo* bo;
    };
    struct wl_list m_cachedBuffers;

    // WS::BaseTarget::Impl
    void dispatchFrameComplete() override
    {
        wpe_renderer_backend_egl_target_dispatch_frame_complete(m_target);
    }

    struct wpe_renderer_backend_egl_target* m_target { nullptr };

    struct wpe_gbm* m_wpeGBM { nullptr };

    struct {
        struct gbm_surface* surface { nullptr };
    } m_gbm;
};

const struct wl_registry_listener Target::s_registryListener = {
    // global
    [](void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t)
    {
        auto& target = *reinterpret_cast<Target*>(data);

        if (!std::strcmp(interface, "wpe_gbm"))
            target.m_wpeGBM = static_cast<struct wpe_gbm*>(wl_registry_bind(registry, name, &wpe_gbm_interface, 1));
    },
    // global_remove
    [](void*, struct wl_registry*, uint32_t) { },
};

const struct wl_buffer_listener Target::s_bufferListener = {
    // release
    [](void* data, struct wl_buffer* buffer)
    {
        auto& target = *reinterpret_cast<Target*>(data);

        CachedBuffer* cachedBuffer = nullptr;
        wl_list_for_each(cachedBuffer, &target.m_cachedBuffers, link) {
            if (cachedBuffer->buffer == buffer)
                break;
        }
        //fprintf(stderr, "Target::s_bufferListener: buffer %p cached obj %p\n", buffer, cachedBuffer);
        if (!cachedBuffer)
            return;

        //fprintf(stderr, "Target::s_bufferListener::release(), buffer %p, cachedBuffer %p, bo %p handle %u\n",
        //    buffer, cachedBuffer, cachedBuffer->bo, cachedBuffer->handle);
        //fprintf(stderr, "\treleasing bo %p on surface %p\n", cachedBuffer->bo, target.m_gbm.surface);
        gbm_surface_release_buffer(target.m_gbm.surface, cachedBuffer->bo);
    },
};

} // namespace

struct wpe_renderer_backend_egl_interface fdo_renderer_backend_egl = {
    // create
    [](int host_fd) -> void*
    {
        return new Backend(host_fd);
    },
    // destroy
    [](void* data)
    {
        auto* backend = reinterpret_cast<Backend*>(data);
        delete backend;
    },
    // get_native_display
    [](void* data) -> EGLNativeDisplayType
    {
        auto& backend = *reinterpret_cast<Backend*>(data);
        return EGLNativeDisplayType(backend.gbmDevice());
    },
};

struct wpe_renderer_backend_egl_target_interface fdo_renderer_backend_egl_target = {
    // create
    [](struct wpe_renderer_backend_egl_target* target, int host_fd) -> void*
    {
        return new Target(target, host_fd);
    },
    // destroy
    [](void* data)
    {
        auto* target = reinterpret_cast<Target*>(data);
        delete target;
    },
    // initialize
    [](void* data, void* backend_data, uint32_t width, uint32_t height)
    {
        auto& target = *reinterpret_cast<Target*>(data);
        auto& backend = *reinterpret_cast<Backend*>(backend_data);
        target.initialize(backend, width, height);
    },
    // get_native_window
    [](void* data) -> EGLNativeWindowType
    {
        auto& target = *reinterpret_cast<Target*>(data);
        return target.gbmSurface();
    },
    // resize
    [](void* data, uint32_t width, uint32_t height)
    {
    },
    // frame_will_render
    [](void* data)
    {
        auto& target = *reinterpret_cast<Target*>(data);
        target.requestFrame();
    },
    // frame_rendered
    [](void* data)
    {
        auto& target = *reinterpret_cast<Target*>(data);
        target.frameRendered();
    },
};

struct wpe_renderer_backend_egl_offscreen_target_interface fdo_renderer_backend_egl_offscreen_target = {
    // create
    []() -> void*
    {
        return nullptr;
    },
    // destroy
    [](void* data)
    {
    },
    // initialize
    [](void* data, void* backend_data)
    {
    },
    // get_native_window
    [](void* data) -> EGLNativeWindowType
    {
        return nullptr;
    },
};
