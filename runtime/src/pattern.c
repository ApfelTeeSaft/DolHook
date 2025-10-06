/**
 * DolHook Pattern Scanning
 * Find byte patterns in memory with wildcard support
 */

#include "dolhook.h"

#ifndef DOLHOOK_NO_PATTERN

void* dh_find_pattern(const void* start, size_t size,
                      const char* pat, const char* mask) {
    const uint8_t* mem = (const uint8_t*)start;
    size_t pat_len = 0;
    
    /* Calculate pattern length from mask */
    while (mask[pat_len]) pat_len++;
    
    if (pat_len == 0 || size < pat_len) {
        return NULL;
    }
    
    /* Scan memory */
    for (size_t i = 0; i <= size - pat_len; i++) {
        int match = 1;
        
        for (size_t j = 0; j < pat_len; j++) {
            if (mask[j] == 'x' && mem[i + j] != (uint8_t)pat[j]) {
                match = 0;
                break;
            }
            /* '?' means wildcard - always matches */
        }
        
        if (match) {
            return (void*)(mem + i);
        }
    }
    
    return NULL; /* Not found */
}

#endif /* DOLHOOK_NO_PATTERN */