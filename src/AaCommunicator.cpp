// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "AaCommunicator.h"
#include "Configuration.h"
#include "FfsFunction.h"
#include "Udc.h"
#include "descriptors.h"
#include "utils.h"
#include <boost/filesystem.hpp>
#include <fcntl.h>
#include <functionfs.h>
#include <iostream>
#include <boost/signals2.hpp>

using namespace std;
using namespace boost::filesystem;

void AaCommunicator::sendMessage(__u8 channel, __u8 flags,
                                 const std::vector<__u8> &buf) {
  std::unique_lock<std::mutex> lk(sendQueueMutex);
  Message msg;
  msg.channel = channel;
  msg.flags = flags;
  msg.content = buf;
  sendQueue.push_back(msg);
  sendQueueNotEmpty.notify_all();
}

void AaCommunicator::sendVersionResponse(__u16 major, __u16 minor) {
  std::vector<__u8> msg;
  pushBackInt16(msg, MessageType::VersionResponse);
  pushBackInt16(msg, major);
  pushBackInt16(msg, minor);
  // 0 => version match
  pushBackInt16(msg, 0);
  sendMessage(0, EncryptionType::Plain | FrameType::Bulk, msg);
}

void AaCommunicator::handleVersionRequest(const void *buf, size_t nbytes) {
  const __u16 *shortView = (const __u16 *)buf;
  auto versionMajor = be16_to_cpu(shortView[0]);
  auto versionMinor = be16_to_cpu(shortView[1]);
  if (versionMajor == 1)
    sendVersionResponse(1, 5);
  else
    throw std::runtime_error("unsupported version");
}

void AaCommunicator::handleMessageContent(const Message &message) {
  auto msg = message.content;
  const __u16 *shortView = (const __u16 *)(&msg[0]);
  MessageType messageType = (MessageType)be16_to_cpu(shortView[0]);
  if (messageType == MessageType::VersionRequest) {
    handleVersionRequest(shortView + 1, msg.size() - sizeof(__u16));
  } else {
    throw std::runtime_error("unhandled message type");
  }
}

ssize_t AaCommunicator::handleMessage(int fd, const void *buf, size_t nbytes) {
  const __u8 *byteView = (const __u8 *)buf;
  int channel = byteView[0];
  int flags = byteView[1];
  bool encrypted = flags & 0x8;
  FrameType frameType = (FrameType)(flags & 0x3);
  int lengthRaw = *(__u16 *)(byteView + 2);
  int length = be16_to_cpu(*(__u16 *)(byteView + 2));
  std::vector<__u8> msg;
  if (nbytes < 4 + length)
    throw std::runtime_error("nbytes<4+length");
  std::copy(byteView + 4, byteView + 4 + length, std::back_inserter(msg));
  Message message;
  message.channel = channel;
  message.flags = flags;
  message.content = msg;
  handleMessageContent(message);
  return length + 4;
}

ssize_t AaCommunicator::getMessage(int fd, void *buf, size_t nbytes) {
  std::unique_lock<std::mutex> lk(sendQueueMutex);
  if (!sendQueueNotEmpty.wait_for(lk, 1s, [=] { return !sendQueue.empty(); })) {
    errno = EINTR;
    return 0;
  }

  int maxSize = 2000;

  auto msg = sendQueue.front();
  uint32_t totalLength = msg.content.size();
  std::vector<__u8> msgBytes;

  sendQueue.pop_front();
  msgBytes.push_back(msg.channel);
  msgBytes.push_back(msg.flags);
  int length = msg.content.size();
  pushBackInt16(msgBytes, length);
  std::copy(msg.content.begin(), msg.content.end(),
            std::back_inserter(msgBytes));
  std::copy(msgBytes.begin(), msgBytes.end(), (__u8 *)buf);

  return msgBytes.size();
}

ssize_t AaCommunicator::handleEp0Message(int fd, const void *buf,
                                         size_t nbytes) {
  const usb_functionfs_event *event = (const usb_functionfs_event *)buf;
  for (size_t n = nbytes / sizeof *event; n; --n, ++event) {
    std::cout << "ep0 event " << (int)event->type << " " << std::endl;
  }
  return nbytes;
}

