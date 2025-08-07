/**
 * @file audiosys.c
 * @brief Audio System Abstraction Layer Implementation
 * 
 * This implements the unified audio interface that routes calls to the
 * appropriate backend (Bluetooth A2DP or PWM buzzer) based on configuration.
 */

#include "audiosys.h"
#include "bluetooth.h"
#include "../utils/error_handling.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(audiosys, LOG_LEVEL_DBG);

/* Global audio system state */
static struct {
    bool initialized;
    audio_config_t config;
    audio_state_t state;
} audio_system = {
    .initialized = false,
    .state = AUDIO_STATE_UNINITIALIZED
};

/* External function prototypes for buzzer implementation */
extern int audioplay_buzzer_init(const audio_format_t *format);
extern int audioplay_buzzer_start(void);
extern int audioplay_buzzer_stop(void);
extern int audioplay_buzzer_write(const uint8_t *data, size_t len);
extern int audioplay_buzzer_set_volume(uint8_t volume);
extern size_t audioplay_buzzer_get_free_space(void);
extern int audioplay_buzzer_cleanup(void);

int audio_system_init(const audio_config_t *config)
{
    int ret;
    
    if (!config) {
        LOG_ERR("Invalid configuration");
        return -EINVAL;
    }
    
    if (audio_system.initialized) {
        LOG_WRN("Audio system already initialized");
        return 0;
    }
    
    /* Store configuration */
    audio_system.config = *config;
    
    LOG_INF("Initializing audio system with %s output", 
            (config->output_type == AUDIO_OUTPUT_BLUETOOTH) ? "Bluetooth" : "Buzzer");
    
    /* Initialize the selected audio backend */
    switch (config->output_type) {
        case AUDIO_OUTPUT_BLUETOOTH:
            ret = bluetooth_audio_init(&config->format);
            break;
            
        case AUDIO_OUTPUT_BUZZER:
            ret = audioplay_buzzer_init(&config->format);
            break;
            
        default:
            LOG_ERR("Unsupported audio output type: %d", config->output_type);
            return -ENOTSUP;
    }
    
    if (ret < 0) {
        LOG_ERR("Audio backend initialization failed: %d", ret);
        audio_system.state = AUDIO_STATE_ERROR;
        return ret;
    }
    
    audio_system.initialized = true;
    audio_system.state = AUDIO_STATE_INITIALIZED;
    
    LOG_INF("Audio system initialized successfully");
    return 0;
}

int audio_system_start(void)
{
    if (!audio_system.initialized) {
        LOG_ERR("Audio system not initialized");
        return -EINVAL;
    }
    
    if (audio_system.state == AUDIO_STATE_PLAYING) {
        LOG_WRN("Audio system already playing");
        return 0;
    }
    
    int ret;
    
    switch (audio_system.config.output_type) {
        case AUDIO_OUTPUT_BLUETOOTH:
            ret = bluetooth_audio_start();
            break;
            
        case AUDIO_OUTPUT_BUZZER:
            ret = audioplay_buzzer_start();
            break;
            
        default:
            LOG_ERR("Unsupported audio output type");
            return -ENOTSUP;
    }
    
    if (ret < 0) {
        LOG_ERR("Failed to start audio playback: %d", ret);
        audio_system.state = AUDIO_STATE_ERROR;
        return ret;
    }
    
    audio_system.state = AUDIO_STATE_PLAYING;
    LOG_INF("Audio playback started");
    return 0;
}

int audio_system_stop(void)
{
    if (!audio_system.initialized) {
        return -EINVAL;
    }
    
    int ret;
    
    switch (audio_system.config.output_type) {
        case AUDIO_OUTPUT_BLUETOOTH:
            ret = bluetooth_audio_stop();
            break;
            
        case AUDIO_OUTPUT_BUZZER:
            ret = audioplay_buzzer_stop();
            break;
            
        default:
            return -ENOTSUP;
    }
    
    if (ret >= 0) {
        audio_system.state = AUDIO_STATE_INITIALIZED;
        LOG_INF("Audio playback stopped");
    }
    
    return ret;
}

int audio_system_pause(void)
{
    /* For now, just stop - can be enhanced later */
    return audio_system_stop();
}

int audio_system_resume(void)
{
    /* For now, just start - can be enhanced later */
    return audio_system_start();
}

int audio_system_write(const uint8_t *data, size_t len)
{
    if (!audio_system.initialized) {
        return -EINVAL;
    }
    
    if (!data || len == 0) {
        return -EINVAL;
    }
    
    switch (audio_system.config.output_type) {
        case AUDIO_OUTPUT_BLUETOOTH:
            return bluetooth_audio_write(data, len);
            
        case AUDIO_OUTPUT_BUZZER:
            return audioplay_buzzer_write(data, len);
            
        default:
            return -ENOTSUP;
    }
}

int audio_system_set_volume(uint8_t volume)
{
    if (!audio_system.initialized) {
        return -EINVAL;
    }
    
    if (volume > 100) {
        return -EINVAL;
    }
    
    switch (audio_system.config.output_type) {
        case AUDIO_OUTPUT_BLUETOOTH:
            return bluetooth_audio_set_volume(volume);
            
        case AUDIO_OUTPUT_BUZZER:
            return audioplay_buzzer_set_volume(volume);
            
        default:
            return -ENOTSUP;
    }
}

audio_state_t audio_system_get_state(void)
{
    return audio_system.state;
}

size_t audio_system_get_free_space(void)
{
    if (!audio_system.initialized) {
        return 0;
    }
    
    switch (audio_system.config.output_type) {
        case AUDIO_OUTPUT_BLUETOOTH:
            return bluetooth_audio_get_free_space();
            
        case AUDIO_OUTPUT_BUZZER:
            return audioplay_buzzer_get_free_space();
            
        default:
            return 0;
    }
}

int audio_system_cleanup(void)
{
    if (!audio_system.initialized) {
        return 0;
    }
    
    int ret;
    
    switch (audio_system.config.output_type) {
        case AUDIO_OUTPUT_BLUETOOTH:
            ret = bluetooth_audio_cleanup();
            break;
            
        case AUDIO_OUTPUT_BUZZER:
            ret = audioplay_buzzer_cleanup();
            break;
            
        default:
            ret = 0;
    }
    
    audio_system.initialized = false;
    audio_system.state = AUDIO_STATE_UNINITIALIZED;
    
    LOG_INF("Audio system cleaned up");
    return ret;
}
