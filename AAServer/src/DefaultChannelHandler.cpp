// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "DefaultChannelHandler.h"
#include <iostream>

using namespace std;

DefaultChannelHandler::DefaultChannelHandler(uint8_t channelId)
    : ChannelHandler(channelId) {
  cout << "DefaultChannelHandler: " << (int)channelId << endl;
}
DefaultChannelHandler::~DefaultChannelHandler() {}

void DefaultChannelHandler::openChannel() {}
void DefaultChannelHandler::closeChannel() {}

bool DefaultChannelHandler::handleMessageFromHeadunit(const Message &message) {
  ChannelHandler::sendToClient(message.channel,
                               message.flags & MessageTypeFlags::Specific,
                               message.content);
  return true;
}

bool DefaultChannelHandler::handleMessageFromClient(
    uint8_t channelId, bool specific, const vector<uint8_t> &data) {
  uint8_t flags = EncryptionType::Encrypted | FrameType::Bulk;
  if (specific) {
    flags |= MessageTypeFlags::Specific;
  }
  sendToHeadunit(channelId, flags, data);
  return true;
}
