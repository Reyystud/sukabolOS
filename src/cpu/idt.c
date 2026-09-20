#include "header/cpu/idt.h"
#include "header/cpu/portio.h"
#include "header/driver/keyboard.h"
#include "header/driver/framebuffer.h"

extern void main_interrupt_empty_handler(void);

extern void main_interrupt_handler_0x00(void);
extern void main_interrupt_handler_0x01(void);
extern void main_interrupt_handler_0x02(void);
extern void main_interrupt_handler_0x03(void);
extern void main_interrupt_handler_0x04(void);
extern void main_interrupt_handler_0x05(void);
extern void main_interrupt_handler_0x06(void);
extern void main_interrupt_handler_0x07(void);
extern void main_interrupt_handler_0x08(void);
extern void main_interrupt_handler_0x09(void);
extern void main_interrupt_handler_0x0A(void);
extern void main_interrupt_handler_0x0B(void);
extern void main_interrupt_handler_0x0C(void);
extern void main_interrupt_handler_0x0D(void);
extern void main_interrupt_handler_0x0E(void);
extern void main_interrupt_handler_0x0F(void);
extern void main_interrupt_handler_0x10(void);
extern void main_interrupt_handler_0x11(void);
extern void main_interrupt_handler_0x12(void);
extern void main_interrupt_handler_0x13(void);
extern void main_interrupt_handler_0x14(void);
extern void main_interrupt_handler_0x15(void);
extern void main_interrupt_handler_0x16(void);
extern void main_interrupt_handler_0x17(void);
extern void main_interrupt_handler_0x18(void);
extern void main_interrupt_handler_0x19(void);
extern void main_interrupt_handler_0x1A(void);
extern void main_interrupt_handler_0x1B(void);
extern void main_interrupt_handler_0x1C(void);
extern void main_interrupt_handler_0x1D(void);
extern void main_interrupt_handler_0x1E(void);
extern void main_interrupt_handler_0x1F(void);

struct InterruptGate interrupt_descriptor_table[IDT_MAX_ENTRY] = {0};
struct IDTR _idt_idtr;

static const char *exception_messages[32] = {
    "Division By Zero",            // 0x00
    "Debug",                       // 0x01
    "Non-Maskable Interrupt",      // 0x02
    "Breakpoint",                  // 0x03
    "Into Detected Overflow",      // 0x04
    "Out of Bounds",               // 0x05
    "Invalid Opcode",              // 0x06
    "No Coprocessor",              // 0x07
    "Double Fault",                // 0x08
    "Coprocessor Segment Overrun", // 0x09
    "Bad TSS",                     // 0x0A
    "Segment Not Present",         // 0x0B
    "Stack Fault",                 // 0x0C
    "General Protection Fault",    // 0x0D
    "Page Fault",                  // 0x0E
    "Unknown Interrupt",           // 0x0F
    "Coprocessor Fault",           // 0x10
    "Alignment Check",             // 0x11
    "Machine Check",               // 0x12
    "SIMD FP Exception",           // 0x13
    "Virtualization Exception",    // 0x14
    "Control Protection Exception",// 0x15
    "Reserved",                    // 0x16
    "Reserved",                    // 0x17
    "Reserved",                    // 0x18
    "Reserved",                    // 0x19
    "Reserved",                    // 0x1A
    "Reserved",                    // 0x1B
    "Reserved",                    // 0x1C
    "Reserved",                    // 0x1D
    "Reserved",                    // 0x1E
    "Reserved",                    // 0x1F
};

static void pic_remap(void) {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    outb(0x21, 0xFD);
    outb(0xA1, 0xFF);
}

void idt_set_interrupt_handler(uint8_t int_number, void *handler_pt, uint8_t gdt_seg, uint8_t attr) {
    uint32_t base = (uint32_t)(uintptr_t) handler_pt;
    interrupt_descriptor_table[int_number].offset_low     = base & 0xFFFF;
    interrupt_descriptor_table[int_number].segment        = gdt_seg;
    interrupt_descriptor_table[int_number]._reserved      = 0;
    interrupt_descriptor_table[int_number].type_attribute = attr;
    interrupt_descriptor_table[int_number].offset_high    = (base >> 16) & 0xFFFF;
}

