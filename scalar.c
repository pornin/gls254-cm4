/*
 * Implementation of scalars, i.e. integers modulo the prime r.
 * r = r = 2^253 + 83877821160623817322862211711964450037
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "inner.h"

static inline uint32_t
dec32le(const void *src)
{
	const uint8_t *buf = src;
	return (uint32_t)buf[0]
		| ((uint32_t)buf[1] << 8)
		| ((uint32_t)buf[2] << 16)
		| ((uint32_t)buf[3] << 24);
}

static inline void
enc32le(void *dst, uint32_t x)
{
	uint8_t *buf = dst;
	buf[0] = (uint8_t)x;
	buf[1] = (uint8_t)(x >> 8);
	buf[2] = (uint8_t)(x >> 16);
	buf[3] = (uint8_t)(x >> 24);
}

static inline unsigned char
addcarry_u32(unsigned char cc, uint32_t x, uint32_t y, uint32_t *z)
{
	uint64_t w = (uint64_t)x + (uint64_t)y + (uint64_t)cc;
	*z = (uint32_t)w;
	return (unsigned char)(w >> 32);
}

static inline unsigned char
subborrow_u32(unsigned char cc, uint32_t x, uint32_t y, uint32_t *z)
{
	uint64_t w = (uint64_t)x - (uint64_t)y - (uint64_t)cc;
	*z = (uint32_t)w;
	return (unsigned char)(w >> 63);
}

/*
 * Types for integers that fit on two, four, six or eight 64-bit limbs,
 * respectively. Depending on context, these types may be used with
 * signed or unsigned interpretation.
 */
typedef struct {
	uint32_t v[4];
} i128;
typedef struct {
	uint32_t v[8];
} i256;
typedef struct {
	uint32_t v[12];
} i384;
typedef struct {
	uint32_t v[16];
} i512;

/* Forward declaration of functions for reduction modulo r. These
   functions are defined in curve-specific files. */
static void modr_reduce256_partial(i256 *d, const i256 *a, uint32_t ah);
static void modr_reduce256_finish(i256 *d, const i256 *a);
static void modr_reduce384_partial(i256 *d, const i384 *a);

/* unused
static void
i128_decode(i128 *d, const void *a)
{
	const uint8_t *buf;
	int i;

	buf = a;
	for (i = 0; i < 4; i ++) {
		d->v[i] = dec32le(buf + 4 * i);
	}
}
*/

/*
 * Encode a 128-bit signed integer as an absolute value and a sign.
 * Sign is returned as 0xFFFFFFFF (negative) or 0x00000000 (non-negative).
 */
static uint32_t
i128_abs_encode(void *d, const i128 *a)
{
	void *buf = d;
	uint32_t s = (uint32_t)(*(int32_t *)&a->v[3] >> 31);
	unsigned char cc = 0;
	for (int i = 0; i < 4; i ++) {
		uint32_t x;
		cc = subborrow_u32(cc, a->v[i] ^ s, s, &x);
		enc32le(buf + (i << 2), x);
	}
	return s;
}

/*
 * Decode a 32-byte integer.
 */
static void
i256_decode(i256 *d, const void *a)
{
	const uint8_t *buf;
	int i;

	buf = a;
	for (i = 0; i < 8; i ++) {
		d->v[i] = dec32le(buf + 4 * i);
	}
}

/*
 * Encode a 32-byte integer.
 */
static void
i256_encode(void *d, const i256 *a)
{
	uint8_t *buf;
	int i;

	buf = d;
	for (i = 0; i < 8; i ++) {
		enc32le(buf + 4 * i, a->v[i]);
	}
}

/*
 * Test whether a 256-bit integer is zero.
 */
static int
i256_is_zero(const i256 *a)
{
	uint32_t x = a->v[0] | a->v[1] | a->v[2] | a->v[3]
		| a->v[4] | a->v[5] | a->v[6] | a->v[7];
	return x == 0;
}

/*
 * Subtract a 128-bit value from another (truncated result).
 */
static void
sub128trunc(i128 *d, const i128 *a, const i128 *b)
{
	unsigned char cc = 0;
	for (int i = 0; i < 4; i ++) {
		cc = subborrow_u32(cc, a->v[i], b->v[i], &d->v[i]);
	}
}

/*
 * Multiply two 128-bit integers, result is truncated to 128 bits.
 */
