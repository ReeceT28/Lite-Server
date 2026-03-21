#pragma once 
#include "ls_http_request.h"

int parser_run_benchmark(int iterations);
int parser_run_print_test(void);
void ls_print_parsed_request(const ls_http_request_t* req);