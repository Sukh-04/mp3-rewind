/**
 * @file audiosys.h
 * @brief Audio System Abstraction Layer
 * 
 * This header provides a unified interface for different audio output methods:
 * - Buzzer/PWM output (Currently implemented for testing and debugging purposes).
 * - Bluetooth A2DP output (Planned for future implementation).
 * 
 * The abstraction allows switching between output methods without changing
 * the core audio pipeline implementation.
 */

#ifndef AUDIOSYS_H
#define AUDIOSYS_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Audio output types supported by the system
 */
typedef enum {
    AUDIO_OUTPUT_BUZZER,     ///< PWM-based buzzer output
    AUDIO_OUTPUT_BLUETOOTH   ///< Bluetooth A2DP output
} audio_output_type_t;

/**
 * @brief Audio format specification
 */
typedef struct {
    uint32_t sample_rate;    ///< Sample rate in Hz (e.g., 44100)
    uint16_t channels;       ///< Number of channels (1=mono, 2=stereo)
    uint16_t bits_per_sample; ///< Bits per sample (8, 16, 24, 32)
} audio_format_t;

/**
 * @brief Audio system configuration
 */
typedef struct {
    audio_output_type_t output_type;
    audio_format_t format;
    uint32_t buffer_size_ms;  ///< Buffer size in milliseconds
} audio_config_t;

/**
 * @brief Audio system state
 */
typedef enum {
    AUDIO_STATE_UNINITIALIZED,
    AUDIO_STATE_INITIALIZED,
    AUDIO_STATE_PLAYING,
    AUDIO_STATE_PAUSED,
    AUDIO_STATE_ERROR
} audio_state_t;

/**
 * @brief Initialize the audio system
 * 
 * @param config Audio system configuration
 * @return 0 on success, negative error code on failure
 */
int audio_system_init(const audio_config_t *config);

/**
 * @brief Start audio playback
 * 
 * @return 0 on success, negative error code on failure
 */
int audio_system_start(void);

/**
 * @brief Stop audio playback
 * 
 * @return 0 on success, negative error code on failure
 */
int audio_system_stop(void);

/**
 * @brief Pause audio playback
 * 
 * @return 0 on success, negative error code on failure
 */
int audio_system_pause(void);

/**
 * @brief Resume audio playback
 * 
 * @return 0 on success, negative error code on failure
 */
int audio_system_resume(void);

/**
 * @brief Write audio data to the output
 * 
 * @param data Pointer to audio data buffer
 * @param len Length of audio data in bytes
 * @return Number of bytes written, negative error code on failure
 */
int audio_system_write(const uint8_t *data, size_t len);

/**
 * @brief Set audio volume
 * 
 * @param volume Volume level (0-100)
 * @return 0 on success, negative error code on failure
 */
int audio_system_set_volume(uint8_t volume);

/**
 * @brief Get current audio system state
 * 
 * @return Current audio system state
 */
audio_state_t audio_system_get_state(void);

/**
 * @brief Get available buffer space
 * 
 * @return Available buffer space in bytes
 */
size_t audio_system_get_free_space(void);

/**
 * @brief Cleanup and deinitialize audio system
 * 
 * @return 0 on success, negative error code on failure
 */
int audio_system_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIOSYS_H */