AaCommunicator::AaCommunicator(const Library &_lib) : lib(_lib) {}

void AaCommunicator::setup(const Udc &udc) {
  mainGadget =
      unique_ptr<Gadget>(new Gadget(lib, 0x18d1, 0x2d00, rr("main_state")));
  mainGadget->setStrings("TAG", "AAServer", sr("TAGAAS"));

  auto tmpMountpoint = boost::filesystem::temp_directory_path() /
                       rr("AAServer_mp_loopback_main");
  create_directory(tmpMountpoint);
  ffs_function = unique_ptr<Function>(
      new FfsFunction(*mainGadget, rr("ffs_main"), tmpMountpoint.c_str()));

  auto configuration = new Configuration(*mainGadget, "c0");
  configuration->addFunction(*ffs_function, "ffs_main");

  ep0fd = open((tmpMountpoint / "ep0").c_str(), O_RDWR);
  write_descriptors(ep0fd, 0);
  ep1fd = open((tmpMountpoint / "ep1").c_str(), O_RDWR);
  ep2fd = open((tmpMountpoint / "ep2").c_str(), O_RDWR);

  startThread(ep0fd, readWraper,
              [=](auto &&... args) { return handleEp0Message(args...); });
  startThread(
      ep1fd, [=](auto &&... args) { return getMessage(args...); }, write);
  startThread(ep2fd, readWraper,
              [=](auto &&... args) { return handleMessage(args...); });

  mainGadget->enable(udc);
}

ssize_t AaCommunicator::readWraper(int fd, void *buf, size_t nbytes) {
  struct timeval timeout {
    1, 0
  };

  fd_set set;
  FD_ZERO(&set);
  FD_SET(fd, &set);

  auto rv = select(fd + 1, &set, NULL, NULL, &timeout);
  if (rv == -1) {
    return -1l;
  } else if (rv == 0) {
    errno = EINTR;
    return 0l;
  } else
    return read(fd, buf, nbytes); /* there was data to read */
}

void AaCommunicator::startThread(
    int fd, std::function<ssize_t(int, void *, size_t)> readFun,
    std::function<ssize_t(int, const void *, size_t)> writeFun) {
  auto td = new ThreadDescriptor();
  td->fd = fd;
  td->readFun = readFun;
  td->writeFun = writeFun;
  td->endFun = [this](std::exception &ex) { threadTerminated(ex); };
  td->checkTerminate = [this]() { return threadFinished; };
  threads.push_back(std::thread(&AaCommunicator::dataPump, td));
}

void AaCommunicator::dataPump(ThreadDescriptor *t) {
  int bufSize = 100 * 1024;
  char buffer[bufSize];

  for (;;) {
    try {
      auto length =
          checkError(t->readFun(t->fd, buffer, bufSize), {EINTR, EAGAIN});
      if (t->checkTerminate()) {
        return;
      }
      auto start = 0;
      while (length) {
        auto partLength =
            checkError(t->writeFun(t->fd, (char *)buffer + start, length),
                       {EINTR, EAGAIN});
        if (t->checkTerminate()) {
          return;
        }
        length -= partLength;
        start += partLength;
      }
    } catch (std::exception &ex) {
      t->endFun(ex);
      break;
    }
  }
  return;
}

void AaCommunicator::threadTerminated(std::exception &ex) {
  std::unique_lock<std::mutex> lk(threadsMutex);
  threadFinished = true;
  error(ex);
}

AaCommunicator::~AaCommunicator() {
  std::unique_lock<std::mutex> lk(threadsMutex);
  threadFinished = true;
  lk.release();

  for (auto &&th : threads) {
    th.join();
  }

  if (ep2fd != -1)
    close(ep2fd);
  if (ep1fd != -1)
    close(ep1fd);
  if (ep0fd != -1)
    close(ep0fd);
}
