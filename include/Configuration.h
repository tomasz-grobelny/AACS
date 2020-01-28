// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include <usbg/usbg.h>
#include <string>

class Gadget;
class Function;

class Configuration {
  usbg_config *config;

public:
  Configuration(const Gadget &gadget, const std::string &name);
  void addFunction(const Function &fun, const std::string &name);
};
