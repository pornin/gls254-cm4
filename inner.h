/*
 * Inner API: functions declared in this file are meant only for use
 * within this implementation; many are used only for test purposes.
 */

#ifndef INNER_H__
#define INNER_H__

#include <stdint.h>
#include "gls254.h"

/* ---------------------------------------------------------------------- */
/*
 * GF(2^127) operations.
 *
 * All these functions are defined in the assembly source file. They have
 * external linkage only so as to support test code.
 *
 * General rules:
 *  - Everything is constant-time.
 *  - The `ctl` parameters must have value 0xFFFFFFFF ("True") or 0x00000000
 *    ("False").
 *  - `gfb127_iszero()`, `gfb127_equals()` and `gfb127_decode16()` return
 *    0xFFFFFFFF for "True", 0x00000000 for "False".
 *  - The bit functions (`gfb127_get_bit()`, `gfb127_set_bit()`,
 *    `gfb127_xor_bit()` and `gfb127_trace()`) represent bits as
 *    `uint32_t` with value 0 or 1. No other value shall be used.
 *  - In general, the output is called `d`, while inputs are tagged
 *    `const`. The output may be the same structure are any of the
 *    input operands.
 *
 * Notes on specific operations:
 * -----------------------------
 * Values in GF(2^127) are binary polynomials in GF[2](z), taken modulo
 * z^127 + z^63 + 1 (which is irreducible).
 *
 * mul_b() is multiplication by the constant 1 + z^54.
 *
 * mul_sb() is multiplicaiton by the constant sqrt(b) = 1 + z^27.
 *
 * div_z() is division by z.
 *
 * div_z2() is division by z^2.
 *
 * If the input of an inversion is zero, then the output is zero. Similarly,
 * a division by zero yields zero, regardless of the value of the dividend.
 *
 * GF(2^127) elements are encoded over exactly 16 bytes; the most significant
 * bit of the last byte is always 0. decode16_trunc() ignores that extra bit.
 * decode16_reduce() processes the extra bit with the value z^127 (i.e.
 * reduction of the input polynomial modulo the field modulus).
 * decode16() _verifies_ that the bit is zero; it returns 0xFFFFFFFF on
 * success. If the bit is not zero, then the output element is set to
 * zero, and 0x00000000 is returned.
 *
 * xsquare() computes n successive squarings.
 */

void gfb127_normalize(gfb127 *d, const gfb127 *a);
uint32_t gfb127_get_bit(const gfb127 *a, int k);
void gfb127_set_bit(gfb127 *a, int k, uint32_t val);
void gfb127_xor_bit(gfb127 *a, int k, uint32_t val);
void gfb127_set_cond(gfb127 *d, const gfb127 *a, uint32_t ctl);
void gfb127_add(gfb127 *d, const gfb127 *a, const gfb127 *b);
void gfb127_mul_sb(gfb127 *d, const gfb127 *a);
void gfb127_mul_b(gfb127 *d, const gfb127 *a);
void gfb127_div_z(gfb127 *d, const gfb127 *a);
void gfb127_div_z2(gfb127 *d, const gfb127 *a);
void gfb127_mul(gfb127 *d, const gfb127 *a, const gfb127 *b);
void gfb127_square(gfb127 *d, const gfb127 *a);
void gfb127_xsquare(gfb127 *d, const gfb127 *a, unsigned n);
void gfb127_invert(gfb127 *d, const gfb127 *a);
void gfb127_div(gfb127 *d, const gfb127 *a, const gfb127 *b);
void gfb127_sqrt(gfb127 *d, const gfb127 *a);
uint32_t gfb127_trace(const gfb127 *a);
void gfb127_halftrace(gfb127 *d, const gfb127 *a);
uint32_t gfb127_iszero(const gfb127 *a);
uint32_t gfb127_equals(const gfb127 *a, const gfb127 *b);
void gfb127_encode(void *dst, const gfb127 *a);
void gfb127_decode16_trunc(gfb127 *d, const void *src);
void gfb127_decode16_reduce(gfb127 *d, const void *src);
uint32_t gfb127_decode16(gfb127 *d, const void *src);

/* ---------------------------------------------------------------------- */
/*
 * GF(2^254) operations.
 *
 * All non-inline functions are defined in the assembly source file. They
 * have external linkage only so as to support test code.
 *
 * The same rules as for GF(2^127) code apply here.
 *
 * Extra notes:
 * ------------
 * GF(2^254) is defined as a degree-2 extension of GF(2^127): each element
 * x can be written as:
 *    x = x0 + u*x1
 * where x0 and x1 are elements of GF(2^127), and u is a formal element
 * such that u^2 + u + 1 = 0 (no such element exists in GF(2^127).
 *
 * The mul_u() and mul_u1() functions multiply their operand by u and u + 1,
 * respectively (note: u + 1 = u^2).
 *
 * add_u() adds u to its operand.
 *
 * mul_b127() multiplies an elemeng of GF(2^254) by an element of GF(2^127).
 *
 * qsolve(x) returns y such that y^2 + y = x + Tr(x)*u (with Tr(x) = trace of
 * x, always 0 or 1); there are always two such values y (y and y+1), and
 * it is not specified which of the two solutions is returned.
 *
 * mul_selfphi() multiplies an element x = x0 + u*x1 by the element
 * phi(x) = (x0 + x1) + u*x1, where phi() is the Frobenius automorphism
 * (i.e. phi(x) = x^(2^127)); the result is always an element of GF(2^127).
 *
 * Elements of GF(2^254) encode over exactly 32 bytes; most significant bits
 * of bytes 15 and 31 are always zero. decode32() verifies that the bits are
 * indeed zero, while decode32_trunc() _ignores_ the values of these bits,
 * and decode32_reduce() uses these bits with implicit reduction (like
 * decode16_reduce() for GF(2^127)).
 *
 * None of the inline functions defined below is used by non-test code.
 * The assembly implementation includes optimized versions of these
 * functions.
 */

