#pragma once
#include <stdlib.h>
#include <stdint.h>
#include "server_config.h"

#if defined(__GNUC__) || defined(__clang__)
#define LIKELY(x)   (__builtin_expect(!!(x), 1))
#define UNLIKELY(x) (__builtin_expect(!!(x), 0))
#else
#define LIKELY(x)   (x)
#define UNLIKELY(x) (x)
#endif

#define SEND_BUF_SIZE 4096
#define MAX_REQUEST_HEADERS 32
#define MAX_LEADING_CRLF 6
#define IS_OWS(c) (c == ' ' || c == '\t')

// PROBABLY MORE ACCURATE TO CALL LENIENT FAST

/*
 * ERROR CODE TABLE:
 * -1 - cursor >= end when it should not have.
 * -2 - malformed CRLF, usually CR without LF
 * -3 - too many headers on a request
 * -4 - unexpected character in token
 * -5 - too many leading CRLF before request line
 */

#define ERR_EMPTY_TOKEN_NAME -5
#define ERR_MISSING_TOKEN_DELIMITER -6



/*
 * Leniently parsing be at most Conditionally Compliant. Satisfies MUST and MUST NOT but may omit SHOULD or SHOULD NOT
 * Strict parsing will be Unconditionally Compliant. Satisfies all MUST, MUST NOTE, SHOULD and SHOULD NOT criteria. 
 *
 * Notes On Unconditional Compliance: 
 * (1) In RFC 9112 Section 2.2 it states that:
 * "When a server listening only for HTTP request messages, or processing what appears from the start-line to be an HTTP request message, receives a sequence"
 * "of octets that does not match the HTTP-message grammar aside from the ronustness exceptions listed above, the server SHOULD respond with a 400 (Bad Request)"
 * "response and close the connection". 
 * This means when parsing strictly we should return a 400 if any of the grammar is wrong at all. E.g. we need to check tokens contain ONLY valid characters
 * this is overhead likely not needed for lenient parsing
*/

// Macro to compute the bit corresponding to a character (mod 64)
#define TCHAR_BIT(c) (1ULL << ((c) % 64))

// SCRAP ALL THIS SHIT AND CREATE A DIGIT AND ALPHA BITMAP OR SOMETHING AND THEN COMBINE THEM WITH THIS MAYBE
// Lower 64 bits: ASCII 0–63
static const uint64_t tchar_lo =
    TCHAR_BIT('!') | TCHAR_BIT('#') | TCHAR_BIT('$') | TCHAR_BIT('%') | TCHAR_BIT('&') | TCHAR_BIT('\'') | TCHAR_BIT('*') | TCHAR_BIT('+') |
    TCHAR_BIT('-') | TCHAR_BIT('.') | TCHAR_BIT('0') | TCHAR_BIT('1') | TCHAR_BIT('2') | TCHAR_BIT('3')  | TCHAR_BIT('4') | TCHAR_BIT('5') |
    TCHAR_BIT('6') | TCHAR_BIT('7') | TCHAR_BIT('8') | TCHAR_BIT('9');

