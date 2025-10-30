#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

// SPI pins (default for SPI0)
#define SPI_PORT spi0
#define PIN_MISO 16
#define PIN_CS   17
#define PIN_SCK  18
#define PIN_MOSI 19

#define CMD_PAGE_PROGRAM        0x02
#define CMD_READ_DATA           0x03
#define CMD_SECTOR_ERASE        0x20
#define CMD_RDSR                0x05
#define CMD_WREN                0x06
#define CMD_READ_REG3           0x15
#define CMD_JEDEC_ID            0x9F
#define CMD_SECTOR_BLOCK_LOCK   0x3D

#define TEST_ADDR 0x000000
#define TEST_PAGE_SIZE 256

void flash_select()   { gpio_put(PIN_CS, 0); }
void flash_deselect() { gpio_put(PIN_CS, 1); }

void flash_write_enable() {
    uint8_t cmd = CMD_WREN;
    flash_select();
    spi_write_blocking(SPI_PORT, &cmd, 1);
    flash_deselect();
    sleep_us(5); // small tCSH delay
}

uint8_t flash_read_status() {
    uint8_t tx[] = { CMD_RDSR, 0x00 };
    uint8_t rx[2];
    flash_select();
    spi_write_read_blocking(SPI_PORT, tx, rx, 2);
    flash_deselect();
    return rx[1];
}

void flash_wait_for_not_busy() {
    // WIP is bit 0 of the status register
    while (flash_read_status() & 0x01) {
        sleep_us(100);
    }
}

void flash_read_jedec_id(uint8_t *buf) {
    uint8_t cmd = CMD_JEDEC_ID;
    flash_select();
    spi_write_blocking(SPI_PORT, &cmd, 1);
    spi_read_blocking(SPI_PORT, 0x00, buf, 3);
    flash_deselect();
}

void flash_read_data(uint32_t addr, uint8_t *rx_buf, size_t len) {
    // Construct command buffer: {CMD_READ_DATA, Addr[23:16], Addr[15:8], Addr[7:0]}
    uint8_t tx_buf[4] = {
        CMD_READ_DATA,
        (addr >> 16) & 0xFF,
        (addr >> 8) & 0xFF,
        addr & 0xFF
    };

    flash_select();
    // Send the read command and address
    spi_write_blocking(SPI_PORT, tx_buf, 4);
    // Read the data
    spi_read_blocking(SPI_PORT, 0x00, rx_buf, len);
    flash_deselect();
}

uint32_t benchmark_read(uint32_t addr, uint8_t *rx_buf, size_t len) {
    // Construct command buffer: {CMD_READ_DATA, Addr[23:16], Addr[15:8], Addr[7:0]}
    uint8_t tx_buf[4] = {
        CMD_READ_DATA,
        (addr >> 16) & 0xFF,
        (addr >> 8) & 0xFF,
        addr & 0xFF
    };

    flash_select();
    // Send the read command and address
    spi_write_blocking(SPI_PORT, tx_buf, 4);
    // Read the data
    spi_read_blocking(SPI_PORT, 0x00, rx_buf, len);
    flash_deselect();
}

void flash_page_program(uint32_t addr, const uint8_t *data, size_t len) {
    // Construct command buffer: {CMD_PAGE_PROGRAM, Addr[23:16], Addr[15:8], Addr[7:0]}
    uint8_t tx_buf[4] = {
        CMD_PAGE_PROGRAM,
        (addr >> 16) & 0xFF,
        (addr >> 8) & 0xFF,
        addr & 0xFF
    };

    flash_write_enable(); // Send WREN (0x06) first

    flash_select();
    // Send the program command and address
    spi_write_blocking(SPI_PORT, tx_buf, 4);
    // Send the data
    spi_write_blocking(SPI_PORT, data, len);
    flash_deselect();

    // Wait for the program to complete (WIP bit to clear)
    flash_wait_for_not_busy();
}

uint8_t flash_read_status_reg3() {
    uint8_t tx[] = { CMD_READ_REG3, 0x00 }; // {CMD, DUMMY_BYTE}
    uint8_t rx[2];
    flash_select();
    spi_write_read_blocking(SPI_PORT, tx, rx, 2);
    flash_deselect();
    return rx[1]; // Status is in the second byte
}

uint8_t flash_read_block_lock(uint32_t addr) {
    uint8_t tx_buf[4] = {
        CMD_SECTOR_BLOCK_LOCK,  // Read Block/Sector Lock command
        (addr >> 16) & 0xFF,    // A23-A16
        (addr >> 8) & 0xFF,     // A15-A8
        addr & 0xFF             // A7-A0
    };
    uint8_t rx_buf[1];

    flash_select();
    spi_write_blocking(SPI_PORT, tx_buf, 4);
    spi_read_blocking(SPI_PORT, 0x00, rx_buf, 1); // Read the 1-byte lock value
    flash_deselect();

    return rx_buf[0];
}

void flash_sector_erase(uint32_t addr) {
    // Construct command buffer: {CMD_SECTOR_ERASE, Addr[23:16], Addr[15:8], Addr[7:0]}
    uint8_t tx_buf[4] = {
        CMD_SECTOR_ERASE,
        (addr >> 16) & 0xFF,
        (addr >> 8) & 0xFF,
        addr & 0xFF
    };

    flash_write_enable(); // Send WREN (0x06) first

    flash_select();
    spi_write_blocking(SPI_PORT, tx_buf, 4);
    flash_deselect();

    // Wait for the erase to complete (WIP bit to clear)
    flash_wait_for_not_busy();
}

