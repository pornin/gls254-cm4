/*
 * Header file for GLS254 computations. This file documents the external API.
 */

#ifndef GLS254_H__
#define GLS254_H__

#include <stdint.h>

/* ====================================================================== */
/*
 * LOW-LEVEL API
 *
 * This low-level API is intended for "advanced" usage (building arbitrary
 * cryptographic protocols that use the GLS254 group as a primitive).
 * For "normal" usage (e.g. key exchange or signatures), use the high-level
 * API (defined later in this file).
 */

/*
 * The `gfb127` and `gfb254` types are internal types specified here only
 * because they are used in the definition of the GLS254 point types.
 * No assumption should be made about their contents.
 */

typedef struct {
	uint32_t v[4];
} gfb127;

typedef struct {
	gfb127 v[2];
} gfb254;

/*
 * A GLS254 point. Contents are opaque.
 */
typedef struct {
	gfb254 X, S, Z, T;
} gls254_point;

/*
 * An affine (scaled) GLS254 point. Contents are opaque.
 */
typedef struct {
	gfb254 scaled_x, scaled_s;
} gls254_point_affine;

/*
 * Statically allocated read-only GLS254 points representing the group
 * neutral element and the conventional generator, respectively.
 */
extern const gls254_point GLS254_NEUTRAL;
extern const gls254_point GLS254_BASE;

/*
 * Test whether a point is the neutral element; returned value is 0xFFFFFFFF
 * for the neutral, and 0x00000000 for all other points.
 */
uint32_t gls254_isneutral(const gls254_point *p);

/*
 * Test whether `*p1` and `*p2` represent the same group element. Returned
 * value is 0xFFFFFFFF on equality, 0x00000000 otherwise.
 */
uint32_t gls254_equals(const gls254_point *p1, const gls254_point *p2);

/*
 * Set `*p2` to a copy of `*p1` if `ctl` is 0xFFFFFFFF; if `ctl` is 0x00000000
 * then `*p2` is unmodified. `ctl` MUST be either 0xFFFFFFFF or 0x00000000.
 */
void gls254_set_cond(gls254_point *p2, const gls254_point *p1, uint32_t ctl);

/*
 * Decode the 32 bytes pointed at by `src` into the point `*p`. If the
 * decoding succeeds, then 0xFFFFFFFF is returned; otherwise, `*p` is
 * set to a representation of the neutral element, and 0x00000000 is
 * returned. Encoding rules are canonical; for a given group element, a
 * single 32-byte representation is accepted.
 */
uint32_t gls254_decode(gls254_point *p, const void *src);

/*
 * Encode the point `*p` into exactly 32 bytes at `*dst`. Encoding is
 * canonical: two representations of the same group element encode into
 * the exact same sequence of bytes.
 */
void gls254_encode(void *dst, const gls254_point *p);

/*
 * Add points `*p1` and `*p2`, with result in `*p3`. The operands need
 * not be distinct structures.
 */
void gls254_add(gls254_point *p3,
	const gls254_point *p1, const gls254_point *p2);

/*
 * Subtract point `*p2` from point `*p1`, result in `*p3`. The operands need
 * not be distinct structures.
 */
void gls254_sub(gls254_point *p3,
	const gls254_point *p1, const gls254_point *p2);

/*
 * Negate point `*p1`, result in `*p3`. The operands need not be distinct
 * structures.
 */
void gls254_neg(gls254_point *p3, const gls254_point *p1);

/*
 * Set `*p3` to either `*p1` (if `ctl` is 0x00000000) or to the negation
 * or `*p1` (if `ctl` is 0xFFFFFFFF). `ctl` MUST be equal to 0xFFFFFFFF
 * or 0x00000000. The operands need not be distinct structures.
 */
void gls254_condneg(gls254_point *p3, const gls254_point *p1, uint32_t ctl);

/*
 * Double point `*p1` `n` times, successively, with result in `*p3`. If
 * `n` is zero, then this is a simple copy. The operands need not be
 * distinct structures. The number of point doublings (`n`) may leak
 * through side channels, since the execution time of this function is
 * proportional to the value of `n`.
 */
void gls254_xdouble(gls254_point *p3, const gls254_point *p1, unsigned n);

/*
 * Double point `*p1`, result in `*p3`. The operands need not be distinct
 * structures.
 *
 * If more than one doubling must be performed on a point, it is more
 * efficient to call `gls254_xdouble()` with the number of doublings
 * as last parameter, than to call this function repeatedly.
 */
