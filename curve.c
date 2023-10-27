/*
 * High-level operations on GLS254.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "inner.h"
#include "blake2.h"

static inline void
enc32le(void *dst, uint32_t x)
{
	uint8_t *buf = dst;
	buf[0] = (uint8_t)x;
	buf[1] = (uint8_t)(x >> 8);
	buf[2] = (uint8_t)(x >> 16);
	buf[3] = (uint8_t)(x >> 24);
}

/*
 * Booth recoding.
 * Input: n[], unsigned little-endian, length = 8 bytes (64 bits)
 * Output: sd[], 16 signed digits ([-8..+8]), low to high order
 * Returned: carry (0 or 1)
 * If carry is one, then the signed digits encode n - 2^64 instead of n.
 */
static uint32_t
recode4_u64(int8_t *sd, const uint8_t *n)
{
	uint8_t *dd = (uint8_t *)sd;
	uint32_t cc = 0;
	for (int i = 0; i < 8; i ++) {
		uint32_t x, d, m;

		x = (uint32_t)n[i];
		d = (x & 0x0F) + cc;
		m = (8 - d) >> 8;
		dd[(i << 1) + 0] = (uint8_t)(d - (m & 16));
		cc = m & 1;

		d = (x >> 4) + cc;
		m = (8 - d) >> 8;
		dd[(i << 1) + 1] = (uint8_t)(d - (m & 16));
		cc = m & 1;
	}
	return cc;
}

/*
 * Booth recoding.
 * Input: n[], unsigned little-endian, length = 16 bytes (127 bits)
 * !!!IMPORTANT: input value must be lower than 2^127
 * Output: sd[], 32 signed digits ([-8..+8]), low to high order
 */
static void
recode4_u128(int8_t *sd, const uint8_t *n)
{
	uint8_t *dd = (uint8_t *)sd;
	uint32_t cc = 0;
	for (int i = 0; i < 16; i ++) {
		uint32_t x, d, m;

		x = (uint32_t)n[i];
		d = (x & 0x0F) + cc;
		m = (8 - d) >> 8;
		dd[(i << 1) + 0] = (uint8_t)(d - (m & 16));
		cc = m & 1;

		d = (x >> 4) + cc;
		m = (8 - d) >> 8;
		dd[(i << 1) + 1] = (uint8_t)(d - (m & 16));
		cc = m & 1;
	}
}

/* see gls254.h */
void
gls254_mul(gls254_point *q, const gls254_point *p, const void *k)
{
	gls254_point_affine win[8], pa, qa;
	uint8_t n0[16], n1[16];
	int8_t sd0[32], sd1[32];
	uint32_t s0, s1, zn;

	scalar_split(n0, &s0, n1, &s1, k);

	/* We make the window over P or -P, to match the sign of n0. */
	gls254_condneg(q, p, s0);
	gls254_make_window_affine_8(win, q);

	/* If zn != 0 then n1 has not the same sign as n0, and we must
	   apply the -zeta() endomorphism. */
	zn = s0 ^ s1;

	recode4_u128(sd0, n0);
	recode4_u128(sd1, n1);

	gls254_lookup8_affine(&pa, win, sd0[31]);
	gls254_lookup8_affine(&qa, win, sd1[31]);
	gls254_zeta_affine(&qa, &qa, zn);
	gls254_add_affine_affine(q, &pa, &qa);
	for (int i = 30; i >= 0; i --) {
		gls254_point t;

		gls254_xdouble(q, q, 4);
		gls254_lookup8_affine(&pa, win, sd0[i]);
		gls254_lookup8_affine(&qa, win, sd1[i]);
		gls254_zeta_affine(&qa, &qa, zn);
		gls254_add_affine_affine(&t, &pa, &qa);
		gls254_add(q, q, &t);
	}
}

/* Forward declaration of precomputed tables of multiples of the base
   point (located at the end of this file). */
static const gls254_point_affine PRECOMP_B[];
static const gls254_point_affine PRECOMP_B32[];
static const gls254_point_affine PRECOMP_B64[];
static const gls254_point_affine PRECOMP_B96[];

/*
 * Convenience wrapper for lookup + zeta.
 */
static inline void
gls254_lookup8_affine_zeta(gls254_point_affine *p,
	const gls254_point_affine *win, int8_t k, uint32_t zn)
{
	gls254_lookup8_affine(p, win, k);
	gls254_zeta_affine(p, p, zn);
}

