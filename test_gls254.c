/*
 * Test code. Accesses both internal and external functions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "inner.h"
#include "gls254.h"
#include "blake2.h"

static size_t
hextobin(void *dst, size_t dst_len, const char *src)
{
	size_t k = 0;
	int z = 0;
	unsigned acc = 0;
	for (;;) {
		int d = *src ++;
		if (d == 0) {
			if (z) {
				printf("invalid hex: odd number of digits\n");
				exit(EXIT_FAILURE);
			}
			return k;
		}
		if (d <= 32 || d == '-' || d == ':') {
			continue;
		}
		if (d >= '0' && d <= '9') {
			d -= '0';
		} else if (d >= 'A' && d <= 'F') {
			d -= 'A' - 10;
		} else if (d >= 'a' && d <= 'f') {
			d -= 'a' - 10;
		} else {
			printf("invalid hex character: %c\n", d);
			exit(EXIT_FAILURE);
		}
		if (z) {
			if (k >= dst_len) {
				printf("hex decode: overflow\n");
				exit(EXIT_FAILURE);
			}
			((uint8_t *)dst)[k ++] = (acc << 4) | d;
		} else {
			acc = d;
		}
		z = !z;
	}
}

#define HEXTOBIN(dst, src)   do { \
		size_t h2b_len = sizeof(dst); \
		if (hextobin(dst, h2b_len, src) != h2b_len) { \
			printf("hex decode: unexpected length\n"); \
			exit(EXIT_FAILURE); \
		} \
	} while (0)

static void
check_eq_buf(const char *msg, const void *b1, const void *b2, size_t len)
{
	if (memcmp(b1, b2, len) == 0) {
		return;
	}
	printf("\nFAIL: %s\n", msg);
	for (size_t u = 0; u < len; u ++) {
		printf("%02x", ((const uint8_t *)b1)[u]);
	}
	printf("\n");
	for (size_t u = 0; u < len; u ++) {
		printf("%02x", ((const uint8_t *)b2)[u]);
	}
	printf("\n");
	exit(EXIT_FAILURE);
}

static void
check_consistent(const char *msg, const gls254_point *p)
{
	gfb254 t0, t1, t2;

	if (gfb254_iszero(&p->Z) != 0) {
		printf("%s: invalid: Z = 0\n", msg);
		exit(EXIT_FAILURE);
	}
	gfb254_mul(&t0, &p->X, &p->Z);
	if (gfb254_equals(&t0, &p->T) != 0xFFFFFFFF) {
		printf("%s: invalid: T != X*Z\n", msg);
		exit(EXIT_FAILURE);
	}

	// s^2 + x*s = (x^2 + a*x + b)^2
	// -> S^2 + T*S = (sb*X^2 + a*T + sb*Z^2)^2

	gfb254_square(&t0, &p->S);
	gfb254_mul(&t1, &p->T, &p->S);
	gfb254_add(&t0, &t0, &t1);

	gfb254_square(&t1, &p->X);
	gfb254_mul_sb(&t1, &t1);
	gfb254_mul_u(&t2, &p->T);
	gfb254_add(&t1, &t1, &t2);
	gfb254_square(&t2, &p->Z);
	gfb254_mul_sb(&t2, &t2);
	gfb254_add(&t1, &t1, &t2);
	gfb254_square(&t1, &t1);

	if (gfb254_equals(&t0, &t1) != 0xFFFFFFFF) {
		printf("%s: invalid: curve equation\n", msg);
		exit(EXIT_FAILURE);
	}
}

static void
check_eq_point(const char *msg, const gls254_point *p1, const gls254_point *p2)
{
	check_consistent(msg, p1);
	check_consistent(msg, p2);

	uint8_t buf1[32], buf2[32];
	if (gls254_equals(p1, p2) != 0xFFFFFFFF) {
		printf("%s: not equal\n", msg);
		exit(EXIT_FAILURE);
	}
	gls254_encode(buf1, p1);
	gls254_encode(buf2, p2);
	if (memcmp(buf1, buf2, 32) != 0) {
		printf("%s: distinct encodings\n", msg);
		for (size_t u = 0; u < 32; u ++) {
			printf("%02x", buf1[u]);
		}
		printf("\n");
		for (size_t u = 0; u < 32; u ++) {
			printf("%02x", buf2[u]);
		}
		printf("\n");
		exit(EXIT_FAILURE);
	}
}

static void
point_decode(const char *banner, gls254_point *p, const void *src)
{
	uint8_t buf[32];
	if (gls254_decode(p, src) != 0xFFFFFFFF) {
		printf("%s: decode failed\n", banner);
		exit(EXIT_FAILURE);
	}
	gls254_encode(buf, p);
	if (memcmp(buf, src, 32) != 0) {
		printf("%s: distinct encodings\n", banner);
		for (size_t u = 0; u < 32; u ++) {
			printf("%02x", ((const uint8_t *)src)[u]);
		}
		printf("\n");
		for (size_t u = 0; u < 32; u ++) {
			printf("%02x", buf[u]);
		}
		printf("\n");
		exit(EXIT_FAILURE);
	}
}

static void
check_neq_point(const char *msg, const gls254_point *p1, const gls254_point *p2)
{
	if (gls254_equals(p1, p2) != 0x00000000) {
		printf("%s: not distinct\n", msg);
		exit(EXIT_FAILURE);
	}
}

static void
r127_norm(uint8_t *d, const uint8_t *a)
{
	memmove(d, a, 16);
	unsigned x = d[15] & 0x80;
	d[0] ^= x >> 7;
	d[7] ^= x;
	d[15] ^= x;
}

static void
r127_add(uint8_t *d, const uint8_t *a, const uint8_t *b)
{
	for (int i = 0; i < 16; i ++) {
		d[i] = a[i] ^ b[i];
	}
}

static void
r127_mul(uint8_t *d, const uint8_t *a, const uint8_t *b)
{
	uint8_t t[16], c[16];
	memset(t, 0, 16);
	memcpy(c, b, 16);
	for (int i = 0; i < 128 ; i ++) {
		if (((a[i >> 3] >> (i & 7)) & 1) != 0) {
			r127_add(t, t, c);
		}
		unsigned e = 0;
		for (int j = 0; j < 16; j ++) {
			unsigned w = c[j];
			c[j] = (w << 1) | e;
			e = (w >> 7);
		}
		c[0] ^= e << 1;
		c[8] ^= e;
	}
	memcpy(d, t, 16);
}

static void
r127_checkeq(const char *banner, const gfb127 *a, const uint8_t *r)
{
	uint8_t s[16], t[16];
	gfb127_encode(s, a);
	r127_norm(t, r);
	if (memcmp(s, t, 16) != 0) {
		printf("%s: NOT EQUAL\n", banner);
		printf("a = ");
		for (int i = 15; i >= 0; i --) {
			printf("%02X", s[i]);
		}
		printf("\n");
		printf("r = ");
		for (int i = 15; i >= 0; i --) {
			printf("%02X", t[i]);
		}
		printf("\n");
		exit(EXIT_FAILURE);
	}
}

static void
r127_check_zero(const char *banner, const gfb127 *a)
{
	if (gfb127_iszero(a) != 0xFFFFFFFF) {
		printf("%s: NOT ZERO\n", banner);
		printf("a = %08X:%08X:%08X:%08X\n",
			a->v[3], a->v[2], a->v[1], a->v[0]);
		printf(" -> 0x%08X\n", gfb127_iszero(a));
		exit(EXIT_FAILURE);
	}
}

static void
r127_check_notzero(const char *banner, const gfb127 *a)
{
	if (gfb127_iszero(a) != 0x00000000) {
		printf("%s: ZERO\n", banner);
		printf("a = %08X:%08X:%08X:%08X\n",
			a->v[3], a->v[2], a->v[1], a->v[0]);
		printf(" -> 0x%08X\n", gfb127_iszero(a));
		exit(EXIT_FAILURE);
	}
}

static void
r127_check_equals(const char *banner, const gfb127 *a, const gfb127 *b)
{
	if (gfb127_equals(a, b) != 0xFFFFFFFF) {
		printf("%s: NOT EQUAL\n", banner);
		printf("a = %08X:%08X:%08X:%08X\n",
			a->v[3], a->v[2], a->v[1], a->v[0]);
		printf("b = %08X:%08X:%08X:%08X\n",
			b->v[3], b->v[2], b->v[1], b->v[0]);
		printf(" -> 0x%08X\n", gfb127_equals(a, b));
		exit(EXIT_FAILURE);
	}
}

static void
test_gfb127_ops(const uint8_t *va, const uint8_t *vb)
{
	gfb127 a, b, c, d;
	uint8_t vc[16], vx[16];
	gfb127_decode16_reduce(&a, va);
	gfb127_decode16_reduce(&b, vb);
	r127_checkeq("encode", &a, va);
	r127_checkeq("encode", &b, vb);

	int bz = 1;
	r127_norm(vc, vb);
	for (int i = 0; i < 16; i ++) {
		bz &= (vc[i] == 0);
	}
	if (gfb127_iszero(&b) != -(uint32_t)bz) {
		printf("ERR iszero (bz=%d)\n", bz);
		exit(EXIT_FAILURE);
	}

	gfb127_add(&c, &a, &b);
	r127_add(vc, va, vb);
	r127_checkeq("add", &c, vc);

	gfb127_mul_sb(&c, &a);
	memset(vx, 0, 16);
	vx[0] = 1;
	vx[3] = 8;
	r127_mul(vc, va, vx);
	r127_checkeq("mul_sb", &c, vc);

	gfb127_mul_b(&c, &a);
	memset(vx, 0, 16);
	vx[0] = 1;
	vx[6] = 64;
	r127_mul(vc, va, vx);
	r127_checkeq("mul_b", &c, vc);

	gfb127_mul(&c, &a, &b);
	r127_mul(vc, va, vb);
	r127_checkeq("mul", &c, vc);

	gfb127_square(&c, &a);
	r127_mul(vc, va, va);
	r127_checkeq("square", &c, vc);

	gfb127_xsquare(&c, &a, 0);
	r127_checkeq("xsquare(0)", &c, va);
	gfb127_xsquare(&c, &a, 1);
	r127_checkeq("xsquare(1)", &c, vc);
	gfb127_xsquare(&c, &a, 2);
	r127_mul(vc, vc, vc);
	r127_checkeq("xsquare(2)", &c, vc);
	gfb127_xsquare(&c, &a, 3);
	r127_mul(vc, vc, vc);
	r127_checkeq("xsquare(3)", &c, vc);

	gfb127_div(&c, &a, &b);
	if (bz) {
		r127_check_zero("div[b]", &b);
		r127_check_zero("div[c]", &c);
	} else {
		r127_check_notzero("div[b]", &b);
		gfb127_mul(&d, &c, &b);
		r127_checkeq("div(1)", &d, va);
		r127_check_equals("div(2)", &a, &d);
	}

	gfb127_sqrt(&c, &a);
	gfb127_square(&d, &c);
	r127_checkeq("sqrt", &d, va);

	uint32_t tra = gfb127_trace(&a);
	gfb127_halftrace(&c, &a);
	gfb127_square(&d, &c);
	gfb127_add(&c, &c, &d);
	if (tra != 0) {
		memset(vx, 0, 16);
		vx[0] = 1;
		gfb127_decode16(&d, vx);
		gfb127_add(&c, &c, &d);
	}
	r127_check_equals("halftrace", &a, &c);

	gfb127_normalize(&c, &a);
	r127_checkeq("normalize", &c, va);

	r127_mul(vc, va, vb);
	r127_norm(vc, vc);
	for (int i = 0; i < 127; i ++) {
		uint32_t rc = (vc[i >> 3] >> (i & 7)) & 1;
		gfb127_mul(&c, &a, &b);
		if (gfb127_get_bit(&c, i) != rc) {
			printf("wrong get_bit(%d)\n", i);
			exit(EXIT_FAILURE);
		}
		gfb127_set_bit(&c, i, 0);
		if (gfb127_get_bit(&c, i) != 0) {
			printf("wrong set_bit(%d) (0)\n", i);
			exit(EXIT_FAILURE);
		}
		gfb127_set_bit(&c, i, 1);
		if (gfb127_get_bit(&c, i) != 1) {
			printf("wrong set_bit(%d) (1)\n", i);
			exit(EXIT_FAILURE);
		}
		gfb127_set_bit(&c, i, rc);
		r127_checkeq("set_bit(%d) (rev)\n", &c, vc);
	}
}

static void
test_gfb127(void)
{
	printf("Test gfb127: ");
	fflush(stdout);

	uint8_t va[16], vb[16];

	memset(va, 0, 16);
	memset(vb, 0, 16);
	test_gfb127_ops(va, vb);
	printf(".");
	fflush(stdout);

	va[0] = 0x01;
	va[7] = 0x80;
	va[15] = 0x80;
	test_gfb127_ops(va, vb);
	printf(".");
	fflush(stdout);

	vb[15] = 0x80;
	test_gfb127_ops(va, vb);
	printf(".");
	fflush(stdout);

	memset(va, 0xFF, 16);
	memset(vb, 0xFF, 16);
	test_gfb127_ops(va, vb);
	printf(".");
	fflush(stdout);

	va[15] &= 0x7F;
	vb[15] &= 0x7F;
	test_gfb127_ops(va, vb);
	printf(".");
	fflush(stdout);

	for (int i = 0; i < 300; i ++) {
		uint8_t tmp[32];
		tmp[0] = (uint8_t)i;
		tmp[1] = (uint8_t)(i >> 8);
		blake2s(tmp, sizeof tmp, NULL, 0, tmp, 2);
		test_gfb127_ops(tmp, tmp + 16);
		printf(".");
		fflush(stdout);
	}

	printf(" done.\n");
}

static void
r254_norm(uint8_t *d, const uint8_t *a)
{
	memmove(d, a, 32);
	unsigned x;
	x = d[15] & 0x80;
	d[0] ^= x >> 7;
	d[7] ^= x;
	d[15] ^= x;
	x = d[31] & 0x80;
	d[16] ^= x >> 7;
	d[23] ^= x;
	d[31] ^= x;
}

static void
r254_add(uint8_t *d, const uint8_t *a, const uint8_t *b)
{
	for (int i = 0; i < 32; i ++) {
		d[i] = a[i] ^ b[i];
	}
}

static void
r254_mul(uint8_t *d, const uint8_t *a, const uint8_t *b)
{
	uint8_t a0b0[16], a0b1[16], a1b0[16], a1b1[16];
	r127_mul(a0b0, a, b);
	r127_mul(a0b1, a, b + 16);
	r127_mul(a1b0, a + 16, b);
	r127_mul(a1b1, a + 16, b + 16);
	r127_add(d, a0b0, a1b1);
	r127_add(d + 16, a0b1, a1b0);
	r127_add(d + 16, d + 16, a1b1);
}

static void
r254_checkeq(const char *banner, const gfb254 *a, const uint8_t *r)
{
	uint8_t s[32], t[32];
	gfb254_encode(s, a);
	r254_norm(t, r);
	if (memcmp(s, t, 32) != 0) {
		printf("%s: NOT EQUAL\n", banner);
		printf("a = ");
		for (int i = 15; i >= 0; i --) {
			printf("%02X", s[i]);
		}
		printf(" + u*");
		for (int i = 31; i >= 16; i --) {
			printf("%02X", s[i]);
		}
		printf("\n");
		printf("r = ");
		for (int i = 15; i >= 0; i --) {
			printf("%02X", t[i]);
		}
		printf(" + u*");
		for (int i = 31; i >= 16; i --) {
			printf("%02X", t[i]);
		}
		printf("\n");
		exit(EXIT_FAILURE);
	}
}

static void
r254_check_zero(const char *banner, const gfb254 *a)
{
	if (gfb254_iszero(a) != 0xFFFFFFFF) {
		printf("%s: NOT ZERO\n", banner);
		printf(
			"a = %08X:%08X:%08X:%08X + u*%08X:%08X:%08X:%08X\n",
			a->v[0].v[3], a->v[0].v[2], a->v[0].v[1], a->v[0].v[0],
			a->v[1].v[3], a->v[1].v[2], a->v[1].v[1], a->v[1].v[0]);
		printf(" -> 0x%08X\n", gfb254_iszero(a));
		exit(EXIT_FAILURE);
	}
}

static void
r254_check_notzero(const char *banner, const gfb254 *a)
{
	if (gfb254_iszero(a) != 0x00000000) {
		printf("%s: ZERO\n", banner);
		printf(
			"a = %08X:%08X:%08X:%08X + u*%08X:%08X:%08X:%08X\n",
			a->v[0].v[3], a->v[0].v[2], a->v[0].v[1], a->v[0].v[0],
			a->v[1].v[3], a->v[1].v[2], a->v[1].v[1], a->v[1].v[0]);
		printf(" -> 0x%08X\n", gfb254_iszero(a));
		exit(EXIT_FAILURE);
	}
}

static void
r254_check_equals(const char *banner, const gfb254 *a, const gfb254 *b)
{
	if (gfb254_equals(a, b) != 0xFFFFFFFF) {
		printf("%s: NOT EQUAL\n", banner);
		printf(
			"a = %08X:%08X:%08X:%08X + u*%08X:%08X:%08X:%08X\n",
			a->v[0].v[3], a->v[0].v[2], a->v[0].v[1], a->v[0].v[0],
			a->v[1].v[3], a->v[1].v[2], a->v[1].v[1], a->v[1].v[0]);
		printf(
			"b = %08X:%08X:%08X:%08X + u*%08X:%08X:%08X:%08X\n",
			b->v[0].v[3], b->v[0].v[2], b->v[0].v[1], b->v[0].v[0],
			b->v[1].v[3], b->v[1].v[2], b->v[1].v[1], b->v[1].v[0]);
		printf(" -> 0x%08X\n", gfb254_equals(a, b));
		exit(EXIT_FAILURE);
	}
}

static void
test_gfb254_ops(const uint8_t *va, const uint8_t *vb)
{
	gfb254 a, b, c, d;
	uint8_t vc[32], vx[32];
	gfb254_decode32_reduce(&a, va);
	gfb254_decode32_reduce(&b, vb);
	r254_checkeq("encode", &a, va);
	r254_checkeq("encode", &b, vb);

	int bz = 1;
	r254_norm(vc, vb);
	for (int i = 0; i < 32; i ++) {
		bz &= (vc[i] == 0);
	}

	gfb254_add(&c, &a, &b);
	r254_add(vc, va, vb);
	r254_checkeq("add", &c, vc);

	gfb254_mul_sb(&c, &a);
	memset(vx, 0, 32);
	vx[0] = 1;
	vx[3] = 8;
	r254_mul(vc, va, vx);
	r254_checkeq("mul_sb", &c, vc);

	gfb254_mul_b(&c, &a);
	memset(vx, 0, 32);
	vx[0] = 1;
	vx[6] = 64;
	r254_mul(vc, va, vx);
	r254_checkeq("mul_b", &c, vc);

	gfb254_mul_u(&c, &a);
	memset(vx, 0, 32);
	vx[16] = 1;
	r254_mul(vc, va, vx);
	r254_checkeq("mul_u", &c, vc);

	gfb254_mul_u1(&c, &a);
	memset(vx, 0, 32);
	vx[0] = 1;
	vx[16] = 1;
	r254_mul(vc, va, vx);
	r254_checkeq("mul_u1", &c, vc);

	gfb254_mul(&c, &a, &b);
	r254_mul(vc, va, vb);
	r254_checkeq("mul", &c, vc);

	gfb254_square(&c, &a);
	r254_mul(vc, va, va);
	r254_checkeq("square", &c, vc);

	memset(vx, 0, 32);
	vx[0] = 2;
	gfb254_div_z(&c, &a);
	gfb254_decode32(&d, vx);
	gfb254_mul(&d, &d, &c);
	r254_check_equals("div_z", &d, &a);

	memset(vx, 0, 32);
	vx[0] = 4;
	gfb254_div_z2(&c, &a);
	gfb254_decode32(&d, vx);
	gfb254_mul(&d, &d, &c);
	r254_check_equals("div_z2", &d, &a);

	gfb254_div(&c, &a, &b);
	if (bz) {
		r254_check_zero("div[b]", &b);
		r254_check_zero("div[c]", &c);
	} else {
		r254_check_notzero("div[b]", &b);
		gfb254_mul(&d, &c, &b);
		r254_checkeq("div(1)", &d, va);
		r254_check_equals("div(2)", &a, &d);
	}

	gfb254_sqrt(&c, &a);
	gfb254_square(&d, &c);
	r254_checkeq("sqrt", &d, va);

	uint32_t tra = gfb254_trace(&a);
	gfb254_qsolve(&c, &a);
	gfb254_square(&d, &c);
	gfb254_add(&c, &c, &d);
	if (tra != 0) {
		memset(vx, 0, 32);
		vx[16] = 1;
		gfb254_decode32(&d, vx);
		gfb254_add(&c, &c, &d);
	}
	r254_check_equals("qsolve", &a, &c);
}

static void
test_gfb254(void)
{
	printf("Test gfb254: ");
	fflush(stdout);

	uint8_t va[32], vb[32];

	memset(va, 0, 32);
	memset(vb, 0, 32);
	test_gfb254_ops(va, vb);
	printf(".");
	fflush(stdout);

	va[0] = 0x01;
	va[7] = 0x80;
	va[15] = 0x80;
	va[16] = 0x01;
	va[23] = 0x80;
	va[31] = 0x80;
	test_gfb254_ops(va, vb);
	printf(".");
	fflush(stdout);

	vb[15] = 0x80;
	vb[31] = 0x80;
	test_gfb254_ops(va, vb);
	printf(".");
	fflush(stdout);

	memset(va, 0xFF, 32);
	memset(vb, 0xFF, 32);
	test_gfb254_ops(va, vb);
	printf(".");
	fflush(stdout);

	va[15] &= 0x7F;
	va[31] &= 0x7F;
	vb[15] &= 0x7F;
	vb[31] &= 0x7F;
	test_gfb254_ops(va, vb);
	printf(".");
	fflush(stdout);

	for (int i = 0; i < 300; i ++) {
		uint8_t tmp[64];
		tmp[0] = (uint8_t)i;
		tmp[1] = (uint8_t)(i >> 8);
		tmp[2] = 0;
		blake2s(tmp, 32, NULL, 0, tmp, 3);
		tmp[32] = (uint8_t)i;
		tmp[33] = (uint8_t)(i >> 8);
		tmp[34] = 1;
		blake2s(tmp + 32, 32, NULL, 0, tmp + 32, 3);
		test_gfb254_ops(tmp, tmp + 32);
		printf(".");
		fflush(stdout);
	}

	printf(" done.\n");
}

static const char *KAT_DECODE_OK[] = {
	// These values can be decoded into points. First one is the neutral.
	"0000000000000000000000000000000000000000000000000000000000000000",
	"cbd10bd0365bcd76de1b2418d01a906c61bb948da5f84f1866f62ab301d9870f",
	"6aeb610d4a16d7632c0209704e27c27adafb3825c4a446f7181b219c5a280d36",
	"84bd6a2d2af05abd13433e3a5133245f2e36b5cb9d9861bf4e8cc224a7287b6a",
	"4bfa00756e0b43b2e8424c971c7f930a1f8d62d792d245a82aaffe9b09004273",
	"40acd94753c08aa352824049b87a211a2ffb23ebf05fd2231f5f5153da06591d",
	"11dd6f132cf1c3601628b6998e7c0e2f039ef726b298662e2ec76465fba3cb4e",
	"6f2110c1e88b12e750ca9cb2d7d6b044b2ee5b5c47ec56e3f867f2e486fa8c4a",
	"1c94e2cce8426abc891f4066dab0245349fa07c65665d1deaae287c350644c0a",
	"b479ff50dcdc3c45cae258bcf7685d6d0ec0dd6f267f9cf3211763d8b273dd68",
	"09cccfe1ff69d31c38b328f26bb5b976093d9dd0d65f7921714b26989c97b559",
	"40a7a912e6eec20305569fa56b01e475c2e46f8e3370877c67551424d923fe79",
	"2be7959c9e0b491bf75001f261aa453166abac274f3c43b7e88614f4463bf50e",
	"78fe600050c526e269e0ae75fdea027dbd32d5644ec39dd7cf42887d8c288f29",
	"dd40017bf526e3c8c19b2c34c5528645458c7e7d1db6c6e8e746be53f09c2659",
	"2db49a993475789b2661b944f244412f7b3fc1306664c8e96290f9c457fab724",
	"b3ffaeca7b8ec292983c3de6734a636c1f0742b5a4977d2b77add7a8f61d3810",
	"b581ea3d7de746cd96b29878c2e92d7909c2882c36e698916dd5be27566d8760",
	"8d224a26157643bcd22d8fbc4199af4994f3dd08c41a4708050e605443adf168",
	"4ddbc4e7ce2ff43f19edea4472eb754076b01062e83de82efdeb58224e39c77c",
	"239f1ebb2acf00d3334d4c04df45d558a89837da3ddb48ed3b7bc488266b0d35",
	NULL
};

static const char *KAT_DECODE_BAD[] = {
	// These values cannot be decoded into points.
	"105bf9e33fb81d01d91fa654cc6c3336737769caa64eed272c84ad26a88ece46",
	"7c0abc802bb637d213220093bea30674b600e33a72fe1fc3d32153ec9416ab5a",
	"53bb562569dd42fc9c19ae0f9961e95d50722cc9a2c4842a906a1f360d01db24",
	"3ab2b22de7e879efa3ee5aaebc9ceb11edade9e541938f2f2a84c285e685a131",
	"f34201da0c73d1575562b00bdbcb8221d93e6aef119f7a50986517788a192d7b",
	"51e885251baaa8389ef82ac57fd9b029dda6a0db3a6371d8e76dc6cf36034454",
	"4846d6cbb55e41a11cc70683a8221c4727a9042764add54c977690800c41340e",
	"ca67d13126559b7dc7e34b1e4a1f720ad2749ffab2afccbda708658942e61637",
	"bdb68995508e52fc778a6e14f3939d2506de28e4d07cc13bef265d7f0eca6d73",
	"3e40adf4128a8be4e7bf78c4d6882b4f6ea22035498c4b5c431bbb96396bfc27",
	"eb7502768f86f7e06dc7be9fa6aafa57fae3ad9f3f3bc0694b6b6068e7e7e562",
	"336c9617613efd1316f914e4248e6045a64b900c7dddd571f2c55c3f3aa1ea4b",
	"ac600877fc0c09e4212e234d17a8eb560cc26066b96c73868bbb2d93a2e80917",
	"8a580f479987ff9330f4a4a1b72b217aa08c79c2d4b1020e8ce16075a6a60e46",
	"99b9f8148f894acd53b4c4f4881a130a5670e47a87f49a2db1a0c10d7795af43",
	"29dbe07e0ba63b0542a0b45e07d47d5e0407df4420c14db6f3c8f9a01ab1ce31",
	"56a00a70550e365fc4d23c36d96d7823da084c6ef64f996178795e4d5c25e777",
	"e36a1a362e8c4e6b6629c086defe720250994a4a0a859f8415a01eb206c11648",
	"15c28f1ae6fb7dc3b57ff5f731f2b23e3cf6205ac442f008b49dd352e7ba4346",
	"b203cdd232198a1a3cec2d9270ebc1493e88d5206e2f3b8834ea69d8d7797b29",
	NULL
};

static void
test_gls254_encode_decode(void)
{
	printf("Test GLS254 encode/decode: ");

	for (int i = 0; KAT_DECODE_OK[i] != NULL; i ++) {
		uint8_t buf1[32], buf2[32];
		gls254_point p;
		HEXTOBIN(buf1, KAT_DECODE_OK[i]);
		if (gls254_decode(&p, buf1) != 0xFFFFFFFF) {
			printf("DECODE FAILED\n");
			exit(EXIT_FAILURE);
		}
		if (i == 0) {
			if (gls254_isneutral(&p) != 0xFFFFFFFF) {
				printf("bad isneutral (1)\n");
				exit(EXIT_FAILURE);
			}
		} else {
			if (gls254_isneutral(&p) != 0x00000000) {
				printf("bad isneutral (2)\n");
				exit(EXIT_FAILURE);
			}
		}
		gls254_encode(buf2, &p);
		check_eq_buf("KAT ENCODE 1", buf1, buf2, sizeof buf1);

		// Setting one or both of the "high bits" should make
		// decoding fail.
		buf1[15] |= 0x80;
		if (gls254_decode(&p, buf1) != 0x00000000) {
			printf("DECODE SHOULD HAVE FAILED (1)\n");
			exit(EXIT_FAILURE);
		}
		buf1[31] |= 0x80;
		if (gls254_decode(&p, buf1) != 0x00000000) {
			printf("DECODE SHOULD HAVE FAILED (2)\n");
			exit(EXIT_FAILURE);
		}
		buf1[15] &= 0x7F;
		if (gls254_decode(&p, buf1) != 0x00000000) {
			printf("DECODE SHOULD HAVE FAILED (3)\n");
			exit(EXIT_FAILURE);
		}
		buf1[31] &= 0x7F;
		if (gls254_decode(&p, buf1) != 0xFFFFFFFF) {
			printf("DECODE FAILED (2)\n");
			exit(EXIT_FAILURE);
		}
		gls254_encode(buf2, &p);
		check_eq_buf("KAT ENCODE 2", buf1, buf2, sizeof buf1);

		gls254_point_affine pa, qa;
		uint8_t tmp[64];
		gls254_normalize(&pa, &p);
		gls254_uncompressed_encode(tmp, &pa);
		if (gls254_uncompressed_decode(&qa, tmp) != 0xFFFFFFFF) {
			printf("uncompressed_decode failed\n");
			exit(EXIT_FAILURE);
		}
		if (!gfb254_equals(&pa.scaled_x, &qa.scaled_x)
			|| !gfb254_equals(&pa.scaled_s, &qa.scaled_s))
		{
			printf("bad uncompressed enc/dec\n");
			exit(EXIT_FAILURE);
		}
		tmp[5] ^= 0x04;
		if (gls254_uncompressed_decode(&qa, tmp) != 0x00000000) {
			printf("uncompressed_decode should have failed\n");
			exit(EXIT_FAILURE);
		}

		printf(".");
		fflush(stdout);
	}

	printf(" ");
	fflush(stdout);

	for (int i = 0; KAT_DECODE_BAD[i] != NULL; i ++) {
		uint8_t buf1[32], buf2[32];
		gls254_point p;
		HEXTOBIN(buf1, KAT_DECODE_BAD[i]);
		if (gls254_decode(&p, buf1) != 0x00000000) {
			printf("DECODE SHOULD HAVE FAILED\n");
			exit(EXIT_FAILURE);
		}
		if (gls254_isneutral(&p) != 0xFFFFFFFF) {
			printf("bad isneutral (3)\n");
			exit(EXIT_FAILURE);
		}
		gls254_encode(buf2, &p);
		memset(buf1, 0, sizeof buf1);
		check_eq_buf("KAT ENCODE 3", buf1, buf2, sizeof buf1);
		printf(".");
		fflush(stdout);
	}

	printf(" done.\n");
}

static const char *KAT_ADD[] = {
        // Each group of 6 values is encodings of points:
        // P1, P2, P1+P2, 2*P1, 2*P1+P2, 2*(P1+P2)
        // (points randomly generated with Sage)
	"94d5f4bae9121a19c57110b50ab85a45d86768c170fa0f898b0e4514fbbadc07",
	"4bc0a4701f3bd5647e737eb229e55a4ad6617fe853f6df0760682ca26ed5fb5f",
	"0bc6c0f1ea55ce2d58ef439018bb3f3b87e7e469841eeb73ce62e2d4d707d253",
	"73ae6e40fb9b6157048e54c94bbdb764c07c84a0fdf6dd93c25c940161340c67",
	"a933ad92f379c3cf47fdfdc6623a875ce1225223e40de2448e9cfe3c520a8d1c",
	"1e19f4d5341e736f1701f150e750ad2a4a4abc6459bdbefe2b16ed6beb5a9035",

	"70dafd65c8d22e882cdd2c8605836219d9ed37b86d0fd4003b05a92a89407018",
	"da4587d68b0bd5be2a0f41fd05bcf61eb51f0124b158ca01a8ff2005e543b56f",
	"30a544ca2ce27c50438871e0556c604194160bb66ab75af7fbe6dd584bcd530b",
	"9015220008bf9c1c4729ea667e49d5240189db0ad548d868a68db0a87988800e",
	"8791605ac3d93aad52bd13674769e97ea8217b1b3275f6437c74cbd3edd5e87b",
	"603af6a2a915d9bad1e7bb36c659e82c0e80f437b1a4e959221749abd80c0e79",

	"a57bb7cb871acf1bcb8d9f60382e0c612e79c2e5297cf7b4e4f03b1e37ab2c40",
	"32a5c7c7b38120d2a1604694b733e9510c1e50db41e6237debde0ccaf9cc6368",
	"56bb66baff5fb145d84ce65eef1eab25607453048e2f03f610391796c8b48328",
	"b402c4fa79de650b10f0c1aca9e96048b91b19fe3d12a556f587900332f7b767",
	"0fb56df72c8f0746bcd0b4bd9b2a4e79b9a18373fb91d27b23b832094590ba03",
	"9c86cd3e7dc906278d25ec8944f75d116e1df56fa25f41b74c8f0837f3c5b428",

	"d585c4caeb5ad5ee65c95516631b7e0a3d3dd3aff2b5d2e7f408573f5ad00a28",
	"d4363cd4c92f46e27c3ecda7e0d3ab600a1f1dbf7a76a9c590ae551a72a73d71",
	"d16d27659ad81fbe5493633d4def6d224d91d02648a8de724c6c284338acf50a",
	"7e9d24c4dcad65f94952db0775eb3c31cf0277b60b08547e43fbadb92c587106",
	"4aea15b4ac9a397bf656ea78c69ede3b5c35244bc1b7dde064707b96102b1574",
	"0cc02b81a15368d854b3c7cc9345bc39dafe628645d1f4daddbebac1fa79c51a",

	"32bdb70caf703cbcdf35eb8e890ed61a2ec5f43cd1ed0827c773b64be0a92c7c",
	"f91c194f2053d8ad90c93eb445ed6617ee9ee6bc1106a678c9038d1947811f1e",
	"35d928b773cb2f19d91d23b975b1041513ce88147a89496605d75578a6434e59",
	"ac971551707a68c0685213277c4386567c78f9540f816f445637ba6adce66764",
	"54b00f6e8086fdf6378240201e7b244e22d4aea6f718f80836fb41003613883a",
	"8c9708fe406ee39d411804ee15a43c3c02ac5b9b5351c9a7ad3b680026f4ac52",

	"bfb6790f2a4eb3aa2227ab334d97bc578a05da8b8c87887bb86bac028719222b",
	"3fa757ae29e4d7f4e8fd370584af305107c088117aa17cd71bee36fe32617f33",
	"39579fe6d10d6ec82922d9c16ffedf08f5e77c9224a4cadc44500b88f167687b",
	"0035eeb77eb648ba8443ebe3e334df42226303b9f0eb4120ebbba2ce0e862f09",
	"cff2405b060fd293c6c0291caf7bff0c6e966853ba8896be831ad73b7117907c",
	"bb3545afeb4566a3e9f7dd469e6f58119fbc1eee85dbe1f2fa50cba3df9ebf20",

	"2bf8dd630dc1954a33453439fff6f75bec34ccfdb3d5e54b2b4a0aef6072b22b",
	"7f3cf960972df858e1eed009a810e04528f60e304d1b7509a2f59ff0e832bf25",
	"261c17ecd98cb6c39d8a945ff93bde0d2cd2912c35f8454f469cfa6ace65e944",
	"826186473bca039c92aebdfa2e30a32f0acfc7318e24aa4e8cd4b325ebf7b772",
	"3553dae3dd2c8431f7cc448ff78fda3902225926c66fc4525e35f8f136510c78",
	"2d57fd866f7711a3b29ddfc2cc419a571fc65f25578ccb8550ae793e7ded9727",

	"f5526cf94b6360e394a73da959c5da5f8e927ac0476d3fd5a9d263e71fa4691c",
	"7993d8e7edd72b4af3e70ab3e429341d8a001ae4cafd897263bca446b0eb8470",
	"e63d4ef78e84128c099c886e6b151d3657f1f81d1be9398448d07be84e214c3f",
	"4d47da4f69ac711afe1b6d3d8dfcbd129d8d059f3fe5fb398069bc436b146127",
	"2c5e5d9be466c2c6495e5cb7b479315eda990b59ffa1b65edf3b76ff43a7a937",
	"b0616ef58cfa4e0eb5c87e56e0a95123c02ca9686863cae0c65368099c0a1e11",

	"b963b2b3898944a87fbf9bfee3dab20f93157f4977ef51eec2268da43827013d",
	"9486bf8c4731e85e1b05ed802d505d08f172ed797e752327bc1c9a74ba179d40",
	"5cde0c1e3c74287b582d228c77274e26ee42c538cbd3bf6065ff35f7f879c222",
	"91dfd1b855ea5f0f5dc5cfeba713d16d4b9275d721d76bfaf72c3b9017297076",
	"d5ce2884f64694e8743e2109b3a9f76f08935b036ad1bad95fa09de5dcfce528",
	"ebdec8fb0615167d3442b267f77d0a3a570faad06c3d0e33bd625f9a1e08bf73",

	"a934a7f2a91218dd50ed68c2a23f512bec9ba9c91b52e0a5eb2dafda65f02708",
	"38c8ddc6f673690c7b648b8de6e0be371ff874cacc598cb3f54b3b69cecaf938",
	"5f6a261d448d01e5becfe180bc4b953d0bbfd0952e266e67833ab6de6c13a568",
	"5a173b80cd44291b91c92d2429f4a5549b3d2bd3852164f4ddceaa9288667559",
	"e47f811f3efd31972d820f51fefff47c9c755c8a60f4e2910b712d19a1ee0d62",
	"436ceea7a2673b4709c567d60535f4185ad10efaafaa48580c6e45ba5eb9cb6f",

	"e02ae0dd198ad9ba5a05d1af8888991fcbe1c6ef8ed9236edb50e494d894dd54",
	"f9d68bfc6e2bea9d80a656efeb897d68724033a1f27de4a50d85de69acfc2a59",
	"d2cf5623d0330576aa104a0e9ae0215fef169d6f84efddcfc824ad62749d9c1d",
	"22322eb119221f4b55cef3a375185b365710ae01884415d50cf80e2438361332",
	"5a6fca50f85ccd987b264b68f45ac8008438caf4e5edb531561a1f9674f44308",
	"f79473d5352c991f53bcf6e3ffe13d60d01ed506b40eb81aac32316d2e632331",

	"1a953b6a305b08f979c95f2767a99e50ef6ba4a729fa46c2a9c35951210c2216",
	"40586276a0a27de3231abf85685ddc256940bb31e47ec894d3d1a32033e00337",
	"b1e5cfa28033947c5a8ee389ed020f1f2a5f0532650a7c73e5a4ee878f8f6335",
	"e5c221a103894510687c492ce015510ab43c93dc8592fbccd748e6af724aee7f",
	"96db506fbb440f9ffd3a1ee8e7cba008759f815f1746f25dc379de79e486e802",
	"acb7e61f28f6a214a634b5eb7c0e313e69c57f70cc1eb6fe5a44c910fef6f80a",

	"e4a0c0347d53036b9b24d1c1ad508233362e365ea8643eb73fbba29d2965d658",
	"41a140d1fccc86cf0013c01d58cba02efab5bc618eaaade9003c69fc06605447",
	"075da096356d298348d54c466e2427561f8fc01621678e239fcf66027a55053e",
	"03f72d0d67f65555a34d6d3fffe6d51c34744778ab642017828959ae4e70221b",
	"4dc518d9990aba63584604a85aa6005f31fbb50a01f763da5f4c3016b5f3f467",
	"a378cdb44a171b07b838195548c3e05e8cdc376579037b0923bb1986bd06c065",

	"4161b36bbced2460bdd8298450d28e48b293e5abf4993458057b82fb3530c20a",
	"52e194460cf20f62e0979e69ded92d0bab87b2060d7cd0b23ce23afac608283e",
	"0417448e0011bdabed512a517d8c731d13b0c1b623e437c68db785fffc0a7865",
	"64d600a2dc1e577e628d8eb0db522f32abacddea209d7efc13f44f9825ac8674",
	"0a7abe492f6de7d98cb1cfd5b7e1fe63509f3fddeee4e0ab11f4fa2036787602",
	"e4af7e308f64032e1710e0c29f9c7d38a8f99b44176f6a4a5b708fb219763836",

	"1a4b218e2abbbe73a7d1d16970b4e62244a7a8f33ce0dc8a136a2ef070129953",
	"230ed9c76a65e0c321c7a34ba867ea754a4315823257f3e740e69f976aa02b3f",
	"c8872f2ee158bf466b121ed7df3c4f12a1c37ab43f56104af5745ba57cd8e278",
	"5e2da8ac31308839d156643b1cd14c15c9c8334eff2a99a0cc01c78dd0f84841",
	"6f0cfea5c7f4d8fe97551454fd1f3018b611d393a49f800024cb64332378864e",
	"b8caa845e0f0cdde18426a8015980e30ce9a745b458db34804929b8d1a565111",

	"f65b443c06216f94f292db2e52ecf05f61814d9c674df7d6022ebded87d0d47b",
	"07d8dfc8e5d6c0ecdcc4ad391fcf4f7b8cf95f84c7a087953f42843397a47a0a",
	"e31456234385822032872a78a18c4a552d277a118048449e3500b3502a9db125",
	"2dc3026ef9108702b8fdbda142e9531ea2ce60a79e718192787d77a3364b3520",
	"60f4c1f37620b0ab209415804f636616d7b9161a0ec1c08b7b9b32615724f511",
	"88e678d3e77794da66012ab8279858170504cfe56ca102b461cab1801cadc45c",

	"ff12c92d545b154bb01b97f73e3c9842a3459f6060d397325dcb74495b0a7050",
	"6ab785216b20309fa95521bc68632273c4c2ae1ff2d23f385441b468f830cb4d",
	"89339bfcbcc9bbc895cbedad3427e75755fe975e65f810a5c6af975a8062c622",
	"b7c5b06ed3cef8c54c0edf36b0bd48022e9ab2e2aa7a1712602bf0be5c487b6c",
	"afa79291112d4c3284487166773ca747fa019841570f852e48258c109b905911",
	"d860c7a2834f3bff2981fbdc3589e81484a6382754d8952a53d1aa074c10682c",

	"6ffd71b6080088a1ac7ab66b2d60dd74fe559956e8a00310052e3bb0aaeaad0c",
	"e81ffaac4cf15066633b34daef1c5e433636fb8c0524025cdb310d2ccec80453",
	"e5ba686596aa3e1d9a5774b5df193c19c20dd7f70525b122ab464c9a67e88f5b",
	"cfeba433d736ea12a2ad777ec899fc262fa7fcefb9b2393e7aeebc334582fd78",
	"de30dc12a96279ed8a9685de2d289424147995c0b0f5214c41f8a6818504152e",
	"2e705cc87d66348e31db3ca299851d03f5e7a0058e5597b1b41b8112c26eed6b",

	"950a5fce5f917cf3f32300275d814235cda77ad91c1b6b940bbab6beaa901603",
	"19063ca0f6e050c2c82f0597f5e2cf5bc19e50e433f316dca8af4c23dba07707",
	"68c03e3c7bde8fd0f7a6b5e0919ec66139a1b375c0b078c057205a53102e5227",
	"24ae96b1446c349926e72e1514fca568870a3cdefa562b642e81123293199972",
	"da50c6993bf4b0d38ec90d6567905b081897a205938e6aeb054a115c7bc2af59",
	"4bac02328418fff1a9600c925de437305c58906b62a43c6c3d917ce93fcb6519",

	"5664397fc93ce68a751dd06a0276345cd7786293b96efa76512fa990e188655f",
	"48377bca5a35e3be8e97bce252da010396f5986e1eaea3fcba68fec527d2a025",
	"716d35f7e38791c9123310a739ba857ba44d22562dd70ede859b6991f7034747",
	"7bb0bf14fd50c7e31d5f6e3b11fb41610404ac7450c68675a6a3db59f517e57f",
	"36e644fe707eabbfbf6f2bd8b6367d7b87361b7eaec183746ce4dbd86d17dd73",
	"1463f9cb32eda42898a02f0141ec5c5d1a000011166192f9498415ead3a43e43",

	NULL
};

static void
test_gls254_ops(void)
{
	printf("Test GLS254 operations: ");
	fflush(stdout);

	for (int i = 0; KAT_ADD[i] != NULL; i += 6) {
		uint8_t buf1[32], buf2[32], buf3[32];
		uint8_t buf4[32], buf5[32], buf6[32];
		HEXTOBIN(buf1, KAT_ADD[i + 0]);
		HEXTOBIN(buf2, KAT_ADD[i + 1]);
		HEXTOBIN(buf3, KAT_ADD[i + 2]);
		HEXTOBIN(buf4, KAT_ADD[i + 3]);
		HEXTOBIN(buf5, KAT_ADD[i + 4]);
		HEXTOBIN(buf6, KAT_ADD[i + 5]);
		gls254_point P1, P2, P3, P4, P5, P6;
		point_decode("P1", &P1, buf1);
		point_decode("P2", &P2, buf2);
		point_decode("P3", &P3, buf3);
		point_decode("P4", &P4, buf4);
		point_decode("P5", &P5, buf5);
		point_decode("P6", &P6, buf6);
		check_eq_point("P1", &P1, &P1);
		check_eq_point("P2", &P2, &P2);
		check_eq_point("P3", &P3, &P3);
		check_eq_point("P4", &P4, &P4);
		check_eq_point("P5", &P5, &P5);
		check_eq_point("P6", &P6, &P6);
		check_neq_point("P1/P2", &P1, &P2);
		check_neq_point("P1/P3", &P1, &P3);
		check_neq_point("P1/P4", &P1, &P4);
		check_neq_point("P1/P5", &P1, &P5);
		check_neq_point("P1/P6", &P1, &P6);

		gls254_point Q3, Q4, R4, Q5, R5, S5, Q6, R6, S6, T6;
		gls254_add(&Q3, &P1, &P2);
		check_eq_point("op(1)", &Q3, &P3);
		gls254_double(&Q4, &P1);
		check_eq_point("op(2)", &Q4, &P4);
		gls254_add(&R4, &P1, &P1);
		check_eq_point("op(3)", &R4, &P4);
		check_eq_point("op(4)", &R4, &Q4);
		gls254_add(&Q5, &P4, &P2);
		check_eq_point("op(5)", &Q5, &P5);
		gls254_add(&R5, &Q4, &P2);
		check_eq_point("op(6)", &R5, &P5);
		check_eq_point("op(7)", &R5, &Q5);
		gls254_add(&S5, &P1, &Q3);
		check_eq_point("op(8)", &S5, &P5);
		check_eq_point("op(9)", &S5, &Q5);
		check_eq_point("op(10)", &S5, &R5);
		gls254_double(&Q6, &Q3);
		check_eq_point("op(11)", &Q6, &P6);
		gls254_double(&R6, &P2);
		gls254_add(&R6, &Q4, &R6);
		check_eq_point("op(12)", &R6, &P6);
		check_eq_point("op(13)", &R6, &Q6);
		gls254_add(&S6, &R5, &P2);
		check_eq_point("op(14)", &S6, &P6);
		check_eq_point("op(15)", &S6, &Q6);
		check_eq_point("op(16)", &S6, &R6);
		gls254_sub(&T6, &S6, &R5);
		check_eq_point("op(17)", &T6, &P2);

		gls254_point V1, V2;
		gls254_point_affine pa;
		gls254_normalize(&pa, &S5);
		gls254_add(&V1, &T6, &S5);
		gls254_add_affine(&V2, &T6, &pa);
		check_eq_point("op(18)", &V1, &V2);

		gls254_point T = Q6;
		for (int j = 0; j < 10; j ++) {
			gls254_point S;
			gls254_xdouble(&S, &R6, j);
			check_eq_point("xdbl", &T, &S);
			gls254_add(&T, &T, &T);
		}

		gls254_condneg(&T, &R5, 0);
		check_eq_point("condneg(0)", &P5, &R5);
		gls254_point U;
		gls254_neg(&U, &P6);
		gls254_condneg(&T, &R6, 0xFFFFFFFF);
		check_eq_point("condneg(-1)", &T, &U);

		printf(".");
		fflush(stdout);
	}

	printf(" done.\n");
}

static const char *KAT_MAP_TO_CURVE[] = {
	// Each group of two values is:
	//   32-byte input
	//   map_to_point output (point, encoded over 32 bytes)
	"1fd6a4420e7bbdf24133096e1528fe727a9f57c9074b61a8af111f241d6f166a",
	"7e98d209635a08b896104923cc224f66442d488585b61f88b4db12be62705644",

	"033f18d29f561043f6dd852af914815d14112d278d0b97150ce81c922f0eec47",
	"a8230361227b63e341ad3e02ffc10f6d3bbedba3234efd159a3670c79725c520",

	"b388dc2f0f84fd925a0e276610d7bc3d0e0c0429e365970f7c0fec67d4cfa230",
	"b0ea5be5ea714172538aaa3e4e04da1d1693947e2e932dc670d0cc9bf4f6e960",

	"0bb58d35373a061187664f39b3713816454a0af5cf3e1e81b0426771e8c2cd17",
	"951d4e43e8ca3e0e768f6baf44a7703afe25d81856c9b9f988d77d640ceaf067",

	"1432af36d4423cfeadc73bf9e6e7936356fe12af86531fa60478f8ccf33a3b0f",
	"d43b93ab0835773c462adb334c0287062bef060ce754a3fdf86da607370f6729",

	"46b1149877793e714f2da1c7c6ee433c8787e517881786615824ea69c8b5686d",
	"7bba003b2d9a085c57353e9f90b820084f4939287f89a8f5b1df3732e72d1c39",

	"4c9b6166e0a376ef53df4bcee675152a7394fdcc3dbaf71b272a8932b14c5868",
	"537550590f57fe81d54d1bc63b7938023b62372da43ff9f8e78e6c83f00d4a03",

	"1e15e17915bcec56257a18a8278d6218031ef41c546ff9b6b052f3d6f7543706",
	"1fd3f7b89cdb854ce3b692081beb2b64bfa13721cb6cb6770e2e49dbb504ae6d",

	"d1c4c82236756e448df547a0fd0bff200b99d02bcdc57de59c260a7c24b5145b",
	"611cb59a05a96556565c5bb0195bd42866203931c53dfb9dd2d5fe9eb76c3e3f",

	"3e35e31cc1367739dcbf17bec6e4a374e92b7f202aa5d281347541084b0bdb4c",
	"bd40634f54f679361ccd04373f800c489164a820e8a874881e0ed0da64640435",

	"4d5ecf0ab094a2238dfef8a0edeaa037bd0d01ad9d679ff3da19a381e7815409",
	"476d54469249b6cc406ff2b8620fb409b27e616a7b4fa77b80f76bbdb81e9940",

	"2a308772b05e5f486d6a80c3c0f3e56727fe18e070343f22e42f426c34cfdf57",
	"4119a979fbaf24fa16e1134109dbae4aa9c884fdd4a80b77b5746cdd968aff63",

	"8b6cdd3efd76711c9257900ed85da0061dc02f720f798f72c4cb0d5ab6c62a26",
	"e12ee72b132f406105e76ff67e40f16781690fecc8a0dad4b455158486ceb416",

	"0d2481388c05ddfdb233e856e8a3e5662dac01599f23e11da629dc0eb96c5d78",
	"0966447eb06aafda6cb5ffa7c5ee6d55e8afe8b7be78e124106282bc6bddd12f",

	"0c95a69895da0dfa4924ac633fe3651e91d5cc67384a460417941aaf8c634f15",
	"29bd953b3a5095e7d33b54947af1303e9b738b1da00677f39ea37db939f01e10",

	"ef45b300c5a3d1a71db2b4dd989587656f7329dabbb6a9d2a5c83e35e4c2de6f",
	"6b454fbaa0bad3e01b9470c5c7beea3175ed80f28ad9a75ee0bdcfeb62015e76",

	"6f782d135fb90d15f9d304280ad64063ea69b5bbe876afdd7d170dab3227de40",
	"aa6e28c982fd988fd97cd49d584b3a7726f909a4d04552eda5286abf5a349219",

	"174da2cf649a6e36dab18c80304ab66eef81f1410a51a35d6f2371d2b343dc4e",
	"4fb3d4d5d78eddf2dcf24bc6e6a85c2d261913d8906276fa4f395b35c9508b59",

	"280cd6ad59a47eaf0e20ce2026d28e18f61f8a0e0e058451870d5ab0811e9209",
	"14070e7b73e38aa61132118eed7c746d6b9b30b48a78b811476efdf4882fdc36",

	"3a6dab01abb780f9b4072ea7179fb105d03f4c8ca24baca11de4552c64ddb21c",
	"103e4b895750d4ac5d1bd300d71591052daec65c4b5d1f315d30cbf7dd01434d",

	"f0d4894b6c295b2e5a0f686e905d870587edcf4af93f2626132cf5919f9b906c",
	"1dba1ba0721332105dd82bb6f46e262899becec26f0b365223fe125fbdf6511d",

	"3e5d9fe52d9eca109fe9cd93356d8a7f5ce094643827bc466bd1e6f023cc6c0a",
	"04e78ea15d6be00e6f79c32a5dc7917ceb5a5037b199c893541e2aabb09a153c",

	"d19f3e961e943031f7ef78de827cd81c9647763aca9c591a3ef33bd3ba3ff213",
	"14725de901c513bab21359874ec0573edabf350affafb34c0e47a748c855931e",

	"3fc852735b02b49cc818768a938c335c3f9c6c7687b7c00c5072b91b46bde47f",
	"af253cc999dbdd25280a672de33d6f05f41d0aa1a6812a34ab572ffa72f1b520",

	"836b7ddbdf083910e787ae21412d931d4ad6c4371963945bee3960373b5b567d",
	"a6031d1a6ae02089752972beda74e7018810b57d5bc7dc71082248fe7cd6912d",

	"91cb535f9d1801c11bb6cba4f8ca4704b1f4b319564fa492f6715d24b7b3d510",
	"add8f2d0d696e48eeb1a88e8084f554562c9d732ad79e0fc11e5b319b8a3cc60",

	"d13634dfc5071664c57ad88b99587e55bd5d10dd9617c1237fa0d0fdbd326d45",
	"8d81f19cf45575a02080d2b2e7a41d4e9a97eb72cc0858f42f23c1697959cf78",

	"709e769413668c944e922308bbb3ee328a1b3dd3714fb45436fa4d49245c845a",
	"128965f29455b2ac8334e41f0545987b516683fadbef9211829558ee42b28d5e",

	"ed08481212de7c21f46c6e6bb403165ed5f6b6040330441fe809e30e3c92b241",
	"c52b8b2f456e0a053d0505db8230c51474b9083f8006520133b5edb788b7bd77",

	"1de84b2b45e1ff0f6b2baacc833f6d5f37fc717f50569166a48225cfa427cf5e",
	"4f3150b8df513df81be786cd2fc27b48bc32e88a8639eef961672771d217d34b",

	"76a9329fd3b6bad8f95fd998c1741c0a7dbf3b6d6147994407d65d69cfa0ba59",
	"2b9cf6f01ac512fd8ff284b4c2698b4bd3cc41f7bc6888ab495cc31027424d59",

	"803b0bd34b259edfd95938a43f6cda58ebb1ab6c5c333b218c6ce3117cae795a",
	"3b11f3f4cb26a54e63e1ac54bb230d4aaf67c3a3694cf96ebe3f929823e5e250",

	"06dc547b7dccf1aee12673a8b6339421df26c88e0e640fcdb5751949b159d73d",
	"0bcc440754312b0334b89b7380bb4027e0e57f807faa11ba63d05bb8df9e0338",

	"94ff0b92dddb730f1d472de6a285595eaca1c9fa155498eacb45f8fed9f0d074",
	"dc2ed058d58ec68f46e080ffef08c868f9b6dbc91acf26c4561823ec293e8573",

	"f2fbe53749eff134e6edcdeda3f55b5d8e7a94302934074c49234dd529559b4c",
	"7a1c06704d6e66034959894e40559e5d61724f1c348b6fa549df90170428f15d",

	"638280f5fc76063e36089b6443e83969279201d65cb781b9ce6ccc4bbaf37d6c",
	"8b8276287d923d9b11134abcecb9b6474710d4938aa0e3927dca9cb31c25df17",

	"fd460d3a94a1caac733f35f6b6e2c31e36ab3bf3a92e7fe26caad70166cef260",
	"c09a92269daa9de74786543d4fbbb909ec89624558d6e76b9741de24b8f0e361",

	"a273ab53ea0ca2979bbb1854ff8c1362166b7118e152d126953c4976316ae706",
	"240a36220b86b9f2fe27bf678160db0f4df5707237503dc64aaf3ed3a1919a1f",

	"9ea17dff46e618beba058f397124ec1f65ed76562a9f06c875b6e0236a9bce63",
	"790e38bc001787a9e9f04e79bfa8021c07c646fa1f97094c93f60e2dfd93b670",

	"b261e5a4ecee50b861e864c68f94db34295490e22594214d914ba904abc29966",
	"05a59ca022c433fc91368f98038a11004496bd817adc968dcafa54c7a673ab36",

	NULL
};

static const char *KAT_HASH1[] = {
        // For i = 0 to 99, hash-to-curve using as data the first i bytes
        // of the sequence 00 01 02 03 .. 62 (raw data, no hash function)
	"6af795c7563d68eaad7eaee938e70e4664b4f4cb90359ca814fa8a46bda5fe4d",
	"ae842d9baaf34c55a5430409de01a4135c5c019670d1a283d7c2d8e3bc2b0351",
	"a1ba722ccc3ffb533446b7c1347b380e3e2ded837fdd200d1e0b9b4769751326",
	"6e4c78646686f73394ea5f4e82e2a86496ded5909c5b53a8ef8507d036f01826",
	"4b57080067b53784b33fd442ca7c2164a6f54e0d31e53314b112ebe641c77c5f",
	"780ceee9bf7df21db01371ef5e20a9658dfa6137769bc30cb4598fd30718d218",
	"dbb6969d16e809f06754d0167de9402f9ce34e3634af0b4ca24036ff582a3d3a",
	"7d60c9504d87920a527301ece992111d095f39b63a21a13521449596a3eeed5e",
	"d829733616b83850101d74388d385f34ba928dbe904c72d42c3d293fa36aa83d",
	"ad95cbab565a826391abc7317a67146d3b09e6edb46b486fa6e77a5fbce6e077",
	"bce87c2ca561a19deb9ca83af3ccae6b832b13a31b6a10049cf263fbd12f150a",
	"50e2b93801bef67616bd9971bfe6110eb5ba9299d02e19633a6c1da45aad090a",
	"4ca997b43491dd2954987f6d075cda1fd384ae24fc0980e98afd3bf90c107e5b",
	"12da8fdb3fc09696619d422f1e607723dccbc2d5d3104a768343003e1530df36",
	"e5a82a42a03e65e66f534440d0a894539ef9b80e8c0141ba920889c176056f6f",
	"8e35cb5b17a579a3959a4a6a3297034c30d1805e8554f30507b4b98802703311",
	"b10c604f6076c70cac209ef309978e5296ff701ab43ebd4db74ead091788b821",
	"5aca4aec264cc298ababa55671b1a51c00ab232f7c86f2a1c8eb1be9d3a1a933",
	"163172d96e57e3b1bc72f7387d8af659f3f031135a3afdbfd53a69d3913a054c",
	"5a4c9c4ce7af71d87647696ff1076933782ba95ded63407503ff0f6747337c19",
	"a441e512c81d31b4de44549cc282571cc4c648065fbd0def621a338363b2591b",
	"386bf9b14f5be9d11b62a0deaa5b0c0ac6adbb3426bd7de7c8795255c62bce68",
	"59c1c323353a7045b8bd5063251c4c7ad5074c8e72b7e02f520ed612a5d73e61",
	"eb7bbd64e37c7868345783e99c188d65344b491c57d65afee6f05f31da0a7a63",
	"bac5d33dc34f732a07bc1e64d7dda95096ae8171f5e1122630f327cef8cfc22e",
	"026fd98c04a3791211c12fc4959b1b2e0be878334ef5bb719363ab3a9144f36b",
	"b269ca4a71f2dbec6a34733d30de260db0debc0406c3ee8a29ca815fefd79f63",
	"fd65af02ac1358f2892dea8c29b6427d4a316774a31eed9733f86ff35813f409",
	"486fd0ad00fac25fe247851221a2865bfa124991c25551b9c48807df9a7cb53a",
	"58e45d524f40abf97785c8cc91dac66fad4799aeb76fa1c33bb288a53003c602",
	"47fb4ef3fa4632da4f849993ba4853579784ac9871ee0052b0f30cae26a2f003",
	"2dcc4d1e7d1bcd78eecda3b7ee39d74769182fdb6f06781de52086086ed97a1b",
	"778a77bb965f4777ada7f159c7555b2b541d1a54c062c8f3eca43e85d0a80203",
	"205203b5ac6df5590a33e6c232e60d50950d0fc85cd05c242d0e43c97b1aa020",
	"f8bd428335eaa272451f4ebc447fdc6d458c19a42716125302b32fbae23f2b49",
	"82f8458bcb0df058351b282ed284504708722572f3b85d7b9db313a87514f321",
	"cb403daa8d7e6b0e96687c0363ab2513add942b3b2bac07a0d76752d02cedb54",
	"de180f7d5ef5ce4b366b64be216d7c7fa4fb25e6f77b87bbc5856421b9e05859",
	"1fbd7df96ed85911f3198caeeaf546117a36b08e3961b3dd0406012326384b10",
	"b75025058a8b123542b7145fd7230e328e6b70becdd2a4ff61d97f1bb0ddd07a",
	"1d114823a1e9cc5866b13e594052e34914882bbc0bd3ec78f1943d3bfe62ad38",
	"6748a0728604c97b5f2b9bdcf4173b2e4b3a997d7c5763292da6ae10d230772d",
	"042c01ab08e74200d9bd273c1d221169bfbcdac9afa5b453df212602a24a215e",
	"100950ae92efa862dae749971fc5b644c2173f6210cc3c08aea20c5ba151fd06",
	"9a57ba8a59fa9de45c414796887c54078d5d0f56a1a2e7d75cbd533db691de4c",
	"09183158574310c489df842ba6a3c71c07386c3e5bc98d664b2a9cbcd69b4b32",
	"b74eb45ded2684c8bf42210f05fde01d11a61b72f7ac33f7a58a6b1cad47291f",
	"70394ef4ee5c257b3b1696de2c1e151c63584ed79e2dd95549d07151771b874b",
	"e61588f80fed7338d56bff60b0145826d01acef5597483bb18c9f3a1cdec295b",
	"2ef74bb77ac7152c2006a2f58f73be553049320d69d294ed42aa4f8e21704f7e",
	"95088b65d3652adb2556baa4c94dbf6fcb5ec08920c97e43b9277abc3f376067",
	"02a5c4f3f4c13092920a53ce38605f706f66cf4e062a475798c71f28f880ac55",
	"0869ffe7893e62eb4882d6b37e15c61e9f259d127b5cec40a962c33172c1ec0d",
	"ae619274e8378942854540f92d361e114d7ad619211cf9a5015ef4b005746465",
	"8986c350152febe95656f8096d5a917c9193010920f568154d8ae8b9ae516775",
	"ab670c4fafcd2e61b30d914ec83702663b2b5d07281dd4fa37540ba0ce8d7f59",
	"093eb91eaf5c8aba2a1710f53fe998125498d015cf3c1324a68c1fdf635f0863",
	"1aec392791a31d6c06ce6f4b5333436c53a8faf10658580de8e179faf3762133",
	"31a5c67b86c55f7038c57f848407624c4364d816eb991bdfca1dea4eeacf5200",
	"ac87a37a2f0d6b151c987ca673b9dd792a8c3b3acaf78790dd35ed62a6823d00",
	"03ecc758606cac9526dc52b9d444a467ae2dd47f5f65bd6d825738bc1db7c34a",
	"bbfd4e41526acb5804f66131936772428a72d2151fc11b361f1e71cc0a4be75d",
	"5853684a2056cb289e4621dd201c5c0cac05ef23228791989da581827a9db266",
	"9f1e98fcb3cd67e4d0166fdc42585b68836305ce612ab00110293fc0ee3aa34a",
	"8b36e7926d73751bcad84e96c05d1445f3b1fce7ebde5bdcad63e1b11ddf4705",
	"8fd0fc5fc708d6e6ee23c98bc3bb7e30dc9ee047fdf9bb3ef7d18541f617f771",
	"1a25afc2eb78b710c8b8dcf996e9140ce4321d2a74c1eb077b4994a32c4ee471",
	"8febe0d70ef71c62186630bb010a257830261a1993e6cab96b83aaa942b1cd48",
	"613f795dc36217b4f9a4ed435000514742f06bc2cb29cc8c99632e4e02127b7e",
	"4b83a94bca39bb07be53d40e4b0c530ca1ae85f9b4283f8f57000a59e7eccf56",
	"8dc2c41527d4cc11be582d7c29b6d55392365adeeebdb19f4e3105ce9875740c",
	"dcf964e96cb3d5ef362da9d2467cee6e50396e290ab89d7c0846a7d989ab6848",
	"97297f266aee72fd8b4a030f08d1762f285bd5a25a631f820a8757c844fdc744",
	"b9775ba5ab5bab7e4b46719db640462e716e0c1310724900753d7247b69f7a13",
	"224cbaa7bcd59547c354ce80b28b797e12d6c08ab8ad2635bf069081bad9dc43",
	"d0c50cfe6125d048957ccb41e61fd8042857f03c439dcad1ccbeada8d018100b",
	"ef17e5fb67c9bcababb137f553f3d5392f4861d7686bd5a8fedd57ef4d42515b",
	"780ced0930f23d9eb4edc3aed931bb6a459c02c616cd31af0d58fab7109ac028",
	"52f578d6efb1db0ea8530fd18803da00aad8080328c81c8868187c505e36ba72",
	"03a53b75c6bc97a37333ac65aa1be747a247afc50ca4dd95e719113174da1a08",
	"d4e0b7979e60631e905c315ecc3ed14cdf1b2d3d5020014cda256b71e35f9070",
	"223d4bb8969b6f2fed55591a03d0211d2b0f2e5fd1bc748ac31524536c61e911",
	"cb0be4b6c1c0f5a1eac6487d22f74b49db5f2d16b65fafbda86a26af6357ba0b",
	"1dc6eeebe5974108f66c2b69741ab84ac3e82c311c69630453789c6d1527c420",
	"0919f9499d4d9160e53bcae2266e69544533cd2fd8b551500f6a8f98246a8028",
	"d20dce7ff19d2d7cea5a32138a6c212c38e0b76fa63457dbb7d3e07ffc117b50",
	"79175bfdd77bd630efaa7958f8e4ed37dbe03c1aabf3bd5675d0b63d17be405c",
	"c981fa60ae83e38dda0412fc16d207067d4bd6aaa0b4c19153ab5723ce80b06f",
	"d65b7ddb137319bef9ab0c4540ef9962e35bb27b9186f6e7ae9aa581c6c3ec48",
	"8a9986da2e304daa564f75fc3afa0d62fb7be8e68f055c7d91d9855f4824ab6c",
	"32ed68aba78e6aaa37df5d3113940165d26a5264bb8f89a6bc415160e8a6af33",
	"524864775d0206a2a49d6095ce01c97f65497d78d4826ec658c8c3d4e0648e2d",
	"03cd9d01e3564c79f5e9051b47bcd8438f20123efa497f067d4a7a1b07001f0b",
	"20c87e0b39d6a92505150dc5fea2aa5373df09faca8259a7137ee074230b145b",
	"9a03cceed0e10aece557e98a41bcb9562d7ee8488bbd9352c4195b5fb2ac1431",
	"01c285042be6b83819e7b6768b714f36d5629d33c65e0e13055c5fc6e978242a",
	"0152b50944652f92c30f01dc1270ac158c3eb0f6f2a58d67bd0f2f570fcc654c",
	"dafe815d212af1e7729ad8e43af10e129dc5bb80b57f2724d68d5d1adafacf61",
	"eea07f90a65cd2991c00b70f8f132d4691df52f874937d6e632ddaea6b462134",
	"7e069d51723e31f6d7e7a5fb6423121a7297b8984ad1a3cb721211f574b6e76d",
	NULL
};

static const char *KAT_HASH2[] = {
        // For i = 0 to 9, hash-to-curve using as data the BLAKE2s hash of
        // a single byte of value i.
	"89c0d5e5d8b85dc64aa01128392fde67cfdf3d134d1c1cb2454f49a7fa332d10",
	"146108e34523fb073833c020c39c47695cec4e8ebc9df120b380f75ccf480266",
	"2e1eb08730fb1029454cc363c0341472e38f0ad5ffd205e767cf935f1af76737",
	"24416b842dcbf578b38a00b0565c8e2387d9d7391d846ed746685bb4afc3ee0b",
	"e9f8bbd2dad91f2c4aca106f953a53026f3a4b0e2d04e49b279a6f45d8fb5347",
	"7d57851cb89cf067255171d8c29be752a9d1b0337b3ac446ba35d9daab6e0c0f",
	"dcc2b346f340cbb9e2c2e2a75c14997bcb828135a8e6996345effff4f7b89127",
	"585cc121fbcf8fded35d98f232f2100bb6758d76c30ba3c9c9d0dc2aa5818a29",
	"b93b56b759832e62911dc203a8f9225e150f33b6daacd22559715611b808bc29",
	"f04a925c43f525efce533891141f0352b91537b1bb56f2e0740bd6695dc9e62b",
	NULL
};

static void
test_hash_to_point(void)
{
	printf("Test hash_to_point: ");
	fflush(stdout);

	for (int i = 0; KAT_MAP_TO_CURVE[i] != NULL; i += 2) {
		uint8_t buf[32], rbuf[32];
		gls254_point p;

		HEXTOBIN(buf, KAT_MAP_TO_CURVE[i]);
		HEXTOBIN(rbuf, KAT_MAP_TO_CURVE[i + 1]);
		gls254_map_to_point(&p, buf);
		gls254_encode(buf, &p);
		check_eq_buf("KAT map", buf, rbuf, 32);

		printf(".");
		fflush(stdout);
	}

	printf(" ");
	fflush(stdout);

	uint8_t data[100];

	for (int i = 0; i < 100; i ++) {
		uint8_t buf[32], rbuf[32];
		gls254_point p;

		HEXTOBIN(rbuf, KAT_HASH1[i]);
		data[i] = (uint8_t)i;
		gls254_hash_to_point(&p, NULL, data, i);
		gls254_encode(buf, &p);
		check_eq_buf("KAT hash1", buf, rbuf, 32);

		printf(".");
		fflush(stdout);
	}

	printf(" ");
	fflush(stdout);

	for (int i = 0; i < 10; i ++) {
		uint8_t buf[32], rbuf[32];
		gls254_point p;

		HEXTOBIN(rbuf, KAT_HASH2[i]);
		buf[0] = (uint8_t)i;
		blake2s(buf, 32, NULL, 0, buf, 1);
		gls254_hash_to_point(&p, "blake2s", buf, 32);
		gls254_encode(buf, &p);
		check_eq_buf("KAT hash2", buf, rbuf, 32);

		printf(".");
		fflush(stdout);
	}

	printf(" done.\n");
}

static void
scal_reduce_finish(uint8_t *d, const uint8_t *r)
{
	for (;;) {
		uint8_t tmp[32];
		unsigned cc;
		int j;

		cc = 0;
		for (j = 0; j < 32; j ++) {
			unsigned m;

			m = d[j] - r[j] - cc;
			tmp[j] = (uint8_t)m;
			cc = (m >> 8) & 1;
		}
		if (cc != 0) {
			return;
		}
		memcpy(d, tmp, 32);
	}
}

static void
scal_reduce(uint8_t *d, const uint8_t *a, size_t a_len, const uint8_t *r)
{
	uint8_t tmp[32];
	int i, j;

	memset(tmp, 0, sizeof tmp);
	for (i = (int)a_len * 8 - 1; i >= 0; i --) {
		for (j = 31; j > 0; j --) {
			tmp[j] = (tmp[j] << 1) | (tmp[j - 1] >> 7);
		}
		tmp[0] = (tmp[0] << 1) | ((a[i >> 3] >> (i & 7)) & 1);
		while (tmp[31] >= 0x80) {
			unsigned cc;

			cc = 0;
			for (j = 0; j < 32; j ++) {
				unsigned m;

				m = tmp[j] - r[j] - cc;
				tmp[j] = (uint8_t)m;
				cc = (m >> 8) & 1;
			}
		}
	}
	scal_reduce_finish(tmp, r);
	memcpy(d, tmp, 32);
}

static void
scal_add(uint8_t *d, const uint8_t *a, const uint8_t *b, const uint8_t *r)
{
	int i;
	unsigned cc;

	cc = 0;
	for (i = 0; i < 32; i ++) {
		unsigned m;

		m = a[i] + b[i] + cc;
		d[i] = (uint8_t)m;
		cc = m >> 8;
	}
	while (cc != 0) {
		cc = 0;
		for (i = 0; i < 32; i ++) {
			unsigned m;

			m = d[i] - r[i] - cc;
			d[i] = (uint8_t)m;
			cc = (m >> 8) & 1;
		}
		cc = 1 - cc;
	}
	scal_reduce_finish(d, r);
}

static void
scal_sub(uint8_t *d, const uint8_t *a, const uint8_t *b, const uint8_t *r)
{
	int i;
	unsigned cc;

	cc = 0;
	for (i = 0; i < 32; i ++) {
		unsigned m;

		m = a[i] - b[i] - cc;
		d[i] = (uint8_t)m;
		cc = (m >> 8) & 1;
	}
	while (cc != 0) {
		cc = 0;
		for (i = 0; i < 32; i ++) {
			unsigned m;

			m = d[i] + r[i] + cc;
			d[i] = (uint8_t)m;
			cc = m >> 8;
		}
		cc = 1 - cc;
	}
	scal_reduce_finish(d, r);
}

static void
scal_half(uint8_t *d, const uint8_t *a, const uint8_t *r)
{
	int i;

	memmove(d, a, 32);
	scal_reduce_finish(d, r);
	if ((d[0] & 1) != 0) {
		unsigned cc;

		cc = 0;
		for (i = 0; i < 32; i ++) {
			unsigned m;

			m = d[i] + r[i] + cc;
			d[i] = (uint8_t)m;
			cc = m >> 8;
		}
	}
	for (i = 0; i < 31; i ++) {
		d[i] = (uint8_t)((d[i] >> 1) | (d[i + 1] << 7));
	}
	d[31] = d[31] >> 1;
}

static void
scal_mul(uint8_t *d, const uint8_t *a, const uint8_t *b, const uint8_t *r)
{
	int i, j;
	uint8_t t[64];

	memset(t, 0, sizeof t);
	for (i = 0; i < 32; i ++) {
		unsigned cc;

		cc = 0;
		for (j = 0; j < 32; j ++) {
			unsigned m;

			m = a[i] * b[j] + t[i + j] + cc;
			t[i + j] = (uint8_t)m;
			cc = m >> 8;
		}
		t[i + 32] = (uint8_t)cc;
	}
	scal_reduce(d, t, 64, r);
}

static const uint8_t GLS254_R[] = {
	0xF5, 0x8C, 0x3A, 0xF4, 0x7C, 0xE3, 0xBD, 0x3C,
	0xAD, 0x1D, 0x1A, 0xDC, 0xDE, 0x47, 0x1A, 0x3F,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20
};

static const uint8_t GLS254_MU[] = {
	0x14, 0xF6, 0xA1, 0x89, 0xFC, 0x87, 0x84, 0x1B,
	0xFC, 0x63, 0xE1, 0xFA, 0xF1, 0xAD, 0xEF, 0x1E,
	0x99, 0xE4, 0x3F, 0x36, 0xDA, 0xBD, 0x58, 0x9F,
	0x93, 0xBC, 0x54, 0x0F, 0xD0, 0xD0, 0xE6, 0x17
};

static void
test_scalar(void)
{
	printf("Test scalar: ");
	fflush(stdout);

	for (int i = 0; i < 20; i ++) {
		uint8_t bb[96], a[32], b[32], c[32], d[32];
		unsigned w;
		static const uint8_t zero[32] = { 0 };

		bb[0] = (uint8_t)(3 * i);
		bb[1] = (uint8_t)((3 * i) >> 8);
		blake2s(bb, 32, NULL, 0, bb, 2);
		bb[32] = (uint8_t)(3 * i + 1);
		bb[33] = (uint8_t)((3 * i + 1) >> 8);
		blake2s(bb + 32, 32, NULL, 0, bb + 32, 2);
		bb[64] = (uint8_t)(3 * i + 2);
		bb[65] = (uint8_t)((3 * i + 2) >> 8);
		blake2s(bb + 64, 32, NULL, 0, bb + 64, 2);

		for (int j = 0; j <= (int)sizeof bb; j ++) {
			scalar_reduce(a, bb, j);
			scal_reduce(b, bb, j, GLS254_R);
			check_eq_buf("scalar_reduce", a, b, 32);
		}
		memcpy(a, bb, sizeof a);
		memcpy(b, bb + 32, sizeof b);

		scalar_add(c, a, b);
		scal_add(d, a, b, GLS254_R);
		check_eq_buf("scalar_add", c, d, 32);

		scalar_sub(c, a, b);
		scal_sub(d, a, b, GLS254_R);
		check_eq_buf("scalar_sub", c, d, 32);

		scalar_neg(c, a);
		scal_sub(d, zero, a, GLS254_R);
		check_eq_buf("scalar_neg", c, d, 32);

		scalar_half(c, a);
		scal_half(d, a, GLS254_R);
		check_eq_buf("scalar_half", c, d, 32);

		scalar_mul(c, a, b);
		scal_mul(d, a, b, GLS254_R);
		check_eq_buf("scalar_mul", c, d, 32);

		uint32_t sk0, sk1;
		memset(c, 0, sizeof c);
		memset(d, 0, sizeof d);
		scalar_split(c, &sk0, d, &sk1, a);
		if (sk0 == 0xFFFFFFFF) {
			scal_sub(c, zero, c, GLS254_R);
		} else if (sk0 != 0x00000000) {
			printf("invalid sk0: 0x%08X\n", sk0);
			exit(EXIT_FAILURE);
		}
		scal_mul(d, d, GLS254_MU, GLS254_R);
		if (sk1 == 0x00000000) {
			scal_add(c, c, d, GLS254_R);
		} else if (sk1 == 0xFFFFFFFF) {
			scal_sub(c, c, d, GLS254_R);
		} else {
			printf("invalid sk1: 0x%08X\n", sk1);
			exit(EXIT_FAILURE);
		}
		scal_reduce(d, a, sizeof a, GLS254_R);
		check_eq_buf("scalar_split", c, d, 32);

		memcpy(a, GLS254_R, 32);
		w = (unsigned)a[0] + ((unsigned)a[1] << 8);
		w -= 500;
		w += i;
		a[0] = (uint8_t)w;
		a[1] = (uint8_t)(w >> 8);
		if (i < 500) {
			if (!scalar_is_reduced(a)) {
				printf("test_reduced (%d)\n", i);
				exit(EXIT_FAILURE);
			}
		} else {
			if (scalar_is_reduced(a)) {
				printf("test_reduced (%d)\n", i);
				exit(EXIT_FAILURE);
			}
		}

		printf(".");
		fflush(stdout);
	}

	printf(" done.\n");
}

static void
test_gls254_window(void)
{
	printf("Test window: ");
	fflush(stdout);

	for (int i = 1; KAT_DECODE_OK[i] != NULL; i ++) {
		uint8_t buf[32];
		gls254_point p, q;
		gls254_point_affine win[8];

		HEXTOBIN(buf, KAT_DECODE_OK[i]);
		gls254_decode(&p, buf);
		gls254_xdouble(&p, &p, 3);
		gls254_make_window_affine_8(win, &p);
		q = GLS254_NEUTRAL;
		for (int j = 0; j < 8; j ++) {
			gfb254 tx, ts, tz2;
			gls254_add(&q, &q, &p);
			gfb254_square(&tz2, &q.Z);
			gfb254_mul(&tx, &win[j].scaled_x, &tz2);
			gfb254_mul(&ts, &win[j].scaled_s, &tz2);
			if (!gfb254_equals(&tx, &q.T)
				|| !gfb254_equals(&ts, &q.S))
			{
				printf("ERR (j=%d)\n", j);
				exit(EXIT_FAILURE);
			}

			gls254_point_affine ta;
			gls254_normalize(&ta, &q);
			if (!gfb254_equals(&ta.scaled_x, &win[j].scaled_x)
				|| !gfb254_equals(&ta.scaled_s,
					&win[j].scaled_s))
			{
				printf("ERR (j=%d) [norm]\n", j);
				exit(EXIT_FAILURE);
			}

			gls254_point r;
			gls254_from_affine(&r, &ta);
			if (!gls254_equals(&r, &q)
				|| !gfb254_equals(&r.X, &r.T)
				|| !gfb254_equals(&r.Z, &GLS254_NEUTRAL.Z))
			{
				printf("ERR (j=%d) [from_aff]\n", j);
				exit(EXIT_FAILURE);
			}
		}

		gls254_point_affine pa;
		gfb127 x0, x1, s0, s1, tt;

		x0 = win[4].scaled_x.v[0];
		x1 = win[4].scaled_x.v[1];
		s0 = win[4].scaled_s.v[0];
		s1 = win[4].scaled_s.v[1];

		gls254_zeta_affine(&pa, &win[4], 0);
		gfb127_add(&tt, &x0, &x1);
		if (!gfb127_equals(&tt, &pa.scaled_x.v[0])) {
			printf("ERR zeta(zn = 0) x0\n");
			exit(EXIT_FAILURE);
		}
		if (!gfb127_equals(&x1, &pa.scaled_x.v[1])) {
			printf("ERR zeta(zn = 0) x1\n");
			exit(EXIT_FAILURE);
		}
		gfb127_add(&tt, &s0, &s1);
		gfb127_add(&tt, &tt, &x0);
		if (!gfb127_equals(&tt, &pa.scaled_s.v[0])) {
			printf("ERR zeta(zn = 0) s0\n");
			exit(EXIT_FAILURE);
		}
		gfb127_add(&tt, &s1, &x0);
		gfb127_add(&tt, &tt, &x1);
		if (!gfb127_equals(&tt, &pa.scaled_s.v[1])) {
			printf("ERR zeta(zn = 0) s1\n");
			exit(EXIT_FAILURE);
		}

		gls254_zeta_affine(&pa, &win[4], 0xFFFFFFFF);
		gfb127_add(&tt, &x0, &x1);
		if (!gfb127_equals(&tt, &pa.scaled_x.v[0])) {
			printf("ERR zeta(zn = -1) x0\n");
			exit(EXIT_FAILURE);
		}
		if (!gfb127_equals(&x1, &pa.scaled_x.v[1])) {
			printf("ERR zeta(zn = -1) x1\n");
			exit(EXIT_FAILURE);
		}
		gfb127_add(&tt, &s0, &s1);
		gfb127_add(&tt, &tt, &x1);
		if (!gfb127_equals(&tt, &pa.scaled_s.v[0])) {
			printf("ERR zeta(zn = -1) s0\n");
			exit(EXIT_FAILURE);
		}
		gfb127_add(&tt, &s1, &x0);
		if (!gfb127_equals(&tt, &pa.scaled_s.v[1])) {
			printf("ERR zeta(zn = -1) s1\n");
			exit(EXIT_FAILURE);
		}

		gls254_point r;
		gls254_add_affine_affine(&r, &win[2], &win[4]);
		if (!gls254_equals(&q, &r)) {
			printf("ERR add_affine_affine\n");
			exit(EXIT_FAILURE);
		}

		for (int j = -8; j <= 8; j ++) {
			gls254_point_affine qa;

			gls254_lookup8_affine(&pa, win, (int8_t)j);
			if (j < 0) {
				qa.scaled_x = win[-j - 1].scaled_x;
				gfb254_add(&qa.scaled_s,
					&win[-j - 1].scaled_s,
					&win[-j - 1].scaled_x);
			} else if (j == 0) {
				qa.scaled_x = GLS254_NEUTRAL.X;
				qa.scaled_s = GLS254_NEUTRAL.S;
			} else {
				qa = win[j - 1];
			}
			if (!gfb254_equals(&pa.scaled_x, &qa.scaled_x)) {
				printf("ERR lookup(j = %d) X\n", j);
				exit(EXIT_FAILURE);
			}
			if (!gfb254_equals(&pa.scaled_s, &qa.scaled_s)) {
				printf("ERR lookup(j = %d) S\n", j);
				exit(EXIT_FAILURE);
			}
		}

		printf(".");
		fflush(stdout);
	}

	printf(" done.\n");
}

static void
test_mul(void)
{
	printf("Test mul: ");
	fflush(stdout);

	uint8_t buf1[32], buf2[32];
	blake2s(buf1, sizeof buf1, NULL, 0, "test_mul", 8);
	gfb254 z;
	gfb254_decode32_reduce(&z, buf1);
	gls254_point p, q;
	p = GLS254_BASE;
	gfb254_mul(&p.X, &p.X, &z);
	gfb254_mul(&p.Z, &p.Z, &z);
	gfb254_square(&z, &z);
	gfb254_mul(&p.S, &p.S, &z);
	gfb254_mul(&p.T, &p.T, &z);
	if (!gls254_equals(&p, &GLS254_BASE)) {
		printf("ERR base\n");
		exit(EXIT_FAILURE);
	}

	HEXTOBIN(buf1, "d2d85b649ca1cb28cf6a710ea180864b48be872c7a9585fafc01ff8259ee4e09");
	gls254_mul(&q, &p, buf1);
	gls254_encode(buf1, &q);
	HEXTOBIN(buf2, "6832ca87b11a5efd7718bc3cff30dc7e2fe8dd0309aa4744208c43157cc1eb46");
	check_eq_buf("KAT mul 1", buf1, buf2, 32);

	printf(".");
	fflush(stdout);

	gls254_xdouble(&p, &GLS254_BASE, 120);
	gls254_encode(buf1, &p);
	HEXTOBIN(buf2, "18e08856b0ee260dd4bb2c94e52044378415677408e515f7fb22fbd6215c2a4b");
	check_eq_buf("KAT mul 2", buf1, buf2, 32);
	printf(".");
	fflush(stdout);
	for (int i = 0; i < 1000; i ++) {
		gls254_encode(buf1, &p);
		gls254_mul(&p, &p, buf1);
		if ((i % 20) == 19) {
			printf(".");
			fflush(stdout);
		}
	}
	gls254_encode(buf1, &p);
	HEXTOBIN(buf2, "4af66e2bd76b1cbdc04913cbd8b66d4e04f7935cae2ca489dd60a43b98db0f59");
	check_eq_buf("KAT mul 3", buf1, buf2, 32);

	printf(" done.\n");
}

static void
test_mulgen(void)
{
	printf("Test mulgen: ");
	fflush(stdout);

	for (int i = 0; i < 20; i ++) {
		uint8_t v[32];
		v[0] = i;
		blake2s(v, 32, NULL, 0, &v[0], 1);

		gls254_point p1, p2;
		gls254_mul(&p1, &GLS254_BASE, v);
		gls254_mulgen(&p2, v);
		if (!gls254_equals(&p1, &p2)) {
			printf("ERR\n");
			exit(EXIT_FAILURE);
		}

		printf(".");
		fflush(stdout);
	}

	printf(" done.\n");
}

static void
test_key(void)
{
	printf("Test key: ");
	fflush(stdout);

	for (int i = 0; i < 20; i ++) {
		gls254_private_key sk, sk2;
		gls254_public_key pk;
		uint8_t tmp[32];

		tmp[0] = (uint8_t)i;
		gls254_keygen(&sk, tmp, 1);
		gls254_encode_private(tmp, &sk);
		gls254_decode_private(&sk2, tmp);
		check_eq_buf("skey(sec)", sk.sec, sk2.sec, 32);
		if (!gls254_equals(&sk.pub.pp, &sk2.pub.pp)) {
			printf("ERR skey(pub)\n");
			exit(EXIT_FAILURE);
		}
		gls254_encode_public(tmp, &sk.pub);
		gls254_decode_public(&pk, tmp);
		if (!gls254_equals(&sk.pub.pp, &pk.pp)) {
			printf("ERR pkey\n");
			exit(EXIT_FAILURE);
		}

		printf(".");
		fflush(stdout);
	}

	printf(" done.\n");
}

static const char *KAT_SIGN[] = {
	// Each group of five values is:
	//   private key
	//   public key
	//   seed ("-" for an empty seed)
	//   data (BLAKE2s of "sample X" with X = 0, 1,...)
	//   signature

	"1dbf78267a3c78d87e567b309ec8d053c83325b32353220aa82add4d8a77b51d",
	"91cac0f40686c2e29c007f6db14e8e110708ce9c1dcbc8b57e8167c3b2c9c77e",
	"",
	"ec14004660d4b02da3b86b1bc5afa7b2e4827f0ee1c9a25472a2bcac521bc231",
	"85787ce65a0679bed708dd2655dd7e0fe1c677c1f09690fcbcb721737b942b77fe2a1fb9e90cf7f1d0807c11435adf0d",

	"c69e2616d78bddabe8a3ca558c9e399ed2945665c96494891c46dcd8c116d103",
	"891b2b8b544486b3a458159dc324ee0eb4f091eb3d23526bd917c636b28b7d69",
	"11",
	"bd1a4655b90f873c53fe908f4109bb8dfcd9096312b447a6434af3c35304b7d1",
	"4eaca5162addfbaf45cb1009d963c904a5a1b8eb67677d41f9686ed68434493df08bf07ef7286f402242028e30d4020c",

	"0f4f28a0a11c48fb9c61fc8f5842347a613e7bcb24f0a0cbc4f9dc0e51aae917",
	"3b2f0706d2c51ab5e627a1cec8246664f739020c8025c6898a00e905cca4ce37",
	"5b6e",
	"6230441be7f030f180e81dc44502b24ed94260490d140ae738bb80746051651e",
	"885dfbae8e88f5fac429bc628c9942c432a7e2cd06ddacdec9cf3abc644d1b2f1d29deca504e9fe67d040141f50d0701",

	"78d4ab8b96ae6bd93823cf8e2f79ab9f5b4df74a54974bac023c7fdc1d162a1e",
	"176963b138dd2b5e8f9c8a5f2fe50e198a65a16820fde23d9cbc505fc6c3201e",
	"fe7b59",
	"e877b70f8c12aff466a4dbd6284bd0c6ad7cf66376bdad599f22145f8277bc52",
	"2096ce24c16aae16117fe0f03c20b27c9e51bbb48ea8527773f0b435a27d8d3d90a5c05d625ce3d4cae066684bc02a1a",

	"68cd1cfad4651bb160abef55f88710bde5ee6636b637ce4aaf027da99baa871b",
	"f74c41414d731f85a1cc0d277ec0b177abcb8e22b970b9bb626952e7c400f90e",
	"91d66d55",
	"b4c94e55cc622b96b49fcfe6b913ce3a06050b7e9b26fe840389145088d59502",
	"d4390b07858770cca8abc9915085e914048e712de6f7ec412440024defe90100921724d18a882c340b407fd99bbba21b",

	"d881adfd928d96b413e15e35a55a1a144d4fea4dfca24df5357fc276c84a3103",
	"0a72c07740eeb9795310ccc7527bae56f884c24d679139ac4da1209346e8ce64",
	"b768840af3",
	"a7ad895209663ae35bfb3fb0e44cc83616bb876d14608e5b09c20d19f57839d4",
	"2b52d00f71dad9707aeb329eef0beb0ac2d65850928c6981e048e46d40b36d5b7c9064a8e6dd06d88bd478b763bb1516",

	"3b7ff4b04edc7e95d5a5f4d75756c178a21b76c01b32375baab60d46bd608f0d",
	"ec3a048a22b148d88b8c82c9d00949276e15b8246b21438b3854d5b53385c818",
	"92c1c211204d",
	"0bd1a3ff8506a918b8bd733c31cec084927241dda2ede63f719a6758872c94ab",
	"17501cc4379c33edef42bf87a41be514903c0719c65eae67abe622d8fa3e196a31db2f430ee5c5e3a93d824eb02f7d0e",

	"d2c3599251bc3e8412f06c336136fe6a206563b128ab817f21ac2c07b6f1a412",
	"976468d48c037f40997e37d74115c8647d4933f7f03d174319e64a794513e238",
	"d5bfee51716f4c",
	"f328909fd158f3541c2da54b758ccf750bfe4afa717b00094fd30e7fd69661e3",
	"beda93626600f76a265dcb15d7611ef8f3714ee4e334b4c0f040db12e42cc229fcefb159f19927decc439a4942ea7007",

	"2e6ad8738eab9d29811feda93fbc71f87621a444514b61940892e5ef24c99518",
	"15e419df2569d74c8f0491747b10f74cfa5952e2dd84611f345e26edaf31a848",
	"25d55a3117fcfb1d",
	"7de5f8c2c35149558c0a6bef84596669100f6350f07aefed58120d6dc3531231",
	"be6d4facaff0115f886e5cbf701a6baf55af24d53fce8c6c3dbdee14b652762d1c6b97c83aa3d00682ebe31c2e4f5c18",

	"c13ff9d1170875eb7eab4f0013cd1945092917b705fc3dc8f18bc33f62443d14",
	"74ada21664eb808ba4fbe1b150c61d207547e3b2b0588f732dc18127f4be7a58",
	"540398f5f8ac3bd048",
	"9fbcee44419bc19b97bb673d0055faa0aae1861f44c682345fb3494e610e26da",
	"0606b1a812dd9163ef721de26de05e41f1a371d29dce4f123a5ac51963ab43999ae9e283194ec31736ce12e5938f4713",

	"ceb71f69091862d376bd54d86483bd8122464f6b2f6fcbb3a92f3d1604336a17",
	"c0dd1b298fafb007016288f0fe19a179ba61c265502a04162bb3377e845fec48",
	"f7dc92978d97e11aacbc",
	"4ca14993a888660c624f816db0c893bfac69d5ddf04cced60333d94ac1b0e2f5",
	"d108ca24db6dc30cd36b5eb0266ae495255851a72a078ceb2394702c959adaa0ae9bef52f534bffb93d4a53cf1febb15",

	"8dac486f7f8c7e0f8530f113e4b9a754fb6c4bc807a0eaad4f2965dcc574cf02",
	"0b8b6baa03341662361e3dbecc933e28c40030cca49b8cc272609e2cf9c64656",
	"282ce007c2f416cf4eff41",
	"ed427029b6afbfe2a73c7a73605bfb47b4db8eadc940bddc103098a06d7b7daf",
	"f15d35f52952c6c7644e66331fa5b78468b46e6e08fe0f83c4f56e0d8cb14abce98414fec58df3e2773ecba970c55101",

	"b8a52aca615df8fc7cd48a7391ff8c101494f43ef4a95c49808d754112409816",
	"471ebd8a26d3857957721e5139f3117d48dc691eb4dc3f4030405721eeb1c250",
	"6bce806f389db2ae12e9fd9f",
	"2b083962ac0f0d9421bffdf9377f06e7152c3677e911029b08f9d40688c8aaa8",
	"702ba7a045265d5e0d516e8df6ed1f6ea5399f6b07eb2adf75a23e914c8670c3ab1e44f717de3c528f7936bb888d1216",

	"0dda8526b240c1544d3c39f43bdd16984fd33dcd2fb464b8066c889daeabce1b",
	"3a1ef01cb3345d888bc078a05569ce2b11ef37eb388dc6157e8e786fba08146b",
	"c360afc16a5fe7e775739e7fa1",
	"cf44d2ca3441b9089e99a00eb90fe161bc994990469a46b488e08711a7ba8d9e",
	"71bd7242279e9f555b8bd8578b29803409fb215f81b31192a6b13552c10dfec7029297c5dd3a64aa596df0f2de01b71a",

	"45164615ada309a0564da2c6f81c798e255e491659b3199af834252990890d1a",
	"e51fcd414416561f91fffe4435c3232544961897fd27bada532d0b9e702ba206",
	"ece299589e82b605a1e20723de3a",
	"79d41d37434fa78c4cd3fb421c7caa26704df53c215adcc4f7807adde10c7438",
	"3fc0755f21a565b7a49fc6447884ffd73c10752b8b0219fb650a015c4feeabcccd70f15ba0c6a689ccf3ba1960995a06",

	"592382f4b473bd254c710ec78461d68caff55b5bd9cd71303fe5fcf8b34ef108",
	"7b9ba7240bc13d361315efb641ed776d61a12c10f1bd2df16999dc2b92a75f6e",
	"a541117e1b92ae3d2e5fceddcb1a58",
	"0756a67df9f84be0d319c4e8d324f3b77077f9322f9603f015df27f2804b17a2",
	"31a66f0e2054db3fa6a33eae6ceb8f7725fb79dcab20fd727857fb69ebfc8956c10e6b7163950410574e736a034bcb0f",

	"9b44f46bb75e7e710882365d02be0d40c211d926921629d2105a068eaa541008",
	"f0cb399edcbd2d72928787e09b3cef576940c2f2be1a780831a4e299b001aa41",
	"f3a9781c21ea1c8fbf65677793cc9449",
	"86b36dde6d628b67332456b5d41d09737a057215f72f89094d071422e705b82e",
	"fcb261760103af02ef4c2e768c6ec67049eb26878cb7bb3b17154e332e63d2335a125bbc295fcefba030634b051b9a09",

	"0ab6fa175df3ee6ee0ba766652d5583032d2f60ce8e4bc4afdde81f94b935a07",
	"b1bd4d4ac2cada44053708afe4ad200b9d2cf74b2e87212365271db21948db1a",
	"62e978968fca2374a00cd45788172b866a",
	"86497726e18b409075f7036b1c65deacab22cf85d2ae64ef1857e17a9713e4fb",
	"4cb2068f5a0fd13f7668dd0bc95af371ad95f024ffb6060c035fb251c2d2e4161cddc0e8ccf0d991446a33efec403319",

	"17b237e86dbc84537be61750d9237dadf8b779a080b7c159d25cf6a8727bea06",
	"dc852b6cdaddcdf3963c96cc7511dc1a84a6a7c5e9d243f4d86fee0a36a7d726",
	"5bf1bc88327a261bfeb63e7a9da4d6930cc5",
	"a2edb2c979a443ff733c32453d350f09af33068a5640af90940315e7d3c87957",
	"8f2784580557ffd3a7907c7eac293c8fe162231c2145987207200a52ea8d7ed6bc2ac31ff029f8776652302ba0c2c615",

	"29567c5fee0619ee539d0a835e90683f796caca4b20ea671cc8e75a47574d006",
	"ebdd954c92cc91b565f11dda690bfb158f2579582a624dd6553508b4a0484a3e",
	"c266e16ad55956e200d682349704f04454a79b",
	"ba789f5876b8db6ae44d0e4507de9993c83e504804c1f3f8619adbd717847b77",
	"fc52da48297237021dacec1c9cae7b6f99885dd5a679528f7ee5b835e8725be444a838715e75202870ace597cd5a8104",

	NULL
};

static void
test_sign(void)
{
	printf("Test sign: ");
	fflush(stdout);

	for (int i = 0; KAT_SIGN[i] != NULL; i += 5) {
		uint8_t skbuf[32], pkbuf[32], tmp[32];
		uint8_t seed[20], data[32], sig[48], rsig[48];
		size_t seed_len;
		gls254_private_key sk;
		gls254_public_key pk;

		HEXTOBIN(skbuf, KAT_SIGN[i]);
		HEXTOBIN(pkbuf, KAT_SIGN[i + 1]);
		seed_len = hextobin(seed, sizeof seed, KAT_SIGN[i + 2]);
		HEXTOBIN(data, KAT_SIGN[i + 3]);
		HEXTOBIN(rsig, KAT_SIGN[i + 4]);
		if (!gls254_decode_private(&sk, skbuf)) {
			printf("ERR decode private\n");
			exit(EXIT_FAILURE);
		}
		pk = sk.pub;
		gls254_encode_public(tmp, &pk);
		check_eq_buf("encode public", tmp, pkbuf, 32);

		gls254_sign(sig, &sk, seed, seed_len,
			"blake2s", data, sizeof data);
		check_eq_buf("signature", sig, rsig, 48);

		if (!gls254_verify_vartime(&pk, sig,
			"blake2s", data, sizeof data))
		{
			printf("ERR verify 1\n");
			exit(EXIT_FAILURE);
		}
		data[5] ^= 0x20;
		if (gls254_verify_vartime(&pk, sig,
			"blake2s", data, sizeof data))
		{
			printf("ERR verify 2\n");
			exit(EXIT_FAILURE);
		}

		printf(".");
		fflush(stdout);
	}

	printf(" done.\n");
}

static const char *KAT_ECDH[] = {
        // Each group of five values is:
        //   private key
        //   public peer point (valid)
        //   secret from ECDH with valid peer point
        //   public peer point (invalid)
        //   secret from ECDH with invalid peer point
	"efa5335c242f3fa3460af5edbb6d4ec580805d92fbdd6a74d823ca7eced1e913",
	"2d0df2615ae681cd2734bd905dad7017061b41a5559a28b60c9ec6e3ef23eb63",
	"527ab0a449272ba46cfcde5080277ae686de3d9380b3b48d8018cb7478f13022",
	"56dd447da86d00199a7aa9700ac6a04ce8bf17dcd63c4852a944cce18956d8df",
	"3264d7a70609782179f2f503e7d8a984b96d05e316a63f4b307ac6b71f071cd4",

	"eb7d1f13254c97ed868c48b437198443d52e89776dc36d55556f391283402e13",
	"4a98f953b3235351e9920e9a400ade7faa285d836e133e4f5d7583bf6b65c443",
	"dd4b60dc669d857307fdc0648cc7006ba916dbcdfef98d5e6ebfbc609e31b753",
	"784fdbfbfb8390d648db26ff5257ef297ad42e583cffa4097e066a554dcd68e1",
	"dc29ca8aa56d3c655b3bf54fa89488eb4b6ca8530f1eadfabf0e26f5120a46f9",

	"ab0ac3d0cda9e1c8ae4b3cd8c8a1035d1b481d49bf0db3f26231222e348ad110",
	"8b9259f63f63a281096e271510e87030bb0aa7ca39c1d60aaed2508316df8d4d",
	"822b628932f27a2cf111a7b1c969b8546122df0f9d66f33d658c64d1ab2d3ed0",
	"c1ab9fd79840127eae427a76fcecee00ae1f6c433b89da8afbe48f88f99bec18",
	"bb863d6b699612d0979d738e76805217b7464fe90105263b7680365a4de0220b",

	"72c32ea104b590c07cc19ccf0cb8c37ff0b050f995b68758753b548f4c2fd016",
	"cfcc2f4f284bfda1698f84f6abc0a051e83f885982d3f40da3b9f66f4fe6ce77",
	"0c70336df7bfbc428ef358636795d9770e94dfc1a7f386b4ef27dfe6edc76e8c",
	"015bc69f16e32d20718ec2f764fda2cfe4fa18f26ea953fe979d5a351b640f85",
	"7813d018d0de012b018024e5201227f4b0c493e1f2b374738ee2949894efc941",

	"f36d7dbd24414accc953191f6a223ea786a840f7c9dae46748848aa8c861760e",
	"4d73d669fa5499630a697cbc46d69c41e93f228fb7835fe32c0bc0d44536c24f",
	"0224aaf6009c89e9b3b96e70df9f0194071d5776470cbbc472953d309161f267",
	"917300222b564fc52e9c9de57df5ca87e2cbbd0ef8997946da0538eba3181b4f",
	"22c5125e020c4f4df71504b8b10e990af8e35aef02038a86f4a768111f2398d7",

	"db200046d4c6a9582518905450aeec6986dd29af62fa9ff72b1008e154545c00",
	"9aea9fa32969d4271f85474cc01ae24831ab62c4e55a2c5298732cda9d1b504a",
	"627e355cf8749fc778122d03f8a96dd0830404750fec3fa860f8bf8e98bade41",
	"53b112c4663a3e3518a795470dc89a315cff943406dabf5ee7029daa47016832",
	"f5555105d30629bbbe326ff746f59df940c8f90e5f7d23789e70e383354fdea0",

	"3849adc00abb42c53ad88d9321b8e7a87911ca33be530d0836073c8324ff4118",
	"4f09dddadd97fd62c5b042ee13b902696b8c5722c4635cf7f5a6602637ad5d63",
	"f3e4b28341d0be8c46f0ab0ca078560e36467195ef9e8a965436f0b12f146def",
	"623ac592961d2a91872138b1541dec4df67c4f90f47d2002e3b3ccaf511306e6",
	"eef21bae28cc67a99789678c3a5d2f7530c1944d496ed874e14f19826bb53829",

	"866140149b9ea7be1b15389b8872a5fff3e21547a43dd814fb223c164d77660a",
	"54f9982d97a33034e4e242413945bd7dee991b946ab5d64234a2f05ed674fe30",
	"4260598f7c3f908f74feb1860625a5fe1492410ea7c6e0cfbec226fd261773a1",
	"637cc59fac87980e66fe84ccb3c7ecb31694eeff78697155556a5dae6e88c0e7",
	"a991edf71253568dbec60100611bc8e53e7f16186472cafe14f5ee4dfe6ca37b",

	"b65cbbf772e0a85a7fac2ce1815df97185618d1d95a1d3eeddc60ef74f25c118",
	"7c515892f0d7f150a55c422274f21d7a2668f10ba17ee9cc03c39e2c894ea511",
	"ee1216e0f0d71caaa3a4168bfa3630a4499dffaf42cc4cde94b16b7b216b4a4e",
	"2412bb0dc99084fa88af4dc6f175bdfb3e48f1fd9737ae20d35ad7c7cf9ad7e6",
	"14e58ed179e5ff113e6c0fe6421f2d8ae872cb8ab3e02357fafc34d24799167a",

	"96725c787c7c6598c065d671c7ff9ea8d7e6c5d2bf43feaf6a9a1de3b4db2717",
	"c48d0a03c6f89cda616857eec962392a8473722c56d9fec6f08bbfd792b36c1c",
	"4b10091e1036ddb1f69a1299a7ff938a65ba095e087118e7900f11baaa729aad",
	"a5c172c8f415e4d680cd5684f2a629f260ac21694d4a812b5e9c63e93a117150",
	"516ea9297a13940f75cae38bca1c8e36b6c04ff7b53d3eda885d6408f555cede",

	"93a5bc7b61b87e2c73a96711524aebb61a08c0f86f07b76f8fcdead6081ce50c",
	"747f23e58a49cacac7ee2235e533f1676ae6b0fba3ecec66ec2fb1135008af68",
	"4b33a2006c0738463a08c2711a5777ffc1f4a0e0ceaf1c021459ed1334834852",
	"f09821ee19466e5d3fcd3e71b94084dd2a5e2ab2edf7ff5355983f2ea1c5fd07",
	"47a621d21f7e29885ba221aabde5f27a103f424782231d2f28a1c7653fc876f3",

	"7aba07d7b0eb9b91942c34d4b7d3109c169e16942bd8a10486bd91ae06385e05",
	"a5dce975971eb9a64003a540a6442f168919369ecdf04ae55b493a60c0b0b106",
	"8fa93f3e6a34d25fb9f0747c2684b54daebaa28b93d08da52f910c52f349609a",
	"aaa53549035e84af4b0dd9b9bf7ba124f619b7ee717755ce0e65fd08c8c32a96",
	"233836eb88365a170b5590b5b3fcde4d95afb9916a0dc34e8db685a80f933f02",

	"47aea7b0b75fd88ba8d25c2b5f2b5a5bfcb563fdde482ef17f9dc8908b649906",
	"0648d22d5fe5b96da305b049c2f6d076109fff5779555660f9bad7b1d9f4eb13",
	"2c6bcff0f28cb3238a6de3d21d256a714f327d65c834686dd013b200e4c74a3f",
	"0b24b3a777bd9281c29e37ebe395f33484b80355ee528539d752ba859d4ca94c",
	"4e4d9126698a5876a86f020dd297243a4464bb82dd350e47ce4f5f2ce3efbcfe",

	"e223af2985452c4873e22a8054c3db51d48841cd6888dcbcd953cdba4345c005",
	"270e8959897e231cccb7efa6bcd9341e5094e0f589d706b30d56ec6a7d6d494c",
	"008dd7dff93cd282b562fc7ee1f2e0f2c540b9a72c9d294bb261abd6bd1c92da",
	"7622faa5152516defc8df28b8c9c891bb3a20e06b6b255ed2e569e76521ee69f",
	"17777e61af7fd8c97c981b2f8731f7a153b4d98ae8185cbb68cfc15e1cb2117e",

	"333bfdb81bcf8fa7913263ed1d80533dfebc774c81c70dd285a69fbbf0508510",
	"c74aed16a68de4a045dcf9d111c87a041540c12cca1eab8748d4d4bd552ab701",
	"a495d9953f8c751191d9bccf8a25340d004b076a83f4d78c1e9a804518014a7e",
	"fcb135f686469182f8be6b771edcab95b9948abb16de7c972fce8edf5d709282",
	"4585b322626b6fca3767eccb347f1b47b06431a027d898bd668caed022fd43a3",

	"95cb2548d0205ba7141711002f1d331494c5a556800b31619e794139bd482605",
	"89fb4784ca2fd64308b8bfc5de0b9b3b28e3fd3bd809e92e3974508b762acb21",
	"54e6e8cd15c76989965526ed1f5c1f152395cf3291887b788f9e9e32bf02bc8d",
	"b86e4019bd672905f50e0f21050aaed117e65e475b9f465f449f91020dca90cd",
	"e222e92d4c16c0dc8cd662891e35a8c2b286cc9f581e46e29f55215192ef2cf0",

	"ade45d8a677bf6e66c4326360f09db3ac0cba03c5a7b80d8688d4a03b799760b",
	"98363d6f2c3deae404863fc2b43698244e626587ee11b34dd73907379bf7f153",
	"73af72e90d985a53ddbd71c0bdd7cda3994590f08ff2032375021b201b5d7d62",
	"6d4ddb5037a1df6b5d8dc214a205b3104c4487283739130700226352ee4d4197",
	"f82855d6a49d11aef755cc26c843ecbcb0267ea6f0643ea387c62cfb7d1a7e24",

	"2d9c4afe9d71deb4694fd00d5cb74c61d59bfdcefabfaa78c6b8bb5fc1fd831a",
	"4405a2f934eaf30dc1dc6f665844a72c179ffab7098042942e32914786e04820",
	"c735033bc49ecaf07e8936930cc853460a624c1026005b01f0907f08c1e447f9",
	"aa3a8b3048358a6ee8bc63462a15df75ff8c7f8bb77566d4a14cdb39d44ad3f5",
	"1ba9ccf41c40c334dfb3229670957e89b857770814d82c81053a9cfb916e645a",

	"b799b16aa1c859a5baecee1a32bdaaa6447e8a78c781fe257b57ed08b6d29703",
	"85d24ee9425ae5bbbfb5f972e7212741a4baff12814d1c15257a4d33df552963",
	"e65696b330ae269a21dd38355b3b0e0e8ba063b1b67af2f6140f5ea60e42e926",
	"66110cad99421b884a8a981f14545b6c3f51d1cb71f0dd0bb032b4a7a043bd8a",
	"6430c21a8fb58d23313e5f2adfbaa8610d4e9bf6ba6cd37a59ae6e74e796a0e9",

	"bff62ebd0cf6603ce6dbf09d1fed616db866d024a77260e78e4eac3445595e08",
	"20f70a98b6467a7d27a04ed34dc0db6bf7771b722ded7f96b7995768f9c6f925",
	"0b7aa162b8a7ca9d78da1dfeea1dd3b2c0cdcd088746a054c51f7f1ac5e5abb2",
	"4a88e940513e625a8519517ecbdd70914cfc3dcd38914086a34c0c9e2b1f7a57",
	"b4c1a5e6a04eb51cb1fe10a1d109cc1aae1a2ee0143d437e9038374ee9299ccf",
	NULL
};

static void
test_ECDH(void)
{
	printf("Test ECDH: ");
	fflush(stdout);

	for (int i = 0; KAT_ECDH[i] != NULL; i += 5) {
		uint8_t bufsk[32], bufpk1[32], bufr1[32], bufpk2[32], bufr2[32];
		HEXTOBIN(bufsk, KAT_ECDH[i]);
		HEXTOBIN(bufpk1, KAT_ECDH[i + 1]);
		HEXTOBIN(bufr1, KAT_ECDH[i + 2]);
		HEXTOBIN(bufpk2, KAT_ECDH[i + 3]);
		HEXTOBIN(bufr2, KAT_ECDH[i + 4]);
		gls254_private_key sk;
		gls254_public_key pk;
		uint8_t tmp[32];

		if (!gls254_decode_private(&sk, bufsk)) {
			printf("ERR decode priv\n");
			exit(EXIT_FAILURE);
		}

		if (!gls254_decode_public(&pk, bufpk1)) {
			printf("ERR decode pub (1)\n");
			exit(EXIT_FAILURE);
		}
		if (!gls254_ECDH(tmp, &sk, &pk)) {
			printf("ERR ECDH (1)\n");
			exit(EXIT_FAILURE);
		}
		check_eq_buf("ECDH 1", tmp, bufr1, 32);

		if (gls254_decode_public(&pk, bufpk2)) {
			printf("ERR decode pub (2)\n");
			exit(EXIT_FAILURE);
		}
		if (gls254_ECDH(tmp, &sk, &pk)) {
			printf("ERR ECDH (2)\n");
			exit(EXIT_FAILURE);
		}
		check_eq_buf("ECDH 2", tmp, bufr2, 32);

		printf(".");
		fflush(stdout);
	}

	printf(" done.\n");
}

static void
test_raw_ECDH(void)
{
	printf("Test raw ECDH: ");
	fflush(stdout);

	for (int i = 0; i < 20; i ++) {
		uint8_t v1[32], v2[32];
		v1[0] = 2 * i;
		blake2s(v1, 32, NULL, 0, &v1[0], 1);
		v2[0] = 2 * i + 1;
		blake2s(v2, 32, NULL, 0, &v2[0], 1);

		gls254_point p1, p2;
		gls254_point_affine pa1, pa2;
		gls254_mul(&p1, &GLS254_BASE, v1);
		gls254_mul(&p2, &p1, v2);
		gls254_normalize(&pa1, &p1);
		gls254_normalize(&pa2, &p2);

		uint8_t tmp1[64], tmp2[64];
		gfb254_encode(tmp1, &pa1.scaled_x);
		gfb254_encode(tmp1 + 32, &pa1.scaled_s);
		if (!gls254_raw_ECDH(tmp1, tmp1, v2)) {
			printf("ECDH_raw: failed\n");
			exit(EXIT_FAILURE);
		}
		gfb254_encode(tmp2, &pa2.scaled_x);
		gfb254_encode(tmp2 + 32, &pa2.scaled_s);
		check_eq_buf("ECDH_raw", tmp1, tmp2, 64);

		printf(".");
		fflush(stdout);
	}

	printf(" done.\n");
}

int
main(void)
{
	test_gfb127();
	test_gfb254();
	test_gls254_encode_decode();
	test_gls254_ops();
	test_gls254_window();
	test_hash_to_point();
	test_scalar();
	test_mul();
	test_mulgen();
	test_key();
	test_sign();
	test_ECDH();
	test_raw_ECDH();
	return 0;
}
