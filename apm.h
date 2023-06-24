#ifndef APM_H
#define APM_H

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "mem.h"

/* LP64, X86_64, AMD64, AARCH64 */
#define DIGIT_SIZE 8
#define DIGIT_BITS 64U

#define zero(u, size) memset((u), 0, DIGIT_SIZE *(size))
#define copy(u, size, v) memmove((v), (u), DIGIT_SIZE *(size))

static inline uint64_t *enew(uint32_t size)
{
    return MALLOC(size * DIGIT_SIZE);
}

static inline uint64_t *new0(uint32_t size)
{
    return zero(enew(size), size);
}

static inline uint64_t *resize(uint64_t *u, uint32_t size)
{
    if (u)
        return REALLOC(u, size * DIGIT_SIZE);
    return enew(size);
}

#define APM_TMP_ALLOC(size) enew(size)
#define APM_TMP_FREE(num) FREE(num)
#define APM_TMP_COPY(num, size) \
    memcpy(APM_TMP_ALLOC(size), (num), (size) *DIGIT_SIZE)

/* Return real size of u[size] with leading zeros removed. */
static inline uint32_t rsize(const uint64_t *u, uint32_t size)
{
    u += size;
    while (size && !*--u)
        --size;
    return size;
}

int cmp_n(const uint64_t *u, const uint64_t *v, uint32_t size)
{
    u += size;
    v += size;
    while (size--) {
        --u;
        --v;
        if (*u < *v)
            return -1;
        if (*u > *v)
            return +1;
    }
    return 0;
}

#define APM_NORMALIZE(u, usize)         \
    while ((usize) && !(u)[(usize) -1]) \
        --(usize);

int cmp(const uint64_t *u, uint32_t usize, const uint64_t *v, uint32_t vsize)
{
    APM_NORMALIZE(u, usize);
    APM_NORMALIZE(v, vsize);
    if (usize < vsize)
        return -1;
    if (usize > vsize)
        return +1;
    return usize ? cmp_n(u, v, usize) : 0;
}

static uint64_t inc(uint64_t *u, uint32_t size)
{
    if (size == 0)
        return 1;

    for (; size--; ++u) {
        if (++*u)
            return 0;
    }
    return 1;
}

static uint64_t dec(uint64_t *u, uint32_t size)
{
    if (size == 0)
        return 1;

    for (;;) {
        if (u[0]--)
            return 0;
        if (--size == 0)
            return 1;
        u++;
    }
    return 1;
}

uint64_t daddi(uint64_t *u, uint32_t size, uint64_t v)
{
    if (v == 0 || size == 0)
        return v;
    return ((u[0] += v) < v) ? inc(&u[1], size - 1) : 0;
}

uint64_t add_n(const uint64_t *u, const uint64_t *v, uint32_t size, uint64_t *w)
{
    uint64_t cy = 0;
    while (size--) {
        uint64_t ud = *u++;
        const uint64_t vd = *v++;
        cy = (ud += cy) < cy;
        cy += (*w = ud + vd) < vd;
        ++w;
    }
    return cy;
}

uint64_t add(const uint64_t *u,
             uint32_t usize,
             const uint64_t *v,
             uint32_t vsize,
             uint64_t *w)
{
    if (usize < vsize) {
        uint64_t cy = add_n(u, v, usize, w);
        if (v != w)
            copy(v + usize, vsize - usize, w + usize);
        return cy ? inc(w + usize, vsize - usize) : 0;
    } else if (usize > vsize) {
        uint64_t cy = add_n(u, v, vsize, w);
        if (u != w)
            copy(u + vsize, usize - vsize, w + vsize);
        return cy ? inc(w + vsize, usize - vsize) : 0;
    }
    /* usize == vsize */
    return add_n(u, v, usize, w);
}

#define addi_n(u, v, size) add_n(u, v, size, u)

uint64_t addi(uint64_t *u, uint32_t usize, const uint64_t *v, uint32_t vsize)
{
    uint64_t cy = addi_n(u, v, vsize);
    return cy ? inc(u + vsize, usize - vsize) : 0;
}

uint64_t subi_n(uint64_t *u, const uint64_t *v, uint32_t size)
{
    uint64_t cy = 0;
    while (size--) {
        uint64_t vd = *v++;
        const uint64_t ud = *u;
        cy = (vd += cy) < cy;
        cy += (*u -= vd) > ud;
        ++u;
    }
    return cy;
}

