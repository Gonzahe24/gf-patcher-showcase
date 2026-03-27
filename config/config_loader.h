/*
 * config_loader.h - Configuration loader interface
 */

#ifndef GF_CONFIG_LOADER_H
#define GF_CONFIG_LOADER_H

#include <time.h>

#define MAX_PATCHES 64
#define MAX_PATCH_NAME 48

struct gf_config {
    const char  *config_path;                   /* Path to config file */
    char         patch_names[MAX_PATCHES][MAX_PATCH_NAME];  /* Registered patch keys */
    int          patch_enabled[MAX_PATCHES];    /* 1=enabled, 0=disabled */
    int          num_patches;                   /* Number of registered patches */
    time_t       last_load_time;                /* When we last loaded the file */
};

/* Load/reload the configuration file. Returns 0 on success. */
int gf_load_config(struct gf_config *cfg);

/* Check if the config file needs reloading. Returns 1 if yes. */
int gf_config_needs_reload(const struct gf_config *cfg);

#endif /* GF_CONFIG_LOADER_H */