static inline void
gfb254_add(gfb254 *d, const gfb254 *a, const gfb254 *b)
{
	gfb127_add(&d->v[0], &a->v[0], &b->v[0]);
	gfb127_add(&d->v[1], &a->v[1], &b->v[1]);
}

static inline void
gfb254_add_u(gfb254 *d, const gfb254 *a)
{
	if (d != a) {
		*d = *a;
	}
	d->v[1].v[0] ^= 1;
}

static inline void
gfb254_mul_sb(gfb254 *d, const gfb254 *a)
{
	gfb127_mul_sb(&d->v[0], &a->v[0]);
	gfb127_mul_sb(&d->v[1], &a->v[1]);
}

static inline void
gfb254_mul_b(gfb254 *d, const gfb254 *a)
{
	gfb127_mul_b(&d->v[0], &a->v[0]);
	gfb127_mul_b(&d->v[1], &a->v[1]);
}

static inline void
gfb254_div_z(gfb254 *d, const gfb254 *a)
{
	gfb127_div_z(&d->v[0], &a->v[0]);
	gfb127_div_z(&d->v[1], &a->v[1]);
}

static inline void
gfb254_div_z2(gfb254 *d, const gfb254 *a)
{
	gfb127_div_z2(&d->v[0], &a->v[0]);
	gfb127_div_z2(&d->v[1], &a->v[1]);
}

void gfb254_mul(gfb254 *d, const gfb254 *a, const gfb254 *b);

void gfb254_square(gfb254 *d, const gfb254 *a);

static inline void
gfb254_xsquare(gfb254 *d, const gfb254 *a, unsigned n)
{
	if (n == 0) {
		*d = *a;
		return;
	}
	gfb254_square(d, a);
	while (-- n > 0) {
		gfb254_square(d, d);
	}
}

static inline void
gfb254_mul_b127(gfb254 *d, const gfb254 *a, const gfb127 *b)
{
	gfb127_mul(&d->v[0], &a->v[0], b);
	gfb127_mul(&d->v[1], &a->v[1], b);
}

static inline void
gfb254_mul_u(gfb254 *d, const gfb254 *a)
{
	gfb127 t;
	gfb127_add(&t, &a->v[0], &a->v[1]);
	d->v[0] = a->v[1];
	d->v[1] = t;
}

static inline void
gfb254_mul_u1(gfb254 *d, const gfb254 *a)
{
	gfb127 t;
	gfb127_add(&t, &a->v[0], &a->v[1]);
	d->v[1] = a->v[0];
	d->v[0] = t;
}

void gfb254_mul_selfphi(gfb127 *d, const gfb254 *a);

void gfb254_invert(gfb254 *d, const gfb254 *a);

void gfb254_div(gfb254 *d, const gfb254 *a, const gfb254 *b);

void gfb254_sqrt(gfb254 *d, const gfb254 *a);

static inline uint32_t
gfb254_trace(const gfb254 *a)
{
	return gfb127_trace(&a->v[1]);
}

void gfb254_qsolve(gfb254 *d, const gfb254 *a);

static inline uint32_t
gfb254_equals(const gfb254 *a, const gfb254 *b)
{
	return gfb127_equals(&a->v[0], &b->v[0])
		& gfb127_equals(&a->v[1], &b->v[1]);
}

static inline uint32_t
gfb254_iszero(const gfb254 *a)
{
	return gfb127_iszero(&a->v[0])
		& gfb127_iszero(&a->v[1]);
}

void gfb254_encode(void *dst, const gfb254 *a);

void gfb254_decode32_trunc(gfb254 *d, const void *src);

void gfb254_decode32_reduce(gfb254 *d, const void *src);

uint32_t gfb254_decode32(gfb254 *d, const void *src);

static inline void
gfb254_set_cond(gfb254 *d, const gfb254 *a, uint32_t ctl)
{
	gfb127_set_cond(&d->v[0], &a->v[0], ctl);
	gfb127_set_cond(&d->v[1], &a->v[1], ctl);
}

/* ---------------------------------------------------------------------- */

/*
 * The uncompressed format is used only for the "raw ECDH" function, whose
 * purpose is benchmarks only; hence, decoding and encoding functions
 * for that format are declared in the non-public header.
 *
 * Uncompressed format is 64 bytes; it consists of the encodings of the
 * scaled_x and scaled_s coordinates, in that order. Encoding is
 * canonical; gls254_uncompressed_decode() returns 0 if the source point
 * is invalid (not canonical, or not on the curve, or not in the right
 * subset of the curve).
 *
 * On success, gls254_uncompressed_decode() returns 0xFFFFFFFF.
 */

uint32_t gls254_uncompressed_decode(gls254_point_affine *p, const void *src);
void gls254_uncompressed_encode(void *dst, const gls254_point_affine *p);

/*
 * Map an arbitrary sequence of 32 bytes to a curve point. This is an
 * internal function because it is biased; the hash-to-curve process calls
 * it twice, on two 32-byte strings derived (through hashing) from the
 * same input, and adds the values together.
 */
void gls254_map_to_point(gls254_point *p, const void *src);

/* ---------------------------------------------------------------------- */

#endif
