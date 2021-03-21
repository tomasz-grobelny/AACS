// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "MassStorageFunction.h"
#include "Gadget.h"
#include "utils.h"
#include <ServerUtils.h>
#include <usbg/function/ms.h>

using namespace std;

MassStorageFunction::MassStorageFunction(const Gadget &gadget,
                                         const string &function_name,
                                         const string &lun) {
  usbg_function *function;
  struct usbg_f_ms_lun_attrs f_ms_luns_array[] = {{
      .id = -1, /* allows to place in any position */
      .cdrom = 1,
      .ro = 0,
      .nofua = 0,
      .removable = 1,
      .file = lun.c_str(),
  }};

  struct usbg_f_ms_lun_attrs *f_ms_luns[] = {
      &f_ms_luns_array[0],
      NULL,
  };

  struct usbg_f_ms_attrs f_attrs = {
      .stall = 0,
      .nluns = 1,
      .luns = f_ms_luns,
  };

  checkUsbgError(usbg_create_function(
      gadget.getGadget(), usbg_function_type::USBG_F_MASS_STORAGE,
      function_name.c_str(), &f_attrs, &function));
  this->function = function;
}
