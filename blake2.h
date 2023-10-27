/*
 * Header file for a BLAKE2s implementation.
 */

#ifndef BLAKE2_H__
#define BLAKE2_H__

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * BLAKE2s context. Contents are opaque. The context contains no interior
 * pointer and does not reference external resources, so it can be cloned
 * in order to capture the hash state at any given point.
 */
typedef struct {
	uint8_t buf[64];
	uint32_t h[8];
	uint64_t ctr;
	size_t out_len;
} blake2s_context;

/*
 * Initialize BLAKE2s with the specified output length (in bytes). The
 * output length MUST be between 1 and 32. The normal output length (for
 * BLAKE2s-256) is 32.
 */
void blake2s_init(blake2s_context *bc, size_t out_len);

/*
 * Initialize BLAKE2s with the specified output length (in bytes) and a
 * key. The output length MUST be between 1 and 32. The normal output
 * length (for BLAKE2s-256) is 32. The key length must not exceed 32
 * bytes; if `key_len` is zero then `key` can be NULL, and the context
 * is initialized as if it was unkeyed. Keyed BLAKE2s operates as a message
 * authentication code (MAC).
 */
void blake2s_init_key(blake2s_context *bc, size_t out_len,
	const void *key, size_t key_len);

/*
 * Update the provided context by injecting more data. `data` can be NULL
 * if `len` is zero.
 */
void blake2s_update(blake2s_context *bc, const void *data, size_t len);

/*
 * Finalize the BLAKE2s computation and write the output in the provided
 * `dst` buffer (the output length is the one that was set in the last
 * initialization call on this context). The context is consumed in the
 * process; it MUST NOT be reused for injecting more data until it is
 * reinitialized with `blake2s_init()` or `blake2s_init_with_key()`.
 */
void blake2s_final(blake2s_context *bc, void *dst);

/*
 * One-stop function to compute BLAKE2s with the specified output length
 * (`dst_len`, from 1 to 32), key, and input data (`src`, of size
 * `src_len` bytes). The key length (`key_len`) MUST be either 0 (unkeyed
 * hashing), or an integer between 1 and 32.
 */
void blake2s(void *dst, size_t dst_len, const void *key, size_t key_len,
	const void *src, size_t src_len);

#ifdef __cplusplus
}
#endif

#endif
