#include "interfaces.h"

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
};

GSourceFuncs Source::s_sourceFuncs = {
    // prepare
    [](GSource* base, gint* timeout) -> gboolean
    {
        auto& source = *reinterpret_cast<Source*>(base);
        *timeout = -1;
        wl_display_dispatch_pending(source.display);
        wl_display_flush(source.display);
        return FALSE;
    },
    // check
    [](GSource* base) -> gboolean
    {
        auto& source = *reinterpret_cast<Source*>(base);
        return !!source.pfd.revents;
    },
    // dispatch
    [](GSource* base, GSourceFunc, gpointer) -> gboolean
    {
        auto& source = *reinterpret_cast<Source*>(base);

        if (source.pfd.revents & G_IO_IN)
            wl_display_dispatch(source.display);

        if (source.pfd.revents & (G_IO_ERR | G_IO_HUP))
            return FALSE;

        source.pfd.revents = 0;
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

        m_source = g_source_new(&Source::s_sourceFuncs, sizeof(Source));
        auto& source = *reinterpret_cast<Source*>(m_source);
        source.pfd.fd = wl_display_get_fd(m_display);
        source.pfd.events = G_IO_IN | G_IO_ERR | G_IO_HUP;
        source.pfd.revents = 0;
        source.display = m_display;

        g_source_add_poll(m_source, &source.pfd);
        g_source_set_name(m_source, "WPEBackend-fdo::Backend");
        g_source_set_priority(m_source, -70);
        g_source_set_can_recurse(m_source, TRUE);
        g_source_attach(m_source, g_main_context_get_thread_default());

        m_registry = wl_display_get_registry(m_display);
        wl_registry_add_listener(m_registry, &s_registryListener, this);
        wl_display_roundtrip(m_display);
    }

    ~Backend()
    {
        if (m_source) {
            g_source_destroy(m_source);
            g_source_unref(m_source);
        }
    }

    struct wl_display* display() const { return m_display; }
    struct wl_compositor* compositor() const { return m_compositor; }

private:
    static const struct wl_registry_listener s_registryListener;

    struct wl_display* m_display;
    struct wl_registry* m_registry;
    struct wl_compositor* m_compositor;

    GSource* m_source;
};

const struct wl_registry_listener Backend::s_registryListener = {
    // global
    [](void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t)
    {
        auto& backend = *reinterpret_cast<Backend*>(data);

        if (!std::strcmp(interface, "wl_compositor"))
            backend.m_compositor = static_cast<struct wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, 1));
    },
    // global_remove
    [](void*, struct wl_registry*, uint32_t) { },
};

class Target {
public:
    Target(struct wpe_renderer_backend_egl_target* target, int hostFd)
        : m_target(target)
    {
        m_socket = g_socket_new_from_fd(hostFd, nullptr);
        if (m_socket)
            g_socket_set_blocking(m_socket, FALSE);
    }

    ~Target()
    {
        if (m_frameCallback)
            wl_callback_destroy(m_frameCallback);
        if (m_window)
            wl_egl_window_destroy(m_window);
        if (m_surface)
            wl_surface_destroy(m_surface);

        if (m_socket)
            g_object_unref(m_socket);
        if (m_source) {
            g_source_destroy(m_source);
            g_source_unref(m_source);
        }
    }

    void initialize(Backend& backend, uint32_t width, uint32_t height)
    {
        m_source = g_source_new(&s_sourceFuncs, sizeof(GSource));
        g_source_set_priority(m_source, -70);
        g_source_set_name(m_source, "WPEBackend-fdo::Target");
        g_source_set_callback(m_source, [](gpointer userData) -> gboolean {
            auto* target = static_cast<Target*>(userData);

            wl_callback_destroy(target->m_frameCallback);
            target->m_frameCallback = nullptr;

            wpe_renderer_backend_egl_target_dispatch_frame_complete(target->m_target);
            return G_SOURCE_CONTINUE;
        }, this, nullptr);
        g_source_attach(m_source, g_main_context_get_thread_default());

        m_surface = wl_compositor_create_surface(backend.compositor());
        m_window = wl_egl_window_create(m_surface, width, height);
        wl_display_roundtrip(backend.display());

        uint32_t message[] = { 0x42, wl_proxy_get_id(reinterpret_cast<struct wl_proxy*>(m_surface)) };
        if (m_socket)
            g_socket_send(m_socket, reinterpret_cast<gchar*>(message), 2 * sizeof(uint32_t),
                nullptr, nullptr);
    }

    void requestFrame()
    {
        if (m_frameCallback)
            std::abort();

        m_frameCallback = wl_surface_frame(m_surface);
        wl_callback_add_listener(m_frameCallback, &s_callbackListener, this);
    }

    void dispatchFrameComplete()
    {
        g_source_set_ready_time(m_source, 0);
    }

    struct wl_egl_window* window() const { return m_window; }

private:
    static const struct wl_callback_listener s_callbackListener;
    static GSourceFuncs s_sourceFuncs;

    struct wpe_renderer_backend_egl_target* m_target { nullptr };
    GSource* m_source { nullptr };
    GSocket* m_socket { nullptr };

    struct wl_surface* m_surface { nullptr };
    struct wl_egl_window* m_window { nullptr };
    struct wl_callback* m_frameCallback { nullptr };
};

const struct wl_callback_listener Target::s_callbackListener = {
    // done
    [](void* data, struct wl_callback*, uint32_t time)
    {
        static_cast<Target*>(data)->dispatchFrameComplete();
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
