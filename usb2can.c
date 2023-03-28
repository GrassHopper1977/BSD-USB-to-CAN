#include <stdio.h>
#include <string.h>
#include <cheriintrin.h>
#ifdef __CHERI_PURE_CAPABILITY__
#include <cheri/cheri.h>
#endif
#include "libusb.h"
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <sys/socket.h> // Sockets
#include <netinet/in.h>
#include <sys/un.h>     // ?
#include <sys/event.h>  // Events
#include <assert.h>     // The assert function
#include <unistd.h>     // ?
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netdb.h>
#include <signal.h>
#include <sys/time.h>

//#define DEBUG_INFO_CAN_IN
#define DEBUG_INFO_CAN_OUT
#define DEBUG_INFO_SOCK_IN
//#define DEBUG_INFO_SOCK_OUT
#define DEBUG_INFO_SETUP


#define USB_VENDOR_ID   0x1D50
#define USB_PRODUCT_ID  0x606F

#define ENDPOINT_FLAG_IN        0x80
#define ENDPOINT_IN     (0x01 | ENDPOINT_FLAG_IN)
#define ENDPOINT_OUT    0x02

#ifdef __CHERI_PURE_CAPABILITY__
#define PRINTF_PTR "#p"
#else
#define PRINTF_PTR "p"
#endif

#define MAX_EVENTS      (32)

#define TIMER_FD (1234)


#define BITRATE_DATA_LEN	(20)
int8_t bitrate = 12;
unsigned char bitrates[16][BITRATE_DATA_LEN] = {
  {	// 20k
    0x06, 0x00, 0x00, 0x00, // Prop seg
    0x07, 0x00, 0x00, 0x00, // Phase Seg 1
    0x02, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x96, 0x00, 0x00, 0x00  // BRP
  },
  {	// 33.33k
    0x03, 0x00, 0x00, 0x00, // Prop seg
    0x03, 0x00, 0x00, 0x00, // Phase Seg 1
    0x01, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0xB4, 0x00, 0x00, 0x00  // BRP
  },
  {	// 40k
    0x06, 0x00, 0x00, 0x00, // Prop seg
    0x07, 0x00, 0x00, 0x00, // Phase Seg 1
    0x02, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x4B, 0x00, 0x00, 0x00  // BRP
  },
  {	// 50k
    0x06, 0x00, 0x00, 0x00, // Prop seg
    0x07, 0x00, 0x00, 0x00, // Phase Seg 1
    0x02, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x3C, 0x00, 0x00, 0x00  // BRP
  },
  {	// 66.66k
    0x03, 0x00, 0x00, 0x00, // Prop seg
    0x03, 0x00, 0x00, 0x00, // Phase Seg 1
    0x01, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x5A, 0x00, 0x00, 0x00  // BRP
  },
  {	// 80k
    0x03, 0x00, 0x00, 0x00, // Prop seg
    0x03, 0x00, 0x00, 0x00, // Phase Seg 1
    0x01, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x4B, 0x00, 0x00, 0x00  // BRP
  },
  {	// 83.33k
    0x03, 0x00, 0x00, 0x00, // Prop seg
    0x03, 0x00, 0x00, 0x00, // Phase Seg 1
    0x01, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x48, 0x00, 0x00, 0x00  // BRP
  },
  {	// 100k
    0x06, 0x00, 0x00, 0x00, // Prop seg
    0x07, 0x00, 0x00, 0x00, // Phase Seg 1
    0x02, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x1E, 0x00, 0x00, 0x00  // BRP
  },
  {	// 125k
    0x06, 0x00, 0x00, 0x00, // Prop seg
    0x07, 0x00, 0x00, 0x00, // Phase Seg 1
    0x02, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x18, 0x00, 0x00, 0x00  // BRP
  },
  {	// 200k
    0x06, 0x00, 0x00, 0x00, // Prop seg
    0x07, 0x00, 0x00, 0x00, // Phase Seg 1
    0x02, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x0f, 0x00, 0x00, 0x00  // BRP
  },
  {	// 250k
    0x06, 0x00, 0x00, 0x00, // Prop seg
    0x07, 0x00, 0x00, 0x00, // Phase Seg 1
    0x01, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x0C, 0x00, 0x00, 0x00  // BRP
  },
  {	// 400k
    0x03, 0x00, 0x00, 0x00, // Prop seg
    0x03, 0x00, 0x00, 0x00, // Phase Seg 1
    0x01, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x0F, 0x00, 0x00, 0x00  // BRP
  },
  {	// 500k (default)
    0x06, 0x00, 0x00, 0x00, // Prop seg
    0x07, 0x00, 0x00, 0x00, // Phase Seg 1
    0x02, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x06, 0x00, 0x00, 0x00  // BRP
  },
  {	// 666k
    0x03, 0x00, 0x00, 0x00, // Prop seg
    0x03, 0x00, 0x00, 0x00, // Phase Seg 1
    0x02, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x08, 0x00, 0x00, 0x00  // BRP
  },
  {	// 800k
    0x07, 0x00, 0x00, 0x00, // Prop seg
    0x08, 0x00, 0x00, 0x00, // Phase Seg 1
    0x04, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x03, 0x00, 0x00, 0x00  // BRP
  },
  {	// 1m
    0x05, 0x00, 0x00, 0x00, // Prop seg
    0x06, 0x00, 0x00, 0x00, // Phase Seg 1
    0x04, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x03, 0x00, 0x00, 0x00  // BRP
  }
};