uint64_t sub_n(const uint64_t *u, const uint64_t *v, uint32_t size, uint64_t *w)
{
    uint64_t cy = 0;
    while (size--) {
        const uint64_t ud = *u++;
        uint64_t vd = *v++;
        cy = (vd += cy) < cy;
        cy += (*w = ud - vd) > ud;
        ++w;
    }
    return cy;
}

uint64_t subi(uint64_t *u, uint32_t usize, const uint64_t *v, uint32_t vsize)
{
    return subi_n(u, v, vsize) ? dec(u + vsize, usize - vsize) : 0;
}

uint64_t sub(const uint64_t *u,
             uint32_t usize,
             const uint64_t *v,
             uint32_t vsize,
             uint64_t *w)
{
    if (usize == vsize)
        return sub_n(u, v, usize, w);

    uint64_t cy = sub_n(u, v, vsize, w);
    usize -= vsize;
    w += vsize;
    copy(u + vsize, usize, w);
    return cy ? dec(w, usize) : 0;
}

#define digit_mul(u, v, hi, lo) \
    __asm__("mulq %3" : "=a"(lo), "=d"(hi) : "%0"(u), "rm"(v))
#define digit_sqr(u, hi, lo) digit_mul((u), (u), (hi), (lo))

uint64_t dmul(const uint64_t *u, uint32_t size, uint64_t v, uint64_t *w)
{
    if (v <= 1) {
        if (v == 0)
            zero(w, size);
        else
            copy(u, size, w);
        return 0;
    }

    uint64_t cy = 0;
    while (size--) {
        uint64_t p1, p0;
        digit_mul(*u, v, p1, p0);
        cy = ((p0 += cy) < cy) + p1;
        *w++ = p0;
        ++u;
    }
    return cy;
}

uint64_t dmul_add(const uint64_t *u, uint32_t size, uint64_t v, uint64_t *w)
{
    if (v <= 1)
        return v ? addi_n(w, u, size) : 0;

    uint64_t cy = 0;
    while (size--) {
        uint64_t p1, p0;
        digit_mul(*u, v, p1, p0);
        cy = ((p0 += cy) < cy) + p1;
        cy += ((*w += p0) < p0);
        ++u;
        ++w;
    }
    return cy;
}

#define KARATSUBA_MUL_THRESHOLD 32
#define KARATSUBA_SQR_THRESHOLD 64

#ifndef SWAP
#define SWAP(x, y)           \
    do {                     \
        typeof(x) __tmp = x; \
        x = y;               \
        y = __tmp;           \
    } while (0)
#endif

/* Set w[usize + vsize] = u[usize] * v[vsize]. */
void mul(const uint64_t *u,
         uint32_t usize,
         const uint64_t *v,
         uint32_t vsize,
         uint64_t *w);

/* Set v[usize*2] = u[usize]^2. */
void sqr(const uint64_t *u, uint32_t usize, uint64_t *v);

uint64_t lshift(const uint64_t *u,
                uint32_t size,
                unsigned int shift,
                uint64_t *v)
{
    if (!size)
        return 0;

    shift &= DIGIT_BITS - 1;
    if (!shift) {
        if (u != v)
            copy(u, size, v);
        return 0;
    }

    const unsigned int subp = DIGIT_BITS - shift;
    uint64_t q = 0;
    do {
        const uint64_t p = *u++;
        *v++ = (p << shift) | q;
        q = p >> subp;
    } while (--size);
    return q;
}

uint64_t lshifti(uint64_t *u, uint32_t size, unsigned int shift)
{
    shift &= DIGIT_BITS - 1;
    if (!size || !shift)
        return 0;

    const unsigned int subp = DIGIT_BITS - shift;
    uint64_t q = 0;
    do {
        const uint64_t p = *u;
        *u++ = (p << shift) | q;
        q = p >> subp;
    } while (--size);
    return q;
}

/* Multiply u[usize] by v[vsize] and store the result in w[usize + vsize],
 * using the simple quadratic-time algorithm.
 */