static inline void
gls254_double(gls254_point *p3, const gls254_point *p1)
{
	gls254_xdouble(p3, p1, 1);
}

/*
 * Fill `win[0]` to `win[7]` with points 1*P to 8*P, respectively. The
 * points are normalized to affine (scaled) coordinates.
 */
void gls254_make_window_affine_8(
	gls254_point_affine *win, const gls254_point *p);

/*
 * For an integer `k` between -8 and +8 (inclusive), set `*p` to `k*P`,
 * for the point `P` which was used to fill the provided window. This
 * is constant-time. If `k` is zero, then `*p` is set to the neutral.
 */
void gls254_lookup8_affine(gls254_point_affine *p,
	const gls254_point_affine *win, int8_t k);

/*
 * Apply the endomorphism zeta on the provided affine point `*p1`,
 * result in `*p2`. Flag `zn` MUST be 0x00000000 or 0xFFFFFFFF; if
 * `zn` is 0xFFFFFFFF, then the point is also negated (i.e. `*p2` is
 * set to -zeta(P1) instead of zeta(P1)).
 */
void gls254_zeta_affine(gls254_point_affine *p2,
	const gls254_point_affine *p1, uint32_t zn);

/*
 * Add affine point `*p2` to non-affine point `*p1`, result in `*p3`.
 */
void gls254_add_affine(gls254_point *p3,
	const gls254_point *p1, const gls254_point_affine *p2);

/*
 * Add affine points `*p1` and `*p2`, result in `*p3`.
 */
void gls254_add_affine_affine(gls254_point *p3,
	const gls254_point_affine *p1, const gls254_point_affine *p2);

/*
 * Multiply point `*p` by the scalar `k`, result in `*q`. The scalar
 * is a 32-byte sequence that encodes an integer in unsigned little-endian
 * convention. It is not required that the scalar is reduced modulo the
 * group order.
 */
void gls254_mul(gls254_point *q, const gls254_point *p, const void *k);

/*
 * Same as `gls254_mul(q, &GLS254_BASE, k)`. This uses internal precomputed
 * tables for the conventional generator, and is faster than `gls254_mul()`
 * in that case.
 */
void gls254_mulgen(gls254_point *q, const void *k);

/*
 * Normalize a point `*p` to affine coordinates.
 */
void gls254_normalize(gls254_point_affine *q, const gls254_point *p);

/*
 * Convert a point from affine to extended coordinates.
 */
void gls254_from_affine(gls254_point *q, const gls254_point_affine *p);

/*
 * Hash some input data into a point. The process should produce a
 * uniformly selected group element (or, at least, the bias from uniform
 * selection is negligible). The input string is tagged with a
 * "hash name": if the string is itself a hash value, then `hash_name`
 * should qualify the hash function that was used to process it; for raw
 * data, set `hash_name` to NULL or an empty string. See `gls254_sign()`
 * for details.
 *
 * Note that the resulting point can conceptually be the neutral element
 * (though actually finding an input that yields the neutral is considered
 * computationally infeasible in practice).
 */
void gls254_hash_to_point(gls254_point *p, const char *hash_name,
	const void *data, size_t data_len);

/*
 * Scalars are integers, considered modulo the group order r:
 *   r = 2^253 + 83877821160623817322862211711964450037
 * In this API, scalars are represented as sequences of 32 bytes, using
 * unsigned little-endian convention.
 *
 * All functions below output only properly reduced scalars, but accept
 * non-reduced inputs (i.e. values up to 2^256 - 1). They are all
 * constant-time. The output buffer may overlap with any of the input
 * operands.
 */

/*
 * Reduce the provided integer (`*a`, of size `a_len` bytes, with unsigned
 * little-endian convention) into a scalar `*d`.
 */
void scalar_reduce(void *d, const void *a, size_t a_len);

/*
 * Test whether a scalar is reduced: returned value is 1 if the scalar `*a`
 * (32 bytes) encodes an integer in the 0 to r-1 range; otherwise, 0 is
 * returned.
 */
int scalar_is_reduced(const void *a);

/*
 * Test whether a scalar is equal to zero. This returns 1 if all 32 bytes are
 * equal to zero, or 0 if any of the bytes is non-zero.
 * IMPORTANT: this function does _not_ perform any reduction; it assumes
 * that the input scalar is already reduced. Use `scalar_reduce()` first
 * if this property cannot be ensured from the usage context.
 */
