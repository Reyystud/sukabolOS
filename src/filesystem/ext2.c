#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/driver/disk.h"
#include "header/filesystem/ext2.h"
#include "header/stdlib/string.h"

const uint8_t fs_signature[BLOCK_SIZE] = {
    'C', 'o', 'u', 'r', 's', 'e', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ' ',
    'D', 'e', 's', 'i', 'g', 'n', 'e', 'd', ' ', 'b', 'y', ' ', ' ', ' ', ' ',  ' ',
    'L', 'a', 'b', ' ', 'S', 'i', 's', 't', 'e', 'r', ' ', 'I', 'T', 'B', ' ',  ' ',
    'M', 'a', 'd', 'e', ' ', 'w', 'i', 't', 'h', ' ', '<', '3', ' ', ' ', ' ',  ' ',
    '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '2', '0', '2', '5', '\n',
    [BLOCK_SIZE-2] = 'O',
    [BLOCK_SIZE-1] = 'k',
};

// In-RAM filesystem state, populated by create_ext2() or loaded from disk by
// initialize_filesystem_ext2().
static struct EXT2Superblock superblock_state;
static struct EXT2BlockGroupDescriptorTable bgd_table_state;

/* =================== REGULAR helpers =================== */

char *get_entry_name(void *entry) {
    return (char*) ((uint8_t*) entry + sizeof(struct EXT2DirectoryEntry));
}

struct EXT2DirectoryEntry *get_directory_entry(void *ptr, uint32_t offset) {
    return (struct EXT2DirectoryEntry*) ((uint8_t*) ptr + offset);
}

struct EXT2DirectoryEntry *get_next_directory_entry(struct EXT2DirectoryEntry *entry) {
    return (struct EXT2DirectoryEntry*) ((uint8_t*) entry + entry->rec_len);
}

uint16_t get_entry_record_len(uint8_t name_len) {
    uint16_t raw = (uint16_t) (sizeof(struct EXT2DirectoryEntry) + name_len);
    return (uint16_t) ((raw + 3u) & ~3u);
}

uint32_t get_dir_first_child_offset(void *ptr) {
    struct EXT2DirectoryEntry *self_entry   = get_directory_entry(ptr, 0);
    struct EXT2DirectoryEntry *parent_entry = get_next_directory_entry(self_entry);
    return (uint32_t) ((uint8_t*) parent_entry - (uint8_t*) ptr) + parent_entry->rec_len;
}

/* =================== inode <-> block group helpers =================== */

uint32_t inode_to_bgd(uint32_t inode) {
    return (inode - 1) / INODES_PER_GROUP;
}

uint32_t inode_to_local(uint32_t inode) {
    return (inode - 1) % INODES_PER_GROUP;
}

static void read_inode(uint32_t inode, struct EXT2Inode *out) {
    uint32_t bgd            = inode_to_bgd(inode);
    uint32_t local          = inode_to_local(inode);
    uint32_t block_offset   = local / INODES_PER_TABLE;
    uint32_t index_in_block = local % INODES_PER_TABLE;

    struct BlockBuffer block;
    read_blocks(&block, bgd_table_state.table[bgd].bg_inode_table + block_offset, 1);
    memcpy(out, block.buf + index_in_block * INODE_SIZE, INODE_SIZE);
}

void sync_node(struct EXT2Inode *node, uint32_t inode) {
    uint32_t bgd            = inode_to_bgd(inode);
    uint32_t local          = inode_to_local(inode);
    uint32_t block_offset   = local / INODES_PER_TABLE;
    uint32_t index_in_block = local % INODES_PER_TABLE;

    struct BlockBuffer block;
    read_blocks(&block, bgd_table_state.table[bgd].bg_inode_table + block_offset, 1);
    memcpy(block.buf + index_in_block * INODE_SIZE, node, INODE_SIZE);
    write_blocks(&block, bgd_table_state.table[bgd].bg_inode_table + block_offset, 1);
}

/* =================== superblock/bgd persistence =================== */

static void sync_superblock_and_bgd(void) {
    struct BlockBuffer sb_block;
    memset(&sb_block, 0, sizeof(sb_block));
    memcpy(sb_block.buf, &superblock_state, sizeof(superblock_state));
    write_blocks(&sb_block, 1, 1);

    struct BlockBuffer bgd_block;
    memset(&bgd_block, 0, sizeof(bgd_block));
    memcpy(bgd_block.buf, &bgd_table_state, sizeof(bgd_table_state));
    write_blocks(&bgd_block, 2, 1);
}