// Helper function to print a buffer
void print_buf(const uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        printf("%02X ", buf[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    printf("\n");
}

void perform_benchmark() {
    puts("starting benchmark");

    // 1. Check which protection scheme is active
    uint8_t status_reg3 = flash_read_status_reg3();
    // WPS is bit S18 (bit 2 of Status Register-3)
    bool wps_enabled = (status_reg3 & 0x04) ? true : false;
    printf("Status Register 3: 0x%02X\n", status_reg3);
    printf(" - Individual Lock Scheme (WPS=1): %s\n", wps_enabled ? "Active" : "Inactive");
    printf(" - Default Protection Scheme (WPS=0): %s\n", wps_enabled ? "Inactive" : "Active");


    // 2. Check the lock status for the start address (0x000000)
    //    This command works regardless of the WPS bit setting, but its
    //    write-protection effect only applies if WPS=1.
    //    However, it's the most direct way to read a lock bit.
    uint32_t start_addr = 0x000000;
    uint8_t lock_status = flash_read_block_lock(start_addr);
    
    // Per the datasheet, LSB=1 means locked 
    bool is_locked = (lock_status & 0x01) ? true : false; 
    
    printf("Checking lock status for address 0x%06X...\n", start_addr);
    printf(" - Lock byte returned: 0x%02X\n", lock_status);

    if (is_locked) {
        printf(" - Result: Address 0x%06X is LOCKED.\n", start_addr);
    } else {
        printf(" - Result: Address 0x%06X is UNLOCKED (usable).\n", start_addr);
    }

    // begin benchmark
    uint8_t read_buf[TEST_PAGE_SIZE];
    memset(read_buf, 0, TEST_PAGE_SIZE); // Clear read buffer
    flash_read_data(TEST_ADDR, read_buf, TEST_PAGE_SIZE);
    print_buf(read_buf, TEST_PAGE_SIZE);
    puts("read done");

    puts("performing erase test");
    flash_sector_erase(TEST_ADDR);
    puts("erase done");

    puts("performing read on erased page");
    memset(read_buf, 0, TEST_PAGE_SIZE); // Clear read buffer
    flash_read_data(TEST_ADDR, read_buf, TEST_PAGE_SIZE);
    print_buf(read_buf, TEST_PAGE_SIZE);
    puts("read done");

    puts("performing write test");
    uint8_t write_buf[TEST_PAGE_SIZE];
    for (int i = 0; i < TEST_PAGE_SIZE; i++) {
        // write_buf[i] = i ^ 0xFF;
        write_buf[i] = i;
        // printf("%u", i);
    }
    print_buf(write_buf, TEST_PAGE_SIZE);
    flash_page_program(TEST_ADDR, write_buf, TEST_PAGE_SIZE);
    puts("write test finished");

    puts("performing read on written page");
    memset(read_buf, 0, TEST_PAGE_SIZE); // Clear read buffer
    flash_read_data(TEST_ADDR, read_buf, TEST_PAGE_SIZE);
    print_buf(read_buf, TEST_PAGE_SIZE);
    puts("read done");

    if (memcmp(write_buf, read_buf, TEST_PAGE_SIZE) == 0) {
        printf(">>> SUCCESS: Data read back matches data written! <<<\n");
    } else {
        printf(">>> FAILURE: Data mismatch! <<<\n");
    }

    puts("benchmark completed");
}

int main() {
    stdio_init_all();

    // Initialize SPI at 1 MHz
    spi_init(SPI_PORT, 1 * 1000 * 1000);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1); // deselect

    // sleep_ms(2000);
    // printf("Requesting JEDEC ID...\n");

    // // JEDEC ID command (0x9F)
    // uint8_t cmd = 0x9F;
    // uint8_t rx_buf[3] = {0};

    // gpio_put(PIN_CS, 0); // select
    // spi_write_blocking(SPI_PORT, &cmd, 1);
    // spi_read_blocking(SPI_PORT, 0x00, rx_buf, 3);
    // gpio_put(PIN_CS, 1); // deselect

    // printf("JEDEC ID: %02X %02X %02X\n", rx_buf[0], rx_buf[1], rx_buf[2]);

    sleep_ms(2000);
    printf("SPI Flash test start\n");

    // Read JEDEC ID
    uint8_t id[3];
    flash_read_jedec_id(id);
    printf("JEDEC ID: %02X %02X %02X\n", id[0], id[1], id[2]);

    // Send Write Enable
    printf("Sending Write Enable (0x06)...\n");
    flash_write_enable();

    // Read Status Register
    uint8_t status = flash_read_status();
    printf("Status Register: 0x%02X\n", status);

    // Interpret bits
    printf(" - WIP (Write in Progress): %d\n",  status & 0x01 ? 1 : 0);
    printf(" - WEL (Write Enable Latch): %d\n", status & 0x02 ? 1 : 0);

    perform_benchmark();

    while (true) {
        tight_loop_contents();
    }
}
