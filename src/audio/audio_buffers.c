/**
 * @file audio_buffers.c
 * @brief Audio Buffer Management System
 * 
 * Manages multiple audio buffers for smooth streaming and playback.
 * Handles buffer allocation, management, and synchronization between
 * HTTP client (producer) and audio output (consumer).
 * 
 * The HTTP cleint and audio output are yet to be implemented, here is just 
 * a basic implementation of the audio buffer management system. This file is 
 * heavily inspired by the circular buffer implementation in the utils/circular_buffers.h/.c files.
 * 
 */

#include "audio_buffers.h"
#include "../utils/circular_buffers.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(audio_buffers, LOG_LEVEL_DBG);

/* Buffer pool configuration */
#define MAX_AUDIO_BUFFERS 4
#define BUFFER_SIZE_BYTES (2048)  // 2KB per buffer

/* Audio buffer pool */
static struct {
    /* Buffer pool */
    struct audio_buffer buffers[MAX_AUDIO_BUFFERS];
    uint8_t buffer_memory[MAX_AUDIO_BUFFERS][BUFFER_SIZE_BYTES];
    
    /* Free buffer management */
    struct k_mem_slab buffer_slab;
    struct k_mutex pool_mutex;
    
    /* Statistics */
    uint32_t buffers_allocated;
    uint32_t buffers_freed;
    uint32_t allocation_failures;
    
    bool initialized;
} buffer_pool = {
    .initialized = false
};

int audio_buffer_pool_init(void)
{
    if (buffer_pool.initialized) {
        LOG_WRN("Audio buffer pool already initialized");
        return -EALREADY;
    }

    /* Initialize memory slab for buffer allocation */
    k_mem_slab_init(&buffer_pool.buffer_slab, 
                    buffer_pool.buffer_memory,
                    BUFFER_SIZE_BYTES, 
                    MAX_AUDIO_BUFFERS);

    /* Initialize mutex */
    k_mutex_init(&buffer_pool.pool_mutex);

    /* Initialize all buffers */
    for (int i = 0; i < MAX_AUDIO_BUFFERS; i++) {
        struct audio_buffer *buf = &buffer_pool.buffers[i];
        buf->data = buffer_pool.buffer_memory[i];
        buf->size = BUFFER_SIZE_BYTES;
        buf->used = 0;
        buf->sequence = 0;
        buf->timestamp = 0;
        buf->flags = 0;
    }

    buffer_pool.buffers_allocated = 0;
    buffer_pool.buffers_freed = 0;
    buffer_pool.allocation_failures = 0;
    buffer_pool.initialized = true;

    LOG_INF("Audio buffer pool initialized: %d buffers x %d bytes", 
            MAX_AUDIO_BUFFERS, BUFFER_SIZE_BYTES);
    
    return 0;
}

struct audio_buffer *audio_buffer_alloc(k_timeout_t timeout)
{
    if (!buffer_pool.initialized) {
        LOG_ERR("Buffer pool not initialized");
        return NULL;
    }

    void *mem;
    int ret = k_mem_slab_alloc(&buffer_pool.buffer_slab, &mem, timeout);
    if (ret != 0) {
        k_mutex_lock(&buffer_pool.pool_mutex, K_FOREVER);
        buffer_pool.allocation_failures++;
        k_mutex_unlock(&buffer_pool.pool_mutex);
        
        LOG_WRN("Buffer allocation failed: %d", ret);
        return NULL;
    }

    /* Find the corresponding buffer structure */
    struct audio_buffer *buf = NULL;
    for (int i = 0; i < MAX_AUDIO_BUFFERS; i++) {
        if (buffer_pool.buffers[i].data == mem) {
            buf = &buffer_pool.buffers[i];
            break;
        }
    }

    if (!buf) {
        LOG_ERR("Failed to find buffer structure");
        k_mem_slab_free(&buffer_pool.buffer_slab, mem);
        return NULL;
    }

    /* Reset buffer */
    buf->used = 0;
    buf->sequence = 0;
    buf->timestamp = k_uptime_get();
    buf->flags = 0;

    k_mutex_lock(&buffer_pool.pool_mutex, K_FOREVER);
    buffer_pool.buffers_allocated++;
    k_mutex_unlock(&buffer_pool.pool_mutex);

    LOG_DBG("Buffer allocated: %p", buf);
    return buf;
}

