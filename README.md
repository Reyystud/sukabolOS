![Credit: 朧月](mascot.jpg)

# sukabolOS — IF2130 Sistem Operasi 2026/2027

Implementasi *bare-metal Operating System* x86 32-bit (IA-32) berbasis **Multiboot1** (boot via GRUB legacy) untuk Tugas Besar IF2130 Sistem Operasi.

## Daftar Isi

- [Fitur yang Dibuat](#fitur-yang-dibuat)
- [Struktur Repositori](#struktur-repositori)
- [Prasyarat](#prasyarat)
- [Cara Run](#cara-run)
- [Debugging dengan GDB](#debugging-dengan-gdb)
- [Troubleshooting](#troubleshooting)
- [Maskot](#maskot)

---

## Fitur yang Dibuat

Detail lengkap ada di [PROGRESS.md](PROGRESS.md).

| Chapter | Topik | Status |
|---|---|---|
| Ch. 0 | Toolchain, Kernel, GDT | ✅ Selesai |
| Ch. 1 | Framebuffer, Interrupt (IDT/PIC), Keyboard, Serial | ✅ Selesai |
| Ch. 2 | File System EXT2 (disk driver ATA + EXT2) | ✅ Selesai |
| Ch. 3 | Paging (higher-half kernel), User Mode, Shell | 🟨 3.1 Paging kode selesai (belum diuji boot) |
| Ch. 4 | Process, Scheduler, Multitasking | ⬜ Belum dimulai |

---

## Struktur Repositori

```text
.
├── bin/                     # Output build (kernel, OS2025.iso, storage.bin)
├── other/grub1              # Bootloader GRUB legacy untuk ISO
├── src/
│   ├── cpu/                 # GDT, IDT, port I/O, interrupt stub (.s)
│   ├── driver/              # Framebuffer, keyboard, serial, disk (ATA)
│   ├── filesystem/          # EXT2
│   ├── memory/              # Paging / memory manager
│   ├── stdlib/              # string.c
│   ├── header/              # Seluruh header (.h)
│   ├── kernel.c             # Entrypoint C kernel
│   ├── kernel-entrypoint.s  # Entrypoint assembly + Multiboot header
│   ├── linker.ld            # Linker script
│   └── menu.lst             # Konfigurasi menu GRUB
├── Makefile
├── PROGRESS.md
└── README.md
```

---

## Prasyarat

Dibangun dan diuji di Linux (x86_64). Pastikan tool berikut terpasang dan ada di `PATH`:

| Tool | Fungsi | Contoh paket |
|---|---|---|
| `gcc` (dengan dukungan `-m32`) | Kompilasi kernel C | `gcc`, `gcc-multilib` |
| `nasm` | Assembler | `nasm` |
| `ld` (binutils) | Linker (`-melf_i386`) | `binutils` |
| `genisoimage` | Membuat ISO bootable | `cdrtools` / `genisoimage` |
| `qemu-system-i386`, `qemu-img` | Emulator & pembuat disk image | `qemu` |
| `gdb` *(opsional)* | Debugging | `gdb` |

Cek cepat:

```bash
gcc -m32 --version && nasm -v && ld -v && genisoimage --version && qemu-system-i386 --version
```

---

## Cara Run

Jalankan semua perintah dari root repositori.

### 1. Buat disk image (sekali saja)

Kernel membutuhkan disk 4 MB sebagai penyimpanan EXT2. Jika `bin/storage.bin` belum ada:

```bash
mkdir -p bin
make disk
```

Ini menjalankan `qemu-img create -f raw bin/storage.bin 4M`.

### 2. Build kernel & ISO

```bash
make build      # sama dengan `make all` / `make iso`
```

Hasil:
- `bin/kernel` — ELF32 kernel
- `bin/OS2025.iso` — ISO bootable (GRUB legacy + kernel)

### 3. Jalankan di QEMU

Ada dua cara:

**a. Lewat Makefile**

```bash
make run
```

> ⚠️ Target ini memakai flag `-s -S`: QEMU **berhenti di awal dan menunggu GDB** tersambung di port 1234. Jendela QEMU akan tampak kosong/beku sampai kamu menyambungkan GDB lalu `continue` (lihat [Debugging dengan GDB](#debugging-dengan-gdb)).

**b. Langsung (tanpa debugger, langsung boot)**

```bash
qemu-system-i386 \
  -cdrom bin/OS2025.iso \
  -drive file=bin/storage.bin,format=raw,if=ide,index=0,media=disk
```

### 4. Headless / cek log serial (opsional)

```bash
make debug-run
cat bin/out.log
```

Menjalankan QEMU tanpa tampilan selama 8 detik dan menyimpan output serial ke `bin/out.log`.

### 5. Bersihkan hasil build

```bash
make clean
```

### Ringkasan satu baris

```bash
mkdir -p bin && make disk && make build && qemu-system-i386 -cdrom bin/OS2025.iso -drive file=bin/storage.bin,format=raw,if=ide,index=0,media=disk
```

---

## Debugging dengan GDB

1. Terminal 1 — jalankan QEMU dalam mode beku:
   ```bash
   make run
   ```
2. Terminal 2 — sambungkan GDB:
   ```bash
   gdb bin/kernel
   ```
   ```text
   (gdb) target remote localhost:1234
   (gdb) break kernel_setup
   (gdb) continue
   ```
3. Periksa struktur, misalnya GDT:
   ```text
   (gdb) print _gdt_gdtr
   (gdb) print global_descriptor_table
   ```

---

## Troubleshooting

| Masalah | Solusi |
|---|---|
| QEMU blank setelah `make run` | Normal, QEMU menunggu GDB. Sambungkan GDB + `continue`, atau pakai perintah QEMU langsung (cara 3b). |
| `make: ... storage.bin: No such file` | Jalankan `make disk` dulu (pastikan folder `bin/` ada). |
| `gcc: error: unrecognized -m32` / header 32-bit hilang | Pasang `gcc-multilib` (atau toolchain 32-bit setara). |
| `genisoimage: command not found` | Pasang `cdrtools` / `genisoimage`. |
| Kernel reboot terus / triple fault | Cek `bin/out.log` lewat `make debug-run`. |

---

## Maskot

![Maskot kelompok](mascot.jpg)

*Credit: 朧月*
