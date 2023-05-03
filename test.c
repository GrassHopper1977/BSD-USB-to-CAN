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

#include "usb2can.h"

#ifdef __CHERI_PURE_CAPABILITY__
#define PRINTF_PTR "#p"
#else
#define PRINTF_PTR "p"
#endif

#define BUFSIZE 1024

// function prototypes
void diep(const char *s);
int tcpopen(const char *host, int port);
void sendbuftosck(int sckfd, const char *buf, int len);
int sendcantosck(int sckfd, struct can_frame* frame);

#define KQUEUE_CH_SIZE 	3
#define KQUEUE_EV_SIZE	3
#define TIMER_FD  1234

int main(int argc, char *argv[])
{
  struct kevent chlist[KQUEUE_CH_SIZE]; // events we want to monitor
  struct kevent evlist[KQUEUE_EV_SIZE]; // events that were triggered
  char buf[BUFSIZE]; 
  int sckfd, kq, nev, i;
  struct can_frame frame;	// The incoming frame
  int period_ms = 100;

  printf("starting...\n");

  // check argument count
  if (argc != 3) { 
    fprintf(stderr, "USB2CAN Test app\n\n");
    fprintf(stderr, "usage: %s host port\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  // open a connection to a host:port pair
  sckfd = tcpopen(argv[1], atoi(argv[2]));

  // create a new kernel event queue
  if ((kq = kqueue()) == -1)
    diep("kqueue()");

  // initialise kevent structures
  EV_SET(&chlist[0], sckfd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
  EV_SET(&chlist[1], fileno(stdin), EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
  EV_SET(&chlist[2], TIMER_FD, EVFILT_TIMER, EV_ADD | EV_ENABLE, 0, period_ms, 0);
  nev = kevent(kq, chlist, KQUEUE_CH_SIZE, NULL, 0, NULL);
  if (nev < 0)
    diep("kevent() 1");

  printf("Starting loop...\n");
  // loop forever
  for (;;)
  {
    nev = kevent(kq, NULL, 0, evlist, KQUEUE_EV_SIZE, NULL);

    if (nev < 0)
      diep("kevent() 2");
    else if (nev > 0)
    {
      for (i = 0; i < nev; i++) {
        if (evlist[i].flags & EV_EOF) /* read direction of socket has shutdown */
          exit(EXIT_FAILURE);

        if (evlist[i].flags & EV_ERROR) {                /* report errors */
          fprintf(stderr, "EV_ERROR: %s\n", strerror(evlist[i].data));
          exit(EXIT_FAILURE);
        }
  
        if(evlist[i].ident == TIMER_FD) {
          printf("Timeout.\n");
          frame.can_id = 0x81234567U;
          frame.len = 4;
          frame.data[0] = 0x01;
          frame.data[1] = 0x02;
          frame.data[2] = 0x03;
          frame.data[3] = 0x04;
          sendcantosck(sckfd, &frame);
        } else if (evlist[i].ident == sckfd) {                  /* we have data from the host */
          //printf("We have recieved data from the host!\n");
          memset(buf, 0, BUFSIZE);
          //int i = read(sckfd, buf, BUFSIZE);
          int i = recv(sckfd, &frame, sizeof(frame), 0);
          if (i < 0)
            diep("read()");
          printf("ID: ");
          if(frame.can_id & CAN_ERR_FLAG) {
            printf("%08x ERR", frame.can_id);
          } else if(frame.can_id & CAN_EFF_FLAG) {
            printf("%08x", frame.can_id & CAN_EFF_MASK);
          } else {
          	printf("     %03x", frame.can_id & CAN_SFF_MASK);
          }
          printf(" len: %u ", frame.len);
          printf(" data: ");
          for(int n = 0; n < frame.len; n++) {
          	printf("%02x ", frame.data[n]);
          }
          printf("\n");
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
