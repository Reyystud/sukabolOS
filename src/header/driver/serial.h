#ifndef _SERIAL_H
#define _SERIAL_H

#include <stdint.h>

#define SERIAL_COM1_PORT 0x3F8

/**
 * Inisialisasi UART COM1 (38400 baud, 8N1, FIFO enabled).
 * Dipanggil sekali di awal kernel_setup() sebelum apapun lain,
 * agar log [CHECK] tersedia sedini mungkin untuk debugging headless.
 */
void serial_init(void);

/**
 * Menulis 1 karakter ke COM1 (blocking, poll transmit-empty).
 */
void serial_write_char(char c);

/**
 * Menulis null-terminated string ke COM1.
 */
void serial_write(const char *s);

/**
 * Menulis uint32_t sebagai 8 digit hexadecimal (tanpa prefix 0x) ke COM1.
 */
void serial_write_hex32(uint32_t val);

#endif
