/**
 * @file error_handling.c
 * @brief System-wide error handling and logging utilities
 * 
 * Due to this project being built in increments, I believed it was
 * necessary to have the appropriate error handling code. This file 
 * was heavily influenced by Claude Sonnet 4. Specifically, since in
 * the past I have not implemented error handling to a system-wide scale,
 * in the interest of time, I needed a solution that would work to help 
 * move the project forward.
 * 
 * This current implementation handles errors pertaining to v1.0 of the
 * project:
 * 
 * v.1.0: File system architecture and implementation (including SD card support).
 * v.2.0: Audio system pipeline and playback (including WAV decoding) --> see audio/.
 * 
 */

#include "error_handling.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>
#include <stdio.h>
#include <string.h>

LOG_MODULE_REGISTER(error_handler, LOG_LEVEL_DBG);

/* Error statistics */
static struct {
    uint32_t total_errors;
    uint32_t critical_errors;
    uint32_t warnings;
    uint32_t last_error_code;
    int64_t last_error_time;
    char last_error_msg[ERROR_MSG_MAX_LEN];
} error_stats = {0};

/* Error history ring buffer */
#define ERROR_HISTORY_SIZE 16
static struct {
    error_code_t code;
    error_severity_t severity;
    int64_t timestamp;
    char message[64];
} error_history[ERROR_HISTORY_SIZE];

static uint8_t error_history_index = 0;
static uint8_t error_history_count = 0;

/* Mutex for thread-safe error handling */
K_MUTEX_DEFINE(error_mutex);

/* Forward declarations */
static void add_to_history(error_code_t code, error_severity_t severity, 
                          const char *message);
static const char *severity_to_string(error_severity_t severity);
static const char *code_to_string(error_code_t code);

void error_handler_init(void)
{
    LOG_INF("Error handler initialized");
    memset(&error_stats, 0, sizeof(error_stats));
    memset(error_history, 0, sizeof(error_history));
    error_history_index = 0;
    error_history_count = 0;
}

void handle_error(error_code_t code, error_severity_t severity, 
                 const char *message, const char *file, int line)
{
    k_mutex_lock(&error_mutex, K_FOREVER);
    
    /* Update statistics */
    error_stats.total_errors++;
    error_stats.last_error_code = code;
    error_stats.last_error_time = k_uptime_get();
    
    if (message) {
        strncpy(error_stats.last_error_msg, message, 
                sizeof(error_stats.last_error_msg) - 1);
        error_stats.last_error_msg[sizeof(error_stats.last_error_msg) - 1] = '\0';
    } else {
        error_stats.last_error_msg[0] = '\0';
    }
    
    switch (severity) {
        case ERROR_SEVERITY_CRITICAL:
            error_stats.critical_errors++;
            break;
        case ERROR_SEVERITY_WARNING:
            error_stats.warnings++;
            break;
        default:
            break;
    }
    
    /* Add to history */
    add_to_history(code, severity, message);
    
    /* Format full error message */
    char full_message[256];
    if (message) {
        snprintf(full_message, sizeof(full_message), 
                "[%s] %s (%s) at %s:%d", 
                severity_to_string(severity),
                message,
                code_to_string(code),
                file ? file : "unknown",
                line);
    } else {
        snprintf(full_message, sizeof(full_message),
                "[%s] %s at %s:%d",
                severity_to_string(severity),
                code_to_string(code),
                file ? file : "unknown",
                line);
    }
    
    /* Log based on severity */
    switch (severity) {
        case ERROR_SEVERITY_CRITICAL:
            LOG_ERR("%s", full_message);
            break;
        case ERROR_SEVERITY_ERROR:
            LOG_ERR("%s", full_message);
            break;
        case ERROR_SEVERITY_WARNING:
            LOG_WRN("%s", full_message);
            break;
        case ERROR_SEVERITY_INFO:
            LOG_INF("%s", full_message);
            break;
    }
    
    k_mutex_unlock(&error_mutex);
    
    /* Handle critical errors */
    if (severity == ERROR_SEVERITY_CRITICAL) {
        LOG_ERR("CRITICAL ERROR - System may be unstable");
        
        /* Add delay to ensure log is flushed */
        k_sleep(K_MSEC(100));
        
        /* For development, we might want to halt instead of reboot */
        #ifdef CONFIG_DEBUG
        LOG_ERR("Halting system due to critical error (debug mode)");
        k_panic();
        #else
        LOG_ERR("Rebooting system due to critical error");
        sys_reboot(SYS_REBOOT_COLD);
        #endif
    }
}

error_stats_t error_get_stats(void)
{
    error_stats_t stats;
    
    k_mutex_lock(&error_mutex, K_FOREVER);
    
    stats.total_errors = error_stats.total_errors;
    stats.critical_errors = error_stats.critical_errors;
    stats.warnings = error_stats.warnings;
    stats.last_error_code = error_stats.last_error_code;
    stats.last_error_time = error_stats.last_error_time;
    strncpy(stats.last_error_msg, error_stats.last_error_msg, 
            sizeof(stats.last_error_msg));
    
    k_mutex_unlock(&error_mutex);
    
    return stats;
}

void error_clear_stats(void)
{
    k_mutex_lock(&error_mutex, K_FOREVER);
    memset(&error_stats, 0, sizeof(error_stats));
    k_mutex_unlock(&error_mutex);
    
    LOG_INF("Error statistics cleared");
}