int sendStringToAll(char * txt);

void sigint_handler(int sig) {
  printf("\nSignal received (%i).\n", sig);
  fflush(stdout);
  fflush(stderr);
  if(sig == SIGINT) {
    // Make sure the signal is passed down the line correctly.
    signal(SIGINT, SIG_DFL);
    kill(getpid(), SIGINT);
  }
}

void print_time_now() {
  struct timeval now;
  gettimeofday(&now, NULL);
  printf("%ld.%06ld secs", now.tv_sec, now.tv_usec);
}

#ifdef __CHERI_PURE_CAPABILITY__
void printf_caps_info(char * d, void *c) {
  printf("%s = %#p\n", d, c);
  printf("%s address = %lu (0x%lx)\n", d, cheri_address_get(c), cheri_address_get(c));
  printf("%s base address = %lu (0x%lx)\n", d, cheri_base_get(c), cheri_base_get(c));
  printf("%s bounds length = %lu (0x%lx)\n", d, cheri_length_get(c), cheri_length_get(c));
  printf("%s offset = %lu (0x%lx)\n", d, cheri_offset_get(c), cheri_offset_get(c));
  size_t perms = cheri_perms_get(c);
  printf("%s perms: ", d);
  if(perms & CHERI_PERM_EXECUTE) printf("CHERI_PERM_EXECUTE ");
  if(perms & CHERI_PERM_LOAD) printf("CHERI_PERM_LOAD ");
  if(perms & CHERI_PERM_LOAD_CAP) printf("CHERI_PERM_LOAD_CAP ");
  if(perms & CHERI_PERM_STORE) printf("CHERI_PERM_STORE ");
  if(perms & CHERI_PERM_STORE_CAP) printf("CHERI_PERM_STORE_CAP ");
  if(perms & CHERI_PERM_SW_VMEM) printf("CHERI_PERM_SW_VMEM ");
  printf("\n");
}
#endif

int read_packet(struct libusb_device_handle *devh) {
  uint8_t data[24];
  int len = 0;
  int ret = libusb_bulk_transfer(devh, ENDPOINT_IN, data, sizeof(data), &len, 1);
  if(ret == 0) {
#ifdef DEBUG_INFO_CAN_IN
    printf("CAN ");
    print_time_now();
    printf(" IN (%2d):", len);
    for(int i = 0; i < len; i++) {
      printf(" %02x", data[i]);
    }
    printf("\n");
    fflush(stdout);
#endif

    uint32_t identifier = (data[7] << 24) + (data[6] << 16) + (data[5] << 8) + data[4];
    uint8_t data_length_code = data[8];

    char* txt = calloc(1, 8 + 1 + 2 + 1 + (data_length_code * 2) + 3);
    int ltxt = sprintf(txt, "%08x %02x ", identifier, data_length_code);

    for(int i = 0; i < data_length_code; i++) {
      ltxt += sprintf(&(txt[ltxt]), "%02x", data[12 + i]);
    }
    ltxt += sprintf(&(txt[ltxt]), "\r\n");

#ifdef DEBUG_INFO_SOCK_OUT
    printf("CAN ");
    print_time_now();
    printf(" Event Raised: %s\n", txt);
    fflush(stdout);
#endif

    sendStringToAll(txt);
    free(txt);
  }

  return ret;
}

