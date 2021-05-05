// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "AaCommunicator.h"
#include "Channel.pb.h"
#include "ChannelOpenRequest.pb.h"
#include "Configuration.h"
#include "DefaultChannelHandler.h"
#include "FfsFunction.h"
#include "InputChannelHandler.h"
#include "MediaStreamType.pb.h"
#include "Message.h"
#include "PingRequest.pb.h"
#include "PingResponse.pb.h"
#include "ServiceDiscoveryRequest.pb.h"
#include "ServiceDiscoveryResponse.pb.h"
#include "Udc.h"
#include "VideoChannelHandler.h"
#include "descriptors.h"
#include "utils.h"
#include <boost/filesystem.hpp>
#include <boost/signals2.hpp>
#include <cstdint>
#include <fcntl.h>
#include <fmt/core.h>
#include <iostream>
#include <linux/usb/functionfs.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <pcap/pcap.h>
#include <stdexcept>

#define CRT_FILE "android_auto.crt"
#define PRIVKEY_FILE "android_auto.key"
#define DHPARAM_FILE "dhparam.pem"

using namespace std;
using namespace boost::filesystem;
using namespace tag::aas;

void AaCommunicator::logMessage(const Message &msg, bool direction) {
  if (!pdumper)
    return;
  int pktSize = 8 + msg.content.size();
  uint8_t buffer[pktSize];
  buffer[0] = 0;
  buffer[1] = 0;
  buffer[2] = 0;
  buffer[3] = 0;
  buffer[4] = msg.channel;
  buffer[5] = msg.flags;
  buffer[6] = direction ? 1 : 0;
  buffer[7] = 0;
  copy(msg.content.begin(), msg.content.end(), buffer + 8);
  struct pcap_pkthdr packet_header;
  gettimeofday(&packet_header.ts, NULL);
  packet_header.caplen = pktSize;
  packet_header.len = pktSize;
  pcap_dump((unsigned char *)pdumper, &packet_header, buffer);
}

void AaCommunicator::sendMessage(uint8_t channel, uint8_t flags,
                                 const std::vector<uint8_t> &buf) {
  Message msg;
  msg.channel = channel;
  msg.flags = flags;
  msg.content = buf;
  logMessage(msg, true);
  {
    std::unique_lock<std::mutex> lk(sendQueueMutex);
    sendQueue.push_back(msg);
  }
  sendQueueNotEmpty.notify_all();
}

