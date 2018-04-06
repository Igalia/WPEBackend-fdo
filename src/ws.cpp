#include "ws.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "linux-dmabuf/linux-dmabuf.h"
#include <sys/socket.h>
#include <unistd.h>

#ifndef EGL_WL_bind_wayland_display
typedef EGLBoolean (EGLAPIENTRYP PFNEGLBINDWAYLANDDISPLAYWL) (EGLDisplay dpy, struct wl_display *display);
#endif

namespace WS {

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
        wl_display_flush_clients(source.display);
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
    ExportableClient* exportableClient { nullptr };

    struct wl_resource* bufferResource { nullptr };
    const struct linux_dmabuf_buffer* dmabufBuffer { nullptr };
};

static const struct wl_surface_interface s_surfaceInterface = {
    // destroy
    [](struct wl_client*, struct wl_resource*) { },
    // attach
    [](struct wl_client*, struct wl_resource* surfaceResource, struct wl_resource* bufferResource, int32_t, int32_t)
    {
        auto& surface = *static_cast<Surface*>(wl_resource_get_user_data(surfaceResource));

        surface.dmabufBuffer = linux_dmabuf_get_buffer(bufferResource);

        if (surface.bufferResource)
            wl_buffer_send_release(surface.bufferResource);
        surface.bufferResource = bufferResource;
    },
    // damage
    [](struct wl_client*, struct wl_resource*, int32_t, int32_t, int32_t, int32_t)
    {
    },
    // frame
    [](struct wl_client* client, struct wl_resource* surfaceResource, uint32_t callback)
    {
        auto& surface = *static_cast<Surface*>(wl_resource_get_user_data(surfaceResource));

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

        if (surface.dmabufBuffer) {
            const struct linux_dmabuf_attributes *attribs =
                linux_dmabuf_get_buffer_attributes(surface.dmabufBuffer);

            surface.exportableClient->exportLinuxDmabuf(attribs->width, attribs->height,
                                                        attribs->format, attribs->flags,
                                                        attribs->n_planes,
                                                        attribs->fd, attribs->stride,
                                                        attribs->offset, attribs->modifier);
        } else {
            struct wl_resource* bufferResource = surface.bufferResource;
            surface.bufferResource = nullptr;
            surface.exportableClient->exportBufferResource(bufferResource);
        }
    },
    // set_buffer_transform
    [](struct wl_client*, struct wl_resource*, int32_t) { },
    // set_buffer_scale
    [](struct wl_client*, struct wl_resource*, int32_t) { },
    // damage_buffer
    [](struct wl_client*, struct wl_resource*, int32_t, int32_t, int32_t, int32_t) { },
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
        surface->id = id;
        Instance::singleton().createSurface(id, surface);
        wl_resource_set_implementation(surfaceResource, &s_surfaceInterface, surface, nullptr);
    },
    // create_region
    [](struct wl_client*, struct wl_resource*, uint32_t) { },
};

Instance& Instance::singleton()
{
    static Instance* s_singleton;
    if (!s_singleton)
        s_singleton = new Instance;
    return *s_singleton;
}

Instance::Instance()
{

    m_display = wl_display_create();

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

    m_source = g_source_new(&Source::s_sourceFuncs, sizeof(Source));
    auto& source = *reinterpret_cast<Source*>(m_source);

    struct wl_event_loop* eventLoop = wl_display_get_event_loop(m_display);
    source.pfd.fd = wl_event_loop_get_fd(eventLoop);
    source.pfd.events = G_IO_IN | G_IO_ERR | G_IO_HUP;
    source.pfd.revents = 0;
    source.display = m_display;

    g_source_add_poll(m_source, &source.pfd);
    g_source_set_name(m_source, "WPEBackend-fdo::Host");
    g_source_set_priority(m_source, -70);
    g_source_set_can_recurse(m_source, TRUE);
    g_source_attach(m_source, g_main_context_get_thread_default());
}

Instance::~Instance()
{
    linux_dmabuf_teardown();

    if (m_source) {
        g_source_destroy(m_source);
        g_source_unref(m_source);
    }

    if (m_display)
        wl_display_destroy(m_display);
}

void Instance::initialize(EGLDisplay eglDisplay)
{
    PFNEGLBINDWAYLANDDISPLAYWL bindWaylandDisplayWL =
        reinterpret_cast<PFNEGLBINDWAYLANDDISPLAYWL>(eglGetProcAddress("eglBindWaylandDisplayWL"));
    bindWaylandDisplayWL(eglDisplay, m_display);

    /* Initialize Linux dmabuf subsystem. */
    linux_dmabuf_setup(m_display, eglDisplay);
}

int Instance::createClient()
{
    int pair[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, pair) < 0)
        return -1;

    int clientFd = dup(pair[1]);
    close(pair[1]);

    wl_client_create(m_display, pair[0]);
    return clientFd;
}

void Instance::createSurface(uint32_t id, Surface* surface)
{
    m_viewBackendMap.insert({ id, surface });
}

void Instance::registerViewBackend(uint32_t id, ExportableClient& exportableClient)
{
    auto it = m_viewBackendMap.find(id);
    if (it == m_viewBackendMap.end())
        std::abort();

    it->second->exportableClient = &exportableClient;
}

void Instance::unregisterViewBackend(uint32_t id)
{
    auto it = m_viewBackendMap.find(id);
    if (it == m_viewBackendMap.end())
        std::abort();

    it->second->exportableClient = nullptr;
    m_viewBackendMap.erase(id);
}

} // namespace WS
