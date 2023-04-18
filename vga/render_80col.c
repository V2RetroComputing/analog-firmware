#include <string.h>
#include <pico/stdlib.h>
#include "common/config.h"
#include "common/dmacopy.h"

#include "vga/vgabuf.h"
#include "vga/render.h"
#include "vga/vgaout.h"

uint8_t terminal_jsoffset = 0;
uint8_t terminal_ssoffset = 0;
bool terminal_esc = 0;
bool terminal_esc_crz = 0;
int terminal_esc_pos = 0;
uint16_t terminal_charset = 0;
uint8_t terminal_width = 80;
uint8_t terminal_height = 24;

static inline uint_fast8_t char_terminal_bits(uint_fast8_t ch, uint_fast8_t glyph_line) {
    uint_fast8_t bits = terminal_character_rom[terminal_charset | (((uint_fast16_t)ch & 0x7f) << 3) | glyph_line];

    if(ch & 0x80) {
        return (bits & 0x7f) ^ 0x7f;
    }
    return (bits & 0x7f);
}

static void DELAYED_COPY_CODE(terminal_linefeed)() {
    if(terminal_row < (terminal_height-1)) {
        terminal_row++;
    } else {
        // Clear the next text buffer line (using dma if possible)
        memset((void*)(terminal_memory+(((terminal_row+1+terminal_jsoffset) * 128) & 0xFFF)), ' ', 128);

        // Smooth scroll then increment jsoffset
        terminal_ssoffset = 1;
    }
}

static void DELAYED_COPY_CODE(terminal_advance_cursor)() {
    terminal_col++;
    if(terminal_col >= terminal_width) {
        terminal_col = 0;
        terminal_linefeed();
    }
}

void DELAYED_COPY_CODE(terminal_clear_screen)() {
    // Clear screen (using dma)
    memset((void*)terminal_memory, ' ', 4096);
    terminal_jsoffset = 0;
    terminal_row = 0;
    terminal_col = 0;
}

static void DELAYED_COPY_CODE(terminal_clear_to_line_end)() {
    // Clear to end of line first
    memset((void*)(terminal_memory+((((terminal_row+terminal_jsoffset) * 128) + terminal_col) & 0xFFF)), ' ', 128-terminal_col);
}

static void DELAYED_COPY_CODE(terminal_clear_to_screen_end)() {
    // Clear to end of line first
    memset((void*)(terminal_memory+((((terminal_row+terminal_jsoffset) * 128) + terminal_col) & 0xFFF)), ' ', 128-terminal_col);

    // Then clear remaining lines on the screen
    for(uint i = terminal_row; i < terminal_height; i++)
        memset((void*)(terminal_memory+(((i+terminal_jsoffset) * 128) & 0xFFF)), ' ', 128);
}

void DELAYED_COPY_CODE(terminal_process_input)() {
    if(terminal_ssoffset > 0) return;

    if(terminal_fifo_rdptr != terminal_fifo_wrptr) {
        char ch = terminal_fifo[terminal_fifo_rdptr++];
        if(terminal_esc_pos == 1) {
            terminal_col = (ch - 32);
            if(terminal_col >= terminal_width)
                terminal_col = terminal_width - 1;
            terminal_esc_pos = 2;
        } else if(terminal_esc_pos == 2) {
            terminal_row = (ch - 32);
            if(terminal_row >= terminal_height)
                terminal_row = terminal_height - 1;
            terminal_esc_pos = 0;
        } else if(terminal_esc_crz) {
            switch(ch) {
                case '0': // Clear Screen
                    terminal_clear_screen();
                    break;
                case '1':
                    soft_switches &= ~SOFTSW_TERMINAL;
                    break;
                case '2':
                    terminal_charset = 0;
                    break;
                case '3':
                    terminal_charset = 2048;
                    break;
                default:
                    terminal_memory[(((terminal_row+terminal_jsoffset) * 128) + terminal_col) & 0xFFF] = ch;
                    terminal_advance_cursor();
                    break;
            }
            terminal_esc_crz = false;
        } else if(terminal_esc) {
            switch(ch) {
                case 'A':
                    terminal_advance_cursor();
                    break;
                case 'B':
                    if(terminal_col > 0)
                        terminal_col--;
                    break;
                case 'C':
                    terminal_linefeed();
                    break;
                case 'D': // Reverse Linefeed
                    if(terminal_row > 0)
                        terminal_row--;
                    break;
                case 'E': // Clear to end of line
                    terminal_clear_to_line_end();
                    break;
                case 'F': // Clear to end of screen
                    terminal_clear_to_screen_end();
                    break;
                case '@': // Clear screen
                    terminal_clear_screen();
                    break;
                default:
                    terminal_memory[(((terminal_row+terminal_jsoffset) * 128) + terminal_col) & 0xFFF] = ch;
                    terminal_advance_cursor();
                    break;
            }
            terminal_esc = false;
        } else
        switch(ch) {
            case 0x0A: // Line Feed
                terminal_linefeed();
                break;
            case 0x0B: // Ctrl-K: Clear to End of Screen
                terminal_clear_to_screen_end();
                break;
            case 0x0C: // Ctrl-L: Form Feed
                terminal_clear_screen();
                break;
            case 0x0D: // Ctrl-M: Carriage Return
                terminal_col = 0;
                break;
            case 0x13: // Ctrl-S: Xon/Xoff (unimplemented)
                break;
            case 0x15: // Ctrl-U: Copy (unimplemented)
                break;
            case 0x19: // Ctrl-Y: Home Cursor
                terminal_row = 0;
                terminal_col = 0;
                break;
            case 0x1A: // Ctrl-Z
                terminal_esc_crz = true;
                break;
            case 0x1B: // Escape
                terminal_esc = true;
                break;
            case 0x1C: // Forward Space
                terminal_advance_cursor();
                break;
            case 0x1D: // Clear to end of line
                terminal_clear_to_line_end();
                break;
            case 0x1E: // Position Cursor
                terminal_esc_pos = 1;
                break;
            case 0x1F: // Reverse Linefeed
                if(terminal_row > 0)
                    terminal_row--;
                break;
            default:
                terminal_memory[(((terminal_row+terminal_jsoffset) * 128) + terminal_col) & 0xFFF] = ch;
                terminal_advance_cursor();
                break;
        }
    }
}

