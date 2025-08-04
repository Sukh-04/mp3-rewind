/**
 * @file wav_decoder.c
 * @brief WAV Audio Decoder Implementation
 * 
 * Provides WAV file decoding capabilities for the audio streaming system.
 * Uses manual parsing for embedded memory-based WAV data. Will in the 
 * future use the TinyWAV library for more complex WAV files during HTTP streaming.
 * 
 * Please see the wav_decoder.h file for more details and documentation.
 */

#include "wav_decoder.h"
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(wav_decoder, LOG_LEVEL_DBG);

int wav_decoder_init(struct wav_decoder *decoder, const uint8_t *data, size_t data_len)
{
    if (!decoder || !data || data_len == 0) {
        LOG_ERR("Invalid parameters");
        return -EINVAL;
    }

    memset(decoder, 0, sizeof(struct wav_decoder));

    /* Store data reference */
    decoder->data = data;
    decoder->data_len = data_len;
    decoder->position = 0;

    /* Basic WAV header validation */
    if (data_len < 44) {
        LOG_ERR("Data too small for WAV header");
        return -EINVAL;
    }

    /* Check RIFF signature */
    if (memcmp(data, "RIFF", 4) != 0) {
        LOG_ERR("Invalid RIFF signature");
        return -EINVAL;
    }

    /* Check WAVE signature */
    if (memcmp(data + 8, "WAVE", 4) != 0) {
        LOG_ERR("Invalid WAVE signature");
        return -EINVAL;
    }

    /* Parse WAV header manually for embedded use */
    const uint8_t *ptr = data + 12; // Skip RIFF header
    size_t remaining = data_len - 12;
    
    while (remaining >= 8) {
        uint32_t chunk_id, chunk_size;
        
        /* Safe unaligned access using memcpy */
        memcpy(&chunk_id, ptr, 4);
        memcpy(&chunk_size, ptr + 4, 4);
        
        if (chunk_id == 0x20746d66) { // "fmt "
            if (chunk_size < 16) {
                LOG_ERR("Invalid fmt chunk size");
                return -EINVAL;
            }
            
            /* Parse format information using safe unaligned access */
            memcpy(&decoder->format.format_tag, ptr + 8, 2);
            memcpy(&decoder->format.channels, ptr + 10, 2);
            memcpy(&decoder->format.sample_rate, ptr + 12, 4);
            memcpy(&decoder->format.bytes_per_sec, ptr + 16, 4);
            memcpy(&decoder->format.block_align, ptr + 20, 2);
            memcpy(&decoder->format.bits_per_sample, ptr + 22, 2);
            
            LOG_INF("WAV Format: %u channels, %u Hz, %u bits", 
                    decoder->format.channels,
                    decoder->format.sample_rate,
                    decoder->format.bits_per_sample);
        }
        else if (chunk_id == 0x61746164) { // "data"
            /* Found data chunk */
            decoder->audio_data_offset = (ptr + 8) - data;
            decoder->audio_data_size = chunk_size;
            
            LOG_INF("Audio data: offset=%zu, size=%zu", 
                    decoder->audio_data_offset, 
                    decoder->audio_data_size);
            break;
        }
        
        /* Move to next chunk */
        ptr += 8 + chunk_size;
        remaining -= 8 + chunk_size;
        
        /* Handle padding */
        if (chunk_size & 1) {
            ptr++;
            remaining--;
        }
    }

    if (decoder->audio_data_offset == 0) {
        LOG_ERR("No audio data chunk found");
        return -EINVAL;
    }

    /* Validate format */
    if (decoder->format.format_tag != 1) { // PCM
        LOG_ERR("Only PCM format supported, got format %u", decoder->format.format_tag);
        return -ENOTSUP;
    }

    if (decoder->format.channels == 0 || decoder->format.channels > 2) {
        LOG_ERR("Unsupported channel count: %u", decoder->format.channels);
        return -ENOTSUP;
    }

    if (decoder->format.bits_per_sample != 8 && decoder->format.bits_per_sample != 16) {
        LOG_ERR("Unsupported bit depth: %u", decoder->format.bits_per_sample);
        return -ENOTSUP;
    }

    decoder->is_initialized = true;
    decoder->position = decoder->audio_data_offset;

    LOG_INF("WAV decoder initialized successfully");
    return 0;
}

int wav_decoder_get_format(struct wav_decoder *decoder, struct audio_format_info *format)
{
    if (!decoder || !format || !decoder->is_initialized) {
        return -EINVAL;
    }

    memcpy(format, &decoder->format, sizeof(struct audio_format_info));
    return 0;
}

