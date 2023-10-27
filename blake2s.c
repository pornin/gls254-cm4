/*
 * BLAKE2s implementation. If possible, it will use SSE2 or even AVX2
 * intrisics (if the target architecture is x86 and targeting a CPU that
 * supports them); otherwise, a plain portable implementation is used.
 */

/* ====================================================================== */

#include <stdint.h>
#include <string.h>

#include "blake2.h"

/*
 * Auto-enable SSE2 and/or AVX2 depending on defined flags. Also enable
 * SSE2 on 64-bit x86 (since it's part of the ABI).
 */
#ifndef BLAKE2_AVX2
#if defined __AVX2__
#define BLAKE2_AVX2   1
#else
#define BLAKE2_AVX2   0
#endif
#endif
#ifndef BLAKE2_SSE2
#if BLAKE2_AVX2 || defined __SSE2__ \
	|| (defined _M_IX86_FP && _M_IX86_FP >= 2) \
	|| defined __x86_64__ || defined _M_X64
#define BLAKE2_SSE2   1
#else
#define BLAKE2_SSE2   0
#endif
#endif

#if BLAKE2_SSE2 || BLAKE2_AVX2
/*
 * This implementation uses SSE2 and/or AVX2 intrinsics.
 */
#include <immintrin.h>
#ifndef BLAKE2_LE
#define BLAKE2_LE   1
#endif
#ifndef BLAKE2_UNALIGNED
#define BLAKE2_UNALIGNED   1
#endif
#if defined __GNUC__
#define TARGET_SSE2    __attribute__((target("sse2")))
#define ALIGNED_SSE2   __attribute__((aligned(16)))
#if BLAKE2_AVX2
#define TARGET_AVX2    __attribute__((target("avx2")))
#define ALIGNED_AVX2   __attribute__((aligned(32)))
#else
#define ALIGNED_AVX2   ALIGNED_SSE2
#endif
#elif defined _MSC_VER && _MSC_VER
#pragma warning( disable : 4752 )
#endif
#endif

#ifndef TARGET_SSE2
#define TARGET_SSE2
#endif
#ifndef ALIGNED_SSE2
#define ALIGNED_SSE2
#endif
#ifndef TARGET_AVX2
#define TARGET_AVX2
#endif
#ifndef ALIGNED_AVX2
#define ALIGNED_AVX2
#endif

/*
 * Disable warning on applying unary minus on an unsigned type.
 */
#if defined _MSC_VER && _MSC_VER
#pragma warning( disable : 4146 )
#pragma warning( disable : 4244 )
#pragma warning( disable : 4267 )
#pragma warning( disable : 4334 )
#endif

/*
 * Auto-detect endianness and support of unaligned accesses.
 */
#if defined __i386__ || defined _M_IX86 \
	|| defined __x86_64__ || defined _M_X64 \
	|| (defined _ARCH_PWR8 \
		&& (defined __LITTLE_ENDIAN || defined __LITTLE_ENDIAN__))

#ifndef BLAKE2_LE
#define BLAKE2_LE   1
#endif
#ifndef BLAKE2_UNALIGNED
#define BLAKE2_UNALIGNED   1
#endif

#elif (defined __LITTLE_ENDIAN && __LITTLE_ENDIAN__) \
	|| (defined __BYTE_ORDER__ && defined __ORDER_LITTLE_ENDIAN__ \
		&& __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)

#ifndef BLAKE2_LE
#define BLAKE2_LE   1
#endif
#ifndef BLAKE2_UNALIGNED
#define BLAKE2_UNALIGNED   0
#endif

#else

#ifndef BLAKE2_LE
#define BLAKE2_LE   0
#endif
#ifndef BLAKE2_UNALIGNED
#define BLAKE2_UNALIGNED   0
#endif

#endif

/*
 * MSVC 2015 does not known the C99 keyword 'restrict'.
 */
#if defined _MSC_VER && _MSC_VER
#ifndef restrict
#define restrict   __restrict
#endif
#endif

#if !BLAKE2_LE

static inline uint32_t
dec32le(const void *src)
{
	const uint8_t *buf = src;

	return (uint32_t)src[0]
		| ((uint32_t)src[1] << 8)
		| ((uint32_t)src[2] << 16)
		| ((uint32_t)src[3] << 24);
}

static inline void
enc32le(void *dst, uint32_t x)
{
	uint8_t *buf = dst;

	dst[0] = (uint8_t)x;
	dst[1] = (uint8_t)(x >> 8);
	dst[2] = (uint8_t)(x >> 16);
	dst[3] = (uint8_t)(x >> 24);
}

#endif

ALIGNED_AVX2
static const uint32_t IV[] = {
	0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
	0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19
};

#if BLAKE2_AVX2

