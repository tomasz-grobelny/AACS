#include "utils.h"

void pushBackInt16(std::vector<uint8_t> &vec, uint16_t num) {
  vec.push_back((num >> 8) & 0xff);
  vec.push_back((num >> 0) & 0xff);
}