// Upper 64 bits: ASCII 64–127
static const uint64_t tchar_hi =
    TCHAR_BIT('A') | TCHAR_BIT('B') | TCHAR_BIT('C') | TCHAR_BIT('D') | TCHAR_BIT('E') | TCHAR_BIT('F') | TCHAR_BIT('G') | TCHAR_BIT('H') |
    TCHAR_BIT('I') | TCHAR_BIT('J') | TCHAR_BIT('K') | TCHAR_BIT('L') | TCHAR_BIT('M') | TCHAR_BIT('N') | TCHAR_BIT('O') | TCHAR_BIT('P') |
    TCHAR_BIT('Q') | TCHAR_BIT('R') | TCHAR_BIT('S') | TCHAR_BIT('T') | TCHAR_BIT('U') | TCHAR_BIT('V') | TCHAR_BIT('W') | TCHAR_BIT('X') |
    TCHAR_BIT('Y') | TCHAR_BIT('Z') | TCHAR_BIT('a') | TCHAR_BIT('b') | TCHAR_BIT('c') | TCHAR_BIT('d') | TCHAR_BIT('e') | TCHAR_BIT('f') |
    TCHAR_BIT('g') | TCHAR_BIT('h') | TCHAR_BIT('i') | TCHAR_BIT('j') | TCHAR_BIT('k') | TCHAR_BIT('l') | TCHAR_BIT('m') | TCHAR_BIT('n') |
    TCHAR_BIT('o') | TCHAR_BIT('p') | TCHAR_BIT('q') | TCHAR_BIT('r') | TCHAR_BIT('s') | TCHAR_BIT('t') | TCHAR_BIT('u') | TCHAR_BIT('v') |
    TCHAR_BIT('w') | TCHAR_BIT('x') | TCHAR_BIT('y') | TCHAR_BIT('z') | TCHAR_BIT('^') | TCHAR_BIT('_') | TCHAR_BIT('`') | TCHAR_BIT('|') |
    TCHAR_BIT('~');

static const uint64_t tchar_table[2] = { tchar_lo, tchar_hi};

static inline int is_tchar(unsigned char c)
{
    /* c >> 6 gives index in tchar_table as it is basically doing c DIV 64. c & 63 is basically just c % 64 to keep the part we are interested in.*/
    return (tchar_table[c >> 6] >> (c & 63)) & 1;
}
/** 
 * @brief Represents a HTTP header, used for http requests.
 */
typedef struct
{
    const char* name;     /**<  Name of header */
    size_t name_len;      /**<  Length of name string */
    const char* value;    /**< Value of header */
    size_t value_len;     /**< Length of value string */
} request_header;

/*
 * @brief represents a HTTP request.
 */
typedef struct
{
    const char* method;                                /**< HTTP method e.g. GET */
    size_t method_len;                                 /**< Length of method string */
    const char* path;                                  /**< Path of the request */
    size_t path_len;                                   /**< Length of the path string */
    const char* version;                               /**<  The HTTP version */
    size_t version_len;                                /**< Length of the version string */
    request_header headers[MAX_REQUEST_HEADERS];       /**< Array of request_headers */
    size_t header_count;                               /**< Number of headers currently in array */
    const char* raw_request;                           /**< Pointer to original raw request */
    size_t request_len;                                /**< Length of the original request */
} http_request;


const char * parse_request
/**
 * @brief Parse a string up to a specified delimiter.
 * @param[in] start Pointer to the start of the string we want to parse.
 * @param[in] end Pointer to the end of the string we want to parse.
 * @param[out] token_start Pointer to Pointer to start of token.
 * @param[out] token_len Pointer to length of the token.
 * @param[in] delimiter The delimiting character.
 * @return Returns a pointer to the position after the delimiter OR a pointer to end if the delimiter was not found.
 * @warning It is left up to the caller to check if start >= end indicating the delimiter was never found and actually generate an error.
 */
const char* parse_token(const char* start, const char* end, const char** token_start, size_t* token_len, const char delimiter,
    int (*is_valid)(unsigned char), int* err_code);

/**
 * @brief Parse a field name ensuring all checks are performed to be Unconditionally Compliant.
 * @param[in] start Pointer to the start of the field name we want to parse.
 * @param[in] end Pointer to the end of the string we want to parse.
 * @param[out] token_start Pointer to Pointer to start of token.
 * @param[out] token_len Pointer to length of the token.
 * @param[in] delimiter The delimiting character.
 * @param[out] err_code Error code. 
 * @return Returns a pointer to the position after the delimiter OR a pointer to end if the delimiter was not found.
 * @warning The caller should check if start >= end if they do not want this behaviour to occur. Which most callers won't
 */
