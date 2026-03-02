#pragma once 
#include <stddef.h>
#include <stdint.h>
#include "ls_http_request.h"

/* From the header_chars array defined in http_parser.c we only need a range of 77 characters */
typedef struct ls_trie_node_s
{
    struct ls_trie_node_s *children[77];
    int header_id;     /* LS_HTTP_HDR_* if terminal, else LS_HTTP_HDR_UNKOWN */
}  ls_trie_node_t;

void ls_trie_insert(ls_trie_node_t* root, const char* header, int header_id);
ls_trie_node_t* ls_http_init_header_trie();