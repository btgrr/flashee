#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"

#include "device/usbd.h"
#if CFG_TUD_CDC
  #include "class/cdc/cdc_device.h"
#endif

#define HIGH_PIN_1 4
#define HIGH_PIN_2 5

#define BTN_PIN_1 20 // restart read write op
#define BTN_PIN_2 21 // detect

// #define PICO_DEFAULT_SPI_RX_PIN 0
// #define PICO_DEFAULT_SPI_SCK_PIN 2
// #define PICO_DEFAULT_SPI_TX_PIN 3
// #define PICO_DEFAULT_SPI_CSN_PIN 1

#include "flash.h"

typedef struct chip_actions {
    uint8_t chipDetect;
    uint8_t chipRead;
    uint8_t chipWrite;
} chip_actions_t;

chip_actions_t actions = {
    .chipDetect = 0,
    .chipRead = 0,
    .chipWrite = 0
};

void printbuf(const uint8_t buf[FLASH_PAGE_SIZE]) {
    for (int i = 0; i < FLASH_PAGE_SIZE; ++i)
        printf("%02x%c", buf[i], i % 16 == 15 ? '\n' : ' ');
}

void irq_callback(uint gpio, uint32_t event_mask) {
    // GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL

    if (event_mask & GPIO_IRQ_EDGE_FALL) {
        if (gpio == BTN_PIN_1) {
            actions.chipRead = 1;
        } else if (gpio == BTN_PIN_2) {
            actions.chipDetect = 1;
        }
    }
}

int main()
{
    stdio_init_all();

    while (!tud_cdc_connected()) {
        printf(".");
        sleep_ms(500);
    }

    printf("usb host detected.\n");

    spi_init(spi_default, 1000 * 1000);

    pio_spi_inst_t spi = {
        .pio = pio0,
        .sm = 0,
        .cs_pin = PICO_DEFAULT_SPI_CSN_PIN
    };

    gpio_init(BTN_PIN_1);
    gpio_init(BTN_PIN_2);
    gpio_set_dir(BTN_PIN_1, GPIO_IN);
    gpio_set_dir(BTN_PIN_2, GPIO_IN);
    gpio_set_irq_enabled_with_callback(BTN_PIN_1, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &irq_callback);
    gpio_set_irq_enabled(BTN_PIN_2, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);

    gpio_init(HIGH_PIN_1);
    gpio_init(HIGH_PIN_2);

    gpio_set_dir(HIGH_PIN_1, GPIO_OUT);
    gpio_set_dir(HIGH_PIN_2, GPIO_OUT);

    gpio_put(HIGH_PIN_1, 1);
    gpio_put(HIGH_PIN_2, 1);

    gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);

    // select the target device
    gpio_init(PICO_DEFAULT_SPI_CSN_PIN);
    gpio_set_dir(PICO_DEFAULT_SPI_CSN_PIN, GPIO_OUT); 
    gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);

    uint offset = pio_add_program(spi.pio, &spi_cpha0_program);
    printf("Loaded program at %d\n", offset);

    pio_spi_init(spi.pio, spi.sm, offset,
                 8,       // 8 bits per SPI frame
                 31.25f,  // 1 MHz @ 125 clk_sys
                 false,   // CPHA = 0
                 false,   // CPOL = 0
                 PICO_DEFAULT_SPI_SCK_PIN,
                 PICO_DEFAULT_SPI_TX_PIN,
                 PICO_DEFAULT_SPI_RX_PIN
    );

    while (true) {
        if (actions.chipDetect == 1) {
            printf("flash detection\n");

            puts("0x9f jedec id test");
            flash_read_jedec_id(&spi);

            puts("0x90 manufacturer and id test");
            flash_read_manufacturer_id(&spi);
            actions.chipDetect = 0;
        }

        if (actions.chipRead == 1) {
            uint8_t page_buf[FLASH_PAGE_SIZE];
    
            puts("Flash Reading");
            flash_read(&spi, 0, page_buf, FLASH_PAGE_SIZE);
            puts("Flash Read Done");
            printbuf(page_buf);
            for (int i = 0; i < FLASH_PAGE_SIZE; ++i)
                page_buf[i] = i;
            puts("filling up buffer with numbers");
            printbuf(page_buf);
            flash_page_program(&spi, 0, page_buf);
            flash_read(&spi, 0, page_buf, FLASH_PAGE_SIZE);

            puts("After program:");
            printbuf(page_buf);

            actions.chipRead = 0;
        }
        sleep_ms(10);
    }
}