static void
mul128x128trunc(i128 *d, const i128 *a, const i128 *b)
{
	int i, j;
	uint32_t f, g;
	uint64_t z;
	i128 t;

	f = b->v[0];
	g = 0;
	for (i = 0; i < 4; i ++) {
		z = (uint64_t)f * (uint64_t)a->v[i] + (uint64_t)g;
		t.v[i] = (uint32_t)z;
		g = (uint32_t)(z >> 32);
	}
	for (j = 1; j < 4; j ++) {
		f = b->v[j];
		g = 0;
		for (i = 0; i < (4 - j); i ++) {
			z = (uint64_t)f * (uint64_t)a->v[i] + (uint64_t)g;
			z += (uint64_t)t.v[i + j];
			t.v[i + j] = (uint32_t)z;
			g = (uint32_t)(z >> 32);
		}
	}
	*d = t;
}

/*
 * Multiply two 128-bit integers, result is 256 bits.
 */
static void
mul128x128(i256 *d, const i128 *a, const i128 *b)
{
	int i, j;
	uint32_t f, g;
	uint64_t z;

	f = b->v[0];
	g = 0;
	for (i = 0; i < 4; i ++) {
		z = (uint64_t)f * (uint64_t)a->v[i] + (uint64_t)g;
		d->v[i] = (uint32_t)z;
		g = (uint32_t)(z >> 32);
	}
	d->v[4] = g;
	for (j = 1; j < 4; j ++) {
		f = b->v[j];
		g = 0;
		for (i = 0; i < 4; i ++) {
			z = (uint64_t)f * (uint64_t)a->v[i] + (uint64_t)g;
			z += (uint64_t)d->v[i + j];
			d->v[i + j] = (uint32_t)z;
			g = (uint32_t)(z >> 32);
		}
		d->v[j + 4] = g;
	}
}

/*
 * Multiply a 256-bit integer by a 128-bit integer, result is 384 bits.
 */
static void
mul256x128(i384 *d, const i256 *a, const i128 *b)
{
	i128 al, ah;
	i256 dl, dh;
	unsigned char cc;
	int i;

	memcpy(&al.v[0], &a->v[0], 4 * sizeof(uint32_t));
	mul128x128(&dl, &al, b);
	memcpy(&ah.v[0], &a->v[4], 4 * sizeof(uint32_t));
	mul128x128(&dh, &ah, b);
	memcpy(&d->v[0], &dl.v[0], 4 * sizeof(uint32_t));
	cc = addcarry_u32(0, dl.v[4], dh.v[0], &d->v[4]);
	for (i = 1; i < 4; i ++) {
		cc = addcarry_u32(cc, dl.v[4 + i], dh.v[i], &d->v[4 + i]);
	}
	for (i = 4; i < 8; i ++) {
		cc = addcarry_u32(cc, 0, dh.v[i], &d->v[4 + i]);
	}
}

/*
 * Multiply two 256-bit integers together, result is 512 bits.
 */
static void
mul256x256(i512 *d, const i256 *a, const i256 *b)
{
	i128 al, ah;
	i384 dl, dh;
	unsigned char cc;
	int i;

	memcpy(&al.v[0], &a->v[0], 4 * sizeof(uint32_t));
	mul256x128(&dl, b, &al);
	memcpy(&ah.v[0], &a->v[4], 4 * sizeof(uint32_t));
	mul256x128(&dh, b, &ah);
	memcpy(&d->v[0], &dl.v[0], 4 * sizeof(uint32_t));
	cc = addcarry_u32(0, dl.v[4], dh.v[0], &d->v[4]);
	for (i = 1; i < 8; i ++) {
		cc = addcarry_u32(cc, dl.v[4 + i], dh.v[i], &d->v[4 + i]);
	}
	for (i = 8; i < 12; i ++) {
		cc = addcarry_u32(cc, 0, dh.v[i], &d->v[4 + i]);
	}
}

/* unused
static void
modr_mul256x128(i256 *d, const i256 *a, const i128 *b)
{
	i256 t;
	i384 e;

	mul256x128(&e, a, b);
	modr_reduce384_partial(&t, &e);
	modr_reduce256_finish(d, &t);
}
*/

/*
 * Multiplication modulo r, with two 256-bit operands.
 * Input operands can use their full range; output is reduced.
 */
