/**
 * @file fs.h
 * @brief File System Abstraction Layer for Media Player
 * 
 * Provides a clean interface for file operations, abstracting the underlying
 * file system implementation (FatFS in this case). The goal is to allow the
 * audio architecture to use this header instead. Created in case FatFS changes
 * in the future.
 * 
 * Update (v1.1): Added the media_fs_ prefix to all functions to avoid compiler
 * conflicts with Zephyr's file system API/function calls. 
 * 
 */

#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* For safe measures */
#ifdef __cplusplus
extern "C" {
#endif

/* Error codes */
typedef enum {
    FS_OK = 0,
    FS_ERROR_NOT_INITIALIZED,
    FS_ERROR_MOUNT_FAILED,
    FS_ERROR_FILE_NOT_FOUND,
    FS_ERROR_OPEN_FAILED,
    FS_ERROR_READ_FAILED,
    FS_ERROR_WRITE_FAILED,
    FS_ERROR_SEEK_FAILED,
    FS_ERROR_INVALID_PARAM,
    FS_ERROR_NO_MEMORY,
    FS_ERROR_CARD_NOT_PRESENT,
    FS_ERROR_UNSUPPORTED_FORMAT
} fs_result_t;

/* File handle struct */
typedef struct {
    void *handle;       /* Internal file handle */
    bool is_open;       /* File open state */
    size_t size;        /* File size in bytes */
    size_t position;    /* Current file position */
} fs_file_t;

/* Directory entry struct */
typedef struct {
    char name[256];     /* File/directory name */
    size_t size;        /* File size (0 for directories) */
    bool is_directory;  /* True if directory, false if file */
} fs_dirent_t;

/* Directory handle struct */
typedef struct {
    void *handle;       /* Internal directory handle */
    bool is_open;       /* Directory open state */
} fs_dir_t;

/* File system statistics */
typedef struct {
    size_t total_space;     /* Total space in bytes */
    size_t free_space;      /* Free space in bytes */
    size_t used_space;      /* Used space in bytes */
    uint32_t files_count;   /* Number of files */
} fs_stats_t;

/**
 * @brief Initialize the media file system
 * 
 * Sets up the SD card interface and mounts the file system.
 * Must be called before any other file system operations.
 * 
 * @return FS_OK on success, error code on failure
 */
fs_result_t media_fs_init(void);

/**
 * @brief Deinitialize the media file system
 * 
 * Unmounts the file system and cleans up resources.
 * 
 * @return FS_OK on success, error code on failure
 */
fs_result_t media_fs_deinit(void);

/**
 * @brief Check if media file system is initialized and ready
 * 
 * @return true if ready, false otherwise
 */
bool media_fs_is_ready(void);

/**
 * @brief Open a file for reading
 * 
 * @param file Pointer to file handle struct
 * @param path Path to the file to open
 * @return FS_OK on success, error code on failure
 */
fs_result_t media_fs_open(fs_file_t *file, const char *path);

/**
 * @brief Close an open file
 * 
 * @param file Pointer to file handle struct
 * @return FS_OK on success, error code on failure
 */
fs_result_t media_fs_close(fs_file_t *file);

/**
 * @brief Read data from an open file
 * 
 * @param file Pointer to file handle struct
 * @param buffer Buffer to store read data
 * @param size Number of bytes to read
 * @param bytes_read Pointer to store actual bytes read
 * @return FS_OK on success, error code on failure
 */
fs_result_t media_fs_read(fs_file_t *file, void *buffer, size_t size, size_t *bytes_read);

/**
 * @brief Seek to a position in the file
 * 
 * @param file Pointer to file handle struct
 * @param offset Offset from the beginning of file
 * @return FS_OK on success, error code on failure
 */
fs_result_t media_fs_seek(fs_file_t *file, size_t offset);

/**
 * @brief Get current position in file
 * 
 * @param file Pointer to file handle struct
 * @return Current file position, or 0 on error
 */
size_t media_fs_tell(fs_file_t *file);

/**
 * @brief Check if file exists
 * 
 * @param path Path to check
 * @return true if file exists, false otherwise
 */
bool media_fs_exists(const char *path);

/**
 * @brief Get file size
 * 
 * @param path Path to file
 * @param size Pointer to store file size
 * @return FS_OK on success, error code on failure
 */
fs_result_t media_fs_get_size(const char *path, size_t *size);

/**
 * @brief Open a directory for reading
 * 
 * @param dir Pointer to directory handle struct
 * @param path Path to directory (NULL for root)
 * @return FS_OK on success, error code on failure
 */
fs_result_t media_fs_opendir(fs_dir_t *dir, const char *path);

/**
 * @brief Close an open directory
 * 
 * @param dir Pointer to directory handle struct
 * @return FS_OK on success, error code on failure
 */
fs_result_t media_fs_closedir(fs_dir_t *dir);

/**
 * @brief Read next entry from directory
 * 
 * @param dir Pointer to directory handle struct
 * @param entry Pointer to directory entry struct
 * @return FS_OK on success, FS_ERROR_FILE_NOT_FOUND if no more entries
 */
fs_result_t media_fs_readdir(fs_dir_t *dir, fs_dirent_t *entry);

/**
 * @brief Get file system statistics
 * 
 * @param stats Pointer to statistics struct
 * @return FS_OK on success, error code on failure
 */
fs_result_t media_fs_get_stats(fs_stats_t *stats);

/**
 * @brief List audio files in a directory
 * 
 * Utility function to find .wav, .mp3 files in a directory.
 * 
 * @param path Directory path (NULL for root)
 * @param files Array to store file names (caller allocated)
 * @param max_files Maximum number of files to return
 * @param count Pointer to store actual number of files found
 * @return FS_OK on success, error code on failure
 */
fs_result_t media_fs_list_audio_files(const char *path, char files[][256], 
                                     size_t max_files, size_t *count);

/**
 * @brief Convert error code to string
 * 
 * @param error Error code
 * @return String description of error
 */
const char *media_fs_error_to_string(fs_result_t error);

#ifdef __cplusplus
}
#endif

#endif /* FS_H */