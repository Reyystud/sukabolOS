# sukabolOS — Full Requirement Tasks (based on IF2130 Book — 2026)

Extracted from `IF2130 - Book - 2026.pdf` (206 pages, "OS-IF2130" guidebook). This is a checklist of every concrete implementation task, file, function, and behavior the book specifies, organized by chapter. Design notes and "why" context are kept alongside tasks where they affect correctness (e.g. exact struct layouts, error codes, alignment rules).

> Legend: `[ ]` = not yet verified against current repo state. Check off as you confirm/implement. Items marked **(bonus)** are optional/extra credit per the book.

---

## Ch. 0 — Toolchain, Kernel, GDT

### 0.1 Repository & Toolchain
- [ ] Fork/use template repo (`.vscode/`, `.kit/` per-chapter kits, `bin/`, `other/grub1`, `src/` skeleton: `header/kernel-entrypoint.h`, `header/cpu/gdt.h`, `header/stdlib/string.h`, `stdlib/string.c`, `kernel-entrypoint.s`, `.gitignore`, `makefile`, `README.md`)
- [ ] Install toolchain (Linux/WSL2 Ubuntu 20.04/22.04 or Apple Silicon alt): `nasm gcc qemu-system-x86 make genisoimage gdb`
- [ ] VS Code + C/C++ Extension Pack + Remote-WSL
- [ ] Keep Intel x86/x64 Software Developer Manual Vol 3A handy as reference throughout

### 0.2 Kernel
- [ ] `kernel.c`: `void kernel_setup(void)` — includes `stdint.h`, `header/cpu/gdt.h`, `header/kernel-entrypoint.h`; test payload: `uint32_t a; volatile uint32_t b = 0x0000BABE; __asm__("mov $0xCAFE0000, %0" : "=r"(a)); while(true) b += 1;`
- [ ] `src/linker.ld`: `ENTRY(loader)`; sections `.multiboot/.text/.rodata/.data/.bss` each `ALIGN(0x1000)`; load address `0x00100000` (1 MiB)
- [ ] `menu.lst`: `default 0`, `timeout 0`, `title os`, `kernel /boot/kernel`
- [ ] Complete Makefile TODOs: compile `kernel.c`, assemble `kernel-entrypoint.s`, link, build ISO (grub1 + genisoimage), producing `bin/OS2025.iso` (or similarly named)
  - gcc: `-ffreestanding -fshort-wchar -g -nostdlib -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror -m32`
  - nasm: `-f elf32 -g -F dwarf`
  - ld: `-T src/linker.ld -melf_i386`
  - genisoimage: `-b boot/grub/grub1 -no-emul-boot -boot-load-size 4 -A os -input-charset utf8 -quiet -boot-info-table`
- [ ] VS Code "Kernel" Run & Debug config (F5 start w/ debugger, Shift+F5 stop)
- [ ] Boot in QEMU: `qemu-system-i386 -s -cdrom OS2025.iso` — no crash, debugger attaches

### 0.3 Global Descriptor Table
- [ ] `struct SegmentDescriptor` bit-field in `gdt.h`: segment_low, base_low, base_mid, type_bit:4, non_system:1, DPL:2, present:1, segment_high:4, avl:1, long_mode:1, db:1, granularity:1, base_high
- [ ] `global_descriptor_table` with 3 entries: Null Descriptor (all 0), Kernel Code (base=0, limit=0xFFFFF, S=1, DPL=0, P=1, L=0, D/B=1, G=1, type=0xA), Kernel Data (type=0x2, else same)
- [ ] `_gdt_gdtr` (`struct GDTR`) pointing to the table, size = `sizeof(gdt)-1`
- [ ] Makefile rule to compile `gdt.c`
- [ ] `load_gdt` (assembly, in `kernel-entrypoint.s`): `lgdt`, set CR0 bit 0 (PE), far jump into protected mode, reload `ss/ds/es` to kernel selectors
- [ ] Call `load_gdt(&_gdt_gdtr)` in `kernel_setup()`
- [ ] Verify: no triple fault / bootloop

---

## Ch. 1 — Framebuffer, Interrupt, Driver

