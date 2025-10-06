/**
 * DolHook Banner Display
 * Complete VI (Video Interface) + XFB (External Frame Buffer) implementation
 * Implements full hardware initialization and YUV framebuffer rendering
 */

#include "dolhook.h"
#include <string.h>

extern void OSReport(const char* fmt, ...) __attribute__((weak));

#ifndef DOLHOOK_NO_BANNER

/* ============================================================================
 * VI Hardware Registers (0xCC002000 base)
 * ========================================================================= */

#define VI_BASE 0xCC002000

#define VI_VTR          (*(volatile uint16_t*)(VI_BASE + 0x00))
#define VI_DCR          (*(volatile uint16_t*)(VI_BASE + 0x02))
#define VI_HTR0         (*(volatile uint32_t*)(VI_BASE + 0x04))
#define VI_HTR1         (*(volatile uint32_t*)(VI_BASE + 0x08))
#define VI_VTO          (*(volatile uint32_t*)(VI_BASE + 0x0C))
#define VI_VTE          (*(volatile uint32_t*)(VI_BASE + 0x10))
#define VI_BBOI         (*(volatile uint32_t*)(VI_BASE + 0x14))
#define VI_BBEI         (*(volatile uint32_t*)(VI_BASE + 0x18))
#define VI_TFBL         (*(volatile uint32_t*)(VI_BASE + 0x1C))
#define VI_TFBR         (*(volatile uint32_t*)(VI_BASE + 0x20))
#define VI_BFBL         (*(volatile uint32_t*)(VI_BASE + 0x24))
#define VI_BFBR         (*(volatile uint32_t*)(VI_BASE + 0x28))
#define VI_DPV          (*(volatile uint16_t*)(VI_BASE + 0x2C))
#define VI_DPH          (*(volatile uint16_t*)(VI_BASE + 0x2E))
#define VI_DI0          (*(volatile uint32_t*)(VI_BASE + 0x30))
#define VI_DI1          (*(volatile uint32_t*)(VI_BASE + 0x34))
#define VI_DI2          (*(volatile uint32_t*)(VI_BASE + 0x38))
#define VI_DI3          (*(volatile uint32_t*)(VI_BASE + 0x3C))
#define VI_DL0          (*(volatile uint32_t*)(VI_BASE + 0x40))
#define VI_DL1          (*(volatile uint32_t*)(VI_BASE + 0x44))
#define VI_HSW          (*(volatile uint16_t*)(VI_BASE + 0x48))
#define VI_HSR          (*(volatile uint16_t*)(VI_BASE + 0x4A))
#define VI_FCT0         (*(volatile uint32_t*)(VI_BASE + 0x4C))
#define VI_FCT1         (*(volatile uint32_t*)(VI_BASE + 0x50))
#define VI_FCT2         (*(volatile uint32_t*)(VI_BASE + 0x54))
#define VI_FCT3         (*(volatile uint32_t*)(VI_BASE + 0x58))
#define VI_FCT4         (*(volatile uint32_t*)(VI_BASE + 0x5C))
#define VI_FCT5         (*(volatile uint32_t*)(VI_BASE + 0x60))
#define VI_FCT6         (*(volatile uint32_t*)(VI_BASE + 0x64))
#define VI_AA           (*(volatile uint16_t*)(VI_BASE + 0x68))
#define VI_VICLK        (*(volatile uint16_t*)(VI_BASE + 0x6C))
#define VI_VISEL        (*(volatile uint16_t*)(VI_BASE + 0x6E))
#define VI_HBE          (*(volatile uint16_t*)(VI_BASE + 0x70))
#define VI_HBS          (*(volatile uint16_t*)(VI_BASE + 0x72))

/* XFB dimensions */
#define XFB_WIDTH       640
#define XFB_HEIGHT_NTSC 480
#define XFB_HEIGHT_PAL  574
#define XFB_STRIDE      (XFB_WIDTH * 2)

/* Static framebuffer - must be 32-byte aligned for cache operations */
static uint8_t g_xfb[XFB_WIDTH * XFB_HEIGHT_NTSC * 2] __attribute__((aligned(32)));
static int g_vi_initialized = 0;

/* Video mode constants */
#define VI_NTSC 0
#define VI_PAL  1

/* ============================================================================
 * 8x8 Bitmap Font (ASCII 32-126)
 * ========================================================================= */