static void
modr_mul256x256(i256 *d, const i256 *a, const i256 *b)
{
	i256 t;
	i384 e;
	i512 x;

	mul256x256(&x, a, b);
	memcpy(&e.v[0], &x.v[4], 12 * sizeof(uint32_t));
	modr_reduce384_partial(&t, &e);
	memcpy(&e.v[0], &x.v[0], 4 * sizeof(uint32_t));
	memcpy(&e.v[4], &t.v[0], 8 * sizeof(uint32_t));
	modr_reduce384_partial(&t, &e);
	modr_reduce256_finish(d, &t);
}

/* see gls254.h */
void
scalar_reduce(void *d, const void *a, size_t a_len)
{
	const uint8_t *buf;
	size_t k;
	i256 t;

	/*
	 * If the source value is shorter than 32 bytes, then we can
	 * just use it, it's already reduced.
	 */
	if (a_len < 32) {
		memmove(d, a, a_len);
		memset((unsigned char *)d + a_len, 0, 32 - a_len);
		return;
	}

	/*
	 * Decode high bytes; we use as many bytes as possible, but no
	 * more than 32, and such that the number of undecoded bytes is
	 * a multiple of 16. Also ensure that these initial contents are
	 * partially reduced.
	 */
	buf = a;
	k = a_len & 31;
	if (k == 0) {
		i256_decode(&t, buf + a_len - 32);
		buf += a_len - 32;
		modr_reduce256_partial(&t, &t, 0);
	} else if (k == 16) {
		i256_decode(&t, buf + a_len - 32);
		buf += a_len - 32;
	} else {
		uint8_t tmp[32];

		if (k < 16) {
			k += 16;
		}
		memcpy(tmp, buf + a_len - k, k);
		memset(tmp + k, 0, 32 - k);
		i256_decode(&t, tmp);
		buf += a_len - k;
	}

	/*
	 * For each subsequent chunk of 16 bytes, shift and inject the
	 * new chunk, then reduce (partially).
	 */
	while (buf != a) {
		i384 t2;

		buf -= 16;
		t2.v[0] = dec32le(buf);
		t2.v[1] = dec32le(buf + 4);
		t2.v[2] = dec32le(buf + 8);
		t2.v[3] = dec32le(buf + 12);
		memcpy(&t2.v[4], &t.v[0], 8 * sizeof(uint32_t));
		modr_reduce384_partial(&t, &t2);
	}

	/*
	 * Value is almost reduced; it fits on 255 bits.
	 * A single conditional subtraction yields the correct result.
	 */
	modr_reduce256_finish(&t, &t);
	i256_encode(d, &t);
}

/* see gls254.h */
void
scalar_add(void *d, const void *a, const void *b)
{
	i256 ta, tb, td;
	uint32_t t4;
	unsigned char cc;
	int i;

	i256_decode(&ta, a);
	i256_decode(&tb, b);
	cc = addcarry_u32(0, ta.v[0], tb.v[0], &td.v[0]);
	for (i = 1; i < 8; i ++) {
		cc = addcarry_u32(cc, ta.v[i], tb.v[i], &td.v[i]);
	}
	t4 = cc;
	modr_reduce256_partial(&td, &td, t4);
	modr_reduce256_finish(&td, &td);
	i256_encode(d, &td);
}

/* see gls254.h */
void
scalar_mul(void *d, const void *a, const void *b)
{
	i256 ta, tb, td;

	i256_decode(&ta, a);
	i256_decode(&tb, b);
	modr_mul256x256(&td, &ta, &tb);
	i256_encode(d, &td);
}

/*
 * r = 2^253 + r0, with r0 = 83877821160623817322862211711964450037.
 */
static const i128 R0 = { {
	0xF43A8CF5, 0x3CBDE37C, 0xDC1A1DAD, 0x3F1A47DE
} };
#define R_top   ((uint32_t)0x20000000)

/*
 * 1/2 mod r = (r+1)/2
 */
static const i256 Rhf = { {
	0x7A1D467B, 0x9E5EF1BE, 0x6E0D0ED6, 0x1F8D23EF,
	0x00000000, 0x00000000, 0x00000000, 0x10000000
} };

/*
 * -1/2 mod r = (r-1)/2 (padded to 384 bits)
 */
static const i384 HR_pad = { {
	0x7A1D467A, 0x9E5EF1BE, 0x6E0D0ED6, 0x1F8D23EF,
	0x00000000, 0x00000000, 0x00000000, 0x10000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000
} };

/*
 * 8*r0 (mod 2^128)
 */
