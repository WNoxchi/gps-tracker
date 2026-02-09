/* hw_config.c
 * Hardware configuration for GPS Tracker SD card interface
 *
 * SPI0 Configuration:
 * - MISO (GP16), MOSI (GP19), CLK (GP18), CS (GP17)
 * - Baud rate: 12.5 MHz (conservative for SD card compatibility)
 */

#include <string.h>
#include "ff.h"
#include "sd_card.h"
#include "hw_config.h"

/* Hardware Configuration of SPI "objects" */
static spi_t spis[] = {
    {
        .hw_inst = spi0,
        .miso_gpio = 16,
        .mosi_gpio = 19,
        .sck_gpio = 18,
        .baud_rate = 12500 * 1000,  /* 12.5 MHz */
    }
};

/* Hardware Configuration of the SD Card "objects" */
static sd_card_t sd_cards[] = {
    {
        .pcName = "0:",           /* Mount point */
        .spi = &spis[0],          /* SPI bus */
        .ss_gpio = 17,            /* Chip Select GPIO */
        .use_card_detect = false, /* No card detect pin */
        .card_detect_gpio = 0,
        .card_detected_true = 1
    }
};

size_t sd_get_num(void) {
    return sizeof(sd_cards) / sizeof(sd_cards[0]);
}

sd_card_t *sd_get_by_num(size_t num) {
    if (num < sd_get_num()) {
        return &sd_cards[num];
    }
    return NULL;
}

size_t spi_get_num(void) {
    return sizeof(spis) / sizeof(spis[0]);
}

spi_t *spi_get_by_num(size_t num) {
    if (num < spi_get_num()) {
        return &spis[num];
    }
    return NULL;
}
