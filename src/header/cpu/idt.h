#ifndef _IDT_H
#define _IDT_H

#include <stdint.h>

#define IDT_MAX_ENTRY 256

// Type Attribute Flags
#define INTERRUPT_GATE_R3 0xEE // 1110 1110b (Present, Ring 3, 32-bit Interrupt Gate)
#define INTERRUPT_GATE_R0 0x8E // 1000 1110b (Present, Ring 0, 32-bit Interrupt Gate)

/**
 * Interrupt Descriptor Gate Structure
 */
struct InterruptGate {
    uint16_t offset_low;  // Target offset bits 0..15
    uint16_t segment;     // Target code segment selector (Kernel Code = 0x08)
    uint8_t  _reserved;   // Unused, set to 0
    uint8_t  type_attribute; // Type and attributes
    uint16_t offset_high; // Target offset bits 16..31
} __attribute__((packed));

/**
 * IDT Register Pointer Structure (IDTR)
 */
struct IDTR {
    uint16_t limit; // Size of IDT array - 1
    uint32_t base;  // Pointer to the first element in IDT
} __attribute__((packed));

/**
 * Context frame register structure passed by assembly ISR
 * Layout must match stack after isr_common_stub:
 *   pusha (EDI,ESI,EBP,ESP,EBX,EDX,ECX,EAX) + int_number + error_code + CPU (EIP,CS,EFLAGS)
 */
struct InterruptFrame {
    uint32_t edi, esi, ebp, esp;
    uint32_t ebx, edx, ecx, eax;
    uint32_t int_number, error_code;
    uint32_t eip, cs, eflags;
} __attribute__((packed));

extern struct IDTR _idt_idtr;

/**
 * Inisialisasi IDT dan remap PIC
 */
void initialize_idt(void);

/**
 * Memasang handler untuk vektor interrupt tertentu
 */
void idt_set_interrupt_handler(uint8_t int_number, void *handler_pt, uint8_t gdt_seg, uint8_t attr);

/**
 * Main Interrupt Handler C function
 */
void main_interrupt_handler(struct InterruptFrame frame);

#endif