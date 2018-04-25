/*
 * Adopted from the Weston project <https://cgit.freedesktop.org/wayland/weston>
 * along with its license.
 */

#include <assert.h>
#include "drm_fourcc.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "linux-dmabuf.h"
#include "linux-dmabuf-unstable-v1-server-protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef EGL_EXT_image_dma_buf_import_modifiers
#define EGL_EXT_image_dma_buf_import_modifiers 1
#define EGL_DMA_BUF_PLANE3_FD_EXT         0x3440
#define EGL_DMA_BUF_PLANE3_OFFSET_EXT     0x3441
#define EGL_DMA_BUF_PLANE3_PITCH_EXT      0x3442
#define EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT 0x3443
#define EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT 0x3444
#define EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT 0x3445
#define EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT 0x3446
#define EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT 0x3447
#define EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT 0x3448
#define EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT 0x3449
#define EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT 0x344A
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYDMABUFFORMATSEXTPROC) (EGLDisplay dpy, EGLint max_formats, EGLint *formats, EGLint *num_formats);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYDMABUFMODIFIERSEXTPROC) (EGLDisplay dpy, EGLint format, EGLint max_modifiers, EGLuint64KHR *modifiers, EGLBoolean *external_only, EGLint *num_modifiers);
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglQueryDmaBufFormatsEXT (EGLDisplay dpy, EGLint max_formats, EGLint *formats, EGLint *num_formats);
EGLAPI EGLBoolean EGLAPIENTRY eglQueryDmaBufModifiersEXT (EGLDisplay dpy, EGLint format, EGLint max_modifiers, EGLuint64KHR *modifiers, EGLBoolean *external_only, EGLint *num_modifiers);
#endif
#endif /* EGL_EXT_image_dma_buf_import_modifiers */

typedef void (*linux_dmabuf_user_data_destroy_func)(struct linux_dmabuf_buffer *buffer);

/* Per-process global data. */
static struct {
    struct wl_display *wl_display;
    EGLDisplay egl_display;

    PFNEGLQUERYDMABUFFORMATSEXTPROC eglQueryDmaBufFormatsEXT;
    PFNEGLQUERYDMABUFMODIFIERSEXTPROC eglQueryDmaBufModifiersEXT;

    /* A list of active linux_dmabuf_buffer's. */
    struct wl_list dmabuf_buffers;
} linux_dmabuf_data = {NULL, };

struct linux_dmabuf_buffer {
    struct wl_resource *buffer_resource;
    struct wl_resource *params_resource;
    struct linux_dmabuf_attributes attributes;

    void *user_data;
    linux_dmabuf_user_data_destroy_func user_data_destroy_func;

    struct wl_list link;
};

static void
linux_dmabuf_buffer_destroy(struct linux_dmabuf_buffer *buffer)
{
    for (int i = 0; i < buffer->attributes.n_planes; i++) {
        close(buffer->attributes.fd[i]);
        buffer->attributes.fd[i] = -1;
    }
    buffer->attributes.n_planes = 0;

    wl_list_remove(&buffer->link);

    free(buffer);
}

static void
params_destroy(struct wl_client *client, struct wl_resource *resource)
{
    wl_resource_destroy(resource);
}

static void
params_add(struct wl_client *client,
	   struct wl_resource *params_resource,
	   int32_t name_fd,
	   uint32_t plane_idx,
	   uint32_t offset,
	   uint32_t stride,
	   uint32_t modifier_hi,
	   uint32_t modifier_lo)
{
    struct linux_dmabuf_buffer *buffer =
        wl_resource_get_user_data(params_resource);
    if (!buffer) {
        wl_resource_post_error(params_resource,
                               ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED,
                               "params was already used to create a wl_buffer");
        close(name_fd);
        return;
    }

    assert(buffer->params_resource == params_resource);
    assert(!buffer->buffer_resource);

    if (plane_idx >= MAX_DMABUF_PLANES) {
        wl_resource_post_error(params_resource,
                               ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX,
                               "plane index %u is too high", plane_idx);
        close(name_fd);
        return;
    }

    if (buffer->attributes.fd[plane_idx] != -1) {
        wl_resource_post_error(params_resource,
                               ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_SET,
                               "a dmabuf has already been added for plane %u",
                               plane_idx);
        close(name_fd);
        return;
    }

    buffer->attributes.fd[plane_idx] = name_fd;
    buffer->attributes.offset[plane_idx] = offset;
    buffer->attributes.stride[plane_idx] = stride;

    if (wl_resource_get_version(params_resource) < ZWP_LINUX_DMABUF_V1_MODIFIER_SINCE_VERSION)
        buffer->attributes.modifier[plane_idx] = DRM_FORMAT_MOD_INVALID;
    else
        buffer->attributes.modifier[plane_idx] = ((uint64_t)modifier_hi << 32) |
                                                            modifier_lo;

    buffer->attributes.n_planes++;
}

