# PROGRESS — IF2130 Book I: Protected Mode x86 (Edisi 4)

Monitor pengerjaan sukabolOS, mengikuti struktur `Copy of IF2130 - Book - 2026.md`.
Centang `[x]` = sudah dikerjakan & ada di kode (`src/`). `[ ]` = belum.
Status diverifikasi dari isi `src/`, `Makefile`, dan `CHANGES_CH2.md` (per 2026-10-02).

## Ringkasan

| Chapter | Topik | Status |
|---|---|---|
| Ch. 0 | Toolchain, Kernel, GDT | ✅ Selesai |
| Ch. 1 | Framebuffer, Interrupt, Driver | ✅ Selesai |
| Ch. 2 | File System EXT2 — IF2130 Edition | ✅ Selesai |
| Ch. 3 | Paging, User Mode, Shell | 🟨 3.1 Paging kode selesai (belum diuji boot) |
| Ch. 4 | Process, Scheduler, Multitasking | ⬜ Belum dimulai |

Progres implementasi: **Ch. 0–2 dari Ch. 0–4** (3/5 chapter).

---

## Ch. 0 — Toolchain, Kernel, GDT

### 0.1. Repository & Toolchain
- [x] 0.1.1. Repository & Manual
- [x] 0.1.2. Toolchain & Visual Studio Code (`.vscode/tasks.json`, nasm/ld/gcc/qemu)

### 0.2. Kernel
- [x] 0.2.1. C Kernel & Linker (`src/kernel.c`, `src/kernel-entrypoint.s`, `src/linker.ld`)
- [x] 0.2.2. Image Creation & Automation (`Makefile`, `menu.lst`, ISO via genisoimage + GRUB)
- [x] 0.2.3. Running OS in QEMU (`make run`, GDB remote debugging)

### 0.3. Global Descriptor Table
- [x] 0.3.1. Data Structure: Segment Descriptor (`src/header/cpu/gdt.h`)
- [x] 0.3.2. GDT & GDTR Definition (`src/cpu/gdt.c`)
- [x] 0.3.3. Load GDT (`load_gdt` di `kernel-entrypoint.s`)

### 0.4. Extras: Computer
- [ ] Bacaan opsional (tidak ada implementasi)

---

## Ch. 1 — Framebuffer, Interrupt, Driver

- [x] 1.0. Short Note: Kernel Development

### 1.1. Driver Text Framebuffer
- [x] 1.1.1. `framebuffer_write()`
- [x] 1.1.2. `framebuffer_set_cursor()`
- [x] 1.1.3. `framebuffer_clear()`
- [x] 1.1.4. Test: Framebuffer (dashboard boot di `kernel.c`)

### 1.2. Interrupt
- [x] 1.2.1. IRQ Remapping (`pic_remap()` di `idt.c`)
- [x] 1.2.2. Interrupt Descriptor Table (`idt.c`, `idt.h`)
- [x] 1.2.3. Interrupt Service Routine (`interrupt.s`, `main_interrupt_handler()`)
- [x] 1.2.4. Load IDT & Testing Interrupt (`initialize_idt()`, `kernel_panic()`)

### 1.3. Keyboard Driver
- [x] 1.3.1. IRQ1 — Keyboard Controller
- [x] 1.3.2. Keyboard ISR (`keyboard_isr()`)
- [x] 1.3.3. Keyboard Interface (`keyboard_state_activate()`)
- [x] Tips: Keyboard Driver

---

## Ch. 2 — File System: EXT2 (IF2130 Edition)

- [x] 2.1. Disk Driver (ATA PIO: `read_blocks()`, `write_blocks()` di `disk.c`)
- [x] 2.2. Disk Image (`make disk` → `storage.bin` 4MB)
- [x] 2.3. Volatile & Non-Volatile Memory (bacaan)

### 2.4. FS: Design of EXT2 — IF2130 Edition
- [x] 2.4.1. Overview & Terminology
- [x] 2.4.2. Block Group
- [x] 2.4.3. File & Directory (2.4.3.1 File EXT2, 2.4.3.2 Directory EXT2)
- [x] 2.4.4. Root Directory & Interaction
- [x] 2.4.5. Design & Constraint (direct + single + double indirect)

### 2.5. FS: Initializer
- [x] `create_ext2()`, `is_empty_storage()`, `initialize_filesystem_ext2()`

### 2.6. FS: CRUD
- [x] 2.6.1. Read (`read()`, `read_directory()`)
- [x] 2.6.2. Write (`write()`)
- [x] 2.6.3. Delete (`delete()`, `deallocate_*()`)
- [x] 2.6.4. CRUD Implementation & Testing (blok test di `kernel.c`, verifikasi serial log)
- [x] Tips: File System

