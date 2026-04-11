#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <stdint.h>

/* Fill buf with current UTC time as ISO-8601 string (needs len >= 32) */
void util_iso8601_now(char *buf, size_t len);

/* Lower-case s in-place */
void util_strlower(char *s);

/* DJB2 hash over len bytes */
uint32_t util_hash_djb2(const uint8_t *data, size_t len);

/* Hex-encode src[0..len) into dst; dst must be >= 2*len+1 bytes */
void util_hexencode(const uint8_t *src, size_t len, char *dst);

/*
 * JSON-escape src and write as a quoted JSON string into dst.
 * Returns dst.  dst_size should be >= 2 + 6*strlen(src) + 1.
 */
char *util_json_escape(const char *src, char *dst, size_t dst_size);

/* Safe strncpy: always NUL-terminates dst */
void util_strlcpy(char *dst, const char *src, size_t size);

#endif /* UTIL_H */