void AaCommunicator::sendVersionResponse(__u16 major, __u16 minor) {
  std::vector<uint8_t> msg;
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

void AaCommunicator::sendServiceDiscoveryRequest() {
  class ServiceDiscoveryRequest sdr;
  sdr.set_manufacturer("TAG");
  sdr.set_model("AAServer");
  auto msgString = sdr.SerializeAsString();
  std::vector<uint8_t> plainMsg;
  pushBackInt16(plainMsg, MessageType::ServiceDiscoveryRequest);
  std::copy(msgString.begin(), msgString.end(), std::back_inserter(plainMsg));
  sendMessage(0, EncryptionType::Encrypted | FrameType::Bulk, plainMsg);
}

void AaCommunicator::handleServiceDiscoveryResponse(const void *buf,
                                                    size_t nbytes) {
  serviceDescriptor = vector<uint8_t>((uint8_t *)buf, (uint8_t *)buf + nbytes);
  class tag::aas::ServiceDiscoveryResponse sdr;
  sdr.ParseFromArray(buf, nbytes);
  std::cout << sdr.DebugString() << std::endl;
  for (auto ch : sdr.channels()) {
    if (ch.has_media_channel() &&
        ch.media_channel().media_type() ==
            MediaStreamType_Enum::MediaStreamType_Enum_Video) {
      channelTypeToChannelNumber[ChannelType::Video] = ch.channel_id();
      channelHandlers[ch.channel_id()] =
          new VideoChannelHandler(ch.channel_id());
    } else if (ch.has_input_channel()) {
      channelTypeToChannelNumber[ChannelType::Input] = ch.channel_id();
      auto available_buttons = ch.input_channel().available_buttons();
      channelHandlers[ch.channel_id()] =
          new InputChannelHandler(ch.channel_id(), {available_buttons.begin(),
                                                    available_buttons.end()});
    } else {
      channelHandlers[ch.channel_id()] =
          new DefaultChannelHandler(ch.channel_id());
    }
    channelHandlers[ch.channel_id()]->sendToClient.connect(
        [this](int clientId, uint8_t channelNumber, bool specific,
               std::vector<uint8_t> data) {
          gotMessage(clientId, channelNumber, specific, data);
        });
    channelHandlers[ch.channel_id()]->sendToHeadunit.connect(
        [this](uint8_t channelNumber, uint8_t flags,
               std::vector<uint8_t> data) {
          sendMessage(channelNumber, flags, data);
        });
  }
}

uint8_t AaCommunicator::getChannelNumberByChannelType(ChannelType ct) {
  auto channelId = channelTypeToChannelNumber[ct];
  if (channelId < 0) {
    throw runtime_error("Invalid channelId");
  }
  return channelId;
}

void AaCommunicator::handleChannelMessage(const Message &message) {
  auto msg = message.content;
  const __u16 *shortView = (const __u16 *)(msg.data());
  auto messageType = be16_to_cpu(shortView[0]);
  {
    std::unique_lock<std::mutex> lk(m);
    if (message.channel != 0) {
      auto handled =
          this->channelHandlers[message.channel]->handleMessageFromHeadunit(
              message);
      if (!handled) {
        throw aa_runtime_error(
            fmt::format("Unknown packet on channel {0} with message type {1}",
                        to_string(message.channel), to_string(messageType)));
      }
    } else {
      gotMessage(-1, message.channel,
                 message.flags & MessageTypeFlags::Specific, msg);
    }
  }
  cv.notify_all();
}

void AaCommunicator::sendToChannel(int clientId, uint8_t channelNumber,
                                   bool specific, const vector<uint8_t> &data) {
  if (channelHandlers[channelNumber] == nullptr) {
    throw std::runtime_error("No handler for channel " +
                             to_string(channelNumber));
  }
  channelHandlers[channelNumber]->handleMessageFromClient(
      clientId, channelNumber, specific, data);
}

void AaCommunicator::disconnected(int clientId) {
  for (auto ch : channelHandlers) {
    if (ch)
      ch->disconnected(clientId);
  }
}

std::vector<uint8_t> AaCommunicator::getServiceDescriptor() {
  return serviceDescriptor;
}

std::vector<uint8_t>
AaCommunicator::decryptMessage(const std::vector<uint8_t> &encryptedMsg) {
  ERR_clear_error();

  auto bytesWritten =
      BIO_write(readBio, encryptedMsg.data(), encryptedMsg.size());
  if (bytesWritten < 0) {
    throw std::runtime_error("BIO_write failed");
  }
  const int plainBufSize = 100 * 1024;
  char plainBuf[plainBufSize];
  auto ret = SSL_read(ssl, plainBuf, plainBufSize);
  if (ret < 0) {
    auto err = SSL_get_error(ssl, ret);
    ERR_print_errors_fp(stdout);
    auto message = "SSL_read failed: " + std::to_string(ret);
    message += " " + std::to_string(err);
    message += " " + std::to_string(encryptedMsg.size());
    message += " " + std::to_string(bytesWritten);
    if (err == SSL_ERROR_SSL)
      message += " "s + ERR_error_string(ERR_get_error(), NULL);
    throw std::runtime_error(message);
  }
  return std::vector<uint8_t>(plainBuf, plainBuf + ret);
}

void AaCommunicator::handleMessageContent(const Message &message) {
  logMessage(message, false);
  auto msg = message.content;
  const __u16 *shortView = (const __u16 *)msg.data();
  MessageType messageType = (MessageType)be16_to_cpu(shortView[0]);
  if (message.channel != 0) {
    handleChannelMessage(message);
  } else if (messageType == MessageType::AudioFocusResponse) {
    handleChannelMessage(message);
  } else if (messageType == MessageType::NavigationFocusResponse) {
    handleChannelMessage(message);
  } else if (messageType == MessageType::VersionRequest) {
    cout << "got version request" << endl;
    handleVersionRequest(shortView + 1, msg.size() - sizeof(__u16));
    cout << "version negotiation ok" << endl;
  } else if (messageType == MessageType::SslHandshake) {
    handleSslHandshake(shortView + 1, msg.size() - sizeof(__u16));
  } else if (messageType == MessageType::AuthComplete) {
    cout << "auth complete" << endl;
    sendServiceDiscoveryRequest();
  } else if (messageType == MessageType::ServiceDiscoveryResponse) {
    cout << "got service discovery response" << endl;
    handleServiceDiscoveryResponse(shortView + 1, msg.size() - sizeof(__u16));
  } else if (messageType == MessageType::PingRequest) {
    cout << "got ping request" << endl;
    handlePingRequest(shortView + 1, msg.size() - sizeof(__u16));
  } else {
    throw std::runtime_error("Unhandled message type: " +
                             std::to_string(messageType));
  }
}

void AaCommunicator::handlePingRequest(const void *buf, size_t nbytes) {
  class tag::aas::PingRequest preq;
  preq.ParseFromArray(buf, nbytes);
  std::cout << preq.DebugString() << std::endl;

  tag::aas::PingResponse presp;
  presp.set_timestamp(preq.timestamp());
  int bufSize = presp.ByteSize();
  uint8_t buffer[bufSize];
  if (!presp.SerializeToArray(buffer, bufSize))
    throw aa_runtime_error("presp.SerializeToArray failed");

  std::vector<uint8_t> plainMsg;
  pushBackInt16(plainMsg, MessageType::PingResponse);
  copy(buffer, buffer + bufSize, std::back_inserter(plainMsg));
  sendMessage(0, EncryptionType::Encrypted | FrameType::Bulk, plainMsg);
}

void AaCommunicator::handleSslHandshake(const void *buf, size_t nbytes) {
  initializeSsl();
  BIO_write(readBio, buf, nbytes);

  auto ret = SSL_accept(ssl);
  if (ret == -1) {
    auto error = SSL_get_error(ssl, ret);
    if (error != SSL_ERROR_WANT_READ)
      throw runtime_error("SSL_accept failed");
  }

  std::vector<uint8_t> msg;
  pushBackInt16(msg, MessageType::SslHandshake);
  auto bufferSize = 512;
  char buffer[bufferSize];
  int len;
  while ((len = BIO_read(writeBio, buffer, bufferSize)) != -1) {
    std::copy(buffer, buffer + len, std::back_inserter(msg));
  }
  sendMessage(0, EncryptionType::Plain | FrameType::Bulk, msg);
}

void AaCommunicator::initializeSsl() {
  if (ssl)
    return;
  ssl = SSL_new(ctx);
  readBio = BIO_new(BIO_s_mem());
  writeBio = BIO_new(BIO_s_mem());
  SSL_set_accept_state(ssl);
  SSL_set_bio(ssl, readBio, writeBio);
}

void AaCommunicator::initializeSslContext() {
  SSL_load_error_strings();
  OpenSSL_add_ssl_algorithms();
  const SSL_METHOD *method = SSLv23_server_method();

  ctx = SSL_CTX_new(method);
  if (!ctx) {
    throw std::runtime_error("Error while creating SSL context");
  }

  SSL_CTX_set_ecdh_auto(ctx, 1);
  if (SSL_CTX_use_certificate_file(ctx, CRT_FILE, SSL_FILETYPE_PEM) <= 0) {
    throw std::runtime_error("Error on SSL_CTX_use_certificate_file");
  }

  if (SSL_CTX_use_PrivateKey_file(ctx, PRIVKEY_FILE, SSL_FILETYPE_PEM) <= 0) {
    throw std::runtime_error("Error on SSL_CTX_use_PrivateKey_file");
  }

  DH *dh_2048 = NULL;
  FILE *paramfile = fopen(DHPARAM_FILE, "r");
  if (paramfile) {
    dh_2048 = PEM_read_DHparams(paramfile, NULL, NULL, NULL);
    fclose(paramfile);
  } else {
    throw std::runtime_error("Cannot read DH parameters file");
  }
  if (dh_2048 == NULL) {
    throw std::runtime_error("Reading DH parameters failed");
  }
  if (SSL_CTX_set_tmp_dh(ctx, dh_2048) != 1) {
    throw std::runtime_error("SSL_CTX_set_tmp_dh failed");
  }
  SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, &AaCommunicator::verifyCertificate);
  SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1_3);
}

