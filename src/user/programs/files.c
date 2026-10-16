// files: open/read/close on the ramdisk, including every error case
#include "ulib.h"

#define MAX_FILES 13  // free descriptors: the kernel's PROCESS_MAX_FDS (16) minus 0, 1 and 2

static int same(const char *a, const char *b, long n)
{
    for (long i = 0; i < n; i++) {
        if (a[i] != b[i])
            return 0;
    }
    return 1;
}

int main(void)
{
    int bad = 0;
    char whole[512], pieces[512], piece[7];

    // Whole file in one read, then end of file
    long fd = open("/etc/motd", O_RDONLY);
    if (fd < 3) {
        print("files: cannot open /etc/motd\n");
        return 1;
    }
    long size = read(fd, whole, sizeof(whole));
    bad += size <= 0;
    bad += read(fd, whole, 1) != 0;          // end of file
    bad += close(fd) != 0;
    bad += close(fd) != -EBADF;              // already closed

    // Small reads return the same bytes
    fd = open("/etc/motd", O_RDONLY);
    long total = 0, n;
    while ((n = read(fd, piece, sizeof(piece))) > 0 && total + n <= (long)sizeof(pieces)) {
        for (long i = 0; i < n; i++)
            pieces[total + i] = piece[i];
        total += n;
    }
    bad += total != size || !same(whole, pieces, size);
    close(fd);

    // Two descriptors keep their own position
    long a = open("/etc/motd", O_RDONLY), b = open("/etc/motd", O_RDONLY);
    char first_a[5], first_b[5];
    read(a, first_a, 5);
    read(b, first_b, 5);
    bad += a == b || !same(first_a, whole, 5) || !same(first_b, whole, 5);
    close(a);
    close(b);

    // Errors
    bad += open("/no/such/file", O_RDONLY) != -ENOENT;
    bad += open("/bin", O_RDONLY) != -EISDIR;
    bad += open((const char *)0xFFFFFFFF80100000UL, O_RDONLY) != -EFAULT;  // kernel memory
    bad += open((const char *)0, O_RDONLY) != -EFAULT;                     // NULL
    bad += read(42, piece, 1) != -EBADF;

    // Running out of descriptors
    long fds[MAX_FILES + 1];
    for (int i = 0; i < MAX_FILES; i++)
        fds[i] = open("/etc/motd", O_RDONLY);
    bad += open("/etc/motd", O_RDONLY) != -EMFILE;
    for (int i = 0; i < MAX_FILES; i++)
        bad += close((int)fds[i]) != 0;

    write(1, whole, (size_t)size);
    print(bad ? "files: FAILED\n" : "files: ok\n");
    return bad;
}
