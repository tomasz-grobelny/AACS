// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include <usbg/usbg.h>
#include <string>
#pragma once

class Library;
class Udc;

class Gadget {
  usbg_gadget *gadget;

public:
  Gadget(const Library &lib, int vid, int pid, const std::string &name);
  ~Gadget();
  void setStrings(const std::string &manufacturer, const std::string &product,
                  const std::string &serialNumber);
  usbg_gadget *getGadget() const;
  void enable(const Udc &udc);
};