/* see gls254.h */
void
gls254_mulgen(gls254_point *q, const void *k)
{
	gls254_point t;
	gls254_point_affine pa, qa;
	uint8_t n0[16], n1[16];
	int8_t sd0[32], sd1[32];
	uint32_t s0, s1, zn;

	scalar_split(n0, &s0, n1, &s1, k);
	zn = s0 ^ s1;
	recode4_u128(sd0, n0);
	recode4_u128(sd1, n1);

	gls254_lookup8_affine(&pa, PRECOMP_B, sd0[7]);
	gls254_lookup8_affine(&qa, PRECOMP_B32, sd0[15]);
	gls254_add_affine_affine(q, &pa, &qa);
	gls254_lookup8_affine(&pa, PRECOMP_B64, sd0[23]);
	gls254_lookup8_affine(&qa, PRECOMP_B96, sd0[31]);
	gls254_add_affine_affine(&t, &pa, &qa);
	gls254_add(q, q, &t);

	gls254_lookup8_affine_zeta(&pa, PRECOMP_B, sd1[7], zn);
	gls254_lookup8_affine_zeta(&qa, PRECOMP_B32, sd1[15], zn);
	gls254_add_affine_affine(&t, &pa, &qa);
	gls254_add(q, q, &t);
	gls254_lookup8_affine_zeta(&pa, PRECOMP_B64, sd1[23], zn);
	gls254_lookup8_affine_zeta(&qa, PRECOMP_B96, sd1[31], zn);
	gls254_add_affine_affine(&t, &pa, &qa);
	gls254_add(q, q, &t);

	for (int i = 6; i >= 0; i --) {
		gls254_xdouble(q, q, 4);

		gls254_lookup8_affine(&pa, PRECOMP_B, sd0[i]);
		gls254_lookup8_affine(&qa, PRECOMP_B32, sd0[i + 8]);
		gls254_add_affine_affine(&t, &pa, &qa);
		gls254_add(q, q, &t);
		gls254_lookup8_affine(&pa, PRECOMP_B64, sd0[i + 16]);
		gls254_lookup8_affine(&qa, PRECOMP_B96, sd0[i + 24]);
		gls254_add_affine_affine(&t, &pa, &qa);
		gls254_add(q, q, &t);

		gls254_lookup8_affine_zeta(&pa, PRECOMP_B, sd1[i], zn);
		gls254_lookup8_affine_zeta(&qa, PRECOMP_B32, sd1[i + 8], zn);
		gls254_add_affine_affine(&t, &pa, &qa);
		gls254_add(q, q, &t);
		gls254_lookup8_affine_zeta(&pa, PRECOMP_B64, sd1[i + 16], zn);
		gls254_lookup8_affine_zeta(&qa, PRECOMP_B96, sd1[i + 24], zn);
		gls254_add_affine_affine(&t, &pa, &qa);
		gls254_add(q, q, &t);
	}

	gls254_condneg(q, q, s0);
}

/* see gls254.h */
void
gls254_keygen(gls254_private_key *sk, const void *rnd, size_t rnd_len)
{
	/* Make secret scalar by hashing the seed into 32 bytes. */
	blake2s_context bc;
	blake2s_init(&bc, 32);
	blake2s_update(&bc, "GLS254 keygen:", 14);
	blake2s_update(&bc, rnd, rnd_len);
	blake2s_final(&bc, sk->sec);

	/* Ensure that the scalar is properly reduced. */ 
	scalar_reduce(sk->sec, sk->sec, 32);

	/* Compute the public key. */
	gls254_mulgen(&sk->pub.pp, sk->sec);
	gls254_encode(sk->pub.enc, &sk->pub.pp);
}

/* see gls254.h */
int
gls254_decode_private(gls254_private_key *sk, const void *src)
{
	if (!scalar_is_reduced(src) || scalar_is_zero(src)) {
		memset(sk->sec, 0, sizeof sk->sec);
		sk->pub.pp = GLS254_NEUTRAL;
		memset(sk->pub.enc, 0, sizeof sk->pub.enc);
		return 0;
	}
	memcpy(sk->sec, src, 32);
	gls254_mulgen(&sk->pub.pp, sk->sec);
	gls254_encode(sk->pub.enc, &sk->pub.pp);
	return 1;
}

/* see gls254.h */
void
gls254_encode_private(void *dst, const gls254_private_key *sk)
{
	memcpy(dst, sk->sec, 32);
}

/* see gls254.h */
int
gls254_decode_public(gls254_public_key *pk, const void *src)
{
	// When decoding the public key, we preserve the original encoding,
	// even if invalid: this is used in gls254_ECDH().
	memcpy(pk->enc, src, 32);
	if (!gls254_decode(&pk->pp, src) || gls254_isneutral(&pk->pp) != 0) {
		pk->pp = GLS254_NEUTRAL;
		return 0;
	}
	return 1;
}

/* see gls254.h */
void
gls254_encode_public(void *dst, const gls254_public_key *pk)
{
	memcpy(dst, pk->enc, 32);
}

