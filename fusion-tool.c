#include <sys/types.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "sbuf.h"


//static unsigned long g_packets = 0;

static void usage(const char *arg0, int retval)
{
  fprintf(retval ? stderr : stdout,
    "usage: %s [-d port]  [-b baudrate] [-o outputfile] [-i inputfile]\n",
    arg0);

  exit(retval);
}

struct speed_map
{
  const char *string;     /* ASCII representation. */
  speed_t speed;    /* Internal form. */
  unsigned long int value;  /* Numeric value. */
};

static struct speed_map const speeds[] =
{
  {"0", B0, 0},
  {"50", B50, 50},
  {"75", B75, 75},
  {"110", B110, 110},
  {"134", B134, 134},
  {"134.5", B134, 134},
  {"150", B150, 150},
  {"200", B200, 200},
  {"300", B300, 300},
  {"600", B600, 600},
  {"1200", B1200, 1200},
  {"1800", B1800, 1800},
  {"2400", B2400, 2400},
  {"4800", B4800, 4800},
  {"9600", B9600, 9600},
  {"19200", B19200, 19200},
  {"38400", B38400, 38400},
  {"exta", B19200, 19200},
  {"extb", B38400, 38400},
#ifdef B57600
  {"57600", B57600, 57600},
#endif
#ifdef B115200
  {"115200", B115200, 115200},
#endif
#ifdef B230400
  {"230400", B230400, 230400},
#endif
#ifdef B460800
  {"460800", B460800, 460800},
#endif
#ifdef B500000
  {"500000", B500000, 500000},
#endif
#ifdef B576000
  {"576000", B576000, 576000},
#endif
#ifdef B921600
  {"921600", B921600, 921600},
#endif
#ifdef B1000000
  {"1000000", B1000000, 1000000},
#endif
#ifdef B1152000
  {"1152000", B1152000, 1152000},
#endif
#ifdef B1500000
  {"1500000", B1500000, 1500000},
#endif
#ifdef B2000000
  {"2000000", B2000000, 2000000},
#endif
#ifdef B2500000
  {"2500000", B2500000, 2500000},
#endif
#ifdef B3000000
  {"3000000", B3000000, 3000000},
#endif
#ifdef B3500000
  {"3500000", B3500000, 3500000},
#endif
#ifdef B4000000
  {"4000000", B4000000, 4000000},
#endif
  {NULL, 0, 0}
};

#define STREQ(s1,s2) (strcmp(s1,s2) == 0)

speed_t string_to_baud (const char *arg)
{
  int i;

  for (i = 0; speeds[i].string != NULL; ++i)
  if (STREQ (arg, speeds[i].string))
    return speeds[i].speed;
  return (speed_t) -1;
}

enum speed_setting
{
  input_speed, output_speed, both_speeds
};

static void
set_speed (enum speed_setting type, speed_t baud, struct termios *mode)
{
  if (type == input_speed || type == both_speeds)
  cfsetispeed (mode, baud);
  if (type == output_speed || type == both_speeds)
  cfsetospeed (mode, baud);
}

static struct termios old_mode;

static int port_open(const char *device, int baud)
{
  int fd;
  struct termios mode;

  if ((fd = open(device, O_RDWR | O_NONBLOCK)) == -1) {
    perror("cannot open serial port!");
    return -1;
  }

  if (tcgetattr(fd, &old_mode) < 0) {
    perror("tcgetattr failed!");
    close(fd);
    return -1;
  }

  memcpy(&mode, &old_mode, sizeof(mode));

  /* Raw mode. */
  mode.c_iflag = 0;
  mode.c_oflag &= ~OPOST;
  mode.c_lflag &= ~(ISIG | ICANON
#ifdef XCASE
      | XCASE
#endif
      );
  mode.c_cc[VMIN] = 1;
  mode.c_cc[VTIME] = 0;

  set_speed(both_speeds, baud, &mode);

  if (tcsetattr(fd,TCSAFLUSH, &mode) != 0) {
    perror("tcsetattr failed!");
    close(fd);
    return -1;
  }

  return fd;
}

static void port_deinit(int fd)
{
  tcsetattr(fd, TCSANOW, &old_mode);
}

const uint8_t start_packet[] = {0x7e, 0, 0, 0x7e};

int main(int argc, char *argv[])
{
  int opt, ret, magic_found = -1;
  int outputfd, portfd, inputfd = -1;

  const char *device = "/dev/ttyUSB0";
  const char *output_file = "./sensor_data.bin";
  const char *input_file = NULL;

  speed_t baud = 230400;

#define BS_SIZE 512

  char buf1[BS_SIZE], buf2[BS_SIZE * 4];

  sbuf_t sbuf1, sbuf2;

  sbuf_init(&sbuf1,buf1, sizeof(buf1));
  sbuf_init(&sbuf2,buf2, sizeof(buf2));

  while ((opt = getopt(argc, argv, "hd:b:i:o:")) != -1) {
    switch (opt) {
      case 'd':
        device = optarg;
        break;
      case 'b':
        baud = string_to_baud(optarg);
        if (baud == -1) {
          usage(argv[0], -1);
        }
        break;
      case 'i':
        input_file = optarg;
        break;
      case 'o':
        output_file = optarg;
        break;
      case '?':
      case 'h':
        usage(argv[0], 0);
      default:
        usage(argv[0], -1);
    }
  }

  if ((portfd = port_open(device, baud)) < 0) {
    return -1;
  }

  if ((outputfd = open(output_file, O_WRONLY|O_CREAT|O_TRUNC|O_APPEND,
          0644)) == -1) {
    fprintf(stderr, "cannot open %s:%s\n", output_file, strerror(errno));
    port_deinit(portfd);
    exit(-1);
  }

  if (input_file && ((inputfd = open(input_file, O_RDONLY)) < 0)) {
    fprintf(stderr, "cannot open %s:%s\n", input_file, strerror(errno));
    port_deinit(portfd);
    close(outputfd);
    exit(-1);
  }

  while (1) {
    fd_set rfds, wfds;

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);

    FD_SET(portfd, &rfds);
    if (sbuf_count(&sbuf1) || (inputfd >= 0))
      FD_SET(portfd, &wfds);

    ret = select(portfd + 1, &rfds, &wfds, NULL, NULL);
    if (ret > 0) {
      if (FD_ISSET(portfd, &wfds)) {
        if (sbuf_count(&sbuf1) || (sbuf_read_in(&sbuf1,inputfd) > 0)) {
          sbuf_write_out(&sbuf1, portfd, -1);
        } else {
          close(inputfd);
          inputfd = -1;
        }
      }

      if (FD_ISSET(portfd, &rfds)) {
        while ((ret = sbuf_read_in(&sbuf2, portfd)) > 0) {
          if (magic_found >= 0) {
            sbuf_write_out(&sbuf2, outputfd, -1);
          } else {
            if (sbuf_count(&sbuf2) >= sizeof start_packet) {
              if ((magic_found = sbuf_find(&sbuf2, start_packet,
                      sizeof start_packet)) >= 0) {
                sbuf_seek(&sbuf2, magic_found);
              } else {
                sbuf_discard(&sbuf2, -((int)sizeof(start_packet) - 1));
              }
            }
            continue;
          }
        }

        if (ret < 0) {
          fprintf(stderr, "read port failed: %s\n", strerror(errno));
          break;
        }
      }
    }
  }

  fprintf(stdout, "\n");

  port_deinit(portfd);
  close(outputfd);
  if (inputfd >= 0) {
    close(inputfd);
  }

  return 0;
}
