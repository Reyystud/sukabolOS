#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

#define KEYBOARD_DATA_PORT 0x60

/**
 * Peta Scancode PS/2 Set 1 ke Karakter ASCII (Make code / Key Press)
 */
extern const char keyboard_scancode_1_to_ascii[256];

/**
 * Mengaktifkan Keyboard Interrupt Handler pada IDT
 */
void keyboard_state_activate(void);

/**
 * Mengatur posisi awal cursor untuk echo karakter yang diketik, supaya
 * pemanggil (kernel.c) bisa menentukan di baris/kolom mana area ketik
 * dimulai sesuai layout layar yang sedang ditampilkan.
 */
void keyboard_echo_set_cursor(uint8_t row, uint8_t col);

/**
 * Fungsi C handler yang dipanggil saat terjadi interrupt keyboard (IRQ1 / 0x21)
 */
void keyboard_isr(void);

#endif