void error_print_history(void)
{
    k_mutex_lock(&error_mutex, K_FOREVER);
    
    LOG_INF("=== Error History (%d entries) ===", error_history_count);
    
    if (error_history_count == 0) {
        LOG_INF("No errors recorded");
        k_mutex_unlock(&error_mutex);
        return;
    }
    
    /* Print from oldest to newest */
    uint8_t start_idx = (error_history_count < ERROR_HISTORY_SIZE) ? 
                        0 : error_history_index;
    
    for (int i = 0; i < error_history_count; i++) {
        uint8_t idx = (start_idx + i) % ERROR_HISTORY_SIZE;
        
        LOG_INF("%d. [T+%lld] %s: %s (%s)",
                i + 1,
                error_history[idx].timestamp,
                severity_to_string(error_history[idx].severity),
                error_history[idx].message[0] ? error_history[idx].message : "No message",
                code_to_string(error_history[idx].code));
    }
    
    LOG_INF("=== End Error History ===");
    
    k_mutex_unlock(&error_mutex);
}

bool error_recovery_possible(error_code_t code)
{
    switch (code) {
        case ERROR_CODE_SD_CARD_ERROR:
        case ERROR_CODE_AUDIO_BUFFER_UNDERRUN:
        case ERROR_CODE_FILE_READ_ERROR:
        case ERROR_CODE_NETWORK_ERROR:
            return true;
            
        case ERROR_CODE_MEMORY_ALLOCATION_FAILED:
        case ERROR_CODE_HARDWARE_FAILURE:
        case ERROR_CODE_SYSTEM_FAULT:
            return false;
            
        default:
            return false;
    }
}

void error_attempt_recovery(error_code_t code)
{
    LOG_INF("Attempting recovery for error: %s", code_to_string(code));
    
    switch (code) {
        case ERROR_CODE_SD_CARD_ERROR:
            LOG_INF("Recovery: Reinitializing SD card interface");
            /* Implementation would call fs_deinit() and fs_init() */
            break;
            
        case ERROR_CODE_AUDIO_BUFFER_UNDERRUN:
            LOG_INF("Recovery: Resetting audio buffers");
            /* Implementation would reset audio system */
            break;
            
        case ERROR_CODE_FILE_READ_ERROR:
            LOG_INF("Recovery: Closing and reopening file");
            /* Implementation would retry file operation */
            break;
            
        case ERROR_CODE_NETWORK_ERROR:
            LOG_INF("Recovery: Reconnecting network interface");
            /* Implementation would reset network stack */
            break;
            
        default:
            LOG_WRN("No recovery procedure defined for error code %d", code);
            break;
    }
}

/* Helper functions */
static void add_to_history(error_code_t code, error_severity_t severity, 
                          const char *message)
{
    error_history[error_history_index].code = code;
    error_history[error_history_index].severity = severity;
    error_history[error_history_index].timestamp = k_uptime_get();
    
    if (message) {
        strncpy(error_history[error_history_index].message, message, 
                sizeof(error_history[error_history_index].message) - 1);
        error_history[error_history_index].message[
            sizeof(error_history[error_history_index].message) - 1] = '\0';
    } else {
        error_history[error_history_index].message[0] = '\0';
    }
    
    error_history_index = (error_history_index + 1) % ERROR_HISTORY_SIZE;
    
    if (error_history_count < ERROR_HISTORY_SIZE) {
        error_history_count++;
    }
}

static const char *severity_to_string(error_severity_t severity)
{
    switch (severity) {
        case ERROR_SEVERITY_CRITICAL: return "CRITICAL";
        case ERROR_SEVERITY_ERROR:    return "ERROR";
        case ERROR_SEVERITY_WARNING:  return "WARNING";
        case ERROR_SEVERITY_INFO:     return "INFO";
        default:                      return "UNKNOWN";
    }
}

static const char *code_to_string(error_code_t code)
{
    switch (code) {
        case ERROR_CODE_SUCCESS:                    return "SUCCESS";
        case ERROR_CODE_GENERIC_ERROR:              return "GENERIC_ERROR";
        case ERROR_CODE_INVALID_PARAMETER:          return "INVALID_PARAMETER";
        case ERROR_CODE_MEMORY_ALLOCATION_FAILED:   return "MEMORY_ALLOCATION_FAILED";
        case ERROR_CODE_FILE_NOT_FOUND:             return "FILE_NOT_FOUND";
        case ERROR_CODE_FILE_READ_ERROR:            return "FILE_READ_ERROR";
        case ERROR_CODE_FILE_WRITE_ERROR:           return "FILE_WRITE_ERROR";
        case ERROR_CODE_SD_CARD_ERROR:              return "SD_CARD_ERROR";
        case ERROR_CODE_AUDIO_INIT_FAILED:          return "AUDIO_INIT_FAILED";
        case ERROR_CODE_AUDIO_PLAY_FAILED:          return "AUDIO_PLAY_FAILED";
        case ERROR_CODE_AUDIO_BUFFER_UNDERRUN:      return "AUDIO_BUFFER_UNDERRUN";
        case ERROR_CODE_SENSOR_READ_FAILED:         return "SENSOR_READ_FAILED";
        case ERROR_CODE_NETWORK_ERROR:              return "NETWORK_ERROR";
        case ERROR_CODE_BLUETOOTH_ERROR:            return "BLUETOOTH_ERROR";
        case ERROR_CODE_HARDWARE_FAILURE:           return "HARDWARE_FAILURE";
        case ERROR_CODE_SYSTEM_FAULT:               return "SYSTEM_FAULT";
        default:                                    return "UNKNOWN_ERROR";
    }
}