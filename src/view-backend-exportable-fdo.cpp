#include <wpe-fdo/view-backend-exportable.h>

#include "ws.h"
#include <gio/gio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <vector>

namespace {

class ViewBackend;

struct ClientBundle {
    struct wpe_view_backend_exportable_fdo_client* client;
    void* data;
    ViewBackend* viewBackend;
    uint32_t initialWidth;
    uint32_t initialHeight;
};

class ViewBackend : public WS::ExportableClient {
public:
    ViewBackend(ClientBundle* clientBundle, struct wpe_view_backend* backend)
        : m_clientBundle(clientBundle)
        , m_backend(backend)
    {
        m_clientBundle->viewBackend = this;
    }

    ~ViewBackend()
    {
        for (auto* resource : m_callbackResources)
            wl_resource_destroy(resource);

        WS::Instance::singleton().unregisterViewBackend(m_id);

        if (m_clientFd != -1)
            close(m_clientFd);

        if (m_source) {
            g_source_destroy(m_source);
            g_source_unref(m_source);
        }
        if (m_socket)
            g_object_unref(m_socket);
    }

    void initialize()
    {
        int sockets[2];
        int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
        if (ret == -1)
            return;

        m_socket = g_socket_new_from_fd(sockets[0], nullptr);
        if (!m_socket) {
            close(sockets[0]);
            close(sockets[1]);
            return;
        }

        g_socket_set_blocking(m_socket, FALSE);

        m_source = g_socket_create_source(m_socket, G_IO_IN, nullptr);
        g_source_set_callback(m_source, reinterpret_cast<GSourceFunc>(s_socketCallback), this, nullptr);
        g_source_set_priority(m_source, -70);
        g_source_set_can_recurse(m_source, TRUE);
        g_source_attach(m_source, g_main_context_get_thread_default());

        m_clientFd = sockets[1];

        wpe_view_backend_dispatch_set_size(m_backend,
            m_clientBundle->initialWidth, m_clientBundle->initialHeight);
    }

    int clientFd()
    {
        return dup(m_clientFd);
    }

    void frameCallback(struct wl_resource* callbackResource) override
    {
        m_callbackResources.push_back(callbackResource);
    }

    void exportBufferResource(struct wl_resource* bufferResource) override
    {
        m_clientBundle->client->export_buffer_resource(m_clientBundle->data, bufferResource);
    }

    void exportLinuxDmabuf(uint32_t width, uint32_t height, uint32_t format,
                           uint32_t flags, uint32_t num_planes, const int32_t* fds,
                           const uint32_t* strides, const uint32_t* offsets,
                           const uint64_t* modifiers) override
    {
        m_clientBundle->client->export_linux_dmabuf(m_clientBundle->data,
                                                    width, height, format, flags,
                                                    num_planes, fds, strides, offsets,
                                                    modifiers);
    }

    void dispatchFrameCallback()
    {
        for (auto* resource : m_callbackResources)
            wl_callback_send_done(resource, 0);
        m_callbackResources.clear();
    }

    void releaseBuffer(struct wl_resource* buffer_resource)
    {
        wl_buffer_send_release(buffer_resource);
    }

private:
    static gboolean s_socketCallback(GSocket*, GIOCondition, gpointer);

    uint32_t m_id { 0 };

    ClientBundle* m_clientBundle;
    struct wpe_view_backend* m_backend;

    std::vector<struct wl_resource*> m_callbackResources;

    GSocket* m_socket;
    GSource* m_source;
    int m_clientFd { -1 };
};

gboolean ViewBackend::s_socketCallback(GSocket* socket, GIOCondition condition, gpointer data)
{
    if (!(condition & G_IO_IN))
        return TRUE;

    uint32_t message[2];
    gssize len = g_socket_receive(socket, reinterpret_cast<gchar*>(message), sizeof(uint32_t) * 2,
        nullptr, nullptr);
    if (len == -1)
        return FALSE;

    if (len == sizeof(uint32_t) * 2 && message[0] == 0x42) {
        auto& viewBackend = *static_cast<ViewBackend*>(data);
        viewBackend.m_id = message[1];
        WS::Instance::singleton().registerViewBackend(viewBackend.m_id, viewBackend);
    }

    return TRUE;
}

static struct wpe_view_backend_interface view_backend_exportable_fdo_interface = {
    // create
    [](void* data, struct wpe_view_backend* backend) -> void*
    {
        auto* clientBundle = reinterpret_cast<ClientBundle*>(data);
        return new ViewBackend(clientBundle, backend);
    },
    // destroy
    [](void* data)
    {
        auto* backend = reinterpret_cast<ViewBackend*>(data);
        delete backend;
    },
    // initialize
    [](void* data)
    {
        auto& backend = *reinterpret_cast<ViewBackend*>(data);
        backend.initialize();
    },
    // get_renderer_host_fd
    [](void* data) -> int
    {
        auto& backend = *reinterpret_cast<ViewBackend*>(data);
        return backend.clientFd();
    }
};

} // namespace

extern "C" {

struct wpe_view_backend_exportable_fdo {
    ClientBundle* clientBundle;
    struct wpe_view_backend* backend;
};

__attribute__((visibility("default")))
struct wpe_view_backend_exportable_fdo*
wpe_view_backend_exportable_fdo_create(struct wpe_view_backend_exportable_fdo_client* client, void* data, uint32_t width, uint32_t height)
{
    auto* clientBundle = new ClientBundle{ client, data, nullptr, width, height };

    struct wpe_view_backend* backend = wpe_view_backend_create_with_backend_interface(&view_backend_exportable_fdo_interface, clientBundle);

    auto* exportable = new struct wpe_view_backend_exportable_fdo;
    exportable->clientBundle = clientBundle;
    exportable->backend = backend;

    return exportable;
}

__attribute__((visibility("default")))
void
wpe_view_backend_exportable_fdo_destroy(struct wpe_view_backend_exportable_fdo* exportable)
{
    wpe_view_backend_destroy(exportable->backend);
    delete exportable;
}

__attribute__((visibility("default")))
struct wpe_view_backend*
wpe_view_backend_exportable_fdo_get_view_backend(struct wpe_view_backend_exportable_fdo* exportable)
{
    return exportable->backend;
}

__attribute__((visibility("default")))
void
wpe_view_backend_exportable_fdo_dispatch_frame_complete(struct wpe_view_backend_exportable_fdo* exportable)
{
    exportable->clientBundle->viewBackend->dispatchFrameCallback();
}

__attribute__((visibility("default")))
void
wpe_view_backend_exportable_fdo_dispatch_release_buffer(struct wpe_view_backend_exportable_fdo* exportable, struct wl_resource* buffer_resource)
{
    exportable->clientBundle->viewBackend->releaseBuffer(buffer_resource);
}

}