TARGET_AVX2
static void
process_block(uint32_t *h, const uint8_t *data, uint64_t t, int f)
{
	__m128i xh0, xh1, xv0, xv1, xv2, xv3;
	__m128i xm0, xm1, xm2, xm3, xn0, xn1, xn2, xn3;
	__m128i xt0, xt1, xt2, xt3, xt4, xt5, xt6, xt7, xt8, xt9;
	__m128i xror8, xror16;

	xror8 = _mm_setr_epi8(
		1, 2, 3, 0, 5, 6, 7, 4,
		9, 10, 11, 8, 13, 14, 15, 12);
	xror16 = _mm_setr_epi8(
		2, 3, 0, 1, 6, 7, 4, 5,
		10, 11, 8, 9, 14, 15, 12, 13);

	/* Initialize state. */
	xh0 = _mm_loadu_si128((const void *)(h + 0));
	xh1 = _mm_loadu_si128((const void *)(h + 4));
	xv0 = xh0;
	xv1 = xh1;
	xv2 = _mm_loadu_si128((const void *)(IV + 0));
	xv3 = _mm_loadu_si128((const void *)(IV + 4));
	xv3 = _mm_xor_si128(xv3, _mm_setr_epi32(
		(int32_t)(uint32_t)t, (int32_t)(uint32_t)(t >> 32),
		-f, 0));

	/* Load data and move it into the proper order for the first round:
	     xm0:  0  2  4  6
	     xm1:  1  3  5  7
	     xm2:  8 10 12 14
	     xm3:  9 11 13 15 */
	xm0 = _mm_loadu_si128((const void *)(data +  0));
	xm1 = _mm_loadu_si128((const void *)(data + 16));
	xm2 = _mm_loadu_si128((const void *)(data + 32));
	xm3 = _mm_loadu_si128((const void *)(data + 48));

	xn0 = _mm_shuffle_epi32(xm0, 0xD8);
	xn1 = _mm_shuffle_epi32(xm1, 0xD8);
	xm0 = _mm_unpacklo_epi64(xn0, xn1);
	xm1 = _mm_unpackhi_epi64(xn0, xn1);

	xn2 = _mm_shuffle_epi32(xm2, 0xD8);
	xn3 = _mm_shuffle_epi32(xm3, 0xD8);
	xm2 = _mm_unpacklo_epi64(xn2, xn3);
	xm3 = _mm_unpackhi_epi64(xn2, xn3);

#define G4(xx, xy)   do { \
		__m128i xtg; \
		xv0 = _mm_add_epi32(xv0, _mm_add_epi32(xv1, xx)); \
		xv3 = _mm_shuffle_epi8(_mm_xor_si128(xv0, xv3), xror16); \
		xv2 = _mm_add_epi32(xv2, xv3); \
		xtg = _mm_xor_si128(xv1, xv2); \
		xv1 = _mm_or_si128( \
			_mm_srli_epi32(xtg, 12), _mm_slli_epi32(xtg, 20)); \
		xv0 = _mm_add_epi32(xv0, _mm_add_epi32(xv1, xy)); \
		xv3 = _mm_shuffle_epi8(_mm_xor_si128(xv0, xv3), xror8); \
		xv2 = _mm_add_epi32(xv2, xv3); \
		xtg = _mm_xor_si128(xv1, xv2); \
		xv1 = _mm_or_si128( \
			_mm_srli_epi32(xtg, 7), _mm_slli_epi32(xtg, 25)); \
	} while (0)

