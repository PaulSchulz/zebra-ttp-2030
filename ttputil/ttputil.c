#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <linux/lp.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>       

#define NUL 0x00
#define ENQ 0x05
#define ACK 0x06
#define NAK 0x15
#define ESC 0x1B
#define RS  0x1E

int enquiry(int fd, char type, char *buf, int buf_len) {
    char enquiry_cmd[3] = { ESC, ENQ, type };
    write(fd, enquiry_cmd, sizeof(enquiry_cmd));
    return read(fd, buf, buf_len);
}

bool check_length(int real_len, int expected_len) {
    if (real_len < expected_len) {
        fprintf(stderr, "Error: only %d bytes received, expected %d\n", real_len, expected_len);
        return false;
    }
    return true;
}

char *status_text(char code) {
    switch(code) {
        case 0x00:  return "OK";
        case 0x01:  return "Paper left in presenter module. Attempt to clear the paper path failed";
        case 0x02:  return "Cutter jammed";
        case 0x03:  return "Out of paper";
        case 0x04:  return "Print Head lifted";
        case 0x05:  return "Paper-feed error. No paper in presenter although 10cm has been printed";
        case 0x06:  return "Temperature error. Print head temperature exceeded 60°C limit";
        case 0x07:  return "Presenter not running";
        case 0x08:  return "Paper jam during retract";

        case 0x0A:  return "Black mark not found";
        case 0x0B:  return "Black mark calibration error";
        case 0x0C:  return "Index error";
        case 0x0D:  return "Checksum error";
        case 0x0E:  return "Wrong firmware type or targer for firmware loading";
        case 0x0F:  return "Firmware cannot start because no firmware is loaded or firmware checksum is wrong";
        case 0x10:  return "Retract function timed out";

        default:    return "unknown";
    }
}

void print_ack_nak(char *buf, int len) {
    switch (buf[0]) {
        case ACK:
            printf("ACK: OK\n");
            break;
        case NAK:
            if (check_length(len, 2))
                printf("NAK %02hhx: %s\n", buf[1], status_text(buf[1]));
            break;
        default:
            fprintf(stderr, "Error: invalid reply %d\n", buf[0]);
            break;
    }
}

void print_paper(char *buf) {
    switch (buf[0]) {
        case 0:
            printf("Paper present\n");
            break;
        case 1:
            printf("Paper low\n");
            break;
        default:
            fprintf(stderr, "Error: invalid reply %d\n", buf[0]);
            break;
    }
}

void print_string(char *buf, int len) {
    for (int i = 0; i < len; i++)
        printf("%c", buf[i]);
    printf("\n");
}

void print_hex(char *buf, int len) {
    printf("0x");
    for (int i = 0; i < len; i++)
        printf("%02hhX", buf[i]);
    printf("\n");
}

void print_version(char *buf) {
    printf("%hhu.%hhu\n", buf[0], buf[1]);
}

void print_temp(char *buf) {
    printf("%hhd°C\n", buf[0]);
}

void upload_file(FILE *f, int fd) {
    char buf[1024];

    while (!feof(f)) {
        int len = fread(buf, 1, sizeof(buf), f);
        write(fd, f, len);
    }
    fclose(f);
}

