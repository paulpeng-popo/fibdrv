#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>

#include "bn.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

#define MAX_LENGTH 1000000

static dev_t fib_dev = 0;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);
static int major = 0, minor = 0;
static ktime_t kt;

static uint64_t fib_sequence(uint64_t k)
{
    uint64_t f[2] = {0, 1};

    if (k < 2)
        return f[k];

    for (int i = 2; i <= k; i++) {
        // use bit operation to replace mod operation
        f[i & 1] = f[0] + f[1];
    }

    return f[k & 1];
}

static uint64_t fib_fastd(uint64_t n)
{
    if (n < 2)
        return n;

    uint64_t a = 0;
    uint64_t b = 1;

    for (int j = 63 - 1; j >= 0; j--) {
        uint64_t c = a * ((b << 1) - a);
        uint64_t d = a * a + b * b;
        a = c;
        b = d;

        if ((n >> j) & 1LL) {
            a = d;
            b = c + d;
        }
    }

    return a;
}

static uint64_t fib_fastd_clz(uint64_t n)
{
    if (n < 2)
        return n;

    uint64_t a = 0;
    uint64_t b = 1;

    uint64_t mask = 1LL << (63 - __builtin_clzll(n));
    for (; mask; mask >>= 1) {
        uint64_t c = a * ((b << 1) - a);
        uint64_t d = a * a + b * b;
        a = c;
        b = d;

        if (mask & n) {
            a = d;
            b = c + d;
        }
    }

    return a;
}

static void fib_bignum(uint64_t n, bn *fib)
{
    if (unlikely(n <= 2)) {
        if (n == 0)
            bn_zero(fib);
        else
            bn_set_u32(fib, 1);
        return;
    }

    bn *a1 = fib; /* Use output param fib as a1 */

    bn_t a0, tmp, a;
    bn_init_u32(a0, 0); /*  a0 = 0 */
    bn_set_u32(a1, 1);  /*  a1 = 1 */
    bn_init(tmp);       /* tmp = 0 */
    bn_init(a);

    /* Start at second-highest bit set. */
    for (uint64_t k = ((uint64_t) 1) << (62 - __builtin_clzll(n)); k; k >>= 1) {
        /* Both ways use two squares, two adds, one multipy and one shift. */
        bn_lshift(a0, 1, a); /* a03 = a0 * 2 */
        bn_add(a, a1, a);    /*   ... + a1 */
        bn_sqr(a1, tmp);     /* tmp = a1^2 */
        bn_sqr(a0, a0);      /* a0 = a0 * a0 */
        bn_add(a0, tmp, a0); /*  ... + a1 * a1 */
        bn_mul(a1, a, a1);   /*  a1 = a1 * a */
        if (k & n) {
            bn_swap(a1, a0);    /*  a1 <-> a0 */
            bn_add(a0, a1, a1); /*  a1 += a0 */
        }
    }
    /* Now a1 (alias of output parameter fib) = F[n] */

    bn_free(a0);
    bn_free(tmp);
    bn_free(a);
}

static void fib_time_proxy(uint64_t k, bn *result)
{
    kt = ktime_get();
    fib_bignum(k, result);
    kt = ktime_sub(ktime_get(), kt);
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use\n");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    bn_t fib = BN_INITIALIZER;
    fib_time_proxy(*offset, fib);
    uint32_t len = fib->size;
    // char *str_num = bn_to_dec_str(fib);
    // pr_info("fibdrv: %lld %s\n", *offset, str_num);
    size_t num_of_bytes = sizeof(uint64_t) * len / sizeof(char);
    if (copy_to_user(buf, fib->digits, num_of_bytes)) {
        printk(KERN_ALERT "fibdrv: copy_to_user failed\n");
        return -EFAULT;
    }
    bn_free(fib);

    return (size_t) len;
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
    rc = major = register_chrdev(major, DEV_FIBONACCI_NAME, &fib_fops);
    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev\n");
        rc = -2;
        goto failed_cdev;
    }
    fib_dev = MKDEV(major, minor);

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class\n");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device\n");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
failed_cdev:
    unregister_chrdev(major, DEV_FIBONACCI_NAME);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    unregister_chrdev(major, DEV_FIBONACCI_NAME);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