#define ROUND(i0, i1, i2, i3)   do { \
		G4(i0, i1); \
		xv1 = _mm_shuffle_epi32(xv1, 0x39); \
		xv2 = _mm_shuffle_epi32(xv2, 0x4E); \
		xv3 = _mm_shuffle_epi32(xv3, 0x93); \
		G4(i2, i3); \
		xv1 = _mm_shuffle_epi32(xv1, 0x93); \
		xv2 = _mm_shuffle_epi32(xv2, 0x4E); \
		xv3 = _mm_shuffle_epi32(xv3, 0x39); \
	} while (0)

	/* round 0 */
	ROUND(xm0, xm1, xm2, xm3);

	/* round 1 */
	xt0 = _mm_shuffle_epi32(xm0, 0x00);
	xt1 = _mm_shuffle_epi32(xm0, 0xC8);
	xt2 = _mm_shuffle_epi32(xm1, 0x70);
	xt3 = _mm_shuffle_epi32(xm1, 0x80);
	xt4 = _mm_shuffle_epi32(xm2, 0x01);
	xt5 = _mm_shuffle_epi32(xm2, 0x02);
	xt6 = _mm_shuffle_epi32(xm2, 0x03);
	xt7 = _mm_shuffle_epi32(xm3, 0x80);
	xt8 = _mm_shuffle_epi32(xm3, 0x10);
	xt9 = _mm_shuffle_epi32(xm3, 0x30);
	xn0 = _mm_blend_epi32(
		_mm_blend_epi32(xt6, xt1, 0x02),
		xt7, 0x0C);
	xn1 = _mm_blend_epi32(
		_mm_blend_epi32(xt4, xt9, 0x04),
		xt1, 0x08);
	xn2 = _mm_blend_epi32(
		_mm_blend_epi32(xt3, xt0, 0x02),
		xt8, 0x04);
	xn3 = _mm_blend_epi32(
		_mm_blend_epi32(xt5, xm0, 0x02),
		xt2, 0x0C);
	ROUND(xn0, xn1, xn2, xn3);

	/* round 2 */
	xt0 = _mm_shuffle_epi32(xn0, 0x40);
	xt1 = _mm_shuffle_epi32(xn0, 0x80);
	xt2 = _mm_shuffle_epi32(xn1, 0x80);
	xt3 = _mm_shuffle_epi32(xn1, 0x0D);
	xt4 = _mm_shuffle_epi32(xn2, 0x04);
	xt5 = _mm_shuffle_epi32(xn2, 0x32);
	xt6 = _mm_shuffle_epi32(xn3, 0x10);
	xt7 = _mm_shuffle_epi32(xn3, 0x2C);
	xm0 = _mm_blend_epi32(
		_mm_blend_epi32(xt5, xt6, 0x02),
		xt2, 0x08);
	xm1 = _mm_blend_epi32(
		_mm_blend_epi32(xt3, xt4, 0x02),
		_mm_blend_epi32(xt6, xn0, 0x08), 0x0C);
	xm2 = _mm_blend_epi32(
		_mm_blend_epi32(xt2, xt7, 0x06),
		xt1, 0x08);
	xm3 = _mm_blend_epi32(
		_mm_blend_epi32(xt0, xt3, 0x02),
		xt4, 0x04);
	ROUND(xm0, xm1, xm2, xm3);

	/* round 3 */
	xt0 = _mm_shuffle_epi32(xm0, 0x10);
	xt1 = _mm_shuffle_epi32(xm0, 0xC8);
	xt2 = _mm_shuffle_epi32(xm1, 0x10);
	xt3 = _mm_shuffle_epi32(xm1, 0x32);
	xt4 = _mm_shuffle_epi32(xm2, 0x03);
	xt5 = _mm_shuffle_epi32(xm2, 0x06);
	xt6 = _mm_shuffle_epi32(xm3, 0x39);
	xn0 = _mm_blend_epi32(
		_mm_blend_epi32(xt5, xt3, 0x04),
		xt0, 0x08);
	xn1 = _mm_blend_epi32(
		_mm_blend_epi32(xt4, xt6, 0x0A),
		xt0, 0x04);
	xn2 = _mm_blend_epi32(
		_mm_blend_epi32(xt3, xt1, 0x0A),
		xt6, 0x04);
	xn3 = _mm_blend_epi32(
		_mm_blend_epi32(xt6, xt4, 0x02),
		xt2, 0x0C);
	ROUND(xn0, xn1, xn2, xn3);

	/* round 4 */
	xt0 = _mm_shuffle_epi32(xn0, 0x80);
	xt1 = _mm_shuffle_epi32(xn0, 0x4C);
	xt2 = _mm_shuffle_epi32(xn1, 0x09);
	xt3 = _mm_shuffle_epi32(xn1, 0x03);
	xt4 = _mm_shuffle_epi32(xn2, 0x04);
	xt5 = _mm_shuffle_epi32(xn3, 0x40);
	xt6 = _mm_shuffle_epi32(xn3, 0x32);
	xm0 = _mm_blend_epi32(
		_mm_blend_epi32(xn1, xt4, 0x06),
		xt5, 0x08);
	xm1 = _mm_blend_epi32(
		_mm_blend_epi32(xt6, xt0, 0x02),
		xn2, 0x0C);
	xm2 = _mm_blend_epi32(
		_mm_blend_epi32(xt3, xt1, 0x0A),
		xt5, 0x04);
	xm3 = _mm_blend_epi32(
		_mm_blend_epi32(xt2, xt6, 0x04),
		xt0, 0x08);
	ROUND(xm0, xm1, xm2, xm3);

	/* round 5 */
	xt0 = _mm_shuffle_epi32(xm0, 0x04);
	xt1 = _mm_shuffle_epi32(xm0, 0x0E);
	xt2 = _mm_shuffle_epi32(xm1, 0x04);
	xt3 = _mm_shuffle_epi32(xm1, 0x32);
	xt4 = _mm_shuffle_epi32(xm2, 0x08);
	xt5 = _mm_shuffle_epi32(xm2, 0xD0);
	xt6 = _mm_shuffle_epi32(xm3, 0x01);
	xt7 = _mm_shuffle_epi32(xm3, 0x83);
	xn0 = _mm_blend_epi32(
		_mm_blend_epi32(xt1, xt4, 0x02),
		_mm_blend_epi32(xt2, xt7, 0x08), 0x0C);
	xn1 = _mm_blend_epi32(
		_mm_blend_epi32(xt6, xt1, 0x02),
		xt5, 0x0C);
	xn2 = _mm_blend_epi32(
		_mm_blend_epi32(xt3, xt2, 0x02),
		xt6, 0x08);
	xn3 = _mm_blend_epi32(
		_mm_blend_epi32(xt7, xt0, 0x0A),
		xt4, 0x04);
	ROUND(xn0, xn1, xn2, xn3);

	/* round 6 */
	xt0 = _mm_shuffle_epi32(xn0, 0xC6);
	xt1 = _mm_shuffle_epi32(xn1, 0x40);
	xt2 = _mm_shuffle_epi32(xn1, 0x8C);
	xt3 = _mm_shuffle_epi32(xn2, 0x09);
	xt4 = _mm_shuffle_epi32(xn2, 0x0C);
	xt5 = _mm_shuffle_epi32(xn3, 0x01);
	xt6 = _mm_shuffle_epi32(xn3, 0x30);
	xm0 = _mm_blend_epi32(
		_mm_blend_epi32(xt1, xt4, 0x0A),
		xn3, 0x04);
	xm1 = _mm_blend_epi32(
		_mm_blend_epi32(xt5, xt3, 0x02),
		xt1, 0x08);
	xm2 = _mm_blend_epi32(xt0, xt6, 0x04);
	xm3 = _mm_blend_epi32(
		_mm_blend_epi32(xt3, xt2, 0x0A),
		xt0, 0x04);
	ROUND(xm0, xm1, xm2, xm3);

	/* round 7 */
	xt0 = _mm_shuffle_epi32(xm0, 0x0C);
	xt1 = _mm_shuffle_epi32(xm0, 0x18);
	xt2 = _mm_shuffle_epi32(xm1, 0xC2);
	xt3 = _mm_shuffle_epi32(xm2, 0x10);
	xt4 = _mm_shuffle_epi32(xm2, 0xB0);
	xt5 = _mm_shuffle_epi32(xm3, 0x40);
	xt6 = _mm_shuffle_epi32(xm3, 0x83);
	xn0 = _mm_blend_epi32(
		_mm_blend_epi32(xt2, xt5, 0x0A),
		xt0, 0x04);
	xn1 = _mm_blend_epi32(
		_mm_blend_epi32(xt6, xt1, 0x06),
		xt4, 0x08);
	xn2 = _mm_blend_epi32(
		_mm_blend_epi32(xm1, xt4, 0x04),
		xt6, 0x08);
	xn3 = _mm_blend_epi32(
		_mm_blend_epi32(xt3, xt0, 0x02),
		xt2, 0x08);
	ROUND(xn0, xn1, xn2, xn3);

	/* round 8 */
	xt0 = _mm_shuffle_epi32(xn0, 0x02);
	xt1 = _mm_shuffle_epi32(xn0, 0x34);
	xt2 = _mm_shuffle_epi32(xn1, 0x0C);
	xt3 = _mm_shuffle_epi32(xn2, 0x03);
	xt4 = _mm_shuffle_epi32(xn2, 0x81);
	xt5 = _mm_shuffle_epi32(xn3, 0x02);
	xt6 = _mm_shuffle_epi32(xn3, 0xD0);
	xm0 = _mm_blend_epi32(
		_mm_blend_epi32(xt5, xn1, 0x02),
		xt2, 0x04);
	xm1 = _mm_blend_epi32(
		_mm_blend_epi32(xt4, xt2, 0x02),
		xt1, 0x04);
	xm2 = _mm_blend_epi32(
		_mm_blend_epi32(xt0, xn1, 0x04),
		xt6, 0x08);
	xm3 = _mm_blend_epi32(
		_mm_blend_epi32(xt3, xt1, 0x02),
		xt6, 0x04);
	ROUND(xm0, xm1, xm2, xm3);

	/* round 9 */
	xt0 = _mm_shuffle_epi32(xm0, 0xC6);
	xt1 = _mm_shuffle_epi32(xm1, 0x2C);
	xt2 = _mm_shuffle_epi32(xm2, 0x40);
	xt3 = _mm_shuffle_epi32(xm2, 0x83);
	xt4 = _mm_shuffle_epi32(xm3, 0xD8);
	xn0 = _mm_blend_epi32(
		_mm_blend_epi32(xt3, xt1, 0x02),
		xt4, 0x04);
	xn1 = _mm_blend_epi32(xt4, xt0, 0x04);
	xn2 = _mm_blend_epi32(
		_mm_blend_epi32(xm1, xt1, 0x04),
		xt2, 0x08);
	xn3 = _mm_blend_epi32(xt0, xt2, 0x04);
	ROUND(xn0, xn1, xn2, xn3);

