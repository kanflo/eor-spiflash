/* SPI flash driver example
 *
 * This sample code is in the public domain.
 */

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <esp8266.h>
#include <esp/uart.h>
#include <spiflash.h>
#include <FreeRTOS.h>
#include <task.h>
#include <stdlib.h>
#include <espressif/esp_common.h>


#define MAX_ARGC (10)
#define BUFFER_SIZE (64)

#define SPI_CS (5)

int32_t fd;


// Courtesy of @paxdiablo on SO (http://stackoverflow.com/a/7776146)
void hexDump (int addr, void *data, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)data;

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printf ("  %s\n", buff);

            // Output the offset.
            printf ("  %04x ", addr+i);
        }

        // Now the hex code for the specific character.
        printf (" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII bit.
    printf ("  %s\n", buff);
}

static void cmd_info(uint32_t argc, char *argv[])
{
    (void) argc;
    (void) argv;

    uint32_t size;
    char *descr;

    spiflash_info(fd, 0 /*manufacturer*/, 0/*jedecid*/, &size, &descr);
    printf("Type : %s\n", descr);
    printf("Size : %d kBytes\n", size/1024);
}

static void cmd_chiperase(uint32_t argc, char *argv[])
{
    (void) argc;
    (void) argv;
    printf("Chip erase in progress...");
    if (spiflash_chip_erase(fd)) {
        printf("ok\n");
    } else {
        printf("failed!\n");
    }
}

static void cmd_read(uint32_t argc, char *argv[])
{
    if (argc == 3) {
        char data[BUFFER_SIZE];
        uint32_t address = atoi(argv[1]);
        uint32_t length = atoi(argv[2]);
        int32_t remain = length;
        printf("Reading %d bytes from 0x%08x\n", length, address);
        while (remain > 0) {
            if (spiflash_read(fd, address, remain > BUFFER_SIZE ? BUFFER_SIZE : remain, (uint8_t*) data)) {
                hexDump(address, (uint8_t*) data, remain > BUFFER_SIZE ? BUFFER_SIZE : remain);
                address += BUFFER_SIZE;
                remain -= BUFFER_SIZE;
            } else {
                printf("failed!\n");
                return;
            }
        }
    } else {
        printf("Error: read <address> <length>\n");
    }
}

static void cmd_write(uint32_t argc, char *argv[])
{
    char data[BUFFER_SIZE];
    int32_t remain = BUFFER_SIZE;
    data[0] = 0;
    if (argc >= 3) {
        uint32_t address = atoi(argv[1]);
        // Assemble all the remaining arguments as a string
        for(int i=2; i<argc; i++){
            if (*data) {
                strncat (data, " ", remain);
                remain--;
            }
            strncat(data, argv[i], remain);
            remain -= strlen(argv[i]);
            if (remain < 1)
                break;
        }
        // Note, we do not write the null terminator
        printf("Writing %d bytes to 0x%08x...", strlen(data), address);
        if (spiflash_write(fd, address, strlen(data), (uint8_t*) data)) {
            printf("ok\n");
        } else {
            printf("failed!\n");
        }

        printf("write %d '%s'\n", address, data);
    } else {
        printf("Error: write <address> <data>\n");
    }
}

static void cmd_erase(uint32_t argc, char *argv[])
{
    if (argc == 3) {
        uint32_t address = atoi(argv[1]);
        uint32_t length = atoi(argv[2]);
        printf("Erasing %d bytes at 0x%08x...", length, address);
        if (spiflash_erase(fd, address, length)) {
            printf("ok\n");
        } else {
            printf("failed!\n");
        }
    } else {
        printf("Error: erase <address> <length>\n");
    }
}

static void cmd_help(uint32_t argc, char *argv[])
{
    printf("info                         Print flash info\n");
    printf("chiperase                    Erase entire chip (slow!)\n");
    printf("read <addr> <len>            Read <len> bytes from <addr>\n");
    printf("write <addr> <data string>   Write the string <data string> to addr\n");
    printf("erase <addr> <len>           Erase <len> bytes starting at <addr>\n");
    printf("\nExample:\n");
    printf("  read 0 10<enter> reads 10 bytes from address 0\n");
    printf("  write 8 Hello World!<enter> write the string \"Hello World!\" (w/o quotes) to address 8\n");
    printf("Note that all addresses and lengthts are decimal\n");
}

static void handle_command(char *cmd)
{
    char *argv[MAX_ARGC];
    int argc = 1;
    char *temp, *rover;
    memset((void*) argv, 0, sizeof(argv));
    argv[0] = cmd;
    rover = cmd;
    // Split string "<command> <argument 1> <argument 2>  ...  <argument N>"
    // into argv, argc style
    while(argc < MAX_ARGC && (temp = strstr(rover, " "))) {
        rover = &(temp[1]);
        argv[argc++] = rover;
        *temp = 0;
    }

    if (strlen(argv[0]) > 0) {
        if (strcmp(argv[0], "help") == 0) cmd_help(argc, argv);
        else if (strcmp(argv[0], "info") == 0) cmd_info(argc, argv);
        else if (strcmp(argv[0], "chiperase") == 0) cmd_chiperase(argc, argv);
        else if (strcmp(argv[0], "read") == 0) cmd_read(argc, argv);
        else if (strcmp(argv[0], "write") == 0) cmd_write(argc, argv);
        else if (strcmp(argv[0], "erase") == 0) cmd_erase(argc, argv);
        else printf("Unknown command %s, try 'help'\n", argv[0]);
    }
}

static void spiflashmon()
{
    char ch;
    char cmd[81];
    int i = 0;
    printf("\n\n\nWelcome to spiflashmon. Type 'help<enter>' for, well, help\n");

    fd = spiflash_probe(SPI_CS);
    if (fd < 0) {
        printf("There is no known SPI flash on CS pin %d\n", SPI_CS);
        while(1) ;
    }
    printf("%% ");

    while(1) {
        if (read(0, (void*)&ch, 1)) { // 0 is stdin
            printf("%c", ch);
//            printf("%c (%02x)", ch, ch);
            if (ch == '\n' || ch == '\r') {
                cmd[i] = 0;
                i = 0;
                printf("\n");
                handle_command((char*) cmd);
                printf("%% ");
            } else if (ch == 3) {
                i = 0;
                printf("\n%% ");
            } else {
                if (i < sizeof(cmd)) cmd[i++] = ch;
            }
        }
    }
}

void user_init(void)
{
    uart_set_baud(0, 115200);
    setbuf(stdout, NULL);
    spiflashmon();
}
