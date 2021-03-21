// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "ServerUtils.h"
#include <stdexcept>
#include <string>
#include <usbg/usbg.h>

using namespace std;

void checkUsbgError(int returnValue) {
  if (returnValue != USBG_SUCCESS) {
    throw runtime_error(string("Libusbg error: ") +
                        usbg_strerror((usbg_error)returnValue));
  }
}
