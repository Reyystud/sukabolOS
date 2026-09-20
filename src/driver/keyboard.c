#include "header/driver/keyboard.h"
#include "header/cpu/portio.h"
#include "header/cpu/idt.h"
#include "header/driver/framebuffer.h"

// Pemetaan Scancode PS/2 (Set 1) ke ASCII untuk Key Down (0x00 - 0x7F)
const char keyboard_scancode_1_to_ascii[256] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
     0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
     0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0,
   '*',   0, ' ',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
};

static uint8_t cursor_row = 1;
static uint8_t cursor_col = 0;

extern void main_interrupt_handler_0x21(void);

void keyboard_state_activate(void) {
    // Daftarkan handler khusus ISR 0x21 (IRQ1) di IDT
    idt_set_interrupt_handler(
        0x21,
        (void*) main_interrupt_handler_0x21,
        0x08,
        INTERRUPT_GATE_R0
    );
}

static void trigger_test(uint8_t scancode) {
    switch (scancode) {
        case 0x02: // '1' -> #DE Division By Zero via int
            __asm__ volatile("int $0x00");
            break;
        case 0x03: // '2' -> #UD Invalid Opcode via UD2
            __asm__ volatile("ud2");
            break;
        case 0x04: // '3' -> #GP via int
            __asm__ volatile("int $0x0D");
            break;
        case 0x05: // '4' -> #PF real null deref (CR2 should show 0x0 or DEADBEEF)
            *(volatile int*)0x0 = 42;
            break;
        case 0x06: // '5' -> #DF Double Fault
            __asm__ volatile("int $0x08");
            break;
        case 0x07: // '6' -> #BR Bound Range
            __asm__ volatile("int $0x05");
            break;
        case 0x0B: // '0' -> real DIV0 using div instruction (generates #DE with real CPU fault)
            {
                volatile int a = 1;
                volatile int b = 0;
                volatile int c = a / b;
                (void)c;
            }
            break;
        default: break;
    }
}

void keyboard_isr(void) {
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    // Cek apakah scancode adalah "Make Code" (Key Press), yaitu bit-7 = 0 (scancode < 0x80)
    if (!(scancode & 0x80)) {
        // Test trigger takes priority over normal typing for keys 1-6,0
        if (scancode == 0x02 || scancode == 0x03 || scancode == 0x04 ||
            scancode == 0x05 || scancode == 0x06 || scancode == 0x07 || scancode == 0x0B) {
            trigger_test(scancode);
            // if trigger didn't panic (int-based), return and let EOI happen
        }

        char c = keyboard_scancode_1_to_ascii[scancode];
        if (c != 0) {
            // jangan cetak angka test yang sudah dipakai untuk trigger agar tidak double
            if (c >= '1' && c <= '6') {
                // sudah handle sebagai test, skip print to keep screen clean
                // tapi tetap update cursor? skip
            } else if (c == '0') {
                // skip juga
            } else if (c == '\n') {
                cursor_row++;
                if (cursor_row >= FRAMEBUFFER_HEIGHT) cursor_row = FRAMEBUFFER_HEIGHT - 1;
                cursor_col = 0;
            } else if (c == '\b') {
                if (cursor_col > 0) {
                    cursor_col--;
                    framebuffer_write(cursor_row, cursor_col, ' ', COLOR_WHITE, COLOR_BLACK);
                }
            } else {
                if (cursor_row >= FRAMEBUFFER_HEIGHT) cursor_row = FRAMEBUFFER_HEIGHT - 1;
                if (cursor_col >= FRAMEBUFFER_WIDTH) {
                    cursor_col = 0;
                    cursor_row++;
                    if (cursor_row >= FRAMEBUFFER_HEIGHT) cursor_row = FRAMEBUFFER_HEIGHT - 1;
                }
                framebuffer_write(cursor_row, cursor_col, c, COLOR_LIGHT_GREEN, COLOR_BLACK);
                cursor_col++;
                if (cursor_col >= FRAMEBUFFER_WIDTH) {
                    cursor_col = 0;
                    cursor_row++;
                    if (cursor_row >= FRAMEBUFFER_HEIGHT) cursor_row = FRAMEBUFFER_HEIGHT - 1;
                }
            }
            framebuffer_set_cursor(cursor_row, cursor_col);
        }
    }

    // EOI (End of Interrupt) dikirim otomatis di main_interrupt_handler
}