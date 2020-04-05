// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include <usbg/usbg.h>
#pragma once

class Function {
protected:
  usbg_function *function;

public:
  Function();
  virtual ~Function();
  usbg_function *getFunction() const;
};
