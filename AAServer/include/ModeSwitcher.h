// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include <sys/types.h>
#pragma once

class Library;

class ModeSwitcher {
  static ssize_t handleSwitchMessage(int fd, const void *buf, size_t nbytes);

public:
  static void handleSwitchToAccessoryMode(const Library &lib);
};