static void load_superblock_and_bgd(void) {
    struct BlockBuffer sb_block;
    read_blocks(&sb_block, 1, 1);
    memcpy(&superblock_state, sb_block.buf, sizeof(superblock_state));

    struct BlockBuffer bgd_block;
    read_blocks(&bgd_block, 2, 1);
    memcpy(&bgd_table_state, bgd_block.buf, sizeof(bgd_table_state));
}

/* =================== bitmap allocation =================== */

uint32_t allocate_node(void) {
    for (uint32_t g = 0; g < GROUPS_COUNT; g++) {
        struct BlockBuffer bitmap;
        read_blocks(&bitmap, bgd_table_state.table[g].bg_inode_bitmap, 1);
        for (uint32_t i = 0; i < INODES_PER_GROUP; i++) {
            uint32_t byte = i / 8;
            uint32_t bit  = i % 8;
            if (!(bitmap.buf[byte] & (1u << bit))) {
                bitmap.buf[byte] |= (uint8_t) (1u << bit);
                write_blocks(&bitmap, bgd_table_state.table[g].bg_inode_bitmap, 1);
                bgd_table_state.table[g].bg_free_inodes_count--;
                superblock_state.s_free_inodes_count--;
                return g * INODES_PER_GROUP + i + 1; // inode numbers start at 1
            }
        }
    }
    return 0; // no free inode available
}

static uint32_t allocate_one_block(uint32_t prefered_bgd) {
    for (uint32_t offset = 0; offset < GROUPS_COUNT; offset++) {
        uint32_t g = (prefered_bgd + offset) % GROUPS_COUNT;
        struct BlockBuffer bitmap;
        read_blocks(&bitmap, bgd_table_state.table[g].bg_block_bitmap, 1);
        for (uint32_t byte = 0; byte < (BLOCKS_PER_GROUP / 8); byte++) {
            if (bitmap.buf[byte] == 0xFF) continue;
            for (uint32_t bit = 0; bit < 8; bit++) {
                if (!(bitmap.buf[byte] & (1u << bit))) {
                    bitmap.buf[byte] |= (uint8_t) (1u << bit);
                    write_blocks(&bitmap, bgd_table_state.table[g].bg_block_bitmap, 1);
                    bgd_table_state.table[g].bg_free_blocks_count--;
                    superblock_state.s_free_blocks_count--;
                    return g * BLOCKS_PER_GROUP + (byte * 8 + bit);
                }
            }
        }
    }
    return 0; // no free block available
}

/* =================== directory table =================== */

void init_directory_table(struct EXT2Inode *node, uint32_t inode, uint32_t parent_inode) {
    node->i_mode = EXT2_S_IFDIR;
    for (int i = 0; i < 15; i++) node->i_block[i] = 0;

    uint32_t data_block = allocate_one_block(inode_to_bgd(inode));
    node->i_block[0] = data_block;
    node->i_blocks   = 1;
    node->i_size     = BLOCK_SIZE;

    struct BlockBuffer block;
    memset(&block, 0, sizeof(block));

    struct EXT2DirectoryEntry *self_entry = get_directory_entry(&block, 0);
    self_entry->inode     = inode;
    self_entry->name_len  = 1;
    self_entry->file_type = EXT2_FT_DIR;
    self_entry->rec_len   = get_entry_record_len(1);
    get_entry_name(self_entry)[0] = '.';

    struct EXT2DirectoryEntry *parent_entry = get_next_directory_entry(self_entry);
    parent_entry->inode     = parent_inode;
    parent_entry->name_len  = 2;
    parent_entry->file_type = EXT2_FT_DIR;
    // Last entry in the block always extends to the end of the block - this is
    // the invariant later code relies on to find "the current last entry" when
    // appending a new child, and to detect "no more entries" while iterating.
    parent_entry->rec_len = (uint16_t) (BLOCK_SIZE - self_entry->rec_len);
    char *parent_name = get_entry_name(parent_entry);
    parent_name[0] = '.';
    parent_name[1] = '.';

    write_blocks(&block, data_block, 1);
}

