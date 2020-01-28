// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "Function.h"
#include <string>

class Gadget;

class MassStorageFunction : public Function {
public:
  MassStorageFunction(const Gadget &gadget, const std::string &function_name,
                      const std::string &lun);
};
