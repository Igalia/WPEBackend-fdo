#include "ws-shm.h"

namespace WS {

ImplSHM::ImplSHM() = default;
ImplSHM::~ImplSHM() = default;

void ImplSHM::surfaceAttach(Surface& surface, struct wl_resource* bufferResource)
{
    surface.shmBuffer = wl_shm_buffer_get(bufferResource);

    if (surface.bufferResource)
        wl_buffer_send_release(surface.bufferResource);
    surface.bufferResource = bufferResource;
}

void ImplSHM::surfaceCommit(Surface& surface)
{
    if (!surface.exportableClient)
        return;

    struct wl_resource* bufferResource = surface.bufferResource;
    surface.bufferResource = nullptr;

    if (surface.shmBuffer)
        surface.exportableClient->exportShmBuffer(bufferResource, surface.shmBuffer);
    else
        surface.exportableClient->exportBufferResource(bufferResource);
}

bool ImplSHM::initialize()
{
    // wl_display_init_shm() returns `0` on success.
    if (wl_display_init_shm(display()) != 0)
        return false;

    m_initialized = true;
    return true;
}

} // namespace WS