#undef G4
#undef ROUND

	xh0 = _mm_xor_si128(xh0, _mm_xor_si128(xv0, xv2));
	xh1 = _mm_xor_si128(xh1, _mm_xor_si128(xv1, xv3));
	_mm_storeu_si128((void *)(h + 0), xh0);
	_mm_storeu_si128((void *)(h + 4), xh1);
}

#elif BLAKE2_SSE2

TARGET_SSE2
static void
process_block(uint32_t *h, const uint8_t *data, uint64_t t, int f)
{
	__m128i xh0, xh1, xv0, xv1, xv2, xv3;
	__m128i xm0, xm1, xm2, xm3, xn0, xn1, xn2, xn3;
	__m128i xt0, xt1, xt2, xt3, xt4, xt5, xt6, xt7, xt8, xt9;
	__m128i xz1, xz2, xz3, xz4, xz5, xz6, xz7;

	/* Initialize state. */
	xh0 = _mm_loadu_si128((const void *)(h + 0));
	xh1 = _mm_loadu_si128((const void *)(h + 4));
	xv0 = xh0;
	xv1 = xh1;
	xv2 = _mm_loadu_si128((const void *)(IV + 0));
	xv3 = _mm_loadu_si128((const void *)(IV + 4));
	xv3 = _mm_xor_si128(xv3, _mm_setr_epi32(
		(int32_t)(uint32_t)t, (int32_t)(uint32_t)(t >> 32),
		-f, 0));

	/* Load data and move it into the proper order for the first round:
	     xm0:  0  2  4  6
	     xm1:  1  3  5  7
	     xm2:  8 10 12 14
	     xm3:  9 11 13 15 */
	xm0 = _mm_loadu_si128((const void *)(data +  0));
	xm1 = _mm_loadu_si128((const void *)(data + 16));
	xm2 = _mm_loadu_si128((const void *)(data + 32));
	xm3 = _mm_loadu_si128((const void *)(data + 48));

	xn0 = _mm_shuffle_epi32(xm0, 0xD8);
	xn1 = _mm_shuffle_epi32(xm1, 0xD8);
	xm0 = _mm_unpacklo_epi64(xn0, xn1);
	xm1 = _mm_unpackhi_epi64(xn0, xn1);

	xn2 = _mm_shuffle_epi32(xm2, 0xD8);
	xn3 = _mm_shuffle_epi32(xm3, 0xD8);
	xm2 = _mm_unpacklo_epi64(xn2, xn3);
	xm3 = _mm_unpackhi_epi64(xn2, xn3);

#define G4(xx, xy)   do { \
		__m128i xtg; \
		xv0 = _mm_add_epi32(xv0, _mm_add_epi32(xv1, xx)); \
		xtg = _mm_xor_si128(xv0, xv3); \
		xv3 = _mm_or_si128( \
			_mm_srli_epi32(xtg, 16), _mm_slli_epi32(xtg, 16)); \
		xv2 = _mm_add_epi32(xv2, xv3); \
		xtg = _mm_xor_si128(xv1, xv2); \
		xv1 = _mm_or_si128( \
			_mm_srli_epi32(xtg, 12), _mm_slli_epi32(xtg, 20)); \
		xv0 = _mm_add_epi32(xv0, _mm_add_epi32(xv1, xy)); \
		xtg = _mm_xor_si128(xv0, xv3); \
		xv3 = _mm_or_si128( \
			_mm_srli_epi32(xtg, 8), _mm_slli_epi32(xtg, 24)); \
		xv2 = _mm_add_epi32(xv2, xv3); \
		xtg = _mm_xor_si128(xv1, xv2); \
		xv1 = _mm_or_si128( \
			_mm_srli_epi32(xtg, 7), _mm_slli_epi32(xtg, 25)); \
	} while (0)