int send_packet(struct libusb_device_handle *devh, uint32_t id, uint8_t dlc, uint8_t* dat) {
  unsigned char data[] = {
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, // Message ID (LSB to MSB)
    0x00,   // DLC
    0x00,   // Channel
    0x00,   // Flags
    0x00,   // Reserved
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // Data
  };
  data[4] = id & 0x0ff;
  data[5] = (id >> 8) & 0x0ff;
  data[6] = (id >> 16) & 0x0ff;
  data[7] = (id >> 24) & 0x0ff;
  if(id > 0x03f) {
    data[7] |= 0x80; // Set the extended bit flag (if not already set)
  }
  data[8] = dlc;
  for(int i = 0; i < dlc; i++) {
    data[12 + i] = dat[i];
  }
  int len = 0;
  int ret = libusb_bulk_transfer(devh, ENDPOINT_OUT, data, sizeof(data), &len, 1);
  if(ret == 0) {
#ifdef DEBUG_INFO_CAN_OUT
    printf("OUT (%2d):", len);
    for(int i = 0; i < len; i++) {
      printf(" %02x", data[i]);
    }
    printf("\n");
#endif
  } else {
    fprintf(stderr, "%s: %s\n", libusb_error_name(ret), libusb_strerror(ret));
  }
  return ret;
}

// These are the speed settings. We'd like to offer custom bitrates and sample points too.
int set_bitrate(struct libusb_device_handle *devh) {
#ifdef DEBUG_INFO_SETUP
  printf("Setting bitrate...\n");
#endif
  uint8_t bmReqType = 0x41;       // the request type (direction of transfer)
  uint8_t bReq = 0x01;            // the request field for this packet
  uint16_t wVal = 0x0000;         // the value field for this packet
  uint16_t wIndex = 0x0000;       // the index field for this packet
  uint16_t wLen = BITRATE_DATA_LEN;   // length of this setup packet 
  unsigned int to = 0;    // timeout duration (if transfer fails)

  // transfer the setup packet to the USB device
  int config = libusb_control_transfer(devh,bmReqType,bReq,wVal,wIndex, bitrates[bitrate], wLen,to);

  return config;
}

// Not quite sure what this is yet
int port_setup0(struct libusb_device_handle *devh) {
#ifdef DEBUG_INFO_SETUP
  printf("Setting up the port 0\n");
#endif
  uint8_t bmReqType = 0x00;       // the request type (direction of transfer)
  uint8_t bReq = 0x09;            // the request field for this packet
  uint16_t wVal = 0x0001;         // the value field for this packet
  uint16_t wIndex = 0x0000;       // the index field for this packet
  uint16_t wLen = 0;      // length of this setup packet 
  unsigned char data[] = {0x00};
  unsigned int to = 0;    // timeout duration (if transfer fails)

  // transfer the setup packet to the USB device
  int config = libusb_control_transfer(devh, bmReqType, bReq, wVal, wIndex, data, wLen, to);

  return config;
}

// Not sure what this message means yet.
int port_setup1(struct libusb_device_handle *devh) {
#ifdef DEBUG_INFO_SETUP
  printf("Setting up the port 1\n");
#endif
  uint8_t bmReqType = 0x41;       // the request type (direction of transfer)
  uint8_t bReq = 0x00;            // the request field for this packet
  uint16_t wVal = 0x0001;         // the value field for this packet
  uint16_t wIndex = 0x0000;       // the index field for this packet
  // the data buffer for the in/output data
  unsigned char data[] = {
    0xef, 0xbe, 0x00, 0x00
  };
  uint16_t wLen = sizeof(data);   // length of this setup packet 
  unsigned int to = 0;    // timeout duration (if transfer fails)

  // transfer the setup packet to the USB device
  int config = libusb_control_transfer(devh, bmReqType, bReq, wVal, wIndex, data, wLen, to);

  return config;
}

