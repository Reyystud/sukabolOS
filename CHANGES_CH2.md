# Ringkasan Perubahan — Chapter 2 (Disk Driver + EXT2 Filesystem + CRUD)

Dokumen ini menjelaskan semua yang ditambahkan/diubah untuk menyelesaikan Chapter 2 dari `REQUIREMENTS.md`, plus 3 bug nyata yang ditemukan dan diperbaiki di sepanjang jalan.

---

## 1. File baru

### `src/driver/serial.c` + `src/header/driver/serial.h` (35+30 baris)
Driver COM1 (serial port) minimal — bukan bagian dari spesifikasi buku, ini murni alat bantu verifikasi. Karena tidak ada sesi GDB interaktif saat implementasi berjalan otomatis, semua checkpoint di-print via `serial_write("[CHECK] ...")` ke port serial, lalu di-grep dari log hasil boot QEMU headless (`timeout ... qemu-system-i386 ... -serial file:bin/out.log -display none`). Fungsinya: `serial_init()`, `serial_write_char()`, `serial_write()`, `serial_write_hex32()`.

### `src/driver/disk.c` + `src/header/driver/disk.h` (69+50 baris)
Driver ATA PIO (Programmed I/O) untuk disk `storage.bin` (4MB, dipasang sebagai IDE primary master via port `0x1F0`-`0x1F7`). Isinya persis sesuai `.kit/ch2/disk.h`:
- `ATA_busy_wait()` / `ATA_DRQ_wait()` — polling status register.
- `read_blocks(void *ptr, uint32_t lba, uint8_t block_count)` — baca N block (512 byte/block) via `inw`.
- `write_blocks(const void *ptr, uint32_t lba, uint8_t block_count)` — tulis N block via `outw`.

### `src/filesystem/ext2.c` + `src/header/filesystem/ext2.h` (657+397 baris)
Inti pekerjaan Chapter 2 — implementasi filesystem **EXT2 - IF2130 Edition** lengkap:
- **Struct-struct** (`EXT2DriverRequest`, `EXT2Superblock`, `EXT2BlockGroupDescriptor(Table)`, `EXT2Inode`, `EXT2DirectoryEntry`) — disalin persis dari `.kit/ch2/ext2.h` supaya kompatibel dengan `sample-image.bin` dan tool eksternal nanti di Chapter 3.
- **Inisialisasi**: `create_ext2()` (bikin filesystem baru: tulis `fs_signature`, hitung layout 8 block group × 1024 block, root directory), `is_empty_storage()`, `initialize_filesystem_ext2()`.
- **Alokasi**: `allocate_node()`/`allocate_one_block()` (First-Fit), `allocate_node_blocks()` (direct + single-indirect + double-indirect block pointer, sesuai batas "hanya sampai doubly indirect" dari spek), `deallocate_node()`/`deallocate_blocks()`/`deallocate_block()` (rekursif per depth, dengan caching bitmap biar tidak baca-tulis berulang).
- **CRUD**: `read()`, `read_directory()`, `write()`, `delete()` — lengkap dengan kode error persis sesuai dokumentasi header (mis. `write()`: 0=sukses, 1=nama sudah ada, 2=parent invalid).
- Helper directory-entry (`get_entry_name`, `get_directory_entry`, `get_next_directory_entry`, `get_entry_record_len`, `insert_directory_entry`, dst.) yang menjaga invariant "entry terakhir dalam 1 block directory selalu meng-extend sampai akhir block" — supaya penyisipan entry baru & deteksi folder kosong konsisten.

---

## 2. File yang diubah

