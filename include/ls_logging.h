#pragma once
#include <stddef.h>
#include <stdint.h>
#include "ls_http_response.h"
#include "ls_server.h"

/*
 * For logging "INFO" there will be more fine grained control because some information like connection accepted/ disconnected may fill up logs and not be very relevant 
 * For logging "DEBUG" events there will not be as much control because there would have to be an extremely large amount of options and it is likely most information will be needed for debugging 
 */
#define LS_ENABLE_LOGGING 1
#define LS_LOG_ENABLED          1<<0 /* Enable logging*/
#define LS_LOG_FORMAT_COMBINED  1<<1 /* Use Combined Log Format for HTTP request logging */
#define LS_LOG_DEBUG            1<<2 /* Enable logging of information that could be useful when debugging errors */
#define LS_LOG_EVENT_CONNECTION 1<<3 /* Log whenever the server receives a connection request*/
#define LS_LOG_EVENT_DISCONNECT 1<<4 /* Log disconnections */
#define LS_LOG_ERROR            1<<5 /* Log errors */
#define LS_LOG_WARNING          1<<6 /* Log events that shouldn't cause execution of the program to stop but shouldn't normally occur if the program is behaving as intended */

#define LS_LOG_INFO (LS_LOG_FORMAT_COMBINED | LS_LOG_EVENT_DISCONNECT)

typedef struct ls_log_cfg_s{
    uint32_t log_cfgs;
    const char*  file_path;
    size_t   file_length;
    int      log_fd;
} ls_log_cfg_t;

int ls_log_init(uint32_t log_cfgs, const char* file_path, ls_server_context_t* server); /* Null terminated file path*/
int ls_log_close(ls_log_cfg_t* log);
void ls_log_write(ls_log_cfg_t* log, const char* data, size_t len, uint32_t event_flag);
void ls_log_combined(ls_http_response_t* res, ls_connection_t* conn);
void ls_log_disconnect(int fd, ls_log_cfg_t* log);
