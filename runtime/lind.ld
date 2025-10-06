/**
 * DolHook Linker Script
 * 
 * Creates position-tolerant payload for injection into DOL.
 * Sections will be placed at addresses chosen by patcher.
 */

OUTPUT_FORMAT("elf32-powerpc", "elf32-powerpc", "elf32-powerpc")
OUTPUT_ARCH(powerpc:common)

/* Default base - patcher may relocate */
. = 0x80400000;

SECTIONS
{
    /* Entry stub and code */
    .text : {
        *(.text.entry)
        *(.text .text.*)
        . = ALIGN(32);
    }
    
    /* Read-only data */
    .rodata : {
        *(.rodata .rodata.*)
        . = ALIGN(4);
    }
    
    /* Initialized data */
    .data : {
        *(.data.entry)
        *(.data .data.*)
        . = ALIGN(4);
    }
    
    /* Zero-initialized data */
    .bss : {
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(32);
    }
    
    /* Discard debug and unneeded sections */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.ARM.*)
    }
}

/* Export key symbols for patcher */
PROVIDE(__dolhook_start = ADDR(.text));
PROVIDE(__dolhook_end = ADDR(.bss) + SIZEOF(.bss));