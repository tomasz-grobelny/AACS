// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "InputChannelHandler.h"
#include "enums.h"
#include "utils.h"

using namespace std;

InputChannelHandler::InputChannelHandler(uint8_t channelId)
    : ChannelHandler(channelId) {}

InputChannelHandler::~InputChannelHandler() {}

bool InputChannelHandler::handleMessageFromMobile(
    uint8_t channelId, uint8_t flags, const std::vector<uint8_t> &data) {
  const uint16_t *shortView = (const uint16_t *)(data.data());
  auto msgType = be16toh(shortView[0]);
  if (msgType == MessageType::ChannelOpenRequest) {
    sendChannelOpenResponse();
    sendToServer(channelId, false, {});
    return true;
  } else if (msgType == InputChannelMessageType::HandshakeRequest) {
    sendHandshakeResponse();
    return true;
  }
  return false;
}

bool InputChannelHandler::handleMessageFromServer(
    uint8_t channelId, bool specific, const std::vector<uint8_t> &data) {
  uint8_t flags = EncryptionType::Encrypted | FrameType::Bulk;
  if (specific) {
    flags |= MessageTypeFlags::Specific;
  }
  sendToMobile(channelId, flags, data);
  return true;
}

void InputChannelHandler::sendChannelOpenResponse() {
  vector<uint8_t> msg;
  pushBackInt16(msg, MessageType::ChannelOpenResponse);
  msg.push_back(0x08);
  msg.push_back(0x00);
  sendToMobile(channelId,
               EncryptionType::Encrypted | FrameType::Bulk |
                   MessageTypeFlags::Specific,
               msg);
}

void InputChannelHandler::sendHandshakeResponse() {
  vector<uint8_t> msg;
  pushBackInt16(msg, InputChannelMessageType::HandshakeResponse);
  msg.push_back(0x08);
  msg.push_back(0x00);
  sendToMobile(channelId,
               EncryptionType::Encrypted | FrameType::Bulk |
                   MessageTypeFlags::Control,
               msg);
}
