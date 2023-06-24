#ifndef MEM_H
#define MEM_H

#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>

// allocates memory for a bn struct
static inline void *mykmalloc(size_t size)
{
    void *p;
    if (!(p = kmalloc(size, GFP_KERNEL))) {
        printk(KERN_ERR "mykmalloc: kmalloc failed\n");
        return NULL;
    }
    return p;
}

// reallocates memory for a bn struct
static inline void *mykrealloc(void *ptr, size_t size)
{
    void *p;
    if (!(p = krealloc(ptr, size, GFP_KERNEL))) {
        printk(KERN_ERR "mykrealloc: krealloc failed\n");
        return NULL;
    }
    return p;
}

// frees memory for a bn struct
static inline void mykfree(void *ptr)
{
    kfree(ptr);
}

#define MALLOC(n) mykmalloc(n)
#define REALLOC(p, n) mykrealloc(p, n)
#define FREE(p) mykfree(p)

#endif /* MEM_H */
