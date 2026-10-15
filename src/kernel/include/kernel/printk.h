#ifndef _KERNEL_PRINTK_H
#define _KERNEL_PRINTK_H

// Print to the kernel console. Uses the standard printf format syntax, and
// the compiler checks every call's arguments against its format string.
void printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif /* _KERNEL_PRINTK_H */
