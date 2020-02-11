// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "AaCommunicator.h"
#include "Configuration.h"
#include "FfsFunction.h"
#include "Udc.h"
#include "descriptors.h"
#include "utils.h"
#include <boost/filesystem.hpp>
#include <boost/signals2.hpp>
#include <fcntl.h>
#include <functionfs.h>
#include <iostream>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdexcept>

#define CRT_FILE "self_sign.crt"
#define PRIVKEY_FILE "self_sign.key"
#define DHPARAM_FILE "dhparam.pem"

using namespace std;
using namespace boost::filesystem;

void AaCommunicator::sendMessage(__u8 channel, __u8 flags,
                                 const std::vector<__u8> &buf) {
  Message msg;
  msg.channel = channel;
  msg.flags = flags;
  msg.content = buf;
  {
    std::unique_lock<std::mutex> lk(sendQueueMutex);
    sendQueue.push_back(msg);
  }
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
  } else if (messageType == MessageType::SslHandshake) {
    handleSslHandshake(shortView + 1, msg.size() - sizeof(__u16));
  } else {
    throw std::runtime_error("Unhandled message type: " +
                             std::to_string(messageType));
  }
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

  std::vector<__u8> msg;
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
  if (msg.flags & EncryptionType::Encrypted) {
    throw runtime_error("Encrypted messaged not yet supported");
  } else {
    sendQueue.pop_front();
    msgBytes.push_back(msg.channel);
    msgBytes.push_back(msg.flags);
    int length = msg.content.size();
    pushBackInt16(msgBytes, length);
    std::copy(msg.content.begin(), msg.content.end(),
              std::back_inserter(msgBytes));
    std::copy(msgBytes.begin(), msgBytes.end(), (__u8 *)buf);
  }
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

AaCommunicator::AaCommunicator(const Library &_lib) : lib(_lib) {
  initializeSslContext();
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
