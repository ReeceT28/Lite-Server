#include <ctype.h>
#include "ls_trie.h"


static const unsigned char header_chars[256] = {
    [0 ... 255] = 255,
    /* Uppercase maps to lowercase */
    ['A'] = 0, ['B'] = 1, ['C'] = 2, ['D'] = 3,
    ['E'] = 4, ['F'] = 5, ['G'] = 6, ['H'] = 7,
    ['I'] = 8, ['J'] = 9, ['K'] = 10, ['L'] = 11,
    ['M'] = 12, ['N'] = 13, ['O'] = 14, ['P'] = 15,
    ['Q'] = 16, ['R'] = 17, ['S'] = 18, ['T'] = 19,
    ['U'] = 20, ['V'] = 21, ['W'] = 22, ['X'] = 23,
    ['Y'] = 24, ['Z'] = 25,
    /* Lowercase map to themselves */
    ['a'] = 0, ['b'] = 1, ['c'] = 2, ['d'] = 3,
    ['e'] = 4, ['f'] = 5, ['g'] = 6, ['h'] = 7,
    ['i'] = 8, ['j'] = 9, ['k'] = 10, ['l'] = 11,
    ['m'] = 12, ['n'] = 13, ['o'] = 14, ['p'] = 15,
    ['q'] = 16, ['r'] = 17, ['s'] = 18, ['t'] = 19,
    ['u'] = 20, ['v'] = 21, ['w'] = 22, ['x'] = 23,
    ['y'] = 24, ['z'] = 25,
    /* Digits map to themselves */
    ['0'] = 26, ['1'] = 27, ['2'] = 28, ['3'] = 29,
    ['4'] = 30, ['5'] = 31, ['6'] = 32, ['7'] = 33,
    ['8'] = 34, ['9'] = 35,
    /* Dash maps to itself */
    ['-'] = 36,
};

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
        u_char c = header_chars[(u_char)*header];

        if (current->children[c] == NULL) {
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