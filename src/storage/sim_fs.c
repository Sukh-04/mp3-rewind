/**
 * @file sim_fs.c
 * @brief Simulation File System Implementation using POSIX file operations
 * 
 * This file system simulation was created due to hardware issues with the SDcard
 * breakout board. I needed a way to test whether the file system logic would work
 * with minimal invasive changes to the existing codebase.
 *
 * This file provides the same interface as fs.c but uses POSIX file operations
 * to simulate SD card file system operations. This allows testing the file system
 * logic without requiring actual hardware.
 * 
 * The "SD card" is simulated using a local directory (test_data/) on the host system.
 * 
 * This was created with the help of Claude Sonnet 4 that took the existing implementation 
 * of fs.c and adapted it to use POSIX file operations for simulation purposes.
 */

#include "fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

/* Simulation logging - simple printf for now */
#define SIM_LOG_INF(fmt, ...) printf("[SIM_INF] fs_module: " fmt "\n", ##__VA_ARGS__)
#define SIM_LOG_ERR(fmt, ...) printf("[SIM_ERR] fs_module: " fmt "\n", ##__VA_ARGS__)
#define SIM_LOG_WRN(fmt, ...) printf("[SIM_WRN] fs_module: " fmt "\n", ##__VA_ARGS__)
#define SIM_LOG_DBG(fmt, ...) printf("[SIM_DBG] fs_module: " fmt "\n", ##__VA_ARGS__)

/* Simulated SD card directory */
#define SIM_SD_PATH "./test_data"

/* File system state */
static bool fs_initialized = false;
static bool fs_mounted = false;

/* Internal file structure for simulation */
struct sim_internal_file {
    FILE *posix_file;
    bool in_use;
};

/* Internal directory structure for simulation */
struct sim_internal_dir {
    DIR *posix_dir;
    bool in_use;
};

/* Static allocation for file handles */
#define MAX_OPEN_FILES 4
static struct sim_internal_file file_pool[MAX_OPEN_FILES];

/* Static allocation for directory handles */
#define MAX_OPEN_DIRS 2
static struct sim_internal_dir dir_pool[MAX_OPEN_DIRS];

/* Helper function to get free file handle */
static struct sim_internal_file *get_free_file_handle(void)
{
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!file_pool[i].in_use) {
            memset(&file_pool[i], 0, sizeof(file_pool[i]));
            file_pool[i].in_use = true;
            return &file_pool[i];
        }
    }
    return NULL;
}

/* Helper function to release file handle */
static void release_file_handle(struct sim_internal_file *file)
{
    if (file) {
        file->in_use = false;
        memset(file, 0, sizeof(*file));
    }
}

/* Helper function to get free directory handle */
static struct sim_internal_dir *get_free_dir_handle(void)
{
    for (int i = 0; i < MAX_OPEN_DIRS; i++) {
        if (!dir_pool[i].in_use) {
            memset(&dir_pool[i], 0, sizeof(dir_pool[i]));
            dir_pool[i].in_use = true;
            return &dir_pool[i];
        }
    }
    return NULL;
}

/* Helper function to release directory handle */
static void release_dir_handle(struct sim_internal_dir *dir)
{
    if (dir) {
        dir->in_use = false;
        memset(dir, 0, sizeof(*dir));
    }
}

/* Helper function to build full path */
static void build_full_path(char *full_path, size_t size, const char *path)
{
    if (path && path[0] == '/') {
        /* Absolute path, prepend simulation directory */
        snprintf(full_path, size, "%s%s", SIM_SD_PATH, path);
    } else if (path) {
        /* Relative path, prepend simulation directory and slash */
        snprintf(full_path, size, "%s/%s", SIM_SD_PATH, path);
    } else {
        /* NULL path, use simulation directory */
        snprintf(full_path, size, "%s", SIM_SD_PATH);
    }
}

/* Helper function to create directory if it doesn't exist */
static int ensure_directory_exists(const char *path)
{
    struct stat st = {0};
    
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0755) == -1) {
            SIM_LOG_ERR("Failed to create directory %s: %s", path, strerror(errno));
            return -1;
        }
        SIM_LOG_INF("Created simulation directory: %s", path);
    }
    return 0;
}

