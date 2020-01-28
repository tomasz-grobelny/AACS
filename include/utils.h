// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include <string>
#include <vector>

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define cpu_to_le16(x) (x)
#define cpu_to_le32(x) (x)
#else
#error "Big Endian not supported"
#endif

void checkUsbgError(int returnValue);
std::string rr(const std::string& str);
std::string sr(const std::string& str);
ssize_t checkError(ssize_t ret, const std::vector<int> &ignoredErrors);