int audio_buffer_free(struct audio_buffer *buffer)
{
    if (!buffer || !buffer_pool.initialized) {
        return -EINVAL;
    }

    /* Verify this is a valid buffer from our pool */
    bool valid = false;
    for (int i = 0; i < MAX_AUDIO_BUFFERS; i++) {
        if (&buffer_pool.buffers[i] == buffer) {
            valid = true;
            break;
        }
    }

    if (!valid) {
        LOG_ERR("Attempting to free invalid buffer: %p", buffer);
        return -EINVAL;
    }

    /* Clear buffer content for security */
    memset(buffer->data, 0, buffer->size);
    buffer->used = 0;
    buffer->flags = 0;

    /* Free the memory */
    k_mem_slab_free(&buffer_pool.buffer_slab, buffer->data);

    k_mutex_lock(&buffer_pool.pool_mutex, K_FOREVER);
    buffer_pool.buffers_freed++;
    k_mutex_unlock(&buffer_pool.pool_mutex);

    LOG_DBG("Buffer freed: %p", buffer);
    return 0;
}

size_t audio_buffer_write(struct audio_buffer *buffer, const uint8_t *data, size_t len)
{
    if (!buffer || !data || len == 0) {
        return 0;
    }

    size_t available = buffer->size - buffer->used;
    size_t to_write = (len > available) ? available : len;

    if (to_write == 0) {
        LOG_WRN("Buffer full, cannot write %zu bytes", len);
        return 0;
    }

    memcpy(&buffer->data[buffer->used], data, to_write);
    buffer->used += to_write;

    LOG_DBG("Wrote %zu bytes to buffer, now %zu/%zu used", 
            to_write, buffer->used, buffer->size);

    return to_write;
}

size_t audio_buffer_read(struct audio_buffer *buffer, uint8_t *data, size_t len)
{
    if (!buffer || !data || len == 0) {
        return 0;
    }

    size_t to_read = (len > buffer->used) ? buffer->used : len;

    if (to_read == 0) {
        return 0;
    }

    memcpy(data, buffer->data, to_read);
    
    /* Shift remaining data to the beginning */
    if (to_read < buffer->used) {
        memmove(buffer->data, &buffer->data[to_read], buffer->used - to_read);
    }
    
    buffer->used -= to_read;

    LOG_DBG("Read %zu bytes from buffer, %zu bytes remaining", 
            to_read, buffer->used);

    return to_read;
}

size_t audio_buffer_get_free_space(struct audio_buffer *buffer)
{
    if (!buffer) {
        return 0;
    }

    return buffer->size - buffer->used;
}

size_t audio_buffer_get_used_space(struct audio_buffer *buffer)
{
    if (!buffer) {
        return 0;
    }

    return buffer->used;
}

bool audio_buffer_is_empty(struct audio_buffer *buffer)
{
    return buffer ? (buffer->used == 0) : true;
}

bool audio_buffer_is_full(struct audio_buffer *buffer)
{
    return buffer ? (buffer->used == buffer->size) : false;
}

void audio_buffer_clear(struct audio_buffer *buffer)
{
    if (!buffer) {
        return;
    }

    buffer->used = 0;
    buffer->flags = 0;
    LOG_DBG("Buffer cleared: %p", buffer);
}

int audio_buffer_pool_get_stats(struct audio_buffer_stats *stats)
{
    if (!stats || !buffer_pool.initialized) {
        return -EINVAL;
    }

    k_mutex_lock(&buffer_pool.pool_mutex, K_FOREVER);
    
    stats->total_buffers = MAX_AUDIO_BUFFERS;
    stats->buffers_allocated = buffer_pool.buffers_allocated;
    stats->buffers_freed = buffer_pool.buffers_freed;
    stats->allocation_failures = buffer_pool.allocation_failures;
    stats->buffers_in_use = buffer_pool.buffers_allocated - buffer_pool.buffers_freed;
    
    /* Calculate free buffers */
    stats->free_buffers = MAX_AUDIO_BUFFERS - stats->buffers_in_use;
    
    k_mutex_unlock(&buffer_pool.pool_mutex);

    return 0;
}

void audio_buffer_pool_cleanup(void)
{
    if (!buffer_pool.initialized) {
        return;
    }

    /* Free any remaining allocated buffers */
    for (int i = 0; i < MAX_AUDIO_BUFFERS; i++) {
        struct audio_buffer *buf = &buffer_pool.buffers[i];
        if (buf->used > 0) {
            audio_buffer_clear(buf);
        }
    }

    buffer_pool.initialized = false;
    LOG_INF("Audio buffer pool cleaned up");
}
