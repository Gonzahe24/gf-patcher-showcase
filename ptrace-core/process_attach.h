/*
 * process_attach.h - ptrace attach/detach interface
 */

#ifndef GF_PROCESS_ATTACH_H
#define GF_PROCESS_ATTACH_H

#include <sys/types.h>

/* Attach to a process, wait for it to stop. Returns 0 on success. */
int gf_attach_process(pid_t pid);

/* Detach from a process, resuming its execution. Returns 0 on success. */
int gf_detach_process(pid_t pid);

/* Check if a process is still alive. Returns 1 if alive, 0 if not. */
int gf_is_process_alive(pid_t pid);

#endif /* GF_PROCESS_ATTACH_H */
