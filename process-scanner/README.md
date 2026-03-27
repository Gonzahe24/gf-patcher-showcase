# process-scanner

Automatic process detection via `/proc` scanning with PID recycle detection.

## How it works

1. Iterates all numeric directories in `/proc` (each represents a PID)
2. Reads `/proc/PID/exe` symlink to get the executable name
3. Matches against the target server binary name
4. Records `/proc/PID/stat` field 22 (start time in jiffies) for each found process

## PID recycling problem

Linux reuses PIDs. If a server crashes and a new one starts, the new process might get the same PID. Without detection, we'd skip it thinking it's already patched.

Solution: compare the start time from `/proc/PID/stat`. If it changed, it's a new process and needs patching.
