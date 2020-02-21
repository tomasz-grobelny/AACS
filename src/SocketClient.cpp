// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "SocketClient.h"
#include <cstddef>

using namespace std;

SocketClient::SocketClient(int _fd) : fd(_fd) {
  clientThread = std::thread([this]() { clientThreadMethod(); });
}

SocketClient::~SocketClient() {
  clientThreadCancel = true;
  clientThread.join();
  close(fd);
}

void SocketClient::clientThreadMethod() {
  int bufSize = 100 * 1024;
  byte buffer[bufSize];
  for (;;) {
    struct timeval timeout {
      1, 0
    };

    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd, &set);

    auto rv = select(fd + 1, &set, NULL, NULL, &timeout);
    if (clientThreadCancel) {
      break;
    }
    if (rv <= 0)
      continue;

    auto ret = read(fd, buffer, bufSize);
    if (ret == 0) {
      disconnected();
      break;
    } else {
      Packet p;
      copy(buffer, buffer + bufSize, back_inserter(p.data));
      gotPacket(p);
    }
  }
}
