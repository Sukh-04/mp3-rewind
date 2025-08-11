/**
 * @file gatt_audio_service.h
 * @brief Custom BLE GATT Audio Service for MP3 Rewind
 * 
 * This service enables streaming PCM audio data over Bluetooth LE
 * to connected audio devices. It defines custom GATT characteristics
 * for audio data transmission and control.
 * 
 * Service Design:
 * - Audio Data Characteristic: Streams PCM audio chunks
 * - Audio Control Characteristic: Volume, play/pause, etc.
 * - Audio Info Characteristic: Format information (sample rate, channels, etc.)
 * 
 * Compatible with: ST B-L475E-IOT01A Discovery Board SPBTLE-RF module
 * 
 * Some of this information can also be found in the gatt_audio_service.c file.
 * But have a look for more details!
 */

#ifndef GATT_AUDIO_SERVICE_H
#define GATT_AUDIO_SERVICE_H

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Custom Audio Service UUID: 12345678-1234-5678-9ABC-DEF012345678 */
#define BT_UUID_AUDIO_SERVICE_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x9ABC, 0xDEF012345678)

/* Audio Data Characteristic UUID: 12345679-1234-5678-9ABC-DEF012345678 */
#define BT_UUID_AUDIO_DATA_VAL \
    BT_UUID_128_ENCODE(0x12345679, 0x1234, 0x5678, 0x9ABC, 0xDEF012345678)

/* Audio Control Characteristic UUID: 1234567A-1234-5678-9ABC-DEF012345678 */
#define BT_UUID_AUDIO_CONTROL_VAL \
    BT_UUID_128_ENCODE(0x1234567A, 0x1234, 0x5678, 0x9ABC, 0xDEF012345678)

/* Audio Info Characteristic UUID: 1234567B-1234-5678-9ABC-DEF012345678 */
#define BT_UUID_AUDIO_INFO_VAL \
    BT_UUID_128_ENCODE(0x1234567B, 0x1234, 0x5678, 0x9ABC, 0xDEF012345678)

/* Define the UUIDs */
#define BT_UUID_AUDIO_SERVICE   BT_UUID_DECLARE_128(BT_UUID_AUDIO_SERVICE_VAL)
#define BT_UUID_AUDIO_DATA      BT_UUID_DECLARE_128(BT_UUID_AUDIO_DATA_VAL)
#define BT_UUID_AUDIO_CONTROL   BT_UUID_DECLARE_128(BT_UUID_AUDIO_CONTROL_VAL)
#define BT_UUID_AUDIO_INFO      BT_UUID_DECLARE_128(BT_UUID_AUDIO_INFO_VAL)

/* Audio format structure */
typedef struct {
    uint32_t sample_rate;    /* Sample rate (e.g., 44100) */
    uint16_t channels;       /* Number of channels (1=mono, 2=stereo) */
    uint16_t bits_per_sample; /* Bits per sample (8, 16, 24, 32) */
    uint16_t frame_size;     /* Bytes per frame (channels * bits_per_sample / 8) */
} __packed gatt_audio_format_t;

/* Audio control commands */
typedef enum {
    AUDIO_CMD_PLAY = 0x01,
    AUDIO_CMD_PAUSE = 0x02,
    AUDIO_CMD_STOP = 0x03,
    AUDIO_CMD_VOLUME = 0x04,
    AUDIO_CMD_MUTE = 0x05,
    AUDIO_CMD_UNMUTE = 0x06
} gatt_audio_command_t;

/* Audio control structure */
typedef struct {
    uint8_t command;         /* Command from gatt_audio_command_t */
    uint8_t volume;          /* Volume level 0-100 (for AUDIO_CMD_VOLUME) */
    uint8_t reserved[2];     /* Reserved for future use */
} __packed gatt_audio_control_t;

/* Maximum audio chunk size for BLE transmission */
#define GATT_AUDIO_CHUNK_SIZE_MAX    244  /* MTU 247 - 3 bytes for ATT header */
#define GATT_AUDIO_CHUNK_SIZE_DEFAULT 128  /* Conservative size for compatibility */

/**
 * @brief Initialize GATT Audio Service
 * 
 * This function registers the custom audio service with the Bluetooth stack
 * and sets up the necessary characteristics for audio streaming.
 * 
 * @return 0 on success, negative error code on failure
 */
int gatt_audio_service_init(void);

/**
 * @brief Send audio data through GATT characteristic
 * 
 * This function sends PCM audio data to the connected BLE device
 * through the audio data characteristic. The data is sent as notifications
 * to minimize latency.
 * 
 * @param data Pointer to PCM audio data
 * @param len Length of audio data (max GATT_AUDIO_CHUNK_SIZE_MAX)
 * @param conn Bluetooth connection handle
 * @return Number of bytes sent, negative error code on failure
 */
int gatt_audio_send_data(const uint8_t *data, size_t len, struct bt_conn *conn);

/**
 * @brief Set audio format information
 * 
 * This function updates the audio info characteristic with the current
 * audio format parameters (sample rate, channels, bit depth).
 * 
 * @param format Audio format structure
 * @param conn Bluetooth connection handle  
 * @return 0 on success, negative error code on failure
 */
int gatt_audio_set_format(const gatt_audio_format_t *format, struct bt_conn *conn);

/**
 * @brief Check if audio client is subscribed to notifications
 * 
 * @param conn Bluetooth connection handle
 * @return true if subscribed to audio data notifications, false otherwise
 */
bool gatt_audio_is_subscribed(struct bt_conn *conn);

/**
 * @brief Get maximum supported chunk size for current connection
 * 
 * This function calculates the optimal chunk size based on the
 * negotiated MTU size with the connected device.
 * 
 * @param conn Bluetooth connection handle
 * @return Maximum chunk size in bytes
 */
size_t gatt_audio_get_max_chunk_size(struct bt_conn *conn);

/**
 * @brief Audio control callback function type
 * 
 * This callback is called when the client sends audio control commands
 * (play, pause, volume change, etc.)
 * 
 * @param control Audio control structure with command and parameters
 */
typedef void (*gatt_audio_control_cb_t)(const gatt_audio_control_t *control);

/**
 * @brief Register audio control callback
 * 
 * @param callback Callback function for audio control commands
 * @return 0 on success, negative error code on failure
 */
int gatt_audio_register_control_callback(gatt_audio_control_cb_t callback);

#ifdef __cplusplus
}
#endif

#endif /* GATT_AUDIO_SERVICE_H */