int scalar_is_zero(const void *a);

/*
 * Scalar addition: d <- a + b mod r
 */
void scalar_add(void *d, const void *a, const void *b);

/*
 * Scalar subtraction: d <- a - b mod r
 */
void scalar_sub(void *d, const void *a, const void *b);

/*
 * Scalar negation: d <- -a mod r
 */
void scalar_neg(void *d, const void *a);

/*
 * Scalar multiplication: d <- a*b mod r
 */
void scalar_mul(void *d, const void *a, const void *b);

/*
 * Scalar halving: d <- a/2 mod r
 */
void scalar_half(void *d, const void *a);

/*
 * Scalar split: given value mu =
 * 10811011514837737534717025162521437705238749575629098770843741413709931738644
 * (mu is a square root of -1 modulo r), split scalar `k` (32 bytes) into
 * integers k0 and k1 such that:
 *   |k0| < sqrt(r) < 2^127
 *   |k1| < sqrt(r) < 2^127
 *   k = k0 + mu*k1 mod r
 * `*ak0` (16 bytes, little-endian) receives the absolute value of k0.
 * `*sk0` is set to 0xFFFFFFFF is k0 < 0, or to 0x00000000 otherwise.
 * `*ak1` (16 bytes, little-endian) receives the absolute value of k1.
 * `*sk1` is set to 0xFFFFFFFF is k1 < 0, or to 0x00000000 otherwise.
 *
 * The process always works. sqrt(r) is slightly above 2^126.5, but lower
 * than 2^127, hence `*ak0` and `*ak1` always fit on 127 bits.
 */
void scalar_split(uint8_t *ak0, uint32_t *sk0, uint8_t *ak1, uint32_t *sk1,
        const void *k);

/* ====================================================================== */
/*
 * HIGH-LEVEL API
 *
 * This API offers the usual functionalities of key exchange and signing.
 * Private keys are secret scalars and encode over 32 bytes.
 * Public keys are points and encode over 32 bytes ("compressed" format).
 * Signatures have size 48 bytes and offer 128-bit security.
 *
 * Keys are used by first decoding them into in-memory structures, which
 * can be used repeatedly for multiple operations.
 */

/*
 * GLS254 public key. The contents are opaque.
 */
typedef struct {
	gls254_point pp;
	uint8_t enc[32];
} gls254_public_key;

/*
 * GLS254 private key. The contents are opaque.
 */
typedef struct {
	uint8_t sec[32];
	gls254_public_key pub;
} gls254_private_key;

/*
 * Private key generation.
 * ####!!! WARNING / IMPORTANT !!!####
 * This implementation does not have access to a random generator. It is
 * up to the caller to provide a source `rnd` (of size `rnd_len`) with
 * enough entropy (at least 128 bits, preferably 256 bits or more). If the
 * source entropy is not enough then the resulting key is weak. This
 * function derives the actual private key from the provided random bytes
 * with a deterministic process; that process is not specified and callers
 * MUST NOT assume that it will always be the same.
 */
void gls254_keygen(gls254_private_key *sk, const void *rnd, size_t rnd_len);

/*
 * Get the public key from a private key.
 */
static inline void
gls254_get_public(gls254_public_key *pk, const gls254_private_key *sk)
{
	*pk = sk->pub;
}

/*
 * Decode a private key from its encoded form (`*src`, 32 bytes).
 * Returned values is 1 on success, 0 on error; an error is reported
 * if the encoding is invalid. On error, the provided `*sk` structure
 * is filled with an "invalid key" value (zero); however, none of
 * the functions using private keys will test against such a value.
 */
int gls254_decode_private(gls254_private_key *sk, const void *src);

/*
 * Encode a private key into exactly 32 bytes.
 */
void gls254_encode_private(void *dst, const gls254_private_key *sk);

/*
 * Decode a public key from its encoded form (`*src`, 32 bytes).
 * Returned values is 1 on success, 0 on error; an error is reported
 * if the encoding is invalid.
 *
 * On error, the destination structure (`*pk`) is filled with a special
 * "invalid key" value.
 */
int gls254_decode_public(gls254_public_key *pk, const void *src);

/*
 * Encode a public key into exactly 32 bytes.
 */
void gls254_encode_public(void *dst, const gls254_public_key *pk);