static const i128 R0_x8m = { {
	0xA1D467A8, 0xE5EF1BE7, 0xE0D0ED69, 0xF8D23EF6
} };
#define R_x8_top   ((uint32_t)0x00000001)

/*
 * 16*r (mod 2^256)
 */
static const i256 R_x16m = { {
	0x43A8CF50, 0xCBDE37CF, 0xC1A1DAD3, 0xF1A47DED,
	0x00000003, 0x00000000, 0x00000000, 0x00000000
} };
#define R_x16_top   ((uint32_t)0x00000002)

/*
 * Given input 'a' (up to 2^285-1), perform a partial reduction modulo r;
 * output (into 'd') fits on 254 bits and thus is less than 2*r. The high
 * bits of 'a' are provided as extra parameter ah.
 */
static void
modr_reduce256_partial(i256 *d, const i256 *a, uint32_t ah)
{
	i256 t;
	uint32_t u[5], x;
	unsigned char cc;
	int i;

	/*
	 * Truncate the source to 253 bits, and apply reduction (with
	 * 2^253 = -r0 mod r) for the top bits.
	 */
	ah = (ah << 3) | (a->v[7] >> 29);
	memcpy(&t.v[0], &a->v[0], 7 * sizeof(uint32_t));
	t.v[7] = a->v[7] & 0x1FFFFFFF;
	x = 0;
	for (i = 0; i < 4; i ++) {
		uint64_t z;

		z = (uint64_t)ah * (uint64_t)R0.v[i] + (uint64_t)x;
		u[i] = (uint32_t)z;
		x = (uint32_t)(z >> 32);
	}
	u[4] = x;

	cc = subborrow_u32(0, t.v[0], u[0], &t.v[0]);
	for (i = 1; i < 5; i ++) {
		cc = subborrow_u32(cc, t.v[i], u[i], &t.v[i]);
	}
	for (i = 5; i < 8; i ++) {
		cc = subborrow_u32(cc, t.v[i], 0, &t.v[i]);
	}

	/*
	 * We may get a borrow here, in which case the value is negative.
	 * But since we subtracted an integer lower than 2^192, adding r
	 * once will be enough to get back to the expected range.
	 */
	x = -(uint32_t)cc;
	cc = addcarry_u32(0, t.v[0], x & R0.v[0], &d->v[0]);
	for (i = 1; i < 4; i ++) {
		cc = addcarry_u32(cc, t.v[i], x & R0.v[i], &d->v[i]);
	}
	for (i = 4; i < 7; i ++) {
		cc = addcarry_u32(cc, t.v[i], 0, &d->v[i]);
	}
	(void)addcarry_u32(cc, t.v[7], x & R_top, &d->v[7]);
}

/*
 * Given a partially reduced input (less than 2*r), finish reduction
 * (conditional subtraction of r).
 */
static void
modr_reduce256_finish(i256 *d, const i256 *a)
{
	uint32_t t[8], m;
	unsigned char cc;
	int i;

	cc = subborrow_u32(0, a->v[0], R0.v[0], &t[0]);
	for (i = 1; i < 4; i ++) {
		cc = subborrow_u32(cc, a->v[i], R0.v[i], &t[i]);
	}
	for (i = 4; i < 7; i ++) {
		cc = subborrow_u32(cc, a->v[i], 0, &t[i]);
	}
	cc = subborrow_u32(cc, a->v[7], R_top, &t[7]);

	/*
	 * If there was no borrow, then we keep the subtraction result;
	 * otherwise, we use the source value.
	 */
	m = -(uint32_t)cc;
	for (i = 0; i < 8; i ++) {
		d->v[i] = t[i] ^ (m & (t[i] ^ a->v[i]));
	}
}

/*
 * Given input 'a' (up to 2^384-1), perform a partial reduction modulo r;
 * output (into 'd') fits on 254 bits (hence lower than 2*r).
 */