bool is_directory_empty(uint32_t inode) {
    struct EXT2Inode node;
    read_inode(inode, &node);

    struct BlockBuffer block;
    read_blocks(&block, node.i_block[0], 1);

    uint32_t offset = get_dir_first_child_offset(&block);
    if (offset >= BLOCK_SIZE) return true;

    struct EXT2DirectoryEntry *first_child = get_directory_entry(&block, offset);
    return first_child->inode == 0;
}

/* =================== initializer =================== */

bool is_empty_storage(void) {
    struct BlockBuffer boot_sector;
    read_blocks(&boot_sector, BOOT_SECTOR, 1);
    return memcmp(boot_sector.buf, fs_signature, BLOCK_SIZE) != 0;
}

void create_ext2(void) {
    write_blocks(fs_signature, BOOT_SECTOR, 1);

    struct BlockBuffer zero_block;
    memset(&zero_block, 0, sizeof(zero_block));

    for (uint32_t g = 0; g < GROUPS_COUNT; g++) {
        uint32_t group_start = g * BLOCKS_PER_GROUP;
        uint32_t reserved;
        uint32_t block_bitmap_addr, inode_bitmap_addr, inode_table_addr;

        if (g == 0) {
            // Group 0 also carries the boot sector, superblock, and BGD table
            // within its own block range (blocks 0-2), on top of its own
            // block bitmap / inode bitmap / inode table.
            block_bitmap_addr = group_start + 3;
            inode_bitmap_addr = group_start + 4;
            inode_table_addr  = group_start + 5;
            reserved = 3 + 2 + INODES_TABLE_BLOCK_COUNT;
        } else {
            block_bitmap_addr = group_start + 0;
            inode_bitmap_addr = group_start + 1;
            inode_table_addr  = group_start + 2;
            reserved = 2 + INODES_TABLE_BLOCK_COUNT;
        }

        bgd_table_state.table[g].bg_block_bitmap     = block_bitmap_addr;
        bgd_table_state.table[g].bg_inode_bitmap     = inode_bitmap_addr;
        bgd_table_state.table[g].bg_inode_table      = inode_table_addr;
        bgd_table_state.table[g].bg_free_blocks_count = (uint16_t) (BLOCKS_PER_GROUP - reserved);
        bgd_table_state.table[g].bg_free_inodes_count = (uint16_t) INODES_PER_GROUP;
        bgd_table_state.table[g].bg_used_dirs_count   = 0;
        bgd_table_state.table[g].bg_pad               = 0;
        bgd_table_state.table[g].bg_reserved[0] = 0;
        bgd_table_state.table[g].bg_reserved[1] = 0;
        bgd_table_state.table[g].bg_reserved[2] = 0;

        write_blocks(&zero_block, inode_bitmap_addr, 1);

        struct BlockBuffer bitmap;
        memset(&bitmap, 0, sizeof(bitmap));
        for (uint32_t i = 0; i < reserved; i++) {
            bitmap.buf[i / 8] |= (uint8_t) (1u << (i % 8));
        }
        write_blocks(&bitmap, block_bitmap_addr, 1);

        for (uint32_t b = 0; b < INODES_TABLE_BLOCK_COUNT; b++) {
            write_blocks(&zero_block, inode_table_addr + b, 1);
        }
    }

    superblock_state.s_inodes_count       = INODES_PER_GROUP * GROUPS_COUNT;
    superblock_state.s_blocks_count       = BLOCKS_PER_GROUP * GROUPS_COUNT;
    superblock_state.s_r_blocks_count     = 0;
    superblock_state.s_free_inodes_count  = superblock_state.s_inodes_count;
    superblock_state.s_free_blocks_count  = 0;
    for (uint32_t g = 0; g < GROUPS_COUNT; g++) {
        superblock_state.s_free_blocks_count += bgd_table_state.table[g].bg_free_blocks_count;
    }
    superblock_state.s_first_data_block   = 1;
    superblock_state.s_first_ino          = 1;
    superblock_state.s_blocks_per_group   = BLOCKS_PER_GROUP;
    superblock_state.s_frags_per_group    = BLOCKS_PER_GROUP;
    superblock_state.s_inodes_per_group   = INODES_PER_GROUP;
    superblock_state.s_magic              = EXT2_SUPER_MAGIC;
    superblock_state.s_prealloc_blocks    = 0;
    superblock_state.s_prealloc_dir_blocks = 0;

    uint32_t root_inode = allocate_node(); // expected to be 1
    struct EXT2Inode root_node;
    memset(&root_node, 0, sizeof(root_node));
    init_directory_table(&root_node, root_inode, root_inode); // root is its own parent
    sync_node(&root_node, root_inode);
    bgd_table_state.table[inode_to_bgd(root_inode)].bg_used_dirs_count++;

    sync_superblock_and_bgd();
}

