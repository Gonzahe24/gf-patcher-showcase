# config

Hot-reloadable key=value configuration parser.

## Config file format

```ini
# Enable/disable individual patches (1=on, 0=off)
fortification_protection=1
pvp_balance=0
drop_rate_fix=1
```

## Hot-reload

The daemon checks the config file every 2 seconds. If the file's modification time changed, it re-reads all values. This lets operators toggle patches on a live server without restarting anything.

## Design

- Unknown keys are warned but ignored (forward-compatible)
- All patches default to disabled until explicitly enabled
- Whitespace and comments are handled gracefully
