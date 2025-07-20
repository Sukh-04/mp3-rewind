/**
 * @file sim_main.c
 * @brief Simulation version of main.c for testing on host system
 * 
 * This is a simplified version of main.c that can be compiled and run
 * on the host system (macOS) for testing the file system logic without
 * requiring the embedded hardware.
 * 
 * This file is part of a suite of files that simulate the fs.c 
 * file system implementation for testing purposes. Please note 
 * "sim_fs.c" for further documentation. 
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "storage/fs.h"
#include "utils/error_handling.h"

/* Application version */
#define APP_VERSION_MAJOR 1
#define APP_VERSION_MINOR 0
#define APP_VERSION_PATCH 0

/* Test parameters */
#define TEST_BUFFER_SIZE 512
#define MAX_AUDIO_FILES 20

/* Application state */
typedef enum {
    APP_STATE_INIT,
    APP_STATE_READY,
    APP_STATE_ERROR,
    APP_STATE_SHUTDOWN
} app_state_t;

static app_state_t app_state = APP_STATE_INIT;

/* Simulation functions to replace Zephyr kernel calls */
void k_sleep_ms(int ms) {
    usleep(ms * 1000);
}

/* Function prototypes */
static void print_banner(void);
static int initialize_systems(void);
static int test_file_system(void);
static int scan_audio_files(void);
static int test_file_operations(const char *filename);
static void print_system_stats(void);
static void cleanup_systems(void);

/**
 * @brief Main application entry point
 */
int main(void)
{
    int ret;
    
    print_banner();
    
    /* Initialize error handling first */
    error_handler_init();
    REPORT_INFO(ERROR_CODE_SUCCESS, "Application starting");
    
    /* Initialize all systems */
    ret = initialize_systems();
    if (ret != 0) {
        REPORT_CRITICAL_ERROR(ERROR_CODE_SYSTEM_FAULT, "System initialization failed");
        goto error_exit;
    }
    
    app_state = APP_STATE_READY;
    printf("[SIM_INF] main: System initialization complete\n");
    
    /* Run file system tests */
    ret = test_file_system();
    if (ret != 0) {
        REPORT_ERROR(ERROR_CODE_SD_CARD_ERROR, "File system tests failed");
        /* Continue running for debugging */
    }
    
    /* Scan for audio files */
    ret = scan_audio_files();
    if (ret != 0) {
        REPORT_WARNING(ERROR_CODE_FILE_NOT_FOUND, "Audio file scan incomplete");
    }
    
    /* Print system statistics */
    print_system_stats();
    
    printf("[SIM_INF] main: Simulation test completed successfully!\n");
    printf("[SIM_INF] main: In real embedded mode, the system would now enter monitoring loop\n");
    
error_exit:
    app_state = APP_STATE_SHUTDOWN;
    cleanup_systems();
    
    /* Print final error report */
    printf("[SIM_INF] main: === Final System Report ===\n");
    error_print_history();
    print_system_stats();
    
    if (app_state == APP_STATE_ERROR) {
        printf("[SIM_ERR] main: Application exiting due to errors\n");
        return -1;
    }
    
    printf("[SIM_INF] main: Application shutdown complete\n");
    return 0;
}

/**
 * @brief Print application banner
 */
static void print_banner(void)
{
    printf("\n");
    printf("=====================================\n");
    printf("  Media Player (MP3 Rewind) v%d.%d.%d\n", 
           APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_PATCH);
    printf("  SIMULATION MODE - Host Testing\n");
    printf("  File System Logic Validation\n");
    printf("=====================================\n");
    printf("\n");
}

/**
 * @brief Initialize all system components
 */