### 1.0 Coding conventions
- [ ] Consistent style (tabs = 8 chars, Linux-ish)
- [ ] Never weaken `-Wall -Wextra -Werror` — fix all warnings instead
- [ ] Include guards (`#ifndef/#define/#endif`) on every new header

### 1.1 Text Framebuffer Driver (kit: `ch1/1 - Framebuffer/`)
- [ ] `void framebuffer_write(uint8_t row, uint8_t col, char c, uint8_t fg, uint8_t bg)` — MMIO at `FRAMEBUFFER_MEMORY_OFFSET` (`0xB8000`), 2 bytes/char (char, then fg/bg attribute byte)
- [ ] `void framebuffer_set_cursor(uint8_t r, uint8_t c)` — port I/O via `portio.c` in/out wrappers
- [ ] `void framebuffer_clear(void)` — e.g. via `memset()`
- [ ] Doxygen-style comments on new declarations; sensible file/folder organization
- [ ] Test: `framebuffer_clear(); framebuffer_write(3,8,'H',0,0xF); ...` writes "Hai!" inverted-color, `framebuffer_set_cursor(3,10)`

### 1.2 Interrupt (kit: `ch1/2 - Interrupt/`)
- [ ] `io_wait()` (`out(0x80,0)`); `pic_ack(uint8_t irq)` (ack PIC2 if irq≥8, always PIC1); `pic_remap()` (ICW1/ICW2 offsets 0x20/0x28, ICW3 cascade, ICW4_8086, then mask all IRQs)
- [ ] IDT structures: `struct IDTGate`, `struct InterruptDescriptorTable` (256 entries), `struct IDTR`; globals `interrupt_descriptor_table`, `_idt_idtr`
- [ ] ISR scaffolding (`intsetup.s` provided): `isr_stub_table`, `call_generic_handler()`, `interrupt_handler_i()`
- [ ] `void main_interrupt_handler(struct InterruptFrame frame)` — switch on `frame.int_number`
- [ ] `void set_interrupt_gate(uint8_t int_vector, void *handler_address, uint16_t gdt_seg_selector, uint8_t privilege)`
- [ ] `void initialize_idt(void)` — populate all 256 gates from `isr_stub_table`, `lidt`, `sti`
- [ ] Test: `__asm__("int $0x4")` after full init — handler runs and returns, no crash
- [ ] Watch for: IRQ0 unmasked-by-default noise; remap-not-applied symptoms (int_number < 0x20)

### 1.3 Keyboard Driver (kit: `ch1/3 - Keyboard/`)
- [ ] `activate_keyboard_interrupt(void)` — unmask IRQ1 on PIC1, called after `initialize_idt()`
- [ ] `keyboard_isr(void)`: always does ≥1 `in` from `KEYBOARD_DATA_PORT` per IRQ1; only buffers when `keyboard_input_on`; use `keyboard_scancode_1_to_ascii_map` (Set 1, make/break = make|0x80); **no loop inside ISR**; always `pic_ack()` IRQ1; non-blocking
- [ ] `keyboard.c`: static `keyboard_state` (on flag + buffer); `keyboard_state_activate()`; `keyboard_state_deactivate()`; `get_keyboard_buffer(char *buf)` (copies AND clears buffer)
- [ ] Test via `keyboard_state_activate()` + typing, display via framebuffer

---

## Ch. 2 — File System: EXT2 (IF2130 Edition)

### 2.1 Disk Driver
- [x] `outw/inw` 16-bit port I/O helpers in `portio.c`
- [x] ATA PIO driver (`disk.c`, blocking): `ATA_busy_wait()`, `ATA_DRQ_wait()`, `read_blocks(void*, uint32_t lba, uint8_t count)`, `write_blocks(const void*, uint32_t lba, uint8_t count)` — program ports 0x1F2–0x1F7, command 0x20 (read)/0x30 (write), transfer via `in16/out16` on 0x1F0. *(Found & fixed a real race: must `ATA_busy_wait()` once more after the transfer loop, or a command issued immediately after hangs waiting on the drive to settle.)*