#define ROUND(i0, i1, i2, i3)   do { \
		G4(i0, i1); \
		xv1 = _mm_shuffle_epi32(xv1, 0x39); \
		xv2 = _mm_shuffle_epi32(xv2, 0x4E); \
		xv3 = _mm_shuffle_epi32(xv3, 0x93); \
		G4(i2, i3); \
		xv1 = _mm_shuffle_epi32(xv1, 0x93); \
		xv2 = _mm_shuffle_epi32(xv2, 0x4E); \
		xv3 = _mm_shuffle_epi32(xv3, 0x39); \
	} while (0)

	xz1 = _mm_setr_epi32(-1, 0, 0, 0);
	xz2 = _mm_setr_epi32(0, -1, 0, 0);
	xz3 = _mm_setr_epi32(-1, -1, 0, 0);
	xz4 = _mm_setr_epi32(0, 0, -1, 0);
	xz5 = _mm_setr_epi32(-1, 0, -1, 0);
	xz6 = _mm_setr_epi32(0, -1, -1, 0);
	xz7 = _mm_setr_epi32(-1, -1, -1, 0);

	/* round 0 */
	ROUND(xm0, xm1, xm2, xm3);

	/* round 1 */
	xt0 = _mm_shuffle_epi32(xm0, 0x00);
	xt1 = _mm_shuffle_epi32(xm0, 0xC8);
	xt2 = _mm_shuffle_epi32(xm1, 0x70);
	xt3 = _mm_shuffle_epi32(xm1, 0x80);
	xt4 = _mm_shuffle_epi32(xm2, 0x01);
	xt5 = _mm_shuffle_epi32(xm2, 0x02);
	xt6 = _mm_shuffle_epi32(xm2, 0x03);
	xt7 = _mm_shuffle_epi32(xm3, 0x80);
	xt8 = _mm_shuffle_epi32(xm3, 0x10);
	xt9 = _mm_shuffle_epi32(xm3, 0x30);
	xn0 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz1, xt6), _mm_and_si128(xz2, xt1)),
		_mm_andnot_si128(xz3, xt7));
	xn1 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz3, xt4), _mm_and_si128(xz4, xt9)),
		_mm_andnot_si128(xz7, xt1));
	xn2 = _mm_or_si128(
		_mm_or_si128(_mm_andnot_si128(xz6, xt3), _mm_and_si128(xz2, xt0)),
		_mm_and_si128(xz4, xt8));
	xn3 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz1, xt5), _mm_and_si128(xz2, xm0)),
		_mm_andnot_si128(xz3, xt2));
	ROUND(xn0, xn1, xn2, xn3);

	/* round 2 */
	xt0 = _mm_shuffle_epi32(xn0, 0x40);
	xt1 = _mm_shuffle_epi32(xn0, 0x80);
	xt2 = _mm_shuffle_epi32(xn1, 0x80);
	xt3 = _mm_shuffle_epi32(xn1, 0x0D);
	xt4 = _mm_shuffle_epi32(xn2, 0x04);
	xt5 = _mm_shuffle_epi32(xn2, 0x32);
	xt6 = _mm_shuffle_epi32(xn3, 0x10);
	xt7 = _mm_shuffle_epi32(xn3, 0x2C);
	xm0 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz5, xt5), _mm_and_si128(xz2, xt6)),
		_mm_andnot_si128(xz7, xt2));
	xm1 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz1, xt3), _mm_and_si128(xz2, xt4)),
		_mm_or_si128(_mm_and_si128(xz4, xt6), _mm_andnot_si128(xz7, xn0)));
	xm2 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz1, xt2), _mm_and_si128(xz6, xt7)),
		_mm_andnot_si128(xz7, xt1));
	xm3 = _mm_or_si128(
		_mm_or_si128(_mm_andnot_si128(xz6, xt0), _mm_and_si128(xz2, xt3)),
		_mm_and_si128(xz4, xt4));
	ROUND(xm0, xm1, xm2, xm3);

	/* round 3 */
	xt0 = _mm_shuffle_epi32(xm0, 0x10);
	xt1 = _mm_shuffle_epi32(xm0, 0xC8);
	xt2 = _mm_shuffle_epi32(xm1, 0x10);
	xt3 = _mm_shuffle_epi32(xm1, 0x32);
	xt4 = _mm_shuffle_epi32(xm2, 0x03);
	xt5 = _mm_shuffle_epi32(xm2, 0x06);
	xt6 = _mm_shuffle_epi32(xm3, 0x39);
	xn0 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz3, xt5), _mm_and_si128(xz4, xt3)),
		_mm_andnot_si128(xz7, xt0));
	xn1 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz1, xt4), _mm_andnot_si128(xz5, xt6)),
		_mm_and_si128(xz4, xt0));
	xn2 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz1, xt3), _mm_andnot_si128(xz5, xt1)),
		_mm_and_si128(xz4, xt6));
	xn3 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz1, xt6), _mm_and_si128(xz2, xt4)),
		_mm_andnot_si128(xz3, xt2));
	ROUND(xn0, xn1, xn2, xn3);

	/* round 4 */
	xt0 = _mm_shuffle_epi32(xn0, 0x80);
	xt1 = _mm_shuffle_epi32(xn0, 0x4C);
	xt2 = _mm_shuffle_epi32(xn1, 0x09);
	xt3 = _mm_shuffle_epi32(xn1, 0x03);
	xt4 = _mm_shuffle_epi32(xn2, 0x04);
	xt5 = _mm_shuffle_epi32(xn3, 0x40);
	xt6 = _mm_shuffle_epi32(xn3, 0x32);
	xm0 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz1, xn1), _mm_and_si128(xz6, xt4)),
		_mm_andnot_si128(xz7, xt5));
	xm1 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz1, xt6), _mm_and_si128(xz2, xt0)),
		_mm_andnot_si128(xz3, xn2));
	xm2 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz1, xt3), _mm_andnot_si128(xz5, xt1)),
		_mm_and_si128(xz4, xt5));
	xm3 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz3, xt2), _mm_and_si128(xz4, xt6)),
		_mm_andnot_si128(xz7, xt0));
	ROUND(xm0, xm1, xm2, xm3);

	/* round 5 */
	xt0 = _mm_shuffle_epi32(xm0, 0x04);
	xt1 = _mm_shuffle_epi32(xm0, 0x0E);
	xt2 = _mm_shuffle_epi32(xm1, 0x04);
	xt3 = _mm_shuffle_epi32(xm1, 0x32);
	xt4 = _mm_shuffle_epi32(xm2, 0x08);
	xt5 = _mm_shuffle_epi32(xm2, 0xD0);
	xt6 = _mm_shuffle_epi32(xm3, 0x01);
	xt7 = _mm_shuffle_epi32(xm3, 0x83);
	xn0 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz1, xt1), _mm_and_si128(xz2, xt4)),
		_mm_or_si128(_mm_and_si128(xz4, xt2), _mm_andnot_si128(xz7, xt7)));
	xn1 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz1, xt6), _mm_and_si128(xz2, xt1)),
		_mm_andnot_si128(xz3, xt5));
	xn2 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz5, xt3), _mm_and_si128(xz2, xt2)),
		_mm_andnot_si128(xz7, xt6));
	xn3 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz1, xt7), _mm_andnot_si128(xz5, xt0)),
		_mm_and_si128(xz4, xt4));
	ROUND(xn0, xn1, xn2, xn3);

	/* round 6 */
	xt0 = _mm_shuffle_epi32(xn0, 0xC6);
	xt1 = _mm_shuffle_epi32(xn1, 0x40);
	xt2 = _mm_shuffle_epi32(xn1, 0x8C);
	xt3 = _mm_shuffle_epi32(xn2, 0x09);
	xt4 = _mm_shuffle_epi32(xn2, 0x0C);
	xt5 = _mm_shuffle_epi32(xn3, 0x01);
	xt6 = _mm_shuffle_epi32(xn3, 0x30);
	xm0 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz1, xt1), _mm_andnot_si128(xz5, xt4)),
		_mm_and_si128(xz4, xn3));
	xm1 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz5, xt5), _mm_and_si128(xz2, xt3)),
		_mm_andnot_si128(xz7, xt1));
	xm2 = _mm_or_si128(_mm_andnot_si128(xz4, xt0), _mm_and_si128(xz4, xt6));
	xm3 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz1, xt3), _mm_andnot_si128(xz5, xt2)),
		_mm_and_si128(xz4, xt0));
	ROUND(xm0, xm1, xm2, xm3);

	/* round 7 */
	xt0 = _mm_shuffle_epi32(xm0, 0x0C);
	xt1 = _mm_shuffle_epi32(xm0, 0x18);
	xt2 = _mm_shuffle_epi32(xm1, 0xC2);
	xt3 = _mm_shuffle_epi32(xm2, 0x10);
	xt4 = _mm_shuffle_epi32(xm2, 0xB0);
	xt5 = _mm_shuffle_epi32(xm3, 0x40);
	xt6 = _mm_shuffle_epi32(xm3, 0x83);
	xn0 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz1, xt2), _mm_andnot_si128(xz5, xt5)),
		_mm_and_si128(xz4, xt0));
	xn1 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz1, xt6), _mm_and_si128(xz6, xt1)),
		_mm_andnot_si128(xz7, xt4));
	xn2 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz3, xm1), _mm_and_si128(xz4, xt4)),
		_mm_andnot_si128(xz7, xt6));
	xn3 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz5, xt3), _mm_and_si128(xz2, xt0)),
		_mm_andnot_si128(xz7, xt2));
	ROUND(xn0, xn1, xn2, xn3);

	/* round 8 */
	xt0 = _mm_shuffle_epi32(xn0, 0x02);
	xt1 = _mm_shuffle_epi32(xn0, 0x34);
	xt2 = _mm_shuffle_epi32(xn1, 0x0C);
	xt3 = _mm_shuffle_epi32(xn2, 0x03);
	xt4 = _mm_shuffle_epi32(xn2, 0x81);
	xt5 = _mm_shuffle_epi32(xn3, 0x02);
	xt6 = _mm_shuffle_epi32(xn3, 0xD0);
	xm0 = _mm_or_si128(
		_mm_or_si128(_mm_andnot_si128(xz6, xt5), _mm_and_si128(xz2, xn1)),
		_mm_and_si128(xz4, xt2));
	xm1 = _mm_or_si128(
		_mm_or_si128(_mm_andnot_si128(xz6, xt4), _mm_and_si128(xz2, xt2)),
		_mm_and_si128(xz4, xt1));
	xm2 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz3, xt0), _mm_and_si128(xz4, xn1)),
		_mm_andnot_si128(xz7, xt6));
	xm3 = _mm_or_si128(
		_mm_or_si128(_mm_andnot_si128(xz6, xt3), _mm_and_si128(xz2, xt1)),
		_mm_and_si128(xz4, xt6));
	ROUND(xm0, xm1, xm2, xm3);

	/* round 9 */
	xt0 = _mm_shuffle_epi32(xm0, 0xC6);
	xt1 = _mm_shuffle_epi32(xm1, 0x2C);
	xt2 = _mm_shuffle_epi32(xm2, 0x40);
	xt3 = _mm_shuffle_epi32(xm2, 0x83);
	xt4 = _mm_shuffle_epi32(xm3, 0xD8);
	xn0 = _mm_or_si128(
		_mm_or_si128(_mm_andnot_si128(xz6, xt3), _mm_and_si128(xz2, xt1)),
		_mm_and_si128(xz4, xt4));
	xn1 = _mm_or_si128(_mm_andnot_si128(xz4, xt4), _mm_and_si128(xz4, xt0));
	xn2 = _mm_or_si128(
		_mm_or_si128(_mm_and_si128(xz3, xm1), _mm_and_si128(xz4, xt1)),
		_mm_andnot_si128(xz7, xt2));
	xn3 = _mm_or_si128(_mm_andnot_si128(xz4, xt0), _mm_and_si128(xz4, xt2));
	ROUND(xn0, xn1, xn2, xn3);