int AaCommunicator::verifyCertificate(int preverify_ok,
                                      X509_STORE_CTX *x509_ctx) {
  return 1;
}

ssize_t AaCommunicator::handleMessage(int fd, const void *buf, size_t nbytes) {
  const uint8_t *byteView = (const uint8_t *)buf;
  int channel = byteView[0];
  int flags = byteView[1];
  bool encrypted = flags & 0x8;
  FrameType frameType = (FrameType)(flags & 0x3);
  int lengthRaw = *(__u16 *)(byteView + 2);
  int length = be16_to_cpu(*(__u16 *)(byteView + 2));
  std::vector<uint8_t> msg;
  if (nbytes < 4 + length)
    throw std::runtime_error("nbytes<4+length");
  std::copy(byteView + 4, byteView + 4 + length, std::back_inserter(msg));
  Message message;
  message.channel = channel;
  message.flags = flags;
  message.content = encrypted ? decryptMessage(msg) : msg;
  handleMessageContent(message);
  return length + 4;
}

ssize_t AaCommunicator::getMessage(int fd, void *buf, size_t nbytes) {
  std::unique_lock<std::mutex> lk(sendQueueMutex);
  if (!sendQueueNotEmpty.wait_for(lk, 1s, [=] { return !sendQueue.empty(); })) {
    errno = EINTR;
    return 0;
  }

  // it should work up to about 16k, but we might get some weird hardware issues
  int maxSize = 2000;

  auto msg = sendQueue.front();
  uint32_t totalLength = msg.content.size();
  std::vector<uint8_t> msgBytes;
  if (msg.flags & EncryptionType::Encrypted) {
    msgBytes.push_back(msg.channel);
    auto flags = msg.flags;
    std::vector<uint8_t>::iterator contentBegin;
    std::vector<uint8_t>::iterator contentEnd;
    // full frame
    if (msg.content.size() - msg.offset <= maxSize &&
        (flags & FrameType::Bulk)) {
      contentBegin = msg.content.begin() + msg.offset;
      contentEnd = msg.content.end();
      sendQueue.pop_front();
    }
    // first frame
    else if (msg.content.size() - msg.offset > maxSize &&
             (flags & FrameType::Bulk)) {
      flags = flags & ~FrameType::Bulk;
      flags = flags | FrameType::First;
      contentBegin = msg.content.begin() + msg.offset;
      contentEnd = msg.content.begin() + msg.offset + maxSize;
      sendQueue.front().flags = flags & ~FrameType::Bulk;
      sendQueue.front().offset += maxSize;
    }
    // intermediate frame
    else if (msg.content.size() - msg.offset > maxSize) {
      contentBegin = msg.content.begin() + msg.offset;
      contentEnd = msg.content.begin() + msg.offset + maxSize;
      sendQueue.front().flags = flags & ~FrameType::Bulk;
      sendQueue.front().offset += maxSize;
    }
    // last frame
    else {
      contentBegin = msg.content.begin() + msg.offset;
      contentEnd = msg.content.end();
      flags = flags | FrameType::Last;
      sendQueue.pop_front();
    }
    msgBytes.push_back(flags);
    auto ret = SSL_write(ssl, contentBegin.base(), contentEnd - contentBegin);
    if (ret < 0) {
      throw std::runtime_error("SSL_write error");
    }
    lk.unlock();
    auto encBuf = (uint8_t *)buf;
    auto offset = 4;
    if ((flags & FrameType::Bulk) == FrameType::First) {
      offset += 4;
    }
    auto length = BIO_read(writeBio, encBuf + offset, nbytes - offset);
    if (length < 0) {
      throw std::runtime_error("BIO_read error");
    }
    encBuf[0] = msg.channel;
    encBuf[1] = flags;
    encBuf[2] = (length >> 8);
    encBuf[3] = (length & 0xff);
    if ((flags & FrameType::Bulk) == FrameType::First) {
      encBuf[4] = ((totalLength >> 24) & 0xff);
      encBuf[5] = ((totalLength >> 16) & 0xff);
      encBuf[6] = ((totalLength >> 8) & 0xff);
      encBuf[7] = ((totalLength >> 0) & 0xff);
    }
    return length + offset;
  } else {
    sendQueue.pop_front();
    msgBytes.push_back(msg.channel);
    msgBytes.push_back(msg.flags);
    int length = msg.content.size();
    pushBackInt16(msgBytes, length);
    std::copy(msg.content.begin(), msg.content.end(),
              std::back_inserter(msgBytes));
    std::copy(msgBytes.begin(), msgBytes.end(), (uint8_t *)buf);
    return msgBytes.size();
  }
}

