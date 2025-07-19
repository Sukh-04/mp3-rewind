/**
 * @file fs.c
 * @brief File System Implementation using Zephyr FS API
 * 
 * This file was created using Zephyr Documentation, example code
 * found online, knowledge from CSC369H1 (taken previously), and 
 * with the help of Claude Sonnet 4. 
 * 
 */

#include "fs.h"
#include <zephyr/kernel.h>
#include <zephyr/fs/fs.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/fs/fs_interface.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdio.h>

/* Apparently logging in Zephyr projects is recommended. */
LOG_MODULE_REGISTER(fs_module, LOG_LEVEL_DBG);

/* Mount point for SD card */
#define SD_MOUNT_POINT "/SD:"

/* SD card disk name */
#define SD_DISK_NAME "SD"

/* File system type */
static struct fs_mount_t sd_mount = {
    .type = FS_FATFS,
    .mnt_point = SD_MOUNT_POINT,
    .storage_dev = (void *)SD_DISK_NAME,
    .flags = 0,
};

/* File system state */
static bool fs_initialized = false;
static bool fs_mounted = false;

/* Internal file structure */
struct internal_file {
    struct fs_file_t zephyr_file;
    bool in_use;
};

/* Internal directory structure */
struct internal_dir {
    struct fs_dir_t zephyr_dir;
    bool in_use;
};

/* Static allocation for file handles */
#define MAX_OPEN_FILES 4
static struct internal_file file_pool[MAX_OPEN_FILES];

/* Static allocation for directory handles */
#define MAX_OPEN_DIRS 2
static struct internal_dir dir_pool[MAX_OPEN_DIRS];

/* Helper function to get free file handle */
static struct internal_file *get_free_file_handle(void)
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
static void release_file_handle(struct internal_file *file)
{
    if (file) {
        file->in_use = false;
        memset(file, 0, sizeof(*file));
    }
}

/* Helper function to get free directory handle */
static struct internal_dir *get_free_dir_handle(void)
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
static void release_dir_handle(struct internal_dir *dir)
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
        /* Absolute path, just prepend mount point */
        snprintf(full_path, size, "%s%s", SD_MOUNT_POINT, path);
    } else if (path) {
        /* Relative path, prepend mount point and slash */
        snprintf(full_path, size, "%s/%s", SD_MOUNT_POINT, path);
    } else {
        /* NULL path, use mount point */
        snprintf(full_path, size, "%s", SD_MOUNT_POINT);
    }
}

fs_result_t media_fs_init(void)
{
    int ret;
    
    LOG_INF("Initializing file system...");
    
    if (fs_initialized) {
        LOG_WRN("File system already initialized");
        return FS_OK;
    }
    
    /* Initialize disk access */
    ret = disk_access_init(SD_DISK_NAME);
    if (ret != 0) {
        LOG_ERR("Disk access init failed: %d", ret);
        return FS_ERROR_CARD_NOT_PRESENT;
    }
    
    /* Check if disk is ready */
    uint32_t block_count;
    uint32_t block_size;
    
    ret = disk_access_ioctl(SD_DISK_NAME, DISK_IOCTL_GET_SECTOR_COUNT, &block_count);
    if (ret != 0) {
        LOG_ERR("Failed to get sector count: %d", ret);
        return FS_ERROR_CARD_NOT_PRESENT;
    }
    
    ret = disk_access_ioctl(SD_DISK_NAME, DISK_IOCTL_GET_SECTOR_SIZE, &block_size);
    if (ret != 0) {
        LOG_ERR("Failed to get sector size: %d", ret);
        return FS_ERROR_CARD_NOT_PRESENT;
    }
    
    LOG_INF("SD card detected: %u sectors, %u bytes per sector", 
            block_count, block_size);
    
    /* Mount the file system */
    ret = fs_mount(&sd_mount);
    if (ret != 0) {
        LOG_ERR("File system mount failed: %d", ret);
        return FS_ERROR_MOUNT_FAILED;
    }
    
    LOG_INF("File system mounted at %s", SD_MOUNT_POINT);
    
    /* Initialize static pools */
    memset(file_pool, 0, sizeof(file_pool));
    memset(dir_pool, 0, sizeof(dir_pool));
    
    fs_initialized = true;
    fs_mounted = true;
    
    return FS_OK;
}

