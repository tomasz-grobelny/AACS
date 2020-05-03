// Distributed under GPLv3 only as specified in repository's root LICENSE file

#pragma once

#include "ChannelHandler.h"

class VideoChannelHandler : public ChannelHandler {
  void sendChannelOpenResponse();
  void sendSetupResponse();
  void sendVideoFocusIndication();
  void sendAck();

  void sendAvMediaIndication(const std::vector<uint8_t> &data);
  void sendAvMediaIndicationWithTimestamp(const std::vector<uint8_t> &data);

public:
  VideoChannelHandler(uint8_t channelId);
  virtual ~VideoChannelHandler();
  virtual bool handleMessageFromMobile(uint8_t channelId, uint8_t flags,
                                       const std::vector<uint8_t> &data);
  virtual bool handleMessageFromServer(uint8_t channelId, bool specific,
                                       const std::vector<uint8_t> &data);
};
