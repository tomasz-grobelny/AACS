// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "ChannelHandler.h"
#include "ChannelOpenRequest.pb.h"
#include "enums.h"
#include "utils.h"

ChannelHandler::ChannelHandler(uint8_t _channelId) : channelId(_channelId) {}
ChannelHandler::~ChannelHandler() {}

void ChannelHandler::openChannel() {
  gotChannelOpenResponse = false;
  sendChannelOpenRequest();
  expectChannelOpenResponse();
}

bool ChannelHandler::handleMessageFromHeadunit(const Message &message) {
  bool messageHandled = false;
  {
    std::unique_lock<std::mutex> lk(m);
    auto msg = message.content;
    const __u16 *shortView = (const __u16 *)(msg.data());
    auto messageType = be16_to_cpu(shortView[0]);
    if (messageType == MessageType::ChannelOpenResponse) {
      gotChannelOpenResponse = true;
      messageHandled = true;
    }
  }
  cv.notify_all();
  return messageHandled;
}

void ChannelHandler::sendChannelOpenRequest() {
  tag::aas::ChannelOpenRequest cor;
  cor.set_channel_id(channelId);
  cor.set_unknown_field(0);
  const auto &msgString = cor.SerializeAsString();
  std::vector<uint8_t> plainMsg;
  pushBackInt16(plainMsg, MessageType::ChannelOpenRequest);
  std::copy(msgString.begin(), msgString.end(), std::back_inserter(plainMsg));
  sendToHeadunit(channelId,
                 FrameType::Bulk | EncryptionType::Encrypted |
                     MessageTypeFlags::Specific,
                 plainMsg);
}

void ChannelHandler::expectChannelOpenResponse() {
  std::unique_lock<std::mutex> lk(m);
  cv.wait(lk, [=] { return gotChannelOpenResponse; });
}
