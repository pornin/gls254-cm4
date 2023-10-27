	.syntax	unified
	.cpu	cortex-m4
	.file	"gls254-cm4.s"
	.text

@ =======================================================================
@ This file contains assembly code for the ARM Cortex M4 (ARM-v7M) to
@ implement operations on the GLS254 curve. Functions in "gfb127" work
@ with GF(2^127). Functions in "gfb254" work with GF(2^254), defined as
@ a degree-2 extension over GF(2^127). Functions in "gls254" perform
@ some operations in the curve (specifically, in the prime-order
@ subgroup in (x,s) coordinates homomorphic to the points of r-torsion
@ in the curve, with the full curve order being 2*r).
@
@ Functions documented as "Uses the external ABI" are callable from
@ other modulus and comply with the C ABI (i.e. they save the registers
@ that they should save). Other functions (with names in "inner") use
@ an internal ABI; most use the same parameter-passing convention but do
@ not save any register; a few have some other conventions.
@
@ In the comments, "Clobbers: core" means that the function does not
@ preserve the value of any of the general purpose registers (r0-r8,
@ r10-r12, and r14) and may also modify the floating-point registers
@ s0-s15 (no floating-point operation is performed, but the registers
@ are used for storing temporary variables since read/write of pairs of
@ registers from that space are faster than stack-based storage). The
@ ABI does not mandate that registers s0-s15 be preserved. For maximum
@ compatibility, register r9 is not used anywhere (in some operating
@ systems, r9 must be preserved at all times, not just across function
@ calls, possibly because it may be used in asynchronous functions such
@ as signal handlers; early iOS versions were such operating systems).
@ =======================================================================

@ =======================================================================
@ GF(2^127) FUNCTIONS
@ =======================================================================

@ =======================================================================
@ void gfb127_normalize(gfb127 *d, const gfb127 *a)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb127_normalize
	.thumb
	.thumb_func
	.type	gfb127_normalize, %function
gfb127_normalize:
	ldm	r1, { r1, r2, r3, r12 }
	eor	r1, r1, r12, lsr #31
	stm	r0!, { r1 }
	lsrs	r1, r12, #31
	eor	r2, r2, r1, lsl #31
	eor	r12, r12, r1, lsl #31
	stm	r0!, { r2, r3, r12 }
	bx	lr
	.size	gfb127_normalize, .-gfb127_normalize

@ =======================================================================
@ uint32_t gfb127_get_bit(const gfb127 *a, int k)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb127_get_bit
	.thumb
	.thumb_func
	.type	gfb127_get_bit, %function
