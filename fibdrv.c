#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/types.h>
// kmalloc
#include <linux/slab.h>
// __copy_to_user
#include <linux/uaccess.h>
// ktime_t
#include <linux/ktime.h>

#include "bn_kernel.h"
#include "stringAdd.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 1000

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);
static ktime_t kt;

static long long bn_fib_fast_doubling_iterative_clz(long long k, char *buf)
{
    bn *f1 = bn_alloc(1);
    if (k <= 2) {  // Fib(0) = 0, Fib(1) = 1
        f1->number[0] = !!k;
        char *ret = bn_to_string(f1);
        size_t retSize = strlen(ret);
        __copy_to_user(buf, ret, retSize);
        bn_free(f1);
        return retSize;
    }

    bn *f2 = bn_alloc(1);
    f1->number[0] = 1;  // fib[k]
    f2->number[0] = 1;  // fib[k+1]

    bn *k1 = bn_alloc(1);
    bn *k2 = bn_alloc(1);

    uint8_t count = 63 - __builtin_clzll(k);

    for (uint64_t i = count; i-- > 0;) {
        // fib[2k] = fib[k] * (fib[k + 1] * 2 - fib[k]);
        bn_cpy(k1, f2);
        bn_lshift(k1, 1);
        bn_sub(k1, f1, k1);
        bn_mul(f1, k1, k1);
        // fib[2k] = fib[k] * fib[k] + fib[k+1] * fib[k+1]

        bn_mul(f1, f1, f1);
        bn_mul(f2, f2, f2);
        bn_add(f1, f2, k2);

        if (k & (1UL << i)) {
            bn_cpy(f1, k2);  // 2k
            bn_add(k1, k2, f2);
        } else {
            bn_cpy(f1, k1);
            bn_cpy(f2, k2);
        }
    }

    char *ret = bn_to_string(f1);
    size_t retSize = strlen(ret);
    __copy_to_user(buf, ret, retSize);

    bn_free(k1);
    bn_free(k2);
    bn_free(f2);
    bn_free(f1);

    return retSize;
}

static long long bn_fib_iterative(unsigned int n, char *buf)
{
    bn *dest = bn_alloc(1);
    if (n <= 2) {  // Fib(0) = 0, Fib(1) = 1
        dest->number[0] = !!n;
        return 1;
    }

    bn *a = bn_alloc(1);
    bn *b = bn_alloc(1);
    dest->number[0] = 1;

    for (unsigned int i = 1; i < n; i++) {
        bn_cpy(b, dest);        // b = dest
        bn_add(dest, a, dest);  // dest += a
        bn_swap(a, b);          // SWAP(a, b)
    }
    bn_free(a);
    bn_free(b);
    char *ret = bn_to_string(dest);
    bn_free(dest);

    size_t retSize = strlen(ret);
    __copy_to_user(buf, ret, retSize);
    return retSize;
}

static bn bn_fib_helper(long long k, bn *fib, bn *c)
{
    if (k <= 2) {
        long long tmp = k;
        bn_init(&fib[k], 1, !!tmp);
        return fib[k];
    }

    bn a = bn_fib_helper((k >> 1), fib, c);
    bn b = bn_fib_helper((k >> 1) + 1, fib, c);

    bn_init(&fib[k], 1, 0);
    bn_init(&c[0], 1, 0);
    bn_init(&c[1], 1, 0);

    if (k & 1) {
        bn_mul(&a, &a, &c[0]);          // c0 = a * a
        bn_mul(&b, &b, &c[1]);          // c1 = b * b
        bn_add(&c[0], &c[1], &fib[k]);  // fib[k] = a * a + b * b
    } else {
        bn_cpy(&c[0], &b);
        bn_lshift(&c[0], 1);         // c0 = 2 * b
        bn_sub(&c[0], &a, &c[1]);    // c1 = 2 * b - a
        bn_mul(&a, &c[1], &fib[k]);  // fib[k] = a * (2 * b - a)
    }

    return fib[k];
}