/* see gls254.h */
void
gls254_hash_to_point(gls254_point *p, const char *hash_name,
	const void *data, size_t data_len)
{
	blake2s_context bc;
	uint8_t blob1[32], blob2[32];

	if (hash_name == NULL || *hash_name == 0) {
		blake2s_init(&bc, 32);
		blob1[0] = 0x01;
		blob1[1] = 0x52;
		blake2s_update(&bc, blob1, 2);
		blake2s_update(&bc, data, data_len);
		blake2s_final(&bc, blob1);
		blake2s_init(&bc, 32);
		blob2[0] = 0x02;
		blob2[1] = 0x52;
		blake2s_update(&bc, blob2, 2);
		blake2s_update(&bc, data, data_len);
		blake2s_final(&bc, blob2);
	} else {
		blake2s_init(&bc, 32);
		blob1[0] = 0x01;
		blob1[1] = 0x48;
		blake2s_update(&bc, blob1, 2);
		blake2s_update(&bc, hash_name, strlen(hash_name) + 1);
		blake2s_update(&bc, data, data_len);
		blake2s_final(&bc, blob1);
		blake2s_init(&bc, 32);
		blob2[0] = 0x02;
		blob2[1] = 0x48;
		blake2s_update(&bc, blob2, 2);
		blake2s_update(&bc, hash_name, strlen(hash_name) + 1);
		blake2s_update(&bc, data, data_len);
		blake2s_final(&bc, blob2);
	}

	gls254_point q;
	gls254_map_to_point(p, blob1);
	gls254_map_to_point(&q, blob2);
	gls254_add(p, p, &q);
}

/*
 * Compute the "challenge" (16 bytes) in Schnorr signatures.
 */
static void
make_challenge(void *dst, const gls254_point *R, const void *pub,
	const char *hash_name, const void *data, size_t data_len)
{
	blake2s_context bc;
	uint8_t tmp[32];

	blake2s_init(&bc, 32);
	gls254_encode(tmp, R);
	blake2s_update(&bc, tmp, 32);
	blake2s_update(&bc, pub, 32);
	if (hash_name == NULL || *hash_name == 0) {
		tmp[0] = 0x52;
		blake2s_update(&bc, tmp, 1);
	} else {
		tmp[0] = 0x48;
		blake2s_update(&bc, tmp, 1);
		blake2s_update(&bc, hash_name, strlen(hash_name) + 1);
	}
	blake2s_update(&bc, data, data_len);
	blake2s_final(&bc, tmp);
	memcpy(dst, tmp, 16);
}

static const uint8_t MU[] = {
	0x14, 0xF6, 0xA1, 0x89, 0xFC, 0x87, 0x84, 0x1B,
	0xFC, 0x63, 0xE1, 0xFA, 0xF1, 0xAD, 0xEF, 0x1E,
	0x99, 0xE4, 0x3F, 0x36, 0xDA, 0xBD, 0x58, 0x9F,
	0x93, 0xBC, 0x54, 0x0F, 0xD0, 0xD0, 0xE6, 0x17
};

/* see gls254.h */
void
gls254_sign(void *sig, const gls254_private_key *sk,
	const void *seed, size_t seed_len, const char *hash_name,
	const void *data, size_t data_len)
{
	/* Per-signature secret scalar. */
	blake2s_context bc;
	uint8_t tmp[8], k[32];
	blake2s_init(&bc, 32);
	blake2s_update(&bc, sk->sec, sizeof sk->sec);
	blake2s_update(&bc, sk->pub.enc, sizeof sk->pub.enc);
	enc32le(tmp, (uint32_t)seed_len);
	enc32le(tmp + 4, (uint32_t)((uint64_t)seed_len >> 32));
	blake2s_update(&bc, tmp, 8);
	blake2s_update(&bc, seed, seed_len);
	if (hash_name == NULL || *hash_name == 0) {
		tmp[0] = 0x52;
		blake2s_update(&bc, tmp, 1);
	} else {
		tmp[0] = 0x48;
		blake2s_update(&bc, tmp, 1);
		blake2s_update(&bc, hash_name, strlen(hash_name) + 1);
	}
	blake2s_update(&bc, data, data_len);
	blake2s_final(&bc, k);

	/* Use k to generate the signature. */
	gls254_point R;
	uint8_t cb[16], c[32], d[32];
	gls254_mulgen(&R, k);
	make_challenge(cb, &R, sk->pub.enc, hash_name, data, data_len);
	scalar_reduce(c, cb, 8);
	scalar_reduce(d, cb + 8, 8);
	scalar_mul(d, d, MU);
	scalar_add(c, c, d);
	scalar_mul(c, c, sk->sec);
	memcpy(sig, cb, 16);
	scalar_add((uint8_t *)sig + 16, c, k);
}

/*
 * Convenience wrapper for lookup + condneg.
 */