// Not yet sure what this one does.
int port_setup2(struct libusb_device_handle *devh) {
#ifdef DEBUG_INFO_SETUP
  printf("Setting up the port 2\n");
#endif
  uint8_t bmReqType = 0xC1;       // the request type (direction of transfer)
  uint8_t bReq = 0x05;            // the request field for this packet
  uint16_t wVal = 0x0001;         // the value field for this packet
  uint16_t wIndex = 0x0000;       // the index field for this packet
  // the data buffer for the in/output data
  unsigned char data[] = {
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
  };
  uint16_t wLen = sizeof(data);   // length of this setup packet 
  unsigned int to = 0;    // timeout duration (if transfer fails)

  // transfer the setup packet to the USB device
  int ret = libusb_control_transfer(devh, bmReqType, bReq, wVal, wIndex, data, wLen, to);
#ifdef DEBUG_INFO_SETUP
  if(ret >=0) {
    printf("Data:");
    for(int i = 0; i < wLen; i++) {
      printf(" %02X", data[i]);
    }
    printf("\n");
  }
#endif

  return ret;
}

// Not yet sure what this one does.
int port_setup3(struct libusb_device_handle *devh) {
#ifdef DEBUG_INFO_SETUP
  printf("Setting up the port 3\n");
#endif
  uint8_t bmReqType = 0xC1;       // the request type (direction of transfer)
  uint8_t bReq = 0x04;            // the request field for this packet
  uint16_t wVal = 0x0000;         // the value field for this packet
  uint16_t wIndex = 0x0000;       // the index field for this packet
  // the data buffer for the in/output data
  unsigned char data[] = {
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00
  };
  uint16_t wLen = sizeof(data);   // length of this setup packet 
  unsigned int to = 0;    // timeout duration (if transfer fails)

  // transfer the setup packet to the USB device
  int ret = libusb_control_transfer(devh, bmReqType, bReq, wVal, wIndex, data, wLen, to);
#ifdef DEBUG_INFO_SETUP
  if(ret >=0) {
    printf("Data:");
    for(int i = 0; i < wLen; i++) {
      printf(" %02X", data[i]);
    }
    printf("\n");
  }
#endif

  return ret;
}

int port_open(struct libusb_device_handle *devh) {
#ifdef DEBUG_INFO_SETUP
  printf("Opening the port\n");
#endif
  uint8_t bmReqType = 0x41;       // the request type (direction of transfer)
  uint8_t bReq = 0x02;            // the request field for this packet
  uint16_t wVal = 0x0000;         // the value field for this packet
  uint16_t wIndex = 0x0000;       // the index field for this packet
  
  // the data buffer for the in/output data
  unsigned char data[] = {
    0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00 
  };
  uint16_t wLen = sizeof(data);   // length of this setup packet 
  unsigned int to = 0;    // timeout duration (if transfer fails)

  // transfer the setup packet to the USB device
  int config = libusb_control_transfer(devh, bmReqType, bReq, wVal, wIndex, data, wLen, to);

  return config;
}

int port_close(struct libusb_device_handle *devh) {
#ifdef DEBUG_INFO_SETUP
  printf("Closing the port\n");
#endif
  uint8_t bmReqType = 0x41;       // the request type (direction of transfer)
  uint8_t bReq = 0x02;            // the request field for this packet
  uint16_t wVal = 0x0000;         // the value field for this packet
  uint16_t wIndex = 0x0000;       // the index field for this packet
  // the data buffer for the in/output data
  unsigned char data[] = {
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
  };
  uint16_t wLen = sizeof(data);   // length of this setup packet 
  unsigned int to = 0;    // timeout duration (if transfer fails)

  // transfer the setup packet to the USB device
  int config = libusb_control_transfer(devh, bmReqType, bReq, wVal, wIndex, data, wLen, to);

  return config;
}

#define NCLIENTS (10)

#define CLIENT_TYPE_NONE	(0)
#define CLIENT_TYPE_SOCK	(1)
#define CLIENT_TYPE_LIBUSB	(2)

struct client_t {
  int fd;
  int typ;
};

struct client_t clients[NCLIENTS];

// return the index of a particular client's fd, or empty slot if fd = 0;
int conn_index(int fd) {
  int i;
  for(i = 0; i < NCLIENTS; i++) {
    if(clients[i].fd == fd) {
      return i;
    }
  }
  return -1;
}

// Add a new connection to the clients list
int conn_add(int fd, int typ) {
  if(fd < 1) return -1;
  int i = conn_index(0);
  if(i == NCLIENTS) {  // We don't have space for this one!
    close(fd);
    return -1;
  }
  clients[i].fd = fd;
  clients[i].typ = typ;
  return 0;
}

