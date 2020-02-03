// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "Function.h"
#include "Gadget.h"
#include "Message.h"
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#pragma once

class AaCommunicator {
  const Library &lib;
  std::unique_ptr<Gadget> mainGadget;
  std::unique_ptr<Function> ffs_function;
  int ep0fd = -1, ep1fd = -1, ep2fd = -1;

  std::mutex sendQueueMutex;
  std::deque<Message> sendQueue;
  std::condition_variable sendQueueNotEmpty;

  std::mutex threadsMutex;
  std::condition_variable threadsCd;
  bool threadFinished = false;
  std::vector<std::thread> threads;

  enum EncryptionType {
    Plain = 0,
    Encrypted = 1 << 3,
  };

  enum FrameType {
    First = 1,
    Last = 2,
    Bulk = First | Last,
  };

  enum MessageTypeFlags {
    Control = 0,
    Specific = 1 << 2,
  };

  enum MessageType {
    VersionRequest = 1,
    VersionResponse = 2,
    SslHandshake = 3,
    AuthComplete = 4,
    ServiceDiscoveryRequest = 5,
    ServiceDiscoveryResponse = 6,
    ChannelOpenRequest = 7,
    ChannelOpenResponse = 8,
  };

  struct ThreadDescriptor {
    int fd;
    std::function<ssize_t(int, void *, size_t)> readFun;
    std::function<ssize_t(int, const void *, size_t)> writeFun;
    std::function<void(std::exception &ex)> endFun;
    std::function<bool()> checkTerminate;
  };

  void sendMessage(__u8 channel, __u8 flags, const std::vector<__u8> &buf);
  void sendVersionResponse(__u16 major, __u16 minor);
  void handleVersionRequest(const void *buf, size_t nbytes);
  void handleMessageContent(const Message &message);
  ssize_t handleMessage(int fd, const void *buf, size_t nbytes);
  ssize_t getMessage(int fd, void *buf, size_t nbytes);
  ssize_t handleEp0Message(int fd, const void *buf, size_t nbytes);
  void threadTerminated(std::exception &ex);
  static ssize_t readWraper(int fd, void *buf, size_t nbytes);
  static void dataPump(ThreadDescriptor *threadDescriptor);
  void startThread(int fd, std::function<ssize_t(int, void *, size_t)> readFun,
                   std::function<ssize_t(int, const void *, size_t)> writeFun);

public:
  AaCommunicator(const Library &_lib);
  void setup(const Udc &udc);
  void wait();

  ~AaCommunicator();
};
