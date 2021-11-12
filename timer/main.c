/*
 * Naive timer example using epoll
 */

#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <stdio.h>

#include <assert.h>

int main()
{
  // Create the timer
  const int clockid = CLOCK_MONOTONIC;
  const int flags = TFD_NONBLOCK | TFD_CLOEXEC;
  const int tfd = timerfd_create(clockid, flags);
  assert(tfd != -1);

  const int flags_2 = 0;
  struct itimerspec *old_value = NULL; // not interested in how the timer was previously configured
  const struct timespec it_interval = {.tv_sec = 1, .tv_nsec = 100000};  /* Interval for periodic timer */
  const struct timespec it_value = {.tv_sec = 0, .tv_nsec = 100000};     /* Initial expiration */
  const struct itimerspec new_value = {.it_interval = it_interval, .it_value = it_value}; 
  int rc = timerfd_settime(tfd, flags_2, &new_value, old_value);
  assert(rc != -1);

  // Create epoll
  const int flags_3 = EPOLL_CLOEXEC; 
  const int efd = epoll_create1(flags_3);  
  assert(efd != -1);

  const int op = EPOLL_CTL_ADD;

  const epoll_data_t e_data = {.fd = tfd};
  const int e_events = EPOLLIN ; // open for reading
  struct epoll_event event = {.events = e_events, .data = e_data};
  rc = epoll_ctl(efd, op, tfd, &event);
  assert(rc != -1);

  const int maxevents = 32;
  struct epoll_event events[maxevents];
  const int timeout_ms = 1000;
  int n = 5;

  while(n != 0){
    const int events_ready = epoll_wait(efd, events, maxevents, timeout_ms); 
    assert(events_ready != -1);
    for(int i =0; i < events_ready; ++i){
      assert((events[i].events & EPOLLERR) == 0);
      const int cur_fd = events[i].data.fd; 
      if (cur_fd == tfd){
        uint64_t res;
        ssize_t r = read(tfd, &res, sizeof(uint64_t));
        assert(r != 0);
        printf("Timer expired %lu times!\n", res);
      }
    }
    --n;
  }
  rc = close(efd);
  assert(rc != -1);

	rc = close(tfd); 
  assert(rc != -1);

  return 0;
}
