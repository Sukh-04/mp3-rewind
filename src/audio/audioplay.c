/**
 * @file audioplay.c
 * @brief Audio Playback Implementation using PWM/Buzzer Output
 * 
 * This implementation provides audio output through PWM-driven buzzers or speakers.
 * It's designed as the output method before Bluetooth implementation.
 * 
 * Please note this this implementation is created for debugging and testing purposes.
 * The final audio output will be through Bluetooth A2DP, but this provides a way to
 * validate the audio pipeline and ensure the system is working before moving to
 * the more complex Bluetooth streaming. 
 * 
 * The file was created using the UM2153 User Manual for the B-L475E-IOT01A discovery board, 
 * as well as the Zephyr documentation for audio and PWM subsystems, and Claude Sonnet 4 
 * (used as a reference for troubleshooting and debugging).
 */

#include "audiosys.h"
#include "../utils/circular_buffers.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(audioplay, LOG_LEVEL_DBG);

/* PWM configuration for buzzer - using PWM2 Channel 1 on PA15 (Arduino D9) */
#define PWM_DEVICE_NODE DT_NODELABEL(pwm2)
#define PWM_CHANNEL 1
#define PWM_PERIOD_NS 250000  // 4kHz period (1/4000 * 1e9) - audible frequency range

/* Audio buffer configuration - reduced size to prevent memory issues */
#define AUDIO_BUFFER_SIZE (2048)     // 2KB buffer (reduced from 8KB)
#define AUDIO_CHUNK_SIZE 256         // Process in 256-byte chunks

/* Audio playback thread configuration */
#define AUDIO_THREAD_STACK_SIZE 2048
#define AUDIO_THREAD_PRIORITY 5

/* Thread stack definition (must be global) */
K_THREAD_STACK_DEFINE(audio_thread_stack, AUDIO_THREAD_STACK_SIZE);

/* Audio system state */
static struct {
    const struct device *pwm_dev;
    audio_config_t config;
    audio_state_t state;
    uint8_t volume;
    
    /* Audio buffer */
    circular_buffer_t audio_buffer;
    uint8_t buffer_memory[AUDIO_BUFFER_SIZE];
    
    /* Threading */
    struct k_thread audio_thread;
    bool thread_running;
    
    /* Audio filtering */
    uint16_t filter_prev_sample;  // Previous sample for simple low-pass filter
    
} audio_ctx = {
    .state = AUDIO_STATE_UNINITIALIZED,
    .volume = 50,
    .thread_running = false,
    .filter_prev_sample = 32768  // Initialize to midpoint
};

/* Forward declarations */
static void audio_playback_thread(void *arg1, void *arg2, void *arg3);
static int pwm_play_sample(uint16_t sample);

int audio_system_init(const audio_config_t *config)
{
    if (!config) {
        LOG_ERR("Invalid configuration");
        return -EINVAL;
    }

    if (audio_ctx.state != AUDIO_STATE_UNINITIALIZED) {
        LOG_WRN("Audio system already initialized");
        return -EALREADY;
    }

    /* Check if this is buzzer output type */
    if (config->output_type != AUDIO_OUTPUT_BUZZER) {
        LOG_ERR("This implementation only supports buzzer output");
        return -ENOTSUP;
    }

    /* Get PWM device */
    audio_ctx.pwm_dev = DEVICE_DT_GET(PWM_DEVICE_NODE);
    if (!device_is_ready(audio_ctx.pwm_dev)) {
        LOG_ERR("PWM device not ready");
        return -ENODEV;
    }

    /* Initialize audio buffer */
    int ret = circular_buffer_init(&audio_ctx.audio_buffer, 
                                   audio_ctx.buffer_memory, 
                                   AUDIO_BUFFER_SIZE);
    if (ret < 0) {
        LOG_ERR("Failed to initialize audio buffer: %d", ret);
        return ret;
    }

    /* Store configuration */
    memcpy(&audio_ctx.config, config, sizeof(audio_config_t));
    audio_ctx.state = AUDIO_STATE_INITIALIZED;
    
    LOG_INF("Audio system initialized (PWM/Buzzer mode)");
    LOG_INF("Sample rate: %u Hz, Channels: %u, Bits: %u", 
            config->format.sample_rate, 
            config->format.channels, 
            config->format.bits_per_sample);
    
    return 0;
}

int audio_system_start(void)
{
    if (audio_ctx.state != AUDIO_STATE_INITIALIZED && 
        audio_ctx.state != AUDIO_STATE_PAUSED) {
        LOG_ERR("Cannot start audio: invalid state %d", audio_ctx.state);
        return -EINVAL;
    }

    if (!audio_ctx.thread_running) {
        /* Start audio playback thread */
        k_thread_create(&audio_ctx.audio_thread,
                        audio_thread_stack,
                        K_THREAD_STACK_SIZEOF(audio_thread_stack),
                        audio_playback_thread,
                        NULL, NULL, NULL,
                        AUDIO_THREAD_PRIORITY, 0, K_NO_WAIT);
        
        k_thread_name_set(&audio_ctx.audio_thread, "audio_play");
        audio_ctx.thread_running = true;
    }

    audio_ctx.state = AUDIO_STATE_PLAYING;
    LOG_INF("Audio playback started");
    
    return 0;
}