static void
modr_reduce384_partial(i256 *d, const i384 *a)
{
	i128 a1;
	i256 t;
	uint32_t t8, m;
	unsigned char cc;
	int i;

	/*
	 * Since r = 2^253 + r0, we have:
	 *   2^256 = -8*r0 mod r
	 * We write:
	 *   a = a0 + a1*2^256
	 * Then:
	 *   a = a0 - 8*r0*a1  mod r
	 * Since a1 < 2^128 and 8*r0 < 2^129, we know that:
	 *   2^256 > a0 - 8*r0*a1 > -2^257
	 * Thus, if the result of the subtraction is strictly negative,
	 * then we add 16*r, which is slightly above 2^257, and thus enough
	 * to ensure a non-negative result. A call to modr_reduce256_partial()
	 * then sets the result into the expected range.
	 */
	memcpy(&a1.v[0], &a->v[8], 4 * sizeof(uint32_t));
	mul128x128(&t, &a1, &R0_x8m);

	/* 8*r0*a1 = (8*r0 - 2^128)*a1 + a1*2^128 */
	cc = subborrow_u32(0, a->v[0], t.v[0], &t.v[0]);
	for (i = 1; i < 8; i ++) {
		cc = subborrow_u32(cc, a->v[i], t.v[i], &t.v[i]);
	}
	t8 = -(uint32_t)cc;
	cc = subborrow_u32(0, t.v[4], a1.v[0], &t.v[4]);
	for (i = 1; i < 4; i ++) {
		cc = subborrow_u32(cc, t.v[4 + i], a1.v[i], &t.v[4 + i]);
	}
	t8 -= (uint32_t)cc;

	/* Add 16*r if t < 0 */
	m = (uint32_t)(*(int32_t *)&t8 >> 31);
	cc = addcarry_u32(0, t.v[0], m & R_x16m.v[0], &t.v[0]);
	for (i = 1; i < 8; i ++) {
		cc = addcarry_u32(cc, t.v[i], m & R_x16m.v[i], &t.v[i]);
	}
	(void)addcarry_u32(cc, t8, m & R_x16_top, &t8);

	/* Partial reduction. */
	modr_reduce256_partial(d, &t, t8);
}

/* see gls254.h */
int
scalar_is_reduced(const void *a)
{
	i256 t;
	unsigned char cc;
	uint32_t x;

	i256_decode(&t, a);
	cc = subborrow_u32(0, t.v[0], R0.v[0], &x);
	cc = subborrow_u32(cc, t.v[1], R0.v[1], &x);
	cc = subborrow_u32(cc, t.v[2], R0.v[2], &x);
	cc = subborrow_u32(cc, t.v[3], R0.v[3], &x);
	cc = subborrow_u32(cc, t.v[4], 0, &x);
	cc = subborrow_u32(cc, t.v[5], 0, &x);
	cc = subborrow_u32(cc, t.v[6], 0, &x);
	cc = subborrow_u32(cc, t.v[7], R_top, &x);
	return cc;
}

/* see gls254.h */
int
scalar_is_zero(const void *a)
{
	i256 t;

	i256_decode(&t, a);
	return i256_is_zero(&t);
}

/* see gls254.h */
void
scalar_sub(void *d, const void *a, const void *b)
{
	i256 ta, tb, td;
	uint32_t t8, m;
	unsigned char cc;
	int i;

	i256_decode(&ta, a);
	i256_decode(&tb, b);
	cc = subborrow_u32(0, ta.v[0], tb.v[0], &td.v[0]);
	for (i = 1; i < 8; i ++) {
		cc = subborrow_u32(cc, ta.v[i], tb.v[i], &td.v[i]);
	}

	/*
	 * If subtraction yielded a negative value, then we add 8*r. Note
	 * that 2^256 < 8*r < 2^257; thus, we always get a nonnegative
	 * value that fits on 257 bits.
	 */
	m = -(uint32_t)cc;
	cc = addcarry_u32(0, td.v[0], m & R0_x8m.v[0], &td.v[0]);
	for (i = 1; i < 4; i ++) {
		cc = addcarry_u32(cc, td.v[i], m & R0_x8m.v[i], &td.v[i]);
	}
	cc = addcarry_u32(cc, td.v[4], m & 1, & td.v[4]);
	for (i = 5; i < 8; i ++) {
		cc = addcarry_u32(cc, td.v[i], 0, &td.v[i]);
	}
	t8 = (uint32_t)cc;

	/*
	 * Reduce modulo r.
	 */
	modr_reduce256_partial(&td, &td, t8);
	modr_reduce256_finish(&td, &td);
	i256_encode(d, &td);
}

/* see gls254.h */
void
scalar_neg(void *d, const void *a)
{
	static const uint8_t zero[32] = { 0 };

	scalar_sub(d, zero, a);
}

