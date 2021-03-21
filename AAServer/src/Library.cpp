// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "Library.h"
#include "utils.h"
#include <ServerUtils.h>

Library::Library(std::string basePath) {
  checkUsbgError(usbg_init(basePath.c_str(), &state));
}
Library::~Library() { usbg_cleanup(state); }
usbg_state *Library::getState() const { return state; }
