/**
 * @file circular_buffers.h
 * @brief Circular Buffer Implementation for Audio Streaming
 * 
 * Thread-safe circular buffer implementation optimized for audio data streaming.
 * Supports both blocking and non-blocking operations.
 * 
 * This resource is primarly used in /audio. Thread-safe design I first learned 
 * about in CSC369H1 however this implemention is heavily inspired by online resources
 * of alike implementations and modified by Claude Sonnet 4 to suit the needs of this project.
 * 
 */

#ifndef CIRCULAR_BUFFERS_H
#define CIRCULAR_BUFFERS_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Circular buffer structure
 */
typedef struct {
    uint8_t *buffer;           ///< Buffer data storage
    size_t size;               ///< Total buffer size
    volatile size_t head;      ///< Write position
    volatile size_t tail;      ///< Read position
    volatile size_t count;     ///< Current data count
    struct k_mutex mutex;      ///< Mutex for thread safety
    struct k_condvar not_full; ///< Condition variable for not full
    struct k_condvar not_empty;///< Condition variable for not empty
} circular_buffer_t;

/**
 * @brief Initialize a circular buffer
 * 
 * @param cb Pointer to circular buffer structure
 * @param buffer Pointer to buffer memory (must be allocated by caller)
 * @param size Size of the buffer in bytes
 * @return 0 on success, negative error code on failure
 */
int circular_buffer_init(circular_buffer_t *cb, uint8_t *buffer, size_t size);

/**
 * @brief Write data to circular buffer (non-blocking)
 * 
 * @param cb Pointer to circular buffer
 * @param data Pointer to data to write
 * @param len Length of data to write
 * @return Number of bytes actually written
 */
size_t circular_buffer_write(circular_buffer_t *cb, const uint8_t *data, size_t len);

/**
 * @brief Write data to circular buffer (blocking with timeout)
 * 
 * @param cb Pointer to circular buffer
 * @param data Pointer to data to write
 * @param len Length of data to write
 * @param timeout Timeout in milliseconds (K_FOREVER for no timeout)
 * @return Number of bytes actually written
 */
size_t circular_buffer_write_timeout(circular_buffer_t *cb, const uint8_t *data, 
                                     size_t len, k_timeout_t timeout);

/**
 * @brief Read data from circular buffer (non-blocking)
 * 
 * @param cb Pointer to circular buffer
 * @param data Pointer to buffer to read into
 * @param len Maximum length of data to read
 * @return Number of bytes actually read
 */
size_t circular_buffer_read(circular_buffer_t *cb, uint8_t *data, size_t len);

/**
 * @brief Read data from circular buffer (blocking with timeout)
 * 
 * @param cb Pointer to circular buffer
 * @param data Pointer to buffer to read into
 * @param len Maximum length of data to read
 * @param timeout Timeout in milliseconds (K_FOREVER for no timeout)
 * @return Number of bytes actually read
 */
size_t circular_buffer_read_timeout(circular_buffer_t *cb, uint8_t *data, 
                                   size_t len, k_timeout_t timeout);

/**
 * @brief Get available space in buffer
 * 
 * @param cb Pointer to circular buffer
 * @return Available space in bytes
 */
size_t circular_buffer_space_get(circular_buffer_t *cb);

/**
 * @brief Get used space in buffer
 * 
 * @param cb Pointer to circular buffer
 * @return Used space in bytes
 */
size_t circular_buffer_size_get(circular_buffer_t *cb);

/**
 * @brief Check if buffer is empty
 * 
 * @param cb Pointer to circular buffer
 * @return true if empty, false otherwise
 */
bool circular_buffer_is_empty(circular_buffer_t *cb);

/**
 * @brief Check if buffer is full
 * 
 * @param cb Pointer to circular buffer
 * @return true if full, false otherwise
 */
bool circular_buffer_is_full(circular_buffer_t *cb);

/**
 * @brief Clear all data from buffer
 * 
 * @param cb Pointer to circular buffer
 */
void circular_buffer_clear(circular_buffer_t *cb);

/**
 * @brief Reset and cleanup circular buffer
 * 
 * @param cb Pointer to circular buffer
 */
void circular_buffer_cleanup(circular_buffer_t *cb);

#ifdef __cplusplus
}
#endif

#endif /* CIRCULAR_BUFFERS_H */