### 2.2 Disk Image
- [x] Makefile `disk:` target: `qemu-img create -f raw bin/storage.bin 4M` (manual, not on every build)
- [x] Add `-drive file=bin/storage.bin,format=raw,if=ide,index=0,media=disk` to all QEMU launch configs (Makefile `run`/`debug-run` + `.vscode/tasks.json`)
- [x] Verify writes with `xxd` at the correct byte offset (block × 0x200) — confirmed

### 2.3 Volatile/Non-volatile — conceptual only, no separate task beyond using read/write_blocks for persistence.

### 2.4 FS Design (EXT2 - IF2130 Edition) — structures provided in kit `ch2/4 - Filesystem/`
- `EXT2BlockGroupDescriptor`: block group metadata (descriptor entry, block bitmap, inode bitmap, inode table start, data block start)
- `EXT2DirectoryEntry`: `{inode(4B), rec_len(2B), name_len(2B), name(padded 4B-aligned)}`; every directory has "." and ".." entries
- `EXT2Inode`: `i_mode` (file/dir flag), `i_size`, `i_blocks` (512B units reserved), `i_block[15]` (12 direct, 13=indirect, 14=doubly-indirect, 15=triply-indirect; 128 ptrs/block @512B blocks ⇒ ~1GB max)
- Design constraints:
  - [x] Boot sector stores only `fs_signature` (customizable); FS params hardcoded via macros
  - [x] Every file/folder has exactly 1 inode; root's parent is itself, root cannot be deleted (no delete-root path exists)
  - [x] Directory is "empty" iff exactly 2 entries exist (self "." + parent ".."); these 2 entries are permanent once created
  - [x] Inode table entry "empty" iff its bitmap bit = 0; dirs use `i_mode = EXT2_S_IFDIR`
  - [x] Unused `i_block` pointers = 0

### 2.5 FS: Initializer (`ext2.c` — only imports `disk.h ext2.h stdint.h stdbool.h string.h`)
- [x] `static EXT2Superblock`/`EXT2BlockGroupDescriptorTable` — in-RAM FS state
- [x] `create_ext2()` — writes `fs_signature` + creates root superblock
- [x] `is_empty_storage()` — compares boot sector vs `fs_signature`; true if mismatch
- [x] `initialize_filesystem_ext2()` — `create_ext2()` if empty, else load superblock (addr 1) + block group descriptor table (addr 2) into RAM
- [x] Call `initialize_filesystem_ext2()` in `kernel_setup()`
- [x] **Bug fixed**: the kit's `GROUPS_COUNT` macro wasn't fully parenthesized, silently shrinking `BLOCKS_PER_GROUP` from 1024→256 wherever expanded together (classic C macro precedence pitfall) — this cut the addressable filesystem from 4MB to 1MB. Fixed by fully parenthesizing; verified `s_blocks_count` now correctly reads 8192 (full 4MB).

### 2.6 FS: CRUD
- [x] Bump `KERNEL_STACK_SIZE` in `kernel-entrypoint.s` from 4 KiB → **2 MiB**
- [x] `struct EXT2DriverRequest __attribute__((packed))`: `{void *buf; char *name; uint8_t name_len; uint32_t parent_inode; uint32_t buffer_size; bool is_directory;}`
- [x] `int8_t read(struct EXT2DriverRequest request)` — target must be a file; checks `buffer_size`; codes: 0 OK / 1 not-file / 2 buf-too-small / 3 not-found / 4 parent-invalid / -1 other
- [x] `int8_t read_directory(struct EXT2DriverRequest *request)` — target must be dir; codes: 0 OK / 1 not-folder / 2 not-found / 3 parent-invalid / -1 other
- [x] `int8_t write(struct EXT2DriverRequest *request)` — reject duplicate (name+type); allocate via **First Fit**; write exactly `ceil(buffer_size/BLOCK_SIZE)` blocks; new `EXT2DirectoryEntry` in parent; **commit** all modified metadata to storage; codes: 0 OK / 1 name-exists / 2 parent-invalid / -1 other
- [x] `int8_t delete(struct EXT2DriverRequest request)` — free blocks + bitmap + parent entry; folder delete requires empty; codes: 0 OK / 1 not-found / 2 folder-not-empty / 3 parent-invalid / -1 other
- [x] Data-integrity testing (`xxd`): block partition order, first/last byte of each block, full per-block byte content — all verified exact
- [x] Cross-tested against kit's `sample-image.bin`: boot-sector signature and BGD-table layout matched exactly, but the reference image's on-disk **superblock** layout does not match the shipped `ext2.h` header (magic field lands at a different byte offset) — consistent with the book's own disclaimer that the sample is "one possible implementation" and may differ. Not treated as a blocking issue since it's the reference that diverges from the given header, not this implementation.
- [x] Use `do-while` for storage-write loops (style tip) — used in `disk.c`'s block-transfer loops
- [x] **Bug fixed**: GCC was free to emit SSE instructions (`movdqu`/`movups`) for struct-by-value copies (e.g. `EXT2DriverRequest`) since nothing disabled them — this freestanding kernel never initializes FPU/SSE state (no CR0/CR4 setup), so any SSE instruction raised `#UD` immediately, cascading into a triple fault. Fixed by adding `-mgeneral-regs-only` to the Makefile's `CFLAGS`.

