#ifndef ATA_H
#define ATA_H

#include "types.h"

#define ATA_SECTOR_SIZE 512

typedef struct {
    char     model[41];
    uint32_t sectors;
    uint8_t  present;
} ata_drive_t;

int      ata_init(void);
int      ata_read(uint32_t lba, uint8_t count, void* buf);
int      ata_write(uint32_t lba, uint8_t count, const void* buf);
ata_drive_t* ata_get_drive(void);

#endif
