#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
// kmalloc
#include <linux/slab.h>
// __copy_to_user
#include <asm/uaccess.h>

#include "stringAdd.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 500

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);


static long long fast_doubling_recursive(long long k, long long *f)
{
    if (k <= 2)
        return (k > 0) ? (f[k] = 1) : (f[k] = 0);

    int x = k / 2;
    long long a = fast_doubling_recursive(x + 1, f);
    long long b = fast_doubling_recursive(x, f);
    f[k] = (k % 2) ? a * a + b * b : b * (2 * a - b);
    return f[k];
}

static long long fib_sequence_fast_doubling(long long k)
{
    long long *f = kmalloc((k + 2) * sizeof(long long), GFP_KERNEL);
    memset(f, 0, (k + 2) * sizeof(long long));
    fast_doubling_recursive(k, f);

    return f[k];
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

static long long fib_read_proxy(long long k, char *buf, int mode)
{
    long long result = 0;
    switch (mode) {
    case 0:
        result = fib_sequence_basic(k);
        break;
    case 1:
        result = fib_sequence_string_add(k, buf);
        break;
    case 2:
        result = fib_sequence_fast_doubling(k);
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
    return (ssize_t) fib_read_proxy(*offset, buf, 2);
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    return 1;
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