**Verification**: full CRUD test suite (write/dup-reject/read-match/readdir/dir-delete/nonempty-reject/delete-verify) passes cleanly and idempotently across 3+ consecutive reboots on the same disk, confirmed via headless QEMU + serial-log `[CHECK]` markers (see `src/kernel.c`).

---

## Ch. 3 — Paging, User Mode, Shell

Fixed design: no GDT segmentation (Virtual=Linear); 1 kernel Page Directory; Higher-Half Kernel; 4 KiB page frames; 2-level x86 page table.

### 3.1.1 Data Structure: Page Table (kit: `ch3/1 - Paging/`)
- [ ] `struct PageDirectoryEntryFlag` / `struct PageTableEntryFlag` — 8-bit low flags matching Intel Manual 3A Table 4-5/4-6 exactly (alignment-sensitive — mismatch ⇒ triple fault)
- [ ] `struct PageDirectoryEntry` / `struct PageTableEntry` — flag + `global_page:1` + ignored/PAT bits + `higher_address`(8-bit, bits 39:32) + reserved + `lower_address`(10-bit index); use `uint16_t` bitfields
- [ ] `struct PageDirectory` — `__attribute__((packed, aligned(0x1000)))` (4 KB aligned or triple fault)
- [ ] `struct PageTable` — same packing

### 3.1.2 Higher Half Kernel
- [ ] `linker.ld`: relocate to virtual `0xC0100000` (physical load stays `0x100000`); `AT(ADDR(...) - 0xC0000000)` per section; linker symbols `_linker_kernel_virtual_addr_start/end`, `_linker_kernel_physical_addr_start/end`, `_linker_kernel_stack_top`
- [ ] Temporary Identity Paging entry for frame 0 (removed post-jump, handled by kit assembly)
- [ ] Update `FRAMEBUFFER_MEMORY_OFFSET` → `0xC00B8000`

### 3.1.3 Activate Paging (kit handles in `kernel-entrypoint.s`)
- [ ] CR3 = `&_paging_kernel_page_directory`; CR4 PSE bit; CR0 PG bit; jump to higher-half; remove identity mapping; set up stack; call `kernel_setup()`
- [ ] Verify via debugger: `$eip`/`$esp` in `0xC0000000` region, no triple fault

### 3.1.4 Memory Manager
- [ ] 3.1.4.1 `paging_allocate_user_page_frame(struct PageDirectory*, void *virtual_addr)` — find free frame, mark used, map into given PageDirectory; never touch kernel space
- [ ] 3.1.4.2 `paging_free_user_page_frame(struct PageDirectory*, void *virtual_addr)` — inverse; never touch kernel space
- [ ] 3.1.4.3 `paging_allocate_check(uint32_t amount)` — `ceil(amount/PAGE_FRAME_SIZE) <= free_frames` (tracked via `free_page_frame_count` in `page_manager_state`)
- (Swap Space / Page Fault / Page Replacement sections are conceptual reading only — not required to implement per this edition)

### 3.3 User Mode

