#pragma once
#include <stddef.h>
#include <stdint.h>
#include "ls_server.h"

#define LS_ENABLE_LOGGING 1
#define LS_LOG_ENABLED          1<<0 /* Enable logging*/
#define LS_LOG_FORMAT_COMBINED  1<<1 /* Use Combined Log Format for HTTP request logging */
#define LS_LOG_EVENT_ERROR      1<<2 /* Enable logging of errors that are encountered e.g. Failing to write a log */
#define LS_LOG_EVENT_CONNECTION 1<<3 /* Log whenever the server receives a connection request*/
#define LS_LOG_EVENT_HTTP_REQ 1<<4 /* Log http requests*/

#if (LS_ENABLE_LOGGING)

typedef struct ls_log_cfg_s{
    uint32_t log_cfgs;
    const char*  file_path;
    size_t   file_length;
    int      log_fd;
} ls_log_cfg_t;

int ls_log_init(uint32_t log_cfgs, const char* file_path, ls_server_context_t* server); /* Null terminated file path*/
int ls_log_close(ls_log_cfg_t* log);
int ls_log_write(ls_log_cfg_t* log, const char* data, size_t len);

#define LS_LOG_INIT(log_cfgs, file_path, server) ls_log_init(log_cfgs, file_path, server);
#define LS_LOG_CLOSE(log) ls_log_close(log);


#else

#define LOG_INIT(a,b,c) (void)0;
#define LS_LOG_CLOSE(a) (void)0;


#endif 