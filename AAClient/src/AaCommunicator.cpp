#include "AaCommunicator.h"
#include "DefaultChannelHandler.h"
#include "Device.h"
#include "InputChannelHandler.h"
#include "Library.h"
#include "Message.h"
#include "ServiceDiscoveryResponse.pb.h"
#include "VideoChannelHandler.h"
#include "utils.h"
#include <boost/range/algorithm.hpp>
#include <cstdint>
#include <iterator>
#include <memory>
#include <mutex>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <pcap/pcap.h>
#include <stdexcept>
#include <unistd.h>
#include <vector>

#define CRT_FILE "headunit.crt"
#define PRIVKEY_FILE "headunit.key"
#define DHPARAM_FILE "dhparam.pem"

using namespace std;

static libusb_device_handle *getHandle(const Device &dev) {
  return libusb_open_device_with_vid_pid(dev.getLibrary().getHandle(),
                                         dev.getVid(), dev.getPid());
}

AaCommunicator::AaCommunicator(const Device &dev,
                               const std::vector<uint8_t> &sd,
                               const std::string &dumpfile)
    : device(dev), serviceDescription(sd),
      deviceHandle(getHandle(dev), [](auto *device) { libusb_close(device); }) {
  initializeSslContext();
  channelHandlers[0] = new DefaultChannelHandler(0);
  channelHandlers[0]->sendToServer.connect(
      [this](uint8_t channelNumber, bool specific,
             const std::vector<uint8_t> &data) {
        channelMessage(channelNumber, specific, data);
      });
  channelHandlers[0]->sendToMobile.connect(
      [this](uint8_t channelNumber, uint8_t flags,
             const std::vector<uint8_t> &data) {
        sendMessage(channelNumber, flags, data);
      });
  cout << "dumpfile: " << dumpfile << endl;

  if (!dumpfile.empty()) {
    pd = pcap_open_dead(DLT_NULL, 65535);
    pdumper = pcap_dump_open(pd, dumpfile.c_str());
  }
}

AaCommunicator::~AaCommunicator() {
  if (pdumper)
    pcap_dump_close(pdumper);
  if (pd)
    pcap_close(pd);
}

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

void AaCommunicator::setup() {
  cout << dec;
  cout << "setup" << endl;
  threads.push_back(std::thread([this]() { AaCommunicator::writeThread(); }));
  sendVersionRequest(1, 1);
  expectVersionResponse();
  cout << "version negotiation ok" << endl;
  auto message = vector<uint8_t>();
  while (!doSslHandshake(message))
    message = getMessage().content;
  cout << "ssl handshake complete" << endl;
  sendAuthComplete();
  int faultCount = 0;
  for (; faultCount < 30;) {
    Message msg;
    try {
      msg = getMessage();
      faultCount = 0;
    } catch (runtime_error &ex) {
      cout << "E: " << ex.what() << endl;
      faultCount++;
      sleep(1);
      continue;
    }
    auto msgType = (msg.content[0] << 8 | msg.content[1]);
    vector<MessageType> forwardedMessageTypes = {
        MessageType::AudioFocusRequest, MessageType::NavigationFocusRequest,
        MessageType::VoiceSessionRequest};
    if (msgType == MessageType::ServiceDiscoveryRequest) {
      handleServiceDiscoveryRequest(msg);
    } else if (msg.channel != 0) {
      handleChannelMessage(msg);
    } else if (boost::range::find(forwardedMessageTypes, msgType) !=
               forwardedMessageTypes.end()) {
      forwardChannelMessage(msg);
    } else {
      throw runtime_error("unexpected message: " + to_string(msg.channel) +
                          " " + to_string(msg.flags) + " " +
                          hexStr(msg.content.data(), msg.content.size()));
    }
  }
}

void AaCommunicator::handleServiceDiscoveryRequest(const Message &msg) {
  vector<uint8_t> message;
  pushBackInt16(message, MessageType::ServiceDiscoveryResponse);
  copy(serviceDescription.begin(), serviceDescription.end(),
       back_inserter(message));

  tag::aas::ServiceDiscoveryResponse sdr;
  sdr.ParseFromArray(serviceDescription.data(), serviceDescription.size());
  for (auto ch : sdr.channels()) {
    if (ch.has_media_channel() &&
        ch.media_channel().media_type() ==
            tag::aas::MediaStreamType_Enum::MediaStreamType_Enum_Video) {
      channelHandlers[ch.channel_id()] = new VideoChannelHandler(
          ch.channel_id(), ch.media_channel().video_configs().size());
    } else if (ch.has_input_channel()) {
      channelHandlers[ch.channel_id()] =
          new InputChannelHandler(ch.channel_id());
    } else {
      channelHandlers[ch.channel_id()] =
          new DefaultChannelHandler(ch.channel_id());
    }
    channelHandlers[ch.channel_id()]->sendToServer.connect(
        [this](uint8_t channelNumber, bool specific,
               const std::vector<uint8_t> &data) {
          channelMessage(channelNumber, specific, data);
        });
    channelHandlers[ch.channel_id()]->sendToMobile.connect(
        [this](uint8_t channelNumber, uint8_t flags,
               const std::vector<uint8_t> &data) {
          sendMessage(channelNumber, flags, data);
        });
  }
  sendMessage(0,
              EncryptionType::Encrypted | FrameType::Bulk |
                  MessageTypeFlags::Control,
              message);
}

