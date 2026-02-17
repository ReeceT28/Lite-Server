#pragma once
#include <stdint.h>

#define C_ALPHA    0x01
#define C_DIGIT    0x02
#define C_VCHAR    0x04
#define C_OBS_TEXT 0x08 

// Possible performance considerations: could remove check for OBS_TEXT and half size of data, could be faster to check boundaries???

static const uint8_t char_class[256] = 
{
    /* DIGITS */
    ['0' ... '9'] = C_DIGIT | C_VCHAR,

    /* ALPHA */
    ['a' ... 'z'] = C_ALPHA | C_VCHAR,
    ['A' ... 'Z'] = C_ALPHA | C_VCHAR,

    /* V CHAR */
    [0x21 ... 0x2F] = C_VCHAR, /* Go from Lower Bound 0x21 to character before '0' */
    [0x3A ... 0x40] = C_VCHAR, /* From character after '9' to character before 'A' */
    [0x5B ... 0x60] = C_VCHAR, /* From character after 'Z' to character before 'a' */
    [0x7B ... 0x7E] = C_VCHAR, /* From character after 'z' to Upper Bound 0x7E     */

    /* OBS_TEXT */
    [0x80 ... 0xFF] = C_OBS_TEXT
}; 

static inline int is_alpha(unsigned char c)
{
    return char_class[c] & C_ALPHA;
}

static inline int is_vchar(unsigned char c)
{
    return char_class[c] & C_VCHAR;
}

static inline int is_obs_text(unsigned char c)
{
    return char_class[c] & C_OBS_TEXT;
}
