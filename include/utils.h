// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "../external/backward-cpp/backward.hpp"
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <gst/gst.h>
#include <vector>
#pragma once

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define cpu_to_le16(x) (x)
#define cpu_to_le32(x) (x)
#else
#error "Big Endian not supported"
#endif
#define be16_to_cpu(x) be16toh(x)

#define GSTCHECK(x) GstCheck(x, __LINE__)

void GstCheck(gboolean returnCode, int line);
std::string rr(const std::string &str);
std::string sr(const std::string &str);
ssize_t checkError(ssize_t ret, const std::vector<int> &ignoredErrors);
void pushBackInt16(std::vector<uint8_t> &vec, uint16_t num);
void pushBackInt64(std::vector<uint8_t> &vec, uint64_t num);
std::string hexStr(uint8_t *data, int len);
uint64_t bytesToUInt64(const std::vector<uint8_t> &vec, int offset);

class client_disconnected_error : public std::runtime_error {
public:
  client_disconnected_error() : runtime_error("client disconnected error") {}
};

class aa_runtime_error : public std::runtime_error {
  backward::StackTrace st;

public:
  aa_runtime_error(const std::string &error) : runtime_error(error) {
    st.load_here(32);
  }
  void printTrace(std::ostream &ostr) const;
};
