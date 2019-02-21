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

#include "interfaces.h"

#include "bridge/wpe-bridge-client-protocol.h"
#include <cstring>
#include <gio/gio.h>
#include <glib.h>

#include <cstdio>
#include <cstdlib>

namespace {

struct Source {
    static GSourceFuncs s_sourceFuncs;

    GSource source;
    GPollFD pfd;
    struct wl_display* display;
    struct wl_event_queue* queue;
    bool hasError;
};

GSourceFuncs Source::s_sourceFuncs = {
    // prepare
    [](GSource* base, gint* timeout) -> gboolean
    {
        auto& source = *reinterpret_cast<Source*>(base);

        // We take up this opportunity to flush the display object, but
        // otherwise we're not able to determine whether there are any
        // pending dispatches (that would allow us skip the polling)
        // without scheduling a read on the wl_display object.
        *timeout = -1;
        wl_display_flush(source.display);
        return FALSE;
    },
    // check
    [](GSource* base) -> gboolean
    {
        auto& source = *reinterpret_cast<Source*>(base);

        // If error events were read, we note the error and return TRUE,
        // removing the source in the ensuing dispatch callback.
        if (source.pfd.revents & (G_IO_ERR | G_IO_HUP)) {
            source.hasError = true;
            return TRUE;
        }

        // If there are pending dispatches on this queue, we return TRUE
        // to proceed to dispatching ASAP. If there are none, a new read
        // intention is set up for this wl_display object.
        if (wl_display_prepare_read_queue(source.display, source.queue) != 0)
            return TRUE;

        // Only perform the read if input was made available during polling.
        // Error during read is noted and will be handled in the following
        // dispatch callback. If no input is available, the read is canceled.
        // revents value is zeroed out in any case.
        if (source.pfd.revents & G_IO_IN) {
            if (wl_display_read_events(source.display) < 0)
                source.hasError = true;
            source.pfd.revents = 0;
            return TRUE;
        } else {
            wl_display_cancel_read(source.display);
            source.pfd.revents = 0;
            return FALSE;
        }
    },
    // dispatch
    [](GSource* base, GSourceFunc, gpointer) -> gboolean
    {
        auto& source = *reinterpret_cast<Source*>(base);

        // Remove the source if any error was registered.
        if (source.hasError)
            return FALSE;

        // Dispatch any pending events. The source is removed in case of
        // an error occurring during this step.
        if (wl_display_dispatch_queue_pending(source.display, source.queue) < 0)
            return FALSE;
        return TRUE;
    },
    nullptr, // finalize
    nullptr, // closure_callback
    nullptr, // closure_marshall
};

class Backend {
public:
    Backend(int hostFd)
    {
        m_display = wl_display_connect_to_fd(hostFd);
    }

    ~Backend() = default;

    struct wl_display* display() const { return m_display; }

private:
    struct wl_display* m_display;
};

class Target {
public:
    Target(struct wpe_renderer_backend_egl_target* target, int hostFd)
        : m_target(target)
    {
        m_glib.socket = g_socket_new_from_fd(hostFd, nullptr);
        if (m_glib.socket)
            g_socket_set_blocking(m_glib.socket, FALSE);
    }

    ~Target()
    {
        g_clear_pointer(&m_wl.frameCallback, wl_callback_destroy);
        g_clear_pointer(&m_wl.window, wl_egl_window_destroy);
        g_clear_pointer(&m_wl.surface, wl_surface_destroy);

        g_clear_pointer(&m_wl.wpeBridge, wpe_bridge_destroy);
        g_clear_pointer(&m_wl.compositor, wl_compositor_destroy);
        g_clear_pointer(&m_wl.registry, wl_registry_destroy);
        g_clear_pointer(&m_wl.eventQueue, wl_event_queue_destroy);

        g_clear_object(&m_glib.socket);
        if (m_glib.frameSource) {
            g_source_destroy(m_glib.frameSource);
            g_source_unref(m_glib.frameSource);
        }
        if (m_glib.wlSource) {
            g_source_destroy(m_glib.wlSource);
            g_source_unref(m_glib.wlSource);
        }
    }

