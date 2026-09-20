global main_interrupt_empty_handler
global main_interrupt_handler_0x21
extern main_interrupt_handler

; Default empty handler - use 0xFF marker to avoid false Division By Zero panic
main_interrupt_empty_handler:
    push dword 0xFF             ; Push dummy error code
    push dword 0xFF             ; Push dummy interrupt number (marker for unknown)
    jmp isr_common_stub

; Macro untuk ISR TANPA Error Code dari CPU
%macro ISR_NOERRCODE 1
global main_interrupt_handler_%1
main_interrupt_handler_%1:
    push dword 0                ; Push dummy error code
    push dword %1               ; Push interrupt number
    jmp isr_common_stub
%endmacro

; Macro untuk ISR DENGAN Error Code dari CPU
%macro ISR_ERRCODE 1
global main_interrupt_handler_%1
main_interrupt_handler_%1:
    push dword %1               ; Push interrupt number (error code sudah ada di stack)
    jmp isr_common_stub
%endmacro

; --- CPU Exceptions (0x00 - 0x1F) ---
ISR_NOERRCODE 0x00 ; Division By Zero
ISR_NOERRCODE 0x01 ; Debug
ISR_NOERRCODE 0x02 ; Non-Maskable Interrupt
ISR_NOERRCODE 0x03 ; Breakpoint
ISR_NOERRCODE 0x04 ; Overflow
ISR_NOERRCODE 0x05 ; Bound Range Exceeded
ISR_NOERRCODE 0x06 ; Invalid Opcode
ISR_NOERRCODE 0x07 ; Device Not Available
ISR_ERRCODE   0x08 ; Double Fault
ISR_NOERRCODE 0x09 ; Coprocessor Segment Overrun
ISR_ERRCODE   0x0A ; Invalid TSS
ISR_ERRCODE   0x0B ; Segment Not Present
ISR_ERRCODE   0x0C ; Stack-Segment Fault
ISR_ERRCODE   0x0D ; General Protection Fault
ISR_ERRCODE   0x0E ; Page Fault
ISR_NOERRCODE 0x0F ; Reserved
ISR_NOERRCODE 0x10 ; x87 Floating-Point Exception
ISR_ERRCODE   0x11 ; Alignment Check
ISR_NOERRCODE 0x12 ; Machine Check
ISR_NOERRCODE 0x13 ; SIMD Floating-Point Exception
ISR_NOERRCODE 0x14 ; Virtualization Exception
ISR_ERRCODE   0x15 ; Control Protection Exception
ISR_NOERRCODE 0x16 ; Reserved
ISR_NOERRCODE 0x17 ; Reserved
ISR_NOERRCODE 0x18 ; Reserved
ISR_NOERRCODE 0x19 ; Reserved
ISR_NOERRCODE 0x1A ; Reserved
ISR_NOERRCODE 0x1B ; Reserved
ISR_NOERRCODE 0x1C ; Reserved
ISR_NOERRCODE 0x1D ; Reserved
ISR_NOERRCODE 0x1E ; Reserved
ISR_NOERRCODE 0x1F ; Reserved

; --- Hardware Interrupt (0x21 - Keyboard) ---
ISR_NOERRCODE 0x21

; Stub umum menyimpan register & panggil C handler
isr_common_stub:
    pusha                       ; Push EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX
    call main_interrupt_handler
    popa                        ; Pop general registers
    add esp, 8                  ; Clean up error code dan int number
    iret                        ; Interrupt Return