#include "Device.h"
#include "Library.h"
#include <algorithm>
#include <functional>
#include <iomanip>
#include <iterator>
#include <libusb.h>
#include <memory>
#include <stdexcept>

using namespace std;

Device::Device(const Library &lib, libusb_device *device) : library(lib) {
  int r = libusb_get_device_descriptor(device, &descriptor);
  if (r < 0)
    throw runtime_error("libusb_get_device_descriptor failed");
}

Device::Device(const Device &dev)
    : library(dev.library), descriptor(dev.descriptor) {}

Device::~Device() {}

ostream &operator<<(ostream &out, const Device &dev) {
  out << hex << setw(4) << setfill('0') << dev.descriptor.idVendor;
  out << ":";
  out << hex << setw(4) << setfill('0') << dev.descriptor.idProduct;
  return out;
}

uint16_t Device::getVid() const { return descriptor.idVendor; }
uint16_t Device::getPid() const { return descriptor.idProduct; }
const Library &Device::getLibrary() const { return library; }

void Device::switchToAOA() {
  auto deviceDeleter = [](libusb_device_handle *device) {
    libusb_close(device);
  };
  unique_ptr<libusb_device_handle, decltype(deviceDeleter)> device(
      libusb_open_device_with_vid_pid(library.getHandle(),
                                      (uint16_t)descriptor.idVendor,
                                      (uint16_t)descriptor.idProduct),
      deviceDeleter);
  unsigned char stringDescriptor[256];
  auto length = (int)libusb_get_string_descriptor(
      device.get(), descriptor.iManufacturer, 0x0409, stringDescriptor,
      sizeof(stringDescriptor));
  // cout << dec << length << endl;
  // for (int i = 0; i < length; i++) {
  // cout << stringDescriptor[i] << " ";
  //}
  uint8_t version[2];
  if (libusb_control_transfer(device.get(), 0xc0, 51, 0, 0, version, 2, 1000) !=
      2)
    throw runtime_error("libusb_control_transfer failed: not AOA device? (51)");
  vector<string> strings = {"Android", "Android Auto",      "Android Auto",
                            "2.0.1",   "http://myurl.com/", "TAGS Serial 01"};
  int i = 0;
  for (auto str : strings) {
    unique_ptr<unsigned char[]> buffer(new unsigned char[str.length() + 1]);
    copy(str.begin(), str.end(), buffer.get());
    buffer[str.length()] = '\0';
    if (libusb_control_transfer(device.get(), 0x40, 52, 0, i++, buffer.get(),
                                str.length() + 1, 1000) != str.length() + 1)
      throw runtime_error(
          "libusb_control_transfer failed: not AOA device? (52)");
  }
  if (libusb_control_transfer(device.get(), 0x40, 53, 0, 0, nullptr, 0, 1000) !=
      0)
    throw runtime_error("libusb_control_transfer failed: not AOA device? (53)");
}