// Close connection and remove from clients list.
int conn_close(int fd) {
  if(fd < 1) return -1;
  int i = conn_index(fd);
  if(i < 0) return -1;
  clients[i].fd = 0;
  clients[i].typ = 0;
  return close(fd);
}

int sockSend(int fd, const void *msg, size_t len) {
  int res = send(fd, msg, len, 0);
  if(res == -1) {
    fprintf(stderr, "Send error:  ");
    switch(errno) {
      case EBADF:
              fprintf(stderr, "EBADF");
              break;
      case EACCES:
              fprintf(stderr, "EACCES");
              break;
      case ENOTCONN:
              fprintf(stderr, "ENOTCONN");
              break;
      case ENOTSOCK:
              fprintf(stderr, "ENOTSOCK");
              break;
      case EFAULT:
              fprintf(stderr, "EFAULT");
              break;
      case EMSGSIZE:
              fprintf(stderr, "EMSGSIZE");
              break;
      case EAGAIN:
              fprintf(stderr, "EAGAIN");
              break;
      case ENOBUFS:
              fprintf(stderr, "ENOBUFS");
              break;
      case EHOSTUNREACH:
              fprintf(stderr, "EHOSTUNREACH");
              break;
      case EISCONN:
              fprintf(stderr, "EISCONN");
              break;
      case ECONNREFUSED:
              fprintf(stderr, "ECONNREFUSED");
              break;
      case EHOSTDOWN:
              fprintf(stderr, "EHOSTDOWN");
              break;
       case ENETDOWN:
              fprintf(stderr, "ENETDOWN");
              break;
      case EADDRNOTAVAIL:
              fprintf(stderr, "EADDRNOTAVAIL");
              break;
      case EPIPE:
              fprintf(stderr, "EPIPE");
              break;
      default :
              fprintf(stderr, "unknown");
              break;
    }
    fprintf(stderr, "\n");
  }
  return res;
}

int sendString(int fd, char* text) {
  int res = 0;
  res = sockSend(fd, text, strlen(text));
  if(res < 0) {
    fprintf(stderr, "Error sending reading. res = %i\n", res);
  }
  return res;
}

int sendStringToAll(char * txt) {
  int i;
  int cnt = 0;
  for(i = 0; i < NCLIENTS; i++) {
    if((clients[i].fd > 0) && (clients[i].typ == CLIENT_TYPE_SOCK)) {
      int ret = sendString(clients[i].fd, txt);
      if(ret > 0) {
        cnt++;
      }
    }
  }
  return cnt; // How many we succesfully sent to.
}

