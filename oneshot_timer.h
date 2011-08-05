#ifndef ONESHOT_TIMER_H
#define ONESHOT_TIMER_H
#include <stdexcept>

namespace recpt1 {

class oneshot_timer
{
  int fd_;
public:
  oneshot_timer(int sec);
  ~oneshot_timer();
  inline int fd() const { return fd_; }
};

};
#endif /* end of include guard */
