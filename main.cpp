// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "Configuration.h"
#include "FfsFunction.h"
#include "Function.h"
#include "Gadget.h"
#include "Library.h"
#include "MassStorageFunction.h"
#include "ModeSwitcher.h"
#include "Udc.h"
#include "descriptors.h"
#include "utils.h"
#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <functionfs.h>
#include <iostream>
#include <libconfig.h>
#include <stdexcept>
#include <sys/mount.h>
#include <unistd.h>
#include <usbg/function/ms.h>
#include <usbg/usbg.h>

using namespace std;
using namespace boost::filesystem;

string configFsBasePath = "/sys/kernel/config";

int main() {

  try {
    Library lib(configFsBasePath);
    ModeSwitcher::handleSwitchToAccessoryMode(lib);
  } catch (exception &ex) {
    cout << "Exception:" << endl;
    cout << ex.what() << endl;
    return 1;
  }
  return 0;
}