void usage() {
    fprintf(stderr, "Control utility for Swecoin/Zebra TTP series printers v1.0\n");
    fprintf(stderr, "Copyright (c) 2009 Ondrej Zary\n\n");
    fprintf(stderr, "Usage: ttputil <command> [options] <device>\n");
    fprintf(stderr, "  e.g. ttputil enquiry fw-ver /dev/lp0\n\n");
    fprintf(stderr, "Command list:\n");
    fprintf(stderr, " enquiry status-enq|paper|fonts|sensor|status|fw-ver|board-sn|board-rev|head-temp|boot-ver|device-id|ext-status\n");
    fprintf(stderr, " get-param <number>\n");
    fprintf(stderr, " set-param <number> <value>\n");
    fprintf(stderr, " save-params\n");
    fprintf(stderr, " reset\n");
    fprintf(stderr, " reset-full\n");
    fprintf(stderr, " print-test\n");
    fprintf(stderr, " print-font\n");
    fprintf(stderr, " cut\n");
    fprintf(stderr, " cut-eject\n");
    fprintf(stderr, " load-font <filename>\n");
    fprintf(stderr, " erase-fonts-all\n");
    fprintf(stderr, " erase-fonts-4-7\n");
    fprintf(stderr, " load-logotype <filename>\n");
    fprintf(stderr, " erase-logotypes-all\n");
    fprintf(stderr, " load-firmware <filename>\n");
}

