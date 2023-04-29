#include <string.h>
#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <hardware/pio.h>
#include "common/abus.h"
#include "common/config.h"
#include "common/build.h"
#include "common/modes.h"
#include "common/buffers.h"
#include "common/flash.h"
#include "common/dmacopy.h"

#ifdef FUNCTION_Z80
#include "z80/z80buf.h"
#include <hardware/uart.h>
#endif

#ifdef RASPBERRYPI_PICO_W
#include <pico/cyw43_arch.h>
#endif

static void __noinline __time_critical_func(core1_loop)() {
    uint32_t value;
    uint32_t address;

    for(;;) {
        value = pio_sm_get_blocking(CONFIG_ABUS_PIO, ABUS_MAIN_SM);
        address = (value >> 10) & 0xffff;

        // device read access
        if(ACCESS_READ) {
            if(CARD_SELECT) {
                //pio_sm_put(CONFIG_ABUS_PIO, ABUS_DEVICE_READ_SM, apple_memory[address]);
                CONFIG_ABUS_PIO->txf[ABUS_DEVICE_READ_SM] = apple_memory[address];
            }
        }

        busactive = 1;

        if(CARD_SELECT) {
            if(CARD_DEVSEL) {
                cardslot = (address >> 4) & 0x7;
            } else if(CARD_IOSEL) {
                cardslot = (address >> 8) & 0x7;

                // Config memory in card slot-rom address space
                if(ACCESS_WRITE) {
                    if((address & 0xFF) == 0xEC) {
                        apple_memory[address] = (value & 0xff);
                        cfptr = (cfptr & 0x0F00) | (value & 0xff);
                        apple_memory[address+2] = cfbuf[cfptr]; // $CnEE
                        apple_memory[address+3] = cfbuf[cfptr]; // $CnEF
                    }
                    if((address & 0xFF) == 0xED) {
                        apple_memory[address] = (value & 0x0F);
                        cfptr = ((cfptr & 0xFF) | (((uint16_t)value) << 8)) & 0xFFF;
                        apple_memory[address+1] = cfbuf[cfptr]; // $CnEE
                        apple_memory[address+2] = cfbuf[cfptr]; // $CnEF
                    }
                    if((address & 0xFF) == 0xEF) {
                        cfbuf[cfptr] = (value & 0xff);
                        cfptr = (cfptr + 1) & 0x0FFF;
                        apple_memory[address-1] = cfbuf[cfptr]; // $CnEE
                        apple_memory[address]   = cfbuf[cfptr]; // $CnEF
                    }
                    if((address & 0xFF) >= 0xF0) {
                        apple_memory[address] = (value & 0xff);
                    }
                } else if((address & 0xFF) == 0xEE) {
                    cfptr = (cfptr + 1) & 0x0FFF;
                    apple_memory[address]   = cfbuf[cfptr]; // $CnEE
                    apple_memory[address+1] = cfbuf[cfptr]; // $CnEF
                    apple_memory[address-1] = (cfptr >> 8) & 0xff;
                    apple_memory[address-2] = cfptr & 0xff;
                }
            }
#ifdef FUNCTION_VGA
        } else if(current_machine == MACHINE_AUTO) {
            if(value & 0x08000000) {
                // Hardware jumpered for IIGS mode.
                current_machine = MACHINE_IIGS;
                internal_flags &= ~IFLAGS_IIE_REGS;
                internal_flags |= IFLAGS_IIGS_REGS;
            } else if((apple_memory[0x404] == 0xE5) && (apple_memory[0x0403] == 0xD8)) { // ROMXe
                current_machine = MACHINE_IIE;
                internal_flags |= IFLAGS_IIE_REGS;
                internal_flags &= ~IFLAGS_IIGS_REGS;
            } else if((apple_memory[0x0413] == 0xD8) && (apple_memory[0x0412] == 0xC5)) { // ROMX
                current_machine = MACHINE_II;
                internal_flags &= ~(IFLAGS_IIE_REGS | IFLAGS_IIGS_REGS);
            } else if((apple_memory[0x0417] == 0xE7) && (apple_memory[0x416] == 0xC9)) { // Apple IIgs
                current_machine = MACHINE_IIGS;
                internal_flags &= ~IFLAGS_IIE_REGS;
                internal_flags |= IFLAGS_IIGS_REGS;
            } else if((apple_memory[0x0417] == 0xE5) && (apple_memory[0x416] == 0xAF)) { // Apple //e Enhanced
                current_machine = MACHINE_IIE;
                internal_flags |= IFLAGS_IIE_REGS;
                internal_flags &= ~IFLAGS_IIGS_REGS;
            } else if((apple_memory[0x0415] == 0xDD) && (apple_memory[0x413] == 0xE5)) { // Apple //e Unenhanced
                current_machine = MACHINE_IIE;
                internal_flags |= IFLAGS_IIE_REGS;
                internal_flags &= ~IFLAGS_IIGS_REGS;
            } else if(apple_memory[0x0410] == 0xD0) { // Apple II/Plus/J-Plus with Autostart
                current_machine = MACHINE_II;
                internal_flags &= ~(IFLAGS_IIE_REGS | IFLAGS_IIGS_REGS);
            } else if((apple_memory[0x07D1] == 0x60) && (apple_memory[0x07D0] == 0xAA)) { // Apple II without Autostart
                current_machine = MACHINE_II;
                internal_flags &= ~(IFLAGS_IIE_REGS | IFLAGS_IIGS_REGS);
            } else if(apple_memory[0x0410] == 0xF2) { // Pravetz!
                current_machine = MACHINE_PRAVETZ;
                internal_flags &= ~(IFLAGS_IIE_REGS | IFLAGS_IIGS_REGS);
            }
#endif
        } else switch(reset_state) {
            case 0:
                if((value & 0x7FFFF00) == ((0xFFFC << 10) | 0x300))
                     reset_state++;
                break;
            case 1:
                if((value & 0x7FFFF00) == ((0xFFFD << 10) | 0x300))
                     reset_state++;
                else
                     reset_state=0;
                break;
            case 2:
                if((value & 0x7FFFF00) == ((0xFA62 << 10) | 0x300))
                     reset_state++;
                else
                     reset_state=0;
                break;
            case 3:
#ifdef FUNCTION_VGA
                soft_switches |= SOFTSW_TEXT_MODE;
                soft_switches &= ~SOFTSW_80COL;
                soft_switches &= ~SOFTSW_DGR;
                internal_flags &= ~(IFLAGS_TERMINAL | IFLAGS_TEST | IFLAGS_V7_MODE3);
#endif
            default:
                reset_state = 0;
                break;
        }

#ifdef FUNCTION_VGA
        vga_businterface(address, value);
#endif
#ifdef FUNCTION_Z80
        z80_businterface(address, value);
#endif
    }
}

