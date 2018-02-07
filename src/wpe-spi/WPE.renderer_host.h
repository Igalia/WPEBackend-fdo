#pragma once

extern "C" {

typedef void*(*PFN_WPE_SPI_WHAT)();

#define PFN_WPE_SPI_RENDERER_HOST_CREATE (struct wpe_renderer_host*(*)())
extern PFN_WPE_SPI_WHAT __wpe_spi_client_renderer_host_create;

static inline struct wpe_renderer_host*
wpe_renderer_host_create()
{
    return (PFN_WPE_SPI_RENDERER_HOST_CREATE(__wpe_spi_client_renderer_host_create()))();
}

#define PFN_WPE_SPI_RENDERER_HOST_CREATE_CLIENT (int(*)(struct wpe_renderer_host*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_renderer_host_create_client;

static inline int
wpe_renderer_host_create_client(struct wpe_renderer_host* host)
{
    return (PFN_WPE_SPI_RENDERER_HOST_CREATE_CLIENT(__wpe_spi_client_renderer_host_create_client()))(host);
}

}
