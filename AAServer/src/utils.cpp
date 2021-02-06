// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "utils.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <usbg/usbg.h>
#include <vector>
//#include "backward.hpp"

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
    throw aa_runtime_error("checkError: " + to_string(ret) + " " +
                           to_string(errno));
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

void pushBackInt64(std::vector<uint8_t> &vec, uint64_t num) {
  vec.push_back((num >> 56) & 0xff);
  vec.push_back((num >> 48) & 0xff);
  vec.push_back((num >> 40) & 0xff);
  vec.push_back((num >> 32) & 0xff);
  vec.push_back((num >> 24) & 0xff);
  vec.push_back((num >> 16) & 0xff);
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

uint64_t bytesToUInt64(const std::vector<uint8_t> &vec, int offset) {
  if (vec.size() - offset < 8)
    throw runtime_error("bytesToUInt64: vector too small");
  return ((uint64_t)vec[offset + 0] << 56) | ((uint64_t)vec[offset + 1] << 48) |
         ((uint64_t)vec[offset + 2] << 40) | ((uint64_t)vec[offset + 3] << 32) |
         ((uint64_t)vec[offset + 4] << 24) | ((uint64_t)vec[offset + 5] << 16) |
         ((uint64_t)vec[offset + 6] << 8) | ((uint64_t)vec[offset + 7] << 0);
}

void aa_runtime_error::printTrace(std::ostream &ostr) const {
  backward::Printer p;
  p.object = true;
  p.color_mode = backward::ColorMode::always;
  p.address = true;
  p.print(st, ostr);
}
