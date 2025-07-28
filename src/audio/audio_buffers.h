/**
 * @file audio_buffers.h
 * @brief Audio Buffer Management System Header
 * 
 * Defines structures and functions for managing audio buffers used in
 * streaming audio applications. 
 * 
 * The audio buffer system was implemented when researching into ways of 
 * streaming audio data efficiently between different components of the system. 
 * 
 * Please note the /utils/circular_buffers.h/.c files that were used as a reference to
 * create the respective audio_buffers.h/.c files. It is also worth mentioning that this
 * this my first time interacting with audio to this degree and there is a lot of room for 
 * impromentment but I am thorougly enjoying the proceess of working on this project.
 * 
 */

#ifndef AUDIO_BUFFERS_H
#define AUDIO_BUFFERS_H

#include <zephyr/kernel.h>
#include <zephyr/sys/slist.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Audio buffer flags
 */
typedef enum {
    AUDIO_BUFFER_FLAG_NONE = 0,
    AUDIO_BUFFER_FLAG_END_OF_STREAM = (1 << 0),
    AUDIO_BUFFER_FLAG_DISCONTINUITY = (1 << 1),
    AUDIO_BUFFER_FLAG_COMPRESSED = (1 << 2)
} audio_buffer_flags_t;

/**
 * @brief Audio buffer structure
 */
struct audio_buffer {
    sys_snode_t node;          ///< List node for buffer management
    uint8_t *data;             ///< Buffer data pointer
    size_t size;               ///< Total buffer size
    size_t used;               ///< Currently used bytes
    uint32_t sequence;         ///< Sequence number for ordering
    int64_t timestamp;         ///< Timestamp for synchronization
    audio_buffer_flags_t flags;///< Buffer flags
};

/**
 * @brief Audio buffer pool statistics
 */
struct audio_buffer_stats {
    uint32_t total_buffers;
    uint32_t free_buffers;
    uint32_t buffers_in_use;
    uint32_t buffers_allocated;
    uint32_t buffers_freed;
    uint32_t allocation_failures;
};

/**
 * @brief Initialize the audio buffer pool
 * 
 * @return 0 on success, negative error code on failure
 */
int audio_buffer_pool_init(void);

/**
 * @brief Allocate an audio buffer from the pool
 * 
 * @param timeout Timeout for allocation (K_NO_WAIT, K_FOREVER, or timeout)
 * @return Pointer to allocated buffer, NULL on failure
 */
struct audio_buffer *audio_buffer_alloc(k_timeout_t timeout);

/**
 * @brief Free an audio buffer back to the pool
 * 
 * @param buffer Buffer to free
 * @return 0 on success, negative error code on failure
 */
int audio_buffer_free(struct audio_buffer *buffer);

/**
 * @brief Write data to an audio buffer
 * 
 * @param buffer Target buffer
 * @param data Data to write
 * @param len Length of data to write
 * @return Number of bytes actually written
 */
size_t audio_buffer_write(struct audio_buffer *buffer, const uint8_t *data, size_t len);

/**
 * @brief Read data from an audio buffer
 * 
 * @param buffer Source buffer
 * @param data Buffer to read into
 * @param len Maximum bytes to read
 * @return Number of bytes actually read
 */
size_t audio_buffer_read(struct audio_buffer *buffer, uint8_t *data, size_t len);

/**
 * @brief Get available space in buffer
 * 
 * @param buffer Buffer to check
 * @return Available space in bytes
 */
size_t audio_buffer_get_free_space(struct audio_buffer *buffer);

/**
 * @brief Get used space in buffer
 * 
 * @param buffer Buffer to check
 * @return Used space in bytes
 */
size_t audio_buffer_get_used_space(struct audio_buffer *buffer);

/**
 * @brief Check if buffer is empty
 * 
 * @param buffer Buffer to check
 * @return true if empty, false otherwise
 */
bool audio_buffer_is_empty(struct audio_buffer *buffer);

/**
 * @brief Check if buffer is full
 * 
 * @param buffer Buffer to check
 * @return true if full, false otherwise
 */
bool audio_buffer_is_full(struct audio_buffer *buffer);

/**
 * @brief Clear all data from buffer
 * 
 * @param buffer Buffer to clear
 */
void audio_buffer_clear(struct audio_buffer *buffer);

/**
 * @brief Get buffer pool statistics
 * 
 * @param stats Pointer to stats structure to fill
 * @return 0 on success, negative error code on failure
 */
int audio_buffer_pool_get_stats(struct audio_buffer_stats *stats);

/**
 * @brief Cleanup buffer pool
 */
void audio_buffer_pool_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_BUFFERS_H */
