# daemon

Main daemon loop that ties all modules together.

## Lifecycle

```
while (running):
    1. Check config file → reload if modified
    2. Scan /proc for server processes
    3. For each new or recycled process:
       a. ptrace attach (stop process)
       b. Apply all enabled patches
       c. ptrace detach (resume process)
    4. Remove dead PIDs from tracking
    5. Sleep 2 seconds
```

## Graceful degradation

Each patch is applied independently. If one fails (bad address, mmap failure, etc.), the daemon logs the error and continues with the remaining patches. This is critical for production reliability.

## Signal handling

`SIGTERM` and `SIGINT` trigger a clean shutdown. Any attached processes are detached before exit.
