#include <stdint.h>
#include <stdbool.h>
#include "header/cpu/gdt.h"
#include "header/cpu/idt.h"
#include "header/driver/framebuffer.h"
#include "header/driver/keyboard.h"
#include "header/driver/serial.h"
#include "header/driver/disk.h"
#include "header/filesystem/ext2.h"
#include "header/stdlib/string.h"
#include "header/kernel-entrypoint.h"
#include "header/memory/paging.h"

// CP437 box-drawing glyphs (double-line). These must stay as raw byte values,
// not literal Unicode characters - VGA text mode expects single-byte CP437
// codes, not UTF-8 multi-byte sequences.
#define BOX_TL ((char) 0xC9) // top-left    ╔
#define BOX_TR ((char) 0xBB) // top-right   ╗
#define BOX_BL ((char) 0xC8) // bottom-left ╚
#define BOX_BR ((char) 0xBC) // bottom-right╝
#define BOX_H  ((char) 0xCD) // horizontal  ═
#define BOX_V  ((char) 0xBA) // vertical    ║
#define BOX_LT ((char) 0xCC) // left tee    ╠
#define BOX_RT ((char) 0xB9) // right tee   ╣

static void print_at(uint8_t row, uint8_t col, const char *s, uint8_t fg, uint8_t bg) {
    for (int i = 0; s[i] != '\0'; i++) framebuffer_write(row, col + i, s[i], fg, bg);
}

static void print_centered(uint8_t row, const char *s, uint8_t fg, uint8_t bg) {
    int len = 0;
    while (s[len] != '\0') len++;
    int col = (FRAMEBUFFER_WIDTH - len) / 2;
    if (col < 1) col = 1;
    print_at(row, (uint8_t) col, s, fg, bg);
}

static void draw_box_line(uint8_t row, char left, char fill, char right, uint8_t fg, uint8_t bg) {
    framebuffer_write(row, 0, left, fg, bg);
    for (uint8_t c = 1; c < FRAMEBUFFER_WIDTH - 1; c++) framebuffer_write(row, c, fill, fg, bg);
    framebuffer_write(row, FRAMEBUFFER_WIDTH - 1, right, fg, bg);
}

static void draw_box_blank_row(uint8_t row, uint8_t fg, uint8_t bg) {
    draw_box_line(row, BOX_V, ' ', BOX_V, fg, bg);
}

// Draws the boot dashboard: bordered title panel + subsystem status panel.
// fs_freshly_created reflects whether this boot created a brand new EXT2
// filesystem or mounted an existing one, so the status line is honest about
// what actually happened rather than a static decoration.
static void draw_dashboard(bool fs_freshly_created) {
    const uint8_t border_fg = COLOR_LIGHT_CYAN;
    const uint8_t bg = COLOR_BLACK;

    framebuffer_clear();

    draw_box_line(0, BOX_TL, BOX_H, BOX_TR, border_fg, bg);
    for (uint8_t row = 1; row <= 4; row++) draw_box_blank_row(row, border_fg, bg);
    draw_box_line(5, BOX_LT, BOX_H, BOX_RT, border_fg, bg);
    for (uint8_t row = 6; row <= 9; row++) draw_box_blank_row(row, border_fg, bg);
    draw_box_line(10, BOX_BL, BOX_H, BOX_BR, border_fg, bg);

    print_centered(2, "s u k a b o l O S", COLOR_WHITE, bg);
    print_centered(3, "A Bare-Metal x86 Operating System", COLOR_LIGHT_GRAY, bg);

    print_at(6, 2, "System Status", COLOR_LIGHT_BROWN, bg);

    print_at(7, 4, "[OK]", COLOR_LIGHT_GREEN, bg);
    print_at(7, 9, "Global Descriptor Table", COLOR_LIGHT_GRAY, bg);
    print_at(7, 42, "[OK]", COLOR_LIGHT_GREEN, bg);
    print_at(7, 47, "Interrupt Table & PIC", COLOR_LIGHT_GRAY, bg);

    print_at(8, 4, "[OK]", COLOR_LIGHT_GREEN, bg);
    print_at(8, 9, fs_freshly_created ? "EXT2 Filesystem (created)" : "EXT2 Filesystem (mounted)", COLOR_LIGHT_GRAY, bg);
    print_at(8, 42, "[OK]", COLOR_LIGHT_GREEN, bg);
    print_at(8, 47, "PS/2 Keyboard Driver", COLOR_LIGHT_GRAY, bg);

    print_at(12, 0, "sukabolOS> ", COLOR_LIGHT_GREEN, bg);
}