static void DELAYED_COPY_CODE(core0_loop)() {
#ifdef FUNCTION_VGA
    for(;;) vgamain();
#endif
#ifdef FUNCTION_Z80
    for(;;) z80main();
#endif
}

extern uint32_t __ram_delayed_copy_source__[];
extern uint32_t __ram_delayed_copy_start__[];
extern uint32_t __ram_delayed_copy_end__[];

int main() {
#if 0
    // OTA
    if(*(uint32_t*)FLASH_RESERVED != 0xFFFFFFFF) {
        flash_ota();
    }
#endif

    // Adjust system clock for better dividing into other clocks
    set_sys_clock_khz(CONFIG_SYSCLOCK*1000, true);

    abus_init();

    multicore_launch_core1(core1_loop);

    // Load 6502 code from flash into the memory buffer
    memcpy32((void*)apple_memory+0xC000, (void *)FLASH_6502_BASE, FLASH_6502_SIZE);

    // Initialize the config window in each rom slot
    memcpy((uint8_t*)apple_memory+0xC1F0, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFFV2AxCx00", 16);
    memcpy((uint8_t*)apple_memory+0xC2F0, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFFV2AxCx00", 16);
    memcpy((uint8_t*)apple_memory+0xC3F0, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFFV2AxCx00", 16);
    memcpy((uint8_t*)apple_memory+0xC4F0, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFFV2AxCx00", 16);
    memcpy((uint8_t*)apple_memory+0xC5F0, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFFV2AxCx00", 16);
    memcpy((uint8_t*)apple_memory+0xC6F0, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFFV2AxCx00", 16);
    memcpy((uint8_t*)apple_memory+0xC7F0, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFFV2AxCx00", 16);

    // Card Type identifiers
    apple_memory[0xC1FB] = HWBYTE;
    apple_memory[0xC2FB] = HWBYTE;
    apple_memory[0xC3FB] = HWBYTE;
    apple_memory[0xC4FB] = HWBYTE;
    apple_memory[0xC5FB] = HWBYTE;
    apple_memory[0xC6FB] = HWBYTE;
    apple_memory[0xC7FB] = HWBYTE;

    // Slot identifiers
    apple_memory[0xC1FD] = '1';
    apple_memory[0xC2FD] = '2';
    apple_memory[0xC3FD] = '3';
    apple_memory[0xC4FD] = '4';
    apple_memory[0xC5FD] = '5';
    apple_memory[0xC6FD] = '6';
    apple_memory[0xC7FD] = '7';

    // Finish copying remaining data and code to RAM from flash
    dmacpy32(__ram_delayed_copy_start__, __ram_delayed_copy_end__, __ram_delayed_copy_source__);

    // Load the config from flash, or defaults
    read_config();

#if defined(FUNCTION_Z80) && defined(ANALOG_GS)
    uart_init(uart0, sio[0].baudrate);
    uart_init(uart1, sio[1].baudrate);

    gpio_set_function(0, GPIO_FUNC_UART);
    gpio_set_function(1, GPIO_FUNC_UART);
    gpio_set_function(2, GPIO_FUNC_UART);
    gpio_set_function(3, GPIO_FUNC_UART);

    gpio_set_function(4, GPIO_FUNC_UART);
    gpio_set_function(5, GPIO_FUNC_UART);
    gpio_set_function(6, GPIO_FUNC_UART);
    gpio_set_function(7, GPIO_FUNC_UART);
#endif

    core0_loop();

    return 0;
}
