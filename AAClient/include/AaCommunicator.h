#pragma once
#include "ChannelHandler.h"
#include "Device.h"
#include "Message.h"
#include "enums.h"
#include <boost/signals2.hpp>
#include <condition_variable>
#include <deque>
#include <libusb.h>
#include <memory>
#include <mutex>
#include <openssl/ossl_typ.h>
#include <pcap/pcap.h>
#include <thread>

class AaCommunicator {
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
  void handleChannelMessage(const Message &msg);
  void forwardChannelMessage(const Message &msg);

  const std::vector<uint8_t> &serviceDescription;
  std::mutex sendQueueMutex;
  std::deque<Message> sendQueue;
  std::condition_variable sendQueueNotEmpty;

  void writeThread();
  void writeToUsb(const std::vector<uint8_t> &buffer);
  std::vector<uint8_t> prepareMessage();
  std::vector<std::thread> threads;

  ChannelHandler *channelHandlers[UINT8_MAX];

  pcap_t *pd = nullptr;
  pcap_dumper_t *pdumper = nullptr;
  void logMessage(const Message &msg, bool direction);

public:
  AaCommunicator(const Device &device,
                 const std::vector<uint8_t> &serviceDescription,
                 const std::string &dumpfile);
  ~AaCommunicator();
  void setup();
  boost::signals2::signal<void(uint8_t channelNumber, bool specific,
                               const std::vector<uint8_t> &msg)>
      channelMessage;
  void sendMessagePublic(uint8_t channel, bool specific,
                         const std::vector<uint8_t> &buf);
};
