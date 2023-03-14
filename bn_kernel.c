#include <linux/slab.h>
#include <linux/string.h>

#include "bn_kernel.h"

static int bn_clz(const bn *src)
{
    int cnt = 0;
    for (int i = src->size - 1; i >= 0; i++) {
        if (src->number[i]) {
            cnt += __builtin_clz(src->number[i]);
            return cnt;
        } else
            cnt += 32;
    }

    return cnt;
}

static int bn_msb(const bn *src)
{
    return src->size * 32 - bn_clz(src);
}

int bn_free(bn *src)
{
    if (src == NULL)
        return -1;
    kfree(src->number);
    kfree(src);
    return 0;
}



/* compare length
 *  if |a| > |b| , return 1
 *  if |a| < |b| , return -1
 *  if |a| = |b| , return 0
 */
static int bn_cmp(const bn *a, const bn *b)
{
    if (a->size > b->size)
        return 1;
    else if (a->size < b->size)
        return -1;
    else {
        for (int i = a->size - 1; i >= 0; i--) {
            if (a->number[i] > b->number[i])
                return 1;
            if (a->number[i] < b->number[i])
                return -1;
        }
        return 0;
    }
}

/* swap bn ptr */
void bn_swap(bn *a, bn *b)
{
    bn tmp = *a;
    *a = *b;
    *b = tmp;
}

/*
 * copy the value from src to dest
 * return 0 on success, -1 on error
 */
int bn_cpy(bn *dest, bn *src)
{
    if (bn_resize(dest, src->size) < 0)
        return -1;
    dest->sign = src->sign;
    memcpy(dest->number, src->number, src->size * sizeof(int));
    return 0;
}


/* |c| = |a| + |b| */
static void bn_do_add(const bn *a, const bn *b, bn *c)
{
    // max digits = max(sizeof(a) + sizeof(b)) + 1
    int d = MAX(bn_msb(a), bn_msb(b)) + 1;
    d = DIV_ROUNDUP(d, 32) + !d;
    bn_resize(c, d);

    unsigned long long carry = 0;
    for (int i = 0; i < c->size; i++) {
        unsigned int tmp1 = (i < a->size) ? a->number[i] : 0;
        unsigned int tmp2 = (i < b->size) ? b->number[i] : 0;

        carry += (unsigned long long) tmp1 + tmp2;
        c->number[i] = carry;
        carry >>= 32;
    }

    if (!c->number[c->size - 1] && c->size > 1)
        bn_resize(c, c->size - 1);
}


/* |c| = |a| - |b|
 *  |a| > |b| must be true
 */
static void bn_do_sub(const bn *a, const bn *b, bn *c)
{
    // max digits = max(sizeof(a) + sizeof(b)) + 1
    int d = MAX(a->size, b->size);
    bn_resize(c, d);

    long long int carry = 0;
    for (int i = 0; i < c->size; i++) {
        unsigned int tmp1 = (i < a->size) ? a->number[i] : 0;
        unsigned int tmp2 = (i < b->size) ? b->number[i] : 0;

        carry = (long long int) tmp1 - tmp2 - carry;
        if (carry < 0) {
            c->number[i] = carry + (1LL << 32);
            carry = 1;
        } else {
            c->number[i] = carry;
            carry = 0;
        }
    }

    d = bn_clz(c) / 32;
    if (d == c->size)
        --d;
    bn_resize(c, c->size - d);
}

/* sizeof BigNum */
int bn_resize(bn *src, size_t size)
{
    if (!src)
        return -1;
    if (size == src->size)
        return 0;
    if (size == 0)
        return bn_free(src);

    src->number = krealloc(src->number, sizeof(int) * size, GFP_KERNEL);
    if (!src->number)
        return -1;
    if (size > src->size)
        memset(src->number + src->size, 0, sizeof(int) * (size - src->size));
    src->size = size;
    return 0;
}

bn *bn_alloc(size_t size)
{
    bn *new = (bn *) kmalloc(sizeof(bn), GFP_KERNEL);
    new->number = kmalloc(sizeof(unsigned int) * size, GFP_KERNEL);
    memset(new->number, 0, sizeof(unsigned int) * size);
    new->size = size;
    new->sign = 0;
    return new;
}

