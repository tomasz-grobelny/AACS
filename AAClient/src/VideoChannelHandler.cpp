// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "VideoChannelHandler.h"
#include "enums.h"
#include <boost/mpl/back_inserter.hpp>

VideoChannelHandler::VideoChannelHandler(uint8_t channelId)
    : ChannelHandler(channelId) {}

VideoChannelHandler::~VideoChannelHandler() {}

bool VideoChannelHandler::handleMessageFromMobile(
    uint8_t channelId, uint8_t flags, const std::vector<uint8_t> &data) {
  std::vector<uint8_t> msgToServer;
  msgToServer.push_back(0x00); //raw data
  copy(data.begin(), data.end(), back_inserter(msgToServer));
  sendToServer(channelId, flags & MessageTypeFlags::Specific, msgToServer);
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
