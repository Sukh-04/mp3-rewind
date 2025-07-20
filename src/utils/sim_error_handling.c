/**
 * @file sim_error_handling.c
 * @brief Simplified error handling for simulation mode
 * 
 * This file is part of a suite of files that simulate the fs.c 
 * file system implementation for testing purposes. Please note 
 * "sim_fs.c" for further documentation. 
 */

#include "utils/error_handling.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static error_stats_t stats = {0};
static char last_error_msg[64] = {0};

void error_handler_init(void)
{
    memset(&stats, 0, sizeof(stats));
    printf("[SIM_INF] error_handling: Error handler initialized\n");
}

void handle_error(error_code_t code, error_severity_t severity, const char *message, const char *file, int line)
{
    stats.total_errors++;
    stats.last_error_code = code;
    stats.last_error_time = 0; /* Simplified for simulation */
    strncpy(stats.last_error_msg, message ? message : "Unknown error", sizeof(stats.last_error_msg) - 1);
    
    /* Count based on severity */
    if (severity == ERROR_SEVERITY_CRITICAL) {
        stats.critical_errors++;
        printf("[SIM_ERR] ERROR: CRITICAL - %s (code: %d) at %s:%d\n", message, code, file, line);
    } else if (severity == ERROR_SEVERITY_WARNING) {
        stats.warnings++;
        printf("[SIM_WRN] ERROR: WARNING - %s (code: %d) at %s:%d\n", message, code, file, line);
    } else if (severity == ERROR_SEVERITY_ERROR) {
        printf("[SIM_ERR] ERROR: ERROR - %s (code: %d) at %s:%d\n", message, code, file, line);
    } else {
        printf("[SIM_INF] ERROR: INFO - %s (code: %d) at %s:%d\n", message, code, file, line);
    }
}

error_stats_t error_get_stats(void)
{
    return stats;
}

bool error_recovery_possible(error_code_t code)
{
    /* Simplified recovery logic for simulation */
    return (code != ERROR_CODE_SYSTEM_FAULT);
}

void error_attempt_recovery(error_code_t code)
{
    printf("[SIM_INF] error_handling: Attempting recovery for error code %d\n", code);
    /* Simulation - just pretend recovery works */
}

void error_print_history(void)
{
    printf("[SIM_INF] error_handling: Error History Summary:\n");
    printf("[SIM_INF] error_handling:   Total: %u, Critical: %u, Warnings: %u\n", 
           stats.total_errors, stats.critical_errors, stats.warnings);
    if (stats.total_errors > 0) {
        printf("[SIM_INF] error_handling:   Last: %s (%u)\n", 
               stats.last_error_msg, stats.last_error_code);
    }
}