fs_result_t media_fs_deinit(void)
{
    int ret;
    
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
    
    /* Unmount file system */
    if (fs_mounted) {
        ret = fs_unmount(&sd_mount);
        if (ret != 0) {
            LOG_ERR("File system unmount failed: %d", ret);
        }
        fs_mounted = false;
    }
    
    fs_initialized = false;
    LOG_INF("File system deinitialized");
    
    return FS_OK;
}

bool media_fs_is_ready(void)
{
    return fs_initialized && fs_mounted;
}

fs_result_t media_fs_open(fs_file_t *file, const char *path)
{
    int ret;
    char full_path[512];
    struct internal_file *internal_file;
    
    if (!file || !path) {
        return FS_ERROR_INVALID_PARAM;
    }
    
    if (!media_fs_is_ready()) {
        return FS_ERROR_NOT_INITIALIZED;
    }
    
    /* Get a free file handle */
    internal_file = get_free_file_handle();
    if (!internal_file) {
        LOG_ERR("No free file handles available");
        return FS_ERROR_NO_MEMORY;
    }
    
    /* Build full path */
    build_full_path(full_path, sizeof(full_path), path);
    
    /* Initialize Zephyr file structure */
    fs_file_t_init(&internal_file->zephyr_file);
    
    /* Open file */
    ret = fs_open(&internal_file->zephyr_file, full_path, FS_O_READ);
    if (ret != 0) {
        LOG_ERR("Failed to open file %s: %d", full_path, ret);
        release_file_handle(internal_file);
        return (ret == -ENOENT) ? FS_ERROR_FILE_NOT_FOUND : FS_ERROR_OPEN_FAILED;
    }
    
    /* Get file size */
    struct fs_dirent entry;
    ret = fs_stat(full_path, &entry);
    if (ret == 0) {
        file->size = entry.size;
    } else {
        file->size = 0;
    }
    
    /* Set up the public file handle */
    file->handle = internal_file;
    file->is_open = true;
    file->position = 0;
    
    LOG_DBG("Opened file %s, size: %zu bytes", path, file->size);
    
    return FS_OK;
}

fs_result_t media_fs_close(fs_file_t *file)
{
    int ret;
    struct internal_file *internal_file;
    
    if (!file || !file->handle || !file->is_open) {
        return FS_ERROR_INVALID_PARAM;
    }
    
    internal_file = (struct internal_file *)file->handle;
    
    ret = fs_close(&internal_file->zephyr_file);
    if (ret != 0) {
        LOG_ERR("Failed to close file: %d", ret);
    }
    
    release_file_handle(internal_file);
    
    file->handle = NULL;
    file->is_open = false;
    file->size = 0;
    file->position = 0;
    
    return (ret == 0) ? FS_OK : FS_ERROR_OPEN_FAILED;
}

fs_result_t media_fs_read(fs_file_t *file, void *buffer, size_t size, size_t *bytes_read)
{
    int ret;
    struct internal_file *internal_file;
    
    if (!file || !file->handle || !file->is_open || !buffer || !bytes_read) {
        return FS_ERROR_INVALID_PARAM;
    }
    
    internal_file = (struct internal_file *)file->handle;
    
    ret = fs_read(&internal_file->zephyr_file, buffer, size);
    if (ret < 0) {
        LOG_ERR("Failed to read file: %d", ret);
        *bytes_read = 0;
        return FS_ERROR_READ_FAILED;
    }
    
    *bytes_read = ret;
    file->position += ret;
    
    return FS_OK;
}