#### 3.3.1 External Program: Inserter (host-OS tool, kit: `ch3/3 - User Mode/external-inserter.c`)
- [ ] CLI: `./inserter <file or folder> <parent inode index> <storage>`
- [ ] Makefile `inserter:` target compiling `stdlib/string.c filesystem/ext2.c external/external-inserter.c` → `bin/inserter`
- [ ] Ensure FS code only imports allowed headers (Modular & Reusability — same `ext2.c` reused unmodified between kernel and host inserter, which supplies its own Linux-file-I/O "disk driver")
- [ ] Debug via VS Code "Inserter" launch config if segfaulting

#### 3.3.2 GDT: User & Task State Segment Descriptor
- [ ] `struct TSSEntry` in `interrupt.h`: `{prev_tss; esp0; ss0; unused_register[23];}` packed; `extern struct TSSEntry _interrupt_tss_entry;`
- [ ] `void set_tss_kernel_current_stack(void)` — reads `%ebp`, sets `_interrupt_tss_entry.esp0 = stack_ptr + 8`
- [ ] `struct TSSEntry _interrupt_tss_entry = { .ss0 = GDT_KERNEL_DATA_SEGMENT_SELECTOR };`
- [ ] Extend GDT to 6 entries: Null, Kernel Code, Kernel Data, **User Code** (DPL=3), **User Data** (DPL=3), **TSS** (`segment_low=sizeof(TSSEntry)`, `type_bit=0x9`, `privilege=0`, `valid_bit=1`, `opr_32_bit=1`, `long_mode=0`, `granularity=0`)
- [ ] `void gdt_install_tss(void)` — writes `&_interrupt_tss_entry` into TSS descriptor base fields at runtime
- [ ] `gdt.h` macros: `GDT_USER_CODE_SEGMENT_SELECTOR 0x18`, `GDT_USER_DATA_SEGMENT_SELECTOR 0x20`, `GDT_TSS_SELECTOR 0x28`

#### 3.3.3 Simple User Program
- [ ] `user-shell.c`: minimal payload (e.g. `mov $0xDEADBEEF, %eax`), Flat Binary Executable format
- [ ] `crt0.s`: `global _start; extern main; .text; _start: call main; jmp $`
- [ ] `user-linker.ld`: `ENTRY(_start)`, load at `0x00000000`, `.text` (crt0.o first) / `.data` / `.bss` / `.rodata` all `ALIGN(4)`, `_linker_user_program_end`, `ASSERT(≤ 1 MiB)`
- [ ] Makefile `user-shell:` (assemble crt0, compile `-fno-pie`, link `-melf_i386 --oformat=binary`, `size --target=binary`)
- [ ] Makefile `insert-shell: inserter user-shell` → `./inserter shell 2 storage.bin`

#### 3.3.4 Execute Program
- [ ] `kernel_execute_user_program:` (assembly, `kernel-entrypoint.s`) — set ds/es/fs/gs to user data|0x3; `iret` trick: push user SS, user ESP (`0x400000 - 4`), EFLAGS, user CS (code|0x3), EIP (param), then `iret`
- [ ] `kernel-entrypoint.h`: extern linker symbols; `extern void kernel_execute_user_program(void *virtual_addr);` (one-way jump); `extern void set_tss_register(void);`

#### 3.3.5 Launching User Mode
- [ ] `kernel_setup()` sequence: `load_gdt → pic_remap → initialize_idt → activate_keyboard_interrupt → framebuffer_clear → framebuffer_set_cursor(0,0) → initialize_filesystem_ext2() → gdt_install_tss() → set_tss_register()`
- [ ] `paging_allocate_user_page_frame(&paging_kernel_page_directory, (uint8_t*)0)` — first 4 MiB user space
- [ ] Build `EXT2DriverRequest` for `"shell"` (inode=1, buffer_size=0x100000, name_len=5), `read(request)`
- [ ] `set_tss_kernel_current_stack()` then `kernel_execute_user_program((uint8_t*)0)`
- [ ] Debug matrix: eip stuck on kernel instr → read()/execute call missing; QEMU crash → TSS malformed; triple fault → GDT descriptor corrupted; GP/Page Fault → paging/vmem bug; eip in 0x0–0x100 → success

### 3.4 Shell

