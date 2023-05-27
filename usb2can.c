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
#include <sys/endian.h>
#include <netdb.h>
#include <signal.h>
#include <sys/time.h>
#include "usb2can.h"
#include <stdarg.h>
#include <inttypes.h>

// #define DEBUG_INFO_CAN_IN
#define DEBUG_INFO_CAN_OUT
#define DEBUG_INFO_SOCK_IN
// #define DEBUG_INFO_SOCK_OUT
#define DEBUG_INFO_SETUP


#define USB_VENDOR_ID_GS_USB_1            0x1D50
#define USB_PRODUCT_ID_GS_USB_1           0x606F
#define USB_VENDOR_ID_CANDLELIGHT         0x1209
#define USB_PRODUCT_ID_CANDLELIGHT        0x2323
#define USB_VENDOR_ID_CES_CANEXT_FD       0x1cd2
#define USB_PRODUCT_ID_CES_CANEXT_FD      0x606f
#define USB_VENDOR_ID_ABE_CANDEBUGGER_FD  0x16d0
#define USB_PRODUCT_ID_ABE_CANDEBUGGER_FD 0x10b8

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

// Declarations
struct usb2can_can;
struct usb2can_tx_context;

// struct usb2can_device_state {
//   __le32 state;
//   __le32 rxerr;
//   __le32 txerr;
// } __packed;

// Device Specific Constants
enum usb2can_breq {
  USB2CAN_BREQ_HOST_FORMAT = 0,
  USB2CAN_BREQ_BITTIMING,
  USB2CAN_BREQ_MODE,
  USB2CAN_BREQ_BERR,
  USB2CAN_BREQ_BT_CONST,
  USB2CAN_BREQ_DEVICE_CONFIG,
  USB2CAN_BREQ_TIMESTAMP,
  USB2CAN_BREQ_IDENTIFY,
  USB2CAN_BREQ_GET_USER_ID,
  USB2CAN_BREQ_SET_USER_ID,
  USB2CAN_BREQ_DATA_BITTIMING,
  USB2CAN_BREQ_BT_CONST_EXT,
  USB2CAN_BREQ_SET_TERMINATION,
  USB2CAN_BREQ_GET_TERMINATION,
  USB2CAN_BREQ_GET_STATE,
};

enum usb2can_mode {
  USB2CAN_MODE_RESET = 0, // Reset a channel, tunrs it off
  USB2CAN_MODE_START,   // Start a channel
};

// We only send a maximum of USB2CAN_MAX_TX_REQ per channel at any one time.
// We keep track of how many are in play at a time by setting the echo_id and
// looking for it when it comes back.
#define USB2CAN_MAX_TX_REQ  (10)
#define USB2CAN_MAX_RX_REQ  (30)

// There may be some 3 channel devices out there but not more.
#define USB2CAN_MAX_CHANNELS (3)

// Bit Timing Const Definitions
#define USB2CAN_FEATURE_LISTEN_ONLY (1 << 0)
#define USB2CAN_FEATURE_LOOP_BACK (1 << 1)
#define USB2CAN_FEATURE_TRIPLE_SAMPLE (1 << 2)
#define USB2CAN_FEATURE_ONE_SHOT (1 << 3)
#define USB2CAN_FEATURE_HW_TIMESTAMP (1 << 4)
#define USB2CAN_FEATURE_IDENTIFY (1 << 5)
#define USB2CAN_FEATURE_USER_ID (1 << 6)
#define USB2CAN_FEATURE_PAD_PKTS_TO_MAX_PKT_SIZE (1 << 7)
#define USB2CAN_FEATURE_FD (1 << 8)
#define USB2CAN_FEATURE_REQ_USB_QUIRK_LPC546XX (1 << 9)
#define USB2CAN_FEATURE_BT_CONST_EXT (1 << 10)
#define USB2CAN_FEATURE_TERMINATION (1 << 11)
#define USB2CAN_FEATURE_BERR_REPORTING (1 << 12)
#define USB2CAN_FEATURE_GET_STATE (1 << 13)

struct usb2can_device_bt_const {
  uint32_t feature;
  uint32_t fclk_can;
  uint32_t tseg1_min;
  uint32_t tseg1_max;
  uint32_t tseg2_min;
  uint32_t tseg2_max;
  uint32_t sjw_max;
  uint32_t brp_min;
  uint32_t brp_max;
  uint32_t brp_inc;
} __packed;

// USB device information
struct usb2can_device_config {
  uint8_t reserved1;
  uint8_t reserved2;
  uint8_t reserved3;
  uint8_t icount;       // The number of interfaces available on this device (can be up to 3)
  uint32_t sw_version;
  uint32_t hw_version;
} __packed;


struct usb2can_tx_context {
  struct usb2can_can* can;
  uint32_t echo_id;
  uint64_t timestamp;
  struct can_frame* frame;
};

struct usb2can_can {
  struct libusb_device_handle* devh;
  struct usb2can_device_bt_const bt_const;
  struct usb2can_device_config device_config;
  struct usb2can_tx_context tx_context[USB2CAN_MAX_TX_REQ];
};

struct host_frame {
  uint32_t echo_id; // So that we can recognise messages that we have sent.
  uint32_t can_id;
  uint8_t can_dlc;
  uint8_t channel;
  uint8_t flags;
  uint8_t reserved;
  uint8_t data[8];
  // uint32_t timestamp;
} __packed;


#define HOST_FRAME_FLAG_OVERFLOW  (0x01)
#define HOST_FRAME_FLAG_FD        (0x02)
#define HOST_FRAME_FLAG_BRS       (0x04)
#define HOST_FRAME_FLAG_ESI       (0x08)


