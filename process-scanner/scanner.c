/*
 * scanner.c - Process detection via /proc scanning
 *
 * Scans /proc for game server processes by matching their executable
 * name. Includes PID recycling detection: if a PID we patched before
 * now belongs to a different process (different start time), we treat
 * it as a new, unpatched process.
 *
 * Portfolio showcase - simplified from production code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>

#include "scanner.h"

/* Maximum number of server processes we track simultaneously */
#define MAX_SERVERS 16

/* Name of the target executable (basename) */
#define TARGET_EXE_NAME "GameServer"

/*
 * read_proc_starttime - Read the start time of a process from /proc/PID/stat.
 *
 * Field 22 in /proc/PID/stat is the process start time in jiffies.
 * This value is unique enough (combined with PID) to detect PID recycling.
 *
 * Returns the start time, or 0 on failure.
 */
static unsigned long long read_proc_starttime(pid_t pid)
{
    char path[64];
    char buf[1024];
    FILE *fp;
    unsigned long long starttime = 0;

    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    fp = fopen(path, "r");
    if (!fp)
        return 0;

    if (fgets(buf, sizeof(buf), fp)) {
        /* /proc/PID/stat format: pid (comm) state ppid ...
         * The comm field can contain spaces and parentheses, so we
         * find the LAST ')' to skip past it safely. */
        char *p = strrchr(buf, ')');
        if (p) {
            p += 2;  /* skip ") " */
            /* Fields after comm: state(3), ppid(4), pgrp(5), session(6),
             * tty_nr(7), tpgid(8), flags(9), minflt(10), cminflt(11),
             * majflt(12), cmajflt(13), utime(14), stime(15), cutime(16),
             * cstime(17), priority(18), nice(19), num_threads(20),
             * itrealvalue(21), starttime(22) */
            int field = 3;  /* we're at field 3 (state) */
            while (*p && field < 22) {
                if (*p == ' ')
                    field++;
                p++;
            }
            if (field == 22) {
                starttime = strtoull(p, NULL, 10);
            }
        }
    }

    fclose(fp);
    return starttime;
}

/*
 * get_proc_exe_name - Read the executable name for a PID.
 *
 * Reads the /proc/PID/exe symlink to get the full path of the
 * executable, then extracts the basename.
 *
 * Returns 0 on success, -1 on failure.
 */
static int get_proc_exe_name(pid_t pid, char *name, size_t name_len)
{
    char link_path[64];
    char exe_path[256];
    ssize_t len;
    char *base;

    snprintf(link_path, sizeof(link_path), "/proc/%d/exe", pid);

    len = readlink(link_path, exe_path, sizeof(exe_path) - 1);
    if (len == -1)
        return -1;

    exe_path[len] = '\0';

    /* Handle " (deleted)" suffix that the kernel appends if the binary
     * was replaced on disk while the process is running */
    char *deleted = strstr(exe_path, " (deleted)");
    if (deleted)
        *deleted = '\0';

    /* Extract basename */
    base = strrchr(exe_path, '/');
    base = base ? base + 1 : exe_path;

    strncpy(name, base, name_len - 1);
    name[name_len - 1] = '\0';
    return 0;
}

/*
 * gf_find_zone_servers - Scan /proc for running game server processes.
 *
 * Iterates over all numeric directories in /proc (each is a PID),
 * checks if the executable name matches our target, and returns a
 * list of PIDs.
 *
 * @pids:       Output array of PIDs found
 * @max_pids:   Maximum entries in the array
 *
 * Returns the number of matching processes found.
 */
int gf_find_zone_servers(pid_t *pids, int max_pids)
{
    DIR *proc_dir;
    struct dirent *entry;
    int count = 0;
    char exe_name[128];

    proc_dir = opendir("/proc");
    if (!proc_dir) {
        perror("[scanner] Cannot open /proc");
        return 0;
    }

    while ((entry = readdir(proc_dir)) != NULL && count < max_pids) {
        /* Only look at numeric directory names (each is a PID) */
        if (!isdigit((unsigned char)entry->d_name[0]))
            continue;

        pid_t pid = (pid_t)atoi(entry->d_name);
        if (pid <= 0)
            continue;

        /* Check if this process is our target */
        if (get_proc_exe_name(pid, exe_name, sizeof(exe_name)) == 0) {
            if (strcmp(exe_name, TARGET_EXE_NAME) == 0) {
                pids[count++] = pid;
            }
        }
    }

    closedir(proc_dir);
    return count;
}

/*
 * gf_check_pid_recycled - Detect if a PID was recycled (reused by a new process).
 *
 * After a process exits, the kernel can reassign its PID to a new
 * process. If we don't detect this, we'd think we already patched
 * a process that is actually brand new and unpatched.
 *
 * We compare the start time we recorded when first patching against
 * the current start time. If they differ, the PID was recycled.
 *
 * @pid:             The PID to check
 * @known_starttime: The start time we recorded when first patching
 *
 * Returns 1 if recycled (different process), 0 if same process.
 */
int gf_check_pid_recycled(pid_t pid, unsigned long long known_starttime)
{
    unsigned long long current_starttime = read_proc_starttime(pid);

    if (current_starttime == 0) {
        /* Process no longer exists */
        return 1;
    }

    return (current_starttime != known_starttime) ? 1 : 0;
}

/*
 * gf_get_starttime - Get the start time for a PID (to save for later comparison).
 */
unsigned long long gf_get_starttime(pid_t pid)
{
    return read_proc_starttime(pid);
}
