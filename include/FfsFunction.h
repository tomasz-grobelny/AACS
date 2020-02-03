// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include <string>
#include "Function.h"
#pragma once

class Gadget;

class FfsFunction : public Function {
  std::string mountPoint;

public:
  FfsFunction(const Gadget &gadget, const std::string &function_name,
              const std::string &mountPoint);
  ~FfsFunction();
};
