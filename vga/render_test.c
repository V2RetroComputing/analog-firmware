#include <string.h>
#include <pico/stdlib.h>
#include <hardware/timer.h>
#include "common/config.h"
#include "vga/vgabuf.h"
#include "vga/render.h"
#include "vga/vgaout.h"
#include "vga/logo.h"
#include "common/build.h"

#include <pico/unique_id.h>

#define _PIXPAIR(p1, p2) ((uint32_t)(p1) | (((uint32_t)p2) << 16))

char __attribute__((section(".uninitialized_data."))) error_message[40*24+1];

void DELAYED_COPY_CODE(render_test_print)(int y, int x, char *str) {
    memcpy(error_message + (y*40) +  x, str, strlen(str));
}

void DELAYED_COPY_CODE(render_test_cprint)(int y, char *str) {
    int x = 20 - ((strlen(str)+1)/2);
    memcpy(error_message + (y*40) +  x, str, strlen(str));
}

void DELAYED_COPY_CODE(render_test_init)() {
    int y = 0;
    memset(error_message, ' ', sizeof(error_message)-1);
    error_message[sizeof(error_message)-1] = 0;
    y+=2;
    render_test_cprint(y++, "V2 Retro Computing");
    render_test_cprint(y++, "Analog VGA");
    y++;
    render_test_cprint(y++, "Copyright (C) 2022-2023");
    render_test_cprint(y++, "David Kuder");
    y++;
    render_test_cprint(y++, "based on");
    render_test_cprint(y++, "AppleII-VGA by Mark Aikens");
    y++;
    render_test_cprint(y++, "no Apple II bus activity");
    y++;
    render_test_cprint(y++, "Turn off power & check");
    render_test_cprint(y++, "for proper card insertion.");
    y++;
    render_test_cprint(y++, "FIRMWARE: " BUILDSTR);
    render_test_cprint(y++, "HARDWARE: " HWSTRING);
    render_test_print(y, 4, "SERIALNO: ");
    // Get Pico's Flash Serial Number (Board ID) and terminating null.
    pico_get_unique_board_id_string(error_message + (y*40)+20, 17);
    error_message[(y*40)+36] = ' ';
}

// Clear the error message, in case the user sets 0x20 in terminal switches
// to show the about screen.
void DELAYED_COPY_CODE(render_about_init)() {
    memset(error_message + 11*40, ' ', 160);
}

static inline uint_fast8_t char_test_bits(uint_fast8_t ch, uint_fast8_t glyph_line) {
    return terminal_character_rom[((uint_fast16_t)(ch & 0x7f) << 4) | glyph_line];
}

