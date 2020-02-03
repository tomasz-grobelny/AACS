// Distributed under GPLv3 only as specified in repository's root LICENSE file

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
  __u8 channel;
  __u8 flags;
  std::vector<__u8> content;
  int offset;
};