static int initialize_systems(void)
{
    fs_result_t fs_ret;
    
    printf("[SIM_INF] main: Initializing system components...\n");
    
    /* Initialize file system */
    printf("[SIM_INF] main: Initializing file system...\n");
    fs_ret = media_fs_init();
    if (fs_ret != FS_OK) {
        printf("[SIM_ERR] main: File system initialization failed: %s\n", media_fs_error_to_string(fs_ret));
        REPORT_ERROR(ERROR_CODE_SD_CARD_ERROR, "Failed to initialize file system");
        return -1;
    }
    
    printf("[SIM_INF] main: File system initialized successfully\n");
    
    /* Add delay to simulate embedded system timing */
    k_sleep_ms(100);
    
    printf("[SIM_INF] main: System initialization complete\n");
    return 0;
}

/**
 * @brief Test file system functionality
 */
static int test_file_system(void)
{
    fs_result_t result;
    fs_stats_t stats;
    
    printf("[SIM_INF] main: === File System Tests ===\n");
    
    /* Test 1: Check if file system is ready */
    if (!media_fs_is_ready()) {
        printf("[SIM_ERR] main: Test 1 FAILED: File system not ready\n");
        return -1;
    }
    printf("[SIM_INF] main: Test 1 PASSED: File system is ready\n");
    
    /* Test 2: Get file system statistics */
    result = media_fs_get_stats(&stats);
    if (result != FS_OK) {
        printf("[SIM_ERR] main: Test 2 FAILED: Cannot get file system stats: %s\n", 
                media_fs_error_to_string(result));
        return -1;
    }
    
    printf("[SIM_INF] main: Test 2 PASSED: File system statistics:\n");
    printf("[SIM_INF] main:   Total space: %zu bytes (%.1f MB)\n", 
            stats.total_space, (float)stats.total_space / (1024*1024));
    printf("[SIM_INF] main:   Used space:  %zu bytes (%.1f MB)\n", 
            stats.used_space, (float)stats.used_space / (1024*1024));
    printf("[SIM_INF] main:   Free space:  %zu bytes (%.1f MB)\n", 
            stats.free_space, (float)stats.free_space / (1024*1024));
    
    /* Test 3: Test directory operations */
    fs_dir_t dir;
    result = media_fs_opendir(&dir, NULL); /* Root directory */
    if (result != FS_OK) {
        printf("[SIM_ERR] main: Test 3 FAILED: Cannot open root directory: %s\n", 
                media_fs_error_to_string(result));
        return -1;
    }
    
    printf("[SIM_INF] main: Test 3 PASSED: Root directory opened\n");
    
    /* List some files in root directory */
    fs_dirent_t entry;
    int file_count = 0;
    printf("[SIM_INF] main: Root directory contents:\n");
    
    while (media_fs_readdir(&dir, &entry) == FS_OK && file_count < 10) {
        printf("[SIM_INF] main:   %s %s (%zu bytes)\n", 
                entry.is_directory ? "[DIR]" : "[FILE]",
                entry.name, 
                entry.size);
        file_count++;
    }
    
    if (file_count == 0) {
        printf("[SIM_WRN] main: Root directory is empty\n");
    }
    
    media_fs_closedir(&dir);
    
    /* Test 4: Test file existence check */
    const char *test_files[] = {
        "test.txt",
        "audio.wav", 
        "demo_track.mp3",
        "nonexistent.file"
    };
    
    printf("[SIM_INF] main: Test 4: File existence checks:\n");
    for (int i = 0; i < 4; i++) {
        bool exists = media_fs_exists(test_files[i]);
        printf("[SIM_INF] main:   %s: %s\n", test_files[i], exists ? "EXISTS" : "NOT FOUND");
        
        if (exists) {
            /* Test file operations on existing file */
            if (test_file_operations(test_files[i]) == 0) {
                printf("[SIM_INF] main:     File operations test PASSED\n");
            } else {
                printf("[SIM_WRN] main:     File operations test FAILED\n");
            }
        }
    }
    
    printf("[SIM_INF] main: === File System Tests Complete ===\n\n");
    return 0;
}

/**
 * @brief Scan for audio files on the SD card
 */
