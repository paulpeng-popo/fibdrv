typedef struct {
    uint64_t *digits;  /* Digits of number. */
    uint32_t size;     /* Length of number. */
    uint32_t alloc;    /* Size of allocation. */
    unsigned sign : 1; /* Sign bit. */
} bn, bn_t[1];

#define BN_INITIALIZER                                        \
    {                                                         \
        {                                                     \
            .digits = NULL, .size = 0, .alloc = 0, .sign = 0, \
        }                                                     \
    }

void bn_init(bn *p);
void bn_init_u32(bn *p, uint32_t q);
void bn_free(bn *p);

void bn_set_u32(bn *p, uint32_t q);

#define bn_is_zero(n) ((n)->size == 0)
void bn_zero(bn *p);

void bn_swap(bn *a, bn *b);

void bn_lshift(const bn *p, unsigned int bits, bn *q);

/* S = A + B */
void bn_add(const bn *a, const bn *b, bn *s);

/* P = A * B */
void bn_mul(const bn *a, const bn *b, bn *p);

/* B = A * A */
void bn_sqr(const bn *a, bn *b);

char *bn_to_dec_str(const bn *n);