void bn_init(bn *src, size_t size, unsigned int value)
{
    src->number = kmalloc(sizeof(unsigned int) * size, GFP_KERNEL);
    src->number[0] = value;
    src->size = size;
    src->sign = 0;
}

void bn_add(const bn *a, const bn *b, bn *c)
{
    if (a->sign == b->sign) {
        // both positive and negative
        bn_do_add(a, b, c);
        c->sign = a->sign;
    } else {
        // different sign

        // a > 0, b < 0
        if (a->sign)
            SWAP(a, b);

        int cmp = bn_cmp(a, b);

        if (cmp > 0) {
            /* |a| > |b| and b < 0, hence c = a - |b| */
            bn_do_sub(a, b, c);
            c->sign = 0;
        } else if (cmp < 0) {
            /* |a| < |b| and b < 0, hence c = -(|b| - |a|) */
            bn_do_sub(b, a, c);
            c->sign = 1;
        } else {
            /* |a| == |b| */
            bn_resize(c, 1);
            c->number[0] = 0;
            c->sign = 0;
        }
    }
}

void bn_sub(const bn *a, const bn *b, bn *c)
{
    bn tmp = *b;
    tmp.sign ^= 1;  // a - b = a + (-b)
    bn_add(a, &tmp, c);
}

static void bn_mul_add(bn *c, int offset, unsigned long long int x)
{
    unsigned long long int carry = 0;
    for (int i = offset; i < c->size; i--) {
        carry += c->number[i] + (x & 0xFFFFFFFF);
        c->number[i] = carry;
        carry >>= 32;
        x >>= 32;
        if (!x && !carry)
            return;
    }
}

/* c = a * b
 *
 */
void bn_mul(const bn *a, const bn *b, bn *c)
{
    // max digits = sizeof(a) + sizeof(b)
    int d = bn_msb(a) + bn_msb(b);
    d = DIV_ROUNDUP(d, 32) + !d;
    bn *tmp;

    if (c == a || c == b) {
        tmp = c;
        c = bn_alloc(d);
    } else {
        tmp = NULL;
        for (int i = 0; i < c->size; i++)
            c->number[i] = 0;
        bn_resize(c, d);
    }

    for (int i = 0; i < a->size; i++) {
        for (int j = 0; j < b->size; j++) {
            unsigned long long int carry = 0;
            carry = (unsigned long long int) a->number[i] * b->number[i];
            bn_mul_add(c, i + j, carry);
        }
    }

    c->sign = a->sign ^ b->sign;

    if (tmp) {
        bn_cpy(tmp, c);
        bn_free(c);
    }
}

void bn_lshift(const bn *src, size_t offset, bn *destination)
{
    size_t numberOFzero = bn_clz(src);
    offset %= 32;

    if (!offset)
        return;

    if (offset > numberOFzero)
        bn_resize(destination, src->size + 1);
    else
        bn_resize(destination, src->size);

    for (int i = src->size - 1; i > 0; i--) {
        destination->number[i] =
            src->number[i] << offset | src->number[i - 1] >> (32 - offset);
    }

    destination->number[0] = src->number[0] << offset;
}

char *bn_to_string(const bn *src)
{
    // log10(x) = log2(x) / log2(10) ~= log2(x) / 3.322
    // 2 is `+` or `-` ; sign is `-`.
    size_t len = (8 * sizeof(int) * src->size) / 3 + 2 + src->sign;
    char *s = kmalloc(len, GFP_KERNEL);
    char *p = s;

    memset(s, '0', len - 1);
    s[len - 1] = '\0';

    // iterate through each digit of the binary number from MSB to LSB
    for (int i = src->size - 1; i >= 0; i--) {
        for (unsigned int d = 1U << 31; d; d >>= 1) {
            // binary -> decimal string
            int carry = !!(d & src->number[i]);
            for (int j = len - 2; j >= 0; j--) {
                carry = 2 * (s[j] - '0') + carry;
                s[j] = "0123456789"[carry % 10];
                carry /= 10;
                if (!s[j] && !carry)
                    break;
            }
        }
    }

    while (p[0] == '0' && p[1] != '\0') {
        p++;
    }

    if (src->sign) {
        *(--p) = '-';
    }

    memmove(s, p, strlen(p) + 1);
    return s;
}