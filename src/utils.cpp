// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "utils.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <usbg/usbg.h>
#include <vector>

using namespace std;

void checkUsbgError(int returnValue) {
  if (returnValue != USBG_SUCCESS) {
    throw runtime_error(string("Libusbg error: ") +
                        usbg_strerror((usbg_error)returnValue));
  }
}

ssize_t checkError(ssize_t ret, const std::vector<int> &ignoredErrors) {
  if (ret > 0) {
    return ret;
  } else if (ret == 0) {
    return -1;
  } else if (std::find(ignoredErrors.begin(), ignoredErrors.end(), errno) !=
             ignoredErrors.end()) {
    return 0;
  } else {
    throw std::runtime_error("checkError");
  }
}

// input + run random
string rr(const string &str) { return str; }

// input + system random
string sr(const string &str) { return str; }

void pushBackInt16(std::vector<uint8_t> &vec, uint16_t num) {
  vec.push_back((num >> 8) & 0xff);
  vec.push_back((num >> 0) & 0xff);
}

std::string hexStr(uint8_t *data, int len) {
  std::stringstream ss;
  ss << std::hex;

  for (int i = 0; i < len; i++)
    ss << std::setw(2) << std::setfill('0') << (int)data[i] << " ";

  return ss.str();
}
