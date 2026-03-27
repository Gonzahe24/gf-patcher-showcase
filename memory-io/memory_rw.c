/*
 * memory_rw.c - Read/write process memory via /proc/PID/mem
 *
 * Linux exposes each process's virtual address space as a file at
 * /proc/PID/mem. With ptrace attached, we can pread/pwrite to any
 * mapped address. This is faster and simpler than using PTRACE_PEEKDATA
 * (which only reads one word at a time).
 *
 * Portfolio showcase - simplified from production code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

#include "memory_rw.h"

/*
 * gf_read_memory - Read bytes from a target process's memory.
 *
 * Opens /proc/PID/mem, seeks to the target address, and reads.
 * The process MUST be ptrace-attached (stopped) before calling this.
 *
 * @pid:    Target process ID
 * @addr:   Virtual address in target's address space
 * @buf:    Buffer to read into
 * @len:    Number of bytes to read
 *
 * Returns number of bytes read, or -1 on error.
 */
ssize_t gf_read_memory(pid_t pid, unsigned long addr, void *buf, size_t len)
{
    char path[64];
    int fd;
    ssize_t nread;

    snprintf(path, sizeof(path), "/proc/%d/mem", pid);

    fd = open(path, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "[patcher] Cannot open %s: %s\n", path, strerror(errno));
        return -1;
    }

    /* pread reads at an offset without changing the file position.
     * The offset is the virtual address in the target process. */
    nread = pread(fd, buf, len, (off_t)addr);
    if (nread == -1) {
        fprintf(stderr, "[patcher] pread failed at 0x%lx in PID %d: %s\n",
                addr, pid, strerror(errno));
    } else if ((size_t)nread != len) {
        fprintf(stderr, "[patcher] Short read at 0x%lx: got %zd, wanted %zu\n",
                addr, nread, len);
    }

    close(fd);
    return nread;
}

/*
 * gf_write_memory - Write bytes to a target process's memory.
 *
 * Same mechanism as read, but opens /proc/PID/mem for writing.
 * Can write to any mapped region, including .text (code) sections
 * that are normally read-only - the kernel allows this via ptrace.
 *
 * @pid:    Target process ID
 * @addr:   Virtual address in target's address space
 * @buf:    Data to write
 * @len:    Number of bytes to write
 *
 * Returns number of bytes written, or -1 on error.
 */
ssize_t gf_write_memory(pid_t pid, unsigned long addr, const void *buf, size_t len)
{
    char path[64];
    int fd;
    ssize_t nwritten;

    snprintf(path, sizeof(path), "/proc/%d/mem", pid);

    fd = open(path, O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "[patcher] Cannot open %s for write: %s\n",
                path, strerror(errno));
        return -1;
    }

    /* pwrite writes at an offset. The kernel bypasses page protections
     * for ptrace-attached processes, so we can write into .text segments. */
    nwritten = pwrite(fd, buf, len, (off_t)addr);
    if (nwritten == -1) {
        fprintf(stderr, "[patcher] pwrite failed at 0x%lx in PID %d: %s\n",
                addr, pid, strerror(errno));
    } else if ((size_t)nwritten != len) {
        fprintf(stderr, "[patcher] Short write at 0x%lx: wrote %zd, wanted %zu\n",
                addr, nwritten, len);
    }

    close(fd);
    return nwritten;
}

/*
 * gf_read_u32 - Convenience: read a single 32-bit value from the target.
 *
 * Useful for reading game variables (HP, item counts, flags, etc.)
 */
int gf_read_u32(pid_t pid, unsigned long addr, uint32_t *out)
{
    ssize_t n = gf_read_memory(pid, addr, out, sizeof(*out));
    return (n == sizeof(*out)) ? 0 : -1;
}

/*
 * gf_write_u32 - Convenience: write a single 32-bit value to the target.
 */
int gf_write_u32(pid_t pid, unsigned long addr, uint32_t value)
{
    ssize_t n = gf_write_memory(pid, addr, &value, sizeof(value));
    return (n == sizeof(value)) ? 0 : -1;
}
