// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "Configuration.h"
#include "Function.h"
#include "Gadget.h"
#include "utils.h"
#include <ServerUtils.h>

using namespace std;

Configuration::Configuration(const Gadget &gadget, const string &name) {
  usbg_config_attrs c_attrs;
  c_attrs.bmAttributes = 0xc0;
  c_attrs.bMaxPower = 0x30;
  usbg_config_strs c_strs;
  char cfg[200];
  snprintf(cfg, 200, "%s", name.c_str());
  c_strs.configuration = cfg;
  usbg_config *config;
  checkUsbgError(usbg_create_config(gadget.getGadget(), 1, name.c_str(),
                                    &c_attrs, &c_strs, &config));
  this->config = config;
}
void Configuration::addFunction(const Function &fun, const string &name) {
  checkUsbgError(
      usbg_add_config_function(config, name.c_str(), fun.getFunction()));
}