/*
 * Sign some data with a private key `*sk`.
 *
 * The data is provided as a pointer (`data`) and has length exactly
 * `data_len` bytes. The data may be either "raw data" (unhashed),
 * or a hash value. If raw data is used, then `hash_name` should be
 * set to either NULL or an empty string; otherwise, it should contain
 * the symbolic name of the hash function that was used to pre-process
 * the actual data and obtain the hash value. Symbolic names for the
 * usual hash functions are provided below (`GLS254_HASHNAME_SHA256`,
 * etc).
 *
 * The signature has size exactly 48 bytes and is written into the
 * buffer pointed to by `sig`.
 *
 * `seed` may point to an extra "seed" of size `seed_len` bytes. The
 * seed is any non-secret varying data that can be used to make the
 * signature non-deterministic; the seed may be a clock value, some
 * random bytes, or about anything else. If no seed is provided
 * (`seed_len` is zero), then the signature is still cryptographically
 * safe, but deterministic (signing the same data with the same key
 * yields the exact same signature). Non-deterministic signatures are
 * considered to provide extra safety against some physical attackers
 * (especially for fault attacks).
 */
void gls254_sign(void *sig, const gls254_private_key *sk,
	const void *seed, size_t seed_len, const char *hash_name,
	const void *data, size_t data_len);

/*
 * Verify the signature (`sig`, 48 bytes) against the public key `*pk`,
 * for the provided data. The `hash_name`, `data` and `data_len`
 * parameters have the same semantics as in `gls254_sign()`; the
 * signature may verify successfully only if these parameters all have
 * the same values as they had when generating the signature.
 *
 * Returned value is 1 on success, 0 on error.
 *
 * THIS FUNCTION IS NOT CONSTANT-TIME. Public keys and signatures are
 * normally considered public values.
 */
int gls254_verify_vartime(const gls254_public_key *pk, const void *sig,
        const char *hash_name, const void *data, size_t data_len);

/*
 * Symbolic names for classic hash functions. In general, the symbolic
 * name is obtained by removing all punctuation signs from the function
 * name, and converting it to lowercase.
 */
#define GLS254_HASHNAME_SHA224        "sha224"
#define GLS254_HASHNAME_SHA256        "sha256"
#define GLS254_HASHNAME_SHA384        "sha384"
#define GLS254_HASHNAME_SHA512        "sha512"
#define GLS254_HASHNAME_SHA512_224    "sha512224"
#define GLS254_HASHNAME_SHA512_256    "sha512256"
#define GLS254_HASHNAME_SHA3_224      "sha3224"
#define GLS254_HASHNAME_SHA3_256      "sha3256"
#define GLS254_HASHNAME_SHA3_384      "sha3384"
#define GLS254_HASHNAME_SHA3_512      "sha3512"
#define GLS254_HASHNAME_BLAKE2B       "blake2b"
#define GLS254_HASHNAME_BLAKE2S       "blake2s"
#define GLS254_HASHNAME_BLAKE3        "blake3"

/*
 * Key exchange: combine a local private key with the provided peer
 * public key. The resulting key has length 32 bytes and is written into
 * the destination array `shared_key`. The obtained key is the output of
 * a key derivation step, and has no discernable structure; if a shorter
 * key is required (e.g. 128 bits) then it can simply be truncated to
 * the right size.
 *
 * Returned value is 1 on success, 0 on failure. A failure is reported if
 * the peer public key is in the "invalid key" state. In that case, a
 * key is still generated; that key is unguessable by outsiders.
 */
int gls254_ECDH(void *shared_key, const gls254_private_key *sk,
	const gls254_public_key *pk_peer);

/*
 * FOR BENCHMARKS ONLY. This is a "raw ECDH" implementation that expects
 * and outputs points in uncompressed affine format, over 64 bytes (and
 * not 32, as in the usual encode/decode functions). Moreover, the
 * input point (`src`) is assumed to be public: if the input point (`src`)
 * is invalid (non-canonical encoding of the coordinates, or coordinates
 * not matching the curve equation), then the function returns early with
 * a status of 0. Otherwise, the function multiplies the decoded point
 * by the provided `scalar` (32 bytes), encodes the result into the
 * buffer pointed to by `dst` (64 bytes), and returns 1.
 *
 * This function is used mostly for speed benchmarks, following those
 * presented in:
 *    https://eprint.iacr.org/2022/748
 */
int gls254_raw_ECDH(void *dst, const void *src, const void *scalar);

/* ====================================================================== */

#endif
