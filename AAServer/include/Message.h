// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "enums.h"
#include <cstdint>
#include <sys/stat.h>
#include <vector>
#pragma once

class Message {
public:
  Message();
  uint8_t channel;
  uint8_t flags;
  std::vector<uint8_t> content;
  int offset;
};
