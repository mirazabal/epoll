/*
 * Naive single threaded SCTP server using epoll. Remember linking with -lsctp
 */

#define _GNU_SOURCE

#include <assert.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h> 

static
const int SERVER_LISTEN_QUEUE_SIZE = 32;

int init_sctp_conn_server(const char* addr, int port)
{
  struct sockaddr_in  server4_addr;
  memset(&server4_addr, 0, sizeof(struct sockaddr_in));
  if(inet_pton(AF_INET, addr, &server4_addr.sin_addr) != 1){
    // Error occurred
    struct sockaddr_in6 server6_addr;
    memset(&server6_addr, 0, sizeof(struct sockaddr_in6));
    if(inet_pton(AF_INET6, addr, &server6_addr.sin6_addr) == 1){
      assert(0!=0 && "IPv6 not supported");
    }
    assert(0!=0 && "Incorrect IP address string.");
  }

  server4_addr.sin_family = AF_INET;
  server4_addr.sin_port = htons(port);
  const int server_fd = socket (AF_INET, SOCK_SEQPACKET, IPPROTO_SCTP);
  assert(server_fd != -1);

  const size_t addr_len = sizeof(server4_addr);
  struct sockaddr* server_addr = (struct sockaddr*)&server4_addr;
  int rc = bind(server_fd, server_addr, addr_len);
  assert(rc != -1);

  struct sctp_event_subscribe evnts;
  bzero (&evnts, sizeof (evnts)) ;
  evnts.sctp_data_io_event = 1;
  rc = setsockopt (server_fd, IPPROTO_SCTP, SCTP_EVENTS, &evnts, sizeof (evnts));
  assert(rc != -1);

  rc = listen(server_fd, SERVER_LISTEN_QUEUE_SIZE);
  assert(rc != -1);
  return server_fd;
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

int main()
{
  const char* addr = "127.0.0.1";
  const int port = 36421; 
  int sfd = init_sctp_conn_server(addr, port);
  set_fd_non_blocking(sfd);

  int efd = init_epoll();
  add_fd_epoll(efd, sfd);

  const int maxevents = 1;
  struct epoll_event events[maxevents];
  const int timeout_ms = 10000;

  uint8_t buffer[2048];
  struct sockaddr_in from; 
  struct sctp_sndrcvinfo sri;

  for(;;){
    const int events_ready = epoll_wait(efd, events, maxevents, timeout_ms); 
    if(events_ready == 0) {
      printf("Timeout without client data, closing \n");
      break;
    }
    assert(events_ready != -1);
    for(int i = 0; i < events_ready; ++i){
      assert((events[i].events & EPOLLERR) == 0);
      const int cur_fd = events[i].data.fd; 
      assert(cur_fd == sfd)
      memset(buffer, 0, 2048);
      memset(&from, 0, sizeof(struct sockaddr_in));
      memset(&sri, 0, sizeof(struct sctp_sndrcvinfo));
      int msg_flags = 0;
      socklen_t len = sizeof(from);
      int rc = sctp_recvmsg(sfd, buffer, 2048, (struct sockaddr*)&from, &len, &sri, &msg_flags); 
      assert(rc > -1);
      printf("Received data: %s from client and number of bytes = %d\n", (char*)buffer, rc); 
      fflush(stdout);
      rc = sctp_sendmsg(sfd, (void*)buffer, len,(struct sockaddr *)&from, sizeof(struct sockaddr), sri.sinfo_ppid, sri.sinfo_flags, sri.sinfo_stream, 0, 0) ;
      assert(rc != 0);
      printf("Server sending data back %d\n", len);
    }
  } 
  return 0;
}
