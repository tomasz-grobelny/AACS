// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include <usbg/usbg.h>

class Library;

class Udc {
  usbg_udc *udc;

public:
  Udc(usbg_udc *udc);
  static Udc getUdcById(const Library &lib, int id);
  usbg_udc *getUdc() const;
};
