#define _GNU_SOURCE
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "ls_logging.h"
#include "ls_connection.h"

#if (LS_ENABLE_LOGGING)

static pthread_mutex_t ls_log_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Null terminated file path*/
int ls_log_init(uint32_t log_cfgs, const char* file_path, ls_server_context_t* server)
{
    ls_log_cfg_t* log_cfg = ls_palloc(server->pool, sizeof(ls_log_cfg_t));
    if(log_cfg == NULL) {
        return -1;
    }
    int fd = open(file_path, O_CREAT | O_WRONLY | O_APPEND | O_CLOEXEC);
    if (fd == -1) {
        return -1;
    }
    log_cfg->log_fd = fd;
    log_cfg->file_path = file_path;
    log_cfg->file_length = strlen(file_path);
    log_cfg->log_cfgs = log_cfgs;
    server->log_cfg = log_cfg;

    return 0;
}

int ls_log_close(ls_log_cfg_t* log)
{
    if(close(log->log_fd) == -1) {
        return -1;
    }
    return 0;
}

/* Logging failure isn't treated as something critical that should halt the execution of the program because I assume that writing to stderr is a perfect failsafe.
 * This won't always be true but this is secure enough for me for now. Possibly could treat logging failures or failures to write to stderr as a critical error and halt the execution of the program
 * or maybe an approach with too many logging failures in a given amount of time but this would add complexity not needed to my program right now.
 * I am treating this as a "WARNING" event, not an ERROR for now as it doesn't halt the execution of the program (at least not for now). 
 */ 
static void ls_log_failure(ls_log_cfg_t* log, const char* data, size_t len)
{
    if(log->log_cfgs & LS_LOG_WARNING) {
        fprintf(stderr, "WARNING: An event has failed to be logged by the logging module, MESSAGE: %.*s\n", len, data);
    }
}

static int ls_log_write_nocheck(ls_log_cfg_t* log, const char* data, size_t len)
{
    int fd = log->log_fd;
    const char* ptr = data;
    size_t wrote = 0;

    pthread_mutex_lock(&ls_log_mutex);

    while(wrote < len){
        int n = write(fd, ptr, len - wrote);
        if(n >= 0) {
            wrote += n;
            ptr += n;
            continue;
        }
        if(errno == EINTR) {
            continue;
        }
        pthread_mutex_unlock(&ls_log_mutex);
        return -1;
    }
    pthread_mutex_unlock(&ls_log_mutex);
    return 0;
}

/* For simple logs you can just call ls_log_write directly, where you need more complex logic you can make its own function, check the flags and
 * then call write_nocheck
 */
void ls_log_write(ls_log_cfg_t* log, const char* data, size_t len, uint32_t event_flag)
{
    if(!((log->log_cfgs & LS_LOG_ENABLED) && (log->log_cfgs & event_flag))) return;

    if(ls_log_write_nocheck(log, data, len) == -1) {
        ls_log_failure(log, data, len);
    }
}


/* Possibly refactor to not take in conn?*/
void ls_log_combined(ls_http_response_t* res, ls_connection_t* conn)
{
    ls_log_cfg_t* log_cfg = conn->worker->server->log_cfg;
    if(!((log_cfg->log_cfgs & LS_LOG_ENABLED) && (log_cfg->log_cfgs & LS_LOG_FORMAT_COMBINED))) return;

    ls_http_request_t* req = ((ls_http_ctx_t*)conn->protocol_ctx)->req;
    int sock = conn->fd;
    struct sockaddr_storage ss;
    socklen_t len = sizeof(ss);
    if (getpeername(sock, (struct sockaddr*)&ss, &len) == -1) {
        const char* msg = "WARNING: Failed to get address of peer when building combined log format\n";
        ls_log_write(log_cfg, msg, strlen(msg), LS_LOG_WARNING);
        return;
    }

    char host[NI_MAXHOST];
    if (getnameinfo((struct sockaddr*)&ss, len, host, sizeof(host), NULL, 0, NI_NUMERICHOST) != 0) {
        const char* msg = "WARINNG: Failed to convert address of peer into text format when building combined log format\n"; 
        ls_log_write(log_cfg, msg, strlen(msg), LS_LOG_WARNING);
        return;
    }

    time_t now = time(NULL);
    struct tm tm;
    if (localtime_r(&now, &tm) == NULL) {
        const char* msg = "WARNING: Failed to create time representation when building combined log format\n"; 
        ls_log_write(log_cfg, msg, strlen(msg), LS_LOG_WARNING);
        return;
    }

    char timestr[64];
    strftime(timestr, sizeof(timestr), "%d/%b/%Y:%H:%M:%S %z", &tm);

    const char* user = "-";
    const char* req_line = (const char*)req->method_start;
    int req_line_len = req->version_end - req->method_start;
    int status = res->status;
    long size = res->file_size;

    char out[1024];
    int written = snprintf(out, sizeof(out),  "%s - %s [%s] \"%.*s\" %d %ld\n",  host, user, timestr, req_line_len, req_line, status, size);
    if (written < 0) {
        const char* msg = "WARNING: Failed to build final message when building combined log format\n";
        ls_log_write(log_cfg, msg, strlen(msg), LS_LOG_WARNING);
        return;
    }
    size_t send_len = (size_t)written;
    if (send_len >= sizeof(out)) send_len = sizeof(out) - 1;

    if(ls_log_write_nocheck(log_cfg, out, send_len) == -1) {
        ls_log_failure(log_cfg, out, send_len); 
    }
}