void _mul_base(const uint64_t *u,
               uint32_t usize,
               const uint64_t *v,
               uint32_t vsize,
               uint64_t *w)
{
    /* Find real sizes and zero any part of answer which will not be set. */
    uint32_t ul = rsize(u, usize);
    uint32_t vl = rsize(v, vsize);
    /* Zero digits which will not be set in multiply-and-add loop. */
    if (ul + vl != usize + vsize)
        zero(w + (ul + vl), usize + vsize - (ul + vl));
    /* One or both are zero. */
    if (!ul || !vl)
        return;

    /* Now multiply by forming partial products and adding them to the result
     * so far. Rather than zero the low ul digits of w before starting, we
     * store, rather than add, the first partial product.
     */
    uint64_t *wp = w + ul;
    *wp = dmul(u, ul, *v, w);
    while (--vl) {
        uint64_t vd = *++v;
        *++wp = dmul_add(u, ul, vd, ++w);
    }
}

/* TODO: switch to Schönhage–Strassen algorithm
 * https://en.wikipedia.org/wiki/Sch%C3%B6nhage%E2%80%93Strassen_algorithm
 */

/* Karatsuba multiplication [cf. Knuth 4.3.3, vol.2, 3rd ed, pp.294-295]
 * Given U = U1*2^N + U0 and V = V1*2^N + V0,
 * we can recursively compute U*V with
 * (2^2N + 2^N)U1*V1 + (2^N)(U1-U0)(V0-V1) + (2^N + 1)U0*V0
 *
 * We might otherwise use
 * (2^2N - 2^N)U1*V1 + (2^N)(U1+U0)(V1+V0) + (1 - 2^N)U0*V0
 * except that (U1+U0) or (V1+V0) may become N+1 bit numbers if there is carry
 * in the additions, and this will slow down the routine.  However, if we use
 * the first formula the middle terms will not grow larger than N bits.
 */
static void mul_n(const uint64_t *u,
                  const uint64_t *v,
                  uint32_t size,
                  uint64_t *w)
{
    /* TODO: Only allocate a temporary buffer which is large enough for all
     * following recursive calls, rather than allocating at each call.
     */
    if (u == v) {
        sqr(u, size, w);
        return;
    }

    if (size < KARATSUBA_MUL_THRESHOLD) {
        _mul_base(u, size, v, size, w);
        return;
    }

    const bool odd = size & 1;
    const uint32_t even_size = size - odd;
    const uint32_t half_size = even_size / 2;

    const uint64_t *u0 = u, *u1 = u + half_size;
    const uint64_t *v0 = v, *v1 = v + half_size;
    uint64_t *w0 = w, *w1 = w + even_size;

    /* U0 * V0 => w[0..even_size-1]; */
    /* U1 * V1 => w[even_size..2*even_size-1]. */
    if (half_size >= KARATSUBA_MUL_THRESHOLD) {
        mul_n(u0, v0, half_size, w0);
        mul_n(u1, v1, half_size, w1);
    } else {
        _mul_base(u0, half_size, v0, half_size, w0);
        _mul_base(u1, half_size, v1, half_size, w1);
    }

    /* Since we cannot add w[0..even_size-1] to w[half_size ...
     * half_size+even_size-1] in place, we have to make a copy of it now.
     * This later gets used to store U1-U0 and V0-V1.
     */
    uint64_t *tmp = APM_TMP_COPY(w0, even_size);

    /* w[half_size..half_size+even_size-1] += U1*V1. */
    addi_n(w + half_size, w1, even_size);
    /* w[half_size..half_size+even_size-1] += U0*V0. */
    addi_n(w + half_size, tmp, even_size);

    /* Get absolute value of U1-U0. */
    uint64_t *u_tmp = tmp;
    bool prod_neg = cmp_n(u1, u0, half_size) < 0;
    if (prod_neg)
        sub_n(u0, u1, half_size, u_tmp);
    else
        sub_n(u1, u0, half_size, u_tmp);

    /* Get absolute value of V0-V1. */
    uint64_t *v_tmp = tmp + half_size;
    if (cmp_n(v0, v1, half_size) < 0)
        sub_n(v1, v0, half_size, v_tmp), prod_neg ^= 1;
    else
        sub_n(v0, v1, half_size, v_tmp);

    /* tmp = (U1-U0)*(V0-V1). */
    tmp = APM_TMP_ALLOC(even_size);
    if (half_size >= KARATSUBA_MUL_THRESHOLD)
        mul_n(u_tmp, v_tmp, half_size, tmp);
    else
        _mul_base(u_tmp, half_size, v_tmp, half_size, tmp);
    APM_TMP_FREE(u_tmp);

    /* Now add / subtract (U1-U0)*(V0-V1) from
     * w[half_size..half_size+even_size-1] based on whether it is negative or
     * positive.
     */
    if (prod_neg)
        subi_n(w + half_size, tmp, even_size);
    else
        addi_n(w + half_size, tmp, even_size);
    APM_TMP_FREE(tmp);

    /* Now if there was any carry from the middle digits (which is at most 2),
     * add that to w[even_size+half_size..2*even_size-1]. */

    if (odd) {
        /* We have the product U[0..even_size-1] * V[0..even_size-1] in
         * W[0..2*even_size-1].  We need to add the following to it:
         * V[size-1] * U[0..size-2]
         * U[size-1] * V[0..size-1] */
        w[even_size * 2] = dmul_add(u, even_size, v[even_size], &w[even_size]);
        w[even_size * 2 + 1] = dmul_add(v, size, u[even_size], &w[even_size]);
    }
}