#### 3.4.1 System Calls
- [ ] Design: `int 0x30` (IDT gate privilege=3); `eax`=service, `ebx/ecx/edx`=params; **sanitize all user input** — never trust it
- [ ] Syscall table (extend freely):
  | eax | Service | ebx | ecx | edx |
  |---|---|---|---|---|
  | 0 | FS `read()` | ptr `EXT2DriverRequest` | ptr return code | - |
  | 1 | FS `read_directory()` | ptr request | ptr return code | - |
  | 2 | FS `write()` | ptr request | ptr return code | - |
  | 3 | FS `delete()` | ptr request | ptr return code | - |
  | 4 | `getchar()` | ptr char | - | - |
  | 5 | `putchar()` | char value | text color | - |
  | 6 | `puts()` | ptr char buffer | char count | text color |
  | 7 | activate keyboard input | - | - | - |
- [ ] `void syscall(struct InterruptFrame frame)` in `interrupt.c` — switch on `frame.cpu.general.eax`, dispatch to FS/keyboard/framebuffer-backed `puts()`
- [ ] New IDT gate for `int_vector 0x30`, `privilege=0x3` (ring-3 accessible)
- [ ] User-side `void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx)` wrapper — inline asm loads ebx/ecx/edx/eax (eax last, gcc uses it as scratch) then `int $0x30`
- [ ] End-to-end test: read "shell" via syscall 0, loop syscall 7 (activate) + 4/5 (getchar/putchar) to echo typed input

#### 3.4.2 Command Line Interface
- [ ] Debug symbol fix: second executable `shell_elf` (ELF32-i386, same objects, `--oformat=elf32-i386`) purely for gdb, never booted
- [ ] Makefile: compile+link `stdlib/string.c` into shell too; add `shell_elf` link recipe
- [ ] `launch.json` → `customLaunchSetupCommands`: connect `localhost:1234`, `symbol-file kernel`, `add-symbol-file shell_elf`, `set output-radix 16`

#### 3.4.2.2 Shell: Specification (REQUIRED)
- [ ] Ring-3 user program; **REPL**; **output buffering**; **Pipeline** (`|`) support
- [ ] Print **Current Working Directory** each prompt line; initial CWD = Root
- [ ] No requirement to parse relative paths beyond simple handling (e.g. `cd ../folder1/nestedf1/` need not be supported)
- [ ] Built-in commands (UNIX-like):
  - [ ] `cd` — change CWD (incl. `..`)
  - [ ] `ls` — list CWD contents
  - [ ] `mkdir` — create empty folder in CWD
  - [ ] `cat` — print text file to screen (LF newlines)
  - [ ] `cp` — copy a file (**bonus**: folder support)
  - [ ] `rm` — delete a file (**bonus**: folder support)
  - [ ] `mv` — move/rename file or folder
  - [ ] `find` — search file/folder by name **across entire filesystem**
  - [ ] `grep` — search pattern in file or pipeline input, print matching lines (**bonus**: regex)
- [ ] Extra features (splash screen, extra UI/utilities) optional
- [ ] Shell binary must stay < 1 MiB (adjust `user-linker.ld` ASSERT if truly needed)
- [ ] Always `make disk && make insert-shell` after shell code changes

---

## Ch. 4 — Process, Scheduler, Multitasking (Grand Finale)

Read/re-review Paging (esp. address translation & page directory role) first. Ensure Ch. 1–3 are bug-free before starting — debugging difficulty jumps sharply here.

### 4.1.0 Correction: Kit Chapter 1 (apply first!)
- [ ] Replace `intsetup.s` with `ch4/correction/intsetup.s`
- [ ] Replace `struct CPURegister` in `interrupt.h` with corrected version: `{index:{edi,esi}; stack:{ebp,esp}; general:{ebx,edx,ecx,eax}; segment:{gs,fs,es,ds};}` all packed (fixes previous ebp/esp naming bug — only affects `cpu.stack.ebp/esp` usage)
- [ ] Verify OS still boots normally after replacement

