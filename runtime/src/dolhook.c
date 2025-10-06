/**
 * DolHook Runtime Implementation
 * Core memory patching and function hooking for GameCube (PPC Gekko)
 */

#include "dolhook.h"
#include <string.h>
#include <stdarg.h>

/* Simple heap for trampolines (16KB static buffer) */
#define TRAMPOLINE_POOL_SIZE 16384
static uint8_t g_trampoline_pool[TRAMPOLINE_POOL_SIZE] __attribute__((aligned(32)));
static uint32_t g_trampoline_offset = 0;
static int g_initialized = 0;

/* Weak OSReport symbol - may be resolved by game or not */
extern void OSReport(const char* fmt, ...) __attribute__((weak));

/* ============================================================================
 * Cache Maintenance
 * ========================================================================= */

void dh_icache_sync_range(void* addr, unsigned len) {
    uint32_t start = (uint32_t)addr & ~31;
    uint32_t end = ((uint32_t)addr + len + 31) & ~31;
    
    /* Flush data cache */
    for (uint32_t p = start; p < end; p += 32) {
        asm volatile("dcbf 0, %0" : : "r"(p) : "memory");
    }
    asm volatile("sync" : : : "memory");
    
    /* Invalidate instruction cache */
    for (uint32_t p = start; p < end; p += 32) {
        asm volatile("icbi 0, %0" : : "r"(p) : "memory");
    }
    asm volatile("sync; isync" : : : "memory");
}

/* ============================================================================
 * Interrupt Control
 * ========================================================================= */

uint32_t dh_suspend_interrupts(void) {
    uint32_t msr;
    asm volatile(
        "mfmsr %0\n"
        "rlwinm 3, %0, 0, 17, 15\n"  /* Clear EE bit (bit 16) */
        "mtmsr 3\n"
        : "=r"(msr) : : "r3", "memory"
    );
    return msr;
}

void dh_restore_interrupts(uint32_t saved_msr) {
    asm volatile("mtmsr %0" : : "r"(saved_msr) : "memory");
}

/* ============================================================================
 * Memory Write Primitives
 * ========================================================================= */

void dh_write8(volatile void* p, uint8_t v) {
    uint32_t msr = dh_suspend_interrupts();
    *(volatile uint8_t*)p = v;
    dh_icache_sync_range((void*)p, 1);
    dh_restore_interrupts(msr);
}

void dh_write16(volatile void* p, uint16_t v) {
    uint32_t msr = dh_suspend_interrupts();
    *(volatile uint16_t*)p = v;
    dh_icache_sync_range((void*)p, 2);
    dh_restore_interrupts(msr);
}

void dh_write32(volatile void* p, uint32_t v) {
    uint32_t msr = dh_suspend_interrupts();
    *(volatile uint32_t*)p = v;
    dh_icache_sync_range((void*)p, 4);
    dh_restore_interrupts(msr);
}

/* ============================================================================
 * Branch Encoding
 * ========================================================================= */

uint32_t dh_make_branch_imm(uint32_t from, uint32_t to, int link) {
    int32_t offset = (int32_t)to - (int32_t)from;
    
    /* Check if within ±32MB range */
    if (offset < -0x2000000 || offset > 0x1FFFFFF) {
        return 0; /* Out of range */
    }
    
    /* Encode: opcode[6] | offset[24] | AA[1] | LK[1] */
    uint32_t insn = 0x48000000;          /* b/bl opcode */
    insn |= (offset & 0x03FFFFFC);       /* 24-bit signed offset (word-aligned) */
    if (link) insn |= 1;                 /* Set LK bit */
    
    return insn;
}

void dh_write_branch_abs(void* at, void* to, int link) {
    uint32_t addr = (uint32_t)to;
    uint32_t* p = (uint32_t*)at;
    uint32_t msr = dh_suspend_interrupts();
    
    /* lis r12, hi16(addr) */
    p[0] = 0x3D800000 | (addr >> 16);
    
    /* ori r12, r12, lo16(addr) */
    p[1] = 0x618C0000 | (addr & 0xFFFF);
    
    /* mtctr r12 */
    p[2] = 0x7D8903A6;
    
    /* bctr (or bctrl if link requested, though typically not used) */
    p[3] = link ? 0x4E800421 : 0x4E800420;
    
    dh_icache_sync_range(at, 16);
    dh_restore_interrupts(msr);
}