static const uint8_t font_8x8[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* ' ' */
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, /* '!' */
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, /* '"' */
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, /* '#' */
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, /* '$' */
    {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00}, /* '%' */
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, /* '&' */
    {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00}, /* ''' */
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00}, /* '(' */
    {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00}, /* ')' */
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, /* '*' */
    {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00}, /* '+' */
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06}, /* ',' */
    {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00}, /* '-' */
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}, /* '.' */
    {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00}, /* '/' */
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, /* '0' */
    {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00}, /* '1' */
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00}, /* '2' */
    {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00}, /* '3' */
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00}, /* '4' */
    {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00}, /* '5' */
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00}, /* '6' */
    {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00}, /* '7' */
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00}, /* '8' */
    {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00}, /* '9' */
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00}, /* ':' */
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06}, /* ';' */
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00}, /* '<' */
    {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00}, /* '=' */
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, /* '>' */
    {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00}, /* '?' */
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, /* '@' */
    {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00}, /* 'A' */
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, /* 'B' */
    {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00}, /* 'C' */
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00}, /* 'D' */
    {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00}, /* 'E' */
    {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00}, /* 'F' */
    {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00}, /* 'G' */
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00}, /* 'H' */
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, /* 'I' */
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, /* 'J' */
    {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00}, /* 'K' */
    {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00}, /* 'L' */
    {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00}, /* 'M' */
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, /* 'N' */
    {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00}, /* 'O' */
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00}, /* 'P' */
    {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00}, /* 'Q' */
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00}, /* 'R' */
    {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00}, /* 'S' */
    {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, /* 'T' */
    {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00}, /* 'U' */
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00}, /* 'V' */
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, /* 'W' */
    {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00}, /* 'X' */
    {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00}, /* 'Y' */
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00}, /* 'Z' */
    {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00}, /* '[' */
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, /* '\' */
    {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00}, /* ']' */
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, /* '^' */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, /* '_' */
    {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, /* '`' */
    {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00}, /* 'a' */
    {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00}, /* 'b' */
    {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00}, /* 'c' */
    {0x38,0x30,0x30,0x3E,0x33,0x33,0x6E,0x00}, /* 'd' */
    {0x00,0x00,0x1E,0x33,0x3F,0x03,0x1E,0x00}, /* 'e' */
    {0x1C,0x36,0x06,0x0F,0x06,0x06,0x0F,0x00}, /* 'f' */
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F}, /* 'g' */
    {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00}, /* 'h' */
    {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00}, /* 'i' */
    {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E}, /* 'j' */
    {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00}, /* 'k' */
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, /* 'l' */
    {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00}, /* 'm' */
    {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00}, /* 'n' */
    {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00}, /* 'o' */
    {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F}, /* 'p' */
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78}, /* 'q' */
    {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00}, /* 'r' */
    {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00}, /* 's' */
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00}, /* 't' */
    {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00}, /* 'u' */
    {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00}, /* 'v' */
    {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, /* 'w' */
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00}, /* 'x' */
    {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F}, /* 'y' */
    {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00}, /* 'z' */
    {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00}, /* '{' */
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, /* '|' */
    {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00}, /* '}' */
    {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00}, /* '~' */
};

/* ============================================================================
 * VI Timing Configuration
 * ========================================================================= */

typedef struct {
    uint16_t vtr;
    uint16_t dcr;
    uint32_t htr0;
    uint32_t htr1;
    uint32_t vto;
    uint32_t vte;
    uint32_t bboi;
    uint32_t bbei;
    uint16_t dpv;
    uint16_t dph;
    uint16_t hsw;
    uint16_t hsr;
    uint16_t hbe640;
    uint16_t hbs640;
    uint16_t width;
    uint16_t height;
} vi_timing_config;

/* NTSC 480i configuration */
static const vi_timing_config ntsc_480i_config = {
    .vtr = 0x0F06,
    .dcr = 0x01F0,
    .htr0 = 0x01AD0150,
    .htr1 = 0x00C3012C,
    .vto = 0x00060030,
    .vte = 0x00060030,
    .bboi = 0x005B0122,
    .bbei = 0x005B0122,
    .dpv = 0x0018,
    .dph = 0x0028,
    .hsw = 640,
    .hsr = 0x0280,
    .hbe640 = 0x00C7,
    .hbs640 = 0x0027,
    .width = 640,
    .height = 480
};

/* PAL 576i configuration */
static const vi_timing_config pal_576i_config = {
    .vtr = 0x1106,
    .dcr = 0x01F0,
    .htr0 = 0x01AD01B4,
    .htr1 = 0x00C5014C,
    .vto = 0x00120038,
    .vte = 0x00120038,
    .bboi = 0x005B0142,
    .bbei = 0x005B0142,
    .dpv = 0x0023,
    .dph = 0x0028,
    .hsw = 640,
    .hsr = 0x0280,
    .hbe640 = 0x00D7,
    .hbs640 = 0x0027,
    .width = 640,
    .height = 574
};