const char* parse_field_name(const char* start, const char* end, const char** token_start, size_t* token_len, const char delimiter, int* err_code);
/**
 * @brief Skip a block of OWS as defined in RFC 9110 Section 5.6.3.
 * Skips a block of Optional White Space (OWS) which is defined in RFC 9110 Section 5.6.3. 
 * Optional White Space is any number of consecutive SP | HTAB characters.
 * I choose not to pass in an err_code, there is only one error that can occur here which is if start >= end so pointless adding another parameter when caller can check this.
 * @param[in] start Pointer to starting position in string.
 * @param[in] end Pointer to end of string.
 * @return Returns a pointer to the next character after the start pointer which is NOT White Space OR a pointer to end (null terminator) if OWS continues till the end - 1.
 * @note Maybe experiment with inlining, however this could be worse as it could have effects to caching? But I am not that familiar with this so will need testing/ research.
 * @warning It is left up to the caller to check if start >= end indicating something has gone wrong
 */
const char* skip_ows(const char* start, const char* end)
{
    while(start < end && IS_OWS(*start))
        ++start;
    return start;
}

/**
 * @brief Parses from a given starting position up to the end of a line 
 * Parses from a given starting position up to the end of a line. The line terminator is specified as a CRLF (\r\n) in RFC 9112 Section 2.2.
 * RFC 9112 Section 2.2 states: 
 * "a recipient MAY recognize a single LF as a line terminator and ignore any preceding CR.".
 * If the client sends a single LF they are stupid so I will not do this.
 * RFC 9112 Section 2.2 also states:
 * "A sender MUST NOT generate a bare CR (a CR character not immediately followed by LF) within any protocol elements other than the content."
 * "A recipient of such a bare CR MUST consider the element to be invalid or replace each bare CR with SP before processing the element or forwarding the message".
 * As the recipient I choose the former, don't send me bad requests. 
 * @param[in] start Pointer to starting position in string.
 * @param[in] end Pointer to end of string.
 * @param[out] line_start Pointer to starting position in the line, not necessarily the start of the actual line.
 * @param[out] line_len Pointer to the length of the line from line_start, doesn't include \r\n.
 * @param[out] err_code Pointer to an error code
 * @return Returns a pointer to the next character after the first \r\n sequence.
 */
const char* parse_line(const char* start, const char* end, const char** line_start, size_t* line_len,  int* err_code)
{
    *line_start = start;
    while(start < end)
    {
        if(*start == '\r')
        {
            if(*(start+1) != '\n')
            {
                *err_code = -2;
                return NULL;
            }
            *line_len = start - *line_start;
            return start + 2;
        }
        ++start;
    }
    *err_code = -1;
    return NULL;
}

// Purpose: Parse the request line
static const char* lenient_parse_req_line(const char* buf, const char* buf_end, int* err_code, http_request* req)
{
    buf = lenient_parse_token(buf, buf_end, &req->method, &req->method_len, ' ');
    buf = lenient_parse_token(buf, buf_end, &req->path, &req->path_len, ' ');
    buf = parse_line(buf, buf_end, &req->version, &req->version_len, err_code);
    return buf;
}

/*
 * RFC 9112 Section 2.2 States that:
 * "In the interest of robustness, a server that is expecting to receive and parse a request-line SHOULD ignore at least one empty line (CRLF) received prior to the request-line"
 */
static inline const char* skip_leading_crlf(const char* cursor, const char* end, int* err_code)
{
    int skipped = 0;
    while (skipped < MAX_LEADING_CRLF && cursor < end)
    {
        if(*cursor == '\r')
        {
            if(cursor+1 >= end)
            {
                *err_code = -1; /* Cursor has reached end unexpectedly */
                return NULL;
            }
            if(*(cursor+1) != '\n')
            {
                *err_code = -2; /* Malformed CRLF (CR missing its LF) */
                return NULL;
            }
            cursor += 2;
            ++skipped;
            continue;
        }
        return cursor;
    }
    *err_code = -5; /* Too many leading CRLF before request line */
    return NULL;
}


