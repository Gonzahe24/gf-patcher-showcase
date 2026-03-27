/*
 * config_loader.c - Hot-reloadable key=value configuration parser
 *
 * Reads a simple config file where each line is either:
 *   key=value       (toggle a patch on/off)
 *   # comment       (ignored)
 *   empty line      (ignored)
 *
 * The config is re-read every 2 seconds, allowing operators to
 * toggle patches on a live server without restarting the daemon.
 *
 * Portfolio showcase - simplified from production code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

#include "config_loader.h"

/* Default config file path */
#define CONFIG_PATH "/etc/gf-patcher/patches.conf"

/* How often to check for config changes (seconds) */
#define CONFIG_RELOAD_INTERVAL 2

/*
 * parse_config_line - Parse a single "key=value" line.
 *
 * Strips leading/trailing whitespace, skips comments and blank lines.
 * Writes the key and value into the provided buffers.
 *
 * Returns 0 on success, -1 if the line should be skipped.
 */
static int parse_config_line(const char *line, char *key, size_t key_len,
                             char *value, size_t val_len)
{
    const char *p = line;
    const char *eq;
    const char *end;

    /* Skip leading whitespace */
    while (*p && isspace((unsigned char)*p))
        p++;

    /* Skip empty lines and comments */
    if (*p == '\0' || *p == '#' || *p == '\n')
        return -1;

    /* Find the '=' separator */
    eq = strchr(p, '=');
    if (!eq)
        return -1;

    /* Extract key (trim trailing whitespace) */
    end = eq - 1;
    while (end > p && isspace((unsigned char)*end))
        end--;

    size_t klen = (size_t)(end - p + 1);
    if (klen >= key_len)
        klen = key_len - 1;
    memcpy(key, p, klen);
    key[klen] = '\0';

    /* Extract value (trim leading and trailing whitespace) */
    p = eq + 1;
    while (*p && isspace((unsigned char)*p))
        p++;

    end = p + strlen(p) - 1;
    while (end > p && isspace((unsigned char)*end))
        end--;

    size_t vlen = (size_t)(end - p + 1);
    if (vlen >= val_len)
        vlen = val_len - 1;
    memcpy(value, p, vlen);
    value[vlen] = '\0';

    return 0;
}

/*
 * gf_load_config - Load the configuration file into the patch toggle array.
 *
 * Each recognized key maps to a patch slot. The value is expected to
 * be "1" (enabled) or "0" (disabled). Unknown keys are ignored with
 * a warning, making the config forward-compatible.
 *
 * @cfg:  Configuration struct to fill
 *
 * Returns 0 on success, -1 if the config file cannot be opened.
 */
int gf_load_config(struct gf_config *cfg)
{
    FILE *fp;
    char line[256];
    char key[64], value[64];

    fp = fopen(cfg->config_path ? cfg->config_path : CONFIG_PATH, "r");
    if (!fp) {
        fprintf(stderr, "[config] Cannot open config file\n");
        return -1;
    }

    /* Reset all patches to disabled before loading */
    memset(cfg->patch_enabled, 0, sizeof(cfg->patch_enabled));

    while (fgets(line, sizeof(line), fp)) {
        if (parse_config_line(line, key, sizeof(key), value, sizeof(value)) != 0)
            continue;

        /* Look up the key in our known patch list */
        int found = 0;
        for (int i = 0; i < cfg->num_patches; i++) {
            if (strcmp(key, cfg->patch_names[i]) == 0) {
                cfg->patch_enabled[i] = (atoi(value) != 0) ? 1 : 0;
                found = 1;
                break;
            }
        }

        if (!found) {
            fprintf(stderr, "[config] Unknown key '%s', ignoring\n", key);
        }
    }

    fclose(fp);
    cfg->last_load_time = time(NULL);
    return 0;
}

/*
 * gf_config_needs_reload - Check if it's time to re-read the config.
 *
 * Compares current time against the last load time. Also checks the
 * file's modification time so we don't re-parse an unchanged file.
 *
 * Returns 1 if the config should be reloaded, 0 otherwise.
 */
int gf_config_needs_reload(const struct gf_config *cfg)
{
    struct stat st;
    time_t now = time(NULL);

    /* Don't check more often than the reload interval */
    if (difftime(now, cfg->last_load_time) < CONFIG_RELOAD_INTERVAL)
        return 0;

    /* Check if the file was modified since we last loaded it */
    if (stat(cfg->config_path ? cfg->config_path : CONFIG_PATH, &st) == -1)
        return 0;

    return (st.st_mtime > cfg->last_load_time) ? 1 : 0;
}
