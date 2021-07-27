// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "AudioChannelHandler.h"
#include "ChannelHandler.h"
#include "MediaChannelSetupRequest.pb.h"
#include "MediaChannelSetupResponse.pb.h"
#include "MediaChannelStartIndication.pb.h"
#include "utils.h"
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ip/address.hpp>

using namespace std;
using namespace tag::aas;

AudioChannelHandler::AudioChannelHandler(uint8_t channelId)
    : ChannelHandler(channelId) {
  cout << "AudioChannelHandler: " << (int)channelId << endl;
  channelOpened = false;

  th = std::thread([this]() {
    openChannel();
    boost::asio::io_service ios;
    boost::asio::ip::tcp::endpoint endpoint(
        boost::asio::ip::address::from_string("127.0.0.1"), 9999);
    boost::asio::ip::tcp::socket socket(ios);
    socket.connect(endpoint);

    string message = "audio sink ctr isaudio 1\n";
    boost::array<char, 8192> buf;
    std::copy(message.begin(), message.end(), buf.begin());
    boost::system::error_code error;
    boost::asio::streambuf b;
    cout << "ach read: " << boost::asio::read_until(socket, b, '\n')
         << endl;
    cout << "ach write: " << message.size() << endl;
    socket.write_some(boost::asio::buffer(buf, message.size()), error);
    uint64_t startTimestamp = 0ll;
    const int samplesPerBuffer = 2048;
    for (auto i = 0;; i++) {
      auto len = boost::asio::read(
          socket, boost::asio::buffer(buf, samplesPerBuffer * 2 * 2), error);
      cout << "." << flush;
      if (error)
        break;

      std::vector<uint8_t> plainMsg;
      pushBackInt16(plainMsg, MediaMessageType::MediaWithTimestampIndication);
      pushBackInt64(plainMsg, startTimestamp + i * 1000 * samplesPerBuffer / 48);
      std::copy(buf.begin(), buf.end(), back_inserter(plainMsg));
      sendToHeadunit(this->channelId,
                     FrameType::Bulk | EncryptionType::Encrypted, plainMsg);
    }
    socket.close();
  });
}

AudioChannelHandler::~AudioChannelHandler() {}

void AudioChannelHandler::openChannel() {
  channelOpened = true;
  ChannelHandler::openChannel();
  gotSetupResponse = false;
  sendSetupRequest();
  expectSetupResponse();
  sendStartIndication();
}

void AudioChannelHandler::disconnected(int clientId) {}

void AudioChannelHandler::sendSetupRequest() {
  std::vector<uint8_t> plainMsg;
  pushBackInt16(plainMsg, MediaMessageType::SetupRequest);
  MediaChannelSetupRequest mcsr;
  mcsr.set_stream_type(MediaStreamType_Enum::MediaStreamType_Enum_Audio);
  const auto &msgString = mcsr.SerializeAsString();
  std::copy(msgString.begin(), msgString.end(), back_inserter(plainMsg));
  sendToHeadunit(channelId, FrameType::Bulk | EncryptionType::Encrypted,
                 plainMsg);
}

void AudioChannelHandler::expectSetupResponse() {
  std::unique_lock<std::mutex> lk(m);
  cv.wait(lk, [=] { return gotSetupResponse; });
}

void AudioChannelHandler::sendStartIndication() {
  std::vector<uint8_t> plainMsg;
  pushBackInt16(plainMsg, MediaMessageType::StartIndication);
  MediaChannelStartIndication mcsi;
  mcsi.set_session(0);
  mcsi.set_config(0);
  const auto &msgString = mcsi.SerializeAsString();
  std::copy(msgString.begin(), msgString.end(), back_inserter(plainMsg));
  sendToHeadunit(channelId, FrameType::Bulk | EncryptionType::Encrypted,
                 plainMsg);
}

bool AudioChannelHandler::handleMessageFromHeadunit(const Message &message) {
  if (!channelOpened) {
    ChannelHandler::sendToClient(-1, message.channel,
                                 message.flags & MessageTypeFlags::Specific,
                                 message.content);
    return true;
  }
  if (ChannelHandler::handleMessageFromHeadunit(message))
    return true;
  bool messageHandled = false;
  {
    std::unique_lock<std::mutex> lk(m);
    auto msg = message.content;
    const __u16 *shortView = (const __u16 *)(msg.data());
    auto messageType = be16_to_cpu(shortView[0]);
    if (messageType == MediaMessageType::SetupResponse) {
      gotSetupResponse = true;
      messageHandled = true;
    } else if (messageType == MediaMessageType::MediaAckIndication) {
      messageHandled = true;
    }
  }
  cv.notify_all();
  return messageHandled;
}

bool AudioChannelHandler::handleMessageFromClient(int clientId,
                                                  uint8_t channelId,
                                                  bool specific,
                                                  const vector<uint8_t> &data) {
  // Audio is routed through snowmix
  return false;
}