void DELAYED_COPY_CODE(render_testpattern)() {
    uint16_t color1, color2, i;
    vga_prepare_frame();

    for(uint line=0; line < VGA_HEIGHT;) {
        struct vga_scanline *sl = vga_prepare_scanline();
        uint sl_pos = 0;
        sl->repeat_count = 0;

        if(line >= VGA_HEIGHT) {
            sl->data[sl_pos++] = _PIXPAIR(_RGB(0, 0, 0), _RGB(0, 0, 0));

            sl->length = sl_pos;
        } else if((line <= 1) || (line >= VGA_HEIGHT-2)) {
            // Solid White Top & Bottom
            for(i = 0; i < VGA_WIDTH; i+=16) {
                sl->data[sl_pos++] = _PIXPAIR(_RGB(255, 255, 255) | THEN_EXTEND_7, _RGB(255, 255, 255) | THEN_EXTEND_7);
            }
            sl->repeat_count = 1;
            sl->length = sl_pos;
        } else if((line > 40) && (line < VGA_HEIGHT-40)) {
            const uint16_t double_line = ((line-40) / 2);

            // Screen Edge
            sl->data[sl_pos++] = _PIXPAIR(_RGB(255, 255, 255) | THEN_EXTEND_1, _RGB(0, 0, 0) | THEN_EXTEND_1);
            sl->data[sl_pos++] = _PIXPAIR(_RGB(0, 0, 0) | THEN_EXTEND_1, _RGB(0, 0, 0) | THEN_EXTEND_1);

            // Logo 150x200
            for(i=0; i < 75; i++) {
                color1 = lores_palette[PicoPalLogo[(double_line*75) + i] >> 4];
                color2 = lores_palette[PicoPalLogo[(double_line*75) + i] & 0xf];
                sl->data[sl_pos++] = _PIXPAIR(color1 | THEN_EXTEND_1, color2 | THEN_EXTEND_1);
            }

            // 4 Black pixels
            sl->data[sl_pos++] = _PIXPAIR(_RGB(0, 0, 0) | THEN_EXTEND_1, _RGB(0, 0, 0) | THEN_EXTEND_1);

            const uint text_line = (double_line / 10);
            const uint glyph_line = (double_line % 10);
            for(i=0; i < 40; i++) {
                uint32_t bits = char_test_bits(error_message[text_line * 40 + i], glyph_line);
                for(uint j=0; j < 4; j++) {
                    uint32_t pixeldata = (bits & 0x80) ? (_RGB(255, 255, 255)) : (_RGB(0, 0, 0));
                    pixeldata |= (bits & 0x40) ?
                        ((uint32_t)_RGB(255, 255, 255)) << 16 :
                        ((uint32_t)_RGB(0, 0, 0)) << 16;
                    bits <<= 2;

                    sl->data[sl_pos++] = pixeldata;
                }
            }

            // Screen Edge
            sl->data[sl_pos++] = _PIXPAIR(_RGB(0, 0, 0) | THEN_EXTEND_1, _RGB(0, 0, 0) | THEN_EXTEND_1);
            sl->data[sl_pos++] = _PIXPAIR(_RGB(0, 0, 0) | THEN_EXTEND_1, _RGB(255, 255, 255) | THEN_EXTEND_1);

            sl->repeat_count = 1;
            sl->length = sl_pos;
        } else {
            // Screen Edge
            sl->data[sl_pos++] = _PIXPAIR(_RGB(255, 255, 255) | THEN_EXTEND_1, _RGB(0, 0, 0) | THEN_EXTEND_1);
            sl->data[sl_pos++] = _PIXPAIR(_RGB(0, 0, 0) | THEN_EXTEND_1, _RGB(0, 0, 0) | THEN_EXTEND_1);

            for(i = 0; i < VGA_WIDTH-16; i+=16) {
                sl->data[sl_pos++] = _PIXPAIR(_RGB(0, 0, 0) | THEN_EXTEND_7, _RGB(0, 0, 0) | THEN_EXTEND_7);
            }

            // Screen Edge
            sl->data[sl_pos++] = _PIXPAIR(_RGB(0, 0, 0) | THEN_EXTEND_1, _RGB(0, 0, 0) | THEN_EXTEND_1);
            sl->data[sl_pos++] = _PIXPAIR(_RGB(0, 0, 0) | THEN_EXTEND_1, _RGB(255, 255, 255) | THEN_EXTEND_1);

            sl->length = sl_pos;
        }

        line += sl->repeat_count + 1;
        vga_submit_scanline(sl);
    }
}

void DELAYED_COPY_CODE(render_status_line)() {
    for(uint glyph_line=0; glyph_line < 8; glyph_line++) {
        struct vga_scanline *sl = vga_prepare_scanline();
        uint8_t *line_buf = status_line;
        uint32_t bits;
        uint sl_pos = 0;

        for(uint col=0; col < 80; col++) {
            // Grab 8 pixels from the next character
            if(*line_buf != 0) {
                bits |= char_test_bits(line_buf[col], glyph_line);
            }

            // Translate each pair of bits into a pair of pixels
            for(int i=0; i < 4; i++) {
                uint32_t pixeldata = (bits & 0x80) ? (_RGB(255, 255, 255)) : (_RGB(0, 0, 0));
                pixeldata |= (bits & 0x40) ?
                    ((uint32_t)_RGB(255, 255, 255)) << 16 :
                    ((uint32_t)_RGB(0, 0, 0)) << 16;
                bits <<= 2;

                sl->data[sl_pos++] = pixeldata;
            }
        }

        sl->length = sl_pos;
        sl->repeat_count = 1;
        vga_submit_scanline(sl);
    }
}
