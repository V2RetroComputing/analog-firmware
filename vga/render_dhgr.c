#include <pico/stdlib.h>
#include "hires_color_patterns.h"
#include "hires_dot_patterns.h"
#include "common/config.h"
#include "vga/vgabuf.h"
#include "vga/render.h"
#include "vga/vgaout.h"

static void render_dhgr_line(bool p2, uint line);

uint16_t DELAYED_COPY_DATA(dhgr_palette)[16] = {
    RGB_BLACK,    RGB_DBLUE,    RGB_DGREEN,    RGB_HBLUE,
    RGB_BROWN,    RGB_LGRAY,    RGB_HGREEN,    RGB_AQUA,
    RGB_MAGENTA,  RGB_HVIOLET,  RGB_DGRAY,     RGB_LBLUE,
    RGB_HORANGE,  RGB_PINK,     RGB_YELLOW,    RGB_WHITE
};

//#define PAGE2SEL (!(soft_switches & SOFTSW_80STORE) && (soft_switches & SOFTSW_PAGE_2))
#define PAGE2SEL ((soft_switches & (SOFTSW_80STORE | SOFTSW_PAGE_2)) == SOFTSW_PAGE_2)


static inline uint dhgr_line_to_mem_offset(uint line) {
    return ((line & 0x07) << 10) | ((line & 0x38) << 4) | (((line & 0xc0) >> 6) * 40);
}


void DELAYED_COPY_CODE(render_dhgr)() {
    for(uint line=0; line < 192; line++) {
        render_dhgr_line(PAGE2SEL, line);
    }
}


void DELAYED_COPY_CODE(render_mixed_dhgr)() {
    for(uint line=0; line < 160; line++) {
        render_dhgr_line(PAGE2SEL, line);
    }

    for(uint line=20; line < 24; line++) {
        if(soft_switches & SOFTSW_80COL) {
            render_text80_line(PAGE2SEL, line);
        } else {
            render_text40_line(PAGE2SEL, line);
        }
    }
}


static void DELAYED_COPY_CODE(render_dhgr_line)(bool p2, uint line) {
    struct vga_scanline *sl = vga_prepare_scanline();
    uint sl_pos = 0;
    uint i;

    const uint8_t *line_mema = (const uint8_t *)((p2 ? hgr_p2 : hgr_p1) + dhgr_line_to_mem_offset(line));
    const uint8_t *line_memb = (const uint8_t *)((p2 ? hgr_p4 : hgr_p3) + dhgr_line_to_mem_offset(line));

    // Pad 40 pixels on the left to center horizontally
    sl->data[sl_pos++] = (text_border|THEN_EXTEND_31) | ((text_border|THEN_EXTEND_7) << 16); // 16 pixels per word

    // DHGR is weird. Nuff said.
    uint32_t dots = 0;
    uint32_t pixeldata;
    int j;

    i = 0;
    while(i < 40) {
        // Load in the next 28 subpixels
        dots  = (line_memb[i] & 0x7f) << 0;
        dots |= (line_mema[i] & 0x7f) << 7;
        i++;
        dots |= (line_memb[i] & 0x7f) << 14;
        dots |= (line_mema[i] & 0x7f) << 21;
        i++;

        if((soft_switches & SOFTSW_MONOCHROME) || (mono_palette & 0x8)) {
            // Consume 6 pixels (24 subpixel bits)
            for(j = 0; j < 12; j++) {
                pixeldata = ((dots & 1) ? (text_fore) : (text_back));
                dots >>= 1;
                pixeldata |= (((dots & 1) ? (text_fore) : (text_back))) << 16;
                dots >>= 1;
                sl->data[sl_pos++] = pixeldata;
            }
        } else {
            // Consume 6 pixels (24 subpixel bits)
            for(j = 0; j < 3; j++) {
                pixeldata = (dhgr_palette[dots & 0xf] | THEN_EXTEND_3);
                dots >>= 4;
                pixeldata |= (dhgr_palette[dots & 0xf] | THEN_EXTEND_3) << 16;
                dots >>= 4;
                sl->data[sl_pos++] = pixeldata;
            }
        }

        // 4 subpixels roll over to the next block
        // Load in the next 28 subpixels
        dots |= (line_memb[i] & 0x7f) << 4;
        dots |= (line_mema[i] & 0x7f) << 11;
        i++;
        dots |= (line_memb[i] & 0x7f) << 18;
        dots |= (line_mema[i] & 0x7f) << 25;
        i++;

        if(soft_switches & SOFTSW_MONOCHROME) {
            // Consume 8 pixels (32 subpixel bits)
            for(j = 0; j < 16; j++) {
                pixeldata = ((dots & 1) ? (text_fore) : (text_back));
                dots >>= 1;
                pixeldata |= (((dots & 1) ? (text_fore) : (text_back))) << 16;
                dots >>= 1;
                sl->data[sl_pos++] = pixeldata;
            }
        } else {
            // Consume 8 pixels (32 subpixel bits)
            for(j = 0; j < 4; j++) {
                pixeldata = (dhgr_palette[dots & 0xf] | THEN_EXTEND_3);
                dots >>= 4;
                pixeldata |= (dhgr_palette[dots & 0xf] | THEN_EXTEND_3) << 16;
                dots >>= 4;
                sl->data[sl_pos++] = pixeldata;
            }
        }
    }

    // Pad 40 pixels on the right to center horizontally
    sl->data[sl_pos++] = (text_border|THEN_EXTEND_31) | ((text_border|THEN_EXTEND_7) << 16); // 16 pixels per word

    sl->length = sl_pos;
    sl->repeat_count = 1;
    vga_submit_scanline(sl);
}