#undef G4
#undef ROUND

	xh0 = _mm_xor_si128(xh0, _mm_xor_si128(xv0, xv2));
	xh1 = _mm_xor_si128(xh1, _mm_xor_si128(xv1, xv3));
	_mm_storeu_si128((void *)(h + 0), xh0);
	_mm_storeu_si128((void *)(h + 4), xh1);
}

#else

static void
process_block(uint32_t *h, const uint8_t *data, uint64_t t, int f)
{
	uint32_t v[16], m[16];
	int i;

	memcpy(v, h, 8 * sizeof(uint32_t));
	memcpy(v + 8, IV, sizeof IV);
	v[12] ^= (uint32_t)t;
	v[13] ^= (uint32_t)(t >> 32);
	if (f) {
		v[14] = ~v[14];
	}

#if BLAKE2_LE
	memcpy(m, data, sizeof m);
#else
	for (i = 0; i < 16; i ++) {
		m[i] = dec32le(data + (i << 2));
	}
#endif

#define ROR(x, n)   (((x) << (32 - (n))) | ((x) >> (n)))

#define G(a, b, c, d, x, y)   do { \
		v[a] += v[b] + (x); \
		v[d] = ROR(v[d] ^ v[a], 16); \
		v[c] += v[d]; \
		v[b] = ROR(v[b] ^ v[c], 12); \
		v[a] += v[b] + (y); \
		v[d] = ROR(v[d] ^ v[a], 8); \
		v[c] += v[d]; \
		v[b] = ROR(v[b] ^ v[c], 7); \
	} while (0)

