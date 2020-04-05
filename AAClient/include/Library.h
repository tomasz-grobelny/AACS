// Distributed under GPLv3 only as specified in repository's root LICENSE file

#pragma once

#include "Device.h"
#include <libusb.h>
#include <vector>

class Library {
  libusb_context *context;

public:
  Library();
  ~Library();
  std::vector<Device> getDeviceList() const;
  libusb_context *getHandle() const;
};