static inline void
gls254_lookup8_affine_sign(gls254_point_affine *p,
	const gls254_point_affine *win, int8_t k, uint32_t sk)
{
	uint32_t uk = (uint32_t)k;
	uk -= sk & (uk << 1);
	gls254_lookup8_affine(p, win, (int8_t)*(int32_t *)&uk);
}

/* see gls254.h */
int
gls254_verify_vartime(const gls254_public_key *pk, const void *sig,
	const char *hash_name, const void *data, size_t data_len)
{
	// Reject cases with invalid public keys (the decode function
	// maps invalid keys to the neutral, which is not valid as a key).
	if (gls254_isneutral(&pk->pp) != 0) {
		return 0;
	}
	const uint8_t *sigbuf = sig;
	if (!scalar_is_reduced(sigbuf + 16)) {
		return 0;
	}

	gls254_point R, P;
	uint8_t v0[16], v1[16];
	int8_t sd0[16], sd1[16], sd2[32], sd3[32];
	uint32_t t0, t1;
	gls254_point_affine win[8], pa, qa;

	scalar_split(v0, &t0, v1, &t1, sigbuf + 16);
	gls254_neg(&P, &pk->pp);
	gls254_make_window_affine_8(win, &P);

	uint32_t cc0 = recode4_u64(sd0, sigbuf);
	uint32_t cc1 = recode4_u64(sd1, sigbuf + 8);
	recode4_u128(sd2, v0);
	recode4_u128(sd3, v1);

	if (cc0 && cc1) {
		gls254_zeta_affine(&pa, &win[0], 0);
		gls254_add_affine_affine(&R, &win[0], &pa);
	} else if (cc0) {
		R = P;
	} else if (cc1) {
		gls254_zeta_affine(&pa, &win[0], 0);
		gls254_from_affine(&R, &pa);
	} else {
		R = GLS254_NEUTRAL;
	}

	for (int i = 15; i >= 0; i --) {
		gls254_xdouble(&R, &R, 4);

		int8_t k0 = sd0[i], k1 = sd1[i];
		if (k0 != 0 && k1 != 0) {
			gls254_lookup8_affine(&pa, win, k0);
			gls254_lookup8_affine_zeta(&qa, win, k1, 0);
			gls254_add_affine_affine(&P, &pa, &qa);
			gls254_add(&R, &R, &P);
		} else if (k0 != 0) {
			gls254_lookup8_affine(&pa, win, k0);
			gls254_add_affine(&R, &R, &pa);
		} else if (k1 != 0) {
			gls254_lookup8_affine_zeta(&qa, win, k1, 0);
			gls254_add_affine(&R, &R, &qa);
		}

		k0 = sd2[i];
		k1 = sd2[i + 16];
		if (k0 != 0 && k1 != 0) {
			gls254_lookup8_affine_sign(&pa, PRECOMP_B, k0, t0);
			gls254_lookup8_affine_sign(&qa, PRECOMP_B64, k1, t0);
			gls254_add_affine_affine(&P, &pa, &qa);
			gls254_add(&R, &R, &P);
		} else if (k0 != 0) {
			gls254_lookup8_affine_sign(&pa, PRECOMP_B, k0, t0);
			gls254_add_affine(&R, &R, &pa);
		} else if (k1 != 0) {
			gls254_lookup8_affine_sign(&qa, PRECOMP_B64, k1, t0);
			gls254_add_affine(&R, &R, &qa);
		}

		k0 = sd3[i];
		k1 = sd3[i + 16];
		if (k0 != 0 && k1 != 0) {
			gls254_lookup8_affine_zeta(&pa, PRECOMP_B, k0, t1);
			gls254_lookup8_affine_zeta(&qa, PRECOMP_B64, k1, t1);
			gls254_add_affine_affine(&P, &pa, &qa);
			gls254_add(&R, &R, &P);
		} else if (k0 != 0) {
			gls254_lookup8_affine_zeta(&pa, PRECOMP_B, k0, t1);
			gls254_add_affine(&R, &R, &pa);
		} else if (k1 != 0) {
			gls254_lookup8_affine_zeta(&qa, PRECOMP_B64, k1, t1);
			gls254_add_affine(&R, &R, &qa);
		}
	}

	uint8_t cb[16];
	make_challenge(cb, &R, &pk->enc, hash_name, data, data_len);
	return memcmp(sigbuf, cb, 16) == 0;
}

