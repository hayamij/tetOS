#include "ata.h"
#include "../io/io.h"
#include "../string/string.h"

#define ATA_PRIMARY_DATA     0x1F0
#define ATA_PRIMARY_ERROR    0x1F1
#define ATA_PRIMARY_COUNT    0x1F2
#define ATA_PRIMARY_LBA_LO   0x1F3
#define ATA_PRIMARY_LBA_MID  0x1F4
#define ATA_PRIMARY_LBA_HI   0x1F5
#define ATA_PRIMARY_DRIVE    0x1F6
#define ATA_PRIMARY_STATUS   0x1F7
#define ATA_PRIMARY_CMD      0x1F7
#define ATA_PRIMARY_CTRL     0x3F6

#define ATA_STATUS_ERR  0x01
#define ATA_STATUS_DRQ  0x08
#define ATA_STATUS_RDY  0x40
#define ATA_STATUS_BSY  0x80

#define ATA_CMD_READ    0x20
#define ATA_CMD_WRITE   0x30
#define ATA_CMD_IDENT   0xEC

static ata_drive_t drive;

static void ata_400ns_delay(void) {
    inb(ATA_PRIMARY_CTRL);
    inb(ATA_PRIMARY_CTRL);
    inb(ATA_PRIMARY_CTRL);
    inb(ATA_PRIMARY_CTRL);
}

static int ata_wait_ready(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        uint8_t status = inb(ATA_PRIMARY_STATUS);
        if (status & ATA_STATUS_ERR) return -1;
        if (!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_RDY)) return 0;
    }
    return -1;
}

static int ata_wait_drq(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        uint8_t status = inb(ATA_PRIMARY_STATUS);
        if (status & ATA_STATUS_ERR) return -1;
        if (status & ATA_STATUS_DRQ) return 0;
    }
    return -1;
}

int ata_init(void) {
    drive.present = 0;

    outb(ATA_PRIMARY_CTRL, 0x02);
    ata_400ns_delay();
    outb(ATA_PRIMARY_CTRL, 0x00);

    outb(ATA_PRIMARY_DRIVE, 0xA0);
    ata_400ns_delay();

    if (ata_wait_ready() != 0) return -1;

    outb(ATA_PRIMARY_COUNT,  0);
    outb(ATA_PRIMARY_LBA_LO, 0);
    outb(ATA_PRIMARY_LBA_MID,0);
    outb(ATA_PRIMARY_LBA_HI, 0);
    outb(ATA_PRIMARY_CMD, ATA_CMD_IDENT);
    ata_400ns_delay();

    uint8_t status = inb(ATA_PRIMARY_STATUS);
    if (status == 0) return -1;

    if (ata_wait_drq() != 0) return -1;

    uint16_t ident[256];
    for (int i = 0; i < 256; i++) {
        ident[i] = inw(ATA_PRIMARY_DATA);
    }

    for (int i = 0; i < 20; i++) {
        drive.model[i * 2]     = (char)(ident[27 + i] >> 8);
        drive.model[i * 2 + 1] = (char)(ident[27 + i] & 0xFF);
    }
    drive.model[40] = '\0';

    for (int i = 39; i >= 0 && drive.model[i] == ' '; i--) {
        drive.model[i] = '\0';
    }

    drive.sectors = (uint32_t)ident[60] | ((uint32_t)ident[61] << 16);
    drive.present = 1;
    return 0;
}

int ata_read(uint32_t lba, uint8_t count, void* buf) {
    if (!drive.present) return -1;
    if (ata_wait_ready() != 0) return -1;

    outb(ATA_PRIMARY_DRIVE,  0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_COUNT,  count);
    outb(ATA_PRIMARY_LBA_LO, (uint8_t)(lba));
    outb(ATA_PRIMARY_LBA_MID,(uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI, (uint8_t)(lba >> 16));
    outb(ATA_PRIMARY_CMD,    ATA_CMD_READ);

    uint16_t* ptr = (uint16_t*)buf;
    for (int s = 0; s < count; s++) {
        ata_400ns_delay();
        if (ata_wait_drq() != 0) return -1;
        for (int i = 0; i < 256; i++) {
            ptr[s * 256 + i] = inw(ATA_PRIMARY_DATA);
        }
    }
    return 0;
}

int ata_write(uint32_t lba, uint8_t count, const void* buf) {
    if (!drive.present) return -1;
    if (ata_wait_ready() != 0) return -1;

    outb(ATA_PRIMARY_DRIVE,  0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_COUNT,  count);
    outb(ATA_PRIMARY_LBA_LO, (uint8_t)(lba));
    outb(ATA_PRIMARY_LBA_MID,(uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI, (uint8_t)(lba >> 16));
    outb(ATA_PRIMARY_CMD,    ATA_CMD_WRITE);

    const uint16_t* ptr = (const uint16_t*)buf;
    for (int s = 0; s < count; s++) {
        ata_400ns_delay();
        if (ata_wait_drq() != 0) return -1;
        for (int i = 0; i < 256; i++) {
            outw(ATA_PRIMARY_DATA, ptr[s * 256 + i]);
        }
    }

    outb(ATA_PRIMARY_CMD, 0xE7);
    ata_400ns_delay();
    if (ata_wait_ready() != 0) return -1;
    return 0;
}

ata_drive_t* ata_get_drive(void) {
    return &drive;
}
