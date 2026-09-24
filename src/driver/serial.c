#include "header/driver/serial.h"
#include "header/cpu/portio.h"

void serial_init(void) {
    outb(SERIAL_COM1_PORT + 1, 0x00); // disable interrupts
    outb(SERIAL_COM1_PORT + 3, 0x80); // enable DLAB (set baud rate divisor)
    outb(SERIAL_COM1_PORT + 0, 0x03); // divisor low byte -> 38400 baud
    outb(SERIAL_COM1_PORT + 1, 0x00); // divisor high byte
    outb(SERIAL_COM1_PORT + 3, 0x03); // 8 bits, no parity, one stop bit
    outb(SERIAL_COM1_PORT + 2, 0xC7); // enable FIFO, clear, 14-byte threshold
    outb(SERIAL_COM1_PORT + 4, 0x0B); // IRQs disabled, RTS/DSR set
}

static int serial_transmit_empty(void) {
    return inb(SERIAL_COM1_PORT + 5) & 0x20;
}

void serial_write_char(char c) {
    while (!serial_transmit_empty());
    outb(SERIAL_COM1_PORT, (uint8_t) c);
}

void serial_write(const char *s) {
    for (int i = 0; s[i] != '\0'; i++) {
        serial_write_char(s[i]);
    }
}

void serial_write_hex32(uint32_t val) {
    const char *hex = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--) {
        uint8_t nibble = (val >> (i * 4)) & 0xF;
        serial_write_char(hex[nibble]);
    }
}
