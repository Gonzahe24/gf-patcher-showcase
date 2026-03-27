/*
 * main_loop.c - Main daemon loop
 *
 * Orchestrates the entire patcher lifecycle:
 *   1. Reload config (if changed)
 *   2. Scan for game server processes
 *   3. For each new/recycled process: attach, apply patches, detach
 *   4. Sleep briefly and repeat
 *
 * Portfolio showcase - simplified from production code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>

#include "../ptrace-core/process_attach.h"
#include "../memory-io/memory_rw.h"
#include "../process-scanner/scanner.h"
#include "../config/config_loader.h"

/* Maximum number of processes we track */
#define MAX_TRACKED 16

/* Daemon polling interval (seconds) */
#define POLL_INTERVAL 2

/* Patch IDs (indices into config's patch_enabled array) */
enum {
    PATCH_FORTIFICATION = 0,
    PATCH_PVP_BALANCE,
    PATCH_DROP_RATE,
    PATCH_SKILL_FIX,
    /* ... up to 40+ patches in production ... */
    PATCH_COUNT
};

/* State for a tracked process */
struct tracked_process {
    pid_t               pid;
    unsigned long long  starttime;      /* For PID recycle detection */
    int                 patched;        /* 1 if we already applied patches */
    unsigned long       cave_addrs[PATCH_COUNT]; /* Allocated cave addresses */
};

/* Global state */
static volatile int g_running = 1;
static struct tracked_process g_tracked[MAX_TRACKED];
static int g_num_tracked = 0;

/* Forward declarations for patch functions */
extern unsigned long gf_apply_fortification_protection(pid_t pid);
/* extern unsigned long gf_apply_pvp_balance(pid_t pid); */
/* extern unsigned long gf_apply_drop_rate_fix(pid_t pid); */
/* ... etc ... */

/*
 * Signal handler for clean shutdown.
 */
static void handle_signal(int sig)
{
    (void)sig;
    printf("[daemon] Received signal, shutting down...\n");
    g_running = 0;
}

/*
 * find_tracked - Look up a PID in our tracked list.
 * Returns the index, or -1 if not found.
 */
static int find_tracked(pid_t pid)
{
    for (int i = 0; i < g_num_tracked; i++) {
        if (g_tracked[i].pid == pid)
            return i;
    }
    return -1;
}

/*
 * apply_all_patches - Apply enabled patches to a target process.
 *
 * The process must already be ptrace-attached. Each patch is applied
 * independently; if one fails, the others still proceed (graceful
 * degradation).
 *
 * @pid:  Target process ID
 * @cfg:  Current configuration (which patches are enabled)
 * @idx:  Index in the tracked process array (to store cave addresses)
 */
static void apply_all_patches(pid_t pid, const struct gf_config *cfg, int idx)
{
    int applied = 0;
    int failed = 0;

    /* Fortification protection */
    if (cfg->patch_enabled[PATCH_FORTIFICATION]) {
        unsigned long cave = gf_apply_fortification_protection(pid);
        if (cave) {
            g_tracked[idx].cave_addrs[PATCH_FORTIFICATION] = cave;
            applied++;
        } else {
            fprintf(stderr, "[daemon] Fortification patch FAILED for PID %d\n", pid);
            failed++;
        }
    }

    /* Additional patches would follow the same pattern:
     *
     * if (cfg->patch_enabled[PATCH_PVP_BALANCE]) {
     *     unsigned long cave = gf_apply_pvp_balance(pid);
     *     ...
     * }
     *
     * Each patch is independent. A failure in one does not affect others.
     */

    printf("[daemon] PID %d: %d patches applied, %d failed\n",
           pid, applied, failed);
}

/*
 * gf_main_loop - The core daemon loop.
 *
 * Runs indefinitely (until SIGTERM/SIGINT), performing the scan-attach-
 * patch-detach cycle every POLL_INTERVAL seconds.
 *
 * @cfg: Configuration struct (pre-loaded with patch names)
 */
void gf_main_loop(struct gf_config *cfg)
{
    pid_t found_pids[MAX_TRACKED];
    int found_count;

    /* Set up signal handlers for clean shutdown */
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    printf("[daemon] Patcher daemon started. Polling every %d seconds.\n",
           POLL_INTERVAL);

    /* Initial config load */
    gf_load_config(cfg);

    while (g_running) {

        /* ---- Step 1: Reload config if it changed ---- */
        if (gf_config_needs_reload(cfg)) {
            printf("[daemon] Config file changed, reloading...\n");
            gf_load_config(cfg);
        }

        /* ---- Step 2: Scan for server processes ---- */
        found_count = gf_find_zone_servers(found_pids, MAX_TRACKED);

        /* ---- Step 3: Process each found server ---- */
        for (int i = 0; i < found_count; i++) {
            pid_t pid = found_pids[i];
            int idx = find_tracked(pid);

            if (idx >= 0) {
                /* We've seen this PID before. Check for recycling. */
                if (gf_check_pid_recycled(pid, g_tracked[idx].starttime)) {
                    printf("[daemon] PID %d was recycled (new process)\n", pid);
                    /* Reset tracking - treat as new */
                    g_tracked[idx].patched = 0;
                    g_tracked[idx].starttime = gf_get_starttime(pid);
                    memset(g_tracked[idx].cave_addrs, 0,
                           sizeof(g_tracked[idx].cave_addrs));
                } else if (g_tracked[idx].patched) {
                    /* Already patched, skip */
                    continue;
                }
            } else {
                /* New PID, add to tracking */
                if (g_num_tracked >= MAX_TRACKED) {
                    fprintf(stderr, "[daemon] Tracking table full, skipping PID %d\n",
                            pid);
                    continue;
                }
                idx = g_num_tracked++;
                g_tracked[idx].pid = pid;
                g_tracked[idx].starttime = gf_get_starttime(pid);
                g_tracked[idx].patched = 0;
                memset(g_tracked[idx].cave_addrs, 0,
                       sizeof(g_tracked[idx].cave_addrs));
            }

            /* ---- Attach, patch, detach ---- */
            printf("[daemon] Attaching to PID %d...\n", pid);

            if (gf_attach_process(pid) != 0) {
                fprintf(stderr, "[daemon] Failed to attach to PID %d\n", pid);
                continue;
            }

            apply_all_patches(pid, cfg, idx);

            if (gf_detach_process(pid) != 0) {
                fprintf(stderr, "[daemon] Failed to detach from PID %d\n", pid);
            }

            g_tracked[idx].patched = 1;
            printf("[daemon] PID %d patched and detached.\n", pid);
        }

        /* ---- Step 4: Clean up dead processes from tracking ---- */
        for (int i = 0; i < g_num_tracked; i++) {
            if (!gf_is_process_alive(g_tracked[i].pid)) {
                printf("[daemon] PID %d no longer exists, removing from tracking\n",
                       g_tracked[i].pid);
                /* Swap with last entry for O(1) removal */
                g_tracked[i] = g_tracked[g_num_tracked - 1];
                g_num_tracked--;
                i--;  /* Re-check this index */
            }
        }

        /* ---- Sleep until next cycle ---- */
        sleep(POLL_INTERVAL);
    }

    printf("[daemon] Patcher daemon stopped.\n");
}
