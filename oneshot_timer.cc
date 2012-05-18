#include "oneshot_timer.h"
#include <unistd.h>
#include <time.h>
#include <sys/timerfd.h>

namespace recpt1 {

oneshot_timer::oneshot_timer(int sec)
  : fd_(-1)
{
  struct timespec now;
  if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
    throw std::runtime_error("clock_gettime failed");
  }
  struct itimerspec next = {
    {0, 0},
    {now.tv_sec + sec, now.tv_nsec},
  };
  int tfd = timerfd_create(CLOCK_MONOTONIC, 0);
  if (tfd == -1) {
    throw std::runtime_error("timerfd_create failed");
  }
  if (timerfd_settime(tfd, TFD_TIMER_ABSTIME, &next, NULL) == -1) {
    throw std::runtime_error("timerfd_settime failed");
  }

  fd_ = tfd;
}

oneshot_timer::~oneshot_timer()
{
  if (fd_ != -1) {
    close(fd_);
  }
}

bool oneshot_timer::extend(int sec)
{
  if (fd_ == -1) {
    return false;
  }
  struct itimerspec spec;
  if (timerfd_gettime(fd_, &spec) == -1) {
    return false;
  }
  spec.it_value.tv_sec += sec;
  return timerfd_settime(fd_, 0, &spec, NULL) == 0;
}
};