int main(int argc, char *argv[]) {
    int len;
    char buf[1024];

    if (argc < 3) {
        usage();
        return 1;
    }

    int fd = open(argv[argc-1], O_RDWR);
    if (fd < 0) {
        perror("Error opening device");
        return 2;
    }

    if (!strcmp(argv[1], "enquiry")) {
        if (!strcmp(argv[2], "status-enq")) {
            len = enquiry(fd, 0x01, buf, sizeof(buf));
            if (check_length(len, 1))
                print_ack_nak(buf, len);
        } else if (!strcmp(argv[2], "paper")) {
            len = enquiry(fd, 0x02, buf, sizeof(buf));
            if (check_length(len, 1))
                print_paper(buf);
        } else if (!strcmp(argv[2], "fonts")) {
            len = enquiry(fd, 0x04, buf, sizeof(buf));
            print_string(buf, len);
        } else if (!strcmp(argv[2], "sensor")) {
            len = enquiry(fd, 0x05, buf, sizeof(buf));
            if (check_length(len, 2))
                print_hex(buf, len);
        } else if (!strcmp(argv[2], "status")) {
            len = enquiry(fd, 0x06, buf, sizeof(buf));
            if (check_length(len, 2))
                print_hex(buf, len);
        } else if (!strcmp(argv[2], "fw-ver")) {
            len = enquiry(fd, 0x07, buf, sizeof(buf));
            if (check_length(len, 2))
                print_version(buf);
        } else if (!strcmp(argv[2], "board-sn")) {
            len = enquiry(fd, 0x09, buf, sizeof(buf));
            check_length(len, 6);
            print_hex(buf, len);
        } else if (!strcmp(argv[2], "board-rev")) {
            len = enquiry(fd, 0x0A, buf, sizeof(buf));
            if (check_length(len, 1))
                print_string(buf, len);
        } else if (!strcmp(argv[2], "head-temp")) {
            len = enquiry(fd, 0x0B, buf, sizeof(buf));
            if (check_length(len, 1))
                print_temp(buf);
        } else if (!strcmp(argv[2], "boot-ver")) {
            len = enquiry(fd, 0x0C, buf, sizeof(buf));
            if (check_length(len, 2))
                print_version(buf);
        } else if (!strcmp(argv[2], "device-id")) {
            len = enquiry(fd, 0x63, buf, sizeof(buf));
            if (check_length(len, 2))
                print_string(buf+2, len-2);
        } else if (!strcmp(argv[2], "ext-status")) {
            len = enquiry(fd, 'E', buf, sizeof(buf));
            if (check_length(len, 4))
                print_hex(buf, len);
        } else {
            fprintf(stderr, "Invalid enquiry command: '%s'\n", argv[2]);
            return 1;
        }
    /* Get value of parameter argv[2] */
    } else if (!strcmp(argv[1], "get-param")) {
        char enquiry_cmd[4] = { ESC, ENQ, 'P', atoi(argv[2]) };
        write(fd, enquiry_cmd, sizeof(enquiry_cmd));
        len = read(fd, buf, sizeof(buf));
        printf("%hhu\n", buf[0]);
    /* Set parameter argv[2] to value argv[3] */
    } else if (!strcmp(argv[1], "set-param")) {
        char setparam_cmd[5] = { ESC, '&', 'P', atoi(argv[2]), atoi(argv[3]) };
        write(fd, setparam_cmd, sizeof(setparam_cmd));
    /* Save current parameters to EEPROM */
    } else if (!strcmp(argv[1], "save-params")) {
        char save_cmd[3] = { ESC, '&', 4 };
        write(fd, save_cmd, sizeof(save_cmd));
    /* Reset (initialize) */
    } else if (!strcmp(argv[1], "reset")) {
        char reset_cmd[2] = { ESC, '@' };
        write(fd, reset_cmd, sizeof(reset_cmd));
    /* Reset (full) */
    } else if (!strcmp(argv[1], "reset-full")) {
        char reset_cmd[2] = { ESC, '?' };
        write(fd, reset_cmd, sizeof(reset_cmd));
    /* Print self-test */
    } else if (!strcmp(argv[1], "print-test")) {
        char test_cmd[3] = { ESC, 'P', 0 };
        write(fd, test_cmd, sizeof(test_cmd));
    /* Print character set */
    } else if (!strcmp(argv[1], "print-font")) {
        char test_cmd[3] = { ESC, 'P', 1 };
        write(fd, test_cmd, sizeof(test_cmd));
    /* Cut */
    } else if (!strcmp(argv[1], "cut")) {
        char cut_cmd[2] = { ESC, RS };
        write(fd, cut_cmd, sizeof(cut_cmd));
    /* Cut and eject */
    } else if (!strcmp(argv[1], "cut-eject")) {
        char cuteject_cmd[1] = { RS };
        write(fd, cuteject_cmd, sizeof(cuteject_cmd));
    /* Load font */
    } else if (!strcmp(argv[1], "load-font")) {
        FILE *font = fopen(argv[2], "r");
        if (!font) {
            perror("Error opening font");
            return 2;
        }
        char loadfont_cmd[3] = { ESC, '&', 0 };
        write(fd, loadfont_cmd, sizeof(loadfont_cmd));
        upload_file(font, fd);
    /* Erase all fonts */
    } else if (!strcmp(argv[1], "erase-fonts-all")) {
        char erasefonts_cmd[3] = { ESC, '&', 'C' };
        write(fd, erasefonts_cmd, sizeof(erasefonts_cmd));
    /* Erase fonts 4-7 */
    } else if (!strcmp(argv[1], "erase-fonts-4-7")) {
        char erasefonts_cmd[3] = { ESC, '&', 'D' };
        write(fd, erasefonts_cmd, sizeof(erasefonts_cmd));
    /* Load logotype */
    } else if (!strcmp(argv[1], "load-logotype")) {
        FILE *logotype = fopen(argv[2], "r");
        if (!logotype) {
            perror("Error opening logotype");
            return 2;
        }
        char loadlogotype_cmd[3] = { ESC, '&', 1 };
        write(fd, loadlogotype_cmd, sizeof(loadlogotype_cmd));
        upload_file(logotype, fd);
    /* Erase all logotypes */
    } else if (!strcmp(argv[1], "erase-logotypes-all")) {
        char eraselogotypes_cmd[3] = { ESC, '&', 'L' };
        write(fd, eraselogotypes_cmd, sizeof(eraselogotypes_cmd));
    /* Load firmware (firmware upgrade) */
    } else if (!strcmp(argv[1], "load-firmware")) {
        FILE *firmware = fopen(argv[2], "r");
        if (!firmware) {
            perror("Error opening firmware");
            return 2;
        }
        char fwupgrade_cmd[2] = { ESC, NUL };
        write(fd, fwupgrade_cmd, sizeof(fwupgrade_cmd));
        upload_file(firmware, fd);
    } else {
        fprintf(stderr, "Invalid command: '%s'\n", argv[1]);
        return 1;
    }

    close(fd);
    return 0;
}