int audio_system_stop(void)
{
    if (audio_ctx.state != AUDIO_STATE_PLAYING && 
        audio_ctx.state != AUDIO_STATE_PAUSED) {
        return 0; // Already stopped
    }

    audio_ctx.state = AUDIO_STATE_INITIALIZED;
    
    /* Stop PWM output */
    pwm_set(audio_ctx.pwm_dev, PWM_CHANNEL, PWM_PERIOD_NS, 0, 0);
    
    LOG_INF("Audio playback stopped");
    return 0;
}

int audio_system_pause(void)
{
    if (audio_ctx.state != AUDIO_STATE_PLAYING) {
        return -EINVAL;
    }

    audio_ctx.state = AUDIO_STATE_PAUSED;
    LOG_INF("Audio playback paused");
    
    return 0;
}

int audio_system_resume(void)
{
    if (audio_ctx.state != AUDIO_STATE_PAUSED) {
        return -EINVAL;
    }

    audio_ctx.state = AUDIO_STATE_PLAYING;
    LOG_INF("Audio playback resumed");
    
    return 0;
}

int audio_system_write(const uint8_t *data, size_t len)
{
    if (!data || len == 0) {
        return -EINVAL;
    }

    if (audio_ctx.state == AUDIO_STATE_UNINITIALIZED) {
        LOG_ERR("Audio system not initialized");
        return -EINVAL;
    }

    /* Check if buffer is getting too full to prevent memory corruption */
    size_t current_size = circular_buffer_size_get(&audio_ctx.audio_buffer);
    size_t available_space = circular_buffer_space_get(&audio_ctx.audio_buffer);
    size_t total_capacity = current_size + available_space;
    
    if (current_size > (total_capacity * 3 / 4)) {
        LOG_WRN("Audio buffer %zu%% full (%zu/%zu), dropping data to prevent overflow", 
                (current_size * 100) / total_capacity, current_size, total_capacity);
        return 0; // Drop the data to prevent crash
    }

    /* Write data to circular buffer with timeout */
    size_t written = circular_buffer_write_timeout(&audio_ctx.audio_buffer, 
                                                   data, len, 
                                                   K_MSEC(10)); // Reduced timeout
    
    if (written != len) {
        LOG_WRN("Audio buffer full, wrote %zu/%zu bytes", written, len);
    } else {
        LOG_DBG("Audio data written: %zu bytes, buffer now has %zu bytes", 
                written, circular_buffer_size_get(&audio_ctx.audio_buffer));
    }

    return (int)written;
}

int audio_system_set_volume(uint8_t volume)
{
    if (volume > 100) {
        return -EINVAL;
    }

    audio_ctx.volume = volume;
    LOG_DBG("Volume set to %u%%", volume);
    
    return 0;
}

audio_state_t audio_system_get_state(void)
{
    return audio_ctx.state;
}

size_t audio_system_get_free_space(void)
{
    return circular_buffer_space_get(&audio_ctx.audio_buffer);
}

int audio_system_cleanup(void)
{
    /* Stop playback */
    audio_system_stop();
    
    /* Stop thread */
    if (audio_ctx.thread_running) {
        k_thread_abort(&audio_ctx.audio_thread);
        audio_ctx.thread_running = false;
    }
    
    /* Clear buffer */
    circular_buffer_cleanup(&audio_ctx.audio_buffer);
    
    audio_ctx.state = AUDIO_STATE_UNINITIALIZED;
    LOG_INF("Audio system cleaned up");
    
    return 0;
}

