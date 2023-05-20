// test.c

#include <stdio.h>      /* Standard input/output definitions */
#include <string.h>     /* String function definitions */
#include <unistd.h>     /* UNIX standard function definitions */
#include <fcntl.h>      /* File control definitions */
#include <errno.h>      /* Error number definitions */
#include <termios.h>    /* POSIX terminal control definitions */
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
#include <stdarg.h>
#include <inttypes.h>


#include "usb2can.h"

#ifdef __CHERI_PURE_CAPABILITY__
#define PRINTF_PTR "#p"
#else
#define PRINTF_PTR "p"
#endif

#define BUFSIZE 1024

// function prototypes
int tcpopen(const char *host, int port);
void sendbuftosck(int sckfd, const char *buf, int len);
int sendcantosck(int sckfd, struct can_frame* frame);

#define KQUEUE_CH_SIZE 	3
#define KQUEUE_EV_SIZE	3
#define TIMER_FD  1234

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

int main(int argc, char *argv[])
{

  // Create the signal handler here - ensures that Ctrl-C gets passed back up to 
  signal(SIGINT, sigint_handler);

  struct kevent chlist[KQUEUE_CH_SIZE]; // events we want to monitor
  struct kevent evlist[KQUEUE_EV_SIZE]; // events that were triggered
  char buf[BUFSIZE]; 
  int sckfd, kq, nev, i;
  struct can_frame frame;	// The incoming frame
  int period_ms = 100;

  LOGI(__FUNCTION__, "INFO", "starting...\n");

  // check argument count
  if (argc != 3) { 
    fprintf(stderr, "USB2CAN Test app\n\n");
    fprintf(stderr, "usage: %s host port\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  // open a connection to a host:port pair
  sckfd = tcpopen(argv[1], atoi(argv[2]));

  // create a new kernel event queue
  if ((kq = kqueue()) == -1) {
    LOGE(__FUNCTION__, "INFO", "Unable to create kqueue\n");
    exit(EXIT_FAILURE);
  }

  // initialise kevent structures
  EV_SET(&chlist[0], sckfd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
  EV_SET(&chlist[1], fileno(stdin), EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
  EV_SET(&chlist[2], TIMER_FD, EVFILT_TIMER, EV_ADD | EV_ENABLE, 0, period_ms, 0);
  nev = kevent(kq, chlist, KQUEUE_CH_SIZE, NULL, 0, NULL);
  if (nev < 0) {
    LOGE(__FUNCTION__, "INFO", "Unable to listen to kqueue\n");
    exit(EXIT_FAILURE);
  }

  printf("Starting loop...\n");
  uint32_t count = 0;
  // loop forever
  for (;;)
  {
    nev = kevent(kq, NULL, 0, evlist, KQUEUE_EV_SIZE, NULL);

    if (nev < 0) {
      LOGE(__FUNCTION__, "INFO", "Unable to listen to kqueue 2\n");
      exit(EXIT_FAILURE);
    }
    else if (nev > 0)
    {
      for (i = 0; i < nev; i++) {
        if (evlist[i].flags & EV_EOF) {
          LOGE(__FUNCTION__, "INFO", "Read direction of socket has shutdown\n");
          exit(EXIT_FAILURE);
        }

        if (evlist[i].flags & EV_ERROR) {                /* report errors */
          LOGE(__FUNCTION__, "INFO", "EV_ERROR: %s\n", strerror(evlist[i].data));
          exit(EXIT_FAILURE);
        }
  
        if(evlist[i].ident == TIMER_FD) {
          // LOGI(__FUNCTION__, "INFO", "Timeout.\n");
          frame.can_id = 0x01U;
          frame.len = 8;
          frame.data[0] = (uint8_t)((count >> 24) & 0x000000FF);
          frame.data[1] = (uint8_t)((count >> 16) & 0x000000FF);
          frame.data[2] = (uint8_t)((count >> 8) & 0x000000FF);
          frame.data[3] = (uint8_t)(count & 0x000000FF);
          frame.data[4] = 0x01;
          frame.data[5] = 0x23;
          frame.data[6] = 0x45;
          frame.data[7] = 0x67;
          count++;
          sendcantosck(sckfd, &frame);
        } else if (evlist[i].ident == sckfd) {                  /* we have data from the host */
          memset(buf, 0, BUFSIZE);
          int i = recv(sckfd, &frame, sizeof(frame), 0);
          if (i < 0){                /* report errors */
            LOGE(__FUNCTION__, "INFO", "recv()\n");
            exit(EXIT_FAILURE);
          }

          print_can_frame("PIPE", "IN", &frame, 0, "");
        } else if (evlist[i].ident == fileno(stdin)) {     /* we have data from stdin */
          memset(buf, 0, BUFSIZE);
          fgets(buf, BUFSIZE, stdin);
          sendbuftosck(sckfd, buf, strlen(buf));
        }
      }
    }
  }

  close(kq);
  return EXIT_SUCCESS;
}

void diep(const char *s) {
  perror(s); exit(EXIT_FAILURE);
}

int tcpopen(const char *host, int port) {
  struct sockaddr_in server;
  int sckfd;

  struct hostent *hp = gethostbyname(host);
  if (hp == NULL)
    diep("gethostbyname()");

  if ((sckfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    diep("socket()");

  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  server.sin_addr = (*(struct in_addr *)hp->h_addr);
  memset(&(server.sin_zero), 0, 8);

  if (connect(sckfd, (struct sockaddr *)&server, sizeof(struct sockaddr)) < 0)
    diep("connect()");

  return sckfd;
}

int sendcantosck(int sckfd, struct can_frame* frame) {
  print_can_frame("PIPE", "OUT", frame, 0, "");
  return send(sckfd, frame, sizeof(struct can_frame), 0);
}

void sendbuftosck(int sckfd, const char *buf, int len) {
  int bytessent, pos;

  pos = 0;
  do {
    if ((bytessent = send(sckfd, buf + pos, len - pos, 0)) < 0)
      diep("send()");
    pos += bytessent;
  } while (bytessent > 0);
}
