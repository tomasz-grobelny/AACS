// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "ChannelHandler.h"

ChannelHandler::ChannelHandler(uint8_t _channelId) : channelId(_channelId) {}

ChannelHandler::~ChannelHandler() {}

bool ChannelHandler::handleMessageFromMobile(
    uint8_t channelId, uint8_t flags, const std::vector<uint8_t> &data) {
  return false;
}

bool ChannelHandler::handleMessageFromServer(
    uint8_t channelId, bool specific, const std::vector<uint8_t> &data) {
  return false;
}