void initialize_filesystem_ext2(void) {
    if (is_empty_storage()) {
        create_ext2();
    } else {
        load_superblock_and_bgd();
    }
}

/* =================== CRUD helpers =================== */

static bool is_valid_directory_inode(uint32_t inode) {
    if (inode == 0 || inode > INODES_PER_GROUP * GROUPS_COUNT) return false;

    uint32_t bgd   = inode_to_bgd(inode);
    uint32_t local = inode_to_local(inode);
    struct BlockBuffer bitmap;
    read_blocks(&bitmap, bgd_table_state.table[bgd].bg_inode_bitmap, 1);
    if (!(bitmap.buf[local / 8] & (1u << (local % 8)))) return false;

    struct EXT2Inode node;
    read_inode(inode, &node);
    return node.i_mode == EXT2_S_IFDIR;
}

// Name-only lookup (ignores file_type) - used by read()/read_directory()/delete()
// to first locate a candidate, then separately check its type to distinguish
// "not found" from "found but wrong type" per their documented error codes.
static struct EXT2DirectoryEntry *find_entry_by_name(struct BlockBuffer *block, const char *name, uint8_t name_len) {
    uint32_t offset = 0;
    while (offset < BLOCK_SIZE) {
        struct EXT2DirectoryEntry *entry = get_directory_entry(block, offset);
        if (entry->inode != 0 && entry->name_len == name_len &&
            memcmp(get_entry_name(entry), name, name_len) == 0) {
            return entry;
        }
        if (entry->rec_len == 0) break; // corrupt chain guard
        offset += entry->rec_len;
    }
    return NULL;
}

// Name+type lookup - used by write()'s duplicate check and delete()'s target
// lookup, both of which explicitly allow a file and a folder to share a name
// (disambiguated by type) per the header's own doc comments.
static struct EXT2DirectoryEntry *find_entry_by_name_and_type(struct BlockBuffer *block, const char *name, uint8_t name_len, uint8_t file_type) {
    uint32_t offset = 0;
    while (offset < BLOCK_SIZE) {
        struct EXT2DirectoryEntry *entry = get_directory_entry(block, offset);
        if (entry->inode != 0 && entry->name_len == name_len && entry->file_type == file_type &&
            memcmp(get_entry_name(entry), name, name_len) == 0) {
            return entry;
        }
        if (entry->rec_len == 0) break;
        offset += entry->rec_len;
    }
    return NULL;
}

// Appends a new entry to a directory's (single) data block, following the
// invariant that the last entry in the block always extends to BLOCK_SIZE:
// if that last slot is free (inode==0) and big enough, reuse it in place;
// otherwise shrink it to its true minimal size and place the new entry after
// it as the new last slot. Returns false if there is no room left.
static bool insert_directory_entry(struct BlockBuffer *block, uint32_t inode, const char *name, uint8_t name_len, uint8_t file_type) {
    uint32_t offset = 0;
    struct EXT2DirectoryEntry *last = NULL;
    uint32_t last_offset = 0;
    while (offset < BLOCK_SIZE) {
        struct EXT2DirectoryEntry *entry = get_directory_entry(block, offset);
        if (offset + entry->rec_len >= BLOCK_SIZE) {
            last = entry;
            last_offset = offset;
            break;
        }
        offset += entry->rec_len;
    }
    if (last == NULL) return false;

    uint16_t new_entry_len = get_entry_record_len(name_len);

    if (last->inode == 0) {
        if (last->rec_len < new_entry_len) return false;
        last->inode     = inode;
        last->name_len  = name_len;
        last->file_type = file_type;
        memcpy(get_entry_name(last), name, name_len);
        return true;
    }

    uint16_t last_min_len = get_entry_record_len((uint8_t) last->name_len);
    if (last_offset + last_min_len + new_entry_len > BLOCK_SIZE) return false;

    last->rec_len = last_min_len;

    struct EXT2DirectoryEntry *new_entry = get_directory_entry(block, last_offset + last_min_len);
    new_entry->inode     = inode;
    new_entry->name_len  = name_len;
    new_entry->file_type = file_type;
    new_entry->rec_len   = (uint16_t) (BLOCK_SIZE - (last_offset + last_min_len));
    memcpy(get_entry_name(new_entry), name, name_len);
    return true;
}