static int scan_audio_files(void)
{
    fs_result_t result;
    char audio_files[MAX_AUDIO_FILES][256];
    size_t file_count;
    
    printf("[SIM_INF] main: === Audio File Scan ===\n");
    
    /* Scan root directory for audio files */
    result = media_fs_list_audio_files(NULL, audio_files, MAX_AUDIO_FILES, &file_count);
    if (result != FS_OK) {
        printf("[SIM_ERR] main: Failed to scan audio files: %s\n", media_fs_error_to_string(result));
        return -1;
    }
    
    printf("[SIM_INF] main: Found %zu audio files:\n", file_count);
    
    if (file_count == 0) {
        printf("[SIM_WRN] main: No audio files found in root directory\n");
        printf("[SIM_INF] main: Note: Check test_data directory for audio files\n");
        return 0;
    }
    
    /* List all found audio files with details */
    for (size_t i = 0; i < file_count; i++) {
        size_t file_size;
        
        if (media_fs_get_size(audio_files[i], &file_size) == FS_OK) {
            printf("[SIM_INF] main:   %zu. %s (%.1f KB)\n", 
                    i + 1, 
                    audio_files[i], 
                    (float)file_size / 1024.0);
            
            /* Test reading first few bytes of audio file */
            if (i == 0) { /* Test first file only */
                printf("[SIM_INF] main:     Testing file read...\n");
                if (test_file_operations(audio_files[i]) == 0) {
                    printf("[SIM_INF] main:     Read test PASSED\n");
                } else {
                    printf("[SIM_WRN] main:     Read test FAILED\n");
                }
            }
        } else {
            printf("[SIM_ERR] main:   %zu. %s (size unknown)\n", i + 1, audio_files[i]);
        }
    }
    
    printf("[SIM_INF] main: === Audio File Scan Complete ===\n\n");
    return 0;
}

/**
 * @brief Test file operations on a specific file
 */
static int test_file_operations(const char *filename)
{
    fs_file_t file;
    fs_result_t result;
    uint8_t buffer[TEST_BUFFER_SIZE];
    size_t bytes_read;
    
    if (!filename) {
        return -1;
    }
    
    printf("[SIM_DBG] main: Testing file operations on: %s\n", filename);
    
    /* Open file */
    result = media_fs_open(&file, filename);
    if (result != FS_OK) {
        printf("[SIM_ERR] main: Failed to open file %s: %s\n", filename, media_fs_error_to_string(result));
        return -1;
    }
    
    printf("[SIM_DBG] main: File opened successfully, size: %zu bytes\n", file.size);
    
    /* Read first chunk */
    result = media_fs_read(&file, buffer, sizeof(buffer), &bytes_read);
    if (result != FS_OK) {
        printf("[SIM_ERR] main: Failed to read file %s: %s\n", filename, media_fs_error_to_string(result));
        media_fs_close(&file);
        return -1;
    }
    
    printf("[SIM_DBG] main: Read %zu bytes from file\n", bytes_read);
    
    /* Print first 16 bytes as hex for debugging */
    if (bytes_read > 0) {
        printf("[SIM_DBG] main: File header: ");
        for (size_t i = 0; i < (bytes_read < 16 ? bytes_read : 16); i++) {
            printf("%02x ", buffer[i]);
        }
        printf("\n");
        
        /* Check for common audio file signatures */
        if (bytes_read >= 4) {
            if (memcmp(buffer, "RIFF", 4) == 0) {
                printf("[SIM_INF] main: Detected WAV file signature\n");
            } else if (memcmp(buffer, "ID3", 3) == 0 || 
                      (buffer[0] == 0xFF && (buffer[1] & 0xE0) == 0xE0)) {
                printf("[SIM_INF] main: Detected MP3 file signature\n");
            } else if (memcmp(buffer, "fLaC", 4) == 0) {
                printf("[SIM_INF] main: Detected FLAC file signature\n");
            } else {
                printf("[SIM_DBG] main: Unknown file format\n");
            }
        }
    }
    
    /* Test seeking */
    if (file.size > TEST_BUFFER_SIZE) {
        size_t seek_pos = file.size / 2;
        result = media_fs_seek(&file, seek_pos);
        if (result != FS_OK) {
            printf("[SIM_ERR] main: Failed to seek in file %s: %s\n", filename, media_fs_error_to_string(result));
            media_fs_close(&file);
            return -1;
        }
        
        printf("[SIM_DBG] main: Seek to position %zu successful\n", seek_pos);
        
        /* Verify position */
        size_t current_pos = media_fs_tell(&file);
        if (current_pos != seek_pos) {
            printf("[SIM_ERR] main: Position mismatch: expected %zu, got %zu\n", seek_pos, current_pos);
            media_fs_close(&file);
            return -1;
        }
        
        /* Read from middle of file */
        result = media_fs_read(&file, buffer, 64, &bytes_read);
        if (result != FS_OK) {
            printf("[SIM_ERR] main: Failed to read from middle of file: %s\n", media_fs_error_to_string(result));
            media_fs_close(&file);
            return -1;
        }
        
        printf("[SIM_DBG] main: Read %zu bytes from middle of file\n", bytes_read);
    }
    
    /* Close file */
    result = media_fs_close(&file);
    if (result != FS_OK) {
        printf("[SIM_ERR] main: Failed to close file %s: %s\n", filename, media_fs_error_to_string(result));
        return -1;
    }
    
    printf("[SIM_DBG] main: File operations test completed successfully\n");
    return 0;
}

