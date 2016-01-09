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

#ifndef __SPIFLASH_H__
#define __SPIFLASH_H__

#include <stdint.h>
#include <stdbool.h>

// Init SPI flash driver with CS on GPIO pin cs_pin
// Returns a file descriptor for use in later calls or -1 if failure
int32_t spiflash_probe(uint8_t cs_pin);

void spiflash_info(int8_t fd, uint8_t *manufacturer, uint16_t *jedecid, uint32_t *size, char **descr);

bool spiflash_read(int8_t fd, uint32_t address, uint32_t length, uint8_t *buffer);

bool spiflash_write(int8_t fd, uint32_t address, uint32_t length, uint8_t *buffer);

bool spiflash_erase(int8_t fd, uint32_t address, uint32_t length);

bool spiflash_chip_erase(int8_t fd);

#endif // __SPIFLASH_H__
