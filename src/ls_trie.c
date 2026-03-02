#include <ctype.h>
#include "ls_trie.h"

static ls_trie_node_t* ls_trie_create_node()
{
    ls_trie_node_t* node = (ls_trie_node_t*)calloc(1, sizeof(ls_trie_node_t));
    node->header_id = LS_HTTP_HDR_UNKOWN;
    return node;
}

void ls_trie_insert(ls_trie_node_t* root, const char* header, int header_id)
{
    ls_trie_node_t* current = root;

    while (*header) {
        u_char c = (u_char)tolower(*header) - 45;

        if (!current->children[c]) {
            current->children[c] = ls_trie_create_node();
        }

        current = current->children[c];
        header++;
    }

    current->header_id = header_id;
}

ls_trie_node_t* ls_http_init_header_trie()
{
    ls_trie_node_t* root = ls_trie_create_node();

    ls_trie_insert(root, "Host",       LS_HTTP_HDR_HOST);
    ls_trie_insert(root, "Accept",     LS_HTTP_HDR_ACCEPT);
    ls_trie_insert(root, "Connection", LS_HTTP_HDR_CONNECTION);
    ls_trie_insert(root, "Referer",    LS_HTTP_HDR_REFERER);
    ls_trie_insert(root, "Cookie",     LS_HTTP_HDR_COOKIE);
    ls_trie_insert(root, "Origin",     LS_HTTP_HDR_ORIGIN);
    ls_trie_insert(root, "Origin",     LS_HTTP_HDR_ORIGIN);
    ls_trie_insert(root, "User-Agent",       LS_HTTP_HDR_USER_AGENT);
    ls_trie_insert(root, "Accept-Encoding",  LS_HTTP_HDR_ACCEPT_ENCODING);
    ls_trie_insert(root, "Accept-Language",  LS_HTTP_HDR_ACCEPT_LANGUAGE);
    ls_trie_insert(root, "Content-Type",     LS_HTTP_HDR_CONTENT_TYPE);
    ls_trie_insert(root, "Content-Length",   LS_HTTP_HDR_CONTENT_LENGTH);
    ls_trie_insert(root, "Keep-Alive",       LS_HTTP_HDR_KEEP_ALIVE);
    ls_trie_insert(root, "Authorization",    LS_HTTP_HDR_AUTHORIZATION);

    return root;
}