void AaCommunicator::handleChannelMessage(const Message &msg) {
  channelHandlers[msg.channel]->handleMessageFromMobile(msg.channel, msg.flags,
                                                        msg.content);
}

void AaCommunicator::forwardChannelMessage(const Message &msg) {
  channelMessage(msg.channel, msg.flags & MessageTypeFlags::Specific,
                 msg.content);
}

void AaCommunicator::sendAuthComplete() {
  vector<uint8_t> msg;
  pushBackInt16(msg, MessageType::AuthComplete);
  msg.push_back(0x08);
  msg.push_back(0x00);
  sendMessage(
      0, FrameType::Bulk | EncryptionType::Plain | MessageTypeFlags::Control,
      msg);
}

bool AaCommunicator::doSslHandshake(const vector<uint8_t> msgContent) {
  initializeSsl();
  if (msgContent.size() > 2) {
    if ((msgContent[0] << 8 | msgContent[1]) != MessageType::SslHandshake)
      throw runtime_error("Expected SSL handshake message");
    BIO_write(readBio, msgContent.data() + 2, msgContent.size() - 2);
  }
  auto ret = SSL_connect(ssl);
  //  cout << "ret=" << ret << endl;
  if (ret == 1)
    return true;
  else if (ret == -1) {
    auto error = SSL_get_error(ssl, ret);
    ERR_print_errors_fp(stdout);
    if (error != SSL_ERROR_WANT_READ)
      throw runtime_error("SSL_connect failed");
  } else {
    throw runtime_error("Unknown SSL_connect failure");
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
  // cout << "send=" << msg.size() << endl;
  return false;
}

void AaCommunicator::initializeSsl() {
  if (ssl)
    return;
  ssl = SSL_new(ctx);
  readBio = BIO_new(BIO_s_mem());
  writeBio = BIO_new(BIO_s_mem());
  SSL_set_connect_state(ssl);
  SSL_set_bio(ssl, readBio, writeBio);
}

void AaCommunicator::initializeSslContext() {
  SSL_load_error_strings();
  OpenSSL_add_ssl_algorithms();
  const SSL_METHOD *method = SSLv23_client_method();

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

void AaCommunicator::sendVersionRequest(uint16_t major, uint16_t minor) {
  std::vector<uint8_t> buf;
  pushBackInt16(buf, MessageType::VersionRequest);
  pushBackInt16(buf, major);
  pushBackInt16(buf, minor);
  sendMessage(
      0, EncryptionType::Plain | FrameType::Bulk | MessageTypeFlags::Control,
      buf);
}

void AaCommunicator::expectVersionResponse() {
  auto msg = getMessage();
  if (msg.content.size() != 8 ||
      (msg.content[0] << 8 | msg.content[1]) != MessageType::VersionResponse ||
      msg.content[6] != 0 || msg.content[7] != 0)
    throw runtime_error("did not get version response");
}

void AaCommunicator::sendMessagePublic(uint8_t channel, bool specific,
                                       const std::vector<uint8_t> &buf) {
  channelHandlers[channel]->handleMessageFromServer(channel, specific, buf);
}

void AaCommunicator::sendMessage(uint8_t channel, uint8_t flags,
                                 const std::vector<uint8_t> &buf) {
  Message msg;
  msg.channel = channel;
  msg.flags = flags;
  msg.content = buf;
  logMessage(msg, false);
  {
    std::unique_lock<std::mutex> lk(sendQueueMutex);
    sendQueue.push_back(msg);
  }
  sendQueueNotEmpty.notify_all();
}

vector<uint8_t> AaCommunicator::prepareMessage() {
  vector<uint8_t> buffer;
  std::unique_lock<std::mutex> lk(sendQueueMutex);
  if (!sendQueueNotEmpty.wait_for(lk, 1s, [=] { return !sendQueue.empty(); })) {
    return buffer;
  }

  int maxSize = 10000;

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
    uint8_t encBuf[20000];
    auto offset = 4;
    if ((flags & FrameType::Bulk) == FrameType::First) {
      offset += 4;
    }
    auto length = BIO_read(writeBio, encBuf + offset, 20000);
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
    return vector<uint8_t>(encBuf, encBuf + length + offset);
  } else {
    sendQueue.pop_front();
    msgBytes.push_back(msg.channel);
    msgBytes.push_back(msg.flags);
    int length = msg.content.size();
    pushBackInt16(msgBytes, length);
    std::copy(msg.content.begin(), msg.content.end(),
              std::back_inserter(msgBytes));
    // std::copy(msgBytes.begin(), msgBytes.end(), (uint8_t *)buf);
    return msgBytes;
  }
}

void AaCommunicator::writeThread() {
  while (true) {
    auto msg = prepareMessage();
    if (msg.empty())
      continue;
    try {
      writeToUsb(msg);
    } catch (exception &ex) {
      sleep(1);
      writeToUsb(msg);
    }
  }
}

void AaCommunicator::writeToUsb(const vector<uint8_t> &buffer) {
  int transferred;
  int result;
  if ((result = libusb_bulk_transfer(deviceHandle.get(), 0x01,
                                     (uint8_t *)buffer.data(), buffer.size(),
                                     &transferred, 1000)) != 0 ||
      transferred != buffer.size())
    throw runtime_error("out transfer failed: " + to_string(result));
}
/*
Message AaCommunicator::getMessage() {
  int bufSize = 10000;
  uint8_t buffer[bufSize];
  int transferred;
  int result;
  vector<uint8_t> fullContent;
  int totalLength = 0;
  uint8_t channel;
  uint8_t flags;
  cout << "gM1" << endl;
  do {
    cout << "gM2" << endl;
    if ((result = libusb_bulk_transfer(deviceHandle.get(), 0x81, buffer,
                                       bufSize, &transferred, 5000)) != 0 ||
        transferred < 2)
      throw runtime_error("in transfer failed: " + to_string(result));
    channel = buffer[0];
    flags = buffer[1];
    int length = (buffer[2] << 8 | buffer[3]);
    int offset = 4;
    if ((flags & FrameType::Bulk) == FrameType::First) {
      totalLength =
          (buffer[4] << 24 | buffer[5] << 16 | buffer[6] << 8 | buffer[7]);
      offset += 4;
    }
    cout << "gM3: " << channel << " " << (flags & FrameType::Bulk) << " "
         << length << " " << totalLength << endl;
    auto content = vector<uint8_t>(buffer + offset, buffer + transferred);
    cout << "gM4: " << content.size() << endl;
    if (length != content.size()) {
      throw runtime_error("wrong length");
    }
    if (flags & EncryptionType::Encrypted) {
      content = decryptMessage(content);
    }
    copy(content.begin(), content.end(), back_inserter(fullContent));
    cout << "gM5: " << fullContent.size() << endl;
  } while (fullContent.size() < totalLength);

  Message msg;
  msg.channel = channel;
  msg.flags = flags;
  msg.content = fullContent;
  return msg;
}
*/
Message AaCommunicator::getMessage() {
  int bufSize = 100000;
  uint8_t buffer[bufSize];
  int transferred;
  int result;
  vector<uint8_t> fullContent;
  int totalLength = 0;
  Message msg;
  msg.flags = 0;
  do {
    if ((result = libusb_bulk_transfer(deviceHandle.get(), 0x81, buffer,
                                       bufSize, &transferred, 5000)) != 0 ||
        transferred < 2)
      throw runtime_error("in transfer failed: " + to_string(result));
    msg.channel = buffer[0];
    msg.flags = buffer[1];
    int length = (buffer[2] << 8 | buffer[3]);
    int offset = 4;
    if ((msg.flags & FrameType::Bulk) == FrameType::First) {
      totalLength =
          (buffer[4] << 24 | buffer[5] << 16 | buffer[6] << 8 | buffer[7]);
      offset += 4;
    }
    auto content = vector<uint8_t>(buffer + offset, buffer + transferred);
    if (length != content.size()) {
      cout << (msg.flags & FrameType::Bulk) << " " << length << " "
           << content.size() << endl;
      throw runtime_error("wrong length");
    }
    if (msg.flags & EncryptionType::Encrypted) {
      content = decryptMessage(content);
    }
    copy(content.begin(), content.end(), back_inserter(fullContent));
  } while (fullContent.size() < totalLength);
  msg.content = fullContent;
  msg.flags |= FrameType::Bulk;
  // if (totalLength != 0)
  // cout << "totalLength: " << totalLength << " " << msg.content.size() <<
  // endl;
  logMessage(msg, true);
  return msg;
}

std::vector<uint8_t>
AaCommunicator::decryptMessage(const std::vector<uint8_t> &encryptedMsg) {
  ERR_clear_error();

  auto ret = BIO_write(readBio, encryptedMsg.data(), encryptedMsg.size());
  if (ret < 0) {
    throw std::runtime_error("BIO_write failed");
  }
  char plainBuf[20000];
  ret = SSL_read(ssl, plainBuf, 20000);
  if (ret < 0) {
    auto err = SSL_get_error(ssl, ret);
    auto message = "SSL_read failed: " + std::to_string(ret);
    message += " " + std::to_string(err);
    if (err == SSL_ERROR_SSL)
      message += " "s + ERR_error_string(ERR_get_error(), NULL);
    throw std::runtime_error(message);
  }
  return std::vector<uint8_t>(plainBuf, plainBuf + ret);
}