ssize_t AaCommunicator::handleEp0Message(int fd, const void *buf,
                                         size_t nbytes) {
  const usb_functionfs_event *event = (const usb_functionfs_event *)buf;
  for (size_t n = nbytes / sizeof *event; n; --n, ++event) {
    std::cout << "ep0 event " << (int)event->type << " " << std::endl;
    if (event->type == FUNCTIONFS_SUSPEND) {
      throw std::runtime_error("ep0 suspend");
    }
  }
  return nbytes;
}

AaCommunicator::AaCommunicator(const Library &_lib, const std::string &dumpfile)
    : lib(_lib) {
  initializeSslContext();
  fill_n(channelTypeToChannelNumber, ChannelType::MaxValue, -1);
  fill_n(channelHandlers, UINT8_MAX + 1, nullptr);
  channelHandlers[0] = new DefaultChannelHandler(0);
  channelHandlers[0]->sendToClient.connect(
      [this](int clientId, uint8_t channelNumber, bool specific,
             std::vector<uint8_t> data) {
        gotMessage(clientId, channelNumber, specific, data);
      });
  channelHandlers[0]->sendToHeadunit.connect(
      [this](uint8_t channelNumber, uint8_t flags, std::vector<uint8_t> data) {
        sendMessage(channelNumber, flags, data);
      });
  cout << "dumpfile: " << dumpfile << endl;

  if (!dumpfile.empty()) {
    pd = pcap_open_dead(DLT_NULL, 65535);
    pdumper = pcap_dump_open(pd, dumpfile.c_str());
  }
}

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
  write_descriptors_accessory(ep0fd);
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
  td->endFun = [this](const std::exception &ex) { threadTerminated(ex); };
  td->checkTerminate = [this]() { return threadFinished; };
  threads.push_back(std::thread(&AaCommunicator::dataPump, td));
}