/* see gls254.h */
int
gls254_ECDH(void *shared_key, const gls254_private_key *sk,
	const gls254_public_key *pk_peer)
{
	// Set the "bad" flag to True if the peer key was invalid.
	uint32_t bad = gls254_isneutral(&pk_peer->pp);

	// Compute shared point.
	uint8_t shared[32];
	gls254_point p;
	gls254_mul(&p, &pk_peer->pp, sk->sec);
	gls254_encode(shared, &p);

	// If the peer public key was bad, then use our private key
	// as the "shared" secret instead. This will lead to an output
	// key unguessable by outsiders, but will not otherwise leak
	// whether the process worked or not.
	for (int i = 0; i < 32; i ++) {
		shared[i] ^= bad & (shared[i] ^ sk->sec[i]);
	}

	// Key derivation with BLAKE2s.
	// We order the two public keys lexicographically.
	uint8_t tmp[64];
	uint32_t cc = 0;
	for (int i = 31; i >= 0; i --) {
		cc = ((uint32_t)sk->pub.enc[i]
			- (uint32_t)pk_peer->enc[i] - cc) >> 31;
	}
	uint32_t zx = cc - 1;
	for (int i = 0; i < 32; i ++) {
		uint32_t z1 = sk->pub.enc[i];
		uint32_t z2 = pk_peer->enc[i];
		uint32_t zz = zx & (z1 ^ z2);
		tmp[i] = z1 ^ zz;
		tmp[i + 32] = z2 ^ zz;
	}
	blake2s_context bc;
	blake2s_init(&bc, 32);
	blake2s_update(&bc, tmp, 64);
	tmp[0] = 0x53 - (bad & (0x53 - 0x46));
	blake2s_update(&bc, tmp, 1);
	blake2s_update(&bc, shared, 32);
	blake2s_final(&bc, shared_key);
	return (int)(bad + 1);
}

/* see gls254.h */
int
gls254_raw_ECDH(void *dst, const void *src, const void *scalar)
{
	gls254_point p;
	gls254_point_affine pa;

	// Decode the source point. Return early if invalid (the source
	// point is considered public data in this function).
	if (!gls254_uncompressed_decode(&pa, src)) {
		return 0;
	}
	gls254_from_affine(&p, &pa);

	// Note: there are a few minor optimizations that we do not do
	// here; since the source point is affine, the window construction
	// in gls254_make_window_affine_8() (called by gls254_mul())
	// could skip a few multiplications.

	// Multiply by the scalar.
	gls254_mul(&p, &p, scalar);

	// Normalize the result to affine and encode into the output.
	gls254_normalize(&pa, &p);
	gls254_uncompressed_encode(dst, &pa);
	return 1;
}

/* TODO: move the precomputed points to the assembly file?
   Contrary to the rest of this file, they depend on the actual in-memory
   format of points. */

/* Point i*B for i = 1 to 8, affine format (scaled_x, scaled_s) */
static const gls254_point_affine PRECOMP_B[] = {
	// B * 1
	{ { { { { 0x326B8675, 0xB6412F20, 0x9AE29894, 0x657CB9F7 } },
	      { { 0xF66DD010, 0x3932450F, 0xB2E3915E, 0x14C6F62C } } } },
	  { { { { 0x023DC896, 0x5FADCA04, 0xA04300F1, 0x763522AD } },
	      { { 0x9E07345A, 0x206E4C1E, 0x2381CA6D, 0x4F69A66A } } } } },
	// B * 2
	{ { { { { 0xD693FA8F, 0x415A7930, 0xDF2F1CA6, 0x1D78874E } },
	      { { 0xDAE036F7, 0xF61DEA7C, 0xE5F279EA, 0x4B30C0F5 } } } },
	  { { { { 0xFBD6BE01, 0xC19ED043, 0x6ABE9465, 0x693D8F2F } },
	      { { 0xD452AB50, 0x0F2F0D9C, 0x0A6EE21C, 0x19720E49 } } } } },
	// B * 3
	{ { { { { 0x1889FE19, 0x0BC57355, 0x1393238B, 0x665C451B } },
	      { { 0x27CA6F4D, 0xE053B1D0, 0x34043EA7, 0x5C27A07D } } } },
	  { { { { 0xA1F56BB6, 0xFE1E7723, 0x7D15931D, 0x7B780510 } },
	      { { 0xE184E5DF, 0xAE7D87EF, 0xF11925D5, 0x0F6F5F4E } } } } },
	// B * 4
	{ { { { { 0x06C9A0C8, 0xA11DB5F2, 0xC72A3AB3, 0x061309D0 } },
	      { { 0xEED4F57B, 0x91999BBE, 0xC3C0D1DA, 0x77F10DBD } } } },
	  { { { { 0x812A13C2, 0x38EE9EC6, 0x9DCA6BB5, 0x77FBC24A } },
	      { { 0xC034074B, 0x181DB8C3, 0xA8E44BBD, 0x6D296D30 } } } } },
	// B * 5
	{ { { { { 0xCF1FAB5F, 0xC715B038, 0x610AD947, 0x0DA235C1 } },
	      { { 0x7E52B936, 0xD3AC0FF5, 0x42EA1434, 0x7094DAC3 } } } },
	  { { { { 0x32462848, 0x06A589BB, 0x1566BBAF, 0x0F876725 } },
	      { { 0x17C2DAAB, 0x9F808AC9, 0x55FE4D2C, 0x32B14A68 } } } } },
	// B * 6
	{ { { { { 0x2FEA71F8, 0xB210B545, 0x921194F5, 0x14D11ED1 } },
	      { { 0x4E3E4518, 0x476FF44B, 0x007A5A24, 0x6F68AAC2 } } } },
	  { { { { 0x43C891FA, 0x57BE3BF0, 0x548C5D6C, 0x4F28EEAF } },
	      { { 0xE898732D, 0x72895485, 0xB3EB369B, 0x5683B98C } } } } },
	// B * 7
	{ { { { { 0xA16EAC69, 0x1F6121CE, 0xBC02778C, 0x19EB28FD } },
	      { { 0xB2803207, 0x0E86728B, 0xD9893789, 0x03E9B9FC } } } },
	  { { { { 0x7604ABE1, 0x13DE2DAE, 0xA6611933, 0x5121D6B7 } },
	      { { 0x9644C754, 0xAFC835F3, 0xE19E6CB3, 0x0A1F6E2D } } } } },
	// B * 8
	{ { { { { 0xF80BD001, 0xCDCB2821, 0xC02477B7, 0x4D1FCC11 } },
	      { { 0x237C442C, 0x2A6A17AF, 0xD4D6114C, 0x1301DB82 } } } },
	  { { { { 0x44C7077A, 0x83CF1AA2, 0xBC942DCB, 0x327AC316 } },
	      { { 0x8D0BBFA4, 0xAA4C2E84, 0x2A0788B2, 0x235DF1F9 } } } } }
};