/**
 * @brief Print system statistics and health information
 */
static void print_system_stats(void)
{
    error_stats_t error_stats;
    fs_stats_t fs_stats;
    
    printf("[SIM_INF] main: === System Statistics ===\n");
    
    /* Print error statistics */
    error_stats = error_get_stats();
    printf("[SIM_INF] main: Error statistics:\n");
    printf("[SIM_INF] main:   Total errors: %u\n", error_stats.total_errors);
    printf("[SIM_INF] main:   Critical errors: %u\n", error_stats.critical_errors);
    printf("[SIM_INF] main:   Warnings: %u\n", error_stats.warnings);
    
    if (error_stats.total_errors > 0) {
        printf("[SIM_INF] main:   Last error: %s (%u)\n", 
                error_stats.last_error_msg,
                error_stats.last_error_code);
    }
    
    /* Print file system statistics */
    if (media_fs_is_ready() && media_fs_get_stats(&fs_stats) == FS_OK) {
        printf("[SIM_INF] main: File system:\n");
        printf("[SIM_INF] main:   Total: %.1f MB\n", (float)fs_stats.total_space / (1024*1024));
        printf("[SIM_INF] main:   Used:  %.1f MB (%.1f%%)\n", 
                (float)fs_stats.used_space / (1024*1024),
                (float)fs_stats.used_space * 100.0 / fs_stats.total_space);
        printf("[SIM_INF] main:   Free:  %.1f MB\n", (float)fs_stats.free_space / (1024*1024));
    } else {
        printf("[SIM_WRN] main: File system not available\n");
    }
    
    printf("[SIM_INF] main: === End Statistics ===\n\n");
}

/**
 * @brief Clean up system resources
 */
static void cleanup_systems(void)
{
    printf("[SIM_INF] main: Cleaning up system resources...\n");
    
    /* Deinitialize file system */
    if (media_fs_is_ready()) {
        fs_result_t result = media_fs_deinit();
        if (result == FS_OK) {
            printf("[SIM_INF] main: File system deinitialized successfully\n");
        } else {
            printf("[SIM_ERR] main: File system deinit failed: %s\n", media_fs_error_to_string(result));
        }
    }
    
    printf("[SIM_INF] main: System cleanup complete\n");
}
