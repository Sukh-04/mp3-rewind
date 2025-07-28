/**
 * @file wav_decoder.h
 * @brief WAV Audio Decoder Header
 * 
 * Provides WAV file decoding capabilities using the "TinyWAV library".
 * 
 * The TinyWAV library is a lightweight WAV file decoder designed for embedded systems.
 * Please note that this library is a reputable resource that was found online for the 
 * purpose of this project. It is not a part of the Zephyr project but is "in theory" 
 * compatible with Zephyr. 
 * 
 * Update v1.1: Ran into some issues with the TinyWAV library, so this version is a custom 
 * implementation that is inspired by the TinyWAV library but is tailored to the specific needs 
 * of this project. I will change to use TinyWAV when HTTP streaming is implemented.
 */

#ifndef WAV_DECODER_H
#define WAV_DECODER_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Audio format information structure
 */
struct audio_format_info {
    uint16_t format_tag;       ///< Audio format (1 = PCM)
    uint16_t channels;         ///< Number of channels
    uint32_t sample_rate;      ///< Sample rate in Hz
    uint32_t bytes_per_sec;    ///< Bytes per second
    uint16_t block_align;      ///< Block alignment
    uint16_t bits_per_sample;  ///< Bits per sample
};

/**
 * @brief WAV decoder structure
 */
struct wav_decoder {
    /* Input data */
    const uint8_t *data;
    size_t data_len;
    size_t position;
    
    /* WAV format information */
    struct audio_format_info format;
    
    /* Audio data location */
    size_t audio_data_offset;
    size_t audio_data_size;
    
    /* State */
    bool is_initialized;
};

/**
 * @brief Initialize WAV decoder with data buffer
 * 
 * @param decoder Decoder instance to initialize
 * @param data Pointer to WAV file data
 * @param data_len Length of WAV file data
 * @return 0 on success, negative error code on failure
 */
int wav_decoder_init(struct wav_decoder *decoder, const uint8_t *data, size_t data_len);

/**
 * @brief Get audio format information
 * 
 * @param decoder Decoder instance
 * @param format Pointer to format structure to fill
 * @return 0 on success, negative error code on failure
 */
int wav_decoder_get_format(struct wav_decoder *decoder, struct audio_format_info *format);

/**
 * @brief Read audio data from WAV file
 * 
 * @param decoder Decoder instance
 * @param buffer Buffer to read audio data into
 * @param buffer_size Size of the buffer
 * @return Number of bytes read, 0 on EOF
 */
size_t wav_decoder_read(struct wav_decoder *decoder, uint8_t *buffer, size_t buffer_size);

/**
 * @brief Seek to specific position in audio data
 * 
 * @param decoder Decoder instance
 * @param offset Offset in bytes from start of audio data
 * @return 0 on success, negative error code on failure
 */
int wav_decoder_seek(struct wav_decoder *decoder, size_t offset);

/**
 * @brief Get current position in audio data
 * 
 * @param decoder Decoder instance
 * @return Current position in bytes from start of audio data
 */
size_t wav_decoder_get_position(struct wav_decoder *decoder);

/**
 * @brief Get total size of audio data
 * 
 * @param decoder Decoder instance
 * @return Total audio data size in bytes
 */
size_t wav_decoder_get_total_size(struct wav_decoder *decoder);

/**
 * @brief Check if end of file reached
 * 
 * @param decoder Decoder instance
 * @return true if EOF, false otherwise
 */
bool wav_decoder_is_eof(struct wav_decoder *decoder);

/**
 * @brief Get total duration of audio in milliseconds
 * 
 * @param decoder Decoder instance
 * @return Duration in milliseconds
 */
uint32_t wav_decoder_get_duration_ms(struct wav_decoder *decoder);

/**
 * @brief Get total number of samples (frames) in audio data
 * 
 * @param decoder Decoder instance
 * @return Total number of samples
 */
size_t wav_decoder_get_total_samples(struct wav_decoder *decoder);

/**
 * @brief Cleanup decoder resources
 * 
 * @param decoder Decoder instance to cleanup
 */
void wav_decoder_cleanup(struct wav_decoder *decoder);

#ifdef __cplusplus
}
#endif

#endif /* WAV_DECODER_H */