/* see gls254.h */
void
scalar_half(void *d, const void *a)
{
	i256 x;
	unsigned char cc;
	uint32_t m;
	int i;

	i256_decode(&x, a);
	m = -(x.v[0] & 1);

	/*
	 * Right-shift value. Result is lower than 2^255.
	 */
	for (i = 0; i < 7; i ++) {
		x.v[i] = (x.v[i] >> 1) | (x.v[i + 1] << 31);
	}
	x.v[7] = (x.v[7] >> 1);

	/*
	 * Add (r+1)/2 if the value was odd. This cannot overflow
	 * since r < 2^255.
	 */
	cc = addcarry_u32(0, x.v[0], m & Rhf.v[0], &x.v[0]);
	for (i = 1; i < 8; i ++) {
		cc = addcarry_u32(cc, x.v[i], m & Rhf.v[i], &x.v[i]);
	}

	/*
	 * Ensure a reduced value.
	 */
	modr_reduce256_partial(&x, &x, 0);
	modr_reduce256_finish(&x, &x);
	i256_encode(d, &x);
}

/*
 * For integers k and e such that:
 *   k < r
 *   e < (2^127 - 2)
 * set d to round(k*e/r).
 */
static void
mul_divr_rounded(i128 *d, const i256 *k, const i128 *e)
{
	/* z <- k*e */
	i384 z;
	mul256x128(&z, k, e);

	/* z <- z + (r-1)/2 */
	unsigned char cc = 0;
	for (int i = 0; i < 12; i ++) {
		cc = addcarry_u32(cc, z.v[i], HR_pad.v[i], &z.v[i]);
	}

	/* Split z = z0 + z1*2^253 */
	i256 z0;
	i128 z1;
	memcpy(&z0.v, &z.v, 7 * sizeof(uint32_t));
	z0.v[7] = z.v[7] & 0x1FFFFFFF;
	uint32_t g = z.v[7] >> 29;
	for (int i = 0; i < 4; i ++) {
		z1.v[i] = (z.v[i + 8] << 3) | g;
		g = z.v[i + 8] >> 29;
	}

	/* t <- z1*r0 */
	i256 t;
	mul128x128(&t, &z1, &R0);

	/* Subtract t from z0; we are only interested in the resulting
	   borrow (0 or 1), which must be subtracted from z1. */
	cc = 0;
	for (int i = 0; i < 8; i ++) {
		uint32_t dummy;
		cc = subborrow_u32(cc, z0.v[i], t.v[i], &dummy);
	}
	for (int i = 0; i < 4; i ++) {
		cc = subborrow_u32(cc, z1.v[i], 0, &d->v[i]);
	}
}

/*
 * Split the scalar k into k0 and k1 such that k = k0 + k1*mu (with mu
 * being a specific square root of -1 modulo r).
 * k0 and k1 are signed.
 * IMPORTANT: k MUST be fully reduced for this call.
 */
static void
split_mu(i128 *k0, i128 *k1, const i256 *k)
{
	static const i128 vES = { {
		0x3FA56696, 0x639973CF, 0xFFFFFFFF, 0x3FFFFFFF
	} };
	static const i128 vET = { {
		0xC05A9969, 0x9C668C30, 0x00000000, 0x40000000
	} };

	/* c <- round(k*t/r)
	   d <- round(k*s/r) */
	i128 c, d;
	mul_divr_rounded(&c, k, &vET);
	mul_divr_rounded(&d, k, &vES);

	/* k0 <- k - d*s - c*t (truncated) */
	memmove(k0->v, k->v, 4 * sizeof(uint32_t));
	i128 w;
	mul128x128trunc(&w, &d, &vES);
	sub128trunc(k0, k0, &w);
	mul128x128trunc(&w, &c, &vET);
	sub128trunc(k0, k0, &w);

	/* k1 <- d*t - c*s (truncated) */
	mul128x128trunc(k1, &d, &vET);
	mul128x128trunc(&w, &c, &vES);
	sub128trunc(k1, k1, &w);
}

/* see gls254.h */
void
scalar_split(uint8_t *ak0, uint32_t *sk0, uint8_t *ak1, uint32_t *sk1,
	const void *k)
{
	i256 t;
	i128 k0, k1;

	i256_decode(&t, k);
	modr_reduce256_partial(&t, &t, 0);
	modr_reduce256_finish(&t, &t);
	split_mu(&k0, &k1, &t);
	*sk0 = i128_abs_encode(ak0, &k0);
	*sk1 = i128_abs_encode(ak1, &k1);
}
