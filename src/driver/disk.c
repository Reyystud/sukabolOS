#include "header/driver/disk.h"
#include "header/cpu/portio.h"

#define ATA_PRIMARY_DATA         0x1F0
#define ATA_PRIMARY_SECTOR_COUNT 0x1F2
#define ATA_PRIMARY_LBA_LOW      0x1F3
#define ATA_PRIMARY_LBA_MID      0x1F4
#define ATA_PRIMARY_LBA_HIGH     0x1F5
#define ATA_PRIMARY_DRIVE_HEAD   0x1F6
#define ATA_PRIMARY_COMMAND      0x1F7
#define ATA_PRIMARY_STATUS       0x1F7

#define ATA_CMD_READ_SECTORS  0x20
#define ATA_CMD_WRITE_SECTORS 0x30

static void ATA_busy_wait(void) {
    while (inb(ATA_PRIMARY_STATUS) & ATA_STATUS_BSY);
}

static void ATA_DRQ_wait(void) {
    while (!(inb(ATA_PRIMARY_STATUS) & ATA_STATUS_DRQ));
}

static void ATA_setup(uint32_t lba, uint8_t block_count, uint8_t command) {
    outb(ATA_PRIMARY_DRIVE_HEAD, (uint8_t) (0xE0 | ((lba >> 24) & 0x0F)));
    outb(ATA_PRIMARY_SECTOR_COUNT, block_count);
    outb(ATA_PRIMARY_LBA_LOW,  (uint8_t) (lba & 0xFF));
    outb(ATA_PRIMARY_LBA_MID,  (uint8_t) ((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_LBA_HIGH, (uint8_t) ((lba >> 16) & 0xFF));
    outb(ATA_PRIMARY_COMMAND, command);
}

void read_blocks(void *ptr, uint32_t logical_block_address, uint8_t block_count) {
    ATA_setup(logical_block_address, block_count, ATA_CMD_READ_SECTORS);

    uint16_t *buf = (uint16_t*) ptr;
    uint32_t word_index = 0;
    uint8_t remaining = block_count;
    do {
        ATA_busy_wait();
        ATA_DRQ_wait();
        for (uint32_t i = 0; i < HALF_BLOCK_SIZE; i++) {
            buf[word_index++] = inw(ATA_PRIMARY_DATA);
        }
        remaining--;
    } while (remaining != 0);
    // Ensure the drive has fully settled before returning, so a command
    // issued immediately after this one doesn't race the drive's internal
    // post-transfer processing (observed to hang the very next command).
    ATA_busy_wait();
}

void write_blocks(const void *ptr, uint32_t logical_block_address, uint8_t block_count) {
    ATA_setup(logical_block_address, block_count, ATA_CMD_WRITE_SECTORS);

    const uint16_t *buf = (const uint16_t*) ptr;
    uint32_t word_index = 0;
    uint8_t remaining = block_count;
    do {
        ATA_busy_wait();
        ATA_DRQ_wait();
        for (uint32_t i = 0; i < HALF_BLOCK_SIZE; i++) {
            outw(ATA_PRIMARY_DATA, buf[word_index++]);
        }
        remaining--;
    } while (remaining != 0);
    // Same settling wait as read_blocks (see comment there).
    ATA_busy_wait();
}