void mul(const uint64_t *u,
         uint32_t usize,
         const uint64_t *v,
         uint32_t vsize,
         uint64_t *w)
{
    {
        const uint32_t ul = rsize(u, usize);
        const uint32_t vl = rsize(v, vsize);
        if (!ul || !vl) {
            zero(w, usize + vsize);
            return;
        }
        /* Zero digits which won't be set. */
        if (ul + vl != usize + vsize)
            zero(w + (ul + vl), (usize + vsize) - (ul + vl));

        /* Wanted: USIZE >= VSIZE. */
        if (ul < vl) {
            SWAP(u, v);
            usize = vl;
            vsize = ul;
        } else {
            usize = ul;
            vsize = vl;
        }
    }

    if (vsize < KARATSUBA_MUL_THRESHOLD) {
        _mul_base(u, usize, v, vsize, w);
        return;
    }

    mul_n(u, v, vsize, w);
    if (usize == vsize)
        return;

    uint32_t wsize = usize + vsize;
    zero(w + (vsize * 2), wsize - (vsize * 2));
    w += vsize;
    u += vsize;
    usize -= vsize;

    uint64_t *tmp = NULL;
    if (usize >= vsize) {
        tmp = APM_TMP_ALLOC(vsize * 2);
        do {
            mul_n(u, v, vsize, tmp);
            w += vsize;
            u += vsize;
            usize -= vsize;
        } while (usize >= vsize);
    }

    if (usize) { /* Size of U isn't a multiple of size of V. */
        if (!tmp)
            tmp = APM_TMP_ALLOC(usize + vsize);
        /* Now usize < vsize. Rearrange operands. */
        if (usize < KARATSUBA_MUL_THRESHOLD)
            _mul_base(v, vsize, u, usize, tmp);
        else
            mul(v, vsize, u, usize, tmp);
    }
    APM_TMP_FREE(tmp);
}

extern void _mul_base(const uint64_t *u,
                      uint32_t usize,
                      const uint64_t *v,
                      uint32_t vsize,
                      uint64_t *w);

/* Square diagonal. */
static void sqr_diag(const uint64_t *u, uint32_t size, uint64_t *v)
{
    if (!size)
        return;
    /* No compiler seems to recognize that if ((A+B) mod 2^N) < A (or B) iff
     * (A+B) >= 2^N and it can use the carry flag after the adds rather than
     * doing comparisons to see if overflow has ocurred. Instead they generate
     * code to perform comparisons, retaining values in already scarce
     * registers after they should be "dead." At any rate this isn't the
     * time-critical part of squaring so it's nothing to lose sleep over. */
    uint64_t p0, p1;
    digit_sqr(*u, p1, p0);
    p1 += (v[0] += p0) < p0;
    uint64_t cy = (v[1] += p1) < p1;
    while (--size) {
        u += 1;
        v += 2;
        digit_sqr(*u, p1, p0);
        p1 += (p0 += cy) < cy;
        p1 += (v[0] += p0) < p0;
        cy = (v[1] += p1) < p1;
    }
}

#ifndef BASE_SQR_THRESHOLD
#define BASE_SQR_THRESHOLD 10
#endif /* !BASE_SQR_THRESHOLD */