void initialize_idt(void) {
    _idt_idtr.limit = sizeof(interrupt_descriptor_table) - 1;
    _idt_idtr.base  = (uint32_t)(uintptr_t) &interrupt_descriptor_table;

    for (int i = 0; i < IDT_MAX_ENTRY; i++) {
        idt_set_interrupt_handler(i, (void*) main_interrupt_empty_handler, 0x08, INTERRUPT_GATE_R0);
    }

    void *exception_handlers[] = {
        main_interrupt_handler_0x00, main_interrupt_handler_0x01, main_interrupt_handler_0x02,
        main_interrupt_handler_0x03, main_interrupt_handler_0x04, main_interrupt_handler_0x05,
        main_interrupt_handler_0x06, main_interrupt_handler_0x07, main_interrupt_handler_0x08,
        main_interrupt_handler_0x09, main_interrupt_handler_0x0A, main_interrupt_handler_0x0B,
        main_interrupt_handler_0x0C, main_interrupt_handler_0x0D, main_interrupt_handler_0x0E,
        main_interrupt_handler_0x0F, main_interrupt_handler_0x10, main_interrupt_handler_0x11,
        main_interrupt_handler_0x12, main_interrupt_handler_0x13, main_interrupt_handler_0x14,
        main_interrupt_handler_0x15, main_interrupt_handler_0x16, main_interrupt_handler_0x17,
        main_interrupt_handler_0x18, main_interrupt_handler_0x19, main_interrupt_handler_0x1A,
        main_interrupt_handler_0x1B, main_interrupt_handler_0x1C, main_interrupt_handler_0x1D,
        main_interrupt_handler_0x1E, main_interrupt_handler_0x1F
    };

    for (uint8_t i = 0; i <= 0x1F; i++) {
        idt_set_interrupt_handler(i, exception_handlers[i], 0x08, INTERRUPT_GATE_R0);
    }

    pic_remap();

    __asm__ volatile ("lidt %0" : : "m"(_idt_idtr));
    __asm__ volatile ("sti");
}

// Helpers for kernel panic display - no dependencies, direct framebuffer
static void panic_puts(uint8_t row, uint8_t col, const char *s, uint8_t fg, uint8_t bg) {
    for (int i = 0; s[i] != '\0'; i++) {
        if (col >= FRAMEBUFFER_WIDTH) break;
        framebuffer_write(row, col++, s[i], fg, bg);
    }
}

static void panic_put_hex8(uint8_t row, uint8_t col, uint8_t val, uint8_t fg, uint8_t bg) {
    const char *hex = "0123456789ABCDEF";
    framebuffer_write(row, col,     hex[(val >> 4) & 0xF], fg, bg);
    framebuffer_write(row, col + 1, hex[val & 0xF], fg, bg);
}

static void panic_put_hex32(uint8_t row, uint8_t col, uint32_t val, uint8_t fg, uint8_t bg) {
    const char *hex = "0123456789ABCDEF";
    framebuffer_write(row, col,     '0', fg, bg);
    framebuffer_write(row, col + 1, 'x', fg, bg);
    for (int i = 0; i < 8; i++) {
        uint8_t nibble = (val >> (28 - i * 4)) & 0xF;
        framebuffer_write(row, col + 2 + i, hex[nibble], fg, bg);
    }
}

