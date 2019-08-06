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

#define VK_USE_PLATFORM_WAYLAND_KHR

#include <cstring>
#include <wpe/wpe-vulkan.h>
#include "interfaces.h"
#include "ws-client.h"

namespace {

class Backend;
struct BackendRef {
    GMutex mutex;
    Backend* backend;

    static BackendRef* create(Backend* backend)
    {
        BackendRef* ref = g_rc_box_new0(BackendRef);
        g_mutex_init(&ref->mutex);
        ref->backend = backend;
        return ref;
    }

    static BackendRef* reference(BackendRef* object)
    {
        return static_cast<BackendRef*>(g_rc_box_acquire(object));
    }

    static void dereference(BackendRef* object)
    {
        g_rc_box_release_full(object,
            [](gpointer data)
            {
                auto* ref = static_cast<BackendRef*>(data);
                g_mutex_clear(&ref->mutex);
            });
    }
};

class Backend final : public WS::BaseBackend {
public:
    Backend(int hostFD)
        : WS::BaseBackend(hostFD)
    {
        m_ref = BackendRef::create(this);
    }

    ~Backend()
    {
        g_mutex_lock(&m_ref->mutex);
        m_ref->backend = nullptr;
        g_mutex_unlock(&m_ref->mutex);
        BackendRef::dereference(m_ref);
        m_ref = nullptr;

        if (m_vk.instance)
            vkDestroyInstance(m_vk.instance, m_vk.allocator);
        m_vk.instance = VK_NULL_HANDLE;
    }

    using WS::BaseBackend::display;

    BackendRef* ref() const { return m_ref; }

