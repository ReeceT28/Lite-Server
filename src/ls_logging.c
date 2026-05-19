#define _GNU_SOURCE
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "ls_logging.h"

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

    return 1;
}

int ls_log_close(ls_log_cfg_t* log)
{
    if(close(log->log_fd) == -1) {
        return -1;
    }
    return 1;
}

int ls_log_write(ls_log_cfg_t* log, const char* data, size_t len)
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

    return 1;
    pthread_mutex_unlock(&ls_log_mutex);
}