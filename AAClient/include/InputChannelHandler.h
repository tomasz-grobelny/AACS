// Distributed under GPLv3 only as specified in repository's root LICENSE file

#pragma once

#include "ChannelHandler.h"

class InputChannelHandler : public ChannelHandler {
  void sendChannelOpenResponse();
  void sendHandshakeResponse();

public:
  InputChannelHandler(uint8_t channelId);
  virtual ~InputChannelHandler();
  virtual bool handleMessageFromMobile(uint8_t channelId, uint8_t flags,
                                       const std::vector<uint8_t> &data);
  virtual bool handleMessageFromServer(uint8_t channelId, bool specific,
                                       const std::vector<uint8_t> &data);
};
