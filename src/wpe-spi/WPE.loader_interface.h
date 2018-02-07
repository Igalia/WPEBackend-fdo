#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct wpe_loader_interface {
    void* (*load_object)(const char*);
};

#ifdef __cplusplus
}
#endif
