#include <stdint.h>
#include <stdbool.h>
#include "header/cpu/gdt.h"
#include "header/cpu/idt.h"
#include "header/driver/framebuffer.h"
#include "header/driver/keyboard.h"
#include "header/kernel-entrypoint.h"

static void print_at(uint8_t row, uint8_t col, const char *s, uint8_t fg, uint8_t bg) {
    for (int i = 0; s[i] != '\0'; i++) framebuffer_write(row, col + i, s[i], fg, bg);
}

void kernel_setup(void) {
    uint32_t a;
    uint32_t volatile b = 0x0000BABE;
    __asm__("mov $0xCAFE0000, %0" : "=r"(a));

    load_gdt(&_gdt_gdtr);
    initialize_idt();

    framebuffer_clear();
    print_at(0, 0, "sukabolOS - Exception Handler Test", COLOR_LIGHT_CYAN, COLOR_BLACK);
    print_at(1, 0, "Ketik angka untuk trigger CPU exception (panic merah):", COLOR_WHITE, COLOR_BLACK);
    print_at(2, 0, " [1] #DE 0x00 Division By Zero (int $0)", COLOR_WHITE, COLOR_BLACK);
    print_at(3, 0, " [2] #UD 0x06 Invalid Opcode (UD2)", COLOR_WHITE, COLOR_BLACK);
    print_at(4, 0, " [3] #GP 0x0D General Protection (int $0x0D)", COLOR_WHITE, COLOR_BLACK);
    print_at(5, 0, " [4] #PF 0x0E Page Fault (null deref, CR2 test)", COLOR_WHITE, COLOR_BLACK);
    print_at(6, 0, " [5] #DF 0x08 Double Fault (int $0x08)", COLOR_WHITE, COLOR_BLACK);
    print_at(7, 0, " [6] #BR 0x05 Bound Range / [0] auto DIV0", COLOR_WHITE, COLOR_BLACK);
    print_at(8, 0, " Tombol lain = ketik normal di baris bawah.", COLOR_LIGHT_GRAY, COLOR_BLACK);
    print_at(9, 0, "------------------------------------------------", COLOR_DARK_GRAY, COLOR_BLACK);
    framebuffer_set_cursor(10, 0);

    // Aktifkan Keyboard Driver
    keyboard_state_activate();

    // Auto-test disabled by default. Uncomment to panic immediately on boot:
    // __asm__ volatile("int $0x00");

    while (true) {
        b += 1;
        (void)b;
        __asm__ volatile("hlt");
    }
}