/* ============================================================================
 * Video Mode Detection
 * ========================================================================= */

static int detect_video_mode(void) {
    uint16_t vtr = VI_VTR;
    
    if ((vtr & 0xFF00) >= 0x1100) {
        return VI_PAL;
    }
    
    return VI_NTSC;
}

/* ============================================================================
 * VI Hardware Configuration
 * ========================================================================= */

static void vi_configure_hardware(const vi_timing_config* cfg) {
    /* Disable display */
    VI_DCR = 0x0000;
    
    /* Configure timing */
    VI_VTR = cfg->vtr;
    VI_HTR0 = cfg->htr0;
    VI_HTR1 = cfg->htr1;
    VI_VTO = cfg->vto;
    VI_VTE = cfg->vte;
    VI_BBOI = cfg->bboi;
    VI_BBEI = cfg->bbei;
    
    /* Set framebuffer addresses */
    uint32_t xfb_phys = ((uint32_t)g_xfb) & 0x3FFFFFFF;
    VI_TFBL = xfb_phys;
    VI_TFBR = xfb_phys;
    VI_BFBL = xfb_phys;
    VI_BFBR = xfb_phys;
    
    /* Configure display position */
    VI_DPV = cfg->dpv;
    VI_DPH = cfg->dph;
    
    /* Configure horizontal scaling */
    VI_HSW = cfg->hsw;
    VI_HSR = cfg->hsr;
    VI_HBE = cfg->hbe640;
    VI_HBS = cfg->hbs640;
    
    /* Configure anti-aliasing filter */
    VI_FCT0 = 0x00000000;
    VI_FCT1 = 0x00000000;
    VI_FCT2 = 0x01000000;
    VI_FCT3 = 0x00000000;
    VI_FCT4 = 0x00000000;
    VI_FCT5 = 0x00000000;
    VI_FCT6 = 0x00000000;
    
    VI_AA = 0x0000;
    VI_VICLK = 0x0000;
    VI_VISEL = 0x0001;
    
    /* Clear interrupts */
    VI_DI0 = 0x00000000;
    VI_DI1 = 0x00000000;
    VI_DI2 = 0x00000000;
    VI_DI3 = 0x00000000;
    
    /* Enable display */
    VI_DCR = cfg->dcr;
    
    /* Flush XFB cache */
    asm volatile(
        "lis 3, g_xfb@ha\n"
        "addi 3, 3, g_xfb@l\n"
        "li 4, %0\n"
        "1:\n"
        "dcbf 0, 3\n"
        "addi 3, 3, 32\n"
        "addic. 4, 4, -32\n"
        "bgt 1b\n"
        "sync\n"
        : : "i"(sizeof(g_xfb)) : "r3", "r4", "memory"
    );
}

/* ============================================================================
 * XFB Management
 * ========================================================================= */

static void xfb_clear(void) {
    uint32_t* xfb32 = (uint32_t*)g_xfb;
    uint32_t black_yuv = 0x10801080;
    
    size_t words = (XFB_WIDTH * XFB_HEIGHT_NTSC * 2) / 4;
    for (size_t i = 0; i < words; i++) {
        xfb32[i] = black_yuv;
    }
}

static void xfb_put_pixel(int x, int y, uint8_t Y, uint8_t U, uint8_t V) {
    if (x < 0 || x >= XFB_WIDTH || y < 0 || y >= XFB_HEIGHT_NTSC) {
        return;
    }
    
    int offset = (y * XFB_WIDTH + (x & ~1)) * 2;
    
    if (x & 1) {
        g_xfb[offset + 2] = Y;
    } else {
        g_xfb[offset + 0] = Y;
        g_xfb[offset + 1] = U;
        g_xfb[offset + 3] = V;
    }
}

static void xfb_fill_rect(int x, int y, int w, int h, uint8_t Y, uint8_t U, uint8_t V) {
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            xfb_put_pixel(x + col, y + row, Y, U, V);
        }
    }
}

static void xfb_flush_region(int x, int y, int w, int h) {
    int start_line = y;
    int end_line = y + h;
    
    for (int line = start_line; line < end_line; line++) {
        uint32_t addr = (uint32_t)&g_xfb[line * XFB_STRIDE];
        asm volatile("dcbf 0, %0" : : "r"(addr) : "memory");
    }
    
    asm volatile("sync" : : : "memory");
}

static void xfb_flush_all(void) {
    uint8_t* addr = g_xfb;
    size_t size = sizeof(g_xfb);
    
    for (size_t i = 0; i < size; i += 32) {
        asm volatile("dcbf 0, %0" : : "r"(addr + i) : "memory");
    }
    
    asm volatile("sync" : : : "memory");
}

