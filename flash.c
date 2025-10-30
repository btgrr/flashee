#include "flash.h"

void flash_read(const pio_spi_inst_t *spi, uint32_t addr, uint8_t *buf, size_t len) {
    uint8_t cmd[4] = {
            FLASH_CMD_READ,
            (addr >> 16) & 0xFF,
            (addr >> 8) & 0xFF,
            addr & 0xFF
    };
    gpio_put(spi->cs_pin, 0);
    pio_spi_write8_blocking(spi, cmd, 4);
    pio_spi_read8_blocking(spi, buf, len);
    gpio_put(spi->cs_pin, 1);
}

// Read Manufacturer & Device ID
void flash_read_manufacturer_id(const pio_spi_inst_t *spi) {
    int8_t cmd = FLASH_CMD_LEGACY_ID;

    gpio_put(spi->cs_pin, 0);
    pio_spi_write8_blocking(spi, &cmd, 1);

    uint8_t bufSize = 5;
    uint8_t *buf = malloc(sizeof(uint8_t) * bufSize);
    uint8_t *dummy = malloc(sizeof(uint8_t) * bufSize);

    // dummy bytes
    for (int i = 0; i < bufSize; i++) {
        dummy[i] = 0xFF;
    }

    pio_spi_write8_read8_blocking(spi, dummy, buf, bufSize);

    // pio_spi_read8_blocking(spi, buf, bufSize);
    // end of transaction
    gpio_put(spi->cs_pin, 1);
    
    for (int i = 0; i < bufSize; i++) {
        printf("%02x", buf[i]);
    }

    puts("");
    free(buf);
    free(dummy);
}

void flash_read_jedec_id(const pio_spi_inst_t *spi) {
    int8_t cmd = FLASH_CMD_JEDEC_ID;
    // start of a transaction
    gpio_put(spi->cs_pin, 0);
    pio_spi_write8_blocking(spi, &cmd, 1);
    
    uint8_t bufSize = 3;
    uint8_t *buf = malloc(sizeof(uint8_t) * bufSize);
    uint8_t *dummy = malloc(sizeof(uint8_t) * bufSize);

    // dummy bytes
    for (int i = 0; i < bufSize; i++) {
        dummy[i] = 0xFF;
    }

    pio_spi_write8_read8_blocking(spi, dummy, buf, bufSize);

    // pio_spi_read8_blocking(spi, buf, bufSize);
    // end of transaction
    gpio_put(spi->cs_pin, 1);
    
    for (int i = 0; i < bufSize; i++) {
        printf("%02x", buf[i]);
    }

    puts("");
    free(buf);
    free(dummy);
}

void flash_write_enable(const pio_spi_inst_t *spi) {
    uint8_t cmd = FLASH_CMD_WRITE_EN;
    gpio_put(spi->cs_pin, 0);
    pio_spi_write8_blocking(spi, &cmd, 1);
    gpio_put(spi->cs_pin, 1);
}

void flash_wait_done(const pio_spi_inst_t *spi) {
    uint8_t status;
    do {
        gpio_put(spi->cs_pin, 0);
        uint8_t cmd = FLASH_CMD_STATUS;
        pio_spi_write8_blocking(spi, &cmd, 1);
        pio_spi_read8_blocking(spi, &status, 1);
        gpio_put(spi->cs_pin, 1);
    } while (status & FLASH_STATUS_BUSY_MASK);
}

void flash_page_program(const pio_spi_inst_t *spi, uint32_t addr, uint8_t data[]) {
    flash_write_enable(spi);
    uint8_t cmd[4] = {
            FLASH_CMD_PAGE_PROGRAM,
            (addr >> 16) & 0xFF,
            (addr >> 8) & 0xFF,
            addr & 0xFF
    };
    gpio_put(spi->cs_pin, 0);
    pio_spi_write8_blocking(spi, cmd, 4);
    pio_spi_write8_blocking(spi, data, FLASH_PAGE_SIZE);
    gpio_put(spi->cs_pin, 1);
    flash_wait_done(spi);
}
