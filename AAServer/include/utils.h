// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include <string>
#include <sys/stat.h>
#include <vector>
#pragma once

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define cpu_to_le16(x) (x)
#define cpu_to_le32(x) (x)
#else
#error "Big Endian not supported"
#endif
#define be16_to_cpu(x) be16toh(x)

void checkUsbgError(int returnValue);
std::string rr(const std::string &str);
std::string sr(const std::string &str);
ssize_t checkError(ssize_t ret, const std::vector<int> &ignoredErrors);
void pushBackInt16(std::vector<uint8_t> &vec, uint16_t num);
std::string hexStr(uint8_t *data, int len);