fs_result_t media_fs_init(void)
{
    SIM_LOG_INF("Initializing simulation file system...");
    
    if (fs_initialized) {
        SIM_LOG_WRN("File system already initialized");
        return FS_OK;
    }
    
    /* Create simulation directory if it doesn't exist */
    if (ensure_directory_exists(SIM_SD_PATH) != 0) {
        return FS_ERROR_MOUNT_FAILED;
    }
    
    /* Simulate SD card detection */
    SIM_LOG_INF("Simulated SD card detected: using directory %s", SIM_SD_PATH);
    
    /* Initialize static pools */
    memset(file_pool, 0, sizeof(file_pool));
    memset(dir_pool, 0, sizeof(dir_pool));
    
    fs_initialized = true;
    fs_mounted = true;
    
    SIM_LOG_INF("Simulation file system mounted successfully");
    
    return FS_OK;
}

fs_result_t media_fs_deinit(void)
{
    if (!fs_initialized) {
        return FS_OK;
    }
    
    /* Close any open files/directories */
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (file_pool[i].in_use) {
            media_fs_close((fs_file_t *)&file_pool[i]);
        }
    }
    
    for (int i = 0; i < MAX_OPEN_DIRS; i++) {
        if (dir_pool[i].in_use) {
            media_fs_closedir((fs_dir_t *)&dir_pool[i]);
        }
    }
    
    fs_initialized = false;
    fs_mounted = false;
    SIM_LOG_INF("Simulation file system deinitialized");
    
    return FS_OK;
}

bool media_fs_is_ready(void)
{
    return fs_initialized && fs_mounted;
}

fs_result_t media_fs_open(fs_file_t *file, const char *path)
{
    char full_path[512];
    struct sim_internal_file *internal_file;
    struct stat file_stat;
    
    if (!file || !path) {
        return FS_ERROR_INVALID_PARAM;
    }
    
    if (!media_fs_is_ready()) {
        return FS_ERROR_NOT_INITIALIZED;
    }
    
    /* Get a free file handle */
    internal_file = get_free_file_handle();
    if (!internal_file) {
        SIM_LOG_ERR("No free file handles available");
        return FS_ERROR_NO_MEMORY;
    }
    
    /* Build full path */
    build_full_path(full_path, sizeof(full_path), path);
    
    /* Open file using POSIX */
    internal_file->posix_file = fopen(full_path, "rb");
    if (!internal_file->posix_file) {
        SIM_LOG_ERR("Failed to open file %s: %s", full_path, strerror(errno));
        release_file_handle(internal_file);
        return (errno == ENOENT) ? FS_ERROR_FILE_NOT_FOUND : FS_ERROR_OPEN_FAILED;
    }
    
    /* Get file size */
    if (stat(full_path, &file_stat) == 0) {
        file->size = file_stat.st_size;
    } else {
        file->size = 0;
    }
    
    /* Set up the public file handle */
    file->handle = internal_file;
    file->is_open = true;
    file->position = 0;
    
    SIM_LOG_DBG("Opened file %s, size: %zu bytes", path, file->size);
    
    return FS_OK;
}

fs_result_t media_fs_close(fs_file_t *file)
{
    struct sim_internal_file *internal_file;
    
    if (!file || !file->handle || !file->is_open) {
        return FS_ERROR_INVALID_PARAM;
    }
    
    internal_file = (struct sim_internal_file *)file->handle;
    
    if (internal_file->posix_file) {
        fclose(internal_file->posix_file);
    }
    
    release_file_handle(internal_file);
    
    file->handle = NULL;
    file->is_open = false;
    file->size = 0;
    file->position = 0;
    
    return FS_OK;
}

