#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include "ls_logging.h"
#include "ls_server.h"


void ls_set_root_dir(ls_server_context_t* server, const char* root)
{
    server->root = (u_char*)root;
}

ls_server_context_t* ls_create_server(uint32_t log_cfgs, const char* log_file, const char* root_dir)
{
    /* Create memory pool and initialise server context */
    ls_mem_pool_t* server_pool =  ls_init_mem_pool(LS_DEFAULT_BLOCK_SIZE);
    if(server_pool == NULL) {
        fprintf(stderr, "ERROR: Failed to create memory pool for the server");
        return NULL;
    }
    ls_server_context_t* server_context = ls_palloc(server_pool, sizeof(ls_server_context_t));
    if(server_context == NULL) {
        fprintf(stderr, "ERROR: Failed to allocate memory for the server_context");
        return NULL;
    }
    server_context->pool = server_pool;
    /* Configure logging for this server */
    /* LS_ENABLE_LOGGING | LS_LOG_INFO | LS_LOG_DEBUG | LS_LOG_ERROR | LS_LOG_WARNING*/
    if(ls_log_init(log_cfgs, log_file, server_context) == -1) {
        fprintf(stderr, "ERROR: Failed to initialise the logging module");
        return NULL;
    }
    /* ================================================== FROM THIS POINT ON USE LOGGING TO RECORD INFORMATION E.G. ERRORS  ================================================== */

    /* Create the array of resource to function pointers */
    server_context->user_functions = ls_create_array(server_pool, sizeof(ls_resource_function_t), 1);
    if(server_context->user_functions == NULL) {
        const char* msg = "ERROR: Failed to create array for server's resource to user supplied function mapping\n";
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return NULL;
    }

    /* Set the root directory where web server's files will be accessed from */
    char abs_root[PATH_MAX];
    if(realpath(root_dir, abs_root) == NULL) {
        const char* msg = "ERROR: Failed to resolve the path for the servers root directory\n";
        ls_log_write(server_context->log_cfg, msg, strlen(msg), LS_LOG_ERROR);
        return NULL;
    }

    ls_set_root_dir(server_context, abs_root);
    server_context->root_fd = open(abs_root, O_RDONLY | O_DIRECTORY);

    return server_context;
}
