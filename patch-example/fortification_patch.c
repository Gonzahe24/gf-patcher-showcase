/*
 * fortification_patch.c - Complete patch example: Fortification Protection
 *
 * This file demonstrates the full patching flow from start to finish:
 *
 *   1. Build hand-assembled x86 shellcode (the code cave body)
 *   2. Allocate executable memory inside the target (remote mmap)
 *   3. Write the shellcode into the allocated cave
 *   4. Install a JMP hook at the original function to redirect into the cave
 *
 * Patch purpose: Prevent item destruction on failed fortification (upgrade)
 * attempts. The original code deletes the item on failure; our patch
 * redirects the failure path to skip the deletion and return the item
 * intact to the player.
 *
 * Portfolio showcase - simplified from production code.
 * All memory addresses replaced with symbolic placeholders.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>

#include "../memory-io/memory_rw.h"

/* ============================================================
 * Address placeholders (would be concrete addresses in production,
 * determined by reverse-engineering the target binary)
 * ============================================================ */

/* Address of the fortification handler function we're hooking */
#define ADDR_FORT_HANDLER       0xAAAA0000UL

/* Address where the handler checks for failure */
#define ADDR_FORT_FAIL_CHECK    0xAAAA0020UL

/* Address of the "safe exit" path (item kept, returns success to client) */
#define ADDR_SAFE_EXIT          0xAAAA0100UL

/* Address of the item deletion call we want to skip */
#define ADDR_DELETE_ITEM_CALL   0xAAAA0050UL

/* Address of the item pointer dereference (operator[] on the item container) */
#define ADDR_ITEM_OPERATOR      0xAAAA1000UL

/* Address of the "send result to client" function */
#define ADDR_SEND_RESULT        0xAAAA2000UL

/* Size of a JMP instruction (E9 xx xx xx xx) on x86 */
#define JMP_SIZE 5

/* Size of our code cave */
#define CAVE_SIZE 128

/* ============================================================
 * Shellcode builder
 * ============================================================ */

/*
 * fort_build_code_cave - Assemble the code cave shellcode.
 *
 * The cave does the following:
 *   1. Save registers (pushad equivalent on x86-32)
 *   2. Load the item pointer from the stack frame
 *   3. Check the fortification result flag
 *   4. If failure: skip the delete, send "fail but item kept" to client
 *   5. Restore registers and jump back to the safe exit path
 *
 * All machine code is hand-assembled byte-by-byte. This avoids needing
 * an assembler at runtime and gives us complete control over the output.
 *
 * @buf:        Output buffer for the shellcode
 * @cave_addr:  The address where this cave will be placed (needed for
 *              relative JMP/CALL calculations)
 *
 * Returns the number of bytes written to buf.
 */
static int fort_build_code_cave(unsigned char *buf, unsigned long cave_addr)
{
    int off = 0;

    /* ---- Save all general-purpose registers ----
     * PUSHAD: 60
     * Saves EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI to the stack.
     * We restore them before jumping back so the original code is
     * unaffected by our detour. */
    buf[off++] = 0x60;                          /* PUSHAD */

    /* ---- Load the item pointer ----
     * MOV EAX, [EBP+0x08]: 8B 45 08
     * EBP+0x08 is the first argument to the fortification handler,
     * which is the item pointer. We need it to send the result. */
    buf[off++] = 0x8B;                          /* MOV EAX, [EBP+imm8] */
    buf[off++] = 0x45;
    buf[off++] = 0x08;

    /* ---- Check the fortification result ----
     * CMP DWORD PTR [EAX+0x10], 0: 83 78 10 00
     * Offset 0x10 in the item structure holds the fortification result.
     * 0 = success, non-zero = failure. */
    buf[off++] = 0x83;                          /* CMP [EAX+imm8], imm8 */
    buf[off++] = 0x78;
    buf[off++] = 0x10;
    buf[off++] = 0x00;

    /* ---- If success (result == 0), skip our protection ----
     * JE +offset (jump over the protection block)
     * JE: 74 xx */
    buf[off++] = 0x74;                          /* JE short */
    int je_target_offset = off;                 /* Save position to patch later */
    buf[off++] = 0x00;                          /* Placeholder, patched below */

    /* ==== FAILURE PATH: Apply protection (skip item deletion) ==== */

    /* ---- Push arguments for "send result to client" ----
     * PUSH 0x02: 6A 02
     * 0x02 = "fortification failed, item preserved" result code */
    buf[off++] = 0x6A;                          /* PUSH imm8 */
    buf[off++] = 0x02;                          /* Result: fail-but-kept */

    /* ---- Push the item pointer as the second argument ----
     * PUSH EAX: 50 */
    buf[off++] = 0x50;                          /* PUSH EAX */

    /* ---- Call the "send result" function ----
     * CALL rel32: E8 xx xx xx xx
     * The offset is relative: target - (current_addr + 5) */
    buf[off++] = 0xE8;                          /* CALL rel32 */
    {
        unsigned long call_site = cave_addr + off;
        int32_t rel = (int32_t)(ADDR_SEND_RESULT - (call_site + 4));
        memcpy(&buf[off], &rel, 4);
        off += 4;
    }

    /* ---- Clean up the stack (2 args * 4 bytes = 8) ----
     * ADD ESP, 8: 83 C4 08 */
    buf[off++] = 0x83;                          /* ADD ESP, imm8 */
    buf[off++] = 0xC4;
    buf[off++] = 0x08;

    /* ---- Restore registers and jump to safe exit ----
     * POPAD: 61
     * JMP rel32: E9 xx xx xx xx */
    buf[off++] = 0x61;                          /* POPAD */

    buf[off++] = 0xE9;                          /* JMP rel32 */
    {
        unsigned long jmp_site = cave_addr + off;
        int32_t rel = (int32_t)(ADDR_SAFE_EXIT - (jmp_site + 4));
        memcpy(&buf[off], &rel, 4);
        off += 4;
    }

    /* ==== SUCCESS PATH: Let the original code handle it ==== */

    /* Patch the JE offset to jump here */
    buf[je_target_offset] = (unsigned char)(off - je_target_offset - 1);

    /* ---- Restore registers and jump back to original flow ----
     * POPAD: 61
     * Execute the instruction(s) we overwrote with the JMP hook,
     * then jump to the instruction after the hook site.
     *
     * The overwritten instruction was: SUB ESP, 0x10 (83 EC 10)
     * We reproduce it here so the original code flow is preserved. */
    buf[off++] = 0x61;                          /* POPAD */

    buf[off++] = 0x83;                          /* SUB ESP, 0x10 */
    buf[off++] = 0xEC;                          /* (original overwritten bytes) */
    buf[off++] = 0x10;

    /* JMP back to the instruction right after our hook site */
    buf[off++] = 0xE9;                          /* JMP rel32 */
    {
        unsigned long jmp_site = cave_addr + off;
        unsigned long return_addr = ADDR_FORT_HANDLER + JMP_SIZE;
        int32_t rel = (int32_t)(return_addr - (jmp_site + 4));
        memcpy(&buf[off], &rel, 4);
        off += 4;
    }

    return off;
}