static void sqr_base(const uint64_t *u, uint32_t usize, uint64_t *v)
{
    if (!usize)
        return;

    /* Find size, and zero any digits which will not be set. */
    uint32_t ul = rsize(u, usize);
    if (ul != usize) {
        zero(v + (ul * 2), (usize - ul) * 2);
        if (ul == 0)
            return;
        usize = ul;
    }

    /* Single-precision case. */
    if (usize == 1) {
        /* FIXME can't do this: digit_sqr(u[0], v[1], v[0]) */
        uint64_t v0, v1;
        digit_sqr(*u, v1, v0);
        v[1] = v1;
        v[0] = v0;
        return;
    }

    /* It is better to use the multiply routine if the number is small. */
    if (usize <= BASE_SQR_THRESHOLD) {
        _mul_base(u, usize, u, usize, v);
        return;
    }

    /* Calculate products u[i] * u[j] for i != j.
     * Most of the savings vs long multiplication come here, since we only
     * perform (N-1) + (N-2) + ... + 1 = (N^2-N)/2 multiplications, vs a full
     * N^2 in long multiplication. */
    v[0] = 0;
    const uint64_t *ui = u;
    uint64_t *vp = &v[1];
    ul = usize - 1;
    vp[ul] = dmul(&ui[1], ul, ui[0], vp);
    for (vp += 2; ++ui, --ul; vp += 2)
        vp[ul] = dmul_add(&ui[1], ul, ui[0], vp);

    /* Double cross-products. */
    ul = usize * 2 - 1;
    v[ul] = lshifti(v + 1, ul - 1, 1);

    /* Add "main diagonal:"
     * for i=0 .. n-1
     *     v += u[i]^2 * B^2i */
    sqr_diag(u, usize, v);
}

/* Karatsuba squaring recursively applies the formula:
 *		U = U1*2^N + U0
 *		U^2 = (2^2N + 2^N)U1^2 - (U1-U0)^2 + (2^N + 1)U0^2
 * From my own testing this uses ~20% less time compared with slightly easier to
 * code formula:
 *		U^2 = (2^2N)U1^2 + (2^(N+1))(U1*U0) + U0^2
 */
void sqr(const uint64_t *u, uint32_t size, uint64_t *v)
{
    uint32_t tmp_rsize = rsize(u, size);
    if (tmp_rsize != size) {
        zero(v + tmp_rsize * 2, (size - tmp_rsize) * 2);
        size = tmp_rsize;
    }

    if (size < KARATSUBA_SQR_THRESHOLD) {
        if (!size)
            return;
        if (size <= BASE_SQR_THRESHOLD)
            _mul_base(u, size, u, size, v);
        else
            sqr_base(u, size, v);
        return;
    }

    const bool odd_size = size & 1;
    const uint32_t even_size = size & ~1;
    const uint32_t half_size = even_size / 2;
    const uint64_t *u0 = u, *u1 = u + half_size;
    uint64_t *v0 = v, *v1 = v + even_size;

    /* Choose the appropriate squaring function. */
    void (*sqr_fn)(const uint64_t *, uint32_t, uint64_t *) =
        (half_size >= KARATSUBA_SQR_THRESHOLD) ? sqr : sqr_base;
    /* Compute the low and high squares, potentially recursively. */
    sqr_fn(u0, half_size, v0); /* U0^2 => V0 */
    sqr_fn(u1, half_size, v1); /* U1^2 => V1 */

    uint64_t *tmp = APM_TMP_ALLOC(even_size * 2);
    uint64_t *tmp2 = tmp + even_size;
    /* tmp = w[0..even_size-1] */
    copy(v0, even_size, tmp);
    /* v += U1^2 * 2^N */
    addi_n(v + half_size, v1, even_size);
    /* v += U0^2 * 2^N */
    addi_n(v + half_size, tmp, even_size);

    int cmp_v = cmp_n(u1, u0, half_size);
    if (cmp_v) {
        if (cmp_v < 0)
            sub_n(u0, u1, half_size, tmp);
        else
            sub_n(u1, u0, half_size, tmp);
        sqr_fn(tmp, half_size, tmp2);
        subi_n(v + half_size, tmp2, even_size);
    }
    APM_TMP_FREE(tmp);

    if (odd_size) {
        v[even_size * 2] = dmul_add(u, even_size, u[even_size], &v[even_size]);
        v[even_size * 2 + 1] = dmul_add(u, size, u[even_size], &v[even_size]);
    }
}

#endif /* APM_H */
