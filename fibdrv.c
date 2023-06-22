#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 92

static dev_t fib_dev = 0;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);
static int major = 0, minor = 0;
static ktime_t kt;

static long long fib_sequence(long long k)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    long long *f = kmalloc((k + 1) * sizeof(long long), GFP_KERNEL);
    if (!f) {
        printk(KERN_ALERT "Failed to allocate memory\n");
        return -ENOMEM;
    }

    long long result = 0;

    f[0] = 0;
    f[1] = 1;

    for (int i = 2; i <= k; i++) {
        f[i] = f[i - 1] + f[i - 2];
    }
    result = f[k];

    kfree(f);
    return result;
}

static long long fib_fastd(long long n)
{
    unsigned int h = 0;
    for (unsigned int i = n; i; ++h, i >>= 1)
        ;

    long long a = 0;
    long long b = 1;
    for (int j = h - 1; j >= 0; --j) {
        long long c = a * ((b << 1) - a);
        long long d = a * a + b * b;
        a = c;
        b = d;

        if ((n >> j) & 1) {
            a = d;
            b = c + d;
        }
    }

    return a;
}

static long long fib_fastd_clz(long long n)
{
    unsigned int h = sizeof(long long) * 8 - __builtin_clzll(n);

    long long a = 0;
    long long b = 1;

    unsigned long long mask = 1ULL << (h - 1);
    for (; mask; mask >>= 1) {
        long long c = a * ((b << 1) - a);
        long long d = a * a + b * b;
        a = c;
        b = d;

        if (mask & n) {
            a = d;
            b = c + d;
        }
    }

    return a;
}

static long long fib_time_proxy(long long k, size_t size)
{
    long long result;
    kt = ktime_get();
    switch (size) {
    case 1:
        result = fib_sequence(k);
        break;
    case 2:
        result = fib_fastd(k);
        break;
    case 3:
        result = fib_fastd_clz(k);
        break;
    default:
        return -EINVAL;
    }
    kt = ktime_sub(ktime_get(), kt);
    return result;
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
    // size = 1, use fib_sequence
    // size = 2, use fib_fastd
    // size = 3, use fib_fastd_clz
    return (ssize_t) fib_time_proxy(*offset, size);
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