/* Audio playback thread - reads from buffer and outputs to PWM */
static void audio_playback_thread(void *arg1, void *arg2, void *arg3)
{
    uint8_t sample_buffer[4]; // Process 2 samples (4 bytes) at a time for 16-bit audio
    uint64_t next_sample_time = k_uptime_get(); // Precise timing
    const uint32_t sample_interval_us = 1000000 / audio_ctx.config.format.sample_rate; // 22.67 μs for 44.1kHz
    
    LOG_INF("Audio playback thread started");
    LOG_DBG("Sample interval: %u μs (rate: %u Hz)", sample_interval_us, audio_ctx.config.format.sample_rate);
    LOG_INF("Thread running flag: %s", audio_ctx.thread_running ? "true" : "false");
    
    /* Ensure thread doesn't exit immediately */
    if (!audio_ctx.thread_running) {
        LOG_ERR("Thread running flag is false at start!");
        audio_ctx.thread_running = true; // Force it to true
    }
    
    int loop_count = 0;
    while (audio_ctx.thread_running) {
        loop_count++;
        
        /* Debug: Log every 1000 loops to show thread is alive */
        if (loop_count % 1000 == 0) {
            LOG_INF("Audio thread loop %d, state: %d", loop_count, audio_ctx.state);
        }
        
        if (audio_ctx.state != AUDIO_STATE_PLAYING) {
            /* Not playing, just sleep */
            k_sleep(K_MSEC(10));
            next_sample_time = k_uptime_get(); // Reset timing
            continue;
        }
        
        /* Read one sample at a time from buffer */
        size_t bytes_to_read = audio_ctx.config.format.bits_per_sample / 8; // 2 bytes for 16-bit
        size_t bytes_read = circular_buffer_read_timeout(&audio_ctx.audio_buffer,
                                                        sample_buffer,
                                                        bytes_to_read,
                                                        K_MSEC(10));
        
        if (bytes_read == 0) {
            /* No data available, short sleep and continue */
            static int no_data_count = 0;
            if (++no_data_count % 1000 == 0) {
                LOG_DBG("No audio data available, buffer used: %zu bytes", 
                        circular_buffer_size_get(&audio_ctx.audio_buffer));
            }
            k_sleep(K_USEC(100));
            continue;
        }
        
        /* Reset no-data counter when we get data */
        static int sample_count = 0;
        if (++sample_count % 1000 == 0) {
            LOG_DBG("Processed %d samples, buffer used: %zu bytes", 
                    sample_count, circular_buffer_size_get(&audio_ctx.audio_buffer));
        }
        
        if (bytes_read == bytes_to_read) {
            uint16_t sample;
            
            if (audio_ctx.config.format.bits_per_sample == 16) {
                /* 16-bit samples - use proper byte order */
                int16_t signed_sample = ((int16_t)sample_buffer[1] << 8) | sample_buffer[0];
                
                /* Convert signed 16-bit (-32768 to +32767) to unsigned 16-bit (0 to 65535) */
                sample = (uint16_t)(signed_sample + 32768);
                
                /* Debug: Log sample values occasionally */
                static int debug_count = 0;
                if (++debug_count % 2000 == 0) {
                    LOG_DBG("Sample debug: signed=%d, unsigned=%u, volume=%u", 
                            signed_sample, sample, audio_ctx.volume);
                }
            } else {
                /* 8-bit samples, convert to 16-bit */
                sample = sample_buffer[0] << 8;
            }
            
            /* Apply volume control */
            sample = (sample * audio_ctx.volume) / 100;
            
            /* Apply simple low-pass filter to smooth audio output
             * For buzzer output, use less filtering to maintain responsiveness
             * Simple RC-like filter: output = alpha * input + (1-alpha) * prev_output
             * Alpha = 0.8 gives good balance for buzzer (more responsive than 0.7)
             */
            uint16_t filtered_sample = (sample * 8 + audio_ctx.filter_prev_sample * 2) / 10;
            audio_ctx.filter_prev_sample = filtered_sample;
            
            /* Output sample via PWM */
            int ret = pwm_play_sample(filtered_sample);
            if (ret < 0) {
                LOG_ERR("PWM output failed: %d", ret);
            }
            
            /* Precise timing - wait until it's time for next sample */
            next_sample_time += sample_interval_us;
            uint64_t current_time = k_uptime_get() * 1000; // Convert to μs
            
            if (next_sample_time > current_time) {
                uint32_t sleep_time = (next_sample_time - current_time);
                if (sleep_time > 0 && sleep_time < 1000) { // Only sleep if < 1ms
                    k_busy_wait(sleep_time);
                }
            }
        }
    }
    
    LOG_INF("Audio playback thread stopped");
}

/* Convert audio sample to PWM pulse width */
static int pwm_play_sample(uint16_t sample)
{
    /* Convert 16-bit sample to PWM duty cycle 
     * For buzzer audio, we want more dramatic duty cycle changes
     * Map 0-65535 sample to 10%-90% duty cycle for maximum buzzer response and volume
     */
    uint32_t min_pulse = PWM_PERIOD_NS / 10;  // 10% duty cycle minimum
    uint32_t max_pulse = (PWM_PERIOD_NS * 9) / 10;  // 90% duty cycle maximum
    uint32_t pulse_range = max_pulse - min_pulse;
    
    uint32_t pulse_width = min_pulse + ((sample * pulse_range) / 65535);
    
    /* Debug: Log PWM values occasionally */
    static int pwm_debug_count = 0;
    if (++pwm_debug_count % 2000 == 0) {
        LOG_DBG("PWM debug: sample=%u, pulse_width=%u ns, duty=%.1f%%", 
                sample, pulse_width, (float)pulse_width * 100.0f / PWM_PERIOD_NS);
    }
    
    /* Use the same PWM API that worked in direct test */
    return pwm_set(audio_ctx.pwm_dev, PWM_CHANNEL, PWM_PERIOD_NS, pulse_width, 0);
}
