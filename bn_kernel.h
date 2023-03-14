#ifndef BN_KERNEL_H
#define BN_KERNEL_H

#include <linux/slab.h>
#include <linux/string.h>

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#ifndef DIV_ROUNDUP
#define DIV_ROUNDUP(x, len) (((x) + (len) -1) / (len))
#endif

#ifndef SWAP
#define SWAP(x, y)           \
    do {                     \
        typeof(x) __tmp = x; \
        x = y;               \
        y = __tmp;           \
    } while (0)
#endif

typedef struct _bn {
    unsigned int *number;
    unsigned int size;
    int sign;
} bn;

bn *bn_alloc(size_t size);
int bn_free(bn *src);
void bn_init(bn *src, size_t size, unsigned int value);
int bn_resize(bn *src, size_t size);
int bn_cpy(bn *dest, bn *src);
void bn_swap(bn *a, bn *b);
void bn_add(const bn *a, const bn *b, bn *c);
void bn_sub(const bn *a, const bn *b, bn *c);
void bn_mul(const bn *a, const bn *b, bn *c);
void bn_lshift(const bn *src, size_t offset, bn *destination);
char *bn_to_string(const bn *src);
#endif