size_t wav_decoder_read(struct wav_decoder *decoder, uint8_t *buffer, size_t buffer_size)
{
    if (!decoder || !buffer || buffer_size == 0 || !decoder->is_initialized) {
        return 0;
    }

    /* Calculate remaining data */
    size_t data_end = decoder->audio_data_offset + decoder->audio_data_size;
    if (decoder->position >= data_end) {
        /* End of audio data */
        return 0;
    }

    size_t remaining = data_end - decoder->position;
    size_t to_read = (buffer_size > remaining) ? remaining : buffer_size;

    /* Copy data */
    memcpy(buffer, decoder->data + decoder->position, to_read);
    decoder->position += to_read;

    LOG_DBG("Read %zu bytes, position now %zu/%zu", 
            to_read, decoder->position - decoder->audio_data_offset, 
            decoder->audio_data_size);

    return to_read;
}

int wav_decoder_seek(struct wav_decoder *decoder, size_t offset)
{
    if (!decoder || !decoder->is_initialized) {
        return -EINVAL;
    }

    size_t new_position = decoder->audio_data_offset + offset;
    size_t data_end = decoder->audio_data_offset + decoder->audio_data_size;

    if (new_position > data_end) {
        LOG_ERR("Seek beyond end of audio data");
        return -EINVAL;
    }

    decoder->position = new_position;
    return 0;
}

size_t wav_decoder_get_position(struct wav_decoder *decoder)
{
    if (!decoder || !decoder->is_initialized) {
        return 0;
    }

    return decoder->position - decoder->audio_data_offset;
}

bool wav_decoder_is_eof(struct wav_decoder *decoder)
{
    if (!decoder || !decoder->is_initialized) {
        return true;
    }

    return decoder->position >= (decoder->audio_data_offset + decoder->audio_data_size);
}

size_t wav_decoder_get_total_samples(struct wav_decoder *decoder)
{
    if (!decoder || !decoder->is_initialized) {
        return 0;
    }

    size_t total_samples = decoder->audio_data_size /
                          (decoder->format.channels * (decoder->format.bits_per_sample / 8));
    return total_samples;
}

size_t wav_decoder_get_total_size(struct wav_decoder *decoder)
{
    if (!decoder || !decoder->is_initialized) {
        return 0;
    }

    return decoder->audio_data_size;
}

uint32_t wav_decoder_get_duration_ms(struct wav_decoder *decoder)
{
    if (!decoder || !decoder->is_initialized) {
        return 0;
    }

    size_t total_samples = wav_decoder_get_total_samples(decoder);
    return (uint32_t)((total_samples * 1000) / decoder->format.sample_rate);
}

void wav_decoder_cleanup(struct wav_decoder *decoder)
{
    if (!decoder) {
        return;
    }

    memset(decoder, 0, sizeof(struct wav_decoder));
    LOG_DBG("WAV decoder cleaned up");
}

bool wav_decoder_is_initialized(struct wav_decoder *decoder)
{
    return decoder && decoder->is_initialized;
}

int wav_decoder_read_samples(struct wav_decoder *decoder, 
                            const uint8_t *chunk_data, size_t chunk_size,
                            const uint8_t **audio_samples, size_t *samples_len)
{
    if (!decoder || !chunk_data || !audio_samples || !samples_len) {
        return -EINVAL;
    }

    *audio_samples = NULL;
    *samples_len = 0;

    /* If decoder not initialized, try to initialize with this chunk */
    if (!decoder->is_initialized) {
        /* Try to initialize with current chunk data */
        if (wav_decoder_init(decoder, chunk_data, chunk_size) != 0) {
            /* Header not complete yet, need more data */
            LOG_DBG("WAV header incomplete, need more data");
            return 0;
        }
    }

    /* If we have audio data in this chunk, extract it */
    if (decoder->is_initialized) {
        /* Check if this chunk contains audio data */
        size_t audio_start_in_chunk = 0;
        
        /* If this is the first chunk with the header, skip the header part */
        if (decoder->audio_data_offset < chunk_size) {
            audio_start_in_chunk = decoder->audio_data_offset;
        }
        
        /* Calculate how much audio data is in this chunk */
        if (audio_start_in_chunk < chunk_size) {
            *audio_samples = chunk_data + audio_start_in_chunk;
            *samples_len = chunk_size - audio_start_in_chunk;
            
            LOG_DBG("Extracted %zu audio bytes from chunk", *samples_len);
            return *samples_len;
        }
    }

    return 0;
}