| File | Perubahan |
|---|---|
| `src/header/cpu/portio.h`, `src/cpu/portio.c` | Tambah `outw()`/`inw()` (port I/O 16-bit) — dibutuhkan ATA PIO karena transfer data disk pakai word (2 byte), bukan byte. |
| `src/header/stdlib/string.h`, `src/stdlib/string.c` | Tambah `strlen()` dan `strcmp()`. Juga **`string.c` akhirnya di-compile ke kernel** — sebelumnya file ini sudah ada tapi tidak pernah dimasukkan ke `Makefile`, jadi `memset`/`memcpy`/`memcmp` sebenarnya belum pernah ikut ter-link. |
| `src/kernel-entrypoint.s` | `KERNEL_STACK_SIZE` dinaikkan dari 4 KiB → **2 MiB**, sesuai instruksi buku (CRUD butuh stack lebih besar untuk traversal indirect block). |
| `src/kernel.c` | Tambah pemanggilan `serial_init()`, `initialize_filesystem_ext2()`, dan satu blok test CRUD lengkap (lihat bagian Verifikasi di bawah). Blok test disk-RW mentah yang sempat ditambahkan sebelumnya dihapus lagi karena sudah digantikan test level-filesystem. |
| `Makefile` | Tambah target `disk:` (bikin `storage.bin` 4MB) dan `debug-run:` (boot headless + capture serial log ke `bin/out.log`), tambah semua file objek baru ke daftar compile+link, tambah flag `-drive file=...storage.bin...` ke `run:`/`debug-run:`, dan **tambah flag compiler `-mgeneral-regs-only`** (lihat bug #2 di bawah — ini krusial). |
| `.vscode/tasks.json` | Tambah flag `-drive file=storage.bin,...` ke command "Launch QEMU" biar debugging interaktif user juga ikut pakai disk. |

---

## 3. Tiga bug nyata yang ditemukan & diperbaiki

Ini bukan sekadar "ngikutin spek" — ketiganya adalah bug yang benar-benar bikin sistem gagal/crash saat ditest, ditemukan lewat verifikasi headless (grep serial log + `objdump` + `qemu -d int`).

### Bug 1 — Race condition di ATA disk driver
`write_blocks()`/`read_blocks()` langsung `return` begitu transfer word terakhir selesai, padahal drive masih sempat sibuk (BSY) memproses commit internal sesaat setelahnya. Akibatnya, kalau ada command disk lain langsung menyusul, dia bisa hang menunggu `ATA_busy_wait()` yang tidak pernah clear. **Perbaikan**: tambah satu `ATA_busy_wait()` lagi di akhir kedua fungsi, setelah loop transfer selesai.

### Bug 2 — GCC diam-diam pakai instruksi SSE
Ini yang paling tersembunyi. Saat memanggil `read()` dengan struct `EXT2DriverRequest` sebagai parameter by-value, GCC memilih meng-compile penyalinan struct itu pakai instruksi SSE (`movdqu`/`movups`, operasi 128-bit). Masalahnya: kernel bare-metal ini **tidak pernah menginisialisasi state FPU/SSE** (tidak ada setup `CR0`/`CR4` untuk itu), jadi begitu CPU coba eksekusi instruksi SSE tadi ⇒ langsung `#UD` (Invalid Opcode) ⇒ exception handler ikut gagal (belum ada IDT proper) ⇒ triple fault ⇒ QEMU reset. Ditemukan lewat `objdump -d` pada alamat yang di-log `qemu -d int` sebagai titik crash. **Perbaikan**: tambah `-mgeneral-regs-only` ke `CFLAGS` supaya GCC dilarang total memakai SSE/MMX/x87 di kode kernel manapun — ini flag standar yang seharusnya memang selalu ada di kernel freestanding, kebetulan baru "ketahuan hilang" sekarang karena baru sekarang ada struct yang cukup besar untuk memicu optimisasi itu.

### Bug 3 — Macro `GROUPS_COUNT` dari kit sendiri salah presedensi
```c
#define GROUPS_COUNT (BLOCK_SIZE / sizeof(struct EXT2BlockGroupDescriptor)) / 2u
```
Macro ini TIDAK dibungkus tanda kurung penuh. Kalau dipakai sendirian, hasilnya benar (=8). Tapi begitu dipakai di dalam ekspresi lain seperti `BLOCKS_PER_GROUP = DISK_SPACE/BLOCK_SIZE/GROUPS_COUNT`, hasil substitusi teksnya jadi salah urutan operasi, dan `BLOCKS_PER_GROUP` yang seharusnya 1024 malah jadi 256 — filesystem cuma "melihat" 1MB dari 4MB disk yang sebenarnya! Dikonfirmasi lewat program C kecil terpisah yang mereproduksi bug-nya persis. **Perbaikan**: bungkus seluruh macro dengan kurung tambahan (`((...)/2u)`). Setelah fix, `s_blocks_count` di superblock benar menunjukkan 8192 blok (4MB penuh).

---

## 4. Verifikasi

Karena tidak ada sesi GDB interaktif selama coding otomatis, korektnesitas dibuktikan lewat serangkaian checkpoint `[CHECK] ...` yang di-print ke serial port dan di-grep dari `bin/out.log` setelah boot QEMU headless (`make debug-run`). Urutan test CRUD lengkap yang tertanam di `kernel_setup()`:

1. `WRITE_OK` — tulis file `test.txt` (600 byte, melewati batas 1 block).
2. `DUP_REJECT_OK` — tulis nama sama lagi → ditolak (kode error 1).
3. `READ_MATCH_OK` — baca balik, `memcmp` cocok byte-per-byte.
4. `READDIR_OK` — listing root berisi `.`, `..`, dan `test.txt`.
5. `DIR_DELETE_OK` — bikin folder kosong `sub`, hapus → sukses.
6. `NONEMPTY_REJECT_OK` — bikin folder `sub2` berisi file, coba hapus → ditolak (kode error 2, folder tidak kosong).
7. `DELETE_VERIFY_OK` — hapus `test.txt`, baca lagi → *not found* (kode error 3).

Semua lolos **konsisten di 3+ kali reboot berturut-turut** memakai disk yang sama (tanpa dibuat ulang), membuktikan filesystem persisten dengan benar lewat siklus create → reboot → load. Test juga dibuat idempotent (membersihkan diri sendiri) supaya bisa diulang tanpa perlu disk baru tiap kali.

Selain itu, dilakukan pengecekan manual byte-level pakai `xxd` (posisi awal/akhir tiap block cocok persis dengan pola yang ditulis) dan cross-test terhadap `sample-image.bin` bawaan kit (signature boot sector & layout block-group-descriptor cocok persis; layout superblock referensi ternyata beda dari header yang dibagikan — sesuai catatan buku sendiri bahwa sample itu "salah satu contoh implementasi" dan boleh berbeda).

---

## 5. Yang belum disentuh

Chapter 3 (paging, higher-half kernel, user mode, shell, syscall) dan Chapter 4 (process control block, scheduler, context switch, multitasking) — dari `REQUIREMENTS.md` — belum dikerjakan, menunggu review Chapter 2 ini terlebih dahulu.
