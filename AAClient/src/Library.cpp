// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "Library.h"
#include <libusb.h>
#include <stdexcept>

using namespace std;

Library::Library() {
  if (libusb_init(&context) < 0)
    throw runtime_error("libusb_init failed");
}

Library::~Library() { libusb_exit(context); }

vector<Device> Library::getDeviceList() const {
  libusb_device **devs;
  if (libusb_get_device_list(NULL, &devs) < 0)
    throw runtime_error("libusb_get_device_list failed");
  vector<Device> result;
  libusb_device *dev;
  int i = 0;
  try {
    while ((dev = devs[i++]) != NULL) {
      result.push_back(Device(*this, dev));
    }
    libusb_free_device_list(devs, 1);
  } catch (...) {
    libusb_free_device_list(devs, 1);
    throw;
  }
  return result;
}

libusb_context *Library::getHandle() const { return context; }
