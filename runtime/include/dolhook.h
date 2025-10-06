/**
 * DolHook - GameCube Function Hooking Library
 * 
 * Runtime library for safe inline detours on PowerPC32 (Gekko).
 * Provides memory patching, function hooking, and pattern scanning.
 */

#ifndef DOLHOOK_H
#define DOLHOOK_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Version */
#define DOLHOOK_VERSION_MAJOR 1
#define DOLHOOK_VERSION_MINOR 0
#define DOLHOOK_VERSION_PATCH 0

/* ============================================================================
 * Cache Maintenance
 * ========================================================================= */

/**
 * Synchronize instruction cache for modified code region.
 * Must be called after writing executable code.
 * 
 * @param addr Start address (any alignment)
 * @param len  Length in bytes
 */
void dh_icache_sync_range(void* addr, unsigned len);

/* ============================================================================
 * Memory Patching Primitives
 * ========================================================================= */

/**
 * Write 8-bit value with cache synchronization.
 * Atomic with respect to instruction fetch.
 */
void dh_write8(volatile void* p, uint8_t v);

/**
 * Write 16-bit value with cache synchronization (big-endian).
 */
void dh_write16(volatile void* p, uint16_t v);

/**
 * Write 32-bit value with cache synchronization (big-endian).
 */
void dh_write32(volatile void* p, uint32_t v);

/**
 * Suspend interrupts and return previous MSR state.
 * Must call dh_restore_interrupts() to restore.
 * 
 * @return Previous MSR value
 */
uint32_t dh_suspend_interrupts(void);

/**
 * Restore interrupt state from saved MSR.
 * 
 * @param saved_msr Value from dh_suspend_interrupts()
 */
void dh_restore_interrupts(uint32_t saved_msr);

/* ============================================================================
 * Branch Encoding Helpers
 * ========================================================================= */

/**
 * Create PowerPC branch immediate instruction.
 * Range: ±32MB from 'from' address.
 * 
 * @param from Source address
 * @param to   Target address
 * @param link 1 for bl (branch and link), 0 for b
 * @return Encoded instruction, or 0 if target out of range
 */
uint32_t dh_make_branch_imm(uint32_t from, uint32_t to, int link);

/**
 * Write absolute branch sequence (12 bytes).
 * Sequence: lis r12, hi16(to)
 *           ori r12, r12, lo16(to)
 *           mtctr r12
 *           bctr
 * 
 * Clobbers: r12, CTR
 * 
 * @param at Target location to write (overwrites 12 bytes)
 * @param to Destination address
 * @param link Ignored (absolute branches don't support link)
 */
void dh_write_branch_abs(void* at, void* to, int link);

/* ============================================================================
 * Function Hooking
 * ========================================================================= */

/**
 * Function hook descriptor.
 * Zero-initialize before first use.
 */
typedef struct dh_hook {
    void*    target;       /* Function address to detour */
    void*    replacement;  /* Your hook function */
    void*    trampoline;   /* Generated trampoline (call original) */
    uint8_t  saved[16];    /* Saved original bytes */
    uint32_t patch_len;    /* Bytes overwritten at target (4 or 12) */
} dh_hook;

/**
 * Install a function hook.
 * 
 * Replaces target function's prologue with branch to replacement.
 * Generates trampoline containing original prologue + jump back.
 * 
 * Requirements:
 * - h->target and h->replacement must be set
 * - First 16 bytes of target must be safe to overwrite
 * - Target prologue should not contain PC-relative branches
 * 
 * @param h Hook descriptor (must remain valid while hook active)
 * @return 0 on success, -1 on allocation failure, -2 if unsafe
 */
int dh_hook_install(dh_hook* h);

/**
 * Remove a previously installed hook.
 * Restores original bytes and frees trampoline.
 * 
 * @param h Hook descriptor from dh_hook_install()
 * @return 0 on success, -1 on error
 */
int dh_hook_remove(dh_hook* h);

/**
 * Create trampoline for calling original function.
 * Used internally by dh_hook_install().
 * 
 * @param target Function address
 * @param stolen_len Bytes to copy (must be >= bytes overwritten)
 * @return Executable trampoline buffer, or NULL on failure
 */
void* dh_make_trampoline(void* target, uint32_t stolen_len);

/* ============================================================================
 * Pattern Scanning (optional, disable with DOLHOOK_NO_PATTERN)
 * ========================================================================= */

#ifndef DOLHOOK_NO_PATTERN

/**
 * Search for byte pattern in memory region.
 * 
 * @param start Search region start
 * @param size  Search region size
 * @param pat   Pattern bytes (can include wildcards)
 * @param mask  Mask string: 'x' = match, '?' = wildcard
 * @return Pointer to first match, or NULL if not found
 * 
 * Example:
 *   // Find "48 ?? ?? 01" (bl instruction with unknown offset)
 *   char pat[] = {0x48, 0x00, 0x00, 0x01};
 *   char mask[] = "x??x";
 *   void* found = dh_find_pattern(start, size, pat, mask);
 */
void* dh_find_pattern(const void* start, size_t size,
                      const char* pat, const char* mask);

#endif /* DOLHOOK_NO_PATTERN */

/* ============================================================================
 * Logging (optional, uses OSReport if available)
 * ========================================================================= */

/**
 * Log formatted message.
 * Uses OSReport if found, otherwise no-op.
 * 
 * @param fmt Printf-style format string
 */
void dh_log(const char* fmt, ...);

/* ============================================================================
 * Initialization
 * ========================================================================= */

/**
 * Initialize DolHook runtime.
 * Called automatically by entry stub before game start.
 * Idempotent - safe to call multiple times.
 * 
 * Actions:
 * - Print banner
 * - Install user hooks via dh_install_all_hooks()
 */
void dh_init(void);

/**
 * User hook installation callback.
 * Implement this function to install your hooks.
 * Called once by dh_init().
 * 
 * Example:
 *   void dh_install_all_hooks(void) {
 *       static dh_hook my_hook;
 *       my_hook.target = (void*)0x80001234;
 *       my_hook.replacement = my_handler;
 *       dh_hook_install(&my_hook);
 *   }
 */
void dh_install_all_hooks(void);

/* ============================================================================
 * Internal/Advanced
 * ========================================================================= */

/**
 * Display banner message.
 * Tries OSReport first, falls back to VI text rendering.
 * Called by dh_init(), but can be called manually.
 */
void dh_banner(void);

#ifdef __cplusplus
}
#endif

#endif /* DOLHOOK_H */