# ptrace-core

Process attach/detach via Linux `ptrace()`.

## What it does

- `gf_attach_process()` calls `PTRACE_ATTACH`, which sends `SIGSTOP` to the target
- Blocks on `waitpid()` until the process is actually stopped
- Validates the stop status before returning
- `gf_detach_process()` calls `PTRACE_DETACH` to resume the target

## Why ptrace?

On Linux, `ptrace` is the standard mechanism for debuggers and similar tools. It gives us:
- Permission to read/write `/proc/PID/mem`
- Ability to get/set CPU registers (needed for syscall injection)
- Single-step execution (needed for remote mmap)

All other modules depend on this one.
