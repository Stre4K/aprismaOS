// console.c
// <kernel/console.h>
//
// The console thread runs the keyboard driver's decoding and the line
// editor, so input is echoed and edited even while the shell waits for a
// program, and Ctrl+C works while a program is busy. Finished lines are
// queued with their '\n'; readers block on a wait queue until a whole line
// (or end of input) is available.

#include <kernel/console.h>

#ifdef CONFIG_SCHED

#include <kernel/abi/syscall.h>
#include <kernel/arch/irq.h>
#include <kernel/drivers/keyboard.h>
#include <kernel/panic.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <stdbool.h>
#ifdef CONFIG_USERMODE
#include <kernel/proc.h>
#endif

#define QUEUE_SIZE 1024

static char queue[QUEUE_SIZE];  // ring buffer of finished lines
static size_t queue_start;
static size_t queue_length;
static int lines_queued;        // complete lines in the queue
static int eofs_queued;         // pending Ctrl+D presses
static unsigned interrupts;     // Ctrl+C presses so far
static wait_queue_t readers;

#ifdef CONFIG_USERMODE
static int foreground;  // process group Ctrl+C ends, 0 for none
#endif

static void console_thread(void *arg) {
    (void)arg;
    for (;;) {
        keyboard_process();
        keyboard_wait_for_input();
    }
}

void console_start(void) {
    if (thread_create("console", console_thread, NULL) == NULL)
        panic("console_start: cannot create the console thread");
}

void console_line_entered(const char *line, size_t len) {
    unsigned long flags = irq_save();
    if (queue_length + len + 1 > QUEUE_SIZE) {
        irq_restore(flags);
        printk("console: input queue full, line dropped\n");
        return;
    }
    for (size_t i = 0; i <= len; i++) {
        queue[(queue_start + queue_length) % QUEUE_SIZE] = i < len ? line[i] : '\n';
        queue_length++;
    }
    lines_queued++;
    irq_restore(flags);

    wait_queue_wake_all(&readers);
}

void console_eof(void) {
    unsigned long flags = irq_save();
    eofs_queued++;
    irq_restore(flags);

    wait_queue_wake_all(&readers);
}

void console_interrupt(void) {
    unsigned long flags = irq_save();
    interrupts++;
#ifdef CONFIG_USERMODE
    if (foreground)
        process_interrupt_group(foreground);  // acted on when they next return to user mode
#endif
    irq_restore(flags);

    wait_queue_wake_all(&readers);  // a blocked read() must notice
}

#ifdef CONFIG_USERMODE
void console_set_foreground(int pgid) {
    foreground = pgid;
}

int console_get_foreground(void) {
    return foreground;
}
#endif

long console_read(char *buf, size_t len) {
    unsigned long flags = irq_save();
    unsigned interrupts_seen = interrupts;

    for (;;) {
#ifdef CONFIG_USERMODE
        process_t *self = process_current();
        if (self && self->kill_pending) {
            irq_restore(flags);
            return -EINTR;
        }
#endif
        if (lines_queued > 0) {
            size_t n = 0;
            bool end_of_line = false;
            while (n < len && !end_of_line) {
                char c = queue[queue_start];
                queue_start = (queue_start + 1) % QUEUE_SIZE;
                queue_length--;
                buf[n++] = c;
                end_of_line = c == '\n';
            }
            if (end_of_line)
                lines_queued--;
            irq_restore(flags);
            return (long)n;
        }

        if (eofs_queued > 0) {
            eofs_queued--;
            irq_restore(flags);
            return 0;
        }

        // Ctrl+C while waiting: give up (a shell then prints a new prompt)
        if (interrupts != interrupts_seen) {
            irq_restore(flags);
            return -EINTR;
        }

#ifdef CONFIG_USERMODE
        process_sleep_on(&readers);
#else
        wait_queue_sleep(&readers);
#endif
    }
}

#endif // CONFIG_SCHED
