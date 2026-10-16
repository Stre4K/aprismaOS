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
#define SYS_READ    5   // read(int fd, void *buf, size_t len) -> bytes read, 0 at end of input/file
#define SYS_OPEN    6   // open(const char *path) -> file descriptor (files are read-only)
#define SYS_CLOSE   7   // close(int fd) -> 0
#define SYS_SPAWN   8   // spawn(const char *path, char *const argv[]) -> pid of the new child
#define SYS_WAIT    9   // wait(int pid, int *code) -> 0, or WAIT_KILLED if it was killed

// spawn: `path` is looked up in /bin unless it contains a '/'; argv is a
// NULL-terminated array whose first entry becomes the child's argv[0]
// (NULL argv means just { path }).
// wait: blocks until the child `pid` ends and stores its exit code in
// *code (if code is not NULL). Every child can be waited for once. While
// the caller waits, Ctrl+C goes to the child.
#define WAIT_KILLED 1   // ended by a fault or Ctrl+C rather than exit()

// Errors are returned as negative values
#define ENOENT  2   // no such file or directory
#define EINTR   4   // interrupted (the process is being killed)
#define E2BIG   7   // argument list too long
#define ENOEXEC 8   // not a valid program (or out of memory loading it)
#define EBADF   9   // bad file descriptor
#define ECHILD 10   // no such child process (or already waited for)
#define EFAULT 14   // bad address
#define EISDIR 21   // is a directory
#define EINVAL 22   // invalid argument
#define EMFILE 24   // too many open files
#define ENAMETOOLONG 36  // path too long
#define ENOSYS 38   // unknown system call

#endif // __KERNEL_ABI_SYSCALL_H__