fs_result_t media_fs_read(fs_file_t *file, void *buffer, size_t size, size_t *bytes_read)
{
    struct sim_internal_file *internal_file;
    size_t result;
    
    if (!file || !file->handle || !file->is_open || !buffer || !bytes_read) {
        return FS_ERROR_INVALID_PARAM;
    }
    
    internal_file = (struct sim_internal_file *)file->handle;
    
    result = fread(buffer, 1, size, internal_file->posix_file);
    
    *bytes_read = result;
    file->position += result;
    
    return FS_OK;
}

fs_result_t media_fs_seek(fs_file_t *file, size_t offset)
{
    struct sim_internal_file *internal_file;
    
    if (!file || !file->handle || !file->is_open) {
        return FS_ERROR_INVALID_PARAM;
    }
    
    internal_file = (struct sim_internal_file *)file->handle;
    
    if (fseek(internal_file->posix_file, offset, SEEK_SET) != 0) {
        SIM_LOG_ERR("Failed to seek file: %s", strerror(errno));
        return FS_ERROR_SEEK_FAILED;
    }
    
    file->position = offset;
    return FS_OK;
}

size_t media_fs_tell(fs_file_t *file)
{
    if (!file || !file->handle || !file->is_open) {
        return 0;
    }
    
    return file->position;
}

bool media_fs_exists(const char *path)
{
    char full_path[512];
    struct stat file_stat;
    
    if (!path || !media_fs_is_ready()) {
        return false;
    }
    
    build_full_path(full_path, sizeof(full_path), path);
    return (stat(full_path, &file_stat) == 0);
}

fs_result_t media_fs_get_size(const char *path, size_t *size)
{
    char full_path[512];
    struct stat file_stat;
    
    if (!path || !size || !media_fs_is_ready()) {
        return FS_ERROR_INVALID_PARAM;
    }
    
    build_full_path(full_path, sizeof(full_path), path);
    if (stat(full_path, &file_stat) != 0) {
        return (errno == ENOENT) ? FS_ERROR_FILE_NOT_FOUND : FS_ERROR_OPEN_FAILED;
    }
    
    *size = file_stat.st_size;
    return FS_OK;
}

fs_result_t media_fs_opendir(fs_dir_t *dir, const char *path)
{
    char full_path[512];
    struct sim_internal_dir *internal_dir;
    
    if (!dir) {
        return FS_ERROR_INVALID_PARAM;
    }
    
    if (!media_fs_is_ready()) {
        return FS_ERROR_NOT_INITIALIZED;
    }
    
    /* Get a free directory handle */
    internal_dir = get_free_dir_handle();
    if (!internal_dir) {
        SIM_LOG_ERR("No free directory handles available");
        return FS_ERROR_NO_MEMORY;
    }
    
    /* Build full path */
    build_full_path(full_path, sizeof(full_path), path);
    
    /* Open directory using POSIX */
    internal_dir->posix_dir = opendir(full_path);
    if (!internal_dir->posix_dir) {
        SIM_LOG_ERR("Failed to open directory %s: %s", full_path, strerror(errno));
        release_dir_handle(internal_dir);
        return FS_ERROR_OPEN_FAILED;
    }
    
    /* Set up the public directory handle */
    dir->handle = internal_dir;
    dir->is_open = true;
    
    SIM_LOG_DBG("Opened directory %s", path ? path : "root");
    
    return FS_OK;
}

fs_result_t media_fs_closedir(fs_dir_t *dir)
{
    struct sim_internal_dir *internal_dir;
    
    if (!dir || !dir->handle || !dir->is_open) {
        return FS_ERROR_INVALID_PARAM;
    }
    
    internal_dir = (struct sim_internal_dir *)dir->handle;
    
    if (internal_dir->posix_dir) {
        closedir(internal_dir->posix_dir);
    }
    
    release_dir_handle(internal_dir);
    
    dir->handle = NULL;
    dir->is_open = false;
    
    return FS_OK;
}