/* ============================================================================
 * Trampoline Management
 * ========================================================================= */

void* dh_make_trampoline(void* target, uint32_t stolen_len) {
    /* Align allocation to 16 bytes */
    uint32_t aligned_offset = (g_trampoline_offset + 15) & ~15;
    uint32_t needed = stolen_len + 16; /* stolen code + jump back */
    
    if (aligned_offset + needed > TRAMPOLINE_POOL_SIZE) {
        return NULL; /* Out of trampoline memory */
    }
    
    uint8_t* trampoline = &g_trampoline_pool[aligned_offset];
    g_trampoline_offset = aligned_offset + needed;
    
    /* Copy stolen bytes */
    memcpy(trampoline, target, stolen_len);
    
    /* Append jump back to (target + stolen_len) */
    uint32_t return_addr = (uint32_t)target + stolen_len;
    uint32_t branch_at = (uint32_t)(trampoline + stolen_len);
    uint32_t branch_insn = dh_make_branch_imm(branch_at, return_addr, 0);
    
    if (branch_insn != 0) {
        /* Near branch works */
        *(uint32_t*)(trampoline + stolen_len) = branch_insn;
    } else {
        /* Need absolute branch */
        dh_write_branch_abs(trampoline + stolen_len, (void*)return_addr, 0);
    }
    
    /* Sync cache for trampoline */
    dh_icache_sync_range(trampoline, needed);
    
    return trampoline;
}

/* ============================================================================
 * Function Hooking
 * ========================================================================= */

int dh_hook_install(dh_hook* h) {
    if (!h || !h->target || !h->replacement) {
        return -1;
    }
    
    /* Check if target and replacement are within ±32MB */
    uint32_t from = (uint32_t)h->target;
    uint32_t to = (uint32_t)h->replacement;
    int32_t offset = (int32_t)to - (int32_t)from;
    
    /* Determine patch strategy */
    int use_near = (offset >= -0x2000000 && offset <= 0x1FFFFFF);
    uint32_t patch_len = use_near ? 4 : 16; /* 4 for bl, 16 for abs (12) + padding */
    uint32_t stolen_len = patch_len;
    
    /* Save original bytes */
    memcpy(h->saved, h->target, 16);
    h->patch_len = patch_len;
    
    /* Create trampoline */
    h->trampoline = dh_make_trampoline(h->target, stolen_len);
    if (!h->trampoline) {
        return -1; /* Allocation failed */
    }
    
    /* Install hook */
    uint32_t msr = dh_suspend_interrupts();
    
    if (use_near) {
        /* Write branch immediate */
        uint32_t branch = dh_make_branch_imm(from, to, 0);
        *(uint32_t*)h->target = branch;
        dh_icache_sync_range(h->target, 4);
    } else {
        /* Write absolute branch sequence */
        dh_write_branch_abs(h->target, h->replacement, 0);
    }
    
    dh_restore_interrupts(msr);
    
    return 0;
}

int dh_hook_remove(dh_hook* h) {
    if (!h || !h->target) {
        return -1;
    }
    
    /* Restore original bytes */
    uint32_t msr = dh_suspend_interrupts();
    memcpy(h->target, h->saved, h->patch_len);
    dh_icache_sync_range(h->target, h->patch_len);
    dh_restore_interrupts(msr);
    
    /* Note: We don't free trampoline (static pool) */
    h->trampoline = NULL;
    
    return 0;
}

/* ============================================================================
 * Logging
 * ========================================================================= */

void dh_log(const char* fmt, ...) {
    if (OSReport) {
        va_list args;
        va_start(args, fmt);
        /* OSReport doesn't have vprintf variant, format to buffer */
        char buf[256];
        vsnprintf(buf, sizeof(buf), fmt, args);
        OSReport("%s", buf);
        va_end(args);
    }
    /* Otherwise silent */
}

/* ============================================================================
 * Initialization
 * ========================================================================= */

/* Weak symbol - user must implement */
void dh_install_all_hooks(void) __attribute__((weak));

void dh_init(void) {
    if (g_initialized) {
        return; /* Already initialized */
    }
    g_initialized = 1;
    
    /* Print banner */
#ifndef DOLHOOK_NO_BANNER
    dh_banner();
#endif
    
    /* Install user hooks */
    if (dh_install_all_hooks) {
        dh_install_all_hooks();
    }
}