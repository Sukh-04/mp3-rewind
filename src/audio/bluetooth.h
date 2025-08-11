/**
 * @file bluetooth.h  
 * @brief Bluetooth A2DP Audio Output Header
 * 
 * Interface for Bluetooth A2DP audio streaming to wireless headphones.
 * This provides the Bluetooth audio output implementation for audiosys.h
 * 
 * Please see bluetooth.c for more details on the implementation.
 */

#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "audiosys.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Bluetooth audio system
 * 
 * @param format Audio format configuration  
 * @return 0 on success, negative error code on failure
 */
int bluetooth_audio_init(const audio_format_t *format);

/**
 * @brief Start Bluetooth audio streaming
 * 
 * @return 0 on success, negative error code on failure
 */
int bluetooth_audio_start(void);

/**
 * @brief Stop Bluetooth audio streaming
 * 
 * @return 0 on success, negative error code on failure  
 */
int bluetooth_audio_stop(void);

/**
 * @brief Write audio data to Bluetooth output
 * 
 * @param data Pointer to audio data
 * @param len Length of audio data in bytes
 * @return Number of bytes written, negative on error
 */
int bluetooth_audio_write(const uint8_t *data, size_t len);

/**
 * @brief Set Bluetooth audio volume
 * 
 * @param volume Volume level (0-100)
 * @return 0 on success, negative error code on failure
 */
int bluetooth_audio_set_volume(uint8_t volume);

/**
 * @brief Check if Bluetooth device is connected
 * 
 * @return true if connected, false otherwise
 */
bool bluetooth_audio_is_connected(void);

/**
 * @brief Check if Bluetooth audio is streaming
 * 
 * @return true if streaming, false otherwise  
 */
bool bluetooth_audio_is_streaming(void);

/**
 * @brief Get available buffer space
 * 
 * @return Available space in bytes
 */
size_t bluetooth_audio_get_free_space(void);

/**
 * @brief Start device discovery for pairing
 * 
 * @return 0 on success, negative error code on failure
 */
int bluetooth_audio_discover_devices(void);

/**
 * @brief Cleanup Bluetooth audio system
 * 
 * @return 0 on success, negative error code on failure
 */
int bluetooth_audio_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* BLUETOOTH_H */
