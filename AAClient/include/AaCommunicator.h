#pragma once
#include "Device.h"
#include "Message.h"
#include <boost/signals2.hpp>
#include <condition_variable>
#include <deque>
#include <libusb.h>
#include <memory>
#include <mutex>
#include <openssl/ossl_typ.h>
#include <thread>

class AaCommunicator {
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

  const Device &device;
  std::unique_ptr<libusb_device_handle, void (*)(libusb_device_handle *)>
      deviceHandle;

  void initializeSsl();
  void initializeSslContext();
  static int verifyCertificate(int preverify_ok, X509_STORE_CTX *x509_ctx);
  SSL_CTX *ctx = nullptr;
  SSL *ssl = nullptr;
  BIO *readBio = nullptr;
  BIO *writeBio = nullptr;
  bool doSslHandshake(const std::vector<uint8_t> msgContent);
  std::vector<uint8_t> decryptMessage(const std::vector<uint8_t> &encryptedMsg);

  void sendMessage(uint8_t channel, uint8_t flags,
                   const std::vector<uint8_t> &buf);
  Message getMessage();
  void sendVersionRequest(uint16_t major, uint16_t minor);
  void expectVersionResponse();
  void sendAuthComplete();
  void handleServiceDiscoveryRequest(const Message &msg);
  void handleChannelOpenRequest(const Message &msg);
  void forwardChannelMessage(const Message &msg);

  const std::vector<uint8_t> &serviceDescription;
  std::mutex sendQueueMutex;
  std::deque<Message> sendQueue;
  std::condition_variable sendQueueNotEmpty;

  void writeThread();
  void writeToUsb(const std::vector<uint8_t> &buffer);
  std::vector<uint8_t> prepareMessage();
  std::vector<std::thread> threads;

public:
  AaCommunicator(const Device &device,
                 const std::vector<uint8_t> &serviceDescription);
  ~AaCommunicator();
  void setup();
  boost::signals2::signal<void(uint8_t channelNumber, bool specific,
                               const std::vector<uint8_t> &msg)>
      channelMessage;
  void sendMessagePublic(uint8_t channel, bool specific,
                         const std::vector<uint8_t> &buf);
};
