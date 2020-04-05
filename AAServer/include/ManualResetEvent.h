// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include <condition_variable>
#include <mutex>
#pragma once

class ManualResetEvent {
public:
  ManualResetEvent();
  void set();
  void wait();

private:
  std::mutex m;
  std::condition_variable cv;
  bool signaled;
};
