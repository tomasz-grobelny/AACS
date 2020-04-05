// Distributed under GPLv3 only as specified in repository's root LICENSE file

#pragma once

#include <iostream>
#include <libusb.h>
#include <vector>

class Library;

class Device {
  libusb_device_descriptor descriptor;
  const Library &library;

public:
  Device(const Library &lib, libusb_device *device);
  Device(const Device &dev);
  Device();
  ~Device();
  const Library &getLibrary() const;
  uint16_t getVid() const;
  uint16_t getPid() const;
  void switchToAOA();
  friend std::ostream &operator<<(std::ostream &out, const Device &dev);
};
