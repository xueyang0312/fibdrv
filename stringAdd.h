#include <linux/string.h>
#define XOR_SWAP(a, b, type) \
    do {                     \
        type *__c = (a);     \
        type *__d = (b);     \
        *__c ^= *__d;        \
        *__d ^= *__c;        \
        *__c ^= *__d;        \
    } while (0)

typedef struct str {
    char numberStr[128];
} str_t;

static void __swap(void *a, void *b, size_t size)
{
    if (a == b)
        return;

    switch (size) {
    case 1:
        XOR_SWAP(a, b, char);
        break;
    case 2:
        XOR_SWAP(a, b, short);
        break;
    case 4:
        XOR_SWAP(a, b, unsigned int);
        break;
    case 8:
        XOR_SWAP(a, b, unsigned long);
        break;

    default:
        break;
    }
}

static void add_str(char *x, char *y, char *result)
{
    size_t size_x = strlen(x), size_y = strlen(y);
    int i, sum, carry = 0;
    if (size_x > size_y) {
        for (i = 0; i < size_y; i++) {
            sum = (x[i] - '0') + (y[i] - '0') + carry;
            result[i] = '0' + sum % 10;
            carry = sum / 10;
        }

        for (i = size_y; i < size_x; i++) {
            sum = (x[i] - '0') + carry;
            result[i] = '0' + sum % 10;
            carry = sum / 10;
        }
    } else {
        for (i = 0; i < size_x; i++) {
            sum = (x[i] - '0') + (y[i] - '0') + carry;
            result[i] = '0' + sum % 10;
            carry = sum / 10;
        }

        for (i = size_x; i < size_y; i++) {
            sum = (y[i] - '0') + carry;
            result[i] = '0' + sum % 10;
            carry = sum / 10;
        }
    }

    if (carry)
        result[i++] = '0' + carry;
    result[i] = '\0';
}

static void reverse_str(char *str, size_t n)
{
    for (int i = 0; i < (n >> 1); i++) {
        __swap(&str[i], &str[n - i - 1], sizeof(char));
    }
}