gfb127_get_bit:
	push	{ r1, lr }
	sub	sp, #16
	movs	r1, r0
	mov	r0, sp
	bl	gfb127_normalize
	ldr	r1, [sp, #16]
	lsrs	r2, r1, #5
	ldr	r0, [sp, r2, lsl #2]
	ubfx	r1, r1, #0, #5
	lsrs	r0, r1
	ubfx	r0, r0, #0, #1
	add	sp, #20
	pop	{ pc }
	.size	gfb127_get_bit, .-gfb127_get_bit

@ =======================================================================
@ void gfb127_set_bit(const gfb127 *a, int k, uint32_t val)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb127_set_bit
	.thumb
	.thumb_func
	.type	gfb127_set_bit, %function
gfb127_set_bit:
	push	{ r0, r1, r2, lr }
	movs	r1, r0
	bl	gfb127_normalize
	pop	{ r0, r1, r2 }
	ubfx	r3, r1, #0, #5
	lsls	r2, r3
	mov	r12, #1
	lsls	r12, r3
	lsrs	r1, #5
	ldr	r3, [r0, r1, lsl #2]
	bic	r3, r3, r12
	orrs	r3, r2
	str	r3, [r0, r1, lsl #2]
	pop	{ pc }
	.size	gfb127_set_bit, .-gfb127_set_bit

@ =======================================================================
@ void gfb127_xor_bit(const gfb127 *a, int k, uint32_t val)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb127_xor_bit
	.thumb
	.thumb_func
	.type	gfb127_xor_bit, %function
gfb127_xor_bit:
	ubfx	r3, r1, #0, #5
	lsls	r2, r3
	lsrs	r1, #5
	ldr	r3, [r0, r1, lsl #2]
	eors	r3, r2
	str	r3, [r0, r1, lsl #2]
	bx	lr
	.size	gfb127_set_bit, .-gfb127_set_bit

@ =======================================================================
@ void gfb127_set_cond(gfb127 *d, const gfb127 *a, uint32_t ctl)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb127_set_cond
	.thumb
	.thumb_func
	.type	gfb127_set_cond, %function
gfb127_set_cond:
	uadd8	r2, r2, r2
	push	{ r4, r5, r6, r7 }
	mov	r12, r0
	ldm	r1!, { r4, r5, r6, r7 }
	ldm	r0, { r0, r1, r2, r3 }
	sel	r0, r4, r0
	sel	r1, r5, r1
	sel	r2, r6, r2
	sel	r3, r7, r3
	stm	r12, { r0, r1, r2, r3 }
	pop	{ r4, r5, r6, r7 }
	bx	lr
	.size	gfb127_set_cond, .-gfb127_set_cond

@ =======================================================================
@ void gfb127_add(gfb127 *d, const gfb127 *a, const gfb127 *b)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb127_add
	.thumb
	.thumb_func
	.type	gfb127_add, %function
gfb127_add:
	push	{ r4, r5, r6, r7 }
	ldm	r2!, { r4, r5, r6, r7 }
	mov	r12, r0
	ldm	r1, { r0, r1, r2, r3 }
	eors	r0, r4
	eors	r1, r5
	eors	r2, r6
	eors	r3, r7
	stm	r12, { r0, r1, r2, r3 }
	pop	{ r4, r5, r6, r7 }
	bx	lr
	.size	gfb127_add, .-gfb127_add

@ =======================================================================
@ uint32_t gfb127_trace(const gfb127 *a)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb127_trace
	.thumb
	.thumb_func
	.type	gfb127_trace, %function
gfb127_trace:
	ldr	r1, [r0]
	ldr	r2, [r0, #12]
	ubfx	r0, r1, #0, #1
	eor	r0, r0, r2, lsr #31
	bx	lr
	.size	gfb127_trace, .-gfb127_trace

@ =======================================================================
@ uint32_t gfb127_iszero(const gfb127 *a)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb127_iszero
	.thumb
	.thumb_func
	.type	gfb127_iszero, %function
gfb127_iszero:
	ldm	r0, { r0, r1, r2, r3 }
	eor	r0, r0, r3, lsr #31
	orrs	r0, r2
	lsrs	r2, r3, #31
	eor	r1, r1, r2, lsl #31
	eor	r3, r3, r2, lsl #31
	orrs	r0, r1
	orrs	r0, r3
	subs	r0, #1
	sbcs	r0, r0
	bx	lr
	.size	gfb127_iszero, .-gfb127_iszero

@ =======================================================================
@ uint32_t gfb127_equals(const gfb127 *a, const gfb127 *b)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb127_equals
	.thumb
	.thumb_func
	.type	gfb127_equals, %function
gfb127_equals:
	push	{ r4, r5, r6, r7 }
	ldm	r1!, { r4, r5, r6, r7 }
	ldm	r0, { r0, r1, r2, r3 }
	eors	r0, r4
	eors	r1, r5
	eors	r2, r6
	eors	r3, r7
	eor	r0, r0, r3, lsr #31
	orrs	r0, r2
	lsrs	r2, r3, #31
	eor	r1, r1, r2, lsl #31
	eor	r3, r3, r2, lsl #31
	orrs	r0, r1
	orrs	r0, r3
	subs	r0, #1
	sbcs	r0, r0
	pop	{ r4, r5, r6, r7 }
	bx	lr
	.size	gfb127_iszero, .-gfb127_iszero

@ =======================================================================
@ void gfb127_mul_sb(gfb127 *d, const gfb127 *a)
@
@ Uses the external ABI.
@ =======================================================================

@ Multiply by sqrt(b) = 1 + z^27 the value (GF(2^127)) held in registers
@ rv0..rv3. A temporary register (rt) is clobbered.
.macro	B127_MUL_SB  rv0, rv1, rv2, rv3, rt
	lsrs	\rt, \rv3, #5
	eor	\rv3, \rv3, \rv3, lsl #27
	eor	\rv3, \rv3, \rv2, lsr #5
	eor	\rv2, \rv2, \rv2, lsl #27
	eor	\rv2, \rv2, \rv1, lsr #5
	eor	\rv1, \rv1, \rv1, lsl #27
	eor	\rv1, \rv1, \rv0, lsr #5
	eor	\rv0, \rv0, \rv0, lsl #27
	eor	\rv0, \rv0, \rt, lsl #1
	eors	\rv2, \rt
.endm

	.align	1
	.global	gfb127_mul_sb
	.thumb
	.thumb_func
	.type	gfb127_mul_sb, %function
gfb127_mul_sb:
	push	{ lr }
	ldm	r1, { r1, r2, r3, r12 }
	B127_MUL_SB  r1, r2, r3, r12, r14
	stm	r0, { r1, r2, r3, r12 }
	pop	{ pc }
	.size	gfb127_mul_sb, .-gfb127_mul_sb

@ =======================================================================
@ void gfb127_mul_b(gfb127 *d, const gfb127 *a)
@
@ Uses the external ABI.
@ =======================================================================

@ Multiply by b = 1 + z^54 the value (GF(2^127)) held in registers
@ rv0..rv3. Two temporary registers (rt and ru) are clobbered.
.macro	B127_MUL_B  rv0, rv1, rv2, rv3, rt, ru
	lsrs	\rt, \rv2, #10
	eor	\rt, \rt, \rv3, lsl #22
	lsrs	\ru, \rv3, #10
	eor	\rv3, \rv3, \rv2, lsl #22
	eor	\rv3, \rv3, \rv1, lsr #10
	eor	\rv2, \rv2, \rv1, lsl #22
	eor	\rv2, \rv2, \rv0, lsr #10
	eor	\rv1, \rv1, \rv0, lsl #22
	eor	\rv0, \rv0, \rt, lsl #1
	eor	\rv1, \rv1, \rt, lsr #31
	eor	\rv1, \rv1, \ru, lsl #1
	eors	\rv2, \rt
	eors	\rv3, \ru
.endm

	.align	1
	.global	gfb127_mul_b
	.thumb
	.thumb_func
	.type	gfb127_mul_b, %function
gfb127_mul_b:
	push	{ r4, r5 }
	ldm	r1, { r1, r2, r3, r4 }
	B127_MUL_B  r1, r2, r3, r4, r5, r12
	stm	r0!, { r1, r2, r3, r4 }
	pop	{ r4, r5 }
	bx	lr
	.size	gfb127_mul_b, .-gfb127_mul_b

@ =======================================================================
@ void inner_gfb127_mul(gfb127 *d, const gfb127 *a, const gfb127 *b)
@
@ Clobbers: core
@ Cost: 598 (measured)
@ =======================================================================

	@ It seems that some 32-bit instructions get extra delays when
	@ not 32-bit aligned; we ensure that the start of this function
	@ is 32-bit aligned, then try to preserve 32-bit alignment across
	@ the function.
	.align	2
	.thumb
	.thumb_func
	.type	inner_gfb127_mul, %function
inner_gfb127_mul:
	@ We compute 9 elementary 32x32->64 products, each with the MM32
	@ macro:
	@        a0:a1 * b0:b1
	@    1     a0                  b0
	@    2     a1                  b1
	@    3     a0 + a1             b0 + b1
	@       a2:a3 * b2:b3
	@    4     a2                  b2
	@    5     a3                  b3
	@    6     a2 + a3             b2 + b3
	@       (a0:a1 + a2:a3) * (b0:b1 + b2:b3)
	@    7     a0 + a2             b0 + b2
	@    8     a1 + a3             b1 + b3
	@    9     a0 + a1 + a2 + a3   b0 + b1 + b2 + b3
	@
	@ Operand pairs 1 to 9 are first assembled from the source data;
	@ pairs 1 to 8 are saved in floating-point registers s0 to s15;
	@ pair 9 is obtained in r6:r7.
	ldm	r2!, { r4, r5, r6, r7 }     @ 16-bit instruction
	ldm	r1, { r1, r2, r3, r11 }     @ <unaligned>

	@ The "alternate entry" can be used by callers who have preloaded
	@ the first operand into r1:r2:r3:r11 and the second operand into
	@ r4:r5:r6:r7.
inner_gfb127_mul_alt_entry:
	push	{ r0, lr }                  @ 16-bit instruction
	vmov	s0, s1, r1, r4
	vmov	s2, s3, r2, r5
	vmov	s6, s7, r3, r6
	vmov	s8, s9, r11, r7
	eor	r8, r1, r2
	eor	r10, r4, r5
	vmov	s4, s5, r8, r10
	eor	r8, r3, r11
	eor	r10, r6, r7
	vmov	s10, s11, r8, r10
	eors	r1, r3                      @ 16-bit instruction
	eors	r4, r6                      @ 16-bit instruction
	vmov	s12, s13, r1, r4
	eor	r2, r2, r11
	eor	r5, r5, r7
	vmov	s14, s15, r2, r5
	eor	r6, r1, r2
	eor	r7, r4, r5

	@ Perform all nine 32x32->64 products. We inline the multiplier
	@ code sequence (making it a separate function would reduce code
	@ size by about 2kB, but the multiplication cost would raise
	@ by about +13%; a loop would have a similar overhead).
	@
	@ r14 contains the mask value (preserved across the MM32 macro).
	mov	r14, #0x11111111

	@ MM32 reads the specified pair of words (0 <= k <= 8), and
	@ computes their product.
	@ Input:
	@    r14 must contain 0x11111111
	@    r6 and r7 are the operands
	@ Output:
	@    r0:r1 contains the product output
	@    r14 is preserved
	@ All other core registers are clobbered.

	@ Cost: 51
.macro	MM32
	@ 4-way split of each input.
	and	r2, r6, r14, lsl #2
	and	r3, r6, r14, lsl #3
	and	r4, r7, r14
	and	r5, r7, r14, lsl #1
	and	r0, r6, r14
	and	r1, r6, r14, lsl #1
	and	r6, r7, r14, lsl #2
	and	r7, r7, r14, lsl #3

	@ r8:r10 <- x0*y0 + x1*y3 + x2*y2 + x3*y1
	umull	r8, r10, r0, r4
	umlal	r8, r10, r1, r7
	umull	r11, r12, r2, r6
	and	r12, r12, r14
	umlal	r11, r12, r3, r5
	eor	r8, r8, r11
	eor	r10, r10, r12
	and	r8, r8, r14
	and	r10, r10, r14

	@ r8:r10 <- r8:r10 + x0*y1 + x1*y0 + x2*y3 + x3*y2
	umull	r11, r12, r0, r5
	umlal	r11, r12, r2, r7
	and	r11, r11, r14, lsl #1
	and	r12, r12, r14, lsl #1
	umlal	r11, r12, r1, r4
	and	r12, r12, r14, lsl #1
	umlal	r11, r12, r3, r6
	and	r11, r11, r14, lsl #1
	and	r12, r12, r14, lsl #1
	eor	r8, r8, r11
	eor	r10, r10, r12

	@ r8:r10 <- r8:r10 + x0*y2 + x1*y1 + x2*y0 + x3*y3
	umull	r11, r12, r0, r6
	umlal	r11, r12, r3, r7
	and	r11, r11, r14, lsl #2
	and	r12, r12, r14, lsl #2
	umlal	r11, r12, r1, r5
	and	r12, r12, r14, lsl #2
	umlal	r11, r12, r2, r4
	and	r11, r11, r14, lsl #2
	and	r12, r12, r14, lsl #2
	eor	r8, r8, r11
	eor	r10, r10, r12

	@ r0:r1 <- r8:r10 + x0*y3 + x1*y2 + x2*y3 + x3*y0
	umull	r11, r12, r0, r7
	and	r12, r12, r14, lsl #3
	umlal	r11, r12, r1, r6
	umull	r0, r1, r2, r5
	and	r1, r1, r14, lsl #3
	umlal	r0, r1, r3, r4
	eor	r0, r0, r11
	eor	r1, r1, r12
	and	r0, r0, r14, lsl #3
	and	r1, r1, r14, lsl #3
	eor	r0, r0, r8
	eor	r1, r1, r10
.endm
	MM32
	vmov	r6, r7, s14, s15
	vmov	s14, s15, r0, r1
	MM32
	vmov	r6, r7, s12, s13
	vmov	s12, s13, r0, r1
	MM32
	vmov	r6, r7, s10, s11
	vmov	s10, s11, r0, r1
	MM32
	vmov	r6, r7, s8, s9
	vmov	s8, s9, r0, r1
	MM32
	vmov	r6, r7, s6, s7
	vmov	s6, s7, r0, r1
	MM32
	vmov	r6, r7, s4, s5
	vmov	s4, s5, r0, r1
	MM32
	vmov	r6, r7, s2, s3
	vmov	s2, s3, r0, r1
	MM32
	vmov	r6, r7, s0, s1
	vmov	s0, s1, r0, r1
	MM32

	@ Product 1 output (a0*b0) is in r0:r1.
	@ Products 2-9 outputs are in s0:s15, in that order.

	@ Reassemble the 255-bit product.

	@ r0:r1:r2:r3 <- (a0:a1) * (b0:b1)
	vmov	r2, r3, s0, s1
	vmov	r4, r5, s2, s3
	eors	r4, r0             @ 16-bit instruction
	eors	r4, r2             @ 16-bit instruction
	eors	r5, r1             @ 16-bit instruction
	eors	r5, r3             @ 16-bit instruction
	eors	r1, r4             @ 16-bit instruction
	eors	r2, r5             @ 16-bit instruction

	@ r4:r5:r6:r7 <- (a2:a3) * (b2:b3)
	vmov	r4, r5, s4, s5
	vmov	r6, r7, s6, s7
	vmov	r8, r10, s8, s9
	eor	r8, r8, r4
	eor	r8, r8, r6
	eor	r10, r10, r5
	eor	r10, r10, r7
	eor	r5, r5, r8
	eor	r6, r6, r10

	@ r8:r10:r11:r12 <- (a0:a1 + a2:a3)*(b0:b1 + b2:b3)
	@   48
	@   52 + 48 + 56 + 64
	@   56 + 52 + 60 + 68
	@   60
	@
	vmov	r8, r10, s10, s11
	vmov	r11, s12
	vmov	r12, r14, s14, s15
	eor	r14, r14, r10
	eor	r10, r10, r8
	eor	r10, r10, r11
	eor	r10, r10, r12
	vmov	r12, s13
	eor	r11, r11, r14
	eor	r11, r11, r12

	@ r0-r7 <- a*b (unreduced)
	eor	r8, r8, r0
	eor	r8, r8, r4
	eor	r10, r10, r1
	eor	r10, r10, r5
	eor	r11, r11, r2
	eor	r11, r11, r6
	eor	r12, r12, r3
	eor	r12, r12, r7
	eor	r2, r2, r8
	eor	r3, r3, r10
	eor	r4, r4, r11
	eor	r5, r5, r12

	@ Reduction:
	@   k <- c4:c5 + c6:c7
	@   d0:d1 <- c0:c1 + (k << 1)
	@   d2:d3 <- c2:c3 + (k >> 63) + (c6:c7 << 1) + k

	@ r8:r10 <- k
	eor	r8, r4, r6
	eor	r10, r5, r7

	@ d0:d1 <- c0:c1 + (k << 1)
	eor	r0, r0, r8, lsl #1
	eor	r1, r1, r10, lsl #1
	eor	r1, r1, r8, lsr #31

	@ d2:d3 <- c2:c3 + (k >> 63) + (c6:c7 << 1) + k
	eor	r2, r2, r10, lsr #31
	eor	r2, r2, r6, lsl #1
	eor	r3, r3, r7, lsl #1
	eor	r3, r3, r6, lsr #31
	eor	r2, r2, r8
	eor	r3, r3, r10

	@ Write out the result.
	pop	{ r4, lr }
	stm	r4!, { r0, r1, r2, r3 }
	bx	lr
	.size	inner_gfb127_mul, .-inner_gfb127_mul

@ =======================================================================
@ void gfb127_mul(gfb127 *d, const gfb127 *a, const gfb127 *b)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb127_mul
	.thumb
	.thumb_func
	.type	gfb127_mul, %function
gfb127_mul:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	bl	inner_gfb127_mul
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gfb127_mul, .-gfb127_mul

@ =======================================================================
@ void gfb127_div_z(gfb127 *d, const gfb127 *a)
@
@ Uses the external ABI.
@ =======================================================================

@ Divide by z the value (GF(2^127)) held in registers rv0..rv3.
.macro	B127_DIV_Z  rv0, rv1, rv2, rv3
	eor	\rv3, \rv3, \rv0, lsl #31
	eor	\rv1, \rv1, \rv0, lsl #31
	lsrs	\rv3, \rv3, #1
	rrxs	\rv2, \rv2
	rrxs	\rv1, \rv1
	rrxs	\rv0, \rv0
.endm

	.align	1
	.global	gfb127_div_z
	.thumb
	.thumb_func
	.type	gfb127_div_z, %function
gfb127_div_z:
	ldm	r1, { r1, r2, r3, r12 }
	B127_DIV_Z  r1, r2, r3, r12
	stm	r0, { r1, r2, r3, r12 }
	bx	lr
	.size	gfb127_div_z, .-gfb127_div_z

@ =======================================================================
@ void gfb127_div_z2(gfb127 *d, const gfb127 *a)
@
@ Uses the external ABI.
@ =======================================================================

@ Divide by z^2 the value (GF(2^127)) held in registers rv0..rv3.
.macro	B127_DIV_Z2  rv0, rv1, rv2, rv3
	B127_DIV_Z  \rv0, \rv1, \rv2, \rv3
	B127_DIV_Z  \rv0, \rv1, \rv2, \rv3
.endm

	.align	1
	.global	gfb127_div_z2
	.thumb
	.thumb_func
	.type	gfb127_div_z2, %function
gfb127_div_z2:
	ldm	r1, { r1, r2, r3, r12 }
	B127_DIV_Z2  r1, r2, r3, r12
	stm	r0, { r1, r2, r3, r12 }
	bx	lr
	.size	gfb127_div_z2, .-gfb127_div_z2

@ =======================================================================
@ void inner_gfb127_square(gfb127 *d, const gfb127 *a)
@
@ Input:
@   r0:r1:r2:r3   value *a
@ Output:
@   r0:r1:r2:r3   value *d
@ Clobbers: core (but s0-s15 are preserved)
@ Cost: 67
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_gfb127_square, %function
inner_gfb127_square:
	@ Expand each word (carryless squaring)
	mov	r12, #0x11111111

	@ This macro squares rs into rdl:rdh. Registers ru and rv are
	@ temporaries. Register r12 must contain 0x11111111 (r12 is not
	@ modified).
.macro	SQ32  rdl, rdh, rs, ru, rv
	and	\ru, \rs, r12
	umull	\rdl, \rdh, \ru, \ru
	and	\ru, \rs, r12, lsl #2
	umlal	\rdl, \rdh, \ru, \ru
	and	\rdl, \rdl, r12
	and	\rdh, \rdh, r12
	and	\ru, \rs, r12, lsl #1
	umull	\ru, \rv, \ru, \ru
	and	\rs, \rs, r12, lsl #3
	umlal	\ru, \rv, \rs, \rs
	and	\ru, \ru, r12, lsl #2
	and	\rv, \rv, r12, lsl #2
	eors	\rdl, \ru
	eors	\rdh, \rv
.endm

	SQ32	r10, r11, r3, r4, r5
	SQ32	r7, r8, r2, r4, r5
	SQ32	r5, r6, r1, r3, r4
	SQ32	r3, r4, r0, r1, r2

	@ Square is in r3:r4:r5:r6:r7:r8:r10:r11 (unreduced).
	@ Reduction:
	@   k <- c4:c5 + c6:c7
	@   d0:d1 <- c0:c1 + (k << 1)
	@   d2:d3 <- c2:c3 + (k >> 63) + (c6:c7 << 1) + k
	@ Since all values are squares, their odd-indexed bits are zero,
	@ and we can omit all single-bit propagation across words.

	@ r7:r8 <- k
	eor	r7, r7, r10
	eor	r8, r8, r11

	@ r0:r1 <- c0:c1 + (k << 1)
	eor	r0, r3, r7, lsl #1
	eor	r1, r4, r8, lsl #1

	@ r2:r3 <- c2:c3 + (c6:c7 << 1) + k
	eor	r2, r5, r10, lsl #1
	eor	r3, r6, r11, lsl #1
	eors	r2, r7
	eor	r3, r3, r8

	bx	lr
	.size	inner_gfb127_square, .-inner_gfb127_square

@ =======================================================================
@ void gfb127_square(gfb127 *d, const gfb127 *a)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb127_square
	.thumb
	.thumb_func
	.type	gfb127_square, %function
gfb127_square:
	push	{ r0, r4, r5, r6, r7, r8, r10, r11, lr }
	ldm	r1, { r0, r1, r2, r3 }
	bl	inner_gfb127_square
	pop	{ r4 }
	stm	r4!, { r0, r1, r2, r3 }
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gfb127_square, .-gfb127_square

@ =======================================================================
@ void inner_gfb127_xsquare(gfb127 *d, const gfb127 *a, unsigned n)
@
@ ASSUMPTION: n > 0
@ Clobbers: core
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_gfb127_xsquare, %function
inner_gfb127_xsquare:
	push	{ r0, r2, lr }

	ldm	r1, { r0, r1, r2, r3 }
Linner_gfb127_xsquare_loop:
	bl	inner_gfb127_square
	ldr	r5, [sp, #4]
	subs	r5, #1
	beq	Linner_gfb127_xsquare_exit
	str	r5, [sp, #4]
	b	Linner_gfb127_xsquare_loop

Linner_gfb127_xsquare_exit:
	pop	{ r4, r5, lr }
	stm	r4!, { r0, r1, r2, r3 }
	bx	lr
	.size	inner_gfb127_xsquare, .-inner_gfb127_xsquare

@ =======================================================================
@ void gfb127_xsquare(gfb127 *d, const gfb127 *a, unsigned n)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb127_xsquare
	.thumb
	.thumb_func
	.type	gfb127_xsquare, %function
gfb127_xsquare:
	@ Handle the edge case n == 0
	cmp	r2, #0
	beq	Lgfb127_xsquare_zz

	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	bl	inner_gfb127_xsquare
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }

Lgfb127_xsquare_zz:
	push	{ r0, lr }
	movs	r2, #32
	bl	memmove
	pop	{ r0, pc }
	.size	gfb127_square, .-gfb127_square

@ =======================================================================
@ void inner_frob(gfb127 *d, const gfb127 *a, const gfb127 *tab)
@
@ Applies the linear operation incarnated in the 128-entry tab[] array
@ (used for the 42nd Frobenius automorphism x -> x^(2^42)).
@
@ Clobbers: core
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_frob, %function
inner_frob:
	push	{ r0, lr }

	@ Zero buffer for output value.
	movs	r3, #0
	umull	r8, r10, r3, r3
	umull	r11, r12, r3, r3

.macro FROB1  k
	ldm	r2!, { r4, r5, r6, r7 }
	sbfx	r3, r0, #(\k), #1
	ands	r4, r3
	ands	r5, r3
	ands	r6, r3
	ands	r7, r3
	eors	r8, r4
	eors	r10, r5
	eors	r11, r6
	eors	r12, r7
.endm

	@ Loop over all input bytes.
	movs	r14, #16
Linner_frob_loop:
	ldrb	r0, [r1], #1
	FROB1	0
	FROB1	1
	FROB1	2
	FROB1	3
	FROB1	4
	FROB1	5
	FROB1	6
	FROB1	7
	subs	r14, #1
	bne	Linner_frob_loop

	@ Write back result and return.
	pop	{ r0, lr }
	stm	r0!, { r8, r10, r11, r12 }
	bx	lr
	.size	inner_frob, .-inner_frob

@ =======================================================================
@ void gfb127_invert(gfb127 *d, const gfb127 *a)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb127_invert
	.thumb
	.thumb_func
	.type	gfb127_invert, %function
gfb127_invert:
	push	{ r0, r4, r5, r6, r7, r8, r10, r11, lr }
	sub	sp, #52

	@ let x = a^2
	@ We want 1/a = a^(2^128 - 2) = x^(2^127 - 1)

	@ x1 <- x = a^2
	ldm	r1, { r0, r1, r2, r3 }
	bl	inner_gfb127_square
	mov	r4, sp
	stm	r4!, { r0, r1, r2, r3 }

	@ x2 <- x^(2^3 - 1)
	@ x is still in r0:r1:r2:r3
	bl	inner_gfb127_square
	add	r4, sp, #16
	stm	r4!, { r0, r1, r2, r3 }
	add	r0, sp, #16
	add	r1, sp, #16
	mov	r2, sp
	bl	inner_gfb127_mul
	add	r0, sp, #16
	ldm	r0, { r0, r1, r2, r3 }
	bl	inner_gfb127_square
	add	r4, sp, #16
	stm	r4!, { r0, r1, r2, r3 }
	add	r0, sp, #16
	add	r1, sp, #16
	mov	r2, sp
	bl	inner_gfb127_mul

	@ x2 <- x^(2^6 - 1)
	add	r0, sp, #32
	add	r1, sp, #16
	movs	r2, #3
	bl	inner_gfb127_xsquare
	add	r0, sp, #16
	add	r1, sp, #16
	add	r2, sp, #32
	bl	inner_gfb127_mul

	@ x1 <- x^(2^7 - 1)
	add	r0, sp, #16
	ldm	r0, { r0, r1, r2, r3 }
	bl	inner_gfb127_square
	add	r4, sp, #16
	stm	r4!, { r0, r1, r2, r3 }
	mov	r0, sp
	mov	r1, sp
	add	r2, sp, #16
	bl	inner_gfb127_mul

	@ x1 <- x^(2^14 - 1)
	add	r0, sp, #16
	mov	r1, sp
	movs	r2, #7
	bl	inner_gfb127_xsquare
	mov	r0, sp
	add	r1, sp, #16
	mov	r2, sp
	bl	inner_gfb127_mul

	@ x2 <- x^(2^28 - 1)
	add	r0, sp, #16
	mov	r1, sp
	movs	r2, #14
	bl	inner_gfb127_xsquare
	add	r0, sp, #16
	add	r1, sp, #16
	mov	r2, sp
	bl	inner_gfb127_mul

	@ x1 <- x^(2^42 - 1)
	add	r0, sp, #16
	add	r1, sp, #16
	movs	r2, #14
	bl	inner_gfb127_xsquare
	mov	r0, sp
	add	r1, sp, #16
	mov	r2, sp
	bl	inner_gfb127_mul

	@ x2 <- x^(2^84 - 1)
	add	r0, sp, #16
	mov	r1, sp
	adr	r2, const_frob42
	bl	inner_frob
	add	r0, sp, #16
	add	r1, sp, #16
	mov	r2, sp
	bl	inner_gfb127_mul

	@ d <- x^(2^126 - 1) = 1/a
	add	r0, sp, #16
	add	r1, sp, #16
	adr	r2, const_frob42
	bl	inner_frob

	ldr	r0, [sp, #52]
	add	r1, sp, #16
	mov	r2, sp
	bl	inner_gfb127_mul

	add	sp, #56
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }

	@ Table for the 42nd Frobenius automorphism: x -> x^(2^42)
	.align	2
const_frob42:
	.long	0x00000001, 0x00000000, 0x00000000, 0x00000000
	.long	0x00000110, 0x00000001, 0x00000000, 0x00000000
	.long	0x00010100, 0x00000000, 0x00000001, 0x00000000
	.long	0x01111000, 0x00010100, 0x00000110, 0x00000001
	.long	0x00010002, 0x00000001, 0x00000001, 0x00000000
	.long	0x01100220, 0x00010112, 0x00000111, 0x00000001
	.long	0x01020202, 0x00010101, 0x00000103, 0x00000001
	.long	0x12222222, 0x00131313, 0x00001230, 0x00000013
	.long	0x00000006, 0x00000001, 0x00000000, 0x00000000
	.long	0x00000660, 0x00000116, 0x00000001, 0x00000000
	.long	0x00060600, 0x00010100, 0x00000006, 0x00000001
	.long	0x06666002, 0x01171600, 0x00010761, 0x00000116
	.long	0x0006000C, 0x00010004, 0x00000007, 0x00000001
	.long	0x06600CC2, 0x0116044C, 0x00010775, 0x00000117
	.long	0x060C0C0E, 0x01040404, 0x0001070A, 0x00000105
	.long	0x6CCCCCEA, 0x12484848, 0x00137FA0, 0x0000125A
	.long	0x00000014, 0x00000000, 0x00000001, 0x00000000
	.long	0x00001540, 0x00000014, 0x00000110, 0x00000001
	.long	0x00141402, 0x00000000, 0x00010115, 0x00000000
	.long	0x15554220, 0x00141402, 0x01110450, 0x00010115
	.long	0x0014002A, 0x00000014, 0x00010017, 0x00000001
	.long	0x154028A2, 0x0014156A, 0x01101665, 0x00010107
	.long	0x14282A2E, 0x00141416, 0x0102173D, 0x00010114
	.long	0x6AAA8EC8, 0x017D7D5B, 0x12235BD2, 0x0013127C
	.long	0x00000078, 0x00000014, 0x00000006, 0x00000001
	.long	0x00007F82, 0x00001538, 0x00000675, 0x00000116
	.long	0x0078780C, 0x00141402, 0x0006067E, 0x00010115
	.long	0x7FFD8EEA, 0x152D3A2C, 0x06730CF7, 0x0117022E
	.long	0x007800FE, 0x00140052, 0x00060067, 0x00010011
	.long	0x7F82F1C2, 0x153857DE, 0x06756133, 0x01161077
	.long	0x78F2FECC, 0x1450525A, 0x0619678C, 0x01041145
	.long	0x7FD90248, 0x6DA58113, 0x6DA5B7CB, 0x124936DA
	.long	0x00000112, 0x00000000, 0x00000001, 0x00000000
	.long	0x00010320, 0x00000112, 0x00000110, 0x00000001
	.long	0x01131202, 0x00000000, 0x00010013, 0x00000000
	.long	0x02232220, 0x01131203, 0x01101230, 0x00010013
	.long	0x01120226, 0x00000112, 0x00010111, 0x00000001
	.long	0x03220462, 0x01130107, 0x01110103, 0x00010001
	.long	0x10262422, 0x01131311, 0x01031237, 0x00010012
	.long	0x24446004, 0x12040423, 0x12310772, 0x00130116
	.long	0x0000066C, 0x00000112, 0x00000006, 0x00000001
	.long	0x00060AC2, 0x0001054C, 0x00000773, 0x00000116
	.long	0x066A6C0C, 0x01131202, 0x0006006A, 0x00010013
	.long	0x0CC8CCE6, 0x04494E2A, 0x07727EB0, 0x0116125A
	.long	0x066C0CD6, 0x0112044A, 0x00060775, 0x00010117
	.long	0x0ACE194E, 0x05480270, 0x0774070C, 0x01170105
	.long	0x60D6D8E8, 0x164C4E44, 0x07187FB1, 0x0105125B
	.long	0xD9BF4234, 0x485C78CE, 0x7EB11619, 0x125B0106
	.long	0x0000156A, 0x00000000, 0x00000107, 0x00000000
	.long	0x00143CA0, 0x0000156A, 0x00011770, 0x00000107
	.long	0x157F680E, 0x00000000, 0x0106136D, 0x00000000
	.long	0x289E8EE0, 0x157F681A, 0x16725BD0, 0x0106136C
	.long	0x156A28DA, 0x0000156A, 0x01071663, 0x00000107
	.long	0x3C8A55AE, 0x157E146E, 0x1767115D, 0x01060112
	.long	0x40FCF6C6, 0x157F7D71, 0x05185DB9, 0x0106126B
	.long	0xF1358EB4, 0x68765092, 0x5DA10D9F, 0x136B100C
	.long	0x00007F7C, 0x0000156A, 0x00000612, 0x00000107
	.long	0x007889CE, 0x001443DC, 0x0006674D, 0x00011162
	.long	0x7F037024, 0x157F680E, 0x06146B6E, 0x0106136D
	.long	0xF14B0098, 0x579DFEBC, 0x6154A396, 0x106630B8
	.long	0x7F7CF0D2, 0x156A57A6, 0x06126127, 0x01071071
	.long	0x8931FFC0, 0x438E2CCA, 0x672A72B2, 0x11731731
	.long	0x80061242, 0x3FFFF9E1, 0x0A28A28C, 0x030C30C3
	.long	0x006B07A0, 0x80006DDA, 0xB6DB6DDD, 0x36DB6DB6
	.long	0x00010106, 0x00000000, 0x00000001, 0x00000000
	.long	0x01111660, 0x00010106, 0x00000110, 0x00000001
	.long	0x00070602, 0x00000001, 0x00000007, 0x00000000
	.long	0x07766220, 0x00070712, 0x00000771, 0x00000007
	.long	0x0104020E, 0x00010107, 0x00000105, 0x00000001
	.long	0x14422EE2, 0x0015157F, 0x00001456, 0x00000015
	.long	0x060C0C0A, 0x00060607, 0x0000060A, 0x00000006
	.long	0x6CCCCAAC, 0x006A6B7C, 0x00006CA1, 0x0000006A
	.long	0x00060614, 0x00010106, 0x00000006, 0x00000001
	.long	0x06667542, 0x01171074, 0x00010767, 0x00000116
	.long	0x0012140C, 0x00070604, 0x00000013, 0x00000007
	.long	0x13354CCE, 0x0764704C, 0x00071433, 0x00000763
	.long	0x06180C26, 0x0102041C, 0x00010718, 0x00000103
	.long	0x798CE666, 0x143C51E0, 0x00156C9E, 0x00001428
	.long	0x14282830, 0x06181818, 0x0006123D, 0x0000061E
	.long	0x6AAABF3C, 0x6DB1B1A5, 0x006B00D0, 0x00006DDD
	.long	0x0014147A, 0x00000000, 0x00010113, 0x00000000
	.long	0x15553DA0, 0x0014147A, 0x01110230, 0x00010113
	.long	0x006C7826, 0x00000014, 0x00070669, 0x00000001
	.long	0x6ABFA462, 0x006C6D66, 0x07760F85, 0x00070779
	.long	0x14502AD2, 0x0014146E, 0x0104174F, 0x00010112
	.long	0x152A7D04, 0x01050227, 0x14432E8C, 0x0015146E
	.long	0x78F0FC9C, 0x00787860, 0x060C7288, 0x00060679
	.long	0x7FFF5932, 0x070F1AE3, 0x6CCBDE99, 0x006A6C1E
	.long	0x0078791C, 0x0014147A, 0x0006066A, 0x00010113
	.long	0x7FFC8FE6, 0x152D44BC, 0x067319C9, 0x0117045A
	.long	0x016910D6, 0x006C785E, 0x00121563, 0x0007066F
	.long	0x7F8FD7BE, 0x6BD6CB37, 0x135F4B01, 0x07641C93
	.long	0x79E2FCC8, 0x142853B6, 0x060D66DE, 0x01021123
	.long	0x7ED726C4, 0x133471D6, 0x789AF161, 0x143D57E8
	.long	0x122E07BA, 0x79E1EDDD, 0x14575129, 0x0618679E
	.long	0x00D70E90, 0x6DDD0779, 0x6DDDB1AB, 0x6DB6B6DC
	.long	0x0113146E, 0x00000000, 0x00010015, 0x00000000
	.long	0x022528E0, 0x0113146F, 0x01101450, 0x00010015
	.long	0x07786E2A, 0x00000112, 0x0007017B, 0x00000001
	.long	0x0FE8C8A2, 0x07796D0D, 0x07716DA3, 0x0007006B
	.long	0x164A28F6, 0x0113157D, 0x01051451, 0x00010014
	.long	0x2E887948, 0x146E0231, 0x14570178, 0x00150110
	.long	0x60D4DEA0, 0x066A6B74, 0x060A6CB4, 0x0006006D
	.long	0xD99F4ADA, 0x6C191D86, 0x6CA6145F, 0x006A0762
	.long	0x066A7964, 0x0113146E, 0x0006007E, 0x00010015
	.long	0x0CDCF26A, 0x044F5182, 0x07726D9A, 0x0116142E
	.long	0x131164FE, 0x07786846, 0x00120609, 0x0007017D
	.long	0x207CB31A, 0x1CFFA68C, 0x145900AC, 0x07636CD9
	.long	0x75BEF21C, 0x102057F8, 0x070C6C8F, 0x01031429
	.long	0xE71B1590, 0x57EC75EE, 0x6D890431, 0x14290718
	.long	0x42F6C71A, 0x75A9A599, 0x125100A1, 0x061E6DDA
	.long	0xD597B018, 0xB1C907CC, 0x07A76327, 0x6DDA0712
	.long	0x157F1772, 0x00000000, 0x0106157F, 0x00000000
	.long	0x28E60520, 0x157F1766, 0x167428F0, 0x0106157E
	.long	0x6A6958FE, 0x0000156A, 0x07137D0D, 0x00000107
	.long	0xCFCD73EE, 0x6A7D6432, 0x624AC9BD, 0x07126A7A
	.long	0x3F80041A, 0x157F020D, 0x030A28F3, 0x01061479
	.long	0x7A087350, 0x177229F6, 0x2EF36A51, 0x157F1660
	.long	0x820A49E8, 0x7F031A4D, 0x1E51CB84, 0x06146C7D
	.long	0x26C5AE76, 0x7121A0B2, 0xCDC04A0E, 0x6B7B714B
	.long	0x7F02732C, 0x157F1772, 0x06147F02, 0x0106157F
	.long	0xF058343C, 0x57E47674, 0x6141F038, 0x106057F4
	.long	0x7D77D00A, 0x6A692783, 0x126B1A43, 0x07137B1F
	.long	0xAE8BFC90, 0xB2C22B40, 0x20D1BBC7, 0x7027B4A0
	.long	0x830C30AE, 0x40820834, 0x1E45E45E, 0x051E51E5
	.long	0x36CF0720, 0x09248765, 0xE4264270, 0x51F11F11
	.long	0x00156C8A, 0x80001445, 0x3CF3CF29, 0x0A28A28A
	.long	0x006B07A1, 0x80006DDA, 0xB6DB6DDD, 0x36DB6DB6
	.size	gfb127_invert, .-gfb127_invert

@ =======================================================================
@ void gfb127_div(gfb127 *d, const gfb127 *x, const gfb127 *y)
@
@ d <- x/y
@ If y is zero then this sets d to zero.
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb127_div
	.thumb
	.thumb_func
	.type	gfb127_div, %function
gfb127_div:
	push	{ r0, r1, r4, r5, r6, r7, r8, r10, r11, lr }
	sub	sp, #16
	mov	r0, sp
	movs	r1, r2
	bl	gfb127_invert
	ldrd	r0, r1, [sp, #16]
	mov	r2, sp
	bl	inner_gfb127_mul
	add	sp, #24
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gfb127_div, .-gfb127_div

@ =======================================================================
@ uint32_t squeeze32(uint32_t x)
@
@ Squeeze even-indexed bits of x into the lower half.
@ Input:
@    r1 = x
@    r4 = 0x11111111
@    r5 = 0x03030303
@    r6 = 0x000F000F
@ Output:
@    r1 = output
@ Clobbers: r2
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	squeeze32, %function
squeeze32:
	and	r2, r1, r4, lsl #2
	ands	r1, r4
	orr	r1, r1, r2, lsr #1
	and	r2, r1, r5, lsl #4
	ands	r1, r5
	orr	r1, r1, r2, lsr #2
	and	r2, r1, r6, lsl #8
	ands	r1, r6
	orr	r2, r1, r2, lsr #4
	lsrs	r1, r2, #8
	bfi	r1, r2, #0, #8
	bx	lr
	.size	squeeze32, .-squeeze32

@ =======================================================================
@ uint32_t squeeze64(uint32_t x, uint32_t y)
@
@ Squeeze even-indexed bits of x into the lower half.
@ Input:
@    r2 = x
@    r3 = y
@    r4 = 0x11111111
@    r5 = 0x03030303
@    r6 = 0x000F000F
@ Output:
@    r1 = output
@ Clobbers: r1, r2, r3
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	squeeze64, %function
squeeze64:
	@ Squeeze x
	and	r1, r2, r4, lsl #2
	ands	r2, r4
	orr	r2, r2, r1, lsr #1
	and	r1, r2, r5, lsl #4
	ands	r2, r5
	orr	r2, r2, r1, lsr #2
	and	r1, r2, r6, lsl #8
	ands	r2, r6
	orr	r1, r2, r1, lsr #4
	lsrs	r2, r1, #8
	bfi	r2, r1, #0, #8

	@ Squeeze y
	and	r1, r3, r4, lsl #2
	ands	r3, r4
	orr	r3, r3, r1, lsr #1
	and	r1, r3, r5, lsl #4
	ands	r3, r5
	orr	r3, r3, r1, lsr #2
	and	r1, r3, r6, lsl #8
	ands	r3, r6
	orr	r1, r3, r1, lsr #4
	lsrs	r3, r1, #8
	bfi	r3, r1, #0, #8

	@ Pack values and return
	pkhbt	r1, r2, r3, lsl #16
	bx	lr
	.size	squeeze64, .-squeeze64

@ =======================================================================
@ void inner_gfb127_sqrt(gfb127 *d, const gfb127 *a)
@
@ Clobbers: core
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_gfb127_sqrt, %function
inner_gfb127_sqrt:
	push	{ lr }

	@ Load source value, and set constants for squeezing.
	ldm	r1, { r7, r8, r10, r11 }
	movs	r4, #0x11111111
	movs	r5, #0x03030303
	movs	r6, #0x000F000F

	@ r12 <- ao1 = squeeze(a2:a3 >> 1)
	lsrs	r2, r10, #1
	lsrs	r3, r11, #1
	bl	squeeze64
	mov	r12, r1

	@ r11 <- ao0 = squeeze(a0:a1 >> 1)
	lsrs	r2, r7, #1
	lsrs	r3, r8, #1
	bl	squeeze64
	@ write to r11 is delayed

	@ r10 <- ae1 = squeeze(a2:a3)
	movs	r2, r10
	mov	r3, r11
	mov	r11, r1   @ delayed write to r11
	bl	squeeze64
	mov	r10, r1

	@ r1 <- ae0 = squeeze(a0:a1)
	movs	r2, r7
	mov	r3, r8
	bl	squeeze64

	@ ae0:ae1:ao0:ao1 = r1:r10:r11:r12
	@ d0:d1:d2:d3 <- ae0 : ae1 + ao0 : ao0 + ao1 : ao1
	eor	r10, r10, r11
	eor	r11, r11, r12
	stm	r0!, { r1, r10, r11, r12 }

	pop	{ pc }
	.size	inner_gfb127_sqrt, .-inner_gfb127_sqrt

@ =======================================================================
@ void gfb127_sqrt(gfb127 *d, const gfb127 *a)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb127_sqrt
	.thumb
	.thumb_func
	.type	gfb127_sqrt, %function
gfb127_sqrt:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	bl	inner_gfb127_sqrt
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gfb127_sqrt, .-gfb127_sqrt

@ =======================================================================
@ void inner_tab_even(gfb127 *d, uint32_t a, const gfb127 *tab)
@
@ Input:
@   r0:r1:r2:r3   value *d
@   r8            tab
@   r11           value a
@ Output:
@   r0:r1:r2:r3   value *d (updated)
@   r8            tab (updated)
@   r11           value a (unchanged)
@ Clobbers: r4-r7, r10
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_tab_even, %function
inner_tab_even:
.macro TAB1  k
	ldm	r8!, { r4, r5, r6, r7 }
	sbfx	r10, r11, #(\k), #1
	and	r4, r4, r10
	and	r5, r5, r10
	and	r6, r6, r10
	and	r7, r7, r10
	eors	r0, r4
	eors	r1, r5
	eors	r2, r6
	eors	r3, r7
.endm
	TAB1	0
	TAB1	2
	TAB1	4
	TAB1	6
	TAB1	8
	TAB1	10
	TAB1	12
	TAB1	14
	bx	lr
	.size	inner_tab_even, .-inner_tab_even

@ =======================================================================
@ void inner_gfb127_halftrace(gfb127 *d, const gfb127 *a)
@
@ Clobbers: core
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_gfb127_halftrace, %function
inner_gfb127_halftrace:
	push	{ r0, lr }

	@ Load source value into r8:r10:r11:r12 (buffer ao)
	ldm	r1!, { r8, r10, r11, r12 }

	@ Squeeze even-indexed value into r0:r7 = x0:x1
	movs	r4, #0x11111111
	movs	r5, #0x03030303
	movs	r6, #0x000F000F
	mov	r2, r8
	mov	r3, r10
	bl	squeeze64
	movs	r0, r1
	mov	r2, r11
	mov	r3, r12
	bl	squeeze64
	movs	r7, r1

	@ Add x0:x1 to ao; also save them as e0:e1
	eor	r8, r8, r0
	eor	r10, r10, r7

	@ Squeeze x0:x1 and add to e0:e1
	movs	r2, r0
	movs	r3, r7
	bl	squeeze64
	eors	r0, r1

	@ Six more squeezing operations.
	@ x0 is in r1; e0 is in r0.
	eor	r8, r8, r1
	bl	squeeze32
	eors	r0, r1
	eor	r8, r8, r1
	bl	squeeze32
	eors	r0, r1
	eor	r8, r8, r1
	bl	squeeze32
	eors	r0, r1
	eor	r8, r8, r1
	bl	squeeze32
	eors	r0, r1
	eor	r8, r8, r1
	bl	squeeze32
	eors	r0, r1
	eor	r8, r8, r1
	bl	squeeze32
	@ Skip final XORing into e0

	@ Initialize output buffer r0-r3 to e0:e1:0:0
	@ r0 already contains e0
	movs	r1, r7
	movs	r2, #0
	movs	r3, #0

	@ Lookup odd-indexed bits of ao from the precomputed table.
	@ We first compact ao into two registers r11:r12 to gain a bit
	@ of extra room.
	movs	r4, #0xAAAAAAAA
	and	r8, r8, r4
	and	r10, r10, r4
	and	r11, r11, r4
	and	r12, r12, r4
	orr	r11, r11, r8, lsr #1
	orr	r12, r12, r10, lsr #1

	@ Registers r8 and r10 are now free.

	@ Process all relevant bits from ao
	adr	r8, const_halftrace
	bl	inner_tab_even
	ror	r11, r11, #16
	bl	inner_tab_even
	ror	r10, r11, #16
	mov	r11, r12
	mov	r12, r10
	bl	inner_tab_even
	ror	r11, r11, #16
	bl	inner_tab_even
	ror	r10, r11, #16
	lsr	r11, r12, #1
	mov	r12, r10
	bl	inner_tab_even
	ror	r11, r11, #16
	bl	inner_tab_even
	lsr	r11, r12, #1
	bl	inner_tab_even
	lsr	r11, r12, #17
	bl	inner_tab_even

	@ Write output and return.
	pop	{ r4, lr }
	stm	r4!, { r0, r1, r2, r3 }
	bx	lr
	.align	2
const_halftrace:
	.long	0x00000000, 0x00000000, 0x00000001, 0x00000000
	.long	0x01141668, 0x00010112, 0x00010014, 0x00000000
	.long	0x0105135E, 0x00010011, 0x00000016, 0x00000001
	.long	0x116159DE, 0x01031401, 0x01000426, 0x00000005
	.long	0x0117177E, 0x00010115, 0x00000106, 0x00000001
	.long	0x041E2620, 0x0010017C, 0x00060260, 0x00000114
	.long	0x112C52C8, 0x01010472, 0x00040648, 0x00010012
	.long	0x42CC8A00, 0x12045850, 0x10241E00, 0x00040430
	.long	0x00060200, 0x00000014, 0x00000010, 0x00000000
	.long	0x00240200, 0x00000430, 0x00040600, 0x00000010
	.long	0x135E5EE8, 0x0105135E, 0x00121628, 0x00010116
	.long	0x2CA82000, 0x04506EC0, 0x00686000, 0x00100640
	.long	0x04722C20, 0x0010150C, 0x00021460, 0x00000104
	.long	0x3FE878C8, 0x055D5EE2, 0x02284848, 0x00151622
	.long	0x7C28E080, 0x15522EC8, 0x00682080, 0x01120648
	.long	0xF880C080, 0x75E2E808, 0x48804080, 0x05622808
	.long	0x0101115E, 0x00010003, 0x00000002, 0x00000001
	.long	0x110050C8, 0x0101000A, 0x00000008, 0x00010002
	.long	0x00200000, 0x00000420, 0x00000400, 0x00000000
	.long	0x5000C080, 0x11020088, 0x00000080, 0x01020008
	.long	0x06522C20, 0x0014132C, 0x00061060, 0x00000104
	.long	0x00200000, 0x00000400, 0x00200000, 0x00000400
	.long	0x37C878C8, 0x051D52E2, 0x02484848, 0x00151222
	.long	0xC0008000, 0x52088080, 0x00008000, 0x12080080
	.long	0x053F377E, 0x0013057D, 0x00060646, 0x00000111
	.long	0x192050C8, 0x01492C02, 0x00602048, 0x00010442
	.long	0x6BE09848, 0x144F5C2A, 0x062068C8, 0x0107146A
	.long	0x08000000, 0x00402000, 0x08000000, 0x00402000
	.long	0x2EE42A00, 0x06547690, 0x004C7E00, 0x00140270
	.long	0xE0808000, 0x5628C880, 0x20808000, 0x16284880
	.long	0xB8804080, 0x67EAE888, 0x0880C080, 0x176A2888
	.long	0x80000000, 0x68808000, 0x80000000, 0x68808000
	.long	0x00150736, 0x00000113, 0x00010014, 0x00000001
	.long	0x00610916, 0x00021403, 0x01000426, 0x00010007
	.long	0x043E2620, 0x0010057C, 0x00060640, 0x00000114
	.long	0x12CC4A80, 0x03065858, 0x10241E00, 0x01060438
	.long	0x06762E20, 0x0014151C, 0x00021460, 0x00000114
	.long	0x2C882000, 0x045062C0, 0x00486800, 0x00100240
	.long	0x08200000, 0x00402C00, 0x00602000, 0x00000400
	.long	0x38804080, 0x27EAE888, 0x48804080, 0x176A2888
	.long	0x143F67B6, 0x01100577, 0x0004064E, 0x00010113
	.long	0x49209048, 0x10432C8A, 0x006820C8, 0x0103044A
	.long	0x6BC09848, 0x146F582A, 0x062068C8, 0x0107106A
	.long	0xC8008000, 0x52C8A080, 0x08808000, 0x12482080
	.long	0x37C47AC8, 0x051D5A92, 0x022C5E48, 0x00150632
	.long	0xE8808000, 0x5E68E880, 0x20808000, 0x16686880
	.long	0x5800C080, 0x11C22008, 0x08004080, 0x01426008
	.long	0x00000000, 0x80000000, 0x80000000, 0x00000000
	.long	0x00740E20, 0x00021510, 0x01010430, 0x00010004
	.long	0x12AD4396, 0x03044C5B, 0x11241A2E, 0x01070437
	.long	0x28B60620, 0x044067BC, 0x004E6E60, 0x00100374
	.long	0x2A4C0A00, 0x24ECB0D0, 0x58A45E00, 0x166C2C30
	.long	0x4F56BE68, 0x10573996, 0x006A36A8, 0x0103075E
	.long	0xE488A000, 0x5698C240, 0x08C8E000, 0x12582AC0
	.long	0xE0A08000, 0x5E28C480, 0x20E08000, 0x16684C80
	.long	0x38804080, 0xA7EAE888, 0xC880C080, 0x176AA888
	.long	0x06922420, 0x0214492C, 0x11221C60, 0x01040524
	.long	0x636C9A48, 0x34AF9C5A, 0x58C47EC8, 0x1767287A
	.long	0x8F483848, 0x42F79A6A, 0x0EC888C8, 0x137F3AAA
	.long	0xF080C080, 0xF5224808, 0xC0804080, 0x05A28808
	.long	0x54A8E080, 0x31B2C6C8, 0x58E82080, 0x15722E48
	.long	0x18004080, 0xAB4AA088, 0xE800C080, 0x1BCAE088
	.long	0x40008000, 0xBA888080, 0xC0008000, 0x3A888080
	.long	0x80000000, 0x68808000, 0x80000000, 0x68808000
	.size	inner_gfb127_halftrace, .-inner_gfb127_halftrace

@ =======================================================================
@ void gfb127_halftrace(gfb127 *d, const gfb127 *a)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb127_halftrace
	.thumb
	.thumb_func
	.type	gfb127_halftrace, %function
gfb127_halftrace:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	bl	inner_gfb127_halftrace
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gfb127_halftrace, .-gfb127_halftrace

@ =======================================================================
@ void gfb127_encode(void *dst, const gfb127 *a)
@
@ Encode value a over exactly 16 bytes (normalized).
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb127_encode
	.thumb
	.thumb_func
	.type	gfb127_encode, %function
gfb127_encode:
	@ Encoding is about the same as gfb127_normalize(), since we use
	@ little-endian, except that the destination may be unaligned,
	@ and the Cortex M4 does not tolerate unaligned accesses with
	@ stm or strd.
	ldm	r1, { r1, r2, r3, r12 }
	eor	r1, r1, r12, lsr #31
	str	r1, [r0]
	lsrs	r1, r12, #31
	str	r3, [r0, #8]
	eor	r2, r2, r1, lsl #31
	str	r2, [r0, #4]
	eor	r12, r12, r1, lsl #31
	str	r12, [r0, #12]
	bx	lr
	.size	gfb127_encode, .-gfb127_encode

@ =======================================================================
@ void gfb127_decode16_reduce(gfb127 *a, void *src)
@
@ Decode a value from 16 bytes with implicit reduction.
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb127_decode16_reduce
	.thumb
	.thumb_func
	.type	gfb127_decode16_reduce, %function
gfb127_decode16_reduce:
	@ We must read the data word by word because the source might
	@ be unaligned.
	ldr	r2, [r1]
	ldr	r3, [r1, #4]
	stm	r0!, { r2, r3 }
	ldr	r2, [r1, #8]
	ldr	r3, [r1, #12]
	stm	r0!, { r2, r3 }
	bx	lr
	.size	gfb127_decode16_reduce, .-gfb127_decode16_reduce

@ =======================================================================
@ uint32_t gfb127_decode16(gfb127 *a, void *src)
@
@ Decode a value from 16 bytes. On success, 0xFFFFFFFF is returned. If
@ the value is invalid (top bit set), the destination value is set to
@ zero, and 0x00000000 is returned.
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb127_decode16
	.thumb
	.thumb_func
	.type	gfb127_decode16, %function
gfb127_decode16:
	@ We must read the data word by word because the source might
	@ be unaligned.
	ldr	r3, [r1, #12]
	asrs	r2, r3, #31
	bics	r3, r2
	str	r3, [r0, #12]
	ldr	r3, [r1]
	bics	r3, r2
	str	r3, [r0]
	ldr	r3, [r1, #4]
	bics	r3, r2
	str	r3, [r0, #4]
	ldr	r3, [r1, #8]
	bics	r3, r2
	str	r3, [r0, #8]
	mvns	r0, r2
	bx	lr
	.size	gfb127_decode16, .-gfb127_decode16

@ =======================================================================
@ GF(2^254) FUNCTIONS
@ =======================================================================

@ =======================================================================
@ void inner_gfb254_add(gfb254 *d, const gfb254 *a, const gfb254 *b)
@
@ Clobbers: core
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_gfb254_add, %function
inner_gfb254_add:
	ldm	r1!, { r3, r4, r5, r6 }
	ldm	r2!, { r7, r8, r10, r11 }
	eors	r3, r7
	eors	r4, r8
	eors	r5, r10
	eors	r6, r11
	stm	r0!, { r3, r4, r5, r6 }
	ldm	r1!, { r3, r4, r5, r6 }
	ldm	r2!, { r7, r8, r10, r11 }
	eors	r3, r7
	eors	r4, r8
	eors	r5, r10
	eors	r6, r11
	stm	r0!, { r3, r4, r5, r6 }
	bx	lr
	.size	inner_gfb254_add, .-inner_gfb254_add

@ =======================================================================
@ void inner_gfb254_mul_u(gfb254 *d, const gfb254 *a)
@
@ Clobbers: core
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_gfb254_mul_u, %function
inner_gfb254_mul_u:
	ldm	r1, { r1, r2, r3, r4, r5, r6, r7, r8 }
	eors	r1, r5
	eors	r2, r6
	eors	r3, r7
	eors	r4, r8
	stm	r0!, { r5, r6, r7, r8 }
	stm	r0!, { r1, r2, r3, r4 }
	bx	lr
	.size	inner_gfb254_mul_u, .-inner_gfb254_mul_u

@ =======================================================================
@ void inner_gfb254_mul_u1(gfb254 *d, const gfb254 *a)
@
@ Clobbers: core
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_gfb254_mul_u1, %function
inner_gfb254_mul_u1:
	ldm	r1, { r1, r2, r3, r4, r5, r6, r7, r8 }
	eors	r5, r1
	eors	r6, r2
	eors	r7, r3
	eors	r8, r4
	stm	r0!, { r5, r6, r7, r8 }
	stm	r0!, { r1, r2, r3, r4 }
	bx	lr
	.size	inner_gfb254_mul_u1, .-inner_gfb254_mul_u1

@ =======================================================================
@ void inner_gfb254_mul_sb(gfb254 *d, const gfb254 *a)
@
@ Clobbers: core
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_gfb254_mul_sb, %function
inner_gfb254_mul_sb:
	ldm	r1, { r1, r2, r3, r4, r5, r6, r7, r8 }
	B127_MUL_SB  r1, r2, r3, r4, r10
	B127_MUL_SB  r5, r6, r7, r8, r10
	stm	r0, { r1, r2, r3, r4, r5, r6, r7, r8 }
	bx	lr
	.size	inner_gfb254_mul_sb, .-inner_gfb254_mul_sb

@ =======================================================================
@ void inner_gfb254_mul_b(gfb254 *d, const gfb254 *a)
@
@ Clobbers: core
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_gfb254_mul_b, %function
inner_gfb254_mul_b:
	ldm	r1, { r1, r2, r3, r4, r5, r6, r7, r8 }
	B127_MUL_B  r1, r2, r3, r4, r10, r11
	B127_MUL_B  r5, r6, r7, r8, r10, r11
	stm	r0, { r1, r2, r3, r4, r5, r6, r7, r8 }
	bx	lr
	.size	inner_gfb254_mul_b, .-inner_gfb254_mul_b

@ =======================================================================
@ void inner_gfb254_mul(gfb254 *d, const gfb254 *a, const gfb254 *b)
@
@ Clobbers: core
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_gfb254_mul, %function
inner_gfb254_mul:
	push	{ r0, r1, r2, lr }
	sub	sp, #64

	@ sp[0] <- a0*b0
	mov	r0, sp
	bl	inner_gfb127_mul

	@ sp[1] <- a1*b1
	ldrd	r1, r2, [sp, #68]
	add	r0, sp, #16
	adds	r1, #16
	adds	r2, #16
	bl	inner_gfb127_mul

	@ sp[2] <- (a0 + a1)*(b0 + b1)
	ldrd	r1, r12, [sp, #68]
	ldm	r1, { r1, r2, r3, r4, r5, r6, r7, r11 }
	eors	r1, r5
	eors	r2, r6
	eors	r3, r7
	eor	r11, r11, r4
	ldm	r12, { r4, r5, r6, r7, r8, r10, r12, r14 }
	eor	r4, r4, r8
	eor	r5, r5, r10
	eor	r6, r6, r12
	eor	r7, r7, r14
	add	r0, sp, #32
	bl	inner_gfb127_mul_alt_entry

	@ d = (a0*b0 + a1*b1) + u*((a0 + a1)*(b0 + b1) + a0*b0)
	mov	r10, sp
	ldm	r10!, { r0, r1, r2, r3, r4, r5, r6, r7 }
	eors	r4, r0
	eors	r5, r1
	eors	r6, r2
	eors	r7, r3
	ldr	r8, [sp, #64]
	stm	r8!, { r4, r5, r6, r7 }
	ldm	r10, { r4, r5, r6, r7 }
	eors	r4, r0
	eors	r5, r1
	eors	r6, r2
	eors	r7, r3
	stm	r8!, { r4, r5, r6, r7 }

	add	sp, #76
	pop	{ pc }
	.size	inner_gfb254_mul, .-inner_gfb254_mul

@ =======================================================================
@ void gfb254_mul(gfb254 *d, const gfb254 *a, const gfb254 *b)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb254_mul
	.thumb
	.thumb_func
	.type	gfb254_mul, %function
gfb254_mul:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	bl	inner_gfb254_mul
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gfb254_mul, .-gfb254_mul

@ =======================================================================
@ void inner_gfb254_square(gfb254 *d, const gfb254 *a)
@
@ Clobbers: core
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_gfb254_square, %function
inner_gfb254_square:
	push	{ lr }

	@ We can use s0-s15 for temporary storage because
	@ inner_gfb127_square() does not modify these registers.
	vmov	s0, s1, r0, r1
	ldm	r1, { r0, r1, r2, r3 }
	bl	inner_gfb127_square
	vmov	s2, s3, r0, r1
	vmov	s4, s5, r2, r3
	vmov	r5, s1
	adds	r5, #16
	ldm	r5!, { r0, r1, r2, r3 }
	bl	inner_gfb127_square
	vmov	r8, s0
	vmov	r4, r5, s2, s3
	vmov	r6, r7, s4, s5
	eors	r4, r0
	eors	r5, r1
	eors	r6, r2
	eors	r7, r3
	stm	r8!, { r4, r5, r6, r7 }
	stm	r8!, { r0, r1, r2, r3 }

	pop	{ pc }
	.size	inner_gfb254_square, .-inner_gfb254_square

@ =======================================================================
@ void gfb254_square(gfb254 *d, const gfb254 *a)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb254_square
	.thumb
	.thumb_func
	.type	gfb254_square, %function
gfb254_square:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	bl	inner_gfb254_square
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gfb254_square, .-gfb254_square

@ =======================================================================
@ void inner_gfb254_mul_selfphi(gfb127 *d, const gfb254 *a)
@
@ Clobbers: core
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_gfb254_mul_selfphi, %function
inner_gfb254_mul_selfphi:
	push	{ r0, r1, lr }
	sub	sp, #36
	ldm	r1, { r0, r1, r2, r3, r4, r5, r6, r7 }
	eors	r0, r4
	eors	r1, r5
	eors	r2, r6
	eors	r3, r7
	bl	inner_gfb127_square
	mov	r4, sp
	stm	r4!, { r0, r1, r2, r3 }
	ldr	r1, [sp, #40]
	adds	r2, r1, #16
	add	r0, sp, #16
	bl	inner_gfb127_mul
	mov	r0, sp
	ldm	r0, { r0, r1, r2, r3, r4, r5, r6, r7 }
	eors	r0, r4
	eors	r1, r5
	eors	r2, r6
	eors	r3, r7
	ldr	r4, [sp, #36]
	stm	r4!, { r0, r1, r2, r3 }
	add	sp, #44
	pop	{ pc }
	.size	inner_gfb254_mul_selfphi, .-inner_gfb254_mul_selfphi

@ =======================================================================
@ void gfb254_mul_selfphi(gfb127 *d, const gfb254 *a)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb254_mul_selfphi
	.thumb
	.thumb_func
	.type	gfb254_mul_selfphi, %function
gfb254_mul_selfphi:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	bl	inner_gfb254_mul_selfphi
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gfb254_mul_selfphi, .-gfb254_mul_selfphi

@ =======================================================================
@ void inner_gfb254_invert(gfb254 *d, const gfb254 *a)
@
@ Clobbers: core
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_gfb254_invert, %function
inner_gfb254_invert:
	push	{ r0, r1, lr }
	sub	sp, #20

	@ t <- a*phi(a) (\in GF(2^127))
	mov	r0, sp
	bl	inner_gfb254_mul_selfphi

	@ t <- 1/t
	mov	r0, sp
	mov	r1, sp
	bl	gfb127_invert

	@ d0 <- t*(a0 + a1)
	ldrd	r8, r0, [sp, #20]
	ldm	r0, { r0, r1, r2, r3, r4, r5, r6, r7 }
	eors	r0, r4
	eors	r1, r5
	eors	r2, r6
	eors	r3, r7
	stm	r8, { r0, r1, r2, r3 }
	mov	r0, r8
	movs	r1, r0
	mov	r2, sp
	bl	inner_gfb127_mul

	@ d1 <- t*a1
	ldrd	r0, r1, [sp, #20]
	adds	r0, #16
	adds	r1, #16
	mov	r2, sp
	bl	inner_gfb127_mul

	add	sp, #28
	pop	{ pc }
	.size	inner_gfb254_invert, .-inner_gfb254_invert

@ =======================================================================
@ void gfb254_invert(gfb254 *d, const gfb254 *a)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb254_invert
	.thumb
	.thumb_func
	.type	gfb254_invert, %function
gfb254_invert:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	bl	inner_gfb254_invert
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gfb254_invert, .-gfb254_invert

@ =======================================================================
@ void gfb254_div(gfb254 *d, const gfb254 *a, const gfb254 *b)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb254_div
	.thumb
	.thumb_func
	.type	gfb254_div, %function
gfb254_div:
	push	{ r0, r1, r4, r5, r6, r7, r8, r10, r11, lr }
	sub	sp, #32

	mov	r0, sp
	movs	r1, r2
	bl	inner_gfb254_invert
	ldrd	r0, r1, [sp, #32]
	mov	r2, sp
	bl	inner_gfb254_mul

	add	sp, #40
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gfb254_div, .-gfb254_div

@ =======================================================================
@ void inner_gfb254_sqrt(gfb254 *d, const gfb254 *a)
@
@ Clobbers: core
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_gfb254_sqrt, %function
inner_gfb254_sqrt:
	push	{ r0, lr }
	ldm	r1, { r1, r2, r3, r4, r5, r6, r7, r8 }
	eors	r1, r5
	eors	r2, r6
	eors	r3, r7
	eors	r4, r8
	stm	r0, { r1, r2, r3, r4, r5, r6, r7, r8 }
	movs	r1, r0
	bl	inner_gfb127_sqrt
	pop	{ r0 }
	adds	r0, #16
	movs	r1, r0
	bl	inner_gfb127_sqrt
	pop	{ pc }
	.size	inner_gfb254_sqrt, .-inner_gfb254_sqrt

@ =======================================================================
@ void gfb254_sqrt(gfb254 *d, const gfb254 *a)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb254_sqrt
	.thumb
	.thumb_func
	.type	gfb254_sqrt, %function
gfb254_sqrt:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	bl	inner_gfb254_sqrt
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gfb254_sqrt, .-gfb254_sqrt

@ =======================================================================
@ void inner_gfb254_qsolve(gfb254 *d, const gfb254 *a)
@
@ Clobbers: core
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_gfb254_qsolve, %function
inner_gfb254_qsolve:
	push	{ r0, r1, lr }
	sub	sp, #20

	mov	r0, sp
	adds	r1, #16
	bl	inner_gfb127_halftrace
	mov	r0, sp
	bl	gfb127_trace
	movs	r4, r0
	ldr	r0, [sp, #24]
	bl	gfb127_trace
	eor	r2, r0, r4
	movs	r1, #0
	mov	r0, sp
	bl	gfb127_xor_bit
	ldr	r4, [sp, #20]
	adds	r4, #16
	mov	r5, sp
	ldm	r5!, { r0, r1, r2, r3 }
	stm	r4!, { r0, r1, r2, r3 }
	bl	inner_gfb127_square
	ldr	r7, [sp, #24]
	ldm	r7, { r4, r5, r6, r7 }
	eors	r4, r0
	eors	r5, r1
	eors	r6, r2
	eors	r7, r3
	mov	r1, sp
	stm	r1, { r4, r5, r6, r7 }
	ldr	r0, [sp, #20]
	bl	inner_gfb127_halftrace

	add	sp, #28
	pop	{ pc }
	.size	inner_gfb254_qsolve, .-inner_gfb254_qsolve

@ =======================================================================
@ void gfb254_qsolve(gfb254 *d, const gfb254 *a)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb254_qsolve
	.thumb
	.thumb_func
	.type	gfb254_qsolve, %function
gfb254_qsolve:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	bl	inner_gfb254_qsolve
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gfb254_qsolve, .-gfb254_qsolve

@ =======================================================================
@ void gfb254_encode(void *dst, const gfb254 *a)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb254_encode
	.thumb
	.thumb_func
	.type	gfb254_encode, %function
gfb254_encode:
	push	{ r0, r1, r2, lr }
	bl	gfb127_encode
	pop	{ r0, r1 }
	adds	r0, #16
	adds	r1, #16
	bl	gfb127_encode
	pop	{ r2, pc }
	.size	gfb254_encode, .-gfb254_encode

@ =======================================================================
@ void gfb254_decode32_reduce(gfb254 *d, const void *src)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb254_decode32_reduce
	.thumb
	.thumb_func
	.type	gfb254_decode32_reduce, %function
gfb254_decode32_reduce:
	@ We must read the data word by word because the source might
	@ be unaligned.
	ldr	r2, [r1], #4
	ldr	r3, [r1], #4
	ldr	r12, [r1], #4
	stm	r0!, { r2, r3, r12 }
	ldr	r2, [r1], #4
	ldr	r3, [r1], #4
	ldr	r12, [r1], #4
	stm	r0!, { r2, r3, r12 }
	ldr	r2, [r1], #4
	ldr	r3, [r1], #4
	stm	r0!, { r2, r3 }
	bx	lr
	.size	gfb254_decode32_reduce, .-gfb254_decode32_reduce

@ =======================================================================
@ uint32_t gfb254_decode32(gfb254 *d, const void *src)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb254_decode32
	.thumb
	.thumb_func
	.type	gfb254_decode32, %function
gfb254_decode32:
	push	{ r4, r5, r6, r7, lr }

	@ Read source word by word (it might be unaligned).
	ldr	r2, [r1]
	ldr	r3, [r1, #4]
	ldr	r4, [r1, #8]
	ldr	r5, [r1, #12]
	ldr	r6, [r1, #16]
	ldr	r7, [r1, #20]
	ldr	r12, [r1, #24]
	ldr	r14, [r1, #28]

	@ Clear all words if bit 127 or bit 255 is 1.
	asr	r1, r14, #31
	orr	r1, r1, r5, asr #31
	bics	r2, r1
	bics	r3, r1
	bics	r4, r1
	bics	r5, r1
	bics	r6, r1
	bics	r7, r1
	bics	r12, r1
	bics	r14, r1

	@ Store value.
	stm	r0, { r2, r3, r4, r5, r6, r7, r12, r14 }
	mvns	r0, r1

	pop	{ r4, r5, r6, r7, pc }
	.size	gfb254_decode32, .-gfb254_decode32

@ =======================================================================
@ GLS254 FUNCTIONS
@ =======================================================================

	.align	2
	.global	GLS254_NEUTRAL
GLS254_NEUTRAL:
	@ X = 0
	.long	0x00000000, 0x00000000, 0x00000000, 0x00000000
	.long	0x00000000, 0x00000000, 0x00000000, 0x00000000
	@ S = sqrt(b)
	.long	0x08000001, 0x00000000, 0x00000000, 0x00000000
	.long	0x00000000, 0x00000000, 0x00000000, 0x00000000
	@ Z = 1
	.long	0x00000001, 0x00000000, 0x00000000, 0x00000000
	.long	0x00000000, 0x00000000, 0x00000000, 0x00000000
	@ T = 0
	.long	0x00000000, 0x00000000, 0x00000000, 0x00000000
	.long	0x00000000, 0x00000000, 0x00000000, 0x00000000

	.global	GLS254_BASE
GLS254_BASE:
	@ X
	.long	0x326B8675, 0xB6412F20, 0x9AE29894, 0x657CB9F7
	.long	0xF66DD010, 0x3932450F, 0xB2E3915E, 0x14C6F62C
	@ S
	.long	0x023DC896, 0x5FADCA04, 0xA04300F1, 0x763522AD
	.long	0x9E07345A, 0x206E4C1E, 0x2381CA6D, 0x4F69A66A
	@ Z = 1
	.long	0x00000001, 0x00000000, 0x00000000, 0x00000000
	.long	0x00000000, 0x00000000, 0x00000000, 0x00000000
	@ T = X*Z
	.long	0x326B8675, 0xB6412F20, 0x9AE29894, 0x657CB9F7
	.long	0xF66DD010, 0x3932450F, 0xB2E3915E, 0x14C6F62C

@ =======================================================================
@ uint32_t gls254_isneutral(const gls254_point *p)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gls254_isneutral
	.thumb
	.thumb_func
	.type	gls254_isneutral, %function
gls254_isneutral:
	push	{ r0, lr }
	bl	gfb127_iszero
	pop	{ r1 }
	push	{ r0 }
	adds	r0, r1, #16
	bl	gfb127_iszero
	pop	{ r1, lr }
	ands	r0, r1
	bx	lr
	.size	gls254_isneutral, .-gls254_isneutral

@ =======================================================================
@ uint32_t gls254_equals(const gls254_point *p1, const gls254_point *p2)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gls254_equals
	.thumb
	.thumb_func
	.type	gls254_equals, %function
gls254_equals:
	push	{ r0, r1, r4, r5, r6, r7, r8, r10, r11, lr }
	sub	sp, #64

	@ S1*T2
	add	r2, r1, #96
	add	r1, r0, #32
	mov	r0, sp
	bl	inner_gfb254_mul

	@ S2*T1
	ldrd	r1, r2, [sp, #64]
	adds	r1, #96
	adds	r2, #32
	add	r0, sp, #32
	bl	inner_gfb254_mul

	@ Compare the two values.
	mov	r0, sp
	add	r1, sp, #32
	bl	gfb127_equals
	movs	r4, r0
	add	r0, sp, #16
	add	r1, sp, #48
	bl	gfb127_equals
	ands	r0, r4

	add	sp, #72
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gls254_equals, .-gls254_equals

@ =======================================================================
@ uint32_t gls254_set_cond(gls254_point *p2,
@                          const gls254_point *p1, uint32_t ctl)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gls254_set_cond
	.thumb
	.thumb_func
	.type	gls254_set_cond, %function
gls254_set_cond:
	uadd8	r2, r2, r2
	push	{ r4, r5, r6, r7, lr }

.macro	SET_COND_4X
	ldm	r0, { r2, r3, r4, r5 }
	ldm	r1!, { r6, r7, r12, r14 }
	sel	r2, r6, r2
	sel	r3, r7, r3
	sel	r4, r12, r4
	sel	r5, r14, r5
	stm	r0!, { r2, r3, r4, r5 }
.endm
	SET_COND_4X
	SET_COND_4X
	SET_COND_4X
	SET_COND_4X
	SET_COND_4X
	SET_COND_4X
	SET_COND_4X
	SET_COND_4X

	pop	{ r4, r5, r6, r7, pc }
	.size	gls254_set_cond, .-gls254_set_cond

@ =======================================================================
@ void gls254_encode(void *dst, const gls254_point *p)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gls254_encode
	.thumb
	.thumb_func
	.type	gls254_encode, %function
gls254_encode:
	push	{ r0, r4, r5, r6, r7, r8, r10, r11, lr }
	sub	sp, #36

	@ w <- sqrt(S/T)
	mov	r0, sp
	add	r2, r1, #96
	adds	r1, #32
	bl	gfb254_div
	mov	r0, sp
	mov	r1, sp
	bl	inner_gfb254_sqrt

	@ dst <- encode(w)
	ldr	r0, [sp, #36]
	mov	r1, sp
	bl	gfb254_encode

	add	sp, #40
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gls254_encode, .-gls254_encode

@ =======================================================================
@ void gls254_decode(gls254_point *p, const void *src)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gls254_decode
	.thumb
	.thumb_func
	.type	gls254_decode, %function
gls254_decode:
	push	{ r0, r4, r5, r6, r7, r8, r10, r11, lr }
	sub	sp, #100

	@ Stack layout
	@    0   t0
	@   32   t1
	@   64   t2
	@   96   r (one byte)
	@   99	wz (one byte)
	@  100   dst
	@ TODO: reduce stack usage by using the destination point as buffers

	@ t0 <- w = decode(src)   (status in r)
	@ wz <- w == 0  (one byte)
	mov	r0, sp
	bl	gfb254_decode32
	strb	r0, [sp, #96]
	movs	r4, r0
	mov	r0, sp
	bl	gfb127_iszero
	ands	r4, r0
	add	r0, sp, #16
	bl	gfb127_iszero
	ands	r4, r0
	strb	r4, [sp, #99]

	@ t2 <- w^2
	add	r0, sp, #64
	mov	r1, sp
	bl	inner_gfb254_square

	@ t0 <- d = w^2 + w + a
	mov	r0, sp
	mov	r1, sp
	add	r2, sp, #64
	bl	inner_gfb254_add
	ldr	r3, [sp, #16]
	eor	r3, r3, #1
	str	r3, [sp, #16]

	@ t1 <- e = b/d^2
	add	r0, sp, #32
	mov	r1, sp
	bl	inner_gfb254_square
	add	r0, sp, #32
	add	r1, sp, #32
	bl	inner_gfb254_invert
	add	r0, sp, #32
	add	r1, sp, #32
	bl	inner_gfb254_mul_b

	@ If Tr(e) = 1 then the input is not valid (or is zero).
	add	r0, sp, #48
	bl	gfb127_trace
	subs	r0, #1
	ldrb	r3, [sp, #96]
	ands	r3, r0
	strb	r3, [sp, #96]

	@ t1 <- x = d*qsolve(e)
	add	r0, sp, #32
	add	r1, sp, #32
	bl	inner_gfb254_qsolve
	add	r0, sp, #32
	add	r1, sp, #32
	mov	r2, sp
	bl	inner_gfb254_mul

	@ If Tr(x) = 1, then x += d  (i.e. t1 <- t1 + d)
	@ We write x in the destination point.
	add	r0, sp, #48
	bl	gfb127_trace
	rsbs	r0, r0, #0
	mov	r1, sp
	ldm	r1!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	ands	r2, r0
	ands	r3, r0
	ands	r4, r0
	ands	r5, r0
	ands	r6, r0
	ands	r7, r0
	ands	r8, r0
	ands	r10, r0
	ldr	r0, [sp, #100]
	ldm	r1!, { r11, r12, r14 }
	eors	r2, r11
	eors	r3, r12
	eors	r4, r14
	stm	r0!, { r2, r3, r4 }
	ldm	r1!, { r2, r3, r4, r11, r12 }
	eors	r2, r5
	eors	r3, r6
	eors	r4, r7
	eor	r5, r11, r8
	eor	r6, r12, r10
	stm	r0!, { r2, r3, r4, r5, r6 }

	@ t0 <- s = x*w^2
	sub	r1, r0, #32
	mov	r0, sp
	add	r2, sp, #64
	bl	inner_gfb254_mul

	@ Write s in the destination point (with the proper scaling).
	ldr	r0, [sp, #100]
	adds	r0, #32
	mov	r1, sp
	bl	inner_gfb254_mul_sb

	@ Z = sqrt(b), and T = X*Z = sqrt(b)*X
	ldr	r0, [sp, #100]
	adds	r0, #64
	movs	r1, #1
	movt	r1, #0x0800
	movs	r2, #0
	umull	r3, r4, r2, r2
	umull	r5, r6, r2, r2
	umull	r7, r8, r2, r2
	stm	r0, { r1, r2, r3, r4, r5, r6, r7, r8 }
	ldr	r0, [sp, #100]
	movs	r1, r0
	adds	r0, #96
	bl	inner_gfb254_mul_sb

	@ Replace the point with the neutral in case of failure.
	ldr	r0, [sp, #100]
	adr	r1, GLS254_NEUTRAL
	ldrsb	r2, [sp, #96]
	mvns	r2, r2
	bl	gls254_set_cond

	@ If w = 0 then we current have a failure status (and we set the
	@ output point to the neutral), but this is not a failure and we
	@ want to return the neutral.
	ldr	r1, [sp, #96]
	sxtb	r0, r1
	orr	r0, r0, r1, asr #31

	add	sp, #104
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gls254_decode, .-gls254_decode

@ =======================================================================
@ void inner_gls254_add(gls254_point *p3,
@                       const gls254_point *p1, const gls254_point *p2)
@
@ Clobbers: core
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_gls254_add, %function
inner_gls254_add:
	push	{ r0, r1, r2, lr }
	sub	sp, #128

	@ tmp0..tmp3 are 32-byte stack buffers.

	@ tmp0 <- D = (S1 + T1)*(S2 + T2)
	@ Clobbers: tmp1
	ldr	r1, [sp, #132]
	add	r2, r1, #96
	adds	r1, #32
	mov	r0, sp
	bl	inner_gfb254_add
	ldr	r1, [sp, #136]
	add	r2, r1, #96
	adds	r1, #32
	add	r0, sp, #32
	bl	inner_gfb254_add
	mov	r0, sp
	mov	r1, sp
	add	r2, sp, #32
	bl	inner_gfb254_mul

	@ tmp1 <- S1S2 = S1*S2
	ldrd	r1, r2, [sp, #132]
	adds	r1, #32
	adds	r2, #32
	add	r0, sp, #32
	bl	inner_gfb254_mul

	@ tmp2 <- E = (a^2)*T1*T2
	ldrd	r1, r2, [sp, #132]
	adds	r1, #96
	adds	r2, #96
	add	r0, sp, #64
	bl	inner_gfb254_mul
	add	r0, sp, #64
	add	r1, sp, #64
	bl	inner_gfb254_mul_u1

	@ tmp3 <- F = (X1*X2)^2
	ldrd	r1, r2, [sp, #132]
	add	r0, sp, #96
	bl	inner_gfb254_mul
	add	r0, sp, #96
	add	r1, sp, #96
	bl	inner_gfb254_square

	@ X3 <- D + S1S2 = tmp0 + tmp1
	@ Clobbers: X1, X2
	ldr	r0, [sp, #128]
	mov	r1, sp
	add	r2, sp, #32
	bl	inner_gfb254_add

	@ tmp0 <- D + E = tmp0 + tmp2
	@ tmp1 <- S1S2 + E = tmp1 + tmp2
	mov	r0, sp
	mov	r1, sp
	add	r2, sp, #64
	bl	inner_gfb254_add
	add	r0, sp, #32
	add	r1, sp, #32
	add	r2, sp, #64
	bl	inner_gfb254_add

	@ tmp2 <- G = (Z1*Z2)^2
	ldrd	r1, r2, [sp, #132]
	adds	r1, #64
	adds	r2, #64
	add	r0, sp, #64
	bl	inner_gfb254_mul
	add	r0, sp, #64
	add	r1, sp, #64
	bl	inner_gfb254_square

	@ S3 <- sqrt(b)*(G*(S1S2 + E) + F*(D + E))
	@     = sqrt(b)*(tmp1*tmp2 + tmp0*tmp3)
	@ Clobbers: tmp0, tmp1, S1, S2
	mov	r0, sp
	mov	r1, sp
	add	r2, sp, #96
	bl	inner_gfb254_mul
	add	r0, sp, #32
	add	r1, sp, #32
	add	r2, sp, #64
	bl	inner_gfb254_mul
	mov	r0, sp
	mov	r1, sp
	add	r2, sp, #32
	bl	inner_gfb254_add
	ldr	r0, [sp, #128]
	adds	r0, #32
	mov	r1, sp
	bl	inner_gfb254_mul_sb

	@ Z3 <- sqrt(b)*(F + G) = sqrt(b)*(tmp2 + tmp3)
	@ Clobbers: tmp2, Z1, Z2
	add	r0, sp, #64
	add	r1, sp, #64
	add	r2, sp, #96
	bl	inner_gfb254_add
	ldr	r0, [sp, #128]
	adds	r0, #64
	add	r1, sp, #64
	bl	inner_gfb254_mul_sb

	@ T3 <- X3*Z3
	@ Clobbers: T1, T2
	ldr	r0, [sp, #128]
	movs	r1, r0
	adds	r2, r0, #64
	adds	r0, #96
	bl	inner_gfb254_mul

	add	sp, #140
	pop	{ pc }
	.size	inner_gls254_add, .-inner_gls254_add

@ =======================================================================
@ void gls254_add(gls254_point *p3,
@                 const gls254_point *p1, const gls254_point *p2)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gls254_add
	.thumb
	.thumb_func
	.type	gls254_add, %function
gls254_add:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	bl	inner_gls254_add
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gls254_add, .-gls254_add

@ =======================================================================
@ void inner_gls254_add_affine(gls254_point *p3,
@                              const gls254_point *p1,
@                              const gls254_point_affine *p2)
@
@ Clobbers: core
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_gls254_add_affine, %function
inner_gls254_add_affine:
	push	{ r0, r1, r2, lr }
	sub	sp, #128

	@ tmp0..tmp3 are 32-byte stack buffers.

	@ Point p2 is in affine coordinates:
	@    X2 = p2->scaled_x
	@    S2 = p2->scaled_s
	@    Z2 = 1
	@    T2 = p2->scaled_x

	@ tmp0 <- D = (S1 + T1)*(S2 + T2)
	@ Clobbers: tmp1
	ldr	r1, [sp, #132]
	add	r2, r1, #96
	adds	r1, #32
	mov	r0, sp
	bl	inner_gfb254_add
	ldr	r1, [sp, #136]
	add	r2, r1, #32
	add	r0, sp, #32
	bl	inner_gfb254_add
	mov	r0, sp
	mov	r1, sp
	add	r2, sp, #32
	bl	inner_gfb254_mul

	@ tmp1 <- S1S2 = S1*S2
	ldrd	r1, r2, [sp, #132]
	adds	r1, #32
	adds	r2, #32
	add	r0, sp, #32
	bl	inner_gfb254_mul

	@ tmp2 <- E = (a^2)*T1*T2
	ldrd	r1, r2, [sp, #132]
	adds	r1, #96
	add	r0, sp, #64
	bl	inner_gfb254_mul
	add	r0, sp, #64
	add	r1, sp, #64
	bl	inner_gfb254_mul_u1

	@ tmp3 <- F = (X1*X2)^2
	ldrd	r1, r2, [sp, #132]
	add	r0, sp, #96
	bl	inner_gfb254_mul
	add	r0, sp, #96
	add	r1, sp, #96
	bl	inner_gfb254_square

	@ X3 <- D + S1S2 = tmp0 + tmp1
	@ Clobbers: X1, X2
	ldr	r0, [sp, #128]
	mov	r1, sp
	add	r2, sp, #32
	bl	inner_gfb254_add

	@ tmp0 <- D + E = tmp0 + tmp2
	@ tmp1 <- S1S2 + E = tmp1 + tmp2
	mov	r0, sp
	mov	r1, sp
	add	r2, sp, #64
	bl	inner_gfb254_add
	add	r0, sp, #32
	add	r1, sp, #32
	add	r2, sp, #64
	bl	inner_gfb254_add

	@ tmp2 <- G = (Z1*Z2)^2 = Z1^2  (since Z2 = 1)
	add	r0, sp, #64
	ldr	r1, [sp, #132]
	adds	r1, #64
	bl	inner_gfb254_square

	@ S3 <- sqrt(b)*(G*(S1S2 + E) + F*(D + E))
	@     = sqrt(b)*(tmp1*tmp2 + tmp0*tmp3)
	@ Clobbers: tmp0, tmp1, S1, S2
	mov	r0, sp
	mov	r1, sp
	add	r2, sp, #96
	bl	inner_gfb254_mul
	add	r0, sp, #32
	add	r1, sp, #32
	add	r2, sp, #64
	bl	inner_gfb254_mul
	mov	r0, sp
	mov	r1, sp
	add	r2, sp, #32
	bl	inner_gfb254_add
	ldr	r0, [sp, #128]
	adds	r0, #32
	mov	r1, sp
	bl	inner_gfb254_mul_sb

	@ Z3 <- sqrt(b)*(F + G) = sqrt(b)*(tmp2 + tmp3)
	@ Clobbers: tmp2, Z1, Z2
	add	r0, sp, #64
	add	r1, sp, #64
	add	r2, sp, #96
	bl	inner_gfb254_add
	ldr	r0, [sp, #128]
	adds	r0, #64
	add	r1, sp, #64
	bl	inner_gfb254_mul_sb

	@ T3 <- X3*Z3
	@ Clobbers: T1, T2
	ldr	r0, [sp, #128]
	movs	r1, r0
	adds	r2, r0, #64
	adds	r0, #96
	bl	inner_gfb254_mul

	add	sp, #140
	pop	{ pc }
	.size	inner_gls254_add_affine, .-inner_gls254_add_affine

@ =======================================================================
@ void gls254_add_affine(gls254_point *p3,
@                        const gls254_point *p1,
@                        const gls254_point_affine *p2)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gls254_add_affine
	.thumb
	.thumb_func
	.type	gls254_add_affine, %function
gls254_add_affine:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	bl	inner_gls254_add_affine
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gls254_add_affine, .-gls254_add_affine

@ =======================================================================
@ void inner_gls254_add_affine_affine(gls254_point *p3,
@                                     const gls254_point_affine *p1,
@                                     const gls254_point_affine *p2)
@
@ Clobbers: core
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_gls254_add_affine_affine, %function
inner_gls254_add_affine_affine:
	push	{ r0, r1, r2, lr }
	sub	sp, #128

	@ tmp0..tmp3 are 32-byte stack buffers.

	@ tmp0 <- D = (S1 + X1)*(S2 + X2)
	@ Clobbers: tmp1
	ldr	r1, [sp, #132]
	add	r2, r1, #32
	mov	r0, sp
	bl	inner_gfb254_add
	ldr	r1, [sp, #136]
	add	r2, r1, #32
	add	r0, sp, #32
	bl	inner_gfb254_add
	mov	r0, sp
	mov	r1, sp
	add	r2, sp, #32
	bl	inner_gfb254_mul

	@ tmp1 <- S1S2 = S1*S2
	ldrd	r1, r2, [sp, #132]
	adds	r1, #32
	adds	r2, #32
	add	r0, sp, #32
	bl	inner_gfb254_mul

	@ tmp2 <- X1X2 = X1*X2
	ldrd	r1, r2, [sp, #132]
	add	r0, sp, #64
	bl	inner_gfb254_mul

	@ tmp3 <- F = (X1*X2)^2
	add	r0, sp, #96
	add	r1, sp, #64
	bl	inner_gfb254_square

	@ tmp2 <- E = (a^2)*X1*X2
	add	r0, sp, #64
	add	r1, sp, #64
	bl	inner_gfb254_mul_u1

	@ X3 <- D + S1S2 = tmp0 + tmp1
	@ Clobbers: X1, X2
	ldr	r0, [sp, #128]
	mov	r1, sp
	add	r2, sp, #32
	bl	inner_gfb254_add

	@ tmp0 <- D + E = tmp0 + tmp2
	@ tmp1 <- S1S2 + E = tmp1 + tmp2
	mov	r0, sp
	mov	r1, sp
	add	r2, sp, #64
	bl	inner_gfb254_add
	add	r0, sp, #32
	add	r1, sp, #32
	add	r2, sp, #64
	bl	inner_gfb254_add

	@ S3 <- sqrt(b)*(S1S2 + E + F*(D + E))
	@     = sqrt(b)*(tmp1 + tmp0*tmp3)
	@ Clobbers: tmp0, S1, S2
	mov	r0, sp
	mov	r1, sp
	add	r2, sp, #96
	bl	inner_gfb254_mul
	mov	r0, sp
	mov	r1, sp
	add	r2, sp, #32
	bl	inner_gfb254_add
	ldr	r0, [sp, #128]
	adds	r0, #32
	mov	r1, sp
	bl	inner_gfb254_mul_sb

	@ Z3 <- sqrt(b)*(F + 1) = sqrt(b)*(tmp3 + 1)
	@ Clobbers: tmp3, Z1, Z2
	ldr	r0, [sp, #96]
	eor	r0, r0, #1
	str	r0, [sp, #96]
	ldr	r0, [sp, #128]
	adds	r0, #64
	add	r1, sp, #96
	bl	inner_gfb254_mul_sb

	@ T3 <- X3*Z3
	@ Clobbers: T1, T2
	ldr	r0, [sp, #128]
	movs	r1, r0
	adds	r2, r0, #64
	adds	r0, #96
	bl	inner_gfb254_mul

	add	sp, #140
	pop	{ pc }
	.size	inner_gls254_add_affine_affine, .-inner_gls254_add_affine_affine

@ =======================================================================
@ void gls254_add_affine_affine(gls254_point *p3,
@                               const gls254_point_affine *p1,
@                               const gls254_point_affine *p2)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gls254_add_affine_affine
	.thumb
	.thumb_func
	.type	gls254_add_affine_affine, %function
gls254_add_affine_affine:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	bl	inner_gls254_add_affine_affine
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gls254_add_affine_affine, .-gls254_add_affine_affine

@ =======================================================================
@ void inner_gls254_neg(gls254_point *p2, const gls254_point *p1)
@
@ Clobbers: core
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_gls254_neg, %function
inner_gls254_neg:
	cmp	r0, r1
	beq	Linner_gls254_neg_cont

	@ Copy X, Z, T
	ldm	r1!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	stm	r0!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	adds	r1, #32
	adds	r0, #32
	ldm	r1!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	stm	r0!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	ldm	r1!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	stm	r0!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	subs	r0, #128
	subs	r1, #128

Linner_gls254_neg_cont:
	adds	r2, r1, #96
	adds	r1, #32
	adds	r0, #32
	b	inner_gfb254_add   @ tail call
	.size	inner_gls254_neg, .-inner_gls254_neg

@ =======================================================================
@ void gls254_neg(gls254_point *p2, const gls254_point *p1)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gls254_neg
	.thumb
	.thumb_func
	.type	gls254_neg, %function
gls254_neg:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	bl	inner_gls254_neg
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gls254_neg, .-gls254_neg

@ =======================================================================
@ void inner_gls254_condneg(gls254_point *p2,
@                           const gls254_point *p1, uint32_t ctl)
@
@ Clobbers: core
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_gls254_condneg, %function
inner_gls254_condneg:
	cmp	r0, r1
	beq	Linner_gls254_condneg_cont

	@ Copy X, Z, T
	ldm	r1!, { r3, r4, r5, r6, r7, r8, r10, r11 }
	stm	r0!, { r3, r4, r5, r6, r7, r8, r10, r11 }
	adds	r1, #32
	adds	r0, #32
	ldm	r1!, { r3, r4, r5, r6, r7, r8, r10, r11 }
	stm	r0!, { r3, r4, r5, r6, r7, r8, r10, r11 }
	ldm	r1!, { r3, r4, r5, r6, r7, r8, r10, r11 }
	stm	r0!, { r3, r4, r5, r6, r7, r8, r10, r11 }
	subs	r0, #128
	subs	r1, #128

Linner_gls254_condneg_cont:
	adds	r0, #32        @ destination S
	add	r12, r1, #32   @ source S
	adds	r1, #96        @ source T
	ldm	r1!, { r3, r4, r5, r6 }
	ldm	r12!, { r7, r8, r10, r11 }
	ands	r3, r2
	ands	r4, r2
	ands	r5, r2
	ands	r6, r2
	eors	r3, r7
	eor	r4, r4, r8
	eor	r5, r5, r10
	eor	r6, r6, r11
	stm	r0!, { r3, r4, r5, r6 }
	ldm	r1!, { r3, r4, r5, r6 }
	ldm	r12!, { r7, r8, r10, r11 }
	ands	r3, r2
	ands	r4, r2
	ands	r5, r2
	ands	r6, r2
	eors	r3, r7
	eor	r4, r4, r8
	eor	r5, r5, r10
	eor	r6, r6, r11
	stm	r0!, { r3, r4, r5, r6 }
	bx	lr
	.size	inner_gls254_condneg, .-inner_gls254_condneg

@ =======================================================================
@ void gls254_condneg(gls254_point *p2, const gls254_point *p1, uint32_t ctl)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gls254_condneg
	.thumb
	.thumb_func
	.type	gls254_condneg, %function
gls254_condneg:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	bl	inner_gls254_condneg
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gls254_condneg, .-gls254_condneg

@ =======================================================================
@ void inner_gls254_sub(gls254_point *p3,
@                       const gls254_point *p1, const gls254_point *p2)
@
@ Clobbers: core
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_gls254_sub, %function
inner_gls254_sub:
	push	{ r0, r1, lr }
	sub	sp, #132

	mov	r0, sp
	movs	r1, r2
	bl	inner_gls254_neg
	ldrd	r0, r1, [sp, #132]
	mov	r2, sp
	bl	inner_gls254_add

	add	sp, #140
	pop	{ pc }
	.size	inner_gls254_sub, .-inner_gls254_sub

@ =======================================================================
@ void gls254_sub(gls254_point *p3,
@                 const gls254_point *p1, const gls254_point *p2)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gls254_sub
	.thumb
	.thumb_func
	.type	gls254_sub, %function
gls254_sub:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	bl	inner_gls254_sub
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gls254_sub, .-gls254_sub

@ =======================================================================
@ void inner_gls254_xdouble(gls254_point *p3,
@                           const gls254_point *p1, unsigned n)
@
@ WARNING: this function assumes that n != 0
@ Clobbers: core
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_gls254_xdouble, %function
inner_gls254_xdouble:
	push	{ r0, r1, r2, lr }
	sub	sp, #160

	@ Stack layout:
	@    0   X
	@   32   Y
	@   64   Z
	@   96   T
	@  128   tmp

	@ X <- sqrt(b)*X1
	mov	r0, sp
	ldr	r1, [sp, #164]
	bl	inner_gfb254_mul_sb
	@ T <- sqrt(b)*T1
	add	r0, sp, #96
	ldr	r1, [sp, #164]
	adds	r1, #96
	bl	inner_gfb254_mul_sb
	@ Z <- Z1
	add	r0, sp, #64
	ldr	r1, [sp, #164]
	adds	r1, #64
	ldm	r1, { r2, r3, r4, r5, r6, r7, r8, r10 }
	stm	r0, { r2, r3, r4, r5, r6, r7, r8, r10 }
	@ Y <- sqrt(b)*S1 + X^2 + a*T
	@ Clobbers: tmp
	add	r0, sp, #32
	ldr	r1, [sp, #164]
	adds	r1, #32
	bl	inner_gfb254_mul_sb
	add	r0, sp, #128
	mov	r1, sp
	bl	inner_gfb254_square
	add	r0, sp, #32
	add	r1, sp, #32
	add	r2, sp, #128
	bl	inner_gfb254_add
	add	r0, sp, #128
	add	r1, sp, #96
	bl	inner_gfb254_mul_u
	add	r0, sp, #32
	add	r1, sp, #32
	add	r2, sp, #128
	bl	inner_gfb254_add

Linner_gls254_xdouble_loop:
	@ tmp <- D = (X + sqrt(b)*Z)^2
	add	r0, sp, #128
	add	r1, sp, #64
	bl	inner_gfb254_mul_sb
	add	r0, sp, #128
	add	r1, sp, #128
	mov	r2, sp
	bl	inner_gfb254_add
	add	r0, sp, #128
	add	r1, sp, #128
	bl	inner_gfb254_square

	@ Z <- T^2
	add	r0, sp, #64
	add	r1, sp, #96
	bl	inner_gfb254_square

	@ X <- D^2
	mov	r0, sp
	add	r1, sp, #128
	bl	inner_gfb254_square

	@ tmp <- E = Y + T + D
	add	r0, sp, #128
	add	r1, sp, #128
	add	r2, sp, #32
	bl	inner_gfb254_add
	add	r0, sp, #128
	add	r1, sp, #128
	add	r2, sp, #96
	bl	inner_gfb254_add

	@ T <- X*Z
	add	r0, sp, #96
	mov	r1, sp
	add	r2, sp, #64
	bl	inner_gfb254_mul

	@ tmp <- (Y*E + (a + b)*Z)^2
	@ Clobbers: Y
	add	r0, sp, #32
	add	r1, sp, #32
	add	r2, sp, #128
	bl	inner_gfb254_mul
	add	r0, sp, #128
	add	r1, sp, #64
	bl	inner_gfb254_mul_u
	add	r0, sp, #32
	add	r1, sp, #32
	add	r2, sp, #128
	bl	inner_gfb254_add
	add	r0, sp, #128
	add	r1, sp, #64
	bl	inner_gfb254_mul_b
	add	r0, sp, #32
	add	r1, sp, #32
	add	r2, sp, #128
	bl	inner_gfb254_add
	add	r0, sp, #128
	add	r1, sp, #32
	bl	inner_gfb254_square

	@ Y <- tmp + (a + 1)*T
	add	r0, sp, #32
	add	r1, sp, #96
	bl	inner_gfb254_mul_u1
	add	r0, sp, #32
	add	r1, sp, #32
	add	r2, sp, #128
	bl	inner_gfb254_add

	@ Loop control
	ldr	r0, [sp, #168]
	subs	r0, #1
	str	r0, [sp, #168]
	bne	Linner_gls254_xdouble_loop

	@ Convert back the result to (x,s) coordinates on the right curve.

	@ X3 <- sqrt(b)*Z
	ldr	r0, [sp, #160]
	add	r1, sp, #64
	bl	inner_gfb254_mul_sb

	@ Z3 <- X
	ldr	r0, [sp, #160]
	adds	r0, #64
	mov	r1, sp
	ldm	r1, { r2, r3, r4, r5, r6, r7, r8, r10 }
	stm	r0, { r2, r3, r4, r5, r6, r7, r8, r10 }

	@ S3 <- sqrt(b)*(Y + (a + 1)*T + X^2)
	@ We will have Y + (a + 1)*T in tmp
	@ Clobbers: X
	mov	r0, sp
	mov	r1, sp
	bl	inner_gfb254_square
	mov	r0, sp
	mov	r1, sp
	add	r2, sp, #128
	bl	inner_gfb254_add
	ldr	r0, [sp, #160]
	adds	r0, #32
	mov	r1, sp
	bl	inner_gfb254_mul_sb

	@ T3 <- sqrt(b)*T
	ldr	r0, [sp, #160]
	adds	r0, #96
	add	r1, sp, #96
	bl	inner_gfb254_mul_sb

	add	sp, #172
	pop	{ pc }
	.size	inner_gls254_xdouble, .-inner_gls254_xdouble

@ =======================================================================
@ void gls254_xdouble(gls254_point *p3,
@                     const gls254_point *p1, unsigned n)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gls254_xdouble
	.thumb
	.thumb_func
	.type	gls254_xdouble, %function
gls254_xdouble:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	cmp	r2, #0
	bne	Lgls254_xdouble_cont
	cmp	r0, r1
	beq	Lgls254_xdouble_exit
	ldm	r1!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	stm	r0!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	ldm	r1!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	stm	r0!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	ldm	r1!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	stm	r0!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	ldm	r1!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	stm	r0!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	b	Lgls254_xdouble_exit

Lgls254_xdouble_cont:
	bl	inner_gls254_xdouble
Lgls254_xdouble_exit:
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gls254_xdouble, .-gls254_xdouble

@ =======================================================================
@ void inner_gls254_zeta_affine(gls254_point_affine *p2,
@                               const gls254_point_affine *p1, uint32_t zn)
@
@ Clobbers: core
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_gls254_zeta_affine, %function
inner_gls254_zeta_affine:
	@ zeta(x, s) = (x', s'):
	@   x' = (x0 + x1) + x1*u
	@   s' = (s0 + s1 + x0) + (s1 + x0 + x1)*u
	@ If zn is True, then s' should be replaced with s'':
	@   s'' = s' + x'
	@       = (s0 + s1 + x1) + (s1 + x0)*u

	@ Move zn to the GE flags.
	uadd8	r2, r2, r2

	@ x' <- (x0 + x1) + x1*u
	ldm	r1!, { r4, r5, r6, r7, r8, r10, r11, r12 }
	eor	r2, r4, r8
	eor	r3, r5, r10
	stm	r0!, { r2, r3 }
	eor	r2, r6, r11
	eor	r3, r7, r12
	stm	r0!, { r2, r3, r8, r10, r11, r12 }

	@ Prepare either x0 + (x0 + x1)*u (zn = 0) or x1 + x0*u (zn = -1)
	@ -> into r3:r4:r5:r6:r8:r10:r11:r12
	eor	r2, r4, r8
	sel	r3, r8, r4
	sel	r8, r4, r2
	eor	r2, r5, r10
	sel	r4, r10, r5
	sel	r10, r5, r2
	eor	r2, r6, r11
	sel	r5, r11, r6
	sel	r11, r6, r2
	eor	r2, r7, r12
	sel	r6, r12, r7
	sel	r12, r7, r2

	@ Add the prepared value to (s0 + s1) + s1*u
	ldr	r2, [r1]
	ldr	r7, [r1, #16]
	eors	r2, r7
	eors	r3, r2
	eor	r8, r8, r7
	ldr	r2, [r1, #4]
	ldr	r7, [r1, #20]
	eors	r2, r7
	eors	r4, r2
	eor	r10, r10, r7
	ldr	r2, [r1, #8]
	ldr	r7, [r1, #24]
	eors	r2, r7
	eors	r5, r2
	eor	r11, r11, r7
	ldr	r2, [r1, #12]
	ldr	r7, [r1, #28]
	eors	r2, r7
	eors	r6, r2
	eor	r12, r12, r7
	stm	r0, { r3, r4, r5, r6, r8, r10, r11, r12 }

	bx	lr
	.size	inner_gls254_zeta_affine, .-inner_gls254_zeta_affine

@ =======================================================================
@ void gls254_zeta_affine(gls254_point_affine *p2,
@                         const gls254_point_affine *p1, uint32_t zn)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gls254_zeta_affine
	.thumb
	.thumb_func
	.type	gls254_zeta_affine, %function
gls254_zeta_affine:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	bl	inner_gls254_zeta_affine
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gls254_zeta_affine, .-gls254_zeta_affine

@ =======================================================================
@ void gls254_make_window_affine_8(gls254_point_affine *win,
@                                  const gls254_point *p)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gls254_make_window_affine_8
	.thumb
	.thumb_func
	.type	gls254_make_window_affine_8, %function
gls254_make_window_affine_8:
	push	{ r0, r1, r4, r5, r6, r7, r8, r10, r11, lr }
	sub	sp, #384

	@  point   X         Z         point slot
	@   P      P         P+64       P
	@   8*P    win       win+64     win(0)
	@   3*P    win+128   win+192    win(1)
	@   5*P    win+256   win+320    win(2)
	@   7*P    win+384   win+448    win(3)
	@   2*P    sp        sp+64      sp(0)
	@   4*P    sp+128    sp+192     sp(1)
	@   6*P    sp+256    sp+320     sp(2)

	@ 2*P -> sp
	mov	r0, sp
	movs	r2, #1
	bl	inner_gls254_xdouble

	@ 3*P -> win+128
	ldrd	r0, r1, [sp, #384]
	adds	r0, #128
	mov	r2, sp
	bl	inner_gls254_add

	@ 5*P -> win+256
	ldr	r1, [sp, #384]
	adds	r1, #128
	add	r0, r1, #128
	mov	r2, sp
	bl	inner_gls254_add

	@ 7*P -> win+384
	ldr	r1, [sp, #384]
	add	r1, r1, #256
	add	r0, r1, #128
	mov	r2, sp
	bl	inner_gls254_add

	@ 4*P -> sp+128
	add	r0, sp, #128
	mov	r1, sp
	movs	r2, #1
	bl	inner_gls254_xdouble

	@ 6*P -> sp+256
	add	r0, sp, #256
	ldr	r1, [sp, #384]
	adds	r1, #128
	movs	r2, #1

	@ 8*P -> win
	bl	inner_gls254_xdouble
	ldr	r0, [sp, #384]
	add	r1, sp, #128
	movs	r2, #1
	bl	inner_gls254_xdouble

	@ Batch inversion of all Z coordinates.
	@ For all points, the X coordinate receives the product of all
	@ previous Z coordinates. P.X is unmodified. Points are processed
	@ in win-then-sp order. win(0).X receives the final product (instead
	@ of a copy of P.Z).

	@ Locations of X and Z coordinates:
	@  point   X         Z         point slot
	@   P      P         P+64       P
	@   8*P    win       win+64     win(0)
	@   3*P    win+128   win+192    win(1)
	@   5*P    win+256   win+320    win(2)
	@   7*P    win+384   win+448    win(3)
	@   2*P    sp        sp+64      sp(0)
	@   4*P    sp+128    sp+192     sp(1)
	@   6*P    sp+256    sp+320     sp(2)

	@ win(1).X <- P.Z * win(0).Z
	ldrd	r0, r1, [sp, #384]
	adds	r1, #64
	add	r2, r0, #64
	adds	r0, #128
	bl	inner_gfb254_mul

	@ win(2).X <- win(1).Z * win(1).X
	ldr	r0, [sp, #384]
	add	r1, r0, #192
	add	r2, r0, #128
	add	r0, r0, #256
	bl	inner_gfb254_mul

	@ win(3).X <- win(2).Z * win(2).X
	ldr	r0, [sp, #384]
	add	r1, r0, #320
	add	r2, r0, #256
	add	r0, r0, #384
	bl	inner_gfb254_mul

	@ sp(0).X <- win(3).Z * win(3).X
	mov	r0, sp
	ldr	r1, [sp, #384]
	add	r1, r1, #384
	add	r2, r1, #64
	bl	inner_gfb254_mul

	@ sp(1).X <- sp(0).Z * sp(0).X
	add	r0, sp, #128
	mov	r1, sp
	add	r2, sp, #64
	bl	inner_gfb254_mul

	@ sp(2).X <- sp(1).Z * sp(1).X
	add	r0, sp, #256
	add	r1, sp, #128
	add	r2, sp, #192
	bl	inner_gfb254_mul

	@ win(0).X <- sp(2).Z * sp(2).X
	ldr	r0, [sp, #384]
	add	r1, sp, #256
	add	r2, sp, #320
	bl	inner_gfb254_mul

	@ Invert the product of all Z coordinates (in win(0).X).
	ldr	r0, [sp, #384]
	movs	r1, r0
	bl	inner_gfb254_invert

	@ Propagate the inverse to all Z coordinates. 1/Z goes into
	@ the X slot, while win(0) is updated with the new product of
	@ of inverses.

	@ sp(k).X <- sp(k).X * win(0).X
	@ win(0).X <- sp(k).Z * win(0).X
.macro	WIN8_INV_SP  k
	add	r0, sp, #(128 * (\k))
	ldr	r1, [sp, #384]
	add	r2, sp, #(128 * (\k))
	bl	inner_gfb254_mul
	ldr	r0, [sp, #384]
	movs	r1, r0
	add	r2, sp, #(128 * (\k) + 64)
	bl	inner_gfb254_mul
.endm

	WIN8_INV_SP	2
	WIN8_INV_SP	1
	WIN8_INV_SP	0

	@ win(k).X <- win(k).X * win(0).X
	@ win(0).X <- win(k).Z * win(0).X
.macro	WIN8_INV_WIN  k
	ldr	r1, [sp, #384]
	add	r0, r1, #(128 * (\k))
	movs	r2, r0
	bl	inner_gfb254_mul
	ldr	r0, [sp, #384]
	movs	r1, r0
	add	r2, r0, #(128 * (\k) + 64)
	bl	inner_gfb254_mul
.endm

	WIN8_INV_WIN	3
	WIN8_INV_WIN	2
	WIN8_INV_WIN	1

	@ win(i).X and sp(j).X contain 1/win(i).Z and 1/sp(j).Z,
	@ for i = 1, 2, 3 and j = 0, 1, 2, respectively.
	@ win(0).X contains 1/(win(0).Z * P.Z).

	@ We normalize the points in win(i) (i = 1, 2, 3) and
	@ sp(j) (j = 0, 1, 2). Affine normalization is:
	@   scaled_x <- T/Z^2
	@   scaled_s <- S/Z^2

	@ sp(k).S <- sp(k).S * sp(k).X^2
	@ sp(k).X <- sp(k).T * sp(k).X^2
.macro	WIN8_NORM_SP  k
	add	r0, sp, #(128 * (\k))
	movs	r1, r0
	bl	inner_gfb254_square
	add	r0, sp, #(128 * (\k) + 32)
	movs	r1, r0
	add	r2, sp, #(128 * (\k))
	bl	inner_gfb254_mul
	add	r0, sp, #(128 * (\k))
	movs	r1, r0
	add	r2, sp, #(128 * (\k) + 96)
	bl	inner_gfb254_mul
.endm

	@ win(k).S <- win(k).S * win(k).X^2
	@ win(k).X <- win(k).T * win(k).X^2
.macro	WIN8_NORM_WIN  k
	ldr	r0, [sp, #384]
	.if (\k) >= 2
	add	r0, r0, #(128 * (\k))
	.elseif (\k) == 1
	adds	r0, #128
	.endif
	movs	r1, r0
	bl	inner_gfb254_square
	ldr	r1, [sp, #384]
	.if (\k) >= 2
	add	r1, r1, #(128 * (\k))
	.elseif (\k) == 1
	adds	r1, #128
	.endif
	add	r0, r1, #32
	movs	r2, r0
	bl	inner_gfb254_mul
	ldr	r1, [sp, #384]
	.if (\k) >= 2
	add	r1, r1, #(128 * (\k))
	.elseif (\k) == 1
	adds	r1, #128
	.endif
	add	r2, r1, #96
	movs	r0, r1
	bl	inner_gfb254_mul
.endm

	WIN8_NORM_SP	0
	WIN8_NORM_SP	1
	WIN8_NORM_SP	2
	WIN8_NORM_WIN	1
	WIN8_NORM_WIN	2
	WIN8_NORM_WIN	3

	@ sp+64 is now free. We use it to store 1/P.Z

	@ sp+64 <- 1/P.Z = win(0).X * win(0).Z
	add	r0, sp, #64
	ldr	r1, [sp, #384]
	add	r2, r1, #64
	bl	inner_gfb254_mul

	@ win(0).X <- 1/win(0).Z = win(0).X * P.Z
	ldrd	r0, r1, [sp, #384]
	adds	r1, #64
	movs	r2, r0
	bl	inner_gfb254_mul

	@ Normalize point win(0).
	WIN8_NORM_WIN	0

	@ Move the normalized point coordinates to their respective
	@ positions in the window. Points 3*P, 5*P and 7*P are already
	@ correct. Point 8*P is in win(0), while 2*P, 4*P and 6*P are
	@ in the stack.

	@ 8*P
	ldr	r1, [sp, #384]
	add	r0, r1, #448
	ldm	r1!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	stm	r0!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	ldm	r1!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	stm	r0!, { r2, r3, r4, r5, r6, r7, r8, r10 }

	@ 2*P
	sub	r0, r0, #448
	mov	r1, sp
	ldm	r1!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	stm	r0!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	ldm	r1!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	stm	r0!, { r2, r3, r4, r5, r6, r7, r8, r10 }

	@ 4*P
	adds	r0, #64
	adds	r1, #64
	ldm	r1!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	stm	r0!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	ldm	r1!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	stm	r0!, { r2, r3, r4, r5, r6, r7, r8, r10 }

	@ 6*P
	adds	r0, #64
	adds	r1, #64
	ldm	r1!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	stm	r0!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	ldm	r1!, { r2, r3, r4, r5, r6, r7, r8, r10 }
	stm	r0!, { r2, r3, r4, r5, r6, r7, r8, r10 }

	@ We now have all normalized points in the window, except P itself.
	@ sp+64 contains 1/P.Z, which we can apply now.

	add	r0, sp, #64
	add	r1, sp, #64
	bl	inner_gfb254_square
	ldrd	r0, r1, [sp, #384]
	adds	r1, #96
	add	r2, sp, #64
	bl	inner_gfb254_mul
	ldrd	r0, r1, [sp, #384]
	adds	r0, #32
	adds	r1, #32
	add	r2, sp, #64
	bl	inner_gfb254_mul

	add	sp, #392
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gls254_make_window_affine_8, .-gls254_make_window_affine_8

@ =======================================================================
@ void inner_gls254_lookup8_affine(gls254_point_affine *p,
@                                  const gls254_point_affine *win, int8_t k)
@
@ Clobbers: core
@ =======================================================================

	.align	1
	.thumb
	.thumb_func
	.type	inner_gls254_lookup8_affine, %function
inner_gls254_lookup8_affine:
	@ r3 <- sign(k)
	@ r4 <- 1 << (8 - abs(k)) (broadcast to all four byte positions).
	sxtb	r2, r2
	asrs	r3, r2, #31
	eor	r2, r3
	subs	r2, r3
	mov	r4, #0x100
	lsrs	r4, r2
	uxtb	r4, r4
	mov	r6, #0x01010101
	muls	r4, r6

	push	{ r0, r3, r4, lr }

	@ Prepared counter.
	mov	r14, r4

.macro	WIN8_LOOKUP_STEP  skip
	.if (\skip) != 0
	adds	r1, #32
	.endif
	uadd8	r14, r14, r14
	ldm	r1!, { r0, r2, r3 }
	sel	r4, r0, r4
	sel	r5, r2, r5
	sel	r6, r3, r6
	ldm	r1!, { r0, r2, r3 }
	sel	r7, r0, r7
	sel	r8, r2, r8
	sel	r10, r3, r10
	ldm	r1!, { r0, r2 }
	sel	r11, r0, r11
	sel	r12, r2, r12
.endm

	@ Read X coordinate; default value is zero (for the neutral).
	movs	r4, #0
	movs	r5, #0
	umull	r6, r7, r4, r4
	umull	r8, r10, r4, r4
	umull	r11, r12, r4, r4
	WIN8_LOOKUP_STEP  0
	WIN8_LOOKUP_STEP  1
	WIN8_LOOKUP_STEP  1
	WIN8_LOOKUP_STEP  1
	WIN8_LOOKUP_STEP  1
	WIN8_LOOKUP_STEP  1
	WIN8_LOOKUP_STEP  1
	WIN8_LOOKUP_STEP  1
	ldr	r0, [sp]
	stm	r0, { r4, r5, r6, r7, r8, r10, r11, r12 }

	@ Read S coordinate; default value is sqrt(b) (for the neutral).
	movs	r4, #1
	movt	r4, #0x0800
	movs	r5, #0
	umull	r6, r7, r5, r5
	umull	r8, r10, r5, r5
	umull	r11, r12, r5, r5
	ldr	r14, [sp, #8]
	sub	r1, r1, #448
	WIN8_LOOKUP_STEP  0
	WIN8_LOOKUP_STEP  1
	WIN8_LOOKUP_STEP  1
	WIN8_LOOKUP_STEP  1
	WIN8_LOOKUP_STEP  1
	WIN8_LOOKUP_STEP  1
	WIN8_LOOKUP_STEP  1
	WIN8_LOOKUP_STEP  1

	@ Conditional negate: add X to S, but only if k < 0.
	ldrd	r0, r14, [sp]
	ldm	r0!, { r1, r2, r3 }
	and	r1, r1, r14
	and	r2, r2, r14
	and	r3, r3, r14
	eors	r4, r1
	eors	r5, r2
	eors	r6, r3
	ldm	r0!, { r1, r2, r3 }
	and	r1, r1, r14
	and	r2, r2, r14
	and	r3, r3, r14
	eors	r7, r1
	eor	r8, r8, r2
	eors	r10, r10, r3
	ldm	r0!, { r1, r2 }
	and	r1, r1, r14
	and	r2, r2, r14
	eors	r11, r11, r1
	eors	r12, r12, r2

	@ Store S.
	stm	r0, { r4, r5, r6, r7, r8, r10, r11, r12 }

	add	sp, #12
	pop	{ pc }
	.size	inner_gls254_lookup8_affine, .-inner_gls254_lookup8_affine

@ =======================================================================
@ void gls254_lookup8_affine(gls254_point_affine *p,
@                            const gls254_point_affine *win, int8_t k)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gls254_lookup8_affine
	.thumb
	.thumb_func
	.type	gls254_lookup8_affine, %function
gls254_lookup8_affine:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	bl	inner_gls254_lookup8_affine
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gls254_lookup8_affine, .-gls254_lookup8_affine

@ =======================================================================
@ void gls254_normalize(gls254_point_affine *q, const gls254_point *p)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gls254_normalize
	.thumb
	.thumb_func
	.type	gls254_normalize, %function
gls254_normalize:
	push	{ r0, r1, r4, r5, r6, r7, r8, r10, r11, lr }
	sub	sp, #32

	@ [sp] <- 1/Z^2
	mov	r0, sp
	adds	r1, #64
	bl	inner_gfb254_square
	mov	r0, sp
	mov	r1, sp
	bl	inner_gfb254_invert

	@ Apply to the point coordinates.
	@ scaled_x = T/Z^2
	ldrd	r0, r1, [sp, #32]
	adds	r1, #96
	mov	r2, sp
	bl	inner_gfb254_mul
	@ scaled_s = S/Z^2
	ldrd	r0, r1, [sp, #32]
	adds	r0, #32
	adds	r1, #32
	mov	r2, sp
	bl	inner_gfb254_mul

	add	sp, #40
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gls254_normalize, .-gls254_normalize

@ =======================================================================
@ void gls254_from_affine(gls254_point *q, const gls254_point_affine *p)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gls254_from_affine
	.thumb
	.thumb_func
	.type	gls254_from_affine, %function
gls254_from_affine:
	push	{ r4, r5, r6, r7, r11, lr }

	@ X <- scaled_x
	ldm	r1!, { r2, r3, r4, r5, r6, r7, r11, r12 }
	stm	r0!, { r2, r3, r4, r5, r6, r7, r11, r12 }
	@ T <- scaled_x
	adds	r0, #64
	stm	r0, { r2, r3, r4, r5, r6, r7, r11, r12 }
	subs	r0, #64
	@ S <- scaled_s
	ldm	r1!, { r2, r3, r4, r5, r6, r7, r11, r12 }
	stm	r0!, { r2, r3, r4, r5, r6, r7, r11, r12 }
	@ Z <- 1
	movs	r2, #1
	movs	r3, #0
	umull	r4, r5, r3, r3
	umull	r6, r7, r3, r3
	umull	r11, r12, r3, r3
	stm	r0!, { r2, r3, r4, r5, r6, r7, r11, r12 }

	pop	{ r4, r5, r6, r7, r11, pc }
	.size	gls254_from_affine, .-gls254_from_affine

@ =======================================================================
@ int gls254_uncompressed_decode(gls254_point_affine *p, const void *src)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gls254_uncompressed_decode
	.thumb
	.thumb_func
	.type	gls254_uncompressed_decode, %function
gls254_uncompressed_decode:
	push	{ r0, r1, r4, r5, r6, r7, r8, r10, r11, lr }
	sub	sp, #64

	@ Decode the two coordinates.
	@ Decoding status is stored in r4.
	bl	gfb254_decode32
	movs	r4, r0
	ldrd	r0, r1, [sp, #64]
	adds	r0, #32
	adds	r1, #32
	bl	gfb254_decode32
	ands	r4, r0

	@ Compute the unscaled X coordinate and check that its trace is 0.
	@ We only need the high half of sqrt(b)*scaled_x.
	@ Status is then stored into sp+68
	mov	r0, sp
	ldr	r1, [sp, #64]
	adds	r1, #16
	bl	gfb127_mul_sb
	mov	r0, sp
	bl	gfb127_trace
	subs	r0, #1
	ands	r0, r4
	str	r0, [sp, #68]

	@ Verify the curve equation.
	@ In extended coordinates:
	@    S^2 + T*S = (sqrt(b)*X^2 + a*T + sqrt(b)*Z^2)^2
	@ We have an affine point, hence:
	@    X = scaled_x
	@    S = scaled_s
	@    Z = 1
	@    T = scaled_x

	@ sp <- sqrt(b)*(X^2 + Z^2)
	mov	r0, sp
	ldr	r1, [sp, #64]
	bl	inner_gfb254_square
	ldr	r0, [sp]
	eor	r0, r0, #1
	str	r0, [sp]
	mov	r0, sp
	mov	r1, sp
	bl	inner_gfb254_mul_sb

	@ sp <- (sqrt(b)*X^2 + a*T + sqrt(b)*Z^2)^2
	add	r0, sp, #32
	ldr	r1, [sp, #64]
	bl	inner_gfb254_mul_u
	mov	r0, sp
	mov	r1, sp
	add	r2, sp, #32
	bl	inner_gfb254_add
	mov	r0, sp
	mov	r1, sp
	bl	inner_gfb254_square

	@ sp+32 <- S^2 + S*T
	add	r0, sp, #32
	ldr	r1, [sp, #64]
	add	r2, r1, #32
	bl	inner_gfb254_add
	add	r0, sp, #32
	add	r1, sp, #32
	ldr	r2, [sp, #64]
	adds	r2, #32
	bl	inner_gfb254_mul

	@ Compare the two equation sides, and merge with the previously
	@ computed status.
	mov	r0, sp
	add	r1, sp, #32
	bl	gfb127_equals
	movs	r4, r0
	add	r0, sp, #16
	add	r1, sp, #48
	bl	gfb127_equals
	ands	r0, r4
	ldr	r1, [sp, #68]
	ands	r0, r1

	add	sp, #72
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gls254_uncompressed_decode, .-gls254_uncompressed_decode

@ =======================================================================
@ void gls254_uncompressed_encode(void *dst, const gls254_point_affine *p)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gls254_uncompressed_encode
	.thumb
	.thumb_func
	.type	gls254_uncompressed_encode, %function
gls254_uncompressed_encode:
	push	{ r0, r1, lr }
	sub	sp, #4

	bl	gfb254_encode
	ldrd	r0, r1, [sp, #4]
	adds	r0, #32
	adds	r1, #32
	bl	gfb254_encode

	add	sp, #12
	pop	{ pc }
	.size	gls254_uncompressed_encode, .-gls254_uncompressed_encode

@ =======================================================================
@ void gfb127_decode16_trunc(gfb127 *a, void *src)
@
@ Decode a value from 16 bytes, ignoring the most significant bit of
@ the last byte.
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb127_decode16_trunc
	.thumb
	.thumb_func
	.type	gfb127_decode16_trunc, %function
gfb127_decode16_trunc:
	@ We must read the data word by word because the source might
	@ be unaligned.
	ldr	r2, [r1]
	ldr	r3, [r1, #4]
	stm	r0!, { r2, r3 }
	ldr	r2, [r1, #8]
	ldr	r3, [r1, #12]
	ubfx	r3, r3, #0, #31
	stm	r0!, { r2, r3 }
	bx	lr
	.size	gfb127_decode16_trunc, .-gfb127_decode16_trunc

@ =======================================================================
@ void gfb254_decode32_trunc(gfb254 *d, const void *src)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gfb254_decode32_trunc
	.thumb
	.thumb_func
	.type	gfb254_decode32_trunc, %function
gfb254_decode32_trunc:
	@ We must read the data word by word because the source might
	@ be unaligned.
	ldr	r2, [r1], #4
	ldr	r3, [r1], #4
	ldr	r12, [r1], #4
	stm	r0!, { r2, r3, r12 }
	ldr	r2, [r1], #4
	ubfx	r2, r2, #0, #31
	ldr	r3, [r1], #4
	ldr	r12, [r1], #4
	stm	r0!, { r2, r3, r12 }
	ldr	r2, [r1], #4
	ldr	r3, [r1], #4
	ubfx	r3, r3, #0, #31
	stm	r0!, { r2, r3 }
	bx	lr
	.size	gfb254_decode32_trunc, .-gfb254_decode32_trunc

@ =======================================================================
@ void gls254_map_to_point(gls254_point *p, const void *src)
@
@ Uses the external ABI.
@ =======================================================================

	.align	1
	.global	gls254_map_to_point
	.thumb
	.thumb_func
	.type	gls254_map_to_point, %function
gls254_map_to_point:
	push	{ r0, r4, r5, r6, r7, r8, r10, r11, lr }
	sub	sp, #196

	@ Read the source bytes (which can be unaligned) and
	@ interpret them as a GF(2^254) element, by ignoring the
	@ top bits.
	mov	r0, sp
	bl	gfb254_decode32_trunc

	@ Force bits 0 and 1 of the high half to 1 and 0 respectively.
	@ Also remember the original trace (in sp[192]).
	ldr	r0, [sp, #16]
	ubfx	r1, r0, #0, #1
	str	r1, [sp, #192]
	movs	r2, #1
	bfi	r0, r2, #0, #2
	str	r0, [sp, #16]

	@ sp    <- m_1 = c (already there)
	@ sp+32 <- m_2 = c + z^2
	@ sp+64 <- m_3 = c + c^2/z^2
	add	r0, sp, #64
	mov	r1, sp
	bl	inner_gfb254_square
	add	r0, sp, #64
	add	r1, sp, #64
	bl	gfb127_div_z2
	add	r0, sp, #80
	add	r1, sp, #80
	bl	gfb127_div_z2
	add	r0, sp, #64
	add	r1, sp, #64
	mov	r2, sp
	bl	inner_gfb254_add
	mov	r0, sp
	ldm	r0, { r0, r1, r2, r3, r4, r5, r6, r7 }
	eor	r0, r0, #4
	add	r8, sp, #32
	stm	r8, { r0, r1, r2, r3, r4, r5, r6, r7 }

	@ sp+96  <- e_1 = b/m_1
	@ sp+128 <- e_2 = b/m_2
	@ sp+160 <- e_3 = b/m_3

	@ sp+96 <- m_1*m_2
	add	r0, sp, #96
	mov	r1, sp
	add	r2, sp, #32
	bl	inner_gfb254_mul
	@ sp+128 <- m_1*m_2*m_3
	add	r0, sp, #128
	add	r1, sp, #96
	add	r2, sp, #64
	bl	inner_gfb254_mul
	@ sp+128 <- 1/(m_1*m_2*m_3)
	add	r0, sp, #128
	add	r1, sp, #128
	bl	inner_gfb254_invert
	@ sp+160 <- 1/m_3
	add	r0, sp, #160
	add	r1, sp, #96
	add	r2, sp, #128
	bl	inner_gfb254_mul
	@ sp+128 <- 1/(m_1*m_2)
	add	r0, sp, #128
	add	r1, sp, #64
	add	r2, sp, #128
	bl	inner_gfb254_mul
	@ sp+96 <- 1/m_1
	add	r0, sp, #96
	add	r1, sp, #32
	add	r2, sp, #128
	bl	inner_gfb254_mul
	@ sp+128 <- 1/m_2
	add	r0, sp, #128
	mov	r1, sp
	add	r2, sp, #128
	bl	inner_gfb254_mul
	@ Multiplications by b.
	add	r0, sp, #96
	add	r1, sp, #96
	bl	inner_gfb254_mul_b
	add	r0, sp, #128
	add	r1, sp, #128
	bl	inner_gfb254_mul_b
	add	r0, sp, #160
	add	r1, sp, #160
	bl	inner_gfb254_mul_b

	@ Select the minimal index i such that Tr(e_i) = 0, and write
	@ m_i and e_i in sp and sp+96, respectively. The other m_j and
	@ e_j values are no longer needed and can be discarded.

	@ r12 = -1 if m_1+e_1 is not appropriate (i.e. Tr(e_1) = 1).
	@ r14 = -1 if m_1+e_1 and m_2+e_2 are both inappropriate.
	add	r0, sp, #(96 + 16)
	bl	gfb127_trace
	movs	r4, r0
	add	r0, sp, #(128 + 16)
	bl	gfb127_trace
	sbfx	r12, r4, #0, #1
	sbfx	r14, r0, #0, #1
	and	r14, r14, r12

.macro	SELECT3_128
	ldm	r0, { r3, r4, r5, r6 }
	ldm	r1!, { r7, r8, r10, r11 }
	uadd8	r12, r12, r12
	sel	r3, r7, r3
	sel	r4, r8, r4
	sel	r5, r10, r5
	sel	r6, r11, r6
	ldm	r2!, { r7, r8, r10, r11 }
	uadd8	r14, r14, r14
	sel	r3, r7, r3
	sel	r4, r8, r4
	sel	r5, r10, r5
	sel	r6, r11, r6
	stm	r0!, { r3, r4, r5, r6 }
.endm

	mov	r0, sp
	add	r1, sp, #32
	add	r2, sp, #64
	SELECT3_128
	SELECT3_128
	add	r0, sp, #96
	add	r1, sp, #128
	add	r2, sp, #160
	SELECT3_128
	SELECT3_128

	@ We now have:
	@   sp      m
	@   sp+96   e

	@ sp <- d = sqrt(m)
	mov	r0, sp
	mov	r1, sp
	bl	inner_gfb254_sqrt
	@ sp+32 <- w = qsolve(d)
	add	r0, sp, #32
	mov	r1, sp
	bl	inner_gfb254_qsolve
	@ Adjust lsb(w) to match the original trace of the input.
	ldr	r0, [sp, #32]
	ldr	r1, [sp, #192]
	bfi	r0, r1, #0, #1
	str	r0, [sp, #32]

	@ We now have:
	@   sp      d = w^2 + w + a
	@   sp+32   w
	@   sp+96   e = b/d^2 with Tr(e) = 0
	@ We can finish decoding w into a point:
	@   f <- qsolve(e)
	@   x <- d*f
	@   if Tr(x) = 1, then: x <- x + d
	@   s <- x*w^2

	@ sp+64 <- f = qsolve(e)
	add	r0, sp, #64
	add	r1, sp, #96
	bl	inner_gfb254_qsolve
	@ sp+64 <- x = d*f
	add	r0, sp, #64
	mov	r1, sp
	add	r2, sp, #64
	bl	inner_gfb254_mul
	@ Add d to x if Tr(x) = 1, and write to destination X.
	add	r0, sp, #80
	bl	gfb127_trace
	sbfx	r14, r0, #0, #1
	add	r4, sp, #64
	mov	r5, sp
	ldr	r6, [sp, #196]
	ldm	r4!, { r0, r1, r2, r3 }
	ldm	r5!, { r7, r8, r10, r11 }
	and	r7, r7, r14
	and	r8, r8, r14
	and	r10, r10, r14
	and	r11, r11, r14
	eor	r0, r0, r7
	eor	r1, r1, r8
	eor	r2, r2, r10
	eor	r3, r3, r11
	stm	r6!, { r0, r1, r2, r3 }
	ldm	r4!, { r0, r1, r2, r3 }
	ldm	r5!, { r7, r8, r10, r11 }
	and	r7, r7, r14
	and	r8, r8, r14
	and	r10, r10, r14
	and	r11, r11, r14
	eor	r0, r0, r7
	eor	r1, r1, r8
	eor	r2, r2, r10
	eor	r3, r3, r11
	stm	r6!, { r0, r1, r2, r3 }

	@ s <- x*w^2
	add	r0, sp, #32
	add	r1, sp, #32
	bl	inner_gfb254_square
	ldr	r0, [sp, #196]
	movs	r1, r0
	adds	r0, #32
	add	r2, sp, #32
	bl	inner_gfb254_mul

	@ Adjust the scaling of S, fill Z and T.
	ldr	r0, [sp, #196]
	adds	r0, #32
	movs	r1, r0
	bl	inner_gfb254_mul_sb
	ldr	r1, [sp, #196]
	movs	r0, r1
	adds	r0, #96
	bl	inner_gfb254_mul_sb
	ldr	r0, [sp, #196]
	adds	r0, #64
	movs	r1, #1
	movt	r1, #0x0800
	movs	r2, #0
	umull	r3, r4, r2, r2
	umull	r5, r6, r2, r2
	umull	r7, r8, r2, r2
	stm	r0, { r1, r2, r3, r4, r5, r6, r7, r8 }

	add	sp, #200
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	gls254_map_to_point, .-gls254_map_to_point

@ =======================================================================
@ UTILITY FUNCTIONS FOR BENCHMARKS
@ =======================================================================

@ Each bench_*() function measures the execution speed of a single
@ operation; it expects no input parameter, and returns the cost as a
@ cycle count (uint32_t).

	.align	1
	.thumb
	.thumb_func
	.type	inner_dummy, %function
inner_dummy:
	bx	lr
	.size	inner_dummy, .-inner_dummy

	.align	1
	.global	bench_dummy
	.thumb
	.thumb_func
	.type	bench_dummy, %function
bench_dummy:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	sub	sp, #8
	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r7, [r7]
	str	r7, [sp]

	bl	inner_dummy

	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r0, [r7]
	ldr	r1, [sp]
	subs	r0, r1
	subs	r0, #9
	add	sp, #8
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	bench_dummy, .-bench_dummy

	.align	1
	.global	bench_inner_gfb127_square
	.thumb
	.thumb_func
	.type	bench_inner_gfb127_square, %function
bench_inner_gfb127_square:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	sub	sp, #8
	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r7, [r7]
	str	r7, [sp]

	bl	inner_gfb127_square

	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r0, [r7]
	ldr	r1, [sp]
	subs	r0, r1
	subs	r0, #9
	add	sp, #8
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	bench_inner_gfb127_square, .-bench_inner_gfb127_square

	.align	1
	.global	bench_inner_gfb127_mul
	.thumb
	.thumb_func
	.type	bench_inner_gfb127_mul, %function
bench_inner_gfb127_mul:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	sub	sp, #56
	add	r0, sp, #8
	add	r1, sp, #24
	add	r2, sp, #40
	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r7, [r7]
	str	r7, [sp]

	bl	inner_gfb127_mul

	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r0, [r7]
	ldr	r1, [sp]
	subs	r0, r1
	add	sp, #56
	subs	r0, #9
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	bench_inner_gfb127_mul, .-bench_inner_gfb127_mul

	.align	1
	.global	bench_gfb127_invert
	.thumb
	.thumb_func
	.type	bench_gfb127_invert, %function
bench_gfb127_invert:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	sub	sp, #40
	add	r0, sp, #8
	add	r1, sp, #24
	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r7, [r7]
	str	r7, [sp]

	bl	gfb127_invert

	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r0, [r7]
	ldr	r1, [sp]
	subs	r0, r1
	subs	r0, #9
	add	sp, #40
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	bench_gfb127_invert, .-bench_gfb127_invert

	.align	1
	.global	bench_inner_gfb254_square
	.thumb
	.thumb_func
	.type	bench_inner_gfb254_square, %function
bench_inner_gfb254_square:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	sub	sp, #72
	add	r0, sp, #8
	add	r1, sp, #40
	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r7, [r7]
	str	r7, [sp]

	bl	inner_gfb254_square

	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r0, [r7]
	ldr	r1, [sp]
	subs	r0, r1
	add	sp, #72
	subs	r0, #9
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	bench_inner_gfb254_square, .-bench_inner_gfb254_square

	.align	1
	.global	bench_inner_gfb254_mul
	.thumb
	.thumb_func
	.type	bench_inner_gfb254_mul, %function
bench_inner_gfb254_mul:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	sub	sp, #104
	add	r0, sp, #8
	add	r1, sp, #40
	add	r2, sp, #72
	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r7, [r7]
	str	r7, [sp]

	bl	inner_gfb254_mul

	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r0, [r7]
	ldr	r1, [sp]
	subs	r0, r1
	subs	r0, #9
	add	sp, #104
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	bench_inner_gfb254_mul, .-bench_inner_gfb254_mul

	.align	1
	.global	bench_inner_gfb254_invert
	.thumb
	.thumb_func
	.type	bench_inner_gfb254_invert, %function
bench_inner_gfb254_invert:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	sub	sp, #72
	add	r0, sp, #8
	add	r1, sp, #40
	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r7, [r7]
	str	r7, [sp]

	bl	inner_gfb254_invert

	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r0, [r7]
	ldr	r1, [sp]
	subs	r0, r1
	add	sp, #72
	subs	r0, #9
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	bench_inner_gfb254_invert, .-bench_inner_gfb254_invert

	.align	1
	.global	bench_gls254_add
	.thumb
	.thumb_func
	.type	bench_gls254_add, %function
bench_gls254_add:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	sub	sp, #136
	add	r0, sp, #8
	add	r1, sp, #8
	add	r2, sp, #8
	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r7, [r7]
	str	r7, [sp]

	bl	gls254_add

	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r0, [r7]
	ldr	r1, [sp]
	subs	r0, r1
	subs	r0, #9
	add	sp, #136
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	bench_gls254_add, .-bench_gls254_add

	.align	1
	.global	bench_gls254_add_affine
	.thumb
	.thumb_func
	.type	bench_gls254_add_affine, %function
bench_gls254_add_affine:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	sub	sp, #136
	add	r0, sp, #8
	add	r1, sp, #8
	add	r2, sp, #8
	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r7, [r7]
	str	r7, [sp]

	bl	gls254_add_affine

	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r0, [r7]
	ldr	r1, [sp]
	subs	r0, r1
	subs	r0, #9
	add	sp, #136
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	bench_gls254_add_affine, .-bench_gls254_add_affine

	.align	1
	.global	bench_gls254_add_affine_affine
	.thumb
	.thumb_func
	.type	bench_gls254_add_affine_affine, %function
bench_gls254_add_affine_affine:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	sub	sp, #136
	add	r0, sp, #8
	add	r1, sp, #8
	add	r2, sp, #8
	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r7, [r7]
	str	r7, [sp]

	bl	gls254_add_affine_affine

	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r0, [r7]
	ldr	r1, [sp]
	subs	r0, r1
	subs	r0, #9
	add	sp, #136
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	bench_gls254_add_affine_affine, .-bench_gls254_add_affine_affine

	.align	1
	.global	bench_gls254_xdouble1
	.thumb
	.thumb_func
	.type	bench_gls254_xdouble1, %function
bench_gls254_xdouble1:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	sub	sp, #136
	add	r0, sp, #8
	add	r1, sp, #8
	movs	r2, #1
	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r7, [r7]
	str	r7, [sp]

	bl	gls254_xdouble

	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r0, [r7]
	ldr	r1, [sp]
	subs	r0, r1
	subs	r0, #9
	add	sp, #136
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	bench_gls254_xdouble1, .-bench_gls254_xdouble1

	.align	1
	.global	bench_gls254_xdouble2
	.thumb
	.thumb_func
	.type	bench_gls254_xdouble2, %function
bench_gls254_xdouble2:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	sub	sp, #136
	add	r0, sp, #8
	add	r1, sp, #8
	movs	r2, #2
	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r7, [r7]
	str	r7, [sp]

	bl	gls254_xdouble

	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r0, [r7]
	ldr	r1, [sp]
	subs	r0, r1
	subs	r0, #9
	add	sp, #136
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	bench_gls254_xdouble2, .-bench_gls254_xdouble2

	.align	1
	.global	bench_gls254_xdouble3
	.thumb
	.thumb_func
	.type	bench_gls254_xdouble3, %function
bench_gls254_xdouble3:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	sub	sp, #136
	add	r0, sp, #8
	add	r1, sp, #8
	movs	r2, #3
	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r7, [r7]
	str	r7, [sp]

	bl	gls254_xdouble

	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r0, [r7]
	ldr	r1, [sp]
	subs	r0, r1
	subs	r0, #9
	add	sp, #136
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	bench_gls254_xdouble3, .-bench_gls254_xdouble3

	.align	1
	.global	bench_gls254_xdouble4
	.thumb
	.thumb_func
	.type	bench_gls254_xdouble4, %function
bench_gls254_xdouble4:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	sub	sp, #136
	add	r0, sp, #8
	add	r1, sp, #8
	movs	r2, #4
	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r7, [r7]
	str	r7, [sp]

	bl	gls254_xdouble

	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r0, [r7]
	ldr	r1, [sp]
	subs	r0, r1
	subs	r0, #9
	add	sp, #136
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	bench_gls254_xdouble4, .-bench_gls254_xdouble4

	.align	1
	.global	bench_gls254_lookup8_affine
	.thumb
	.thumb_func
	.type	bench_gls254_lookup8_affine, %function
bench_gls254_lookup8_affine:
	push	{ r4, r5, r6, r7, r8, r10, r11, lr }
	sub	sp, #584
	add	r0, sp, #8
	add	r1, sp, #72
	movs	r2, #7
	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r7, [r7]
	str	r7, [sp]

	bl	gls254_lookup8_affine

	movw	r7, #0x1004
	movt	r7, #0xE000
	ldr	r0, [r7]
	ldr	r1, [sp]
	subs	r0, r1
	subs	r0, #9
	add	sp, #584
	pop	{ r4, r5, r6, r7, r8, r10, r11, pc }
	.size	bench_gls254_lookup8_affine, .-bench_gls254_lookup8_affine
