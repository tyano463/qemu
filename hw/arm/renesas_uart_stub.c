#include "qemu/osdep.h"

#include "renesas_common.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <termios.h>
#include <unistd.h>

#define DEVICE "/dev/ttyUSB0"
#define BAUDRATE B115200
#define BUF_SIZE 2048

static void configure_serial_port(int fd);
static void *ble_receive_main(void *arg);

static void (*rx_callback)(char *, int);
static pthread_t th;
static int running;
static int fd = -1;
static char buf[BUF_SIZE];

static void *ble_receive_main(void *arg) {
    int n;

    (void)arg;

    while (running) {
        n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            if (rx_callback) {
                rx_callback(buf, n);
            }
        }
    }

    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
    return NULL;
}

void im920_init(void (*received)(char *, int)) {
    int ret;

    rx_callback = received;

    fd = open(DEVICE, O_RDWR | O_NOCTTY | O_NDELAY);
    ERRRET(fd == -1, "open_port: Unable to open " DEVICE);

    configure_serial_port(fd);

    running = 1;
    ret = pthread_create(&th, NULL, ble_receive_main, NULL);
    ERRRET(ret != 0, "pthread_create error");

error_return:
    return;
}

void im920_write(char *s, int n) {
    if (fd >= 0 && running)
        write(fd, s, n);
}

static void configure_serial_port(int fd) {
    struct termios options;

    tcgetattr(fd, &options);

    // Set baud rate
    cfsetispeed(&options, BAUDRATE);
    cfsetospeed(&options, BAUDRATE);

    // Set 8N1 (8 data bits, No parity, 1 stop bit)
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;

    // Disable hardware flow control
    options.c_cflag &= ~CRTSCTS;

    // Enable receiver and set local mode
    options.c_cflag |= (CLOCAL | CREAD);

    // Set raw input
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    // Set raw output
    options.c_oflag &= ~OPOST;

    // Set read timeout
    options.c_cc[VMIN] = 1;
    options.c_cc[VTIME] = 0;

    tcsetattr(fd, TCSANOW, &options);
}