#define ROUND(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, sA, sB, sC, sD, sE, sF) \
	do { \
		G(0, 4,  8, 12, m[s0], m[s1]); \
		G(1, 5,  9, 13, m[s2], m[s3]); \
		G(2, 6, 10, 14, m[s4], m[s5]); \
		G(3, 7, 11, 15, m[s6], m[s7]); \
		G(0, 5, 10, 15, m[s8], m[s9]); \
		G(1, 6, 11, 12, m[sA], m[sB]); \
		G(2, 7,  8, 13, m[sC], m[sD]); \
		G(3, 4,  9, 14, m[sE], m[sF]); \
	} while (0)

	ROUND( 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15);
	ROUND(14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3);
	ROUND(11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4);
	ROUND( 7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8);
	ROUND( 9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13);
	ROUND( 2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9);
	ROUND(12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11);
	ROUND(13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10);
	ROUND( 6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5);
	ROUND(10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0);

#undef ROR
#undef G
#undef ROUND

	for (i = 0; i < 8; i ++) {
		h[i] ^= v[i] ^ v[i + 8];
	}
}

#endif

/*
 * State rules:
 *
 *   buf    buffered data
 *   h      current state
 *   ctr    number of bytes injected so far
 *
 * Initially, ctr == 0 and h contains the XOR of IV and parameter block;
 * buf[] is empty. For any ctr > 0, buf[] is non-empty; it might contain
 * a full block worth of data (processing of the block is delayed until
 * we know whether this is the final block or not).
 *
 * If a key is injected, then it counts as a first full block.
 */