fs_result_t media_fs_readdir(fs_dir_t *dir, fs_dirent_t *entry)
{
    struct sim_internal_dir *internal_dir;
    struct dirent *posix_entry;
    struct stat file_stat;
    char full_path[512];
    
    if (!dir || !dir->handle || !dir->is_open || !entry) {
        return FS_ERROR_INVALID_PARAM;
    }
    
    internal_dir = (struct sim_internal_dir *)dir->handle;
    
    posix_entry = readdir(internal_dir->posix_dir);
    if (!posix_entry) {
        return FS_ERROR_FILE_NOT_FOUND; /* End of directory */
    }
    
    /* Skip . and .. entries */
    if (strcmp(posix_entry->d_name, ".") == 0 || strcmp(posix_entry->d_name, "..") == 0) {
        return media_fs_readdir(dir, entry); /* Recursive call to get next entry */
    }
    
    /* Copy to our structure */
    strncpy(entry->name, posix_entry->d_name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    
    /* Get file stats to determine if it's a directory and get size */
    snprintf(full_path, sizeof(full_path), "%s/%s", SIM_SD_PATH, posix_entry->d_name);
    if (stat(full_path, &file_stat) == 0) {
        entry->is_directory = S_ISDIR(file_stat.st_mode);
        entry->size = entry->is_directory ? 0 : file_stat.st_size;
    } else {
        entry->is_directory = false;
        entry->size = 0;
    }
    
    return FS_OK;
}

fs_result_t media_fs_get_stats(fs_stats_t *stats)
{
    if (!stats || !media_fs_is_ready()) {
        return FS_ERROR_INVALID_PARAM;
    }
    
    /* Simulate file system stats - these are just reasonable fake values */
    stats->total_space = 8L * 1024 * 1024 * 1024; /* 8GB simulated SD card */
    stats->used_space = 100L * 1024 * 1024;       /* 100MB used */
    stats->free_space = stats->total_space - stats->used_space;
    stats->files_count = 0; /* Not easily available */
    
    return FS_OK;
}

fs_result_t media_fs_list_audio_files(const char *path, char files[][256], 
                                     size_t max_files, size_t *count)
{
    fs_dir_t dir;
    fs_dirent_t entry;
    fs_result_t result;
    size_t found = 0;
    
    if (!files || !count || max_files == 0) {
        return FS_ERROR_INVALID_PARAM;
    }
    
    *count = 0;
    
    result = media_fs_opendir(&dir, path);
    if (result != FS_OK) {
        return result;
    }
    
    while (found < max_files) {
        result = media_fs_readdir(&dir, &entry);
        if (result != FS_OK) {
            break; /* End of directory or error */
        }
        
        /* Skip directories */
        if (entry.is_directory) {
            continue;
        }
        
        /* Check for audio file extensions */
        char *ext = strrchr(entry.name, '.');
        if (ext) {
            if (strcasecmp(ext, ".wav") == 0 || 
                strcasecmp(ext, ".mp3") == 0 ||
                strcasecmp(ext, ".flac") == 0) {
                strncpy(files[found], entry.name, 255);
                files[found][255] = '\0';
                found++;
            }
        }
    }
    
    media_fs_closedir(&dir);
    
    *count = found;
    SIM_LOG_INF("Found %zu audio files", found);
    
    return FS_OK;
}

const char *media_fs_error_to_string(fs_result_t error)
{
    switch (error) {
        case FS_OK:                     return "Success";
        case FS_ERROR_NOT_INITIALIZED:  return "File system not initialized";
        case FS_ERROR_MOUNT_FAILED:     return "Mount failed";
        case FS_ERROR_FILE_NOT_FOUND:   return "File not found";
        case FS_ERROR_OPEN_FAILED:      return "Open failed";
        case FS_ERROR_READ_FAILED:      return "Read failed";
        case FS_ERROR_WRITE_FAILED:     return "Write failed";
        case FS_ERROR_SEEK_FAILED:      return "Seek failed";
        case FS_ERROR_INVALID_PARAM:    return "Invalid parameter";
        case FS_ERROR_NO_MEMORY:        return "No memory available";
        case FS_ERROR_CARD_NOT_PRESENT: return "SD card not present";
        case FS_ERROR_UNSUPPORTED_FORMAT: return "Unsupported format";
        default:                        return "Unknown error";
    }
}
