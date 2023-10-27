# GLS254 Curve Implementation for ARM Cortex M4

This file implements the GLS254 curve (binary elliptic curve defined
over GF(2^254)), using complete formulas and (x,s) coordinates. Usual
functionalities are offered:

  - Encoding and decoding of points. Encoding format is 32 bytes
    (compressed format) and is canonical (verified upon decoding).

  - Hash-to-curve.

  - Point multiplication by a scalar.

  - Digital signatures (Schnorr); signature size is 48 bytes.

  - Key exchange (ECDH).

Everything is strictly constant-time (except signature verification,
which uses only public data). This code assumes an ARM Cortex M4
microcontroller, with floating-point support. Floating-point
_operations_ are not actually used; but the floating-point registers are
used as storage area for temporary 32-bit values (access is somewhat
faster than stack slots). All low-level operations are implemented in
assembly. The code should be compatible with all relevant ABIs (in
particular, register r9, which is formally reserved, is not used at all,
making the code compatible with platforms where r9 may be used by
asynchronous code).

The default `Makefile` compiles the code with `arm-linux-gcc`, assuming
that it points to a C compiler with libc support (see
[Buildroot](https://buildroot.org/) for setting up such an environment);
the resulting binary can be tested with [QEMU](https://www.qemu.org/).

## Benchmarks

The code was also run on a real ARM Cortex M4 board (STM32F407
microcontroller) and the following benchmarks were measured (in clock
cycles):

| Operation                           |   cycles |
| :---------------------------------- | -------: |
| GF(2^254) squaring                  |      183 |
| GF(2^254) multiplication            |     1875 |
| GF(2^254) inversion                 |    14554 |
| GLS254 key pair generation          |  1018973 |
| GLS254 load private key             |  1016515 |
| GLS254 load public key              |    21939 |
| GLS254 ECDH                         |  1674074 |
| GLS254 signature generation         |  1034123 |
| GLS254 signature verification (avg) |  1735470 |

Cost for GF(2^254) low-level operations are for an internal ABI which
does not include register saving; costs for GLS254 high-level operations
are for calling the exteral API functions from C.

  - "Key pair generation" involves hashing a provided entropy seed with
    BLAKE2s to obtain a secret scalar, then computing the public key
    (multiplication of the curve conventional generator by the secret
    scalar). The encoded public key is also generated.

  - "Load private key" is similar to key pair generation, except that
    the secret scalar is provided (encoded), and no hashing is involved.
    Almost all of the cost is the public key computation.

  - "Load public key" is decoding an input 32-byte value into a public
    key (i.e. a GLS254 point). This includes validating that the key is
    correct (i.e. the bytes are the canonical encoding of a curve point
    in the correct prime-order group).

  - "ECDH" is the combination of multiplying a decoded public key (from
    the peer) by the secret scalar in a private key; the resulting point
    is encoded, and a 32-byte symmetric key is derived from that point
    (using BLAKE2s for hashing, and binding the output to the involved
    public keys).

  - "Signature generation" is the generation of a signature on a
    short message, using a decoded private key; the signature has length
    48 bytes.

  - "Signature verification" is the verification of a signature (48
    bytes) against a given short message, and a decoded public key.
    Since this operation is not constant-time, its running time may
    slightly vary (though not a lot) and the value above is an average
    over 10000 signature verifications. The public key is assumed to be
    already decoded; add the cost of decoding ("load public key") to get
    the verification cost starting with the 32-byte encoded public key.

## Discussion

GLS254 is a binary curve; the curve parameters are the same as described
in the [Aardal-Aranha 2022 paper](https://eprint.iacr.org/2022/748),
adapted for use with (x,s) coordinates as described in [Pornin
2022](https://eprint.iacr.org/2022/1325).

Binary curves are especially fast on architectures that offer a
carryless multiplication opcode (e.g. `pclmulqdq` on recent x86 CPUs).
The ARM Cortex M4 does _not_ provide such an opcode; instead, one must
use various tricks (e.g. many integer multiplications with masks), which
make binary curves expensive on such platforms. The benchmarks above
show that the GLS254 curve is indeed expensive, but not catastrophically
so; e.g. signature verification is about 2.5 times slower than Ed25519,
assuming an optimized implementation of the latter.
