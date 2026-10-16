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
#define SYS_OPEN    6   // open(const char *path, int flags) -> file descriptor
#define SYS_CLOSE   7   // close(int fd) -> 0
#define SYS_SPAWN   8   // spawn(const char *path, char *const argv[], const spawn_options_t *options) -> pid
#define SYS_WAIT    9   // wait(int pid, int *code) -> 0, or WAIT_KILLED if it was killed

// spawn: `path` is looked up in /bin unless it contains a '/'; argv is a
// NULL-terminated array whose first entry becomes the child's argv[0]
// (NULL argv means just { path }). The child gets three of the caller's
// descriptors as its 0, 1 and 2 and no others; NULL options pass on 0, 1
// and 2 and start a new process group. -EBADF for a descriptor that is
// not open, -EPERM for a group that is neither the caller's nor one of
// its children's.
// wait: blocks until the child `pid` ends and stores its exit code in
// *code (if code is not NULL). Every child can be waited for once. While
// the caller waits, Ctrl+C goes to the child.
#define WAIT_KILLED 1   // ended by a fault or Ctrl+C rather than exit()

typedef struct {
    int fd[3];  // the caller's descriptors that become the child's 0, 1 and 2 (-1: not open)
    int pgid;   // process group to join, 0 for a new one; Ctrl+C ends a whole group
} spawn_options_t;

#define SYS_READDIR 10  // readdir(const char *dir, size_t index, dirent_t *entry) -> 1, or 0 past the end

#define SYS_BRK     11  // brk(void *end) -> the new end of the heap
#define SYS_PIPE    12  // pipe(int fds[2]) -> 0; what is written to fds[1] can be read from fds[0]

#define SYS_MKDIR   13  // mkdir(const char *path) -> 0
#define SYS_UNLINK  14  // unlink(const char *path) -> 0 (files)
#define SYS_RMDIR   15  // rmdir(const char *path) -> 0 (empty directories)
#define SYS_STAT    16  // stat(const char *path, stat_t *st) -> 0
#define SYS_FSTAT   17  // fstat(int fd, stat_t *st) -> 0
#define SYS_DUP2    18  // dup2(int fd, int new_fd) -> new_fd, now the same open file as fd

// open flags. The ramdisk is read-only; /tmp is writable. O_CREAT makes a
// missing file, O_TRUNC empties it, O_APPEND writes at the end every time.
#define O_RDONLY  0
#define O_WRONLY  1
#define O_RDWR    2
#define O_ACCMODE 3
#define O_CREAT   0x40
#define O_TRUNC   0x200
#define O_APPEND  0x400

// stat / fstat
#define STAT_FILE    1
#define STAT_DIR     2
#define STAT_CONSOLE 3  // the keyboard and screen (isatty)
#define STAT_PIPE    4
typedef struct {
    unsigned long size;  // bytes in a file (0 otherwise)
    int type;            // STAT_*
    int writable;        // a file or directory that may be changed
} stat_t;

// Descriptors: 0, 1 and 2 are stdin, stdout and stderr, open and close
// work on any of them, and open and pipe take the lowest free ones. A pipe
// holds 4 KiB: read waits for data and returns 0 once every write end is
// closed; write waits for room and fails with -EPIPE once every read end
// is closed. All descriptors are closed when a process ends.

// brk: moves the end of the heap, which starts right after the program
// and may grow to USER_HEAP_MAX bytes (64 MiB). New memory is zeroed;
// brk(0) returns the current end. -ENOMEM if the end is out of range or
// memory runs out (the heap then stays as it was).

// readdir: fills in entry number `index` (0, 1, ...) of a directory
#define DIRENT_NAME_MAX 256
typedef struct {
    unsigned long size;          // file size in bytes (0 for directories)
    int is_dir;
    char name[DIRENT_NAME_MAX];  // the name inside the directory, not a full path
} dirent_t;

// Errors are returned as negative values
#define EPERM   1   // not allowed
#define ENOENT  2   // no such file or directory
#define EINTR   4   // interrupted (the process is being killed)
#define E2BIG   7   // argument list too long
#define ENOEXEC 8   // not a valid program (or out of memory loading it)
#define EBADF   9   // bad file descriptor
#define ECHILD 10   // no such child process (or already waited for)
#define ENOMEM 12   // out of memory
#define EEXIST 17   // already exists
#define EFAULT 14   // bad address
#define ENOTDIR 20  // not a directory
#define EISDIR 21   // is a directory
#define EINVAL 22   // invalid argument
#define EMFILE 24   // too many open files
#define EFBIG  27   // file too large
#define ENOSPC 28   // no space left (for writable files)
#define EROFS  30   // read-only (the ramdisk)
#define ENAMETOOLONG 36  // path too long
#define EPIPE  32   // the reading end of the pipe is closed
#define ENOSYS 38   // unknown system call
#define ENOTEMPTY 39  // directory not empty

#endif // __KERNEL_ABI_SYSCALL_H__