static void
destroy_wl_buffer_resource(struct wl_resource *resource)
{
    struct linux_dmabuf_buffer *buffer;

    buffer = wl_resource_get_user_data(resource);
    assert(buffer && buffer->buffer_resource == resource);
    assert(!buffer->params_resource);

    if (buffer->user_data_destroy_func)
        buffer->user_data_destroy_func(buffer);

    linux_dmabuf_buffer_destroy(buffer);
}

static void
linux_dmabuf_wl_buffer_destroy(struct wl_client *client,
			       struct wl_resource *resource)
{
    wl_resource_destroy(resource);
}

static const struct wl_buffer_interface linux_dmabuf_buffer_implementation = {
    .destroy = linux_dmabuf_wl_buffer_destroy
};

static bool
import_dmabuf(struct linux_dmabuf_buffer *dmabuf)
{
    for (int i = 1; i < dmabuf->attributes.n_planes; i++) {
        /* Return if modifiers passed are unequal. */
        if (dmabuf->attributes.modifier[i] != dmabuf->attributes.modifier[0])
            return false;
    }

    /* Accept the buffer. */
    wl_list_insert(&linux_dmabuf_data.dmabuf_buffers, &dmabuf->link);

    return true;
}

static void
params_create_common(struct wl_client *client, struct wl_resource *params_resource,
                     uint32_t buffer_id, int32_t width, int32_t height,
                     uint32_t format, uint32_t flags)
{
    struct linux_dmabuf_buffer *buffer =
        wl_resource_get_user_data(params_resource);
    if (!buffer) {
        wl_resource_post_error(params_resource,
                               ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED,
                               "params was already used to create a wl_buffer");
        return;
    }

    assert(buffer->params_resource == params_resource);
    assert(!buffer->buffer_resource);

    /* Switch the linux_dmabuf_buffer object from params resource to
     * eventually wl_buffer resource.
     */
    wl_resource_set_user_data(buffer->params_resource, NULL);
    buffer->params_resource = NULL;

    if (!buffer->attributes.n_planes) {
        wl_resource_post_error(params_resource,
                               ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
                               "no dmabuf has been added to the params");
        goto err_out;
    }

    /* Check for holes in the dmabufs set (e.g. [0, 1, 3]). */
    for (int i = 0; i < buffer->attributes.n_planes; i++) {
        if (buffer->attributes.fd[i] == -1) {
            wl_resource_post_error(params_resource,
                                   ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
                                   "no dmabuf has been added for plane %i", i);
            goto err_out;
        }
    }

    buffer->attributes.width = width;
    buffer->attributes.height = height;
    buffer->attributes.format = format;
    buffer->attributes.flags = flags;

    if (width < 1 || height < 1) {
        wl_resource_post_error(params_resource,
                               ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_DIMENSIONS,
                               "invalid width %d or height %d", width, height);
        goto err_out;
    }

    for (int i = 0; i < buffer->attributes.n_planes; i++) {
        off_t size;

        if ((uint64_t) buffer->attributes.offset[i] + buffer->attributes.stride[i] > UINT32_MAX) {
            wl_resource_post_error(params_resource,
                                   ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS,
                                   "size overflow for plane %i", i);
            goto err_out;
        }

        if (i == 0 &&
            (uint64_t) buffer->attributes.offset[i] +
            (uint64_t) buffer->attributes.stride[i] * height > UINT32_MAX) {
            wl_resource_post_error(params_resource,
                                   ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS,
                                   "size overflow for plane %i", i);
            goto err_out;
        }

        /* Don't report an error as it might be caused
         * by the kernel not supporting seeking on dmabuf.
         */
        size = lseek(buffer->attributes.fd[i], 0, SEEK_END);
        if (size == -1)
            continue;

        if (buffer->attributes.offset[i] >= size) {
            wl_resource_post_error(params_resource,
                                   ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS,
                                   "invalid offset %i for plane %i",
                                   buffer->attributes.offset[i], i);
            goto err_out;
        }

        if (buffer->attributes.offset[i] + buffer->attributes.stride[i] > size) {
            wl_resource_post_error(params_resource,
                                   ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS,
                                   "invalid stride %i for plane %i",
                                   buffer->attributes.stride[i], i);
            goto err_out;
        }

        /* Only valid for first plane as other planes might be
         * sub-sampled according to fourcc format.
         */
        if (i == 0 &&
            buffer->attributes.offset[i] + buffer->attributes.stride[i] * height > size) {
            wl_resource_post_error(params_resource,
                                   ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS,
                                   "invalid buffer stride or height for plane %i", i);
            goto err_out;
        }
    }

    /* XXX: Some additional sanity checks could be done with respect
     * to the fourcc format. A centralized collection (kernel or
     * libdrm) would be useful to avoid code duplication for these
     * checks (e.g. drm_format_num_planes).
     */

    if (!import_dmabuf(buffer))
        goto err_failed;

    buffer->buffer_resource = wl_resource_create(client,
                                                 &wl_buffer_interface,
                                                 1, buffer_id);
    if (!buffer->buffer_resource) {
        wl_resource_post_no_memory(params_resource);
        goto err_buffer;
    }

    wl_resource_set_implementation(buffer->buffer_resource,
                                   &linux_dmabuf_buffer_implementation,
                                   buffer, destroy_wl_buffer_resource);

    /* Send 'created' event when the request is not for an immediate
     * import, ie buffer_id is zero.
     */
    if (buffer_id == 0)
        zwp_linux_buffer_params_v1_send_created(params_resource,
						buffer->buffer_resource);

    return;

 err_buffer:
    if (buffer->user_data_destroy_func)
        buffer->user_data_destroy_func(buffer);

 err_failed:
    if (buffer_id == 0) {
        zwp_linux_buffer_params_v1_send_failed(params_resource);
    } else {
      /* Since the behavior is left implementation defined by the
       * protocol in case of create_immed failure due to an unknown cause,
       * we choose to treat it as a fatal error and immediately kill the
       * client instead of creating an invalid handle and waiting for it
       * to be used.
       */
      wl_resource_post_error(params_resource,
                             ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_WL_BUFFER,
                             "importing the supplied dmabufs failed");
    }

 err_out:
    linux_dmabuf_buffer_destroy(buffer);
}