/**
 * @brief parses a request line Unconditonally Complaint to RFC specifications.
 *
 *
*/
static const char* strict_parse_req_line(const char* cursor, const char* end, int* err_code, http_request* req)
{
    cursor = skip_leading_crlf(cursor, end, err_code);
    if(UNLIKELY(cursor == NULL)) return NULL;
    cursor = strict_parse_token(cursor, end, &req->method, &req->method_len, ' ', err_code);
    if(UNLIKELY(cursor == NULL)) return NULL;
    cursor = strict_parse_token(cursor, end, &req->path, &req->path_len, ' ', err_code);
    if(UNLIKELY(cursor == NULL)) return NULL;
    cursor = parse_line(cursor, end, &req->version, &req->version_len, err_code);
    return cursor;
}

const char* parse_headers(const char* buf, const char* buf_end, int* err_code, http_request* req)
{
    while(buf < buf_end)
    {
        // Check for blank line implying end of headers
        if(buf + 1 < buf_end && *buf == '\r' && *(buf+1) == '\n') return buf + 2;

        // Check for too many headers
        if(UNLIKELY(req->header_count >= MAX_REQUEST_HEADERS))
        {
            printf("Error: too many headers");
            *err_code = -3;
            return NULL;
        }

        request_header* header = &req->headers[req->header_count];
        buf = parse_token(buf, buf_end, &header->name, &header->name_len, ':');
        // Deal with OWS
        buf = skip_ows(buf, buf_end);
        // Get the value
        header->value = buf;
        // parse_line returns pointer to character after \n
        buf = parse_line(buf, buf_end, &header->value, &header->value_len, err_code);
        /*
         * Check if buf is NULL indicating some error occured in parse_line.
         * We could check if buf >= buf_end after parse_token or skip_ows but this is checked in parse_line so it would be redundant. 
         */
        if(UNLIKELY(buf == NULL))
            return NULL;
        /* Trim white space at the end of header value */
        while(*(header->value + header->value_len - 1) == ' ' || *(header->value + header->value_len - 1) == '\t') header->value_len -=1;
        ++req->header_count;
    }
    /* Currently return an error if missing the blank line at end of headers, could be a bit strict but will leave it for now */
    *err_code = -1;
    return NULL;
}

static void lenient_parse_request(http_request* req, int* err_code)
{
    const char* buf = req->raw_request;
    const char* buf_end = req->raw_request + req->request_len;
    buf = lenient_parse_req_line(buf, buf_end, err_code, req);
    if(UNLIKELY(buf == NULL))
    {
        printf("Error parsing request line, code is: %d", *err_code);
        return;
    }
    buf = parse_headers(buf, buf_end, err_code, req);
    if(UNLIKELY(buf == NULL))
    {
        printf("Error parsing headers, code is: %d", *err_code);
        return;
    } 
}

static void strict_parse_request(http_request* req, int* err_code)
{
    const char* buf = req->raw_request;
    const char* buf_end = req->raw_request + req->request_len;
 
    buf = strict_parse_req_line(buf, buf_end, err_code, req);
    if(UNLIKELY(buf == NULL))
    {
        printf("Error parsing request line, code is: %d", *err_code);
        return;
    }
    buf = parse_headers(buf, buf_end, err_code, req);
    if(UNLIKELY(buf == NULL))
    {
        printf("Error parsing headers, code is: %d", *err_code);
        return;
    } 
}

const char* parse_request_line(const char* cursor, const char* end, http_request* req, const lite_server_config* lt_config, int* err_code);
void parse_request(http_request* req, int* err_code, const lite_server_config* lt_config);

void print_request_info(http_request* req)
{
    size_t i;

    printf("=== HTTP Request ===\n");

    /* Request line */
    printf("Method : %.*s\n",
           (int)req->method_len, req->method);

    printf("Path   : %.*s\n",
           (int)req->path_len, req->path);

    printf("Version: %.*s\n",
           (int)req->version_len, req->version);

    /* Headers */
    printf("\nHeaders (%zu):\n", req->header_count);

    for (i = 0; i < req->header_count; i++) {
        printf("  %.*s: %.*s\n",
               (int)req->headers[i].name_len,
               req->headers[i].name,
               (int)req->headers[i].value_len,
               req->headers[i].value);
    }

    /* Raw buffer info */
    printf("\nRaw buffer length: %zu bytes\n", req->request_len);
}
