// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "Gadget.h"
#include "Library.h"
#include "Udc.h"
#include "utils.h"
#include <ServerUtils.h>
#include <iostream>

using namespace std;

Gadget::Gadget(const Library &lib, int vid, int pid, const string &name) {
  checkUsbgError(usbg_create_gadget_vid_pid(lib.getState(), name.c_str(), vid,
                                            pid, &gadget));
}

Gadget::~Gadget() {
  auto returnValue = usbg_rm_gadget(gadget, USBG_RM_RECURSE);
  if (returnValue != USBG_SUCCESS)
    cout << usbg_strerror((usbg_error)returnValue) << endl;
}

void Gadget::setStrings(const string &manufacturer, const string &product,
                        const string &serialNumber) {
  checkUsbgError(usbg_set_gadget_str(gadget,
                                     usbg_gadget_str::USBG_STR_MANUFACTURER,
                                     0x409, manufacturer.c_str()));
  checkUsbgError(usbg_set_gadget_str(gadget, usbg_gadget_str::USBG_STR_PRODUCT,
                                     0x409, product.c_str()));
  checkUsbgError(usbg_set_gadget_str(gadget,
                                     usbg_gadget_str::USBG_STR_SERIAL_NUMBER,
                                     0x409, serialNumber.c_str()));
}

usbg_gadget *Gadget::getGadget() const { return gadget; }

void Gadget::enable(const Udc &udc) {
  checkUsbgError(usbg_enable_gadget(gadget, udc.getUdc()));
}
