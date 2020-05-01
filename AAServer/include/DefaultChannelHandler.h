// Distributed under GPLv3 only as specified in repository's root LICENSE file

#pragma once

#include "ChannelHandler.h"
#include <vector>

class DefaultChannelHandler : public ChannelHandler {
public:
  DefaultChannelHandler(uint8_t channelId);
  virtual bool handleMessageFromHeadunit(const Message &message);
  virtual bool handleMessageFromClient(uint8_t channelId, bool specific,
                                       const std::vector<uint8_t> &data);
  virtual ~DefaultChannelHandler();
};
