#pragma once
#include "ls_worker.h"



void run_server_loop(ls_worker_t* worker);
void* run_server_loop_thread(void* worker);
