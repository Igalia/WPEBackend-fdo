#pragma once

#include <glib.h>
#include <unordered_map>
#include <wayland-server.h>

typedef void *EGLDisplay;

namespace WS {

struct ExportableClient {
    virtual void frameCallback(struct wl_resource*) = 0;
    virtual void exportBufferResource(struct wl_resource*) = 0;
    virtual void exportLinuxDmabuf(uint32_t width, uint32_t height,
                                   uint32_t format, uint32_t flags,
                                   uint32_t num_planes,
                                   const int32_t* fds, const uint32_t* strides,
                                   const uint32_t* offsets,
                                   const uint64_t* modifiers) = 0;
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

private:
    Instance();

    struct wl_display* m_display;
    struct wl_global* m_compositor;
    GSource* m_source;

    std::unordered_map<uint32_t, Surface*> m_viewBackendMap;
};

} // namespace WS
