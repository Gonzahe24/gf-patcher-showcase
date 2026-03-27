/*
 * remote_mmap.c - Allocate executable memory inside a remote process
 *
 * This is the most advanced technique in the project. To create a code
 * cave (a new executable memory region) inside the target process, we
 * need to call mmap() in its context. Since we can't call functions in
 * the target directly, we:
 *
 *   1. Save the target's current CPU registers
 *   2. Save the bytes at the current instruction pointer (RIP)
 *   3. Write a SYSCALL instruction at RIP
 *   4. Set up registers for mmap (syscall number + arguments)
 *   5. Single-step the target to execute the SYSCALL
 *   6. Read the return value (the allocated address)
 *   7. Restore the original bytes and registers
 *
 * The target process never knows this happened.
 *
 * Portfolio showcase - simplified from production code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/mman.h>

#include "../memory-io/memory_rw.h"

/* x86-64 SYSCALL opcode: 0F 05 */
static const unsigned char SYSCALL_INSN[2] = { 0x0F, 0x05 };

/* Linux x86-64 syscall number for mmap */
#define SYS_MMAP 9

/*
 * gf_remote_mmap - Allocate RWX memory inside the target process.
 *
 * This performs a "syscall injection": we temporarily replace the
 * instruction at RIP with a SYSCALL instruction, set up the registers
 * to perform mmap(), single-step to execute it, then restore everything.
 *
 * @pid:    Target process (must be ptrace-attached and stopped)
 * @size:   Number of bytes to allocate
 *
 * Returns the address of the allocated region in the target, or 0 on failure.
 */
unsigned long gf_remote_mmap(pid_t pid, size_t size)
{
    struct user_regs_struct orig_regs, syscall_regs;
    unsigned char saved_bytes[2];
    unsigned long alloc_addr;
    int status;

    /* ----- Step 1: Save current register state ----- */
    if (ptrace(PTRACE_GETREGS, pid, NULL, &orig_regs) == -1) {
        fprintf(stderr, "[mmap] GETREGS failed: %s\n", strerror(errno));
        return 0;
    }

    /* ----- Step 2: Save the 2 bytes at current RIP -----
     * We need to restore these after executing our SYSCALL. */
    if (gf_read_memory(pid, orig_regs.rip, saved_bytes, 2) != 2) {
        fprintf(stderr, "[mmap] Failed to save bytes at RIP 0x%llx\n",
                (unsigned long long)orig_regs.rip);
        return 0;
    }

    /* ----- Step 3: Write SYSCALL opcode (0F 05) at RIP -----
     * This overwrites whatever instruction was about to execute.
     * We'll restore it later. */
    if (gf_write_memory(pid, orig_regs.rip, SYSCALL_INSN, 2) != 2) {
        fprintf(stderr, "[mmap] Failed to write SYSCALL at RIP\n");
        return 0;
    }

    /* ----- Step 4: Set up registers for mmap syscall -----
     *
     * Linux x86-64 syscall convention:
     *   RAX = syscall number (9 = mmap)
     *   RDI = addr   (0 = let kernel choose)
     *   RSI = length  (requested size)
     *   RDX = prot   (PROT_READ | PROT_WRITE | PROT_EXEC)
     *   R10 = flags  (MAP_PRIVATE | MAP_ANONYMOUS)
     *   R8  = fd     (-1 for anonymous)
     *   R9  = offset (0)
     */
    syscall_regs = orig_regs;
    syscall_regs.rax = SYS_MMAP;
    syscall_regs.rdi = 0;                                       /* addr: kernel chooses */
    syscall_regs.rsi = size;                                    /* length */
    syscall_regs.rdx = PROT_READ | PROT_WRITE | PROT_EXEC;     /* prot: RWX */
    syscall_regs.r10 = MAP_PRIVATE | MAP_ANONYMOUS;             /* flags */
    syscall_regs.r8  = (unsigned long long)-1;                  /* fd: -1 (anonymous) */
    syscall_regs.r9  = 0;                                       /* offset */

    if (ptrace(PTRACE_SETREGS, pid, NULL, &syscall_regs) == -1) {
        fprintf(stderr, "[mmap] SETREGS failed: %s\n", strerror(errno));
        goto restore_bytes;
    }

    /* ----- Step 5: Single-step to execute the SYSCALL -----
     * PTRACE_SINGLESTEP executes exactly one instruction (our SYSCALL)
     * then stops the process again. */
    if (ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL) == -1) {
        fprintf(stderr, "[mmap] SINGLESTEP failed: %s\n", strerror(errno));
        goto restore_all;
    }

    /* Wait for the single-step to complete */
    if (waitpid(pid, &status, 0) == -1) {
        fprintf(stderr, "[mmap] waitpid after SINGLESTEP failed: %s\n",
                strerror(errno));
        goto restore_all;
    }

    /* ----- Step 6: Read the return value -----
     * After a syscall, the return value is in RAX.
     * For mmap, this is the allocated address (or MAP_FAILED). */
    if (ptrace(PTRACE_GETREGS, pid, NULL, &syscall_regs) == -1) {
        fprintf(stderr, "[mmap] GETREGS after syscall failed: %s\n",
                strerror(errno));
        goto restore_all;
    }

    alloc_addr = syscall_regs.rax;

    /* Check for mmap failure (returns -errno on error, or
     * MAP_FAILED which is (void*)-1) */
    if ((long long)alloc_addr < 0 && (long long)alloc_addr > -4096) {
        fprintf(stderr, "[mmap] Remote mmap failed in PID %d: error %lld\n",
                pid, -(long long)alloc_addr);
        alloc_addr = 0;
    } else {
        printf("[mmap] Allocated %zu bytes at 0x%lx in PID %d\n",
               size, alloc_addr, pid);
    }

    /* ----- Step 7: Restore everything ----- */
restore_all:
    /* Restore original registers (including RIP) */
    if (ptrace(PTRACE_SETREGS, pid, NULL, &orig_regs) == -1) {
        fprintf(stderr, "[mmap] CRITICAL: Failed to restore regs for PID %d!\n", pid);
    }

restore_bytes:
    /* Restore the original bytes at RIP */
    if (gf_write_memory(pid, orig_regs.rip, saved_bytes, 2) != 2) {
        fprintf(stderr, "[mmap] CRITICAL: Failed to restore bytes at RIP for PID %d!\n", pid);
    }

    return alloc_addr;
}
