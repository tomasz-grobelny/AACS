// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "Udc.h"
#include "Library.h"
#include <stdexcept>

using namespace std;

Udc::Udc(usbg_udc *udc) : udc(udc) {}
Udc Udc::getUdcById(const Library &lib, int id) {
  if (id != 0)
    throw runtime_error("Only id==0 supported");
  auto udc = usbg_get_first_udc(lib.getState());
  return udc;
}
usbg_udc *Udc::getUdc() const { return udc; }
