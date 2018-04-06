#pragma once

#include <glib.h>
#include <unordered_map>
#include <wayland-server.h>

typedef void *EGLDisplay;

namespace WS {

struct ExportableClient {
    virtual void frameCallback(struct wl_resource*) = 0;
    virtual void exportBufferResource(struct wl_resource*) = 0;
};

struct Surface;

class Instance {
public:
    static Instance& singleton();
    ~Instance();

    void initialize(EGLDisplay);

    int createClient();

    void createSurface(uint32_t, Surface*);
    void registerViewBackend(uint32_t, ExportableClient&);
    void unregisterViewBackend(uint32_t);

private:
    Instance();

    struct wl_display* m_display;
    struct wl_global* m_compositor;
    GSource* m_source;

    std::unordered_map<uint32_t, Surface*> m_viewBackendMap;
};

} // namespace WS
