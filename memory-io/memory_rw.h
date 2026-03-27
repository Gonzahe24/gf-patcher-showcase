/*
 * memory_rw.h - Process memory read/write interface
 */

#ifndef GF_MEMORY_RW_H
#define GF_MEMORY_RW_H

#include <sys/types.h>
#include <stdint.h>

/* Read len bytes from addr in target process. Returns bytes read or -1. */
ssize_t gf_read_memory(pid_t pid, unsigned long addr, void *buf, size_t len);

/* Write len bytes to addr in target process. Returns bytes written or -1. */
ssize_t gf_write_memory(pid_t pid, unsigned long addr, const void *buf, size_t len);

/* Read/write a single uint32 value. Returns 0 on success. */
int gf_read_u32(pid_t pid, unsigned long addr, uint32_t *out);
int gf_write_u32(pid_t pid, unsigned long addr, uint32_t value);

#endif /* GF_MEMORY_RW_H */
