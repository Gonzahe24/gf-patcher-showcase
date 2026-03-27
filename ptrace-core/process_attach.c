/*
 * process_attach.c - Process attachment via ptrace
 *
 * Attaches to a running process using ptrace, waits for it to stop,
 * and provides clean detach. This is the foundation for all memory
 * operations: the target must be stopped (SIGSTOP) before we can
 * read/write its memory or inject syscalls.
 *
 * Portfolio showcase - simplified from production code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include "process_attach.h"

/*
 * gf_attach_process - Attach to a target process via ptrace.
 *
 * PTRACE_ATTACH sends SIGSTOP to the target process. We then wait
 * for it to actually stop before returning. This is critical: if we
 * try to read/write memory before the process is stopped, the kernel
 * will reject the operation.
 *
 * Returns 0 on success, -1 on failure.
 */
int gf_attach_process(pid_t pid)
{
    int status;

    /* Request attachment - this sends SIGSTOP to the target */
    if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) == -1) {
        fprintf(stderr, "[patcher] ptrace ATTACH failed for PID %d: %s\n",
                pid, strerror(errno));
        return -1;
    }

    /* Wait for the process to actually stop.
     * waitpid blocks until the target receives the SIGSTOP and enters
     * the stopped state. Without this, subsequent ptrace operations
     * would race against the signal delivery. */
    if (waitpid(pid, &status, 0) == -1) {
        fprintf(stderr, "[patcher] waitpid failed for PID %d: %s\n",
                pid, strerror(errno));
        /* Try to detach even if wait failed */
        ptrace(PTRACE_DETACH, pid, NULL, NULL);
        return -1;
    }

    /* Verify the process actually stopped (not exited or signaled) */
    if (!WIFSTOPPED(status)) {
        fprintf(stderr, "[patcher] PID %d did not stop as expected (status=0x%x)\n",
                pid, status);
        ptrace(PTRACE_DETACH, pid, NULL, NULL);
        return -1;
    }

    return 0;
}

/*
 * gf_detach_process - Detach from a previously attached process.
 *
 * PTRACE_DETACH resumes the target process and removes our tracer
 * relationship. The process continues executing normally with all
 * in-memory patches still active.
 *
 * Returns 0 on success, -1 on failure.
 */
int gf_detach_process(pid_t pid)
{
    if (ptrace(PTRACE_DETACH, pid, NULL, NULL) == -1) {
        fprintf(stderr, "[patcher] ptrace DETACH failed for PID %d: %s\n",
                pid, strerror(errno));
        return -1;
    }

    return 0;
}

/*
 * gf_is_process_alive - Check if a process still exists.
 *
 * Uses kill(pid, 0) which doesn't send a signal but checks if the
 * process exists and we have permission to signal it.
 */
int gf_is_process_alive(pid_t pid)
{
    if (kill(pid, 0) == -1) {
        if (errno == ESRCH) {
            return 0;  /* Process does not exist */
        }
        /* EPERM means it exists but we lack permission - shouldn't happen
         * since we run as root, but treat as alive to be safe */
        return 1;
    }
    return 1;
}
