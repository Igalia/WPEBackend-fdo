/*
 * Copyright (C) 2020 Igalia S.L.
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

#pragma once

#include "ws.h"
#include <functional>

typedef void *EGLDisplay;
typedef void *EGLImageKHR;

namespace WS {

class ImplEGL final : public Instance::Impl {
public:
    ImplEGL();
    virtual ~ImplEGL();

    Type type() const override { return Instance::Impl::Type::EGL; }
    bool initialized() const override { return m_initialized; }

    void surfaceAttach(Surface&, struct wl_resource*) override;
    void surfaceCommit(Surface&) override;

    bool initialize(EGLDisplay);

    EGLImageKHR createImage(struct wl_resource*);
    EGLImageKHR createImage(const struct linux_dmabuf_buffer*);
    void destroyImage(EGLImageKHR);
    void queryBufferSize(struct wl_resource*, uint32_t* width, uint32_t* height);

    void importDmaBufBuffer(struct linux_dmabuf_buffer*);
    const struct linux_dmabuf_buffer* getDmaBufBuffer(struct wl_resource*) const;
    void foreachDmaBufModifier(std::function<void (int format, uint64_t modifier)>);

private:
    bool m_initialized { false };
    EGLDisplay m_eglDisplay;

    struct {
        struct wl_global* global { nullptr };
        struct wl_list buffers;
    } m_dmabuf;
};

template<>
auto inline instanceImpl<ImplEGL>() -> ImplEGL&
{
    auto& instance = WS::Instance::singleton();
    return static_cast<ImplEGL&>(instance.impl());
}

} // namespace WS
