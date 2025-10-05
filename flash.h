#include "hardware/pio.h"
#include "pio_spi.h"
#include <stdio.h>
#include <stdlib.h>

#define FLASH_PAGE_SIZE        256
#define FLASH_SECTOR_SIZE      4096

#define FLASH_CMD_PAGE_PROGRAM 0x02
#define FLASH_CMD_READ         0x03
#define FLASH_CMD_STATUS       0x05
#define FLASH_CMD_WRITE_EN     0x06
#define FLASH_CMD_SECTOR_ERASE 0x20

#define FLASH_CMD_JEDEC_ID     0x9F
#define FLASH_CMD_LEGACY_ID    0x90

#define FLASH_STATUS_BUSY_MASK 0x01

void flash_read(const pio_spi_inst_t *spi, uint32_t addr, uint8_t *buf, size_t len);
void flash_write_enable(const pio_spi_inst_t *spi);
void flash_wait_done(const pio_spi_inst_t *spi);
void flash_page_program(const pio_spi_inst_t *spi, uint32_t addr, uint8_t data[]);

void flash_read_jedec_id(const pio_spi_inst_t *spi);
void flash_read_manufacturer_id(const pio_spi_inst_t *spi);