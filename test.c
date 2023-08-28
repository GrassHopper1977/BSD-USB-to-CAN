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
#include "utils/timestamp.h"
#define LOG_LEVEL 3
#include "utils/logs.h"

#define BUFSIZE 1024

// function prototypes
int tcpopen(const char *host, int port);
void sendbuftosck(int sckfd, const char *buf, int len);
int sendcantosck(int sckfd, struct can_frame* frame);

#define KQUEUE_CH_SIZE 	3
#define KQUEUE_EV_SIZE	3
#define TIMER_FD  1234

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
  int period_ms = 10;

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
