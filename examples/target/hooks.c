/**
 * Example DolHook Usage
 * Demonstrates hooking functions in a GameCube game
 */

#include "dolhook.h"

/* Example: Hook OSReport to prepend "[DolHook] " */
static dh_hook g_osreport_hook;

/* Saved original OSReport signature */
typedef void (*OSReportFunc)(const char* fmt, ...);

static void my_osreport(const char* fmt, ...) {
    /* Get original function from trampoline */
    OSReportFunc original = (OSReportFunc)g_osreport_hook.trampoline;
    
    /* Prepend our tag */
    original("[DolHook] ");
    
    /* Call with original format and args */
    /* Note: Proper implementation would use va_list forwarding */
    original("%s", fmt);
}

/* Example: Hook a game function by pattern */
static dh_hook g_game_func_hook;

static int my_game_function(int x, int y) {
    /* Call original */
    typedef int (*GameFunc)(int, int);
    GameFunc original = (GameFunc)g_game_func_hook.trampoline;
    
    int result = original(x, y);
    
    /* Log the call */
    dh_log("Game function called: %d + %d = %d\n", x, y, result);
    
    /* Modify result (optional) */
    return result * 2;
}

/* Example: Find and hook a function by pattern */
void dh_install_all_hooks(void) {
    dh_log("Installing hooks...\n");
    
#ifndef DOLHOOK_NO_PATTERN
    /* Example: Find OSReport by pattern
     * OSReport typically starts with: stwu r1, -X(r1); mflr r0
     * Pattern: 94 21 ?? ?? 7C 08 02 A6
     */
    const char pattern[] = {0x94, 0x21, 0x00, 0x00, 0x7C, 0x08, 0x02, 0xA6};
    const char mask[] = "xx??xxxx";
    
    void* osreport = dh_find_pattern(
        (void*)0x80003000,  /* Search in typical OS area */
        0x100000,           /* Search size */
        pattern,
        mask
    );
    
    if (osreport) {
        dh_log("Found OSReport at: 0x%08X\n", (unsigned int)osreport);
        
        g_osreport_hook.target = osreport;
        g_osreport_hook.replacement = my_osreport;
        
        if (dh_hook_install(&g_osreport_hook) == 0) {
            dh_log("OSReport hook installed!\n");
        } else {
            dh_log("Failed to hook OSReport\n");
        }
    }
#endif
    
    /* Example: Hook a known game function address */
    void* game_func = (void*)0x80123456; /* Replace with actual address */
    
    if (game_func != (void*)0x80123456) { /* Check if real address set */
        g_game_func_hook.target = game_func;
        g_game_func_hook.replacement = my_game_function;
        
        if (dh_hook_install(&g_game_func_hook) == 0) {
            dh_log("Game function hook installed!\n");
        }
    }
    
    dh_log("Hook installation complete\n");
}

/* Example: Uninstall hooks (call before game exit if needed) */
void remove_all_hooks(void) {
    dh_hook_remove(&g_osreport_hook);
    dh_hook_remove(&g_game_func_hook);
}