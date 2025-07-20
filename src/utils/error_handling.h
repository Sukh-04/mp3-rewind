/**
 * @file error_handling.h
 * @brief System-wide error handling and logging utilities
 * 
 * This abstraction provides a centralized way to handle errors across the system.
 * For more information, please refer to the documentation in "error_handling.c".
 * 
 * The hope is that this will allow for easier debugging and error management. 
 * 
 */

#ifndef ERROR_HANDLING_H
#define ERROR_HANDLING_H

#include <stdint.h>
#include <stdbool.h>


/* Maximum length for error messages */
#define ERROR_MSG_MAX_LEN 128

/* Error severity levels */
typedef enum {
    ERROR_SEVERITY_INFO = 0,     /* Informational only */
    ERROR_SEVERITY_WARNING,      /* Warning condition */
    ERROR_SEVERITY_ERROR,        /* Error condition */
    ERROR_SEVERITY_CRITICAL      /* Critical error - system may reboot */
} error_severity_t;

/* Standard error codes */
typedef enum {
    ERROR_CODE_SUCCESS = 0,
    
    /* Generic errors */
    ERROR_CODE_GENERIC_ERROR,
    ERROR_CODE_INVALID_PARAMETER,
    ERROR_CODE_MEMORY_ALLOCATION_FAILED,
    
    /* File system errors */
    ERROR_CODE_FILE_NOT_FOUND,
    ERROR_CODE_FILE_READ_ERROR,
    ERROR_CODE_FILE_WRITE_ERROR,
    ERROR_CODE_SD_CARD_ERROR,
    
    /* Audio system errors */
    ERROR_CODE_AUDIO_INIT_FAILED,
    ERROR_CODE_AUDIO_PLAY_FAILED,
    ERROR_CODE_AUDIO_BUFFER_UNDERRUN,
    
    /* Sensor errors */
    ERROR_CODE_SENSOR_READ_FAILED,
    
    /* Communication errors */
    ERROR_CODE_NETWORK_ERROR,
    ERROR_CODE_BLUETOOTH_ERROR,
    
    /* Hardware errors */
    ERROR_CODE_HARDWARE_FAILURE,
    ERROR_CODE_SYSTEM_FAULT,
    
    /* Add more error codes as needed */
    ERROR_CODE_MAX
} error_code_t;

/* Error statistics structure */
typedef struct {
    uint32_t total_errors;       /* Total number of errors */
    uint32_t critical_errors;    /* Number of critical errors */
    uint32_t warnings;           /* Number of warnings */
    uint32_t last_error_code;    /* Last error code */
    int64_t last_error_time;     /* Timestamp of last error */
    char last_error_msg[ERROR_MSG_MAX_LEN]; /* Last error message */
} error_stats_t;

/**
 * @brief Initialize the error handling system
 * 
 * Should be called early in system initialization.
 */
void error_handler_init(void);

/**
 * @brief Handle an error condition
 * 
 * @param code Error code
 * @param severity Error severity level
 * @param message Optional error message (can be NULL)
 * @param file Source file name (use __FILE__)
 * @param line Source line number (use __LINE__)
 */
void handle_error(error_code_t code, error_severity_t severity, 
                 const char *message, const char *file, int line);

/**
 * @brief Get error statistics
 * 
 * @return Current error statistics
 */
error_stats_t error_get_stats(void);

/**
 * @brief Clear error statistics
 * 
 * Resets all error counters to zero.
 */
void error_clear_stats(void);

/**
 * @brief Print error history to log
 * 
 * Displays the last several errors that occurred.
 */
void error_print_history(void);

/**
 * @brief Check if recovery is possible for an error
 * 
 * @param code Error code to check
 * @return true if recovery is possible, false otherwise
 */
bool error_recovery_possible(error_code_t code);

/**
 * @brief Attempt to recover from an error
 * 
 * @param code Error code to recover from
 */
void error_attempt_recovery(error_code_t code);

/* Convenience macros for error reporting */
#define REPORT_CRITICAL_ERROR(code, msg) \
    handle_error(code, ERROR_SEVERITY_CRITICAL, msg, __FILE__, __LINE__)

#define REPORT_ERROR(code, msg) \
    handle_error(code, ERROR_SEVERITY_ERROR, msg, __FILE__, __LINE__)

#define REPORT_WARNING(code, msg) \
    handle_error(code, ERROR_SEVERITY_WARNING, msg, __FILE__, __LINE__)

#define REPORT_INFO(code, msg) \
    handle_error(code, ERROR_SEVERITY_INFO, msg, __FILE__, __LINE__)

/* Macros for error handling with automatic recovery attempt */
#define HANDLE_ERROR_WITH_RECOVERY(code, msg) \
    do { \
        handle_error(code, ERROR_SEVERITY_ERROR, msg, __FILE__, __LINE__); \
        if (error_recovery_possible(code)) { \
            error_attempt_recovery(code); \
        } \
    } while(0)

/* Assertion-style macros */
#define ERROR_RETURN_IF_NULL(ptr, code) \
    do { \
        if ((ptr) == NULL) { \
            REPORT_ERROR(code, #ptr " is NULL"); \
            return code; \
        } \
    } while(0)

#define ERROR_RETURN_IF_FAIL(condition, code, msg) \
    do { \
        if (!(condition)) { \
            REPORT_ERROR(code, msg); \
            return code; \
        } \
    } while(0)

#endif /* ERROR_HANDLING_H */