#define BITRATE_DATA_LEN  (20)
int8_t bitrate = 12;
unsigned char bitrates[16][BITRATE_DATA_LEN] = {
  { // 20k
    0x06, 0x00, 0x00, 0x00, // Prop seg
    0x07, 0x00, 0x00, 0x00, // Phase Seg 1
    0x02, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x96, 0x00, 0x00, 0x00  // BRP
  },
  { // 33.33k
    0x03, 0x00, 0x00, 0x00, // Prop seg
    0x03, 0x00, 0x00, 0x00, // Phase Seg 1
    0x01, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0xB4, 0x00, 0x00, 0x00  // BRP
  },
  { // 40k
    0x06, 0x00, 0x00, 0x00, // Prop seg
    0x07, 0x00, 0x00, 0x00, // Phase Seg 1
    0x02, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x4B, 0x00, 0x00, 0x00  // BRP
  },
  { // 50k
    0x06, 0x00, 0x00, 0x00, // Prop seg
    0x07, 0x00, 0x00, 0x00, // Phase Seg 1
    0x02, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x3C, 0x00, 0x00, 0x00  // BRP
  },
  { // 66.66k
    0x03, 0x00, 0x00, 0x00, // Prop seg
    0x03, 0x00, 0x00, 0x00, // Phase Seg 1
    0x01, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x5A, 0x00, 0x00, 0x00  // BRP
  },
  { // 80k
    0x03, 0x00, 0x00, 0x00, // Prop seg
    0x03, 0x00, 0x00, 0x00, // Phase Seg 1
    0x01, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x4B, 0x00, 0x00, 0x00  // BRP
  },
  { // 83.33k
    0x03, 0x00, 0x00, 0x00, // Prop seg
    0x03, 0x00, 0x00, 0x00, // Phase Seg 1
    0x01, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x48, 0x00, 0x00, 0x00  // BRP
  },
  { // 100k
    0x06, 0x00, 0x00, 0x00, // Prop seg
    0x07, 0x00, 0x00, 0x00, // Phase Seg 1
    0x02, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x1E, 0x00, 0x00, 0x00  // BRP
  },
  { // 125k
    0x06, 0x00, 0x00, 0x00, // Prop seg
    0x07, 0x00, 0x00, 0x00, // Phase Seg 1
    0x02, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x18, 0x00, 0x00, 0x00  // BRP
  },
  { // 200k
    0x06, 0x00, 0x00, 0x00, // Prop seg
    0x07, 0x00, 0x00, 0x00, // Phase Seg 1
    0x02, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x0f, 0x00, 0x00, 0x00  // BRP
  },
  { // 250k
    0x06, 0x00, 0x00, 0x00, // Prop seg
    0x07, 0x00, 0x00, 0x00, // Phase Seg 1
    0x01, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x0C, 0x00, 0x00, 0x00  // BRP
  },
  { // 400k
    0x03, 0x00, 0x00, 0x00, // Prop seg
    0x03, 0x00, 0x00, 0x00, // Phase Seg 1
    0x01, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x0F, 0x00, 0x00, 0x00  // BRP
  },
  { // 500k (default)
    0x06, 0x00, 0x00, 0x00, // Prop seg
    0x07, 0x00, 0x00, 0x00, // Phase Seg 1
    0x02, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x06, 0x00, 0x00, 0x00  // BRP
  },
  { // 666k
    0x03, 0x00, 0x00, 0x00, // Prop seg
    0x03, 0x00, 0x00, 0x00, // Phase Seg 1
    0x02, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x08, 0x00, 0x00, 0x00  // BRP
  },
  { // 800k
    0x07, 0x00, 0x00, 0x00, // Prop seg
    0x08, 0x00, 0x00, 0x00, // Phase Seg 1
    0x04, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x03, 0x00, 0x00, 0x00  // BRP
  },
  { // 1m
    0x05, 0x00, 0x00, 0x00, // Prop seg
    0x06, 0x00, 0x00, 0x00, // Phase Seg 1
    0x04, 0x00, 0x00, 0x00, // Seg 2
    0x01, 0x00, 0x00, 0x00, // SJW
    0x03, 0x00, 0x00, 0x00  // BRP
  }
};


#define _POSIX_C_SOURCE 199309L
        
#include <time.h>

/// Convert seconds to milliseconds
#define SEC_TO_MS(sec) ((sec)*1000)
/// Convert seconds to microseconds
#define SEC_TO_US(sec) ((sec)*1000000)
/// Convert seconds to nanoseconds
#define SEC_TO_NS(sec) ((sec)*1000000000)

/// Convert nanoseconds to seconds
#define NS_TO_SEC(ns)   ((ns)/1000000000)
/// Convert nanoseconds to milliseconds
#define NS_TO_MS(ns)    ((ns)/1000000)
/// Convert nanoseconds to microseconds
#define NS_TO_US(ns)    ((ns)/1000)

/// Get a time stamp in milliseconds.
uint64_t millis() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_FAST, &ts);
    uint64_t ms = SEC_TO_MS((uint64_t)ts.tv_sec) + NS_TO_MS((uint64_t)ts.tv_nsec);
    return ms;
}

/// Get a time stamp in microseconds.
uint64_t micros() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_FAST, &ts);
    uint64_t us = SEC_TO_US((uint64_t)ts.tv_sec) + NS_TO_US((uint64_t)ts.tv_nsec);
    return us;
}

/// Get a time stamp in nanoseconds.
uint64_t nanos() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_FAST, &ts);
    uint64_t ns = SEC_TO_NS((uint64_t)ts.tv_sec) + (uint64_t)ts.tv_nsec;
    return ns;
}


// Function Declarations
int sendCANToAll(struct can_frame * frame);
int send_packet(struct usb2can_can* can, struct can_frame* frame);
int release_tx_context(struct usb2can_can* can, uint32_t tx_echo_id);
struct usb2can_tx_context* get_tx_context(struct usb2can_can* can, struct can_frame* frame);

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


#define LOG_MIN_FILE_LEN  9
#define LOG_MAX_FILE_LEN  LOG_MIN_FILE_LEN
#define LOG_MIN_SOURCE_LEN  8
#define LOG_MAX_SOURCE_LEN  LOG_MIN_SOURCE_LEN
#define LOG_MIN_TYPE_LEN  5
#define LOG_MAX_TYPE_LEN  LOG_MIN_TYPE_LEN
void LOGI(const char* source, const char* type, const char *format, ...) {
  struct timeval now;
  va_list args;
  va_start(args, format);

  gettimeofday(&now, NULL);
  // fprintf(stdout, "%-10ld.%06ld,   INFO: %*.*s, %*.*s, %*.*s, ", now.tv_sec, now.tv_usec, LOG_MIN_FILE_LEN, LOG_MAX_FILE_LEN, __FILE__, LOG_MIN_SOURCE_LEN, LOG_MAX_SOURCE_LEN, source, LOG_MIN_TYPE_LEN, LOG_MAX_TYPE_LEN, type);
  fprintf(stdout, "%16.16" PRIu64 ",  INFO: %*.*s, %*.*s, %*.*s, ", nanos(), LOG_MIN_FILE_LEN, LOG_MAX_FILE_LEN, __FILE__, LOG_MIN_SOURCE_LEN, LOG_MAX_SOURCE_LEN, source, LOG_MIN_TYPE_LEN, LOG_MAX_TYPE_LEN, type);
  vfprintf(stdout, format, args);
  // printf("\n");
  va_end(args);
}

