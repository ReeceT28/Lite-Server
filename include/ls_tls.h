#pragma once
#include <stdint.h>
/* I am beginning to work on TLS 1.3 for fun but it will likely be quite a big project.
 * I will work on it when I am away from home and cannot benchmark HTTP/1.1 modifications
 */

/* Defined in RFC 8446 Section 4.
 * Should be stored under a uint8_t as defined as ranging from 0 -> 255
 */
enum ls_handshake_type {
    LS_CLIENT_HELLO = 1,
    LS_SERVER_HELLO = 2,
    LS_NEW_SESSION_TICKET = 4,
    LS_END_OF_EARLY_DATA = 5,
    LS_ENCRYPTED_EXTENSIONS = 8,
    LS_CERTIFICATE = 11,
    LS_CERTIFICATE_REQUEST = 13,
    LS_CERTIFICATE_VERIFY = 15,
    LS_FINISHED = 20,
    LS_KEY_UPDATE = 24,
    LS_MESSAGE_HASH = 254,
};

/* Defined in RFC 8446 Section 4.
 * Length is defined as 24 bits but there is no 24 bit container in C so just use 32 bits
 */
typedef struct ls_tls_handshake_s {
    uint8_t msg_type;
    uint32_t length;
    void* handshake;
} ls_tls_handshake_t;

typedef struct ls_chello_s {
    uint16_t protocol_version; /* Should always be 0x0303 */
} ls_chello_t;
