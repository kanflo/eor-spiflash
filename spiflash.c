/* 
 * The MIT License (MIT)
 * 
 * Copyright (c) 2016 Johan Kanflo (github.com/kanflo)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <esp8266.h>
#include <string.h>
#include <spi.h>
#include <spiflash.h>
#include <FreeRTOS.h>
#include <task.h> // vTaskDelay


#define CMD_GETID            (0x9f)
#define CMD_READ_STATUS      (0x05)
#define CMD_WRITE_ENABLE     (0x06)
#define CMD_WRITE_DISABLE    (0x04)
#define CMD_PAGE_PROGRAM     (0x02)
#define CMD_READ_DATA        (0x03)
#define CMD_READ_DATA_HISPD  (0x0b)
#define CMD_ERASE_SECTOR     (0xd8) // 4kb
#define CMD_ERASE_SUBSECTOR  (0x20) // 64kb
#define CMD_ERASE_CHIP       (0xc7)

#define STATUS_WIP           (1 << 0)
#define STATUS_WEL           (1 << 1)
#define STATUS_BP0           (1 << 2)
#define STATUS_BP1           (1 << 3)
#define STATUS_BP2           (1 << 4)
#define STATUS_TB            (1 << 5)
#define STATUS_SWDW          (1 << 7)

#define SUBSECTOR_SIZE       ( 4*1024)
#define SECTOR_SIZE          (64*1024)

#define PAGE_PROGRAM_SIZE    (256)

// Max number of chips we can place on a board
#define MAX_CHIPS (4)

typedef struct {
    uint8_t manufacturer;
    uint16_t jedecid;
    uint32_t size;
    char *description;
} spi_flash_t;


#define delay_ms(t) vTaskDelay((t) / portTICK_RATE_MS)
#define systime_ms() (xTaskGetTickCount()*portTICK_RATE_MS)

// Turn on debug prints
#ifdef CONFIG_SPIFLASH_DEBUG
 #define FLASH_PRINT(x) (x)
#else
 #define FLASH_PRINT(x)
#endif // CONFIG_SPIFLASH_DEBUG

static uint8_t cs_pins[MAX_CHIPS];
static uint8_t flash_types[MAX_CHIPS];
static uint8_t chip_count;

// List of supported flashes. Feel free to add the flash of your heart's desire
static spi_flash_t flashes[] = {
    { 0x20, 0x7114, 1024*1024, "Micron M25PX80" },
    { 0, 0, 0, 0 } // End marker
};

static uint8_t read_status(uint8_t cs_pin);
static void write_enable(uint8_t cs_pin);
static void write_disable(uint8_t cs_pin);
static int32_t check_flash(uint8_t manufacturer, uint16_t jedecid);
static void flash_cmd(uint8_t cs_pin, int8_t cmd);
static void chip_select(uint8_t cs_pin);
static void chip_unselect(uint8_t cs_pin);


int32_t spiflash_probe(uint8_t cs_pin)
{
    uint8_t cur_chip = chip_count;
    uint32_t flash_idx = -1;
    uint8_t info[20]; // TODO: Chip specific

    gpio_enable(cs_pin, GPIO_OUTPUT);
    chip_unselect(cs_pin);

    spi_init(iHSPI);

    memset(info, 0, sizeof(info));

    FLASH_PRINT(printf("Probing SPI flash on CS pin %u\n", cs_pin));

    chip_select(cs_pin);
    spi_tx8(iHSPI, CMD_GETID);

    for(int i = 0; i < sizeof(info); i++) {
        info[i] = spi_rx8(iHSPI);
    }

    chip_unselect(cs_pin);

    if (info[0] != 0xff) {
        FLASH_PRINT(printf("Read ID %02x %04x\n", info[0], info[1] << 8 | info[2]));
        flash_idx = check_flash(info[0], info[1] << 8 | info[2]);
    }

    if (flash_idx >= 0) {
        cs_pins[chip_count] = cs_pin;
        flash_types[chip_count] = flash_idx;
        chip_count++;
        FLASH_PRINT(printf("  Success\n"));
        return cur_chip;
    } else {
        FLASH_PRINT(printf("  Failed\n"));
        return -1;
    }
}

void spiflash_info(int8_t fd, uint8_t *manufacturer, uint16_t *jedecid, uint32_t *size, char **descr)
{
    if (fd < MAX_CHIPS) {
        if(manufacturer) *manufacturer = flashes[fd].manufacturer;
        if(jedecid) *jedecid = flashes[fd].jedecid;
        if(size) *size = flashes[fd].size;
        if(descr) *descr = flashes[fd].description;
    }
}

bool spiflash_read(int8_t fd, uint32_t address, uint32_t length, uint8_t *buffer)
{
    if (buffer && fd < MAX_CHIPS) {
        FLASH_PRINT(printf("Reading %u bytes from 0x%08x\n", (unsigned int) length, (unsigned int) address));
        chip_select(cs_pins[fd]);
        spi_tx8(iHSPI, CMD_READ_DATA);
        spi_tx8(iHSPI, address >> 16);
        spi_tx8(iHSPI, address >> 8);
        spi_tx8(iHSPI, address);
        while(length--) {
            *(buffer++) = (uint8_t) spi_rx8(iHSPI);
        }
        chip_unselect(cs_pins[fd]);
        return true;
    }
    return false;
}

bool spiflash_write(int8_t fd, uint32_t address, uint32_t length, uint8_t *buffer)
{
    if (fd < MAX_CHIPS) {
        int32_t remain = length;
        while(remain > 0) {
            uint8_t chunk_size = remain > PAGE_PROGRAM_SIZE ? PAGE_PROGRAM_SIZE : remain;
            uint8_t cs = cs_pins[fd];
            FLASH_PRINT(printf("Writing %u bytes to 0x%08x\n", (unsigned int) length, (unsigned int) address));
            write_enable(cs);
            if (!(read_status(cs) & STATUS_WEL)) {
                FLASH_PRINT(printf("Error, flash did not latch WE\n"));
                return false;
            }
            chip_select(cs);
            FLASH_PRINT(printf("  %u bytes at 0x%08x\n", (unsigned int) chunk_size, (unsigned int) address));
            spi_tx8(iHSPI, CMD_PAGE_PROGRAM);
            spi_tx8(iHSPI, address >> 16);
            spi_tx8(iHSPI, address >> 8);
            spi_tx8(iHSPI, address);
            while(chunk_size--) {
                uint8_t ch = *(buffer++);
                FLASH_PRINT(printf("%c [%02x]\n", ch, ch));
                spi_tx8(iHSPI, ch);
            }
            chip_unselect(cs);
            write_disable(cs);
            while(read_status(cs) & STATUS_WIP) {
            // TODO: Handle timeout
                delay_ms(1);
            }
            address += PAGE_PROGRAM_SIZE;
            remain -= PAGE_PROGRAM_SIZE;
        }
    } else {
        return false;
    }
    return true;
}

bool spiflash_erase(int8_t fd, uint32_t address, uint32_t length)
{
   if (fd < MAX_CHIPS) {
        uint32_t address_aligned = address & ~(SUBSECTOR_SIZE-1);
        uint32_t end_address = address + length;
        length += address & ~(SUBSECTOR_SIZE-1);
        FLASH_PRINT(printf("Erasing %u bytes at 0x%08x\n", (unsigned int) length, (unsigned int) address_aligned));
        uint8_t cs = cs_pins[fd];
        while (end_address > address_aligned) {
            write_enable(cs);
            if (!(read_status(cs) & STATUS_WEL)) {
                FLASH_PRINT(printf("Error, flash did not latch WE\n"));
                return false;
            }
            chip_select(cs);
            // TODO: Optimise by erasing sectors when possible (~2x erase speed)
            spi_tx8(iHSPI, CMD_ERASE_SUBSECTOR);
            FLASH_PRINT(printf("  Erasing subsector at 0x%08x\n", (unsigned int) address_aligned));
            spi_tx8(iHSPI, address_aligned >> 16);
            spi_tx8(iHSPI, address_aligned >> 8);
            spi_tx8(iHSPI, address_aligned);
            chip_unselect(cs);
            delay_ms(70); // Subsector erase takes 70-150ms
            while(read_status(cs) & STATUS_WIP) {
            // TODO: Handle timeout
                delay_ms(5);
            }
            FLASH_PRINT(printf("    Done\n"));
            address_aligned += SUBSECTOR_SIZE;
        }
        write_disable(cs);
        return true;
    }
    return false;
}

bool spiflash_chip_erase(int8_t fd)
{
    if (fd < MAX_CHIPS) {
        FLASH_PRINT(printf("Erasing chip\n"));
        uint8_t status, cs = cs_pins[fd];
        write_enable(cs);
        status = read_status(cs);
        if (!(status & STATUS_WEL)) {
            FLASH_PRINT(printf("Error, flash did not latch WE\n"));
            return false;
        } else {
            flash_cmd(cs, CMD_ERASE_CHIP);
            while(1) {
                // TODO: Handle timeout
                status = read_status(cs);
                if (!(status & STATUS_WIP)) {
                    FLASH_PRINT(printf("Erase done\n"));
                    return true;
                }
                delay_ms(25);
            }
        }
    } else {
        return false;
    }
    return true; // Actually never reached
}

static uint8_t read_status(uint8_t cs_pin)
{
    uint8_t status = 0;
    chip_select(cs_pin);
    spi_tx8(iHSPI, CMD_READ_STATUS);
    status = spi_rx8(iHSPI);
    chip_unselect(cs_pin);
    return status;
}

static void flash_cmd(uint8_t cs_pin, int8_t cmd)
{
    chip_select(cs_pin);
    spi_tx8(iHSPI, (uint8_t) cmd);
    chip_unselect(cs_pin);
}

static void write_enable(uint8_t cs_pin)
{
    FLASH_PRINT(printf("Write enable\n"));
    flash_cmd(cs_pin, CMD_WRITE_ENABLE);
}

static void write_disable(uint8_t cs_pin)
{
    flash_cmd(cs_pin, CMD_WRITE_DISABLE);
}

static void chip_select(uint8_t cs_pin)
{
    gpio_write(cs_pin, 0);
}

static void chip_unselect(uint8_t cs_pin)
{
    gpio_write(cs_pin, 1);
}

static int32_t check_flash(uint8_t manufacturer, uint16_t jedecid)
{
    uint32_t rover = 0;
    do {
        if (flashes[rover].manufacturer == manufacturer &&
            flashes[rover].jedecid == jedecid) {
            FLASH_PRINT(printf("Found %s\n", flashes[rover].description));
            return rover;
        }
        rover++;
    } while(flashes[rover].manufacturer != 0);
    return -1;
}