/* ============================================================================
 * Text Rendering
 * ========================================================================= */

static void draw_char(int x, int y, char c, uint8_t Y, uint8_t U, uint8_t V) {
    if (c < 32 || c > 126) {
        return;
    }
    
    const uint8_t* glyph = font_8x8[c - 32];
    
    for (int row = 0; row < 8; row++) {
        uint8_t line = glyph[row];
        
        for (int col = 0; col < 8; col++) {
            uint8_t bit = (line >> (7 - col)) & 1;
            
            if (bit) {
                xfb_put_pixel(x + col, y + row, Y, U, V);
            }
        }
    }
}

static void draw_text(int x, int y, const char* text, uint8_t Y, uint8_t U, uint8_t V) {
    int cursor_x = x;
    int cursor_y = y;
    
    while (*text) {
        if (*text == '\n') {
            cursor_x = x;
            cursor_y += 8;
        } else if (*text == '\r') {
            cursor_x = x;
        } else {
            draw_char(cursor_x, cursor_y, *text, Y, U, V);
            cursor_x += 8;
            
            if (cursor_x >= XFB_WIDTH - 8) {
                cursor_x = x;
                cursor_y += 8;
            }
        }
        text++;
    }
}

static void draw_text_shadowed(int x, int y, const char* text) {
    draw_text(x + 1, y + 1, text, 16, 128, 128);
    draw_text(x, y, text, 235, 128, 128);
}

/* ============================================================================
 * VI/XFB Initialization
 * ========================================================================= */

static void init_vi_and_xfb(void) {
    int mode = detect_video_mode();
    const vi_timing_config* cfg = (mode == VI_PAL) ? &pal_576i_config : &ntsc_480i_config;
    
    xfb_clear();
    vi_configure_hardware(cfg);
    
    /* Wait for VI to stabilize */
    for (volatile int i = 0; i < 100000; i++) {
        asm volatile("nop");
    }
}

/* ============================================================================
 * Public Banner Function
 * ========================================================================= */

void dh_banner(void) {
    /* Try OSReport first */
    if (OSReport) {
        OSReport("Patched with DolHook\n");
        return;
    }
    
    /* Full VI/XFB initialization */
    if (!g_vi_initialized) {
        init_vi_and_xfb();
        g_vi_initialized = 1;
    }
    
    /* Draw banner text with shadow */
    draw_text_shadowed(16, 16, "Patched with DolHook");
    
    /* Flush affected region */
    xfb_flush_region(16, 16, 160, 16);
    
    /* Draw indicator box in corner */
    xfb_fill_rect(XFB_WIDTH - 20, 4, 16, 8, 235, 128, 128);
    xfb_flush_region(XFB_WIDTH - 20, 4, 16, 8);
}

/* ============================================================================
 * Additional VI/XFB Utilities
 * ========================================================================= */

void* dh_get_xfb(void) {
    return g_xfb;
}

void dh_get_xfb_size(int* width, int* height) {
    if (width) *width = XFB_WIDTH;
    if (height) *height = XFB_HEIGHT_NTSC;
}

void dh_draw_text(int x, int y, const char* text) {
    if (!g_vi_initialized) {
        init_vi_and_xfb();
        g_vi_initialized = 1;
    }
    
    draw_text_shadowed(x, y, text);
    
    int len = 0;
    while (text[len]) len++;
    xfb_flush_region(x, y, len * 8 + 2, 10);
}

void dh_clear_screen(void) {
    if (!g_vi_initialized) {
        init_vi_and_xfb();
        g_vi_initialized = 1;
    }
    
    xfb_clear();
    xfb_flush_all();
}

void dh_draw_box(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    /* RGB to YUV conversion (BT.601) */
    int Y = ((77 * r + 150 * g + 29 * b) >> 8) + 16;
    int U = ((-43 * r - 85 * g + 128 * b) >> 8) + 128;
    int V = ((128 * r - 107 * g - 21 * b) >> 8) + 128;
    
    /* Clamp to valid ranges */
    if (Y < 16) Y = 16;
    if (Y > 235) Y = 235;
    if (U < 16) U = 16;
    if (U > 240) U = 240;
    if (V < 16) V = 16;
    if (V > 240) V = 240;
    
    xfb_fill_rect(x, y, w, h, Y, U, V);
    xfb_flush_region(x, y, w, h);
}

const void* dh_capture_xfb(void) {
    return g_xfb;
}

#endif /* DOLHOOK_NO_BANNER */
