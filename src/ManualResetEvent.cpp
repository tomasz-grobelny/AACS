// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "ManualResetEvent.h"

ManualResetEvent::ManualResetEvent() : signaled(false) {}

void ManualResetEvent::set() {
  {
    std::unique_lock<std::mutex> lk(m);
    signaled = true;
  }
  cv.notify_all();
}

void ManualResetEvent::wait() {
  std::unique_lock<std::mutex> lk(m);
  cv.wait(lk, [this]() { return signaled; });
}
