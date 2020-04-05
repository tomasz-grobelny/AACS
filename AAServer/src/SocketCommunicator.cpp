// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "SocketCommunicator.h"
#include <iostream>
#include <iterator>
#include <mutex>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/un.h>

using namespace std;

SocketCommunicator::SocketCommunicator(std::string _path) : path(_path) {
  listenThread = std::thread([this]() { listenThreadMethod(); });
}

void SocketCommunicator::listenThreadMethod() {
  struct sockaddr_un server;

  sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
  if (sock < 0) {
    throw runtime_error("opening stream socket");
  }
  server.sun_family = AF_UNIX;
  strcpy(server.sun_path, path.c_str());
  if (bind(sock, (struct sockaddr *)&server, sizeof(struct sockaddr_un))) {
    throw runtime_error("bind failed");
  }
  listen(sock, 5);
  for (;;) {
    struct timeval timeout {
      1, 0
    };

    fd_set set;
    FD_ZERO(&set);
    FD_SET(sock, &set);

    auto rv = select(sock + 1, &set, NULL, NULL, &timeout);
    if (listenThreadCancel) {
      break;
    }
    if (rv <= 0)
      continue;
    SocketClient *client = nullptr;
    try {
      auto msgsock = accept(sock, 0, 0);
      if (msgsock == -1) {
        throw runtime_error("accept failed");
      }
      client = new SocketClient(msgsock);
      clients.insert(client);
      client->disconnected.connect([&, client]() { clients.erase(client); });
      newClient(client);
      client->ready();
    } catch (const exception &ex) {
      if (client)
        delete client;
    } catch (...) {
      if (client)
        delete client;
    }
  }
}

SocketCommunicator::~SocketCommunicator() {
  listenThreadCancel = true;
  listenThread.join();
  close(sock);
  unlink(path.c_str());
  for (auto &client : clients)
    delete client;
}
