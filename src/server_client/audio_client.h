/**
 * @file audio_client.h
 * @brief HTTP Audio Streaming Client Header
 * 
 * Defines the interface for the HTTP audio streaming client that
 * connects to Python audio server and receives audio streams.
 */

#ifndef AUDIO_CLIENT_H
#define AUDIO_CLIENT_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Audio client states
 */
typedef enum {
    AUDIO_CLIENT_DISCONNECTED,
    AUDIO_CLIENT_INITIALIZED,
    AUDIO_CLIENT_CONNECTED,
    AUDIO_CLIENT_STREAMING,
    AUDIO_CLIENT_ERROR
} audio_client_state_t;

/**
 * @brief Audio client commands
 */
typedef enum {
    AUDIO_CLIENT_CMD_PLAY,
    AUDIO_CLIENT_CMD_PAUSE,
    AUDIO_CLIENT_CMD_STOP,
    AUDIO_CLIENT_CMD_VOLUME,
    AUDIO_CLIENT_CMD_NEXT,
    AUDIO_CLIENT_CMD_PREV
} audio_client_command_t;

/**
 * @brief Initialize the audio client
 * 
 * @param server_host Server hostname or IP address
 * @param server_port Server port number
 * @return 0 on success, negative error code on failure
 */
int audio_client_init(const char *server_host, uint16_t server_port);

/**
 * @brief Connect to the audio server
 * 
 * @return 0 on success, negative error code on failure
 */
int audio_client_connect(void);

/**
 * @brief Start streaming audio from server
 * 
 * @param track_path Path to the track on server (optional)
 * @return 0 on success, negative error code on failure
 */
int audio_client_start_stream(const char *track_path);

/**
 * @brief Stop audio streaming
 * 
 * @return 0 on success, negative error code on failure
 */
int audio_client_stop_stream(void);

/**
 * @brief Send control command to server
 * 
 * @param cmd Command to send
 * @param param Optional parameter for command (e.g., volume level)
 * @return 0 on success, negative error code on failure
 */
int audio_client_send_command(audio_client_command_t cmd, const char *param);

/**
 * @brief Get current client state
 * 
 * @return Current client state
 */
audio_client_state_t audio_client_get_state(void);

/**
 * @brief Disconnect from server
 * 
 * @return 0 on success, negative error code on failure
 */
int audio_client_disconnect(void);

/**
 * @brief Cleanup client resources
 */
void audio_client_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_CLIENT_H */