static void read_inode_data(struct EXT2Inode *node, void *buf, uint32_t size) {
    uint8_t *dst = (uint8_t*) buf;
    uint32_t blocks_needed = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint32_t block_idx = 0;

    for (int i = 0; i < 12 && block_idx < blocks_needed; i++, block_idx++) {
        read_blocks(dst + block_idx * BLOCK_SIZE, node->i_block[i], 1);
    }
    if (block_idx < blocks_needed && node->i_block[12] != 0) {
        struct BlockBuffer indirect;
        read_blocks(&indirect, node->i_block[12], 1);
        uint32_t *ptrs = (uint32_t*) indirect.buf;
        for (int i = 0; i < 128 && block_idx < blocks_needed; i++, block_idx++) {
            read_blocks(dst + block_idx * BLOCK_SIZE, ptrs[i], 1);
        }
    }
    if (block_idx < blocks_needed && node->i_block[13] != 0) {
        struct BlockBuffer dindirect;
        read_blocks(&dindirect, node->i_block[13], 1);
        uint32_t *dptrs = (uint32_t*) dindirect.buf;
        for (int i = 0; i < 128 && block_idx < blocks_needed; i++) {
            if (dptrs[i] == 0) continue;
            struct BlockBuffer indirect;
            read_blocks(&indirect, dptrs[i], 1);
            uint32_t *ptrs = (uint32_t*) indirect.buf;
            for (int j = 0; j < 128 && block_idx < blocks_needed; j++, block_idx++) {
                read_blocks(dst + block_idx * BLOCK_SIZE, ptrs[j], 1);
            }
        }
    }
}

/* =================== MEMORY ==================== */

void allocate_node_blocks(void *ptr, struct EXT2Inode *node, uint32_t prefered_bgd) {
    uint32_t blocks_needed = node->i_blocks;
    const uint8_t *data = (const uint8_t*) ptr;
    uint32_t block_idx = 0;

    for (int i = 0; i < 12 && block_idx < blocks_needed; i++, block_idx++) {
        uint32_t blk = allocate_one_block(prefered_bgd);
        node->i_block[i] = blk;
        write_blocks(data + block_idx * BLOCK_SIZE, blk, 1);
    }

    if (block_idx < blocks_needed) {
        uint32_t indirect_blk = allocate_one_block(prefered_bgd);
        node->i_block[12] = indirect_blk;
        struct BlockBuffer indirect_table;
        memset(&indirect_table, 0, sizeof(indirect_table));
        uint32_t *ptrs = (uint32_t*) indirect_table.buf;
        for (int i = 0; i < 128 && block_idx < blocks_needed; i++, block_idx++) {
            uint32_t blk = allocate_one_block(prefered_bgd);
            ptrs[i] = blk;
            write_blocks(data + block_idx * BLOCK_SIZE, blk, 1);
        }
        write_blocks(&indirect_table, indirect_blk, 1);
    }

    if (block_idx < blocks_needed) {
        uint32_t dindirect_blk = allocate_one_block(prefered_bgd);
        node->i_block[13] = dindirect_blk;
        struct BlockBuffer dindirect_table;
        memset(&dindirect_table, 0, sizeof(dindirect_table));
        uint32_t *dptrs = (uint32_t*) dindirect_table.buf;
        for (int i = 0; i < 128 && block_idx < blocks_needed; i++) {
            uint32_t indirect_blk = allocate_one_block(prefered_bgd);
            dptrs[i] = indirect_blk;
            struct BlockBuffer indirect_table;
            memset(&indirect_table, 0, sizeof(indirect_table));
            uint32_t *ptrs = (uint32_t*) indirect_table.buf;
            for (int j = 0; j < 128 && block_idx < blocks_needed; j++, block_idx++) {
                uint32_t blk = allocate_one_block(prefered_bgd);
                ptrs[j] = blk;
                write_blocks(data + block_idx * BLOCK_SIZE, blk, 1);
            }
            write_blocks(&indirect_table, indirect_blk, 1);
        }
        write_blocks(&dindirect_table, dindirect_blk, 1);
    }
    // i_block[14] (triply indirect) intentionally left unused - only doubly
    // indirect is implemented, per the header's own note (this 4MB disk never
    // needs more than that).
}

