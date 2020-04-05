// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "ChannelType.h"
#include "Function.h"
#include "Gadget.h"
#include "Message.h"
#include <boost/signals2.hpp>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <openssl/ossl_typ.h>
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
    PingRequest = 0xb,
    PingResponse = 0xc,
    NavigationFocusRequest = 0x0d,
    NavigationFocusResponse = 0x0e,
    AudioFocusRequest = 0x12,
    AudioFocusResponse = 0x13,
  };

  enum MediaMessageType {
    MediaWithTimestampIndication = 0x0000,
    MediaIndication = 0x0001,
    SetupRequest = 0x8000,
    StartIndication = 0x8001,
    SetupResponse = 0x8003,
    MediaAckIndication = 0x8004,
    VideoFocusIndication = 0x8008,
  };

  struct ThreadDescriptor {
    int fd;
    std::function<ssize_t(int, void *, size_t)> readFun;
    std::function<ssize_t(int, const void *, size_t)> writeFun;
    std::function<void(const std::exception &ex)> endFun;
    std::function<bool()> checkTerminate;
  };

  void sendMessage(uint8_t channel, uint8_t flags,
                   const std::vector<uint8_t> &buf);
  void sendVersionResponse(__u16 major, __u16 minor);
  void handleVersionRequest(const void *buf, size_t nbytes);
  void handleSslHandshake(const void *buf, size_t nbytes);
  void sendServiceDiscoveryRequest();
  void handleServiceDiscoveryResponse(const void *buf, size_t nbytes);
  void handleMessageContent(const Message &message);
  ssize_t handleMessage(int fd, const void *buf, size_t nbytes);
  std::vector<uint8_t> decryptMessage(const std::vector<uint8_t> &encryptedMsg);
  ssize_t getMessage(int fd, void *buf, size_t nbytes);
  ssize_t handleEp0Message(int fd, const void *buf, size_t nbytes);
  void threadTerminated(const std::exception &ex);
  static ssize_t readWraper(int fd, void *buf, size_t nbytes);
  static void dataPump(ThreadDescriptor *threadDescriptor);
  void startThread(int fd, std::function<ssize_t(int, void *, size_t)> readFun,
                   std::function<ssize_t(int, const void *, size_t)> writeFun);

  // SSL related
  void initializeSsl();
  void initializeSslContext();
  static int verifyCertificate(int preverify_ok, X509_STORE_CTX *x509_ctx);
  SSL_CTX *ctx = nullptr;
  SSL *ssl = nullptr;
  BIO *readBio = nullptr;
  BIO *writeBio = nullptr;

  uint8_t channelTypeToChannelNumber[ChannelType::MaxValue];
  void handleChannelMessage(const Message &message);

  std::map<int, bool> gotChannelOpenResponse;
  std::map<int, bool> gotSetupResponse;
  void sendChannelOpenRequest(int channelId);
  void expectChannelOpenResponse(int channelId);
  void sendSetupRequest(int channelId);
  void expectSetupResponse(int channelId);
  void sendStartIndication(int channelId);
  std::mutex m;
  std::condition_variable cv;
  std::vector<uint8_t> serviceDescriptor;

  std::set<uint8_t> openedChannels;

public:
  AaCommunicator(const Library &_lib);
  void setup(const Udc &udc);
  boost::signals2::signal<void(const std::exception &ex)> error;
  boost::signals2::signal<void(uint8_t channelNumber, bool specific,
                               std::vector<uint8_t> data)>
      gotMessage;
  uint8_t openChannel(ChannelType ct);
  void sendToChannel(uint8_t channelNumber, bool specific,
                     const std::vector<uint8_t> &data);
  void closeChannel(ChannelType ct);
  std::vector<uint8_t> getServiceDescriptor();

  ~AaCommunicator();
};