void AaCommunicator::dataPump(ThreadDescriptor *t) {
  int bufSize = 100 * 1024;
  char buffer[bufSize];

  signal(SIGUSR1, [](int) {});

  for (;;) {
    try {
      auto length =
          checkError(t->readFun(t->fd, buffer, bufSize), {EINTR, EAGAIN});
      if (t->checkTerminate()) {
        return;
      }
      auto start = 0;
      while (length > 0) {
        auto partLength =
            checkError(t->writeFun(t->fd, (char *)buffer + start, length),
                       {EINTR, EAGAIN});
        if (t->checkTerminate()) {
          return;
        }
        length -= partLength;
        start += partLength;
      }
    } catch (const std::exception &ex) {
      t->endFun(ex);
      break;
    }
  }
  return;
}

void AaCommunicator::threadTerminated(const std::exception &ex) {
  {
    std::unique_lock<std::mutex> lk(m);
    threadFinished = true;
  }
  cv.notify_all();
  error(ex);
}

AaCommunicator::~AaCommunicator() {
  {
    std::unique_lock<std::mutex> lk(m);
    threadFinished = true;
  }

  // workaround for blocking read
  for (auto &&th : threads) {
    pthread_kill(th.native_handle(), SIGUSR1);
  }
  for (auto &&th : threads) {
    th.join();
  }

  if (ep2fd != -1)
    close(ep2fd);
  if (ep1fd != -1)
    close(ep1fd);
  if (ep0fd != -1)
    close(ep0fd);
  if (pdumper)
    pcap_dump_close(pdumper);
  if (pd)
    pcap_close(pd);
}
