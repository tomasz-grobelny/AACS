#pragma once

#include <vector>
#include <cstdint>

class Message {
public:
  Message();
  uint8_t channel;
  uint8_t flags;
  std::vector<uint8_t> content;
  int offset;
};
