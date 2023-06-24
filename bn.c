#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "apm.h"
#include "bn.h"

#define BN_INIT_DIGITS ((8 + DIGIT_SIZE - 1) / DIGIT_SIZE)

static void bn_min_alloc(bn *n, uint32_t s)
{
    if (n->alloc < s) {
        n->alloc = ((s + 3) & ~3U);
        n->digits = resize(n->digits, n->alloc);
    }
}

static void bn_size(bn *n, uint32_t size)
{
    n->size = size;
    if (n->alloc < n->size) {
        n->alloc = ((n->size + 3) & ~3U);
        n->digits = resize(n->digits, n->alloc);
    }
}

void bn_init(bn *n)
{
    n->alloc = BN_INIT_DIGITS;
    n->digits = new0(BN_INIT_DIGITS);
    n->size = 0;
    n->sign = 0;
}

void bn_init_u32(bn *n, uint32_t ui)
{
    bn_init(n);
    bn_set_u32(n, ui);
}

void bn_free(bn *n)
{
    FREE(n->digits);
}

static void bn_set(bn *p, const bn *q)
{
    if (p == q)
        return;

    if (q->size == 0) {
        bn_zero(p);
    } else {
        bn_size(p, q->size);
        copy(q->digits, q->size, p->digits);
        p->sign = q->sign;
    }
}

void bn_zero(bn *n)
{
    n->sign = 0;
    n->size = 0;
}

void bn_set_u32(bn *n, uint32_t m)
{
    n->sign = 0;
    if (m == 0) {
        n->size = 0;
        return;
    }

    bn_size(n, 1);
    n->digits[0] = (uint64_t) m;
}

void bn_swap(bn *a, bn *b)
{
    bn tmp = *a;
    *a = *b;
    *b = tmp;
}

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

void bn_add(const bn *a, const bn *b, bn *c)
{
    if (a->size == 0) {
        if (b->size == 0)
            c->size = 0;  // 0 + 0
        else
            bn_set(c, b);  // 0 + b
        return;
    } else if (b->size == 0) {
        bn_set(c, a);  // a + 0
        return;
    }

    // a + a
    if (a == b) {
        uint64_t cy;
        if (a == c) {
            // c = a + a
            // c = 2 * a, a == c
            // c <<= 1
            cy = lshifti(c->digits, c->size, 1);
        } else {
            // c = a + a
            // c = 2 * a
            // c = a << 1
            bn_size(c, a->size);
            cy = lshift(a->digits, a->size, 1, c->digits);
        }
        if (cy) {
            bn_min_alloc(c, c->size + 1);
            c->digits[c->size++] = cy;
        }
        return;
    }

    /* Note: it should work for A == C or B == C */
    uint32_t size;
    if (a->sign == b->sign) { /* Both positive or negative. */
        size = MAX(a->size, b->size);
        bn_min_alloc(c, size + 1);
        uint64_t cy = add(a->digits, a->size, b->digits, b->size, c->digits);
        if (cy)
            c->digits[size++] = cy;
        else
            APM_NORMALIZE(c->digits, size);
        c->sign = a->sign;
    } else { /* Differing signs. */
        if (a->sign)
            SWAP(a, b);

        int cmpv = cmp(a->digits, a->size, b->digits, b->size);
        if (cmpv > 0) { /* |A| > |B| */
            /* If B < 0 and |A| > |B|, then C = A - |B| */
            bn_min_alloc(c, a->size);
            c->sign = 0;
            size = rsize(c->digits, a->size);
        } else if (cmpv < 0) { /* |A| < |B| */
            /* If B < 0 and |A| < |B|, then C = -(|B| - |A|) */
            bn_min_alloc(c, b->size);
            c->sign = 1;
            size = rsize(c->digits, b->size);
        } else { /* |A| = |B| */
            c->sign = 0;
            size = 0;
        }
    }
    c->size = size;
}

void bn_mul(const bn *a, const bn *b, bn *c)
{
    if (a->size == 0 || b->size == 0) {
        bn_zero(c);
        return;
    }

    if (a == b) {
        bn_sqr(a, c);
        return;
    }

    uint32_t csize = a->size + b->size;
    if (a == c || b == c) {
        uint64_t *prod = APM_TMP_ALLOC(csize);
        mul(a->digits, a->size, b->digits, b->size, prod);
        csize -= (prod[csize - 1] == 0);
        bn_size(c, csize);
        copy(prod, csize, c->digits);
        APM_TMP_FREE(prod);
    } else {
        bn_min_alloc(c, csize);
        mul(a->digits, a->size, b->digits, b->size, c->digits);
        c->size = csize - (c->digits[csize - 1] == 0);
    }
    c->sign = a->sign ^ b->sign;
}

void bn_sqr(const bn *a, bn *b)
{
    if (a->size == 0) {
        bn_zero(b);
        return;
    }

    uint32_t bsize = a->size * 2;
    if (a == b) {
        uint64_t *prod = APM_TMP_ALLOC(bsize);
        sqr(a->digits, a->size, prod);
        bsize -= (prod[bsize - 1] == 0);
        bn_size(b, bsize);
        copy(prod, bsize, b->digits);
        APM_TMP_FREE(prod);
    } else {
        bn_min_alloc(b, bsize);
        sqr(a->digits, a->size, b->digits);
        b->size = bsize - (b->digits[bsize - 1] == 0);
    }
    b->sign = 0;
}

void bn_lshift(const bn *p, unsigned int bits, bn *q)
{
    if (bits == 0 || bn_is_zero(p)) {
        if (bits == 0)
            bn_set(q, p);
        else
            bn_zero(q);
        return;
    }

    const unsigned int digits = bits / DIGIT_BITS;
    bits %= DIGIT_BITS;

    uint64_t cy;
    if (p == q) {
        cy = lshifti(q->digits, q->size, bits);
        if (digits != 0) {
            bn_min_alloc(q, q->size + digits);
            for (int j = q->size - 1; j >= 0; j--)
                q->digits[j + digits] = q->digits[j];
            q->size += digits;
        }
    } else {
        bn_size(q, p->size + digits);
        cy = lshift(p->digits, p->size, bits, q->digits + digits);
    }

    zero(q->digits, digits);
    if (cy) {
        bn_size(q, q->size + 1);
        q->digits[q->size - 1] = cy;
    }
}

char *bn_to_dec_str(const bn *n)
{
    size_t str_size = (size_t) ((n->size) * DIGIT_BITS) / 3 + 2;

    if (n->size == 0) {
        char *str = kmalloc(2, GFP_KERNEL);
        str[0] = '0';
        str[1] = '\0';
        return str;
    } else {
        char *s = kmalloc(str_size, GFP_KERNEL);
        char *p = s;

        memset(s, '0', str_size - 1);
        s[str_size - 1] = '\0';

        /* n.digits[0] contains least significant bits */
        for (int i = n->size - 1; i >= 0; i--) {
            /* walk through every bit of bn */
            for (unsigned long long d = 1ULL << 63; d; d >>= 1) {
                /* binary -> decimal string */
                int carry = !!(d & n->digits[i]);
                for (int j = str_size - 2; j >= 0; j--) {
                    s[j] += s[j] - '0' + carry;
                    carry = (s[j] > '9');
                    if (carry)
                        s[j] -= 10;
                }
            }
        }
        // skip leading zero
        while (p[0] == '0' && p[1] != '\0') {
            p++;
        }
        if (n->sign)
            *(--p) = '-';
        memmove(s, p, strlen(p) + 1);
        return s;
    }
}
