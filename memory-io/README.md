# memory-io

Read and write target process memory via `/proc/PID/mem`.

## How it works

- Opens `/proc/<pid>/mem` as a regular file
- Uses `pread()`/`pwrite()` with the virtual address as the offset
- The kernel allows writes to read-only pages (like `.text`) when ptrace-attached

## Why /proc/PID/mem instead of PTRACE_PEEKDATA?

`PTRACE_PEEKDATA` reads one machine word at a time (4 or 8 bytes per syscall). `/proc/PID/mem` lets us read/write arbitrary-length buffers in a single syscall, which is critical when writing multi-byte shellcode into code caves.

## Dependencies

Requires the target process to be ptrace-attached (see `ptrace-core/`).
