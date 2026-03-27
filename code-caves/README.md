# code-caves

Remote `mmap()` via syscall injection -- the most advanced technique in the project.

## The problem

We need executable memory inside the target process to hold our custom code (shellcode). But we can't call `mmap()` from our process -- that would allocate in *our* address space, not the target's.

## The solution: syscall hijacking

1. Save the target's CPU state (all registers + 2 bytes at RIP)
2. Overwrite the instruction at RIP with `SYSCALL` (opcode `0F 05`)
3. Load mmap arguments into registers (RAX=9, RDI=0, RSI=size, RDX=RWX, R10=MAP_PRIVATE|MAP_ANONYMOUS)
4. Single-step the target -- it executes the SYSCALL, kernel allocates memory
5. Read RAX for the allocated address
6. Restore the original registers and bytes

The target process never sees this happen. It resumes exactly where it was.

## Why RWX?

The allocated region needs `PROT_EXEC` (to run code), `PROT_WRITE` (to write shellcode into it), and `PROT_READ` (for data reads within the cave).