### 2.7. Extras: Hardware
- [ ] Bacaan opsional

---

## Ch. 3 — Paging, User Mode, Shell

> 3.1 Paging sudah ada di `src/`. Sisanya belum. Kit di `.kit/ch3/`.

### 3.1. Paging
> Kode selesai & berhasil di-compile/link; **belum diuji boot di QEMU** (cek `[CHECK]` di serial log).
- [x] 3.1.0. Paging: Overview (bacaan)
- [x] 3.1.1. Data Structure: Page Table (`src/header/memory/paging.h`)
- [x] 3.1.2. Higher Half Kernel (`src/linker.ld`, `0xC0100000`)
- [x] 3.1.3. Activate Paging (`src/kernel-entrypoint.s`, + recursive PDE 1023, `FRAMEBUFFER` → `0xC00B8000`)
- [x] 3.1.4. Memory Manager (`src/memory/paging.c`)
  - [x] 3.1.4.1. Frame Allocator
  - [x] 3.1.4.2. Frame Deallocator
  - [x] 3.1.4.3. Free Memory Check
- [x] Tips / Extra / Frequent Issue: Paging

### 3.2. Swap Space (opsional/bonus)
- [ ] 3.2.1. Page Fault
- [ ] 3.2.2. Swap In and Out
- [ ] 3.2.3. Page Replacement

### 3.3. User Mode
- [ ] 3.3.1. External Program: Inserter (`external-inserter.c` — kit ada, belum dipakai)
- [ ] 3.3.2. GDT: User & Task State Segment Descriptor
  - [ ] Task State Segment
  - [ ] User Segment Descriptor
- [ ] 3.3.3. Simple User Program
- [ ] 3.3.4. Execute Program
- [ ] 3.3.5. Launching User Mode
- [ ] Tips / Frequent Issue: User Mode

### 3.4. Shell
- [ ] 3.4.1. System Calls
  - [ ] 3.4.1.1. Designing System Calls
  - [ ] 3.4.1.2. Inter-Privilege Interrupt Syscall
  - [ ] 3.4.1.3. Calling Syscall
- [ ] 3.4.2. Command Line Interface
  - [ ] 3.4.2.1. Shell: Debugger
  - [ ] 3.4.2.2. Shell: Specification
- [ ] Tips: Shell
- [ ] Extra — Ch. 3: Security (opsional)

---

## Ch. 4 — Process, Scheduler, Multitasking

> Belum ada di `src/`. Kit tersedia di `.kit/ch4/` (`process.c/.h`, `scheduler.h`, `intsetup.s`).

### 4.1. Process
- [ ] 4.1.0. Correction: Kit Chapter 1 (`.kit/ch4/correction/intsetup.s`)
- [ ] 4.1.1. Multi Virtual Address Space
- [ ] 4.1.2. Process Control Block
  - [ ] 4.1.2.1. Process Context
  - [ ] 4.1.2.2. Process State
  - [ ] 4.1.2.3. Memory & Metadata
- [ ] 4.1.3. Process Creation
  - [ ] 4.1.3.1. Virtual Address Space
  - [ ] 4.1.3.2. Load Executable
  - [ ] 4.1.3.3. Context Initialization
  - [ ] 4.1.3.4. Process Metadata & Cleanup
- [ ] 4.1.4. Process: Init
- [ ] Frequent Issue: Process

### 4.2. Scheduler
- [ ] 4.2.1. Task Scheduler
  - [ ] 4.2.1.1. IRQ0 — Timer Interrupt & Scheduler Initialization
  - [ ] 4.2.1.2. Scheduling Algorithm
- [ ] 4.2.2. Context Switch
  - [ ] 4.2.2.1. CPU Register
  - [ ] 4.2.2.2. Virtual Address Space & Process
- [ ] 4.2.3. Test: Single Process

### 4.3. Multitasking
- [ ] 4.3.1. Process Entrypoint & Exit
- [ ] 4.3.2. Process Management & Command
- [ ] 4.3.3. Clock
- [ ] 4.3.4. External Application
- [ ] 4.3.5. Bonus: Environment Variables
- [ ] 4.3.6. Bonus: Separate Shell Commands
- [ ] 4.3.7. Grand Finale
- [ ] Extras: Operating System (opsional)

---

## Catatan

- Bug yang sudah ditemukan & diperbaiki di Ch. 2: ATA busy-wait race condition, SSE/#UD triple fault (`-mgeneral-regs-only`), precedence macro `GROUPS_COUNT` (detail di `CHANGES_CH2.md`).
- Update file ini setiap selesai satu sub-bab: ubah `[ ]` jadi `[x]` dan sesuaikan tabel Ringkasan.