static long long bn_fib_fast_doubling_recursive(long long k, char *buf)
{
    bn *fib = (bn *) kmalloc((k + 2) * sizeof(bn), GFP_KERNEL);
    bn *c = (bn *) kmalloc(2 * sizeof(bn), GFP_KERNEL);
    bn_fib_helper(k, fib, c);
    char *ret = bn_to_string(&fib[k]);
    size_t retSize = strlen(ret);
    __copy_to_user(buf, ret, retSize);
    return retSize;
}

static long long fib_sequence_fast_doubling_iterative(long long k)
{
    if (k <= 2)
        return !!k;

    uint8_t count = 63 - __builtin_clzll(k);
    uint64_t fib_n0 = 1, fib_n1 = 1;

    for (uint64_t i = count, fib_2n0, fib_2n1; i-- > 0;) {
        fib_2n0 = fib_n0 * ((fib_n1 << 1) - fib_n0);
        fib_2n1 = fib_n0 * fib_n0 + fib_n1 * fib_n1;

        if (k & (1UL << i)) {
            fib_n0 = fib_2n1;            // 2k
            fib_n1 = fib_2n0 + fib_2n1;  // 2K + 1
        } else {
            fib_n0 = fib_2n0;
            fib_n1 = fib_2n1;
        }
    }

    return fib_n0;
}

static long long fib_sequence_fast_doubling_recursive(long long k)
{
    if (k <= 2)
        return !!k;

    // fib(2n) = fib(n) * (2 * fib(n+1) − fib(n))
    // fib(2n+1) = fib(n) * fib(n) + fib(n+1) * fib(n+1)
    long long a = fib_sequence_fast_doubling_recursive(k >> 1);
    long long b = fib_sequence_fast_doubling_recursive((k >> 1) + 1);

    if (k & 1)
        return a * a + b * b;
    return a * ((b << 1) - a);
}

static long long fib_sequence_string_add(long long k, char *buf)
{
    // GFP_KERNEL is a flag used for memory allocation in the Linux kernel.
    str_t *f = kmalloc((k + 2) * sizeof(str_t), GFP_KERNEL);
    strncpy(f[0].numberStr, "0", 1);
    f[0].numberStr[1] = '\0';

    strncpy(f[1].numberStr, "1", 1);
    f[1].numberStr[1] = '\0';

    for (int i = 2; i <= k; i++) {
        add_str(f[i - 1].numberStr, f[i - 2].numberStr, f[i].numberStr);
    }
    size_t retSize = strlen(f[k].numberStr);
    reverse_str(f[k].numberStr, retSize);
    __copy_to_user(buf, f[k].numberStr, retSize);
    return retSize;
}

static long long fib_sequence_basic(long long k)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    long long *f = kmalloc((k + 2) * sizeof(long long), GFP_KERNEL);

    f[0] = 0;
    f[1] = 1;

    for (int i = 2; i <= k; i++) {
        f[i] = f[i - 1] + f[i - 2];
    }

    return f[k];
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

static long long fib_time_proxy(long long k, char *buf, int mode)
{
    long long result = 0;
    switch (mode) {
    case 0:
        kt = ktime_get();
        result = fib_sequence_basic(k);
        kt = ktime_sub(ktime_get(), kt);
        break;
    case 1:
        kt = ktime_get();
        result = fib_sequence_string_add(k, buf);
        kt = ktime_sub(ktime_get(), kt);
        break;
    case 2:
        kt = ktime_get();
        result = fib_sequence_fast_doubling_recursive(k);
        kt = ktime_sub(ktime_get(), kt);
        break;
    case 3:
        kt = ktime_get();
        result = fib_sequence_fast_doubling_iterative(k);
        kt = ktime_sub(ktime_get(), kt);
        break;
    case 4:
        kt = ktime_get();
        result = bn_fib_fast_doubling_recursive(k, buf);
        kt = ktime_sub(ktime_get(), kt);
        break;
    case 5:
        kt = ktime_get();
        result = bn_fib_iterative(k, buf);
        kt = ktime_sub(ktime_get(), kt);
        break;
    case 6:
        kt = ktime_get();
        bn_fib_fast_doubling_iterative_clz(k, buf);
        kt = ktime_sub(ktime_get(), kt);
    default:
        break;
    }

    return result;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    return (ssize_t) fib_time_proxy(*offset, buf, 4);
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    return ktime_to_ns(kt);
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