static void kernel_panic(struct InterruptFrame frame) {
    __asm__ volatile ("cli");

    framebuffer_clear();

    panic_puts(0, 0, "=== KERNEL PANIC ===", COLOR_WHITE, COLOR_RED);

    // Line 2: Exception name + vector hex
    const char *msg = (frame.int_number < 32) ? exception_messages[frame.int_number] : "Unknown Exception";
    panic_puts(2, 0, msg, COLOR_LIGHT_RED, COLOR_BLACK);
    panic_puts(2, 40, "Vector:", COLOR_WHITE, COLOR_BLACK);
    panic_puts(2, 48, "0x", COLOR_WHITE, COLOR_BLACK);
    panic_put_hex8(2, 50, (uint8_t)frame.int_number, COLOR_WHITE, COLOR_BLACK);

    // Line 3: Error code + EIP/CS/EFLAGS
    panic_puts(3, 0, "ERR:", COLOR_WHITE, COLOR_BLACK);
    panic_put_hex32(3, 5, frame.error_code, COLOR_WHITE, COLOR_BLACK);
    panic_puts(3, 16, "EIP:", COLOR_WHITE, COLOR_BLACK);
    panic_put_hex32(3, 21, frame.eip, COLOR_WHITE, COLOR_BLACK);
    panic_puts(3, 32, "CS:", COLOR_WHITE, COLOR_BLACK);
    panic_put_hex32(3, 36, frame.cs, COLOR_WHITE, COLOR_BLACK);
    panic_puts(4, 0, "EFLAGS:", COLOR_WHITE, COLOR_BLACK);
    panic_put_hex32(4, 8, frame.eflags, COLOR_WHITE, COLOR_BLACK);
    if (frame.int_number == 0x0E) {
        uint32_t cr2;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
        panic_puts(4, 20, "CR2:", COLOR_WHITE, COLOR_BLACK);
        panic_put_hex32(4, 25, cr2, COLOR_WHITE, COLOR_BLACK);
    }

    // Line 5-6: Register dump (order matches InterruptFrame)
    panic_puts(6, 0, "EAX:", COLOR_LIGHT_CYAN, COLOR_BLACK);
    panic_put_hex32(6, 5, frame.eax, COLOR_WHITE, COLOR_BLACK);
    panic_puts(6, 16, "EBX:", COLOR_LIGHT_CYAN, COLOR_BLACK);
    panic_put_hex32(6, 21, frame.ebx, COLOR_WHITE, COLOR_BLACK);
    panic_puts(6, 32, "ECX:", COLOR_LIGHT_CYAN, COLOR_BLACK);
    panic_put_hex32(6, 37, frame.ecx, COLOR_WHITE, COLOR_BLACK);
    panic_puts(6, 48, "EDX:", COLOR_LIGHT_CYAN, COLOR_BLACK);
    panic_put_hex32(6, 53, frame.edx, COLOR_WHITE, COLOR_BLACK);

    panic_puts(7, 0, "ESI:", COLOR_LIGHT_CYAN, COLOR_BLACK);
    panic_put_hex32(7, 5, frame.esi, COLOR_WHITE, COLOR_BLACK);
    panic_puts(7, 16, "EDI:", COLOR_LIGHT_CYAN, COLOR_BLACK);
    panic_put_hex32(7, 21, frame.edi, COLOR_WHITE, COLOR_BLACK);
    panic_puts(7, 32, "EBP:", COLOR_LIGHT_CYAN, COLOR_BLACK);
    panic_put_hex32(7, 37, frame.ebp, COLOR_WHITE, COLOR_BLACK);
    panic_puts(7, 48, "ESP:", COLOR_LIGHT_CYAN, COLOR_BLACK);
    panic_put_hex32(7, 53, frame.esp, COLOR_WHITE, COLOR_BLACK);

    panic_puts(9, 0, "System halted. Please reboot.", COLOR_LIGHT_GRAY, COLOR_BLACK);

    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}

void main_interrupt_handler(struct InterruptFrame frame) {
    // Marker 0xFF from empty handler = unknown/spurious interrupt, do not panic
    if (frame.int_number == 0xFF) {
        // Still need EOI if it was a hardware IRQ in PIC range
        // But empty handler with 0xFF should not be PIC range normally;
        // handle safely if mis-attributed
        return;
    }

    if (frame.int_number < 0x20) {
        kernel_panic(frame);
    } else if (frame.int_number == 0x21) {
        keyboard_isr();
    }

    if (frame.int_number >= 0x20 && frame.int_number <= 0x2F) {
        if (frame.int_number >= 0x28) {
            outb(0xA0, 0x20);
        }
        outb(0x20, 0x20);
    }
}