/* Point i*(2^32)*B for i = 1 to 8, affine format (scaled_x, scaled_s) */
static const gls254_point_affine PRECOMP_B32[] = {
	// (2^32)*B * 1
	{ { { { { 0x4DDB30B8, 0x63557581, 0x5030FA03, 0x5B61982B } },
	      { { 0x22FC0A21, 0x11DFBA3C, 0x0F317C69, 0x59B8AAF2 } } } },
	  { { { { 0x4BA656F7, 0x24CCD3E5, 0x8F12A690, 0x75E44943 } },
	      { { 0x83593FAD, 0x35A7574A, 0xD281984B, 0x605B7617 } } } } },
	// (2^32)*B * 2
	{ { { { { 0x63E928F5, 0x90CF4E35, 0x5223D2E7, 0x50074E81 } },
	      { { 0x354B113C, 0x5C404A45, 0xC8167241, 0x0FA6E6AE } } } },
	  { { { { 0x6DA726AA, 0xA1301F5B, 0x36FADE6F, 0x417E796A } },
	      { { 0xA030F951, 0x132B507C, 0x27837BD6, 0x1B059582 } } } } },
	// (2^32)*B * 3
	{ { { { { 0xBD1848ED, 0x3EB8194B, 0xA973E23F, 0x49233033 } },
	      { { 0x9659B3C6, 0x162E3AC5, 0xCF1B0A47, 0x55D7E164 } } } },
	  { { { { 0x50D0746F, 0x8408AE6F, 0xDA5B5D8C, 0x54B1EF88 } },
	      { { 0xE0266218, 0xBEEF1BC0, 0x31BD68F4, 0x47AEBA16 } } } } },
	// (2^32)*B * 4
	{ { { { { 0xFEBCA318, 0xACDCDE13, 0x6F23CA1C, 0x2054A068 } },
	      { { 0x9A944830, 0x4FC664CE, 0x5CC70929, 0x0EE62762 } } } },
	  { { { { 0xF712C3D2, 0x10FFCF13, 0x378DADCF, 0x7AEF8651 } },
	      { { 0x3A88BB41, 0x83BF078A, 0xED94CCB7, 0x6540AA59 } } } } },
	// (2^32)*B * 5
	{ { { { { 0x548A5B5C, 0xC3A9CDF7, 0x09251988, 0x7CC55823 } },
	      { { 0x1D8FB1B6, 0x359CFD6F, 0xAEBB6DFE, 0x16617EA6 } } } },
	  { { { { 0x3B570A8D, 0x5402D084, 0xDDCA45AF, 0x72E1B8FC } },
	      { { 0x6A5F05AF, 0x8E894715, 0x4943A009, 0x47E972B5 } } } } },
	// (2^32)*B * 6
	{ { { { { 0x9D224CE8, 0x21ED4996, 0xF0314FFD, 0x502BF009 } },
	      { { 0x29EDEB9F, 0x378AD19D, 0x0F08EA14, 0x217F953E } } } },
	  { { { { 0x58AA005F, 0xFBEACAA8, 0xB084D2E9, 0x5C3956EA } },
	      { { 0x9C0AF3DD, 0x02FFF925, 0x4DA7B8F3, 0x2E6C4558 } } } } },
	// (2^32)*B * 7
	{ { { { { 0xA435418E, 0xEFE3D1C9, 0x78B4B863, 0x29220A41 } },
	      { { 0xAB5842A9, 0xF43A2709, 0x40A92711, 0x0C7C4F15 } } } },
	  { { { { 0x7D638424, 0x1C6B7791, 0x16ED213B, 0x611353A7 } },
	      { { 0x4BC271D7, 0x64291F07, 0x1D26E566, 0x65DD7EC4 } } } } },
	// (2^32)*B * 8
	{ { { { { 0xC194ECC9, 0x9B4DA61D, 0xCA8836CF, 0x7CB707BD } },
	      { { 0xD4BFFDF2, 0x77ACDF95, 0xD3A61F80, 0x36586184 } } } },
	  { { { { 0xC2CE8B3D, 0xCDAA62F1, 0x824B839B, 0x10B26E50 } },
	      { { 0x667C1F45, 0xFC7E3B92, 0x267FACD9, 0x27F128AB } } } } }
};

