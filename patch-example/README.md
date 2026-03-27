# patch-example

A complete end-to-end patch: fortification (item upgrade) protection.

## What the patch does

In the original server, if an item upgrade ("fortification") fails, the item is destroyed. This patch intercepts the failure path and skips the deletion, keeping the item intact.

## The patching flow

```
1. gf_remote_mmap()           Allocate RWX memory inside the target process
       |
2. fort_build_code_cave()     Hand-assemble x86 shellcode (PUSHAD, checks, CALL, JMP)
       |
3. gf_write_memory()          Write shellcode into the allocated cave
       |
4. Install JMP hook           Overwrite first 5 bytes of original function:
                               E9 xx xx xx xx  (JMP rel32 -> cave)
```

## Key x86 instructions used

- `PUSHAD/POPAD` (60/61) -- save/restore all registers around our custom logic
- `CMP [reg+off], imm` (83 78 xx xx) -- check the fortification result flag
- `JE` (74 xx) -- conditional branch for success vs failure
- `CALL rel32` (E8 xx xx xx xx) -- call server functions from inside the cave
- `JMP rel32` (E9 xx xx xx xx) -- jump back to original code flow

All offsets are relative, computed as `target - (current_address + instruction_size)`.