void LOGE(const char* source, const char* type, const char *format, ...) {
  struct timeval now;
  va_list args;
  va_start(args, format);

  gettimeofday(&now, NULL);
  // fprintf(stderr, "%-10ld.%06ld,  ERROR: %*.*s, %*.*s, %*.*s, ", now.tv_sec, now.tv_usec, LOG_MIN_FILE_LEN, LOG_MAX_FILE_LEN, __FILE__, LOG_MIN_SOURCE_LEN, LOG_MAX_SOURCE_LEN, source, LOG_MIN_TYPE_LEN, LOG_MAX_TYPE_LEN, type);
  fprintf(stderr, "%16.16" PRIu64 ", ERROR: %*.*s, %*.*s, %*.*s, ", nanos(), LOG_MIN_FILE_LEN, LOG_MAX_FILE_LEN, __FILE__, LOG_MIN_SOURCE_LEN, LOG_MAX_SOURCE_LEN, source, LOG_MIN_TYPE_LEN, LOG_MAX_TYPE_LEN, type);
  vfprintf(stderr, format, args);
  // printf("\n");
  va_end(args);
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

void print_can_frame(const char* source, const char* type, struct can_frame *frame, uint8_t err, const char *format, ...) {
  FILE * fd = stdout;

  if(err || (frame->can_id & CAN_ERR_FLAG)) {
    LOGE(source, type, "ID: ");
    fd = stderr;
  } else {
    LOGI(source, type, "ID: ");
  }

  if((frame->can_id & CAN_EFF_FLAG) || (frame->can_id & CAN_ERR_FLAG)) {
    fprintf(fd, "%08x", frame->can_id & CAN_EFF_MASK);
  } else {
    fprintf(fd, "     %03x", frame->can_id & CAN_SFF_MASK);
  }
  fprintf(fd, ", len: %2u", frame->len);
  fprintf(fd, ", Data: ");
  for(int n = 0; n < CAN_MAX_DLC; n++) {
    fprintf(fd, "%02x, ", frame->data[n]);
  }

  va_list args;
  va_start(args, format);

  vfprintf(fd, format, args);
  va_end(args);

  if(frame->can_id & CAN_ERR_FLAG) {
    fprintf(fd, ", ERROR FRAME");
  }

  fprintf(fd, "\n");
}

#define TX_TIMEOUT_LENGTH_MS  (100) // When we Tx we should see the message come back to us within this time threshold.
// Checks to see if there is space to Tx.
// If we have space then we return a pointer to to the struct usb2can_tx_context, else we return NULL.
struct usb2can_tx_context* get_tx_context(struct usb2can_can* can, struct can_frame* frame) {
  // printf("GET CONTEXT... ");
  for(uint32_t i = 0; i < USB2CAN_MAX_TX_REQ; i++) {
    // printf("%u (%u of %u), ", i, can->tx_context[i].echo_id, USB2CAN_MAX_TX_REQ);
    if(can->tx_context[i].echo_id == USB2CAN_MAX_TX_REQ) {
      can->tx_context[i].can = can;
      can->tx_context[i].echo_id = i;
      can->tx_context[i].timestamp = millis() + TX_TIMEOUT_LENGTH_MS;  // Set a timestamp.
      can->tx_context[i].frame = malloc(sizeof(struct can_frame));
      memcpy(can->tx_context[i].frame, frame, sizeof(struct can_frame));
      // printf("echo_id %u allocated.\n", i);
      return &can->tx_context[i];
    }
  }
  return NULL;
}

// Go through the tx_contexts and check if the messages were sent within the specified time period. If not then we need to cancel the context and resend them.
void handleRetries(struct usb2can_can* can) {
  uint64_t now = millis();
  struct can_frame frame;
  for(uint32_t i = 0; i < USB2CAN_MAX_TX_REQ; i++) {
    if((can->tx_context[i].echo_id < USB2CAN_MAX_TX_REQ) && (now > can->tx_context[i].timestamp)) {
      memcpy(&frame, can->tx_context[i].frame, sizeof(struct can_frame));
      release_tx_context(can, can->tx_context[i].echo_id);
      //send_packet(can, &frame);
    }
  }
}

// Release the tx_echo_id
// If you pass an echo_id out of range it will be ignored.
int release_tx_context(struct usb2can_can* can, uint32_t tx_echo_id) {
  if(tx_echo_id == 0xFFFFFFFF) {
    return 0;
  } else if(tx_echo_id >= USB2CAN_MAX_TX_REQ){
    // print_host_frame("CAN", "IN", &data, 1, "echo_id: %08x (%u) is invalid! TOO LARGE - ERROR!.\n", tx_echo_id, tx_echo_id);
    // printf("ERROR CONTEXT      ");
    // print_time_now();
    // printf("         ");
    // printf(" echo_id: %08x (%u) is invalid! TOO LARGE - ERROR!.\n", tx_echo_id, tx_echo_id);
    return -2;
  } else if(tx_echo_id != can->tx_context[tx_echo_id].echo_id){
    // print_host_frame("CAN", "IN", &data, 1, "echo_id %08x (%u) is invalid! MISMATCH with %08x (%u). - ERROR!.\n", tx_echo_id, tx_echo_id, can->tx_context[tx_echo_id].echo_id, can->tx_context[tx_echo_id].echo_id);
    // printf("ERROR CONTEXT      ");
    // print_time_now();
    // printf("         ");
    // printf(" echo_id %08x (%u) is invalid! MISMATCH with %08x (%u). - ERROR!.\n", tx_echo_id, tx_echo_id, can->tx_context[tx_echo_id].echo_id, can->tx_context[tx_echo_id].echo_id);
    return -1;
  } else if(tx_echo_id < USB2CAN_MAX_TX_REQ) {
    // printf("1");
    // printf("      CONTEXT      ");
    // print_time_now();
    // printf("         ");
    // printf(" echo_id: %08x (%u) %08x (%u) released.\n", tx_echo_id, tx_echo_id, can->tx_context[tx_echo_id].echo_id, can->tx_context[tx_echo_id].echo_id);
    can->tx_context[tx_echo_id].can = NULL;
    can->tx_context[tx_echo_id].echo_id = USB2CAN_MAX_TX_REQ;
    free(can->tx_context[tx_echo_id].frame);
    can->tx_context[tx_echo_id].frame = NULL;  // House keeping
    can->tx_context[tx_echo_id].timestamp = 0; // House keeping
    // printf("2");
    // printf("      CONTEXT      ");
    // print_time_now();
    // printf("         ");
    // printf(" echo_id: %08x (%u) %08x (%u) released.\n", tx_echo_id, tx_echo_id, can->tx_context[tx_echo_id].echo_id, can->tx_context[tx_echo_id].echo_id);
    return 1;
  }
  // print_host_frame("CAN", "IN", &data, 1, "ERROR! Unreachable code reached! - ERROR!\n");
  LOGE("CAN", "IN", "Unreachable code reached!, Line: %i\n", __LINE__);
  // printf("ERROR CONTEXT      ");
  // print_time_now();
  // printf(" ERROR! Unreachable code reached! - ERROR!\n");

  return 0;
}

// Create and allocate space for a struct usb2can_can and initialise it.
// Returns pointer or NULL if failed.
struct usb2can_can* init_usb2can_can(struct libusb_device_handle* devh) {
  struct usb2can_can* can;
  can = calloc(1, sizeof(struct usb2can_can));
  if(can != NULL) {
    can->devh = devh;
    for(uint32_t i = 0; i < USB2CAN_MAX_TX_REQ; i++) {
      can->tx_context[i].can = NULL;
      can->tx_context[i].echo_id = USB2CAN_MAX_TX_REQ;
    }
  }

  return can;
}

void print_host_frame(const char* source, const char* type, struct host_frame *data, uint8_t err, const char *format, ...) {
  FILE * fd = stdout;

  if(err) {
    LOGE(source, type, "ID: ");
    fd = stderr;
  } else {
    LOGI(source, type, "ID: ");
  }

  if((data->can_id & CAN_EFF_FLAG) || (data->can_id & CAN_ERR_FLAG)) {
    fprintf(fd, "%08x", data->can_id & CAN_EFF_MASK);
  } else {
    fprintf(fd, "     %03x", data->can_id & CAN_SFF_MASK);
  }
  
  fprintf(fd, ", DLC: %2u,", data->can_dlc);

  fprintf(fd, " Data: ");
  for(int i = 0; i < CAN_MAX_DLC; i++) {
    fprintf(fd, "%02x, ", data->data[i]);
  }

  fprintf(fd, "echo_id: %08x, ", data->echo_id);
  fprintf(fd, "channel: %2u, ", data->channel);
  fprintf(fd, "flags: %02x", data->flags);
  if(data->flags > 0) {
    fprintf(fd, " (");
    if(data->flags & HOST_FRAME_FLAG_OVERFLOW) {
      fprintf(fd, "HOST_FRAME_FLAG_OVERFLOW ");
    }
    if(data->flags & HOST_FRAME_FLAG_FD) {
      fprintf(fd, "HOST_FRAME_FLAG_FD ");
    }
    if(data->flags & HOST_FRAME_FLAG_BRS) {
      fprintf(fd, "HOST_FRAME_FLAG_BRS ");
    }
    if(data->flags & HOST_FRAME_FLAG_ESI) {
      fprintf(fd, "HOST_FRAME_FLAG_ESI ");
    }
    fprintf(fd, ")");
  }
  fprintf(fd, ", reserved: %02x, ", data->reserved);

  va_list args;
  va_start(args, format);

  vfprintf(fd, format, args);
  va_end(args);

  fprintf(fd, "\n");
}

void print_host_frame_raw(struct host_frame *data) {
  printf("Raw: |     echo_id      |       can_id      |dlc | ch |flg | rs |  0 |  1 |  2 |  3 |  4 |  5 |  6 |  7 |\n");
  // printf("Raw:  xx   xx   xx   xx   xx   xx   xx   xx   xx   xx   xx   xx   xx   xx   xx   xx   xx   xx   xx   xx");
  printf("Raw:  ");
  for(int i = 0; i < sizeof(struct host_frame); i++) {
    printf("%02x   ", ((uint8_t*)data)[i]);
  }
  printf("\n");
}

int read_packet(struct usb2can_can* can) {
  struct host_frame data;
  memset(&data, 0, sizeof(data));
  int len = 0;
  int ret = libusb_bulk_transfer(can->devh, ENDPOINT_IN, (uint8_t*) &data, sizeof(data), &len, 1);
  if(ret == 0) {
    if(len != sizeof(data)) {
      LOGE("CAN", "IN", "Size mismatch! sizeof(data) = %lu, len = %u, ret = %x \n", sizeof(data), len, ret);
      print_host_frame_raw(&data);
      fflush(stdout);
    }

    if(data.can_id & CAN_ERR_FLAG) {
      print_host_frame("CAN", "IN", &data, 1, "");
      print_host_frame_raw(&data);
    } else if((data.channel >= USB2CAN_MAX_CHANNELS) || (data.can_dlc > CAN_MAX_DLC)) {
      print_host_frame("CAN", "IN", &data, 1, "");
      print_host_frame_raw(&data);
    } else {
      int tmp1 = release_tx_context(can, le32toh(data.echo_id));
      if(tmp1 > 0) {
        print_host_frame("CAN", "IN", &data, 0, "Context Released");
      } else if(tmp1 == 0) {
        print_host_frame("CAN", "IN", &data, 0, "");
      } else if(tmp1 == -2) {
        print_host_frame("CAN", "IN", &data, 1, "echo_id: %08x (%u) is invalid! TOO LARGE - ERROR!.\n", data.echo_id, data.echo_id);

        print_host_frame_raw(&data);
        fflush(stdout);
      } else if(tmp1 == -1) {
        // print_host_frame("CAN", "IN", &data, 1, "Context Error");
        print_host_frame("CAN", "IN", &data, 1, "echo_id %08x (%u) is invalid! MISMATCH with %08x (%u). - ERROR!.\n", data.echo_id, data.echo_id, can->tx_context[data.echo_id].echo_id, can->tx_context[data.echo_id].echo_id);

        print_host_frame_raw(&data);
        fflush(stdout);
      } else if(tmp1 < 0) {
        print_host_frame("CAN", "IN", &data, 1, "Context Error");

        print_host_frame_raw(&data);
        fflush(stdout);
      }

      struct can_frame frame;

      frame.can_id = le32toh(data.can_id);

      frame.len = data.can_dlc;
      if(frame.len > CAN_MAX_DLC) {
        frame.len = CAN_MAX_DLC;
      }

      for(int i = 0; i < frame.len; i++) {
        frame.data[i] = data.data[i];
      }

      sendCANToAll(&frame);
    }
  } else if(ret != LIBUSB_ERROR_TIMEOUT) {
    switch(ret) {
    // case LIBUSB_ERROR_TIMEOUT:
    //   print_host_frame("CAN", "IN", &data, 1, "LIBUSB_ERROR_TIMEOUT");
    //   break;
    case LIBUSB_ERROR_PIPE:
      print_host_frame("CAN", "IN", &data, 1, "LIBUSB_ERROR_PIPE");
      break;
    case LIBUSB_ERROR_OVERFLOW:
      print_host_frame("CAN", "IN", &data, 1, "LIBUSB_ERROR_OVERFLOW");
      break;
    case LIBUSB_ERROR_NO_DEVICE:
      print_host_frame("CAN", "IN", &data, 1, "LIBUSB_ERROR_NO_DEVICE");
      break;
    case LIBUSB_ERROR_BUSY:
      print_host_frame("CAN", "IN", &data, 1, "LIBUSB_ERROR_BUSY");
      break;
    case LIBUSB_ERROR_INVALID_PARAM:
      print_host_frame("CAN", "IN", &data, 1, "LIBUSB_ERROR_INVALID_PARAM");
      break;
    default:
      print_host_frame("CAN", "IN", &data, 1, "UKNOWN (0x%08x)", ret);
      break;
    }
    fflush(stdout);
  }

  return ret;
}

int send_packet(struct usb2can_can* can, struct can_frame* frame) {

  struct usb2can_tx_context* tx_context = get_tx_context(can, frame);
  if(tx_context == NULL) {
    print_can_frame("Q", "OUT", frame, 1, "BUSY");
    return LIBUSB_ERROR_BUSY;
  }

  struct host_frame data = {
    .echo_id = htole32(tx_context->echo_id),
    .can_id = htole32(frame->can_id),
    .can_dlc = frame->len,
    .channel = 0,
    .flags = 0,
    .reserved = 0
  };
  if(frame->can_id > 0x03ff) {
    data.can_id |= CAN_EFF_FLAG; // Set the extended bit flag (if not already set)
  }
  for(int i = 0; i < CAN_MAX_DLEN; i++) {
    if(i < frame->len) {
      data.data[i] = frame->data[i];
    } else {
      data.data[i] = 0;
    }
  }

  int len = 0;
  int ret = libusb_bulk_transfer(can->devh, ENDPOINT_OUT, (uint8_t*) &data, sizeof(data), &len, 1);
  if((len != sizeof(data)) && (ret != LIBUSB_ERROR_TIMEOUT)) {
    print_can_frame("Q", "OUT", frame, 1, "ERROR");
    LOGE("CAN", "OUT", "Size mismatch! sizeof(data) = %lu, len = %u, ret = %x \n", sizeof(data), len, ret);
    print_host_frame_raw(&data);
    fflush(stdout);
  }
  if(ret == 0) {
    print_can_frame("Q", "OUT", frame, 0, "SUCCESS");
    print_host_frame("CAN", "OUT", &data, 0, "Tx Queue");
    fflush(stdout);
  } else if (ret == LIBUSB_ERROR_TIMEOUT) {
    print_can_frame("Q", "OUT", frame, 1, "TIMEOUT");
  } else {
    print_can_frame("Q", "OUT", frame, 1, "ERROR");
    print_host_frame("CAN", "OUT", &data, 1, "%s: %s\n", libusb_error_name(ret), libusb_strerror(ret));
    print_host_frame_raw(&data);
    fflush(stdout);
  }
  return ret;
}

// These are the speed settings. We'd like to offer custom bitrates and sample points too.
int set_bitrate(struct usb2can_can* can) {
  LOGI(__FUNCTION__, "INFO", "Setting bitrate...\n");
  uint8_t bmReqType = 0x41;       // the request type (direction of transfer)
  uint8_t bReq = USB2CAN_BREQ_BITTIMING;            // the request field for this packet
  uint16_t wVal = 0x0000;         // the value field for this packet
  uint16_t wIndex = 0x0000;       // the index field for this packet
  uint16_t wLen = BITRATE_DATA_LEN;   // length of this setup packet 
  unsigned int to = 0;    // timeout duration (if transfer fails)

  // transfer the setup packet to the USB device
  int config = libusb_control_transfer(can->devh,bmReqType,bReq,wVal,wIndex, bitrates[bitrate], wLen,to);

  return config;
}

// Set User ID: candleLight allows optional support for reading/writing of a user defined value into the device's flash. It's isn't widely supported and probably isn't required most of the time.
int port_set_user_id(struct usb2can_can* can) {
#ifdef DEBUG_INFO_SETUP
  LOGI(__FUNCTION__, "INFO", "Set the User ID (USB2CAN_BREQ_SET_USER_ID)\n");
#endif
  uint8_t bmReqType = 0x00;       // the request type (direction of transfer)
  uint8_t bReq = USB2CAN_BREQ_SET_USER_ID;            // the request field for this packet
  uint16_t wVal = 0x0001;         // the value field for this packet
  uint16_t wIndex = 0x0000;       // the index field for this packet
  uint16_t wLen = 0;      // length of this setup packet 
  unsigned char data[] = {0x00};
  unsigned int to = 0;    // timeout duration (if transfer fails)

  // transfer the setup packet to the USB device
  int config = libusb_control_transfer(can->devh, bmReqType, bReq, wVal, wIndex, data, wLen, to);

  return config;
}

// Host format sets the endian. We write 0x0000beef to the device and it determines the endianess from that. However, candleLight may only support Little Endian so that why we always send it it Little Endian.
int port_set_host_format(struct usb2can_can* can) {
#ifdef DEBUG_INFO_SETUP
  LOGI(__FUNCTION__, "INFO", "Set the host format to Little Endian (as candleLight devices are always Little Endian) (USB2CAN_BREQ_HOST_FORMAT)\n");
#endif
  uint8_t bmReqType = 0x41;       // the request type (direction of transfer)
  uint8_t bReq = USB2CAN_BREQ_HOST_FORMAT;            // the request field for this packet
  uint16_t wVal = 0x0001;         // the value field for this packet
  uint16_t wIndex = 0x0000;       // the index field for this packet
  // the data buffer for the in/output data
  unsigned char data[] = {
    0xef, 0xbe, 0x00, 0x00
  };
  uint16_t wLen = sizeof(data);   // length of this setup packet 
  unsigned int to = 0;    // timeout duration (if transfer fails)

  // transfer the setup packet to the USB device
  int config = libusb_control_transfer(can->devh, bmReqType, bReq, wVal, wIndex, data, wLen, to);

  return config;
}

// Get device information from the USB 2 CAN device. Includes SW and HW versions.
int port_get_device_config(struct usb2can_can* can) {
#ifdef DEBUG_INFO_SETUP
  LOGI(__FUNCTION__, "INFO", "Getting device config info (USB2CAN_BREQ_DEVICE_CONFIG)\n");
#endif
  uint8_t bmReqType = 0xC1;       // the request type (direction of transfer)
  uint8_t bReq = USB2CAN_BREQ_DEVICE_CONFIG;            // the request field for this packet
  uint16_t wVal = 0x0001;         // the value field for this packet
  uint16_t wIndex = 0x0000;       // the index field for this packet
  // the data buffer for the in/output data
  struct usb2can_device_config data;
  uint16_t wLen = sizeof(data);   // length of this setup packet 
  unsigned int to = 0;    // timeout duration (if transfer fails)

  // transfer the setup packet to the USB device
  int ret = libusb_control_transfer(can->devh, bmReqType, bReq, wVal, wIndex, (uint8_t *)(&data), wLen, to);
  if(ret >=0) {
    can->device_config.reserved1 = data.reserved1;
    can->device_config.reserved2 = data.reserved2;
    can->device_config.reserved3 = data.reserved3;
    can->device_config.sw_version = le32toh(data.sw_version);
    can->device_config.hw_version = le32toh(data.hw_version);
    LOGI(__FUNCTION__, "INFO", "Device Config:\n");
    LOGI(__FUNCTION__, "INFO", "   reserved1: 0x%02x\n", can->device_config.reserved1);
    LOGI(__FUNCTION__, "INFO", "   reserved2: 0x%02x\n", can->device_config.reserved2);
    LOGI(__FUNCTION__, "INFO", "   reserved3: 0x%02x\n", can->device_config.reserved3);
    LOGI(__FUNCTION__, "INFO", "     i count: %u (%u interfaces)\n", can->device_config.icount, data.icount + 1);
    LOGI(__FUNCTION__, "INFO", "  sw_version: %u\n", can->device_config.sw_version);
    LOGI(__FUNCTION__, "INFO", "  hw_version: %u\n", can->device_config.hw_version);
  }

  return ret;
}

// Get the Bit Timming settings.
int port_get_bit_timing(struct usb2can_can* can) {
  LOGI(__FUNCTION__, "INFO", "Getting the port's bit timing info (USB2CAN_BREQ_BT_CONST)\n");
  uint8_t bmReqType = 0xC1;       // the request type (direction of transfer)
  uint8_t bReq = USB2CAN_BREQ_BT_CONST;            // the request field for this packet
  uint16_t wVal = 0x0000;         // the value field for this packet
  uint16_t wIndex = 0x0000;       // the index field for this packet
  // the data buffer for the in/output data
  struct usb2can_device_bt_const data;
  uint16_t wLen = sizeof(data);   // length of this setup packet 
  unsigned int to = 0;    // timeout duration (if transfer fails)

  // transfer the setup packet to the USB device
  int ret = libusb_control_transfer(can->devh, bmReqType, bReq, wVal, wIndex, (uint8_t *)(&data), wLen, to);
  if(ret >=0) {
    can->bt_const.feature = le32toh(data.feature);
    can->bt_const.fclk_can = le32toh(data.fclk_can);
    can->bt_const.tseg1_min = le32toh(data.tseg1_min);
    can->bt_const.tseg1_max = le32toh(data.tseg1_max);
    can->bt_const.tseg2_min = le32toh(data.tseg2_min);
    can->bt_const.tseg2_max = le32toh(data.tseg2_max);
    can->bt_const.sjw_max = le32toh(data.sjw_max);
    can->bt_const.brp_min = le32toh(data.brp_min);
    can->bt_const.brp_max = le32toh(data.brp_max);
    can->bt_const.brp_inc = le32toh(data.brp_inc);
#ifdef DEBUG_INFO_SETUP
    LOGI(__FUNCTION__, "INFO", "BT Const Information:\n");
    int16_t feature = can->bt_const.feature;
    LOGI(__FUNCTION__, "INFO", "    feature: %08x (", feature);
    if(feature & USB2CAN_FEATURE_LISTEN_ONLY) {
      printf("USB2CAN_FEATURE_LISTEN_ONLY, ");
    }
    if(feature & USB2CAN_FEATURE_LOOP_BACK) {
      printf("USB2CAN_FEATURE_LOOP_BACK, ");
    }
    if(feature & USB2CAN_FEATURE_TRIPLE_SAMPLE) {
      printf("USB2CAN_FEATURE_TRIPLE_SAMPLE, ");
    }
    if(feature & USB2CAN_FEATURE_ONE_SHOT) {
      printf("USB2CAN_FEATURE_ONE_SHOT, ");
    }
    if(feature & USB2CAN_FEATURE_HW_TIMESTAMP) {
      printf("USB2CAN_FEATURE_HW_TIMESTAMP, ");
    }
    if(feature & USB2CAN_FEATURE_IDENTIFY) {
      printf("USB2CAN_FEATURE_IDENTIFY, ");
    }
    if(feature & USB2CAN_FEATURE_USER_ID) {
      printf("USB2CAN_FEATURE_USER_ID, ");
    }
    if(feature & USB2CAN_FEATURE_PAD_PKTS_TO_MAX_PKT_SIZE) {
      printf("USB2CAN_FEATURE_PAD_PKTS_TO_MAX_PKT_SIZE, ");
    }
    if(feature & USB2CAN_FEATURE_FD) {
      printf("USB2CAN_FEATURE_FD, ");
    }
    if(feature & USB2CAN_FEATURE_REQ_USB_QUIRK_LPC546XX) {
      printf("USB2CAN_FEATURE_REQ_USB_QUIRK_LPC546XX, ");
    }
    if(feature & USB2CAN_FEATURE_BT_CONST_EXT) {
      printf("USB2CAN_FEATURE_BT_CONST_EXT, ");
    }
    if(feature & USB2CAN_FEATURE_TERMINATION) {
      printf("USB2CAN_FEATURE_TERMINATION, ");
    }
    if(feature & USB2CAN_FEATURE_BERR_REPORTING) {
      printf("USB2CAN_FEATURE_BERR_REPORTING, ");
    }
    if(feature & USB2CAN_FEATURE_GET_STATE) {
      printf("USB2CAN_FEATURE_GET_STATE, ");
    }
    printf(")\n");
    LOGI(__FUNCTION__, "INFO", "   fclk_can: %u\n", can->bt_const.fclk_can);
    LOGI(__FUNCTION__, "INFO", "  tseg1_min: %u\n", can->bt_const.tseg1_min);
    LOGI(__FUNCTION__, "INFO", "  tseg1_max: %u\n", can->bt_const.tseg1_max);
    LOGI(__FUNCTION__, "INFO", "  tseg2_min: %u\n", can->bt_const.tseg2_min);
    LOGI(__FUNCTION__, "INFO", "  tseg2_max: %u\n", can->bt_const.tseg2_max);
    LOGI(__FUNCTION__, "INFO", "    sjw_max: %u\n", can->bt_const.sjw_max);
    LOGI(__FUNCTION__, "INFO", "    brp_min: %u\n", can->bt_const.brp_min);
    LOGI(__FUNCTION__, "INFO", "    brp_max: %u\n", can->bt_const.brp_max);
    LOGI(__FUNCTION__, "INFO", "    brp_inc: %u\n", can->bt_const.brp_inc);
#endif
  }

  return ret;
}

int port_open(struct libusb_device_handle *devh) {
  LOGI(__FUNCTION__, "INFO", "Opening the port (USB2CAN_BREQ_MODE)\n");
  uint8_t bmReqType = 0x41;       // the request type (direction of transfer)
  uint8_t bReq = USB2CAN_BREQ_MODE;            // the request field for this packet
  uint16_t wVal = 0x0000;         // the value field for this packet
  uint16_t wIndex = 0x0000;       // the index field for this packet
  
  // the data buffer for the in/output data
  unsigned char data[] = {
    0x01, 0x00, 0x00, 0x00, // CAN_MODE_START
    0x00, 0x00, 0x00, 0x00 
  };
  uint16_t wLen = sizeof(data);   // length of this setup packet 
  unsigned int to = 0;    // timeout duration (if transfer fails)

  // transfer the setup packet to the USB device
  int config = libusb_control_transfer(devh, bmReqType, bReq, wVal, wIndex, data, wLen, to);

  return config;
}

int port_close(struct libusb_device_handle *devh) {
  LOGI(__FUNCTION__, "INFO", "Closing the port (USB2CAN_BREQ_MODE)\n");
  uint8_t bmReqType = 0x41;       // the request type (direction of transfer)
  uint8_t bReq = USB2CAN_BREQ_MODE;            // the request field for this packet
  uint16_t wVal = 0x0000;         // the value field for this packet
  uint16_t wIndex = 0x0000;       // the index field for this packet
  // the data buffer for the in/output data
  unsigned char data[] = {
    0x00, 0x00, 0x00, 0x00, // CAN_MODE_RESET
    0x00, 0x00, 0x00, 0x00
  };
  uint16_t wLen = sizeof(data);   // length of this setup packet 
  unsigned int to = 0;    // timeout duration (if transfer fails)

  // transfer the setup packet to the USB device
  int config = libusb_control_transfer(devh, bmReqType, bReq, wVal, wIndex, data, wLen, to);

  return config;
}

#define NCLIENTS (10)

#define CLIENT_TYPE_NONE  (0)
#define CLIENT_TYPE_SOCK  (1)
#define CLIENT_TYPE_LIBUSB  (2)

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

int sendCANToAll(struct can_frame * frame) {
  print_can_frame("PIPE", "OUT", frame, 0, "");

  int i;
  int cnt = 0;
  for(i = 0; i < NCLIENTS; i++) {
    if((clients[i].fd > 0) && (clients[i].typ == CLIENT_TYPE_SOCK)) {
      int ret = sockSend(clients[i].fd, frame, sizeof(frame));
      if(ret > 0) {
        cnt++;
      }
    }
  }
  return cnt; // How many we succesfully sent to.
}

int processing_loop(int kq, int sockFd, struct usb2can_can* can, libusb_context *ctx) {
  struct kevent evSet;
  struct kevent evList[MAX_EVENTS];
  struct sockaddr_storage addr;
  socklen_t socklen = sizeof(addr);
  int fd;
  struct timespec zero_ts = {
    .tv_sec = 0,
    .tv_nsec = 0
  };
  struct can_frame frame;

  LOGI(__FUNCTION__, "INFO", "Entering Main Loop...\n");
  LOGI(__FUNCTION__, "INFO", "sockFd = %i\n", sockFd);

  while(1) {
    // if(LIBUSB_ERROR_NO_DEVICE == read_packet(can)) {
    //   break;
    // }
    int max = 0;
    int ret = 0;
    while((0 == ret) && (max < USB2CAN_MAX_RX_REQ)) {
      ret = read_packet(can);
      max++;
    }
    if(LIBUSB_ERROR_NO_DEVICE == ret) {
      break;
    }
    handleRetries(can);

    int nev = kevent(kq, NULL, 0, evList, MAX_EVENTS, &zero_ts);
    if(nev < 0) {
      LOGE(__FUNCTION__, "INFO", "kevent error\n");
      exit(1);
    }

    for(int i = 0; i < nev; i++) {
      if(sockFd == (int)(evList[i].ident)) {
        LOGI(__FUNCTION__, "INFO", "Socket event!\n");
        fd = accept(evList[i].ident, (struct sockaddr *)&addr, & socklen);
        if(fd == -1) {
          LOGE(__FUNCTION__, "INFO", "kevent error\n");
        } else if(conn_add(fd, CLIENT_TYPE_SOCK) == 0) {
        LOGI(__FUNCTION__, "INFO", "Accepted socket %i.\n", fd);
          EV_SET(&evSet, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
          assert(-1 != kevent(kq, &evSet, 1, NULL, 0, NULL));
        } else {
          LOGE(__FUNCTION__, "INFO", "Connection refused!\n");
          close(fd);
        }
      } else if(conn_index((int)(evList[i].ident)) > -1) {
        fd = (int)(evList[i].ident);
        switch(clients[conn_index(fd)].typ) {
        case CLIENT_TYPE_SOCK:
          LOGI(__FUNCTION__, "INFO", "Client socket event! fd = %i, index = %i\n", fd, conn_index(fd));
          if(evList[i].flags & EV_EOF) {
            LOGI(__FUNCTION__, "INFO", "Socket closed: %i\n", fd);
            conn_close(fd);
          } else {
            int ret = recv(fd, &frame, sizeof(struct can_frame), 0);
            if(ret != sizeof(struct can_frame)) {
              LOGE(__FUNCTION__, "INFO", "Read %u bytes, expected %lu bytes!\n", ret, sizeof(struct can_frame));
            } else {
              print_can_frame("PIPE", "IN", &frame, 0, "");
              send_packet(can, &frame);
            }

          }
          break;
        }
      }
    }
  }

  return 0;
}

void printusage() {
  printf("usb2can opens a the frist USB2CAN device that it finds and binds to a socket (default address is 2303).\n");
  printf("All messages received will be presented as Hex in the format:\n");
  printf("  IIIIIIII LL DDDDDDDDDDDDDDDD[CR][LF]\n");
  printf("\n");
  printf("Where: \n");
  printf("  I = Message ID: 8 chars representing a 32-bit unsigned with leading zeros.\n");
  printf("  L = Data Length code: 2 chars representing an 8-bit unsigned with leading zeros. Valid range 0 to 8.\n");
  printf("  D = Data: 0 to 16 chars (depending on Data Length Code) representing a octets with leading zeros.\n");
  printf("  [CR][LF] = Carridge Return Line Feed. \n");
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
  printf("  d[nnnn] = a \"d\" followed by a number - if there are multiple device connected then speficy which to use. \n");
  printf("\n");
}

int port = 2303;  // The port that we're going to open.
int deviceNumber = 0; // If there's multiple device connected then use this one.

void processArgs(int argc, char *argv[]) {
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
      } else if(argv[i][0] == 'd') {
        int deviceNumber2 = atoi(&(argv[i][1]));
        deviceNumber = deviceNumber2;
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
int main(int argc, char *argv[]) {
  int ret = 0;

  // Create the signal handler here - ensures that Ctrl-C gets passed back up to 
  signal(SIGINT, sigint_handler);

  processArgs(argc, argv);

  // Create and bind our socket here.
  LOGI(__FUNCTION__, "INFO", "Creating our server here...\n");
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
  LOGI(__FUNCTION__, "INFO", "Listening on %i\n", port);

  LOGI(__FUNCTION__, "INFO", "Setting up libusb for CAN socket...\n");
  const struct libusb_version * ver = NULL;
  LOGI(__FUNCTION__, "INFO", "LIBUSB_API_VERSION = %08x\n", LIBUSB_API_VERSION);

  ver = libusb_get_version();
  LOGI(__FUNCTION__, "INFO", "LibUSB version = %i.%i.%i.%i %s %s\n", ver->major, ver->minor, ver->micro, ver->nano, ver->rc, ver->describe);

  LOGI(__FUNCTION__, "INFO", "Trying libusb_init...\n");
  libusb_context *ctx;
  ret = libusb_init(&ctx);
  if (ret < 0) {
    LOGE(__FUNCTION__, "INFO", "ERROR: failed to initialize libusb (ret = %d)\n", ret);
    exit(1);
  }

  libusb_set_debug(NULL, LIBUSB_LOG_LEVEL_DEBUG);
  //libusb_set_debug(NULL, LIBUSB_LOG_LEVEL_INFO);

  libusb_device **list;
  ssize_t cnt = libusb_get_device_list(NULL, &list);
  if (cnt < 0) LOGI(__FUNCTION__, "INFO", "ERROR: failed to get device list (cnt = %ld)\n", cnt);
  LOGI(__FUNCTION__, "INFO", "%ld USB devices found.\n", cnt);
  LOGI(__FUNCTION__, "INFO", "USB Devices found:\n");

  // This is where we build out list of device that we are looking for.
  libusb_device* validdevices[cnt];
  int devCnt = 0;
  for (ssize_t i = 0; i < cnt; i++) {
    libusb_device *dev = list[i];
    struct libusb_device_descriptor desc;
    int r = libusb_get_device_descriptor(dev, &desc);
    if (r < 0) LOGI(__FUNCTION__, "ERROR", "failed to get device descriptor (r = %d)\n", r);
    if(((USB_VENDOR_ID_GS_USB_1 == desc.idVendor) && (USB_PRODUCT_ID_GS_USB_1 == desc.idProduct))
      || ((USB_VENDOR_ID_CANDLELIGHT == desc.idVendor) && (USB_PRODUCT_ID_CANDLELIGHT == desc.idProduct))
      || ((USB_VENDOR_ID_CES_CANEXT_FD == desc.idVendor) && (USB_PRODUCT_ID_CES_CANEXT_FD == desc.idProduct))
      || ((USB_VENDOR_ID_ABE_CANDEBUGGER_FD == desc.idVendor) && (USB_PRODUCT_ID_ABE_CANDEBUGGER_FD == desc.idProduct))) {
      LOGI(__FUNCTION__, "INFO", "%2ld Vendor ID: %i (0x%04x), Product ID: %i (0x%04x), Manufacturer: %i, Product: %i, Serial: %i *** DEVICE %i ***\n", i+1, desc.idVendor, desc.idVendor, desc.idProduct, desc.idProduct, desc.iManufacturer, desc.iProduct, desc.iSerialNumber, devCnt);
      validdevices[devCnt] = dev;
      devCnt++;
    } else {
      LOGI(__FUNCTION__, "INFO", "%2ld Vendor ID: %i (0x%04x), Product ID: %i (0x%04x), Manufacturer: %i, Product: %i, Serial: %i\n", i+1, desc.idVendor, desc.idVendor, desc.idProduct, desc.idProduct, desc.iManufacturer, desc.iProduct, desc.iSerialNumber);
    }
  }

  LOGI(__FUNCTION__, "INFO", "Compatible USB to CAN Devices found: %i\n", devCnt);


  struct libusb_device_handle *devh = NULL;
  if(deviceNumber >= devCnt) {
    LOGE(__FUNCTION__, "INFO", "Unable to open device %i as there are only %i devices. Note: Device numbering starts at 0.\n", deviceNumber, devCnt);
    exit(1);
  }
  LOGI(__FUNCTION__, "INFO", "Attempting to device %i now...\n", deviceNumber);
  ret = libusb_open(validdevices[deviceNumber], &devh);
  if(ret != 0) {
    LOGE(__FUNCTION__, "INFO", "failed to open the device.\n");
    exit(1);
  }
  LOGI(__FUNCTION__, "INFO", "Device opened.\n");

  libusb_free_device_list(list, 1);

  int interface = 0;
  LOGI(__FUNCTION__, "INFO", "Checking if kernel driver is active...\n");
  if(libusb_kernel_driver_active(devh, interface) == 1) {
    LOGI(__FUNCTION__, "INFO", "Detaching kernel driver...\n");
    int ret = libusb_detach_kernel_driver(devh, interface);
    if(ret < 0) {
      LOGE(__FUNCTION__, "INFO", "%s: %s Unable to detach.\n", libusb_error_name(ret), libusb_strerror(ret));
    }
  }

  LOGI(__FUNCTION__, "INFO", "Claiming interface...\n");
  ret = libusb_claim_interface(devh, interface);
  if(ret < 0) {
    LOGE(__FUNCTION__, "INFO", "%s: %s Unable to claim interface %d.\n", libusb_error_name(ret), libusb_strerror(ret), interface);
  }

  LOGI(__FUNCTION__, "INFO", "Getting descriptor...\n");
  libusb_device* device = libusb_get_device(devh);
  struct libusb_config_descriptor* descriptor = NULL;
  if(libusb_get_active_config_descriptor(device, &descriptor) < 0)
  {
    LOGE(__FUNCTION__, "INFO", "Failed to get config descriptor\n");
  }
  //check for correct endpoints
  LOGI(__FUNCTION__, "INFO", "Endpoints found:");
  if(descriptor->bNumInterfaces > 0)
  {
    struct libusb_interface_descriptor iface = descriptor->interface[0].altsetting[0];
    for(int i=0; i<iface.bNumEndpoints; ++i) {
      LOGI(__FUNCTION__, "INFO", " %d (0x%02x),", iface.endpoint[i].bEndpointAddress, iface.endpoint[i].bEndpointAddress);
    }
  }

  printf("\n");
  libusb_free_config_descriptor(descriptor);

  LOGI(__FUNCTION__, "INFO", "Creating our CAN context...\n");
  struct usb2can_can * can = init_usb2can_can(devh);
  LOGI(__FUNCTION__, "INFO", "CAN context created!\n");

  LOGI(__FUNCTION__, "INFO", "Setting up for comms...\n");
  ret = port_set_user_id(can);
  if(ret < 0) {
    LOGE(__FUNCTION__, "INFO", "ERROR! Unable to set user ID.\n");
    exit(1);
  }
  ret = port_set_host_format(can);
  if(ret < 0) {
    LOGE(__FUNCTION__, "INFO", "ERROR! Unable to set host format.\n");
    exit(1);
  }
  ret = port_get_device_config(can);
  if(ret < 0) {
    fprintf(stderr, "ERROR! Unable to setup2.\n");
    exit(1);
  }
  ret = port_get_bit_timing(can);
  if(ret < 0) {
    LOGE(__FUNCTION__, "INFO", "ERROR! Unable to get bit timing.\n");
    exit(1);
  }

  ret = set_bitrate(can);
  if(ret < 0) {
    LOGE(__FUNCTION__, "INFO", "ERROR! Unable to set bitrate.\n");
    exit(1);
  }

  LOGI(__FUNCTION__, "INFO", "Opening port...\n");
  ret = port_open(devh);
  if(ret < 0) {
    LOGE(__FUNCTION__, "INFO", "ERROR! Unable to open port.\n");
    exit(1);
  }
  LOGI(__FUNCTION__, "INFO", "USB to CAN device is connected!\n");

  LOGI(__FUNCTION__, "INFO", "Creating Event Queue...\n");
  int kq = kqueue();
  struct kevent evSet;

  EV_SET(&evSet, sock, EVFILT_READ, EV_ADD, 0, 0, NULL);
  assert(-1 != kevent(kq, &evSet, 1, NULL, 0, NULL));

  LOGI(__FUNCTION__, "INFO", "Starting main program loop...\n");
  processing_loop(kq, sock, can, ctx);

  ret = port_close(devh);
  if(ret < 0) {
    LOGE(__FUNCTION__, "INFO", "ERROR! Unable to close port.\n");
  }

  ret = libusb_attach_kernel_driver(devh, interface);
  if(ret < 0) {
    LOGE(__FUNCTION__, "INFO", "%s: %s Unable to reattach existing driver.\n", libusb_error_name(ret), libusb_strerror(ret));
  }

  if(devh != NULL) {
    libusb_close(devh);
    LOGI(__FUNCTION__, "INFO", "Device closed.\n");
  }

  LOGI(__FUNCTION__, "INFO", "Trying libusb_exit...\n");
  libusb_exit(ctx);
  return ret;
}

