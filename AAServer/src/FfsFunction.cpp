// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "FfsFunction.h"
#include "Gadget.h"
#include "utils.h"
#include <ServerUtils.h>
#include <iostream>
#include <stdexcept>
#include <sys/mount.h>

using namespace std;

FfsFunction::FfsFunction(const Gadget &gadget, const string &function_name,
                         const string &mountPoint) {
  usbg_function *function;
  checkUsbgError(usbg_create_function(gadget.getGadget(),
                                      usbg_function_type::USBG_F_FFS,
                                      function_name.c_str(), NULL, &function));
  this->function = function;
  auto ret = mount(function_name.c_str(), mountPoint.c_str(), "functionfs", 0,
                   nullptr);
  if (ret != 0)
    throw runtime_error("Error mounting function");
  this->mountPoint = mountPoint;
}
FfsFunction::~FfsFunction() {
  auto ret = umount(mountPoint.c_str());
  if (ret != 0)
    cout << "umount failed: " << ret << " " << errno << endl;
}