/* Point i*(2^64)*B for i = 1 to 8, affine format (scaled_x, scaled_s) */
static const gls254_point_affine PRECOMP_B64[] = {
	// (2^64)*B * 1
	{ { { { { 0x4D3AE7AC, 0x26123159, 0x28CEB8AD, 0x082A5BBF } },
	      { { 0x83030F30, 0xD959B911, 0x5AF1898E, 0x4447B9E0 } } } },
	  { { { { 0x4D3DE629, 0x2C7A5450, 0xA6F9484C, 0x431796A3 } },
	      { { 0xE5D3C8CD, 0x357D7D22, 0xE5323C2E, 0x147CCFFB } } } } },
	// (2^64)*B * 2
	{ { { { { 0xF207FAC6, 0x05704BF4, 0x161BD3A2, 0x0F16C7B1 } },
	      { { 0x870DEC6E, 0x1AD76AF2, 0xD0BF2740, 0x4FB614A7 } } } },
	  { { { { 0x28566D8A, 0x45D7C01C, 0x4077ABED, 0x005002FF } },
	      { { 0x5672D4B3, 0x6542A776, 0xA98AB48D, 0x04137083 } } } } },
	// (2^64)*B * 3
	{ { { { { 0xF0350244, 0x27C990FF, 0x857F7525, 0x18A5BC91 } },
	      { { 0x79997083, 0x6004C035, 0xE0E1B992, 0x1744491A } } } },
	  { { { { 0x17682DD3, 0x9C8593D7, 0x84AE8661, 0x402364E0 } },
	      { { 0xB2E2B9F3, 0x20F86314, 0x4D9B1FB5, 0x545AF79A } } } } },
	// (2^64)*B * 4
	{ { { { { 0xDDA5DC0B, 0x23CBD429, 0x6A5208C3, 0x27DF09B6 } },
	      { { 0x8B8FF984, 0x10BCC45E, 0x205DF31F, 0x4D7FE346 } } } },
	  { { { { 0xC97F02A7, 0x0CB81A89, 0x7D64DBF2, 0x3C1C9D27 } },
	      { { 0x704354B3, 0xF84A977B, 0x368738E4, 0x2C8704A6 } } } } },
	// (2^64)*B * 5
	{ { { { { 0x448B153F, 0x5FBA8828, 0x9A0F0423, 0x01E91ADB } },
	      { { 0x4BFABFFD, 0x1441B534, 0xA4E2D56F, 0x6D0A611A } } } },
	  { { { { 0xC6ED13FE, 0x67C71E1C, 0xA6321549, 0x4DEFBBD5 } },
	      { { 0x5515923F, 0xA187801F, 0xB7921BE3, 0x5FAD2693 } } } } },
	// (2^64)*B * 6
	{ { { { { 0x9A07D071, 0xCBD5E245, 0xCE94BD91, 0x578067F7 } },
	      { { 0x22EBB7B9, 0x393D9B57, 0xF4C2C566, 0x07F1E938 } } } },
	  { { { { 0x7ACE6FEC, 0xAF27AF4B, 0x2CE0A5CF, 0x6DE1B7A6 } },
	      { { 0x633B4D64, 0xD0C6FCA2, 0x989F7B92, 0x2813A2EA } } } } },
	// (2^64)*B * 7
	{ { { { { 0x41DBB5A8, 0x0A58149A, 0x1073E8F8, 0x106DF92D } },
	      { { 0x493E86B2, 0x197899FC, 0x2197B358, 0x2E0E05CE } } } },
	  { { { { 0x709BC381, 0x2D603F9B, 0xA19EED77, 0x26507080 } },
	      { { 0x1A0926FE, 0x5D86707B, 0x55F08B86, 0x2C55B877 } } } } },
	// (2^64)*B * 8
	{ { { { { 0xCD1523B9, 0x43086DD4, 0x4CF14DC9, 0x25B6941E } },
	      { { 0x40028B29, 0x0C30580B, 0xA4F8EDDF, 0x6B6816FF } } } },
	  { { { { 0x84749178, 0xB9FFB6EF, 0x8D83172B, 0x16BFA2F7 } },
	      { { 0x577E2135, 0xCD9F9599, 0xC1FB34BF, 0x0B9E5031 } } } } }
};