int processing_loop(int kq, int sockFd, struct libusb_device_handle *devh, libusb_context *ctx)
{
  struct kevent evSet;
  struct kevent evList[MAX_EVENTS];
  struct sockaddr_storage addr;
  socklen_t socklen = sizeof(addr);
  int fd;
  struct timespec zero_ts = {
    .tv_sec = 0,
    .tv_nsec = 0
  };

  print_time_now();
#ifdef DEBUG_INFO_SETUP
  printf(" Entering Main Loop...\n");
  printf("sockFd = %i\n", sockFd);
#endif
  while(1) {
    read_packet(devh);  // Our first read to kick it all off.
    int nev = kevent(kq, NULL, 0, evList, MAX_EVENTS, &zero_ts);
    if(nev < 0) {
      perror("ERROR: kevent error");
      exit(1);
    }

    for(int i = 0; i < nev; i++)
    {
      if(sockFd == (int)(evList[i].ident)) {
#ifdef DEBUG_INFO_SOCK_IN
        printf("Socket event!\n");
#endif
        fd = accept(evList[i].ident, (struct sockaddr *)&addr, & socklen);
        if(fd == -1) {
          fprintf(stderr, "Unable to accept socket.\n");
        } else if(conn_add(fd, CLIENT_TYPE_SOCK) == 0) {
#ifdef DEBUG_INFO_SOCK_IN
          printf("Accepted socket %i.\n", fd);
#endif
          EV_SET(&evSet, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
          assert(-1 != kevent(kq, &evSet, 1, NULL, 0, NULL));

          sendString(fd, "Welcome!\n");
        } else {
          fprintf(stderr, "Connection refused!");
          close(fd);
        }
      } else if(conn_index((int)(evList[i].ident)) > -1) {
        fd = (int)(evList[i].ident);
        switch(clients[conn_index(fd)].typ) {
        case CLIENT_TYPE_SOCK:
#ifdef DEBUG_INFO_SOCK_IN
          printf("Client socket event! fd = %i, index = %i\n", fd, conn_index(fd));
#endif
          if(evList[i].flags & EV_EOF) {
#ifdef DEBUG_INFO_SOCK_IN
            printf("Socket closed: %i\n", fd);
#endif
            conn_close(fd);
          } else {
#ifdef DEBUG_INFO_SOCK_IN
            printf("Socket data! Bytes waiting: %li\n", evList[i].data);
            printf("evList[%i].flags = 0x%04x, .fflags = 0x%04x\n", i, evList[i].flags, evList[i].fflags);
#endif
            char* dataIn = calloc(1, evList[i].data + 1);
            dataIn[evList[i].data] = 0;
            int ret = read(fd, dataIn, evList[i].data);
#ifdef DEBUG_INFO_SOCK_IN
            printf("%.*s", ret, dataIn);
#endif
            if(ret > 12) {
              uint32_t id = strtoul(dataIn, NULL, 16);
              printf("Identifier: %08x\n", id);
              uint8_t dlc = (uint8_t)strtoul(&(dataIn[9]), NULL, 16);
              printf("       dlc: %02x\n", dlc);
              uint32_t max_len = 12 + (dlc * 2);
              printf("   max_len: %u\n", max_len);
              printf("       ret: %u\n", ret);
              uint8_t data[8];
              char conv[3];
              if(ret >= max_len) {
                uint32_t rd = 12;
                for(int i=0; i < dlc; i++) {
                  conv[0] = dataIn[rd++];
                  conv[1] = dataIn[rd++];
                  conv[2] = '\0';
                  data[i] = (uint8_t)strtoul(conv, NULL, 16);
                  printf("   data[%u]: %02x\n", i, data[i]);
                } 
                send_packet(devh, id, dlc, data);            
              }
            }
            free(dataIn);
          }
          break;
        }
      }
    }
  }

  return 0;
}

void printusage()
{
  printf("usb2can opens a the frist USB2CAN device that it finds and binds to a socket (default address is 2303).\n");
  printf("All messages received will be presented as Hex in the format:\n");
  printf("  IIIIIIII LL DDDDDDDDDDDDDDDD[CR][NL]\n");
  printf("\n");
  printf("Where: \n");
  printf("  I = Message ID: 8 chars representing a 32-bit unsigned with leading zeros.\n");
  printf("  L = Data Length code: 2 chars representing an 8-bit unsigned with leading zeros. Valid range 0 to 8.\n");
  printf("  D = Data: 0 to 16 chars (depending on Data Length Code) representing a octets with leading zeros.\n");
  printf("\n");
  printf("Use the same format for transmission.\n");
  printf("\n");
  printf("Usage: usb2can <?/s[rate]/p[nnnn]>\n");
  printf("\n");
  printf("Where: \n");
  printf("  s[rate] = Chosen bitrate. Defaults to 500k.\n");
  printf("            Select from: 20k, 33.33k, 40k, 50k, 66.66k\n");
  printf("                         80k, 83.33k, 100k, 125k, 200k\n");
  printf("                         250k, 400k, 500k, 666k, 800k\n");
  printf("                         1m\n");
  printf("  ? = print this message. \n");
  printf("  p[nnnn] = a \"p\" followed by a number - use this as the port number. \n");
  printf("  [CR][NL] = Carridge Return New Line. \n");
  printf("\n");
}

int port = 2303;

void processArgs(int argc, char *argv[])
{
  if(argc > 1) {
    for(int i = 1; i < argc; i++)
    {
      if(argv[i][0]  == '?') {
        printusage();
        exit(0);
      } else if(argv[i][0] == 'p') {
        int port2 = atoi(&(argv[i][1]));
        if(port2 == 0) {
          fprintf(stderr, "Incorrect arguments!\n\n");
          printusage();
          exit(1);
        }
        port = port2;
      } else if(argv[i][0] == 's') {
        if(0 == strncmp(argv[i], "s20k", 4)) {
          bitrate = 0;
        } else if(0 == strncmp(argv[i], "s33.33k", 7)) {
          bitrate = 1;
        } else if(0 == strncmp(argv[i], "s40k", 4))  {
          bitrate = 2;
        } else if(0 == strncmp(argv[i], "s50k", 4))  {
          bitrate = 3;
        } else if(0 == strncmp(argv[i], "s66.66k", 7))  {
          bitrate = 4;
        } else if(0 == strncmp(argv[i], "s80k", 4))  {
          bitrate = 5;
        } else if(0 == strncmp(argv[i], "s83.33k", 7))  {
          bitrate = 6;
        } else if(0 == strncmp(argv[i], "s100k", 5))  {
          bitrate = 7;
        } else if(0 == strncmp(argv[i], "s125k", 5))  {
          bitrate = 8;
        } else if(0 == strncmp(argv[i], "s200k", 5))  {
          bitrate = 9;
        } else if(0 == strncmp(argv[i], "s250k", 5))  {
          bitrate = 10;
        } else if(0 == strncmp(argv[i], "s400k", 5))  {
          bitrate = 11;
        } else if(0 == strncmp(argv[i], "s500k", 5))  {
          bitrate = 12;
        } else if(0 == strncmp(argv[i], "s666k", 5))  {
          bitrate = 13;
        } else if(0 == strncmp(argv[i], "s800k", 5))  {
          bitrate = 14;
        } else if(0 == strncmp(argv[i], "s1m", 3))  {
          bitrate = 15;
        } else {
          fprintf(stderr, "Incorrect bitrate!\n\n");
          printusage();
          exit(1);
        }
      }
    }
  }
}

// Main program entry point. 1st argument will be path to config.json, if it's not present then we'll use the default filename.
int main(int argc, char *argv[])
{
  int ret = 0;

  // Create the signal handler here - ensures that Ctrl-C gets passed back up to 
  signal(SIGINT, sigint_handler);

  processArgs(argc, argv);

  // Create and bind our socket here.
  printf("Creating our server here...\n");
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_len = sizeof(addr);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);

  int sock = socket(addr.sin_family, SOCK_STREAM, 0);
  assert(sock != -1);

  int on = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  if(bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("bind");
    return 1;
  }
  assert(listen(sock, 5) != -1);
  printf("Listening on %i\n", port);

  printf("Setting up libusb for CAN socket...\n");
  const struct libusb_version * ver = NULL;
  printf("LIBUSB_API_VERSION = %08x\n", LIBUSB_API_VERSION);

  ver = libusb_get_version();
  printf("LibUSB version = %i.%i.%i.%i %s %s\n", ver->major, ver->minor, ver->micro, ver->nano, ver->rc, ver->describe);

  printf("Trying libusb_init...\n");
  libusb_context *ctx;
  ret = libusb_init(&ctx);
  if (ret < 0) {
    printf("ERROR: failed to initialize libusb (ret = %d)\n", ret);
    exit(1);
  }

  libusb_set_debug(NULL, LIBUSB_LOG_LEVEL_DEBUG);
  //libusb_set_debug(NULL, LIBUSB_LOG_LEVEL_INFO);

  libusb_device **list;
  ssize_t cnt = libusb_get_device_list(NULL, &list);
  if (cnt < 0) printf("ERROR: failed to get device list (cnt = %ld)\n", cnt);
  printf("%ld USB devices found.\n\nUSB Devices found:\n", cnt);

  for (ssize_t i = 0; i < cnt; i++) {
    libusb_device *dev = list[i];
    struct libusb_device_descriptor desc;
    int r = libusb_get_device_descriptor(dev, &desc);
    if (r < 0) printf("ERROR: failed to get device descriptor (r = %d)\n", r);

    printf("%2ld Vendor ID: %i, Product ID: %i, Manufacturer: %i, Product: %i, Serial: %i\n", i+1, desc.idVendor, desc.idProduct, desc.iManufacturer, desc.iProduct, desc.iSerialNumber);
  }

  libusb_free_device_list(list, 1);

  struct libusb_device_handle *devh = NULL;
  printf("Attempting to open the USB2CAN device with %04x:%04x now...\n", USB_VENDOR_ID, USB_PRODUCT_ID);
  devh = libusb_open_device_with_vid_pid(NULL, USB_VENDOR_ID, USB_PRODUCT_ID);
  if(devh == NULL) {
    printf("ERROR: failed to open the device.\n");
    exit(1);
  }
  printf("Device opened.\n");

  int interface = 0;
  printf("Checking if kernel driver is active...\n");
  if(libusb_kernel_driver_active(devh, interface) == 1) {
    printf("Detaching kernel driver...\n");
    int ret = libusb_detach_kernel_driver(devh, interface);
    if(ret < 0) {
      fprintf(stderr, "%s: %s Unable to detach.\n", libusb_error_name(ret), libusb_strerror(ret));
    }
  }

  printf("Claiming interface...\n");
  ret = libusb_claim_interface(devh, interface);
  if(ret < 0) {
    fprintf(stderr, "%s: %s Unable to claim interface %d.\n",
    libusb_error_name(ret), libusb_strerror(ret), interface);
  }

  printf("Getting descriptor...\n");
  libusb_device* device = libusb_get_device(devh);
  struct libusb_config_descriptor* descriptor = NULL;
  if(libusb_get_active_config_descriptor(device, &descriptor) < 0)
  {
    fprintf(stderr, "failed to get config descriptor");
  }
  //check for correct endpoints
  printf("Endpoints found:");
  if(descriptor->bNumInterfaces > 0)
  {
    struct libusb_interface_descriptor iface = descriptor->interface[0].altsetting[0];
    for(int i=0; i<iface.bNumEndpoints; ++i) {
      printf(" %d (0x%02x),", iface.endpoint[i].bEndpointAddress, iface.endpoint[i].bEndpointAddress);
      //if(iface.endpoint[i].bEndpointAddress == ctrlBulkOutAddr || iface.endpoint[i].bEndpointAddress == ctrlBulkInAddr)
      //{
      //  bulkCtrlAvailable = true;
      //  break;
      //}
    }
  }

  printf("\n");
  libusb_free_config_descriptor(descriptor);

  printf("Setting up for comms...\n");
  ret = port_setup0(devh);
  if(ret < 0) {
    fprintf(stderr, "ERROR! Unable to setup0.\n");
    exit(1);
  }
  ret = port_setup1(devh);
  if(ret < 0) {
    fprintf(stderr, "ERROR! Unable to setup1.\n");
    exit(1);
  }
  ret = port_setup2(devh);
  if(ret < 0) {
    fprintf(stderr, "ERROR! Unable to setup2.\n");
    exit(1);
  }
  ret = port_setup3(devh);
  if(ret < 0) {
    fprintf(stderr, "ERROR! Unable to setup3.\n");
    exit(1);
  }

  ret = set_bitrate(devh);
  if(ret < 0) {
    fprintf(stderr, "ERROR! Unable to configure.\n");
    exit(1);
  }

  printf("Opening port...\n");
  ret = port_open(devh);
  if(ret < 0) {
    fprintf(stderr, "ERROR! Unable to open port.\n");
    exit(1);
  }
  printf("USB to CAN device is connected!\n");

  printf("Creating Event Queue...\n");
  int kq = kqueue();
  struct kevent evSet;

  //const struct libusb_pollfd ** polllist = libusb_get_pollfds(ctx);
  //if(polllist == NULL) {
  //  perror("libusb_get_pollfds");
  //  exit(1);
  //}

  //int i = 0;
  //while(polllist[i] != NULL) {
  //  printf("%i. Adding libusb fd = %i\n", i + 1, polllist[i]->fd);
  //  EV_SET(&evSet, polllist[i]->fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
  //  EV_SET(&evSet, polllist[i]->fd, EVFILT_WRITE, EV_ADD, 0, 0, NULL);
  //  i++;
  //}

  //EV_SET(&evSet, TIMER_FD, EVFILT_READ, EV_ADD, 0, 0, NULL);
  //assert(-1 != kevent(kq, &evSet, 1, NULL, 0, NULL));
  EV_SET(&evSet, sock, EVFILT_READ, EV_ADD, 0, 0, NULL);
  assert(-1 != kevent(kq, &evSet, 1, NULL, 0, NULL));

  printf("Starting main program loop...\n");
  processing_loop(kq, sock, devh, ctx);

  ret = port_close(devh);
  if(ret < 0) {
    fprintf(stderr, "ERROR! Unable to close port.\n");
  }

  ret = libusb_attach_kernel_driver(devh, interface);
  if(ret < 0) {
    fprintf(stderr, "%s: %s Unable to reattach existing driver.\n", libusb_error_name(ret), libusb_strerror(ret));
  }

  if(devh != NULL) {
    libusb_close(devh);
    printf("Device closed.\n");
  }

  printf("Trying libusb_exit...\n");
  libusb_exit(ctx);
  return ret;
}
