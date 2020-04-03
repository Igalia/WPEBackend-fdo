#pragma once

#include "ws.h"

namespace WS {

class ImplSHM : public Instance::Impl {
public:
    ImplSHM();
    virtual ~ImplSHM();

    Type type() const override { return Instance::Impl::Type::SHM; }
    bool initialized() const override { return m_initialized; }

    void surfaceAttach(Surface&, struct wl_resource*) override;
    void surfaceCommit(Surface&) override;

    bool initialize();

private:
    bool m_initialized { false };
};

} // namespace WS
