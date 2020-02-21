// Distributed under GPLv3 only as specified in repository's root LICENSE file

#pragma once
#include "SocketClient.h"
#include <boost/signals2.hpp>
#include <cstddef>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class SocketCommunicator {
  void listenThreadMethod();
  std::thread listenThread;
  bool listenThreadCancel = false;
  std::vector<SocketClient *> clients;
  std::string path;
  int sock;

public:
  SocketCommunicator(std::string path);
  ~SocketCommunicator();
  boost::signals2::signal<void(SocketClient *ex)> newClient;
};
