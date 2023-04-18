#pragma once

#include <stdint.h>


#define VGA_WIDTH 640
#define VGA_HEIGHT 480

#ifdef ANALOG_GS
#define THEN_WAIT_VSYNC (2 << 12)
#define THEN_WAIT_HSYNC (3 << 12)
#define THEN_EXTEND_31  (4 << 12)
#define THEN_EXTEND_15  (5 << 12)
#define THEN_EXTEND_13  (6 << 12)
#define THEN_EXTEND_7   (7 << 12)
#define THEN_EXTEND_6   (8 << 12)
#define THEN_EXTEND_3   (9 << 12)
#define THEN_EXTEND_1   (10 << 12)
#else
#define THEN_WAIT_VSYNC (2 << 9)
#define THEN_WAIT_HSYNC (3 << 9)
#define THEN_EXTEND_31  (4 << 9)
#define THEN_EXTEND_15  (5 << 9)
#define THEN_EXTEND_13  (6 << 9)
#define THEN_EXTEND_7   (7 << 9)
#define THEN_EXTEND_6   (8 << 9)
#define THEN_EXTEND_3   (9 << 9)
#define THEN_EXTEND_1   (10 << 9)
#endif

struct vga_scanline {
    // number of 32-bit words in the data array
    uint_fast16_t length;

    // number of times to repeat the scanline
    uint_fast16_t repeat_count;

    volatile uint_fast8_t _flags;

    uint32_t _sync;
    uint32_t data[(VGA_WIDTH/2)+8];
};

extern void vga_prepare_frame();
extern struct vga_scanline *vga_prepare_scanline();
extern void vga_submit_scanline(struct vga_scanline *scanline);

extern void vga_stop();
extern void vga_dpms_sleep();
extern void vga_dpms_wake();

extern void terminal_process_input();