uint32_t deallocate_block(uint32_t *locations, uint32_t blocks, struct BlockBuffer *bitmap, uint32_t depth, uint32_t *last_bgd, bool bgd_loaded) {
    if (depth == 0) {
        for (uint32_t i = 0; i < blocks; i++) {
            uint32_t blk = locations[i];
            if (blk == 0) continue;

            uint32_t bgd   = blk / BLOCKS_PER_GROUP;
            uint32_t local = blk % BLOCKS_PER_GROUP;
            if (!bgd_loaded || bgd != *last_bgd) {
                if (bgd_loaded) {
                    write_blocks(bitmap, bgd_table_state.table[*last_bgd].bg_block_bitmap, 1);
                }
                read_blocks(bitmap, bgd_table_state.table[bgd].bg_block_bitmap, 1);
                *last_bgd = bgd;
                bgd_loaded = true;
            }

            if (bitmap->buf[local / 8] & (1u << (local % 8))) {
                bitmap->buf[local / 8] &= (uint8_t) ~(1u << (local % 8));
                bgd_table_state.table[bgd].bg_free_blocks_count++;
                superblock_state.s_free_blocks_count++;
            }
        }
    } else {
        for (uint32_t i = 0; i < blocks; i++) {
            uint32_t table_blk = locations[i];
            if (table_blk == 0) continue;

            struct BlockBuffer table;
            read_blocks(&table, table_blk, 1);
            uint32_t *children = (uint32_t*) table.buf;
            bgd_loaded = (deallocate_block(children, 128, bitmap, depth - 1, last_bgd, bgd_loaded) != 0);
            bgd_loaded = (deallocate_block(&table_blk, 1, bitmap, 0, last_bgd, bgd_loaded) != 0);
        }
    }
    return bgd_loaded ? 1u : 0u;
}

void deallocate_blocks(void *loc, uint32_t blocks) {
    uint32_t *i_block = (uint32_t*) loc;
    struct BlockBuffer bitmap;
    uint32_t last_bgd = 0;
    bool bgd_loaded = false;

    uint32_t direct_count = blocks < 12 ? blocks : 12;
    bgd_loaded = (deallocate_block(i_block, direct_count, &bitmap, 0, &last_bgd, bgd_loaded) != 0);

    if (blocks > 12) {
        bgd_loaded = (deallocate_block(&i_block[12], 1, &bitmap, 1, &last_bgd, bgd_loaded) != 0);
    }
    if (blocks > 12u + 128u) {
        bgd_loaded = (deallocate_block(&i_block[13], 1, &bitmap, 2, &last_bgd, bgd_loaded) != 0);
    }

    if (bgd_loaded) {
        write_blocks(&bitmap, bgd_table_state.table[last_bgd].bg_block_bitmap, 1);
    }
}

void deallocate_node(uint32_t inode) {
    struct EXT2Inode node;
    read_inode(inode, &node);

    deallocate_blocks(node.i_block, node.i_blocks);

    uint32_t bgd   = inode_to_bgd(inode);
    uint32_t local = inode_to_local(inode);
    struct BlockBuffer inode_bitmap;
    read_blocks(&inode_bitmap, bgd_table_state.table[bgd].bg_inode_bitmap, 1);
    inode_bitmap.buf[local / 8] &= (uint8_t) ~(1u << (local % 8));
    write_blocks(&inode_bitmap, bgd_table_state.table[bgd].bg_inode_bitmap, 1);
    bgd_table_state.table[bgd].bg_free_inodes_count++;
    superblock_state.s_free_inodes_count++;

    if (node.i_mode == EXT2_S_IFDIR) {
        bgd_table_state.table[bgd].bg_used_dirs_count--;
    }
}

