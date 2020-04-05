// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "Function.h"

Function::Function() : function(nullptr) {}
Function::~Function() {}
usbg_function *Function::getFunction() const { return function; }
