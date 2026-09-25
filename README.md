![Credit: 朧月](mascot.jpg)

# Template Dasar IF2130 Sistem Operasi - 2026/2027
Template dasar untuk Tugas Besar IF2130 - Sistem Operasi 2026/2027

## README Specification

Pada file README, minimal kalian harus memiliki hal berikut:

- Nama Kelompok
- Daftar Isi
- Cara Run
- Fitur yang Dibuat
- Maskot Kelompok Kalian


# IF2230 Operating Systems 2026 - Bare-Metal Kernel

Repositori ini berisi implementasi *bare-metal Operating System* x86 32-bit (IA-32) berbasis **Multiboot1** spesifikasi yang dikembangkan untuk memenuhi tugas mata kuliah **IF2230 Sistem Operasi**.

---

## Fitur & Modul Terimplementasi (Chapter 0)

* **Kernel Entrypoint & Multiboot Header**: Konfigurasi header Multiboot1 (`0x1BADB002`) pada assembly (`src/kernel-entrypoint.s`) untuk booting via GRUB[cite: 1].
* **Global Descriptor Table (GDT)**: Implementasi Flat Memory Model (Ring 0) dengan 3 entri utama[cite: 1]:
  * `Null Descriptor` (`0x00`)[cite: 1]
  * `Kernel Code Segment` (`0x08`) — Base: `0x0`, Limit: `4GB`, Executable/Read, Ring 0[cite: 1]
  * `Kernel Data Segment` (`0x10`) — Base: `0x0`, Limit: `4GB`, Read/Write, Ring 0[cite: 1]
* **Assembly Helper (`load_gdt`)**: Pemuatan struktur GDTR ke CPU menggunakan instruksi assembly `lgdt` dan melakukan *far reload* segmen data[cite: 1].
* **ISO Bootable Generation**: Pembentukan *image* bootable ISO (`OS2026.iso`) menggunakan `genisoimage` dan `grub1`[cite: 1].

---

## Struktur Repositori

```text
.
├── bin/                    # Output kompilasi binary (.o, .iso, kernel)
├── iso/                    # Struktur file/folder untuk pembuatan ISO GRUB
├── src/
│   ├── header/             # Header files (.h)
│   │   └── gdt.h          # Struktur data & definisi segmen GDT
│   ├── gdt.c               # Inisialisasi & pemuatan Global Descriptor Table
│   ├── kernel.c            # Entrypoint C kernel & pengujian sederhana
│   └── kernel-entrypoint.s # Entrypoint assembly bare-metal
├── Makefile                # Skrip kompilasi & pembentukan ISO
└── README.md

```

---

## Prasyarat Sistem

Proyek ini dibangun dan diuji pada sistem **Arch Linux** dengan dependensi berikut:

* **GCC** (x86 32-bit target / `-m32`)


* **NASM** / **GNU Assembler**

* **QEMU** (`qemu-system-i386`)


* **GDB** (`x86_64-pc-linux-gnu` atau `i686-elf-gdb`)


* **cdrtools** (`genisoimage`)



---

## Panduan Kompilasi & Menjalankan OS

### 1. Build Kernel & Generate ISO

Untuk mengompilasi seluruh source code dan membentuk file ISO bootable:

```bash
make iso

```

*Output ISO akan dihasilkan di `bin/OS2026.iso`.*

### 2. Menjalankan OS di QEMU

Untuk menjalankan OS secara langsung di emulator QEMU:

```bash
qemu-system-i386 -cdrom bin/OS2025.iso

```

### 3. Debugging dengan GDB & QEMU

Untuk melakukan *remote debugging* GDT dan register CPU:

1. Jalankan QEMU dalam mode terbekukan (`-S`) dengan port GDB (`-s` / port `1234`):
```bash
qemu-system-i386 -s -S -cdrom bin/OS2025.iso

```


2. Buka terminal baru dan hubungkan GDB ke kernel:
```bash
gdb bin/kernel

```


3. Sambungkan ke remote target QEMU:
```text
(gdb) target remote localhost:1234
(gdb) break kernel_setup
(gdb) continue

```


4. Periksa isi struktur GDT:
```text
(gdb) print _gdt_gdtr
(gdb) print global_descriptor_table

```
