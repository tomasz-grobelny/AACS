// Distributed under GPLv3 only as specified in repository's root LICENSE file

#pragma once

#include "ChannelHandler.h"

class VideoChannelHandler : public ChannelHandler {
public:
  VideoChannelHandler(uint8_t channelId);
  virtual ~VideoChannelHandler();
  virtual bool handleMessageFromMobile(uint8_t channelId, uint8_t flags,
                                       const std::vector<uint8_t> &data);
  virtual bool handleMessageFromServer(uint8_t channelId, bool specific,
                                       const std::vector<uint8_t> &data);
};
