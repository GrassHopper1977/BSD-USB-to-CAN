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
#include <limits.h>


#include "usb2can.h"
#include "utils/timestamp.h"
#define LOG_LEVEL 3
#include "utils/logs.h"
#include <sys/rtprio.h>

#define BUFSIZE 1024

// function prototypes
int tcpopen(const char *host, int port);
void sendbuftosck(int fd, const char *buf, int len);
int sendcantosck(int fd, struct can_frame* frame);

#define KQUEUE_CH_SIZE 	2
#define KQUEUE_EV_SIZE	10
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

#define SYNC_PERIOD_NS  8000000L
#define SYNC_PERIOD_NS_1PC (uint64_t)(SYNC_PERIOD_NS * 0.01)

void checkTimer(uint64_t* timer, int wfdfifo) {
  struct can_frame frame;	// The incoming frame
  uint64_t now = nanos();
  static uint16_t count = 0;
  
  if(now >= (*timer - SYNC_PERIOD_NS_1PC)) {
    *timer = now + SYNC_PERIOD_NS;  // Next period from now.
    // *timer += SYNC_PERIOD_NS; // Next period from when we should've been.

    // MilCAN Sync Frame
    frame.can_id = 0x0200802A | CAN_EFF_FLAG;
    frame.len = 2;
    frame.data[0] = (uint8_t)(count & 0x000000FF);
    frame.data[1] = (uint8_t)((count >> 8) & 0x00000003);
    frame.data[2] = 0x00;
    frame.data[3] = 0x00;
    frame.data[4] = 0x00;
    frame.data[5] = 0x00;
    frame.data[6] = 0x00;
    frame.data[7] = 0x00;

    count++;
    count &= 0x000003FF;
    sendcantosck(wfdfifo, &frame);
  }
}

void displayRTpriority() {
  // Get the current real time priority
  struct rtprio rtdata;
  LOGI(__FUNCTION__, "INFO", "Getting Real Time Priority settings.\n");
  int ret = rtprio(RTP_LOOKUP, 0, &rtdata);
  if(ret < 0) {
    switch(errno) {
      default:
        LOGE(__FUNCTION__, "ERROR", "rtprio returned unknown error (%i)\n", errno);
        break;
      case EFAULT:
        LOGE(__FUNCTION__, "EFAULT", "Pointer to struct rtprio is invalid.\n");
        break;
      case EINVAL:
        LOGE(__FUNCTION__, "EINVAL", "The specified priocess was out of range.\n");
        break;
      case EPERM:
        LOGE(__FUNCTION__, "EPERM", "The calling thread is not allowed to set the priority. Try running as SU or root.\n");
        break;
      case ESRCH:
        LOGE(__FUNCTION__, "ESRCH", "The specified process or thread could not be found.\n");
        break;
    }
  } else {
    switch(rtdata.type) {
      case RTP_PRIO_REALTIME:
        LOGI(__FUNCTION__, "INFO", "Real Time Priority type is: RTP_PRIO_REALTIME\n");
        break;
      case RTP_PRIO_NORMAL:
        LOGI(__FUNCTION__, "INFO", "Real Time Priority type is: RTP_PRIO_NORMAL\n");
        break;
      case RTP_PRIO_IDLE:
        LOGI(__FUNCTION__, "INFO", "Real Time Priority type is: RTP_PRIO_IDLE\n");
        break;
      default:
        LOGI(__FUNCTION__, "INFO", "Real Time Priority type is: %u\n", rtdata.type);
        break;
    }
    LOGI(__FUNCTION__, "INFO", "Real Time Priority priority is: %u\n", rtdata.prio);
  }
}

int setRTpriority(u_short prio) {
  struct rtprio rtdata;

  // Set the real time priority here
  LOGI(__FUNCTION__, "INFO", "Setting the Real Time Priority type to RTP_PRIO_REALTIME and priority to %u\n", prio);
  rtdata.type = RTP_PRIO_REALTIME;  // Real Time priority
  // rtdata.type = RTP_PRIO_NORMAL;  // Normal
  // rtdata.type = RTP_PRIO_IDLE;  // Low priority
  rtdata.prio = prio;  // 0 = highest priority, 31 = lowest.
  int ret = rtprio(RTP_SET, 0, &rtdata);
  if(ret < 0) {
    switch(errno) {
      default:
        LOGE(__FUNCTION__, "ERROR", "rtprio returned unknown error (%i)\n", errno);
        break;
      case EFAULT:
        LOGE(__FUNCTION__, "EFAULT", "Pointer to struct rtprio is invalid.\n");
        break;
      case EINVAL:
        LOGE(__FUNCTION__, "EINVAL", "The specified priocess was out of range.\n");
        break;
      case EPERM:
        LOGE(__FUNCTION__, "EPERM", "The calling thread is not allowed to set the priority. Try running as SU or root.\n");
        break;
      case ESRCH:
        LOGE(__FUNCTION__, "ESRCH", "The specified process or thread could not be found.\n");
        break;
    }
  } else {
    switch(rtdata.type) {
      case RTP_PRIO_REALTIME:
        LOGI(__FUNCTION__, "INFO", "Real Time Priority type is: RTP_PRIO_REALTIME\n");
        break;
      case RTP_PRIO_NORMAL:
        LOGI(__FUNCTION__, "INFO", "Real Time Priority type is: RTP_PRIO_NORMAL\n");
        break;
      case RTP_PRIO_IDLE:
        LOGI(__FUNCTION__, "INFO", "Real Time Priority type is: RTP_PRIO_IDLE\n");
        break;
      default:
        LOGI(__FUNCTION__, "INFO", "Real Time Priority type is: %u\n", rtdata.type);
        break;
    }
    LOGI(__FUNCTION__, "INFO", "Real Time Priority priority is: %u\n", rtdata.prio);
  }
  displayRTpriority();
  return ret;
}

