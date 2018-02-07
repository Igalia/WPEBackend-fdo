#pragma once

extern "C" {

typedef void*(*PFN_WPE_SPI_WHAT)();

#define PFN_WPE_SPI_INPUT_KEY_MAPPER_CREATE (struct wpe_input_key_mapper*(*)())
extern PFN_WPE_SPI_WHAT __wpe_spi_client_input_key_mapper_create;

static inline struct wpe_input_key_mapper*
wpe_input_key_mapper_create()
{
    return (PFN_WPE_SPI_INPUT_KEY_MAPPER_CREATE(__wpe_spi_client_input_key_mapper_create()))();
}

#define PFN_WPE_SPI_INPUT_KEY_MAPPER_IDENTIFIER_FOR_KEY_EVENT (const char*(*)(struct wpe_input_key_mapper*, struct wpe_input_keyboard_event*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_input_key_mapper_identifier_for_key_event;

static inline const char*
wpe_input_key_mapper_identifier_for_key_event(struct wpe_input_key_mapper* mapper, struct wpe_input_keyboard_event* event)
{
    return (PFN_WPE_SPI_INPUT_KEY_MAPPER_IDENTIFIER_FOR_KEY_EVENT(__wpe_spi_client_input_key_mapper_identifier_for_key_event()))(mapper, event);
}

#define PFN_WPE_SPI_INPUT_KEY_MAPPER_WINDOWS_KEY_CODE_FOR_KEY_EVENT (int(*)(struct wpe_input_key_mapper*, struct wpe_input_keyboard_event*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_input_key_mapper_windows_key_code_for_key_event;

static inline int
wpe_input_key_mapper_windows_key_code_for_key_event(struct wpe_input_key_mapper* mapper, struct wpe_input_keyboard_event* event)
{
    return (PFN_WPE_SPI_INPUT_KEY_MAPPER_WINDOWS_KEY_CODE_FOR_KEY_EVENT(__wpe_spi_client_input_key_mapper_windows_key_code_for_key_event()))(mapper, event);
}

#define PFN_WPE_SPI_INPUT_KEY_MAPPER_SINGLE_CHARACTER_FOR_KEY_EVENT (const char*(*)(struct wpe_input_key_mapper*, struct wpe_input_keyboard_event*))
extern PFN_WPE_SPI_WHAT __wpe_spi_client_input_key_mapper_single_character_for_key_event;

static inline const char*
wpe_input_key_mapper_single_character_for_key_event(struct wpe_input_key_mapper* mapper, struct wpe_input_keyboard_event* event)
{
    return (PFN_WPE_SPI_INPUT_KEY_MAPPER_SINGLE_CHARACTER_FOR_KEY_EVENT(__wpe_spi_client_input_key_mapper_single_character_for_key_event()))(mapper, event);
}

}
