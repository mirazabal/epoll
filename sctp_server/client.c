/*
 * Naive SCTP client. Remember linking with -lsctp
 */

#define _GNU_SOURCE
#include <sys/socket.h>

#include <arpa/inet.h>
#include <assert.h>

#include <errno.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h> 

#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>

int init_sctp_conn_client(const char* addr, int port, struct sockaddr_in* servaddr)
{
  int sock_fd = socket(AF_INET, SOCK_SEQPACKET, IPPROTO_SCTP);
  assert(sock_fd != -1);
  bzero(servaddr, sizeof (struct sockaddr_in) ) ;
  servaddr->sin_family = AF_INET;
  servaddr->sin_addr.s_addr = htonl (INADDR_ANY);
  servaddr->sin_port = htons (port);
  int rc = inet_pton(AF_INET, addr, &servaddr->sin_addr);
  assert(rc == 1);

  struct sctp_event_subscribe evnts; 
  bzero(&evnts, sizeof (evnts)) ;
  evnts.sctp_data_io_event = 1 ;
  setsockopt(sock_fd, IPPROTO_SCTP, SCTP_EVENTS, &evnts, sizeof (evnts));

  return sock_fd;
}

void set_fd_non_blocking(int sfd)
{
  int flags = fcntl (sfd, F_GETFL, 0);  
  flags |= O_NONBLOCK;
  fcntl (sfd, F_SETFL, flags);
}

void add_fd_epoll(int efd, int fd)
{
  const int op = EPOLL_CTL_ADD;
  const epoll_data_t e_data = {.fd = fd};
  const int e_events = EPOLLIN; // open for reading
  struct epoll_event event = {.events = e_events, .data = e_data};
  int rc = epoll_ctl(efd, op, fd, &event);
  assert(rc != -1);
}

int init_epoll()
{
  const int flags = EPOLL_CLOEXEC; 
  const int efd = epoll_create1(flags);  
  assert(efd != -1);
  return efd;
}


int counter = 0;

int main()
{
  const char* addr = "127.0.0.1";
  const int port = 36421;
  struct sockaddr_in servaddr;
  const int fd = init_sctp_conn_client(addr, port, &servaddr);
  set_fd_non_blocking(fd);
  int efd = init_epoll();
  add_fd_epoll(efd, fd);

  const int maxevents = 1;
  struct epoll_event events[maxevents];
  const int timeout_ms = 10000;

  uint8_t buffer[2048];
  const char* str_client = "String from client";
  const int bytes = strlen(str_client);
  for(;;){
    sleep(counter);
    counter += 2;
    memset(buffer, 0, 2048);
    memcpy(buffer, &str_client, bytes);
    const int ret = sctp_sendmsg(fd, (void *)buffer, bytes, (struct sockaddr *)&servaddr, sizeof(struct sockaddr), 0, 0, 0, 0, 0 );
    assert(ret != 0);
    printf("Client data send through sctp = %d\n", bytes);
    const int events_ready = epoll_wait(efd, events, maxevents, timeout_ms); 
    if(events_ready == 0) {
      printf("Timeout without server responding, closing \n");
      break;
    }
    for(int i = 0; i < events_ready ; ++i){
    assert((events[i].events & EPOLLERR) == 0);
    const int cur_fd = events[i].data.fd; 
    if(cur_fd == fd){
      memset(buffer, 0, 2048);
      struct sockaddr_in from; 
      memset(&from, 0, sizeof(struct sockaddr_in));
      struct sctp_sndrcvinfo sri;
      memset(&sri, 0, sizeof(struct sctp_sndrcvinfo));
      int msg_flags = 0;
      socklen_t len = sizeof(from);
      const int rc = sctp_recvmsg(fd, buffer, 2048, (struct sockaddr*)&from, &len, &sri, &msg_flags); 
      assert(rc > -1);
      printf("Received data: %s from server and number of bytes = %d\n", (char*)buffer, rc); 
      fflush(stdout);
    }
    }
  }
    return 0;
}