/* =================== CRUD ==================== */

int8_t read_directory(struct EXT2DriverRequest *request) {
    if (!is_valid_directory_inode(request->parent_inode)) return 3;

    struct EXT2Inode parent_node;
    read_inode(request->parent_inode, &parent_node);
    struct BlockBuffer parent_block;
    read_blocks(&parent_block, parent_node.i_block[0], 1);

    struct EXT2DirectoryEntry *entry = find_entry_by_name(&parent_block, request->name, request->name_len);
    if (entry == NULL) return 2;
    if (entry->file_type != EXT2_FT_DIR) return 1;

    struct EXT2Inode target_node;
    read_inode(entry->inode, &target_node);

    struct BlockBuffer target_block;
    read_blocks(&target_block, target_node.i_block[0], 1);
    memcpy(request->buf, &target_block, BLOCK_SIZE);

    return 0;
}

int8_t read(struct EXT2DriverRequest request) {
    if (!is_valid_directory_inode(request.parent_inode)) return 4;

    struct EXT2Inode parent_node;
    read_inode(request.parent_inode, &parent_node);
    struct BlockBuffer parent_block;
    read_blocks(&parent_block, parent_node.i_block[0], 1);

    struct EXT2DirectoryEntry *entry = find_entry_by_name(&parent_block, request.name, request.name_len);
    if (entry == NULL) return 3;
    if (entry->file_type != EXT2_FT_REG_FILE) return 1;

    struct EXT2Inode target_node;
    read_inode(entry->inode, &target_node);
    if (target_node.i_size > request.buffer_size) return 2;

    read_inode_data(&target_node, request.buf, target_node.i_size);

    return 0;
}

int8_t write(struct EXT2DriverRequest *request) {
    if (!is_valid_directory_inode(request->parent_inode)) return 2;

    struct EXT2Inode parent_node;
    read_inode(request->parent_inode, &parent_node);
    struct BlockBuffer parent_block;
    read_blocks(&parent_block, parent_node.i_block[0], 1);

    uint8_t new_file_type = request->is_directory ? EXT2_FT_DIR : EXT2_FT_REG_FILE;
    if (find_entry_by_name_and_type(&parent_block, request->name, request->name_len, new_file_type) != NULL) {
        return 1;
    }

    uint32_t new_inode = allocate_node();
    struct EXT2Inode new_node;
    memset(&new_node, 0, sizeof(new_node));

    if (request->is_directory) {
        init_directory_table(&new_node, new_inode, request->parent_inode);
        bgd_table_state.table[inode_to_bgd(new_inode)].bg_used_dirs_count++;
    } else {
        new_node.i_mode   = EXT2_S_IFREG;
        new_node.i_size   = request->buffer_size;
        new_node.i_blocks = (request->buffer_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
        for (int i = 0; i < 15; i++) new_node.i_block[i] = 0;
        if (new_node.i_blocks > 0) {
            allocate_node_blocks(request->buf, &new_node, inode_to_bgd(new_inode));
        }
    }
    sync_node(&new_node, new_inode);

    insert_directory_entry(&parent_block, new_inode, request->name, request->name_len, new_file_type);
    write_blocks(&parent_block, parent_node.i_block[0], 1);

    sync_superblock_and_bgd();

    return 0;
}

int8_t delete(struct EXT2DriverRequest request) {
    if (!is_valid_directory_inode(request.parent_inode)) return 3;

    struct EXT2Inode parent_node;
    read_inode(request.parent_inode, &parent_node);
    struct BlockBuffer parent_block;
    read_blocks(&parent_block, parent_node.i_block[0], 1);

    uint8_t target_file_type = request.is_directory ? EXT2_FT_DIR : EXT2_FT_REG_FILE;
    struct EXT2DirectoryEntry *entry = find_entry_by_name_and_type(&parent_block, request.name, request.name_len, target_file_type);
    if (entry == NULL) return 1;

    uint32_t target_inode = entry->inode;
    if (request.is_directory && !is_directory_empty(target_inode)) return 2;

    deallocate_node(target_inode);

    entry->inode = 0; // mark slot free; rec_len chain stays intact for future reuse
    write_blocks(&parent_block, parent_node.i_block[0], 1);

    sync_superblock_and_bgd();

    return 0;
}