int main(int argc, char *argv[])
{

  // Create the signal handler here - ensures that Ctrl-C gets passed back up to 
  signal(SIGINT, sigint_handler);

  struct kevent chlist[KQUEUE_CH_SIZE]; // events we want to monitor
  struct kevent evlist[KQUEUE_EV_SIZE]; // events that were triggered
  int kq, nev, i;
  struct can_frame frame;	// The incoming frame
  int period_ms = 8;
  char rfifopath[PATH_MAX + 1] = {0x00};  // We will open the read FIFO here.
  char wfifopath[PATH_MAX + 1] = {0x00};  // We will open the write FIFO here.
  int rfdfifo = -1;
  int wfdfifo = -1;
  uint64_t timer;

  sprintf(rfifopath,"%sr", argv[1]);
  sprintf(wfifopath,"%sw", argv[1]);

  displayRTpriority();
  setRTpriority(0);

  LOGI(__FUNCTION__, "INFO", "Starting...\n");

  // check argument count
  LOGI(__FUNCTION__, "INFO", "argc: %i\n", argc);
  if (argc != 2) { 
    fprintf(stderr, "USB2CAN Test app\n\n");
    fprintf(stderr, "usage: %s pipe (e.g. /tmp/can0.0 Note: Omit the \"r\" or \"w\" on the end of the filename)\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  // Open the FIFOs
  LOGI(__FUNCTION__, "INFO", "Opening FIFO (%s)...\n", rfifopath);
  rfdfifo = open(rfifopath, O_RDONLY | O_NONBLOCK);
  if(rfdfifo < 0) {
    LOGE(__FUNCTION__, "INFO", "ERROR: unable to open FIFO (%s) for Read. Error \n", rfifopath);
    exit(EXIT_FAILURE);
  }

  LOGI(__FUNCTION__, "INFO", "Opening FIFO (%s)...\n", wfifopath);
  wfdfifo = open(wfifopath, O_WRONLY | O_NONBLOCK);
  if(wfdfifo < 0) {
    LOGE(__FUNCTION__, "INFO", "ERROR: unable to open FIFO (%s) for Write. Error \n", wfifopath);
    exit(EXIT_FAILURE);
  }

  // create a new kernel event queue
  if ((kq = kqueue()) == -1) {
    LOGE(__FUNCTION__, "INFO", "Unable to create kqueue\n");
    exit(EXIT_FAILURE);
  }

  // initialise kevent structures
  EV_SET(&chlist[0], rfdfifo, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
  EV_SET(&chlist[1], TIMER_FD, EVFILT_TIMER, EV_ADD | EV_ENABLE, 0, period_ms, 0);
  nev = kevent(kq, chlist, KQUEUE_CH_SIZE, NULL, 0, NULL);
  if (nev < 0) {
    LOGE(__FUNCTION__, "INFO", "Unable to listen to kqueue\n");
    exit(EXIT_FAILURE);
  }

  printf("Starting loop...\n");
  // uint32_t count = 0;
  timer = nanos() + SYNC_PERIOD_NS;  // 8ms
  struct timespec zero_ts = {
    .tv_sec = 0,
    .tv_nsec = 0
  };

  // loop forever
  for (;;)
  {
    // nev = kevent(kq, NULL, 0, evlist, KQUEUE_EV_SIZE, NULL);  // Blocking
    checkTimer(&timer, wfdfifo);
    nev = kevent(kq, NULL, 0, evlist, KQUEUE_EV_SIZE, &zero_ts);  // Non-blocking

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
          // frame.can_id = 0x01U;
          // frame.len = 8;
          // frame.data[0] = (uint8_t)((count >> 24) & 0x000000FF);
          // frame.data[1] = (uint8_t)((count >> 16) & 0x000000FF);
          // frame.data[2] = (uint8_t)((count >> 8) & 0x000000FF);
          // frame.data[3] = (uint8_t)(count & 0x000000FF);
          // frame.data[4] = 0x01;
          // frame.data[5] = 0x23;
          // frame.data[6] = 0x45;
          // frame.data[7] = 0x67;
          // count++;
          // sendcantosck(wfdfifo, &frame);
        } else if (evlist[i].ident == rfdfifo) {                  /* we have data from the host */
          int ret;
          int i = 0;
          int toread = (int)(evlist[i].data);
          do {
            i++;
            ret = read(rfdfifo, &frame, sizeof(struct can_frame));
            toread -= ret;
            if(ret != sizeof(struct can_frame)) {
              LOGE(__FUNCTION__, "INFO", "Read %u bytes, expected %lu bytes!\n", ret, sizeof(struct can_frame));
            } else {
              print_can_frame("PIPE", "IN", &frame, 0, "");
            }
          } while (toread >= sizeof(struct can_frame));

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

int sendcantosck(int fd, struct can_frame* frame) {
  print_can_frame("PIPE", "OUT", frame, 0, "");
  return write(fd, frame, sizeof(struct can_frame));
}