/* Point i*(2^96)*B for i = 1 to 8, affine format (scaled_x, scaled_s) */
static const gls254_point_affine PRECOMP_B96[] = {
	// (2^96)*B * 1
	{ { { { { 0xDA88E093, 0x653346E6, 0x9CD13872, 0x30002265 } },
	      { { 0x5F29D20B, 0x65532D39, 0x7CB5DE42, 0x30FE4C5C } } } },
	  { { { { 0x421D4A31, 0x0D181FE3, 0x94F4D3F7, 0x35F3E726 } },
	      { { 0xDD3ED40C, 0x0AB661AD, 0x4F2CADE5, 0x542B83C0 } } } } },
	// (2^96)*B * 2
	{ { { { { 0xCF11A8C7, 0x5450A803, 0x1DB4620C, 0x1A3EFC52 } },
	      { { 0xB4D6810F, 0x3FA30220, 0x1BC8AF08, 0x56C04218 } } } },
	  { { { { 0xFCE09354, 0x97E3B24D, 0xE7E9C001, 0x7B0F3BAF } },
	      { { 0xBD91FC40, 0x2DD1D729, 0xC21B1AD2, 0x05C74680 } } } } },
	// (2^96)*B * 3
	{ { { { { 0x431C5C00, 0x8F7A7F37, 0x22605514, 0x4487CC96 } },
	      { { 0x955E5D1C, 0x754A0DB2, 0xB8D0072A, 0x6AA1BE4A } } } },
	  { { { { 0x6B1BFC14, 0xA6D4611F, 0x6B2E8951, 0x00390364 } },
	      { { 0x0D536882, 0x723A689D, 0x973B29AB, 0x3B33B3BD } } } } },
	// (2^96)*B * 4
	{ { { { { 0xF4444850, 0xE2D4EE8A, 0xD2D38B53, 0x7C4CCD23 } },
	      { { 0xECC474E6, 0x66C8957A, 0x9CF325E5, 0x70291606 } } } },
	  { { { { 0xE0752CC9, 0x6FEC1E66, 0x3FC42538, 0x3E40F3D7 } },
	      { { 0x8A03A6D1, 0x5E66D9FE, 0x77C4AEDF, 0x73FDAD68 } } } } },
	// (2^96)*B * 5
	{ { { { { 0x4F97E0A6, 0x20505FA3, 0x16909F86, 0x79ACB745 } },
	      { { 0x82094271, 0xA163A5DC, 0x2F63A6BC, 0x1B6E5456 } } } },
	  { { { { 0x7E812C96, 0x9EFD3DD1, 0x136FD51D, 0x6901EB6C } },
	      { { 0xC0488EEA, 0x13157F6F, 0x0270A4C0, 0x67729C40 } } } } },
	// (2^96)*B * 6
	{ { { { { 0xE30AA449, 0xDBEAF734, 0xB81EC506, 0x2E1D908E } },
	      { { 0x61127B0E, 0xF2611727, 0xBA512D9F, 0x2DC2FA82 } } } },
	  { { { { 0x68E311D9, 0x44172899, 0xD5748EBC, 0x57F6D770 } },
	      { { 0x99E2D413, 0x97723CD4, 0xCC746EF0, 0x283638AE } } } } },
	// (2^96)*B * 7
	{ { { { { 0x8B0BCCC7, 0xE16BBA3D, 0x44C9E28F, 0x29BE1EE4 } },
	      { { 0x751536A3, 0x6E4A728A, 0x00888F7C, 0x08FD01F0 } } } },
	  { { { { 0x6105457B, 0x3346C207, 0x67B0008B, 0x290BC8D9 } },
	      { { 0x8C9C3D6E, 0xCC0E64B7, 0x2E01B797, 0x14197A7C } } } } },
	// (2^96)*B * 8
	{ { { { { 0xF4B109E4, 0x891B5765, 0x03AA5B0A, 0x4C341F78 } },
	      { { 0xB329C9A0, 0x7DF0A0F3, 0x55940920, 0x6E637EAE } } } },
	  { { { { 0x7624B8A0, 0x81C1B2EF, 0x54F22B55, 0x528F805E } },
	      { { 0x7A0FFB48, 0x43A540E6, 0x07BE133F, 0x7A79D0B6 } } } } }
};
