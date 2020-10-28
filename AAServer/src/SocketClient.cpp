// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "SocketClient.h"
#include "utils.h"
#include <asm-generic/errno.h>
#include <cstddef>

using namespace std;

SocketClient::SocketClient(int _fd) : fd(_fd) {}

SocketClient::~SocketClient() {
  clientThreadCancel = true;
  clientThread.join();
  close(fd);
}

void SocketClient::ready() {
  clientThread = std::thread([this]() {
    try {
      clientThreadMethod();
    } catch (exception &ex) {
      close(fd);
    }
  });
}

void SocketClient::clientThreadMethod() {
  int bufSize = 100 * 1024;
  uint8_t buffer[bufSize];
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
      p.packetType = (PacketType)buffer[0];
      p.channelNumber = buffer[1];
      p.specific = buffer[2];
      copy(buffer + 3, buffer + ret, back_inserter(p.data));
      gotPacket(p);
    }
  }
}

void SocketClient::sendMessage(const std::vector<uint8_t> &msg) {
  auto ret = write(fd, msg.data(), msg.size());
  if (ret != msg.size()) {
    if (ret == -1 && errno == ECONNRESET) {
      throw client_disconnected_error();
    } else {
      throw runtime_error("sendMessage failed: " + to_string(ret) +
                          " errno=" + to_string(errno));
    }
  }
}