fs_result_t media_fs_seek(fs_file_t *file, size_t offset)
{
    int ret;
    struct internal_file *internal_file;
    
    if (!file || !file->handle || !file->is_open) {
        return FS_ERROR_INVALID_PARAM;
    }
    
    internal_file = (struct internal_file *)file->handle;
    
    ret = fs_seek(&internal_file->zephyr_file, offset, FS_SEEK_SET);
    if (ret != 0) {
        LOG_ERR("Failed to seek file: %d", ret);
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
    struct fs_dirent entry;
    int ret;
    
    if (!path || !media_fs_is_ready()) {
        return false;
    }
    
    build_full_path(full_path, sizeof(full_path), path);
    ret = fs_stat(full_path, &entry);
    
    return (ret == 0);
}

fs_result_t media_fs_get_size(const char *path, size_t *size)
{
    char full_path[512];
    struct fs_dirent entry;
    int ret;
    
    if (!path || !size || !media_fs_is_ready()) {
        return FS_ERROR_INVALID_PARAM;
    }
    
    build_full_path(full_path, sizeof(full_path), path);
    ret = fs_stat(full_path, &entry);
    if (ret != 0) {
        return (ret == -ENOENT) ? FS_ERROR_FILE_NOT_FOUND : FS_ERROR_OPEN_FAILED;
    }
    
    *size = entry.size;
    return FS_OK;
}

fs_result_t media_fs_opendir(fs_dir_t *dir, const char *path)
{
    int ret;
    char full_path[512];
    struct internal_dir *internal_dir;
    
    if (!dir) {
        return FS_ERROR_INVALID_PARAM;
    }
    
    if (!media_fs_is_ready()) {
        return FS_ERROR_NOT_INITIALIZED;
    }
    
    /* Get a free directory handle */
    internal_dir = get_free_dir_handle();
    if (!internal_dir) {
        LOG_ERR("No free directory handles available");
        return FS_ERROR_NO_MEMORY;
    }
    
    /* Build full path */
    build_full_path(full_path, sizeof(full_path), path);
    
    /* Initialize Zephyr directory structure */
    fs_dir_t_init(&internal_dir->zephyr_dir);
    
    /* Open directory */
    ret = fs_opendir(&internal_dir->zephyr_dir, full_path);
    if (ret != 0) {
        LOG_ERR("Failed to open directory %s: %d", full_path, ret);
        release_dir_handle(internal_dir);
        return FS_ERROR_OPEN_FAILED;
    }
    
    /* Set up the public directory handle */
    dir->handle = internal_dir;
    dir->is_open = true;
    
    LOG_DBG("Opened directory %s", path ? path : "root");
    
    return FS_OK;
}

fs_result_t media_fs_closedir(fs_dir_t *dir)
{
    int ret;
    struct internal_dir *internal_dir;
    
    if (!dir || !dir->handle || !dir->is_open) {
        return FS_ERROR_INVALID_PARAM;
    }
    
    internal_dir = (struct internal_dir *)dir->handle;
    
    ret = fs_closedir(&internal_dir->zephyr_dir);
    if (ret != 0) {
        LOG_ERR("Failed to close directory: %d", ret);
    }
    
    release_dir_handle(internal_dir);
    
    dir->handle = NULL;
    dir->is_open = false;
    
    return (ret == 0) ? FS_OK : FS_ERROR_OPEN_FAILED;
}

fs_result_t media_fs_readdir(fs_dir_t *dir, fs_dirent_t *entry)
{
    int ret;
    struct internal_dir *internal_dir;
    struct fs_dirent zephyr_entry;
    
    if (!dir || !dir->handle || !dir->is_open || !entry) {
        return FS_ERROR_INVALID_PARAM;
    }
    
    internal_dir = (struct internal_dir *)dir->handle;
    
    ret = fs_readdir(&internal_dir->zephyr_dir, &zephyr_entry);
    if (ret != 0) {
        return (ret == -ENOENT) ? FS_ERROR_FILE_NOT_FOUND : FS_ERROR_READ_FAILED;
    }
    
    /* Check if we've reached the end */
    if (zephyr_entry.name[0] == '\0') {
        return FS_ERROR_FILE_NOT_FOUND;
    }
    
    /* Copy to our structure */
    strncpy(entry->name, zephyr_entry.name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    entry->size = zephyr_entry.size;
    entry->is_directory = (zephyr_entry.type == FS_DIR_ENTRY_DIR);
    
    return FS_OK;
}

fs_result_t media_fs_get_stats(fs_stats_t *stats)
{
    struct fs_statvfs statvfs;
    int ret;
    
    if (!stats || !media_fs_is_ready()) {
        return FS_ERROR_INVALID_PARAM;
    }
    
    ret = fs_statvfs(SD_MOUNT_POINT, &statvfs);
    if (ret != 0) {
        LOG_ERR("Failed to get file system stats: %d", ret);
        return FS_ERROR_OPEN_FAILED;
    }
    
    stats->total_space = statvfs.f_frsize * statvfs.f_blocks;
    stats->free_space = statvfs.f_frsize * statvfs.f_bfree;
    stats->used_space = stats->total_space - stats->free_space;
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
    LOG_INF("Found %zu audio files", found);
    
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