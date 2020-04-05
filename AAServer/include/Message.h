// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include <cstdint>
#include <sys/stat.h>
#include <vector>
#pragma once

enum EncryptionType {
  Plain = 0,
  Encrypted = 1 << 3,
};

enum FrameType {
  First = 1,
  Last = 2,
  Bulk = First | Last,
};

enum MessageTypeFlags {
  Control = 0,
  Specific = 1 << 2,
};

class Message {
public:
  Message();
  uint8_t channel;
  uint8_t flags;
  std::vector<uint8_t> content;
  int offset;
};
