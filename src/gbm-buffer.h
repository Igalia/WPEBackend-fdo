#pragma once

#include <stdint.h>
#include <wayland-util.h>

struct wl_resource;

struct gbm_buffer {
    struct wl_list link;
    struct wl_resource* resource;

    int fd;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t format;
};
