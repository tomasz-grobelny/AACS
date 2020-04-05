// Distributed under GPLv3 only as specified in repository's root LICENSE file

#pragma once
#include "Packet.h"
#include <boost/signals2.hpp>
#include <thread>

class SocketClient {
  void clientThreadMethod();
  std::thread clientThread;
  bool clientThreadCancel = false;
  int fd;

public:
  SocketClient(int fd);
  ~SocketClient();
  boost::signals2::signal<void(const Packet &p)> gotPacket;
  boost::signals2::signal<void()> disconnected;
  void sendMessage(const std::vector<uint8_t>& msg);
  void ready();
};