/* ============================================================
 * Hook installer
 * ============================================================ */

/* Forward declaration (implemented in code-caves/remote_mmap.c) */
extern unsigned long gf_remote_mmap(pid_t pid, size_t size);

/*
 * gf_apply_fortification_protection - Apply the full patch to a target process.
 *
 * This is the high-level entry point that orchestrates the entire flow:
 *   1. Allocate a code cave in the target via remote mmap
 *   2. Build the shellcode with the cave's actual address
 *   3. Write the shellcode into the cave
 *   4. Install a JMP hook at the fortification handler
 *
 * @pid: Target process (must be ptrace-attached)
 *
 * Returns the cave address on success, 0 on failure.
 */
unsigned long gf_apply_fortification_protection(pid_t pid)
{
    unsigned char cave_code[CAVE_SIZE];
    unsigned char jmp_hook[JMP_SIZE];
    unsigned long cave_addr;
    int cave_len;

    printf("[fort] Applying fortification protection to PID %d...\n", pid);

    /* Step 1: Allocate executable memory in the target process.
     * This calls our remote mmap function, which injects a syscall
     * into the target to allocate RWX memory. */
    cave_addr = gf_remote_mmap(pid, CAVE_SIZE);
    if (cave_addr == 0) {
        fprintf(stderr, "[fort] Failed to allocate code cave\n");
        return 0;
    }

    printf("[fort] Code cave allocated at 0x%lx\n", cave_addr);

    /* Step 2: Build the shellcode.
     * The cave address is needed because JMP/CALL instructions use
     * relative offsets (target - current_address). */
    memset(cave_code, 0x90, sizeof(cave_code));  /* Fill with NOP for safety */
    cave_len = fort_build_code_cave(cave_code, cave_addr);

    printf("[fort] Shellcode assembled: %d bytes\n", cave_len);

    /* Step 3: Write the shellcode into the code cave.
     * This uses /proc/PID/mem to write into the RWX region we
     * just allocated. */
    if (gf_write_memory(pid, cave_addr, cave_code, cave_len) != cave_len) {
        fprintf(stderr, "[fort] Failed to write code cave\n");
        return 0;
    }

    /* Step 4: Install the JMP hook at the original function.
     *
     * We overwrite the first 5 bytes of the fortification handler with:
     *   E9 xx xx xx xx    (JMP rel32 to our cave)
     *
     * The relative offset is: cave_addr - (hook_addr + 5)
     *
     * When the game server calls the fortification handler, it
     * immediately jumps to our cave instead of running the original
     * code. Our cave handles the logic, then jumps back. */
    jmp_hook[0] = 0xE9;  /* JMP rel32 opcode */
    {
        int32_t rel = (int32_t)(cave_addr - (ADDR_FORT_HANDLER + JMP_SIZE));
        memcpy(&jmp_hook[1], &rel, 4);
    }

    if (gf_write_memory(pid, ADDR_FORT_HANDLER, jmp_hook, JMP_SIZE) != JMP_SIZE) {
        fprintf(stderr, "[fort] Failed to install JMP hook\n");
        return 0;
    }

    printf("[fort] JMP hook installed at 0x%lx -> 0x%lx\n",
           ADDR_FORT_HANDLER, cave_addr);
    printf("[fort] Fortification protection active for PID %d\n", pid);

    return cave_addr;
}
