/*
 * scanner.h - Process scanner interface
 */

#ifndef GF_SCANNER_H
#define GF_SCANNER_H

#include <sys/types.h>

/* Scan /proc for game server processes. Returns count of PIDs found. */
int gf_find_zone_servers(pid_t *pids, int max_pids);

/* Check if a PID was recycled since we last saw it. Returns 1 if recycled. */
int gf_check_pid_recycled(pid_t pid, unsigned long long known_starttime);

/* Get the start time of a process (for PID recycle detection). */
unsigned long long gf_get_starttime(pid_t pid);

#endif /* GF_SCANNER_H */
