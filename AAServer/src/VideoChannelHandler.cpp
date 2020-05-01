// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "VideoChannelHandler.h"
#include "ChannelHandler.h"
#include "utils.h"
#include <iostream>

using namespace std;

VideoChannelHandler::VideoChannelHandler(uint8_t channelId)
    : ChannelHandler(channelId) {
  cout << "VideoChannelHandler: " << (int)channelId << endl;
  channelOpened = false;
}

VideoChannelHandler::~VideoChannelHandler() {}

void VideoChannelHandler::openChannel() {
  channelOpened = true;
  ChannelHandler::openChannel();
  gotSetupResponse = false;
  sendSetupRequest();
  expectSetupResponse();
  sendStartIndication();
}

void VideoChannelHandler::disconnected(int clientId) { channelOpened = false; }

void VideoChannelHandler::sendSetupRequest() {
  std::vector<uint8_t> plainMsg;
  pushBackInt16(plainMsg, MediaMessageType::SetupRequest);
  plainMsg.push_back(0x08);
  plainMsg.push_back(0x03);
  sendToHeadunit(channelId, FrameType::Bulk | EncryptionType::Encrypted,
                 plainMsg);
}

void VideoChannelHandler::expectSetupResponse() {
  std::unique_lock<std::mutex> lk(m);
  cv.wait(lk, [=] { return gotSetupResponse; });
}

void VideoChannelHandler::sendStartIndication() {
  std::vector<uint8_t> plainMsg;
  pushBackInt16(plainMsg, MediaMessageType::StartIndication);
  plainMsg.push_back(0x08);
  plainMsg.push_back(0x00);
  plainMsg.push_back(0x10);
  plainMsg.push_back(0x00);
  sendToHeadunit(channelId, FrameType::Bulk | EncryptionType::Encrypted,
                 plainMsg);
}

bool VideoChannelHandler::handleMessageFromHeadunit(const Message &message) {
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
    } else if (messageType == MediaMessageType::VideoFocusIndication) {
      messageHandled = true;
    } else if (messageType == MediaMessageType::MediaAckIndication) {
      messageHandled = true;
    }
  }
  cv.notify_all();
  return messageHandled;
}

bool VideoChannelHandler::handleMessageFromClient(int clientId,
                                                  uint8_t channelId,
                                                  bool specific,
                                                  const vector<uint8_t> &data) {
  uint8_t flags = EncryptionType::Encrypted | FrameType::Bulk;
  if (specific) {
    flags |= MessageTypeFlags::Specific;
  }
  auto msgType = data[0];
  if (msgType == 0x00) {
    // raw data
    vector<uint8_t> msgToHeadunit;
    copy(data.begin() + 1, data.end(), back_inserter(msgToHeadunit));
    sendToHeadunit(channelId, flags, msgToHeadunit);
    return true;
  } else if (msgType == 0x01) {
    openChannel();
    sendToClient(clientId, channelId, false, {0xff});
    return true;
  } else {
    return false;
  }
}
