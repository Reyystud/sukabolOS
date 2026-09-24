#ifndef _PORTIO_H
#define _PORTIO_H

#include <stdint.h>

/**
 * Menulis 1 byte data ke I/O port tertentu.
 */
void outb(uint16_t port, uint8_t data);

/**
 * Membaca 1 byte data dari I/O port tertentu.
 */
uint8_t inb(uint16_t port);

/**
 * Menulis 2 byte data ke I/O port tertentu.
 */
void outw(uint16_t port, uint16_t data);

/**
 * Membaca 2 byte data dari I/O port tertentu.
 */
uint16_t inw(uint16_t port);

#endif