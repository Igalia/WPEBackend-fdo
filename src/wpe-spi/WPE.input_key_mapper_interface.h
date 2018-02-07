#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct wpe_input_keyboard_event;

struct wpe_input_key_mapper_interface {
    const char* (*identifier_for_key_event)(struct wpe_input_keyboard_event*);
    int (*windows_key_code_for_key_event)(struct wpe_input_keyboard_event*);
    const char* (*single_character_for_key_event)(struct wpe_input_keyboard_event*);
};

#ifdef __cplusplus
}
#endif