static void DELAYED_COPY_CODE(render_terminal_line)(uint16_t line) {
    uint glyph_line = line & 0x7;
    const uint8_t *line_buf = (const uint8_t *)terminal_memory + (((line>>3) * 128) & 0xFFF);
    bool cursor_row = ((((terminal_row+terminal_jsoffset) * 128) & 0xFFF) == (((line>>3) * 128) & 0xFFF));

    struct vga_scanline *sl = vga_prepare_scanline();
    uint sl_pos = 0;

    // Pad 40 pixels on the left to center horizontally
    sl->data[sl_pos++] = (text_border|THEN_EXTEND_7) | ((text_border|THEN_EXTEND_7) << 16); // 16 pixels per word
    sl->data[sl_pos++] = (text_border|THEN_EXTEND_7) | ((text_border|THEN_EXTEND_7) << 16); // 16 pixels per word
    sl->data[sl_pos++] = (text_border|THEN_EXTEND_3) | ((text_border|THEN_EXTEND_3) << 16); // 8 pixels per word

    for(uint col=0; col < 80; ) {
        // Grab 14 pixels from the next two characters
        uint32_t bits_a = char_terminal_bits(line_buf[col], glyph_line) ^ ((cursor_row && (col==terminal_col)) ? text_flasher_mask : 0x00);
        col++;
        uint32_t bits_b = char_terminal_bits(line_buf[col], glyph_line) ^ ((cursor_row && (col==terminal_col)) ? text_flasher_mask : 0x00);
        col++;

        uint32_t bits = (bits_a << 7) | bits_b;

        // Translate each pair of bits into a pair of pixels
        for(int i=0; i < 7; i++) {
            uint32_t pixeldata = (bits & 0x2000) ? (text_fore) : (text_back);
            pixeldata |= (bits & 0x1000) ?
                (((uint32_t)text_fore) << 16) :
                (((uint32_t)text_back) << 16);
            bits <<= 2;

            sl->data[sl_pos] = pixeldata;
            sl_pos++;
        }
    }

    // Pad 40 pixels on the right to center horizontally
    sl->data[sl_pos++] = (text_border|THEN_EXTEND_7) | ((text_border|THEN_EXTEND_7) << 16); // 16 pixels per word
    sl->data[sl_pos++] = (text_border|THEN_EXTEND_7) | ((text_border|THEN_EXTEND_7) << 16); // 16 pixels per word
    sl->data[sl_pos++] = (text_border|THEN_EXTEND_3) | ((text_border|THEN_EXTEND_3) << 16); // 8 pixels per word

    sl->length = sl_pos;
    sl->repeat_count = 1;
    vga_submit_scanline(sl);
}

void DELAYED_COPY_CODE(render_terminal)() {
    for(uint line=0; line < 192; line++) {
        render_terminal_line((terminal_jsoffset<<3)+line+terminal_ssoffset);
    }
    if(terminal_ssoffset > 0) {
        terminal_ssoffset++;
        if(terminal_ssoffset >= 8) {
            terminal_ssoffset = 0;
            terminal_jsoffset++;
        }
    }
}