void kernel_setup(void) {
    uint32_t a;
    uint32_t volatile b = 0x0000BABE;
    __asm__("mov $0xCAFE0000, %0" : "=r"(a));
    bool fs_freshly_created = false;

    serial_init();
    serial_write("[CHECK] BOOT_ENTRY\n");

    // Ch.3 Step 1: Paging test sequence
    {
        uint32_t cr0, esp_now;
        __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
        __asm__ volatile("mov %%esp, %0" : "=r"(esp_now));
        bool high_half = ((uint32_t) &kernel_setup >= KERNEL_VIRTUAL_BASE) && esp_now >= KERNEL_VIRTUAL_BASE;
        serial_write((cr0 & 0x80000000u) && high_half ? "[CHECK] PAGING_ON\n" : "[CHECK] PAGING_FAIL\n");

        struct PageDirectory *pd = &_paging_kernel_page_directory;
        uint32_t free_before = page_manager_state.free_page_frame_count;

        serial_write(paging_allocate_check(PAGE_FRAME_SIZE) && !paging_allocate_check(SYSTEM_MEMORY_MB << 20)
            ? "[CHECK] PAGING_CHECK_OK\n" : "[CHECK] PAGING_CHECK_FAIL\n");

        bool ok = paging_allocate_user_page_frame(pd, (void *) 0x0);
        if (ok) {
            volatile uint32_t *user_ptr = (volatile uint32_t *) 0x0;
            *user_ptr = 0xDEADBEEF;
            ok = (*user_ptr == 0xDEADBEEF);
        }
        serial_write(ok ? "[CHECK] ALLOC_OK\n" : "[CHECK] ALLOC_FAIL\n");
        // PT frame + data frame consumed
        serial_write(free_before - page_manager_state.free_page_frame_count == 2
            ? "[CHECK] ALLOC_COUNT_OK\n" : "[CHECK] ALLOC_COUNT_FAIL\n");

        serial_write(!paging_allocate_user_page_frame(pd, (void *) 0x0)
            ? "[CHECK] DOUBLE_ALLOC_REJECT_OK\n" : "[CHECK] DOUBLE_ALLOC_REJECT_FAIL\n");
        serial_write(!paging_allocate_user_page_frame(pd, (void *) KERNEL_VIRTUAL_BASE)
            ? "[CHECK] KERNEL_ALLOC_REJECT_OK\n" : "[CHECK] KERNEL_ALLOC_REJECT_FAIL\n");

        bool freed = paging_free_user_page_frame(pd, (void *) 0x0);
        serial_write(freed && page_manager_state.free_page_frame_count == free_before - 1
            ? "[CHECK] FREE_OK\n" : "[CHECK] FREE_FAIL\n");
        serial_write(!paging_free_user_page_frame(pd, (void *) 0x0)
            ? "[CHECK] DOUBLE_FREE_REJECT_OK\n" : "[CHECK] DOUBLE_FREE_REJECT_FAIL\n");
    }

    // Ch.2 Step 5: EXT2 filesystem initializer
    {
        bool was_empty = is_empty_storage();
        fs_freshly_created = was_empty;
        initialize_filesystem_ext2();
        serial_write(was_empty ? "[CHECK] FS_CREATED\n" : "[CHECK] FS_LOADED\n");

        struct BlockBuffer sb_raw;
        read_blocks(&sb_raw, 1, 1);
        struct EXT2Superblock sb_check;
        memcpy(&sb_check, sb_raw.buf, sizeof(sb_check));
        serial_write("[CHECK] FS_INIT_OK magic=0x");
        serial_write_hex32(sb_check.s_magic);
        serial_write("\n");
    }

    // Ch.2 Step 6: EXT2 CRUD test sequence
    {
        // write()/allocate_node_blocks() copy data in whole BLOCK_SIZE chunks,
        // so the source buffer must be block-aligned in size even though only
        // the first 600 bytes are meaningful test data (avoids an out-of-bounds
        // read on the last partial block).
        uint8_t pattern[1024];
        memset(pattern, 0, sizeof(pattern));
        for (int i = 0; i < 600; i++) pattern[i] = (uint8_t) (i & 0xFF);

        struct EXT2DriverRequest req_write = {
            .buf = pattern, .name = "test.txt", .name_len = 8,
            .parent_inode = 1, .buffer_size = 600, .is_directory = false,
        };
        int8_t rc = write(&req_write);
        serial_write(rc == 0 ? "[CHECK] WRITE_OK\n" : "[CHECK] WRITE_FAIL\n");

        rc = write(&req_write);
        serial_write(rc == 1 ? "[CHECK] DUP_REJECT_OK\n" : "[CHECK] DUP_REJECT_FAIL\n");

        uint8_t readback[1024];
        memset(readback, 0, sizeof(readback));
        struct EXT2DriverRequest req_read = {
            .buf = readback, .name = "test.txt", .name_len = 8,
            .parent_inode = 1, .buffer_size = sizeof(readback), .is_directory = false,
        };
        rc = read(req_read);
        bool match = (rc == 0) && (memcmp(pattern, readback, 600) == 0);
        serial_write(match ? "[CHECK] READ_MATCH_OK\n" : "[CHECK] READ_MATCH_FAIL\n");

        struct BlockBuffer root_block;
        struct EXT2DriverRequest req_readdir = {
            .buf = &root_block, .name = ".", .name_len = 1,
            .parent_inode = 1, .buffer_size = sizeof(root_block), .is_directory = true,
        };
        // "." under root refers to root itself - use it to fetch root's own listing.
        rc = read_directory(&req_readdir);
        bool has_self = false, has_parent = false, has_test = false;
        if (rc == 0) {
            uint32_t offset = 0;
            while (offset < BLOCK_SIZE) {
                struct EXT2DirectoryEntry *entry = get_directory_entry(&root_block, offset);
                if (entry->inode != 0) {
                    char *name = get_entry_name(entry);
                    if (entry->name_len == 1 && name[0] == '.') has_self = true;
                    else if (entry->name_len == 2 && name[0] == '.' && name[1] == '.') has_parent = true;
                    else if (entry->name_len == 8 && memcmp(name, "test.txt", 8) == 0) has_test = true;
                }
                if (entry->rec_len == 0) break;
                offset += entry->rec_len;
            }
        }
        serial_write((has_self && has_parent && has_test) ? "[CHECK] READDIR_OK\n" : "[CHECK] READDIR_FAIL\n");

        struct EXT2DriverRequest req_sub = {
            .buf = NULL, .name = "sub", .name_len = 3,
            .parent_inode = 1, .buffer_size = 0, .is_directory = true,
        };
        rc = write(&req_sub);
        int8_t rc_del_sub = -1;
        if (rc == 0) {
            struct EXT2DriverRequest del_sub = req_sub;
            rc_del_sub = delete(del_sub);
        }
        serial_write(rc_del_sub == 0 ? "[CHECK] DIR_DELETE_OK\n" : "[CHECK] DIR_DELETE_FAIL\n");

        struct EXT2DriverRequest req_sub2 = {
            .buf = NULL, .name = "sub2", .name_len = 4,
            .parent_inode = 1, .buffer_size = 0, .is_directory = true,
        };
        // write() may legitimately return "already exists" here if this test
        // ran before on the same disk and left sub2 behind - either way, look
        // it up via read_directory() so the test stays idempotent across reboots.
        write(&req_sub2);

        struct BlockBuffer sub2_block;
        struct EXT2DriverRequest rd_sub2 = {
            .buf = &sub2_block, .name = "sub2", .name_len = 4,
            .parent_inode = 1, .buffer_size = sizeof(sub2_block), .is_directory = true,
        };
        int8_t rc_nonempty_reject = -1;
        uint32_t sub2_inode = 0;
        // Block-aligned buffer, same reasoning as `pattern` above - write()
        // copies data in whole BLOCK_SIZE chunks regardless of buffer_size.
        static uint8_t file_data[BLOCK_SIZE] = {'h', 'i'};
        struct EXT2DriverRequest req_f = {
            .buf = file_data, .name = "f.txt", .name_len = 5,
            .parent_inode = 0, .buffer_size = 2, .is_directory = false,
        };
        if (read_directory(&rd_sub2) == 0) {
            struct EXT2DirectoryEntry *self_entry = get_directory_entry(&sub2_block, 0);
            sub2_inode = self_entry->inode;
            req_f.parent_inode = sub2_inode;

            // Same idempotency concern as sub2 itself: f.txt may already be
            // there from a previous run.
            write(&req_f);
            struct EXT2DriverRequest del_sub2 = req_sub2;
            rc_nonempty_reject = delete(del_sub2);
        }
        serial_write(rc_nonempty_reject == 2 ? "[CHECK] NONEMPTY_REJECT_OK\n" : "[CHECK] NONEMPTY_REJECT_FAIL\n");

        // Cleanup so the test is fully idempotent across reboots: remove
        // f.txt then sub2 itself.
        if (sub2_inode != 0) {
            struct EXT2DriverRequest del_f = req_f;
            del_f.parent_inode = sub2_inode;
            delete(del_f);
            struct EXT2DriverRequest del_sub2_final = req_sub2;
            delete(del_sub2_final);
        }

        struct EXT2DriverRequest del_test = req_write;
        rc = delete(del_test);
        int8_t rc_read_after_delete = read(req_read);
        serial_write((rc == 0 && rc_read_after_delete == 3) ? "[CHECK] DELETE_VERIFY_OK\n" : "[CHECK] DELETE_VERIFY_FAIL\n");
    }

    load_gdt(&_gdt_gdtr);
    initialize_idt();

    draw_dashboard(fs_freshly_created);

    // Aktifkan Keyboard Driver. Panel diagnostics Chapter 1 (trigger CPU
    // exception lewat tombol 1-6/0) tidak lagi ditampilkan di layar utama,
    // tapi logikanya (trigger_test() di keyboard.c) tetap aktif kalau
    // sewaktu-waktu dibutuhkan lagi untuk debugging.
    keyboard_state_activate();
    keyboard_echo_set_cursor(12, 11);

    // Auto-test disabled by default. Uncomment to panic immediately on boot:
    // __asm__ volatile("int $0x00");

    while (true) {
        b += 1;
        (void)b;
        __asm__ volatile("hlt");
    }
}