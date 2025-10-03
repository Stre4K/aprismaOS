/*
 * ISR/IRQ stubs for x86 interrupt handling.
 *
 * - Interrupts 0–31 are CPU exceptions (faults, traps, errors).
 * - Interrupts 32–47 are hardware IRQs from the PIC, remapped to avoid
 *   conflict with CPU exceptions.
 *
 * Each stub:
 *   - Saves registers
 *   - Pushes error code (0 if not provided by CPU)
 *   - Pushes interrupt number
 *   - Calls isr_common_handler(int int_no, void* regs)
 *   - Restores state and returns with iret
 *
 * This file provides the low-level "entry points" the CPU jumps to
 * whenever an interrupt occurs.
 */

.section .text
.global isr_stub_table

/* --- Common pattern explanation (short)
 * For NOERR:
 *   cli
 *   pushl $0         ; fake error code so stack layout matches ERR-case
 *   pushl $n         ; int_no
 *   pusha            ; save registers (32 bytes)
 *   movl %esp, %eax  ; regs_ptr -> pointer to saved registers
 *   pushl %eax       ; 2nd arg: regs_ptr
 *   pushl $n         ; 1st arg: int_no
 *   call isr_common_handler
 *   addl $8, %esp    ; remove args pushed for the call
 *   popa             ; restore registers
 *   addl $8, %esp    ; remove fake error + int_no we pushed before pusha
 *   sti
 *   iret
 *
 * For ERR:
 *   (CPU already pushed the error_code)
 *   cli
 *   pushl $n         ; int_no
 *   pusha
 *   movl %esp, %eax
 *   pushl %eax
 *   pushl $n
 *   call isr_common_handler
 *   addl $8, %esp
 *   popa
 *   addl $4, %esp    ; remove the int_no we pushed earlier; leave CPU error_code in place for iret
 *   sti
 *   iret
 */

/* -------------------------
 * ISR macros
 * ------------------------- */

/* ISR entry point: save registers, call C handler, restore registers */
/* Common C handler entry */
.global isr_common_entry
.type isr_common_entry, @function
isr_common_entry:
    /* Stack layout on entry from minimal ISR:
       [ESP+4] -> regs_ptr
    */
    lea 0x4(%esp), %eax      /* eax = pointer to regs (saved registers) */
    push %eax             /* 1st arg: regs_ptr */
    call isr_common_handler
    add $4, %esp
    ret

/* ISR stub with no CPU error code, minimal */
#define ISR_NOERR(n)            \
.global isr##n;                 \
isr##n:                         \
    cli;                        \
    pushl $0;                   /* fake error code */ \
    pushl $n;                   /* int_no */ \
    pusha;                    /* save registers */ \
    call isr_common_entry;      \
    popa;                     /* restore registers */ \
    addl $8, %esp;              /* cleanup int_no + error code */ \
    sti;                      \
    iret;                     \

/* ISR stub with CPU-provided error code, minimal */
#define ISR_ERR(n)               \
.global isr##n;                 \
isr##n:                         \
    cli;                        \
    pushl $n;                   /* int_no */ \
    pusha;                    /* save registers */ \
    call isr_common_entry;      \
    popa;                     /* restore registers */ \
    addl $4, %esp;              /* cleanup int_no */ \
    sti;                      \
    iret;                     \

/* Create all exception stubs 0..31 */
ISR_NOERR(0)
ISR_NOERR(1)
ISR_NOERR(2)
ISR_NOERR(3)
ISR_NOERR(4)
ISR_NOERR(5)
ISR_NOERR(6)
ISR_NOERR(7)
ISR_ERR(8)
ISR_NOERR(9)
ISR_ERR(10)
ISR_ERR(11)
ISR_ERR(12)
ISR_ERR(13)
ISR_ERR(14)
ISR_NOERR(15)
ISR_NOERR(16)
ISR_ERR(17)
ISR_NOERR(18)
ISR_NOERR(19)
ISR_NOERR(20)
ISR_ERR(21)
ISR_NOERR(22)
ISR_NOERR(23)
ISR_NOERR(24)
ISR_NOERR(25)
ISR_NOERR(26)
ISR_NOERR(27)
ISR_NOERR(28)
ISR_NOERR(29)
ISR_ERR(30)
ISR_NOERR(31)

/* IRQs 32..47 */
ISR_NOERR(32)
ISR_NOERR(33)
ISR_NOERR(34)
ISR_NOERR(35)
ISR_NOERR(36)
ISR_NOERR(37)
ISR_NOERR(38)
ISR_NOERR(39)
ISR_NOERR(40)
ISR_NOERR(41)
ISR_NOERR(42)
ISR_NOERR(43)
ISR_NOERR(44)
ISR_NOERR(45)
ISR_NOERR(46)
ISR_NOERR(47)

/* Build table of ISR pointers */
isr_stub_table:
    .long isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7
    .long isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15
    .long isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
    .long isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    .long isr32, isr33, isr34, isr35, isr36, isr37, isr38, isr39
    .long isr40, isr41, isr42, isr43, isr44, isr45, isr46, isr47

/* IRQ aliases: make irq0..irq15 point to isr32..isr47 */
.global irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7
.global irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15

irq0  = isr32
irq1  = isr33
irq2  = isr34
irq3  = isr35
irq4  = isr36
irq5  = isr37
irq6  = isr38
irq7  = isr39
irq8  = isr40
irq9  = isr41
irq10 = isr42
irq11 = isr43
irq12 = isr44
irq13 = isr45
irq14 = isr46
irq15 = isr47

#undef ISR_NOERR
#undef ISR_ERR

