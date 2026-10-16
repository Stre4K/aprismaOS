// abi/syscall.h
// System call numbers and error codes, shared by the kernel and user
// programs (src/user includes this file too). Changing a number breaks
// programs built against the old one.

#ifndef __KERNEL_ABI_SYSCALL_H__
#define __KERNEL_ABI_SYSCALL_H__

#define SYS_EXIT    0   // exit(int code)                         never returns
#define SYS_WRITE   1   // write(int fd, const void *buf, size_t len) -> bytes written
#define SYS_GETPID  2   // getpid() -> process id
#define SYS_SLEEP   3   // sleep(unsigned ms) -> 0
#define SYS_YIELD   4   // yield() -> 0
#define SYS_READ    5   // read(int fd, void *buf, size_t len) -> bytes read, 0 at end of input

// Errors are returned as negative values
#define EINTR   4   // interrupted (the process is being killed)
#define EBADF   9   // bad file descriptor
#define EFAULT 14   // bad address
#define EINVAL 22   // invalid argument
#define ENOSYS 38   // unknown system call

#endif // __KERNEL_ABI_SYSCALL_H__