    void initialize(const VkApplicationInfo* applicationInfo, const VkAllocationCallbacks* allocator, const struct wpe_renderer_backend_vulkan_initialization_parameters* initialParameters)
    {
        m_vk.allocator = allocator;

        struct wpe_renderer_backend_vulkan_initialization_parameters parameters;
        parameters.enabled_layer_count = initialParameters->enabled_layer_count;
        parameters.enabled_layer_names = initialParameters->enabled_layer_names;
        parameters.enabled_extension_count = initialParameters->enabled_extension_count + 2;

        const char** expandedExtensionNames = g_new0(const char*, parameters.enabled_extension_count);
        expandedExtensionNames[0] = VK_KHR_SURFACE_EXTENSION_NAME;
        expandedExtensionNames[1] = VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
        for (uint32_t i = 0; i < initialParameters->enabled_extension_count; ++i)
            expandedExtensionNames[2 + i] = initialParameters->enabled_extension_names[i];
        parameters.enabled_extension_names = expandedExtensionNames;

        VkInstanceCreateInfo instanceCreateInfo;
        std::memset(&instanceCreateInfo, 0, sizeof(VkInstanceCreateInfo));
        instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.pNext = nullptr;
        instanceCreateInfo.flags = 0;
        instanceCreateInfo.pApplicationInfo = applicationInfo;
        instanceCreateInfo.enabledLayerCount = parameters.enabled_layer_count;
        instanceCreateInfo.ppEnabledLayerNames = parameters.enabled_layer_names;
        instanceCreateInfo.enabledExtensionCount = parameters.enabled_extension_count;
        instanceCreateInfo.ppEnabledExtensionNames = parameters.enabled_extension_names;

        VkInstance instance;
        VkResult result = vkCreateInstance(&instanceCreateInfo, m_vk.allocator, &instance);
        g_free(expandedExtensionNames);

        if (result != VK_SUCCESS)
            return;

        m_vk.procAddresses.createWaylandSurface =
            (PFN_vkCreateWaylandSurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateWaylandSurfaceKHR");
        m_vk.procAddresses.getPhysicalDeviceWaylandPresentationSupport =
            (PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceWaylandPresentationSupportKHR");
        if (!m_vk.procAddresses.createWaylandSurface || !m_vk.procAddresses.getPhysicalDeviceWaylandPresentationSupport) {
            vkDestroyInstance(instance, m_vk.allocator);
            return;
        }

        m_vk.instance = instance;
    }

    bool supportsPhysicalDevice(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex)
    {
        if (!m_vk.instance)
            return false;
        return !!m_vk.procAddresses.getPhysicalDeviceWaylandPresentationSupport(physicalDevice, queueFamilyIndex, display());
    }

    const VkAllocationCallbacks* allocator() const { return m_vk.allocator; }
    VkInstance instance() const { return m_vk.instance; }

    struct ProcAddresses {
        PFN_vkCreateWaylandSurfaceKHR createWaylandSurface;
        PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR getPhysicalDeviceWaylandPresentationSupport;
    };
    const ProcAddresses& procAddresses() const { return m_vk.procAddresses; }

private:
    BackendRef* m_ref;

    struct {
        const VkAllocationCallbacks* allocator { nullptr };
        VkInstance instance { VK_NULL_HANDLE };
        ProcAddresses procAddresses { nullptr, nullptr };
    } m_vk;
};

class Target final : public WS::BaseTarget, public WS::BaseTarget::Impl {
public:
    Target(struct wpe_renderer_backend_vulkan_target* target, int hostFD)
        : WS::BaseTarget(hostFD, *this)
        , m_target(target)
    { }

    ~Target()
    {
        if (m_backendRef) {
            g_mutex_lock(&m_backendRef->mutex);
            if (m_backendRef->backend && m_vk.surface) {
                auto& backend = *m_backendRef->backend;
                vkDestroySurfaceKHR(backend.instance(), m_vk.surface, backend.allocator());
            }
            g_mutex_unlock(&m_backendRef->mutex);

            BackendRef::dereference(m_backendRef);
        }
    }

    void initialize(Backend& backend, uint32_t width, uint32_t height)
    {
        m_backendRef = BackendRef::reference(backend.ref());

        WS::BaseTarget::initialize(backend.display());

        VkWaylandSurfaceCreateInfoKHR wlSurfaceCreateInfo;
        std::memset(&wlSurfaceCreateInfo, 0, sizeof(VkWaylandSurfaceCreateInfoKHR));
        wlSurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        wlSurfaceCreateInfo.pNext = nullptr;
        wlSurfaceCreateInfo.flags = 0;
        wlSurfaceCreateInfo.display = backend.display();
        wlSurfaceCreateInfo.surface = surface();

        VkSurfaceKHR surface;
        VkResult result = backend.procAddresses().createWaylandSurface(backend.instance(), &wlSurfaceCreateInfo, backend.allocator(), &surface);
        if (result != VK_SUCCESS)
            return;

        m_vk.surface = surface;
    }

    using WS::BaseTarget::requestFrame;

    VkSurfaceKHR vkSurface() const { return m_vk.surface; }

private:
    // WS::BaseTarget::Impl
    void dispatchFrameComplete() override
    {
        wpe_renderer_backend_vulkan_target_dispatch_frame_complete(m_target);
    }

    struct wpe_renderer_backend_vulkan_target* m_target { nullptr };
    BackendRef* m_backendRef { nullptr };

    struct {
        VkSurfaceKHR surface { VK_NULL_HANDLE };
    } m_vk;
};

} // namespace

struct wpe_renderer_backend_vulkan_interface fdo_renderer_backend_vulkan = {
    // create
    [](int host_fd) -> void*
    {
        return new Backend(host_fd);
    },
    // destroy
    [](void* data)
    {
        delete reinterpret_cast<Backend*>(data);
    },
    // initialize
    [](void* data, const VkApplicationInfo* application_info, const VkAllocationCallbacks* allocator, const struct wpe_renderer_backend_vulkan_initialization_parameters* parameters)
    {
        auto& backend = *reinterpret_cast<Backend*>(data);
        backend.initialize(application_info, allocator, parameters);
    },
    // get_instance
    [](void* data) -> VkInstance
    {
        auto& backend = *reinterpret_cast<Backend*>(data);
        return backend.instance();
    },
    // supports_physical_device
    [](void* data, VkPhysicalDevice physical_device, uint32_t queue_family_index) -> bool
    {
        auto& backend = *reinterpret_cast<Backend*>(data);
        return backend.supportsPhysicalDevice(physical_device, queue_family_index);
    },
};

struct wpe_renderer_backend_vulkan_target_interface fdo_renderer_backend_vulkan_target = {
    // create
    [](struct wpe_renderer_backend_vulkan_target* target, int host_fd) -> void*
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
    // get_surface
    [](void* data) -> VkSurfaceKHR
    {
        auto& target = *reinterpret_cast<Target*>(data);
        return target.vkSurface();
    },
    // resize
    [](void*, uint32_t, uint32_t)
    {
    },
    // frame_will_render
    [](void* data)
    {
        auto& target = *reinterpret_cast<Target*>(data);
        target.requestFrame();
    },
    // frame_rendered
    [](void*)
    {
    },
};