    void initialize(Backend& backend, uint32_t width, uint32_t height)
    {
        auto* display = backend.display();
        m_wl.eventQueue = wl_display_create_queue(display);

        m_wl.registry = wl_display_get_registry(display);
        wl_proxy_set_queue(reinterpret_cast<struct wl_proxy*>(m_wl.registry), m_wl.eventQueue);
        wl_registry_add_listener(m_wl.registry, &s_registryListener, this);
        wl_display_roundtrip_queue(display, m_wl.eventQueue);

        m_wl.surface = wl_compositor_create_surface(m_wl.compositor);
        wl_proxy_set_queue(reinterpret_cast<struct wl_proxy*>(m_wl.surface), m_wl.eventQueue);
        m_wl.window = wl_egl_window_create(m_wl.surface, width, height);

        m_glib.wlSource = g_source_new(&Source::s_sourceFuncs, sizeof(Source));
        {
            auto& source = *reinterpret_cast<Source*>(m_glib.wlSource);
            source.pfd.fd = wl_display_get_fd(display);
            source.pfd.events = G_IO_IN | G_IO_ERR | G_IO_HUP;
            source.pfd.revents = 0;
            source.display = display;
            source.queue = m_wl.eventQueue;
            source.hasError = false;

            g_source_add_poll(m_glib.wlSource, &source.pfd);
            g_source_set_name(m_glib.wlSource, "WPEBackend-fdo::wayland");
            g_source_set_priority(m_glib.wlSource, -70);
            g_source_set_can_recurse(m_glib.wlSource, TRUE);
            g_source_attach(m_glib.wlSource, g_main_context_get_thread_default());
        }

        m_glib.frameSource = g_source_new(&s_sourceFuncs, sizeof(GSource));
        g_source_set_priority(m_glib.frameSource, -70);
        g_source_set_name(m_glib.frameSource, "WPEBackend-fdo::frame");
        g_source_set_callback(m_glib.frameSource, [](gpointer userData) -> gboolean {
            auto* target = static_cast<Target*>(userData);

            g_clear_pointer(&target->m_wl.frameCallback, wl_callback_destroy);
            target->m_wl.frameCallback = nullptr;

            wpe_renderer_backend_egl_target_dispatch_frame_complete(target->m_target);
            return G_SOURCE_CONTINUE;
        }, this, nullptr);
        g_source_attach(m_glib.frameSource, g_main_context_get_thread_default());

        wpe_bridge_add_listener(m_wl.wpeBridge, &s_bridgeListener, this);
        wpe_bridge_connect(m_wl.wpeBridge, m_wl.surface);
        wl_display_roundtrip_queue(display, m_wl.eventQueue);
    }

    void requestFrame()
    {
        if (m_wl.frameCallback)
            std::abort();

        m_wl.frameCallback = wl_surface_frame(m_wl.surface);
        wl_callback_add_listener(m_wl.frameCallback, &s_callbackListener, this);
    }

    void dispatchFrameComplete()
    {
        g_source_set_ready_time(m_glib.frameSource, 0);
    }

    void bridgeConnected(uint32_t bridgeID)
    {
        uint32_t message[] = { 0x42, bridgeID };
        if (m_glib.socket)
            g_socket_send(m_glib.socket, reinterpret_cast<gchar*>(message), 2 * sizeof(uint32_t), nullptr, nullptr);
    }

    struct wl_egl_window* window() const { return m_wl.window; }

private:
    static const struct wl_registry_listener s_registryListener;
    static const struct wl_callback_listener s_callbackListener;
    static const struct wpe_bridge_listener s_bridgeListener;

    static GSourceFuncs s_sourceFuncs;

    struct wpe_renderer_backend_egl_target* m_target { nullptr };

    struct {
        GSocket* socket { nullptr };
        GSource* wlSource { nullptr };
        GSource* frameSource { nullptr };
    } m_glib;

    struct {
        struct wl_display* displayWrapper { nullptr };
        struct wl_event_queue* eventQueue { nullptr };
        struct wl_registry* registry { nullptr };
        struct wl_compositor* compositor { nullptr };
        struct wpe_bridge* wpeBridge { nullptr };

        struct wl_surface* surface { nullptr };
        struct wl_egl_window* window { nullptr };
        struct wl_callback* frameCallback { nullptr };
    } m_wl;
};

const struct wl_registry_listener Target::s_registryListener = {
    // global
    [](void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t)
    {
        auto& target = *reinterpret_cast<Target*>(data);

        if (!std::strcmp(interface, "wl_compositor"))
            target.m_wl.compositor = static_cast<struct wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, 1));
        if (!std::strcmp(interface, "wpe_bridge"))
            target.m_wl.wpeBridge = static_cast<struct wpe_bridge*>(wl_registry_bind(registry, name, &wpe_bridge_interface, 1));
    },
    // global_remove
    [](void*, struct wl_registry*, uint32_t) { },
};

const struct wl_callback_listener Target::s_callbackListener = {
    // done
    [](void* data, struct wl_callback*, uint32_t time)
    {
        static_cast<Target*>(data)->dispatchFrameComplete();
    },
};

const struct wpe_bridge_listener Target::s_bridgeListener = {
    // connected
    [](void* data, struct wpe_bridge*, uint32_t id)
    {
        static_cast<Target*>(data)->bridgeConnected(id);
    },
};

GSourceFuncs Target::s_sourceFuncs = {
    nullptr, // prepare
    nullptr, // check
    // dispatch
    [](GSource* source, GSourceFunc callback, gpointer userData) -> gboolean
    {
        if (g_source_get_ready_time(source) == -1)
            return G_SOURCE_CONTINUE;
        g_source_set_ready_time(source, -1);
        return callback(userData);
    },
    nullptr, // finalize
    nullptr, // closure_callback
    nullptr, // closure_marshall
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
