/**
 * @file audioplay_stubs.c
 * @brief Stub implementations for PWM/buzzer audio functions
 * 
 * These are minimal stub implementations to satisfy the linker when
 * the main focus is Bluetooth audio testing. The audiosys.c expects
 * these functions to exist as backend implementations.
 */

#include "audiosys.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(audioplay_stubs, LOG_LEVEL_DBG);

/* Stub implementations for PWM/buzzer backend */

int audioplay_buzzer_init(const audio_format_t *format)
{
    LOG_WRN("PWM/buzzer audio not implemented - using stub");
    return 0; // Return success for now
}

int audioplay_buzzer_start(void)
{
    LOG_WRN("PWM/buzzer start not implemented - using stub");
    return 0;
}

int audioplay_buzzer_stop(void)
{
    LOG_WRN("PWM/buzzer stop not implemented - using stub");
    return 0;
}

int audioplay_buzzer_write(const uint8_t *data, size_t len)
{
    LOG_WRN("PWM/buzzer write not implemented - using stub (ignoring %zu bytes)", len);
    return len; // Pretend we wrote all the data
}

int audioplay_buzzer_set_volume(uint8_t volume)
{
    LOG_WRN("PWM/buzzer volume control not implemented - using stub (volume=%d)", volume);
    return 0;
}

size_t audioplay_buzzer_get_free_space(void)
{
    LOG_WRN("PWM/buzzer free space not implemented - using stub");
    return 1024; // Return some reasonable buffer size
}
