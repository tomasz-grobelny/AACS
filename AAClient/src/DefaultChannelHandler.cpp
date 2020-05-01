// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "DefaultChannelHandler.h"
#include "enums.h"

DefaultChannelHandler::DefaultChannelHandler(uint8_t channelId)
    : ChannelHandler(channelId) {}

DefaultChannelHandler::~DefaultChannelHandler() {}

bool DefaultChannelHandler::handleMessageFromMobile(
    uint8_t channelId, uint8_t flags, const std::vector<uint8_t> &data) {
  sendToServer(channelId, flags & MessageTypeFlags::Specific, data);
  return true;
}

bool DefaultChannelHandler::handleMessageFromServer(
    uint8_t channelId, bool specific, const std::vector<uint8_t> &data) {
  uint8_t flags = EncryptionType::Encrypted | FrameType::Bulk;
  if (specific) {
    flags |= MessageTypeFlags::Specific;
  }
  sendToMobile(channelId, flags, data);
  return true;
}