### 4.1.1 Multi Virtual Address Space (kit: `ch4/1 - Process/paging.c`)
- [ ] `#define PAGING_DIRECTORY_TABLE_MAX_COUNT 32`
- [ ] `struct PageDirectory* paging_create_new_page_directory(void)` — new dir prefilled with kernel higher-half entry; NULL on failure
- [ ] `bool paging_free_page_directory(struct PageDirectory *page_dir)`
- [ ] `struct PageDirectory* paging_get_current_page_directory_addr(void)` — read CR3
- [ ] `void paging_use_page_directory(struct PageDirectory *page_dir_virtual_addr)` — write CR3 (flushes TLB non-global entries)

### 4.1.2 Process Control Block
- [ ] `struct Context` — full x86 CPU register state (supplement kit's incomplete `CPURegister` with any registers missing for context switch), `eip`, `eflags`, `page_directory_virtual_addr`
- [ ] `typedef enum PROCESS_STATE {...} PROCESS_STATE;` — reference uses 3 states; define to fit scheduler
- [ ] `struct ProcessControlBlock { struct {...} metadata; struct Context context; struct {void *virtual_addr_used[PROCESS_PAGE_FRAME_COUNT_MAX]; uint32_t page_frame_used_count;} memory; };` — metadata ≥ process state + PID
- [ ] Static `_process_list[PROCESS_COUNT_MAX]` array

### 4.1.3 Process Creation (kit: `ch4/1 - Process/process.c`)
- [ ] 4.1.3.1 Virtual Address Space: `paging_create_new_page_directory()`; record allocated memory into PCB; layout — kernel at top `0xC0000000`, call stack near `0xBFFFFFFC` growing down, heap ~`0xBC000000` growing up
- [ ] 4.1.3.2 Load Executable: temporarily switch page directory, FS `read()` executable into new memory, restore original directory; **cancel process creation on read failure**
- [ ] 4.1.3.3 Context Initialization: user data/code selectors @ DPL 3; `eflags |= CPU_EFLAGS_BASE_FLAG | CPU_EFLAGS_FLAG_INTERRUPT_ENABLE`; `eip` = entrypoint; `page_directory_virtual_addr` = new directory
- [ ] 4.1.3.4 Process Metadata & Cleanup: fill PCB metadata, return success/failure (avoid `goto` unless comfortable with cleanup-on-error via call stack)

### 4.1.4 Process: Init — kernel_setup() update
- [ ] Keep existing init sequence (gdt/pic/idt/keyboard/framebuffer/fs/tss)
- [ ] Build `EXT2DriverRequest` for `"shell"`, call `process_create_user_process(request)` (treat shell as init process, like UNIX PID 1)
- [ ] `paging_use_page_directory(_process_list[0].context.page_directory_virtual_addr)` then `kernel_execute_user_program((void*)0x0)`
- [ ] Edit `kernel_execute_user_program` assembly stack-pointer setup to match new Virtual Address Space convention
- [ ] Debug matrix: page fault/invalid opcode/triple fault → vmem/executable-load bug; reaches 0x0 but push/pop fails → stack pointer misaligned with allocation

### 4.2 Scheduler

#### 4.2.1.1 IRQ0 — Timer Interrupt & Scheduler Init
- [ ] `void activate_timer_interrupt(void)` — `cli`; program 8253 PIT (`PIT_MAX_FREQUENCY=1193182`, `PIT_TIMER_FREQUENCY=1000`, counter = max/freq); command byte to port `0x43`; low/high counter bytes to channel-0 data port `0x40`; unmask IRQ0 on PIC1
- [ ] Always PIC-ACK every IRQ0 (else other interrupts block)
- [ ] `scheduler_init()` — all scheduler init state; called once before entering idle "loop"

#### 4.2.1.2 Scheduling Algorithm
- [ ] Implement ≥1 preemptive algorithm (e.g. **Round Robin** or **Priority Scheduling**)
- [ ] `scheduler_switch_to_next_process()` — selects next process per algorithm + triggers Context Switch; called periodically by timer interrupt

#### 4.2.2 Context Switch (kit: `ch4/2 - Scheduler/scheduler.h`)
- [ ] `process_context_switch(ctx)` — **assembly** (`context-switch.s`), mirrors `kernel_execute_user_program()`'s `iret` technique:
  1. Save base address of function arg `ctx`
  2. Set up `iret` stack via push
  3. Load all registers from `ctx`
  4. Cleanup leftover register usage
  5. Jump via `iret`
- [ ] 4.2.2.2: virtual address space switch = just changing CR3 (reuse Process Creation's temp-switch logic); implement full swap sequence in `scheduler_switch_to_next_process()`

#### 4.2.3 Test: Single Process
- [ ] `kernel_setup()`: ...`process_create_user_process(request); scheduler_init(); scheduler_switch_to_next_process();`
- [ ] Shell should run normally as in Ch. 3, with timer interrupts firing (PIC-ACK only, no switch logic yet at first pass)
- [ ] Then add context save/switch on every IRQ0 so process switching is live
- [ ] Run >10 seconds with **no errors** before proceeding to Multitasking

### 4.3 Multitasking

#### 4.3.1 Process Entrypoint & Exit
- [ ] Design `exit` syscall — terminates the calling process (assign it a syscall number, e.g. per your table)
- [ ] Modify `crt0.s`: after `call main`, `mov ebx, eax` (exit code) then invoke `exit` syscall (`mov eax, <num>; int $0x30`)

#### 4.3.2 Process Management & Command
- [ ] Syscalls: create new user process; terminate process by PID; get process info
- [ ] Shell commands using the above:
  - [ ] `exec` — run a program from the filesystem (supports relative `./a/b/c` or global `/bin/`-style paths)
  - [ ] `ps` — show process info (≥ name + PID) for all processes on system
  - [ ] `kill` — terminate process by PID

#### 4.3.3 Clock (multitasking demo)
- [ ] Implement CMOS driver + syscall(s) to read RTC
- [ ] `clock` user program — prints HH:MM:SS to a fixed screen location, updates every ≤1s, runs as a background user process alongside shell
- [ ] Must be terminable and re-launchable via `exec` without breaking system state

#### 4.3.4 External Application
- [ ] ≥1 external app launched via `exec`, satisfying:
  - [ ] Uses a proper linker + entrypoint
  - [ ] Uses existing/new syscalls as needed
  - [ ] **Not hardcoded into shell** — `exec` must accept the app name as an argument
  - [ ] At least one app should be a "Hello World!"

#### 4.3.5 Bonus: Environment Variables
- [ ] Implement a `PATH`-like mechanism: stores absolute paths to ≥1 directory; add/remove entries via a command (e.g. `export`); file in any `PATH` dir runnable by bare name from anywhere; **persists across reboot**

#### 4.3.6 Bonus: Separate Shell Commands
- [ ] Rebuild built-in commands (3.4.2.2) as standalone executables invoked via ELF-like flat binaries — own `linker.ld` (`ENTRY(_start)`, load at `0x0`, `.text/.rodata` 4K-aligned, `.data`, `.bss`, discard `.comment/.note*/.eh_frame*`) and own `crt0` (clears registers, calls `main`, exits via syscall)
- [ ] Ensure commands don't become zombie/orphan processes after execution
- [ ] Debug focus if broken only when separated: linker/crt0 correctness, arg passing, request sync between shell and command

#### 4.3.7 Grand Finale
- [ ] Add root-level `README.md` with: OS name as H1 heading, list of contributors
- [ ] Creative README content encouraged
- [ ] (Course-specific: repo access/visibility changes handled outside this checklist)

---

## Notes / Non-Tasks (context only, not implementation items)
- Ch. 0.4, Ch. 2 "Extras: Hardware" (DMA, Hot Plug, Digital Forensic), Ch. 3 "Extras: Security" (Code Injection, Hooking), Ch. 4 "Extras: OS" (Kernel/Bare Metal/OS history) are conceptual reading sections with no required deliverable.
- Swap Space / Page Fault / Page Replacement (§3.2) are explained conceptually but **not required to be implemented** in this edition — just don't accidentally rely on features that would need them (book warns low-RAM test runs will expose broken swap-dependent designs, but swap itself isn't a listed task).
- "IF2130 biasanya diambil bersamaan dengan IF2211 Strategi Algoritma" — EXT2 is explicitly a Tree/Graph structure; `find`/traversal implementations can lean on standard tree algorithms (DFS/BFS).
