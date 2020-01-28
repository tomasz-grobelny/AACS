// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include <string>
#include <usbg/usbg.h>

class Library {
  usbg_state *state = NULL;

public:
  Library(std::string basePath);
  ~Library();
  usbg_state *getState() const;
};
