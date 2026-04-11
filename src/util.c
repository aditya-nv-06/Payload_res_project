#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

void util_iso8601_now(char *buf, size_t len)
{
    time_t t = time(NULL);
    struct tm tm_info;
    gmtime_r(&t, &tm_info);
    strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", &tm_info);
}

void util_strlower(char *s)
{
    for (; *s; s++)
        *s = (char)tolower((unsigned char)*s);
}

uint32_t util_hash_djb2(const uint8_t *data, size_t len)
{
    uint32_t hash = 5381;
    for (size_t i = 0; i < len; i++)
        hash = ((hash << 5) + hash) ^ data[i];
    return hash;
}

void util_hexencode(const uint8_t *src, size_t len, char *dst)
{
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        dst[2 * i]     = hex[(src[i] >> 4) & 0xf];
        dst[2 * i + 1] = hex[src[i] & 0xf];
    }
    dst[2 * len] = '\0';
}

char *util_json_escape(const char *src, char *dst, size_t dst_size)
{
    size_t j = 0;
    if (dst_size < 3) { dst[0] = '\0'; return dst; }
    dst[j++] = '"';
    for (size_t i = 0; src[i] && j + 8 < dst_size; i++) {
        unsigned char c = (unsigned char)src[i];
        if      (c == '"')  { dst[j++] = '\\'; dst[j++] = '"'; }
        else if (c == '\\') { dst[j++] = '\\'; dst[j++] = '\\'; }
        else if (c == '\n') { dst[j++] = '\\'; dst[j++] = 'n'; }
        else if (c == '\r') { dst[j++] = '\\'; dst[j++] = 'r'; }
        else if (c == '\t') { dst[j++] = '\\'; dst[j++] = 't'; }
        else if (c < 0x20)  { j += (size_t)snprintf(dst + j, 7, "\\u%04x", c); }
        else                { dst[j++] = (char)c; }
    }
    dst[j++] = '"';
    if (j < dst_size) dst[j] = '\0';
    return dst;
}

void util_strlcpy(char *dst, const char *src, size_t size)
{
    if (size == 0) return;
    size_t i;
    for (i = 0; i + 1 < size && src[i]; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}
