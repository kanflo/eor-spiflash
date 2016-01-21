# ESP8266 Open RTOS SPI flash driver

This module adds support for accessing external SPI flash.

###Usage

```
cd esp-open-rtos/extras
git clone https://github.com/kanflo/eor-spi.git spi
git clone https://github.com/kanflo/eor-spiflash.git spiflash
```

Include the driver in your project makefile as any other extra component:

```
EXTRA_COMPONENTS=extras/spi extras/spiflash
```

See ```example/spiflashmon.c``` for a complete example while noting that the makefile depends on the environment variable ```$EOR_ROOT``` pointing to your ESP8266 Open RTOS root.
