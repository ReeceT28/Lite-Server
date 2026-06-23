#pragma once
#include <stddef.h>
#include <stdint.h>
#include "ls_http_response.h"
#include "ls_server.h"

#define LS_ENABLE_LOGGING 1
#define LS_LOG_ENABLED          1<<0 /* Enable logging*/
#define LS_LOG_FORMAT_COMBINED  1<<1 /* Use Combined Log Format for HTTP request logging */
#define LS_LOG_EVENT_ERROR      1<<2 /* Enable logging of errors that are encountered e.g. Failing to write a log */
#define LS_LOG_EVENT_CONNECTION 1<<3 /* Log whenever the server receives a connection request*/
#define LS_LOG_EVENT_HTTP_REQ   1<<4 /* Log http requests*/
#define LS_LOG_EVENT_DISCONNECT 1<<5 /* Log disconnections */

typedef struct ls_log_cfg_s{
    uint32_t log_cfgs;
    const char*  file_path;
    size_t   file_length;
    int      log_fd;
} ls_log_cfg_t;

int ls_log_init(uint32_t log_cfgs, const char* file_path, ls_server_context_t* server); /* Null terminated file path*/
int ls_log_close(ls_log_cfg_t* log);
int ls_log_write(ls_log_cfg_t* log, const char* data, size_t len, uint32_t event_flag);
int ls_log_combined(ls_http_response_t* res, ls_connection_t* conn);
int ls_log_disconnect(int fd, ls_log_cfg_t* log);

