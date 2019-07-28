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
#include <wayland-egl.h>

#include <wpe/wpe-egl.h>
#include "interfaces.h"
#include "ws-client.h"

namespace {

class Backend final : public WS::BaseBackend {
public:
    Backend(int hostFD)
        : WS::BaseBackend(hostFD)
    { }
    ~Backend() = default;

    using WS::BaseBackend::display;
};

class Target final : public WS::BaseTarget, public WS::BaseTarget::Impl {
public:
    Target(struct wpe_renderer_backend_egl_target* target, int hostFD)
        : WS::BaseTarget(hostFD, *this)
        , m_target(target)
    { }

    ~Target()
    {
        g_clear_pointer(&m_egl.window, wl_egl_window_destroy);

        m_target = nullptr;
    }

    void initialize(Backend& backend, uint32_t width, uint32_t height)
    {
        WS::BaseTarget::initialize(backend.display());
        m_egl.window = wl_egl_window_create(surface(), width, height);
    }

    using WS::BaseTarget::requestFrame;

    struct wl_egl_window* window() const { return m_egl.window; }

private:
    // WS::BaseTarget::Impl
    void dispatchFrameComplete() override
    {
        wpe_renderer_backend_egl_target_dispatch_frame_complete(m_target);
    }

    struct wpe_renderer_backend_egl_target* m_target { nullptr };

    struct {
        struct wl_egl_window* window { nullptr };
    } m_egl;
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
        return backend.display();
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
        return target.window();
    },
    // resize
    [](void* data, uint32_t width, uint32_t height)
    {
        wl_egl_window_resize(static_cast<Target*>(data)->window(), width, height, 0, 0);
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