static void
params_create(struct wl_client *client,
	      struct wl_resource *params_resource,
	      int32_t width,
	      int32_t height,
	      uint32_t format,
	      uint32_t flags)
{
    params_create_common(client, params_resource, 0, width, height, format,
                         flags);
}

static void
params_create_immed(struct wl_client *client,
		    struct wl_resource *params_resource,
		    uint32_t buffer_id,
		    int32_t width,
		    int32_t height,
		    uint32_t format,
		    uint32_t flags)
{
    params_create_common(client, params_resource, buffer_id, width, height,
                         format, flags);
}

static const struct zwp_linux_buffer_params_v1_interface
zwp_linux_buffer_params_implementation = {
    .destroy = params_destroy,
    .add = params_add,
    .create = params_create,
    .create_immed = params_create_immed
};

static void
destroy_params(struct wl_resource *params_resource)
{
    struct linux_dmabuf_buffer *buffer =
        wl_resource_get_user_data(params_resource);
    if (!buffer)
        return;

    linux_dmabuf_buffer_destroy(buffer);
}

static void
linux_dmabuf_destroy(struct wl_client *client, struct wl_resource *resource)
{
    wl_resource_destroy(resource);
}

static void
linux_dmabuf_create_params(struct wl_client *client,
			   struct wl_resource *linux_dmabuf_resource,
			   uint32_t params_id)
{
    uint32_t version = wl_resource_get_version(linux_dmabuf_resource);

    struct linux_dmabuf_buffer *buffer =
        calloc(1, sizeof(struct linux_dmabuf_buffer));
    if (!buffer)
        goto err_out;

    for (int i = 0; i < MAX_DMABUF_PLANES; i++)
        buffer->attributes.fd[i] = -1;

    buffer->buffer_resource = NULL;
    buffer->params_resource =
        wl_resource_create(client,
                           &zwp_linux_buffer_params_v1_interface,
                           version, params_id);
    if (!buffer->params_resource)
        goto err_dealloc;

    wl_resource_set_implementation(buffer->params_resource,
                                   &zwp_linux_buffer_params_implementation,
                                   buffer, destroy_params);

    return;

err_dealloc:
    free(buffer);

err_out:
    wl_resource_post_no_memory(linux_dmabuf_resource);
}

static const struct zwp_linux_dmabuf_v1_interface linux_dmabuf_implementation = {
    .destroy = linux_dmabuf_destroy,
    .create_params = linux_dmabuf_create_params,
};

