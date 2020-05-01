// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "VideoChannelHandler.h"
#include "enums.h"

VideoChannelHandler::VideoChannelHandler(uint8_t channelId)
    : ChannelHandler(channelId) {}

VideoChannelHandler::~VideoChannelHandler() {}

bool VideoChannelHandler::handleMessageFromMobile(
    uint8_t channelId, uint8_t flags, const std::vector<uint8_t> &data) {
  sendToServer(channelId, flags & MessageTypeFlags::Specific, data);
  return true;
}

bool VideoChannelHandler::handleMessageFromServer(
    uint8_t channelId, bool specific, const std::vector<uint8_t> &data) {
  uint8_t flags = EncryptionType::Encrypted | FrameType::Bulk;
  if (specific) {
    flags |= MessageTypeFlags::Specific;
  }
  sendToMobile(channelId, flags, data);
  return true;
}