/* see blake2.h */
void
blake2s_init(blake2s_context *bc, size_t out_len)
{
	memcpy(bc->h, IV, sizeof bc->h);
	bc->h[0] ^= 0x01010000 ^ (uint32_t)out_len;
	bc->ctr = 0;
	bc->out_len = out_len;
}

/* see blake2.h */
void
blake2s_init_key(blake2s_context *bc, size_t out_len,
	const void *key, size_t key_len)
{
	blake2s_init(bc, out_len);
	if (key_len > 0) {
		bc->h[0] ^= (uint32_t)key_len << 8;
		memcpy(bc->buf, key, key_len);
		memset(bc->buf + key_len, 0, (sizeof bc->buf) - key_len);
		bc->ctr = sizeof bc->buf;
	}
}

/* see blake2.h */
void
blake2s_update(blake2s_context *bc, const void *data, size_t len)
{
	uint64_t ctr;
	size_t p;

	/* Special case: if no input data, return immediately. */
	if (len == 0) {
		return;
	}

	ctr = bc->ctr;

	/* First complete the current block, if not already full. */
	p = (size_t)ctr & ((sizeof bc->buf) - 1);
	if (ctr == 0 || p != 0) {
		/* buffer is not full */
		size_t clen;

		clen = sizeof bc->buf - p;
		if (clen >= len) {
			memcpy(bc->buf + p, data, len);
			bc->ctr = ctr + len;
			return;
		}
		memcpy(bc->buf + p, data, clen);
		ctr += clen;
		data = (const uint8_t *)data + clen;
		len -= clen;
	}

	/* Process the buffered block. */
	process_block(bc->h, bc->buf, ctr, 0);

	/* Process all subsequent full blocks, except the last. */
	while (len > sizeof bc->buf) {
		ctr += sizeof bc->buf;
		process_block(bc->h, data, ctr, 0);
		data = (const uint8_t *)data + sizeof bc->buf;
		len -= sizeof bc->buf;
	}

	/* Copy the last block (possibly partial) into the buffer. */
	memcpy(bc->buf, data, len);
	bc->ctr = ctr + len;
}

/* see blake2.h */
void
blake2s_final(blake2s_context *bc, void *dst)
{
#if !BLAKE2_LE
	int i;
	uint8_t tmp[32];
#endif
	size_t p;

	/* Pad the current block with zeros, if not full. If the
	   buffer is empty (no key, no data) then fill it with zeros
	   as well. */
	p = (size_t)bc->ctr & ((sizeof bc->buf) - 1);
	if (bc->ctr == 0 || p != 0) {
		memset(bc->buf + p, 0, (sizeof bc->buf) - p);
	}

	process_block(bc->h, bc->buf, bc->ctr, 1);
#if BLAKE2_LE
	memcpy(dst, bc->h, bc->out_len);
#else
	for (i = 0; i < 8; i ++) {
		enc32le(tmp + (i << 2), bc->h[i]);
	}
	memcpy(dst, tmp, bc->out_len);
#endif
}

/* see blake2.h */
void
blake2s(void *dst, size_t dst_len, const void *key, size_t key_len,
	const void *src, size_t src_len)
{
	blake2s_context bc;

	blake2s_init_key(&bc, dst_len, key, key_len);
	blake2s_update(&bc, src, src_len);
	blake2s_final(&bc, dst);
}