static void
bind_linux_dmabuf(struct wl_client *client, void *data, uint32_t version,
                  uint32_t id)
{
    struct wl_resource *resource =
        wl_resource_create(client, &zwp_linux_dmabuf_v1_interface,
                           version, id);
    if (resource == NULL) {
        wl_client_post_no_memory(client);
        return;
    }

    wl_resource_set_implementation(resource, &linux_dmabuf_implementation,
                                   data, NULL);

    EGLDisplay egl_display = linux_dmabuf_data.egl_display;
    assert(egl_display);

    EGLint formats[128];
    EGLint num_formats;
    uint64_t modifier_invalid = DRM_FORMAT_MOD_INVALID;
    if (!linux_dmabuf_data.eglQueryDmaBufFormatsEXT(egl_display, 128, formats,
                                                    &num_formats)) {
        assert(!"Linux-dmabuf: Failed to query formats");
    }

    for (int i = 0; i < num_formats; i++) {
        uint64_t modifiers[64];
        EGLint num_modifiers;
        if (!linux_dmabuf_data.eglQueryDmaBufModifiersEXT(egl_display,
                                                          formats[i], 64,
                                                          modifiers, NULL,
                                                          &num_modifiers)) {
            assert(!"Linux-dmabuf: Failed to query modifiers of a format");
        }

        /* Send DRM_FORMAT_MOD_INVALID token when no modifiers are supported
         * for this format.
         */
        if (num_modifiers == 0) {
            num_modifiers = 1;
            modifiers[0] = modifier_invalid;
        }

        for (int j = 0; j < num_modifiers; j++) {
            if (version >= ZWP_LINUX_DMABUF_V1_MODIFIER_SINCE_VERSION) {
                uint32_t modifier_lo = modifiers[j] & 0xFFFFFFFF;
                uint32_t modifier_hi = modifiers[j] >> 32;
                zwp_linux_dmabuf_v1_send_modifier(resource,
                                                  formats[i],
                                                  modifier_hi,
                                                  modifier_lo);
            } else if (modifiers[j] == DRM_FORMAT_MOD_LINEAR ||
                       modifiers == &modifier_invalid) {
                zwp_linux_dmabuf_v1_send_format(resource, formats[i]);
            }
        }
    }
}

/** Advertise linux_dmabuf support.
 *
 * Calling this initializes the zwp_linux_dmabuf protocol support, so that
 * the interface will be advertised to clients. Essentially it creates a
 * global.
 */
bool
linux_dmabuf_setup(struct wl_display *wl_display, EGLDisplay egl_display)
{
    assert(wl_display && egl_display);

    if (linux_dmabuf_data.wl_display)
        assert(!"Linux-dmabuf has already been initialized");

    /* Check for supported EGL extensions. */
    const char *egl_exts = eglQueryString(egl_display, EGL_EXTENSIONS);
    assert(egl_exts);

    if (strstr(egl_exts, "EGL_EXT_image_dma_buf_import") == NULL ||
        strstr(egl_exts, "EGL_EXT_image_dma_buf_import_modifiers") == NULL) {
        /* EGL implementation doesn't support importing DMA buffers, bailing. */
        return false;
    }

    /* Load extension's function pointers. */
    linux_dmabuf_data.eglQueryDmaBufFormatsEXT =
        (PFNEGLQUERYDMABUFFORMATSEXTPROC) eglGetProcAddress("eglQueryDmaBufFormatsEXT");
    assert(linux_dmabuf_data.eglQueryDmaBufFormatsEXT);

    linux_dmabuf_data.eglQueryDmaBufModifiersEXT =
        (PFNEGLQUERYDMABUFMODIFIERSEXTPROC) eglGetProcAddress("eglQueryDmaBufModifiersEXT");
    assert(linux_dmabuf_data.eglQueryDmaBufModifiersEXT);

    linux_dmabuf_data.wl_display = wl_display;
    linux_dmabuf_data.egl_display = egl_display;

    wl_list_init(&linux_dmabuf_data.dmabuf_buffers);

    if (!wl_global_create(wl_display,
                          &zwp_linux_dmabuf_v1_interface, 3,
                          NULL, bind_linux_dmabuf)) {
        return false;
    }

    return true;
}

void
linux_dmabuf_teardown(void)
{
    if (!linux_dmabuf_data.wl_display)
        return;

    struct linux_dmabuf_buffer *buffer;
    struct linux_dmabuf_buffer *tmp;
    wl_list_for_each_safe(buffer, tmp, &linux_dmabuf_data.dmabuf_buffers, link) {
        assert(buffer);

        wl_list_remove(&buffer->link);
        linux_dmabuf_buffer_destroy(buffer);
    }
}

const struct linux_dmabuf_buffer *
linux_dmabuf_get_buffer(struct wl_resource *buffer_resource)
{
    assert(buffer_resource);

    if (!linux_dmabuf_data.wl_display)
        return NULL;

    struct linux_dmabuf_buffer *buffer;
    wl_list_for_each(buffer, &linux_dmabuf_data.dmabuf_buffers, link) {
        assert(buffer);
        if (buffer->buffer_resource == buffer_resource)
            return buffer;
    }

    return NULL;
}

const struct linux_dmabuf_attributes *
linux_dmabuf_get_buffer_attributes(const struct linux_dmabuf_buffer *buffer)
{
    assert(buffer);

    return (const struct linux_dmabuf_attributes *) &buffer->attributes;
}