void ls_log_disconnect(int fd, ls_log_cfg_t* log)
{
    if(!((log->log_cfgs & LS_LOG_ENABLED) && (log->log_cfgs & LS_LOG_EVENT_DISCONNECT))) return;

    struct sockaddr_storage ss;
    socklen_t len = sizeof(ss);
    if (getpeername(fd, (struct sockaddr*)&ss, &len) == -1) {
        const char* msg = "WARNING: Failed to get address of peer when building disconnection message\n";
        ls_log_write(log, msg, strlen(msg), LS_LOG_WARNING);
        return;
    }

    char host[NI_MAXHOST];
    if (getnameinfo((struct sockaddr*)&ss, len, host, sizeof(host), NULL, 0, NI_NUMERICHOST) != 0) {
        const char* msg = "WARINNG: Failed to convert address of peer into text format when building disconnection message\n"; 
        ls_log_write(log, msg, strlen(msg), LS_LOG_WARNING);
        return;
    }

    time_t now = time(NULL);
    struct tm tm;
    if (localtime_r(&now, &tm) == NULL) {
        const char* msg = "WARNING: Failed to create time representation when building disconnection message\n"; 
        ls_log_write(log, msg, strlen(msg), LS_LOG_WARNING);
        return;
    }

    char timestr[64];
    strftime(timestr, sizeof(timestr), "%d/%b/%Y:%H:%M:%S %z", &tm);

    size_t needed = strlen(host) + strlen(" has disconnected from the server at: ") + strlen(timestr) + 2;
    char out[needed];

    int written = snprintf(out, needed, "%s%s%s\n", host, " has disconnected from the server at: ", timestr);
    if (written < 0) {
        const char* msg = "WARNING: Failed to build final message when building disconnection message\n";
        ls_log_write(log, msg, strlen(msg), LS_LOG_WARNING);
        return;
    }


    size_t send_len;
    if ((size_t)written < needed)
      send_len = (size_t)written; 
    else
      send_len = needed - 1; 

    if(ls_log_write_nocheck(log, out, send_len) == -1) {
        ls_log_failure(log, out, send_len); 
    }
}

#else

/* Casting all parameters to void will stop any warnings about unused parameters. It should also (probably) result in the function calls being completely removed (just being no-ops) or having negligible effect */

int ls_log_init(uint32_t log_cfgs, const char* file_path, ls_server_context_t* server)
{
    (void)log_cfgs; (void)file_path; (void)server;
    return 0;
}

int ls_log_close(ls_log_cfg_t* log)
{
    (void)log;
    return 0;
}

int ls_log_write(ls_log_cfg_t* log, const char* data, size_t len, uint32_t event_flag)
{
    (void)log; (void)data; (void)len; (void)event_flag;
    return 0;
}

int ls_log_disconnect(int fd, ls_log_cfg_t* log)
{
    (void)fd; (void)log;
    return 0;
}

int ls_log_combined(ls_http_response_t* res, ls_connection_t* conn)
{
    (void)res; (void)conn;
    return 0;
}

#endif
