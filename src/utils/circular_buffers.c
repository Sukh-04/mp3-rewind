/**
 * @file circular_buffers.c
 * @brief Circular Buffer Implementation
 * 
 * See circular_buffers.h for more details and documentation!
 * 
 */

#include "circular_buffers.h"
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(circular_buffer, LOG_LEVEL_DBG);

int circular_buffer_init(circular_buffer_t *cb, uint8_t *buffer, size_t size)
{
    if (!cb || !buffer || size == 0) {
        return -EINVAL;
    }

    cb->buffer = buffer;
    cb->size = size;
    cb->head = 0;
    cb->tail = 0;
    cb->count = 0;

    k_mutex_init(&cb->mutex);
    k_condvar_init(&cb->not_full);
    k_condvar_init(&cb->not_empty);

    LOG_DBG("Circular buffer initialized: size=%zu", size);
    return 0;
}

size_t circular_buffer_write(circular_buffer_t *cb, const uint8_t *data, size_t len)
{
    if (!cb || !data || len == 0) {
        return 0;
    }

    k_mutex_lock(&cb->mutex, K_FOREVER);

    size_t available = cb->size - cb->count;
    size_t to_write = (len > available) ? available : len;
    
    if (to_write == 0) {
        k_mutex_unlock(&cb->mutex);
        return 0;
    }

    // Handle wrap-around case
    size_t bytes_to_end = cb->size - cb->head;
    if (to_write <= bytes_to_end) {
        // No wrap-around needed
        memcpy(&cb->buffer[cb->head], data, to_write);
    } else {
        // Wrap-around needed
        memcpy(&cb->buffer[cb->head], data, bytes_to_end);
        memcpy(&cb->buffer[0], &data[bytes_to_end], to_write - bytes_to_end);
    }

    cb->head = (cb->head + to_write) % cb->size;
    cb->count += to_write;

    k_condvar_signal(&cb->not_empty);
    k_mutex_unlock(&cb->mutex);

    return to_write;
}

size_t circular_buffer_write_timeout(circular_buffer_t *cb, const uint8_t *data, 
                                     size_t len, k_timeout_t timeout)
{
    if (!cb || !data || len == 0) {
        return 0;
    }

    k_mutex_lock(&cb->mutex, K_FOREVER);

    // Wait for space if buffer is full
    while (cb->count == cb->size) {
        int ret = k_condvar_wait(&cb->not_full, &cb->mutex, timeout);
        if (ret != 0) {
            k_mutex_unlock(&cb->mutex);
            return 0; // Timeout or error
        }
    }

    size_t available = cb->size - cb->count;
    size_t to_write = (len > available) ? available : len;

    // Handle wrap-around case
    size_t bytes_to_end = cb->size - cb->head;
    if (to_write <= bytes_to_end) {
        memcpy(&cb->buffer[cb->head], data, to_write);
    } else {
        memcpy(&cb->buffer[cb->head], data, bytes_to_end);
        memcpy(&cb->buffer[0], &data[bytes_to_end], to_write - bytes_to_end);
    }

    cb->head = (cb->head + to_write) % cb->size;
    cb->count += to_write;

    k_condvar_signal(&cb->not_empty);
    k_mutex_unlock(&cb->mutex);

    return to_write;
}

size_t circular_buffer_read(circular_buffer_t *cb, uint8_t *data, size_t len)
{
    if (!cb || !data || len == 0) {
        return 0;
    }

    k_mutex_lock(&cb->mutex, K_FOREVER);

    size_t to_read = (len > cb->count) ? cb->count : len;
    
    if (to_read == 0) {
        k_mutex_unlock(&cb->mutex);
        return 0;
    }

    // Handle wrap-around case
    size_t bytes_to_end = cb->size - cb->tail;
    if (to_read <= bytes_to_end) {
        // No wrap-around needed
        memcpy(data, &cb->buffer[cb->tail], to_read);
    } else {
        // Wrap-around needed
        memcpy(data, &cb->buffer[cb->tail], bytes_to_end);
        memcpy(&data[bytes_to_end], &cb->buffer[0], to_read - bytes_to_end);
    }

    cb->tail = (cb->tail + to_read) % cb->size;
    cb->count -= to_read;

    k_condvar_signal(&cb->not_full);
    k_mutex_unlock(&cb->mutex);

    return to_read;
}

size_t circular_buffer_read_timeout(circular_buffer_t *cb, uint8_t *data, 
                                   size_t len, k_timeout_t timeout)
{
    if (!cb || !data || len == 0) {
        return 0;
    }

    k_mutex_lock(&cb->mutex, K_FOREVER);

    // Wait for data if buffer is empty
    while (cb->count == 0) {
        int ret = k_condvar_wait(&cb->not_empty, &cb->mutex, timeout);
        if (ret != 0) {
            k_mutex_unlock(&cb->mutex);
            return 0; // Timeout or error
        }
    }

    size_t to_read = (len > cb->count) ? cb->count : len;

    // Handle wrap-around case
    size_t bytes_to_end = cb->size - cb->tail;
    if (to_read <= bytes_to_end) {
        memcpy(data, &cb->buffer[cb->tail], to_read);
    } else {
        memcpy(data, &cb->buffer[cb->tail], bytes_to_end);
        memcpy(&data[bytes_to_end], &cb->buffer[0], to_read - bytes_to_end);
    }

    cb->tail = (cb->tail + to_read) % cb->size;
    cb->count -= to_read;

    k_condvar_signal(&cb->not_full);
    k_mutex_unlock(&cb->mutex);

    return to_read;
}

size_t circular_buffer_space_get(circular_buffer_t *cb)
{
    if (!cb) {
        return 0;
    }

    k_mutex_lock(&cb->mutex, K_FOREVER);
    size_t space = cb->size - cb->count;
    k_mutex_unlock(&cb->mutex);
    
    return space;
}

size_t circular_buffer_size_get(circular_buffer_t *cb)
{
    if (!cb) {
        return 0;
    }

    k_mutex_lock(&cb->mutex, K_FOREVER);
    size_t count = cb->count;
    k_mutex_unlock(&cb->mutex);
    
    return count;
}

bool circular_buffer_is_empty(circular_buffer_t *cb)
{
    if (!cb) {
        return true;
    }

    k_mutex_lock(&cb->mutex, K_FOREVER);
    bool empty = (cb->count == 0);
    k_mutex_unlock(&cb->mutex);
    
    return empty;
}

bool circular_buffer_is_full(circular_buffer_t *cb)
{
    if (!cb) {
        return false;
    }

    k_mutex_lock(&cb->mutex, K_FOREVER);
    bool full = (cb->count == cb->size);
    k_mutex_unlock(&cb->mutex);
    
    return full;
}

void circular_buffer_clear(circular_buffer_t *cb)
{
    if (!cb) {
        return;
    }

    k_mutex_lock(&cb->mutex, K_FOREVER);
    cb->head = 0;
    cb->tail = 0;
    cb->count = 0;
    k_condvar_broadcast(&cb->not_full);
    k_mutex_unlock(&cb->mutex);
    
    LOG_DBG("Circular buffer cleared");
}

void circular_buffer_cleanup(circular_buffer_t *cb)
{
    if (!cb) {
        return;
    }

    circular_buffer_clear(cb);
    LOG_DBG("Circular buffer cleanup completed");
}
