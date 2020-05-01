// Distributed under GPLv3 only as specified in repository's root LICENSE file

#pragma once

#include <boost/signals2.hpp>
#include <vector>

class ChannelHandler {
protected:
  uint8_t channelId;

public:
  ChannelHandler(uint8_t channelId);
  virtual bool handleMessageFromMobile(uint8_t channelId, uint8_t flags,
                                       const std::vector<uint8_t> &data) = 0;
  virtual bool handleMessageFromServer(uint8_t channelId, bool specific,
                                       const std::vector<uint8_t> &data) = 0;
  boost::signals2::signal<void(uint8_t channelNumber, bool specific,
                               std::vector<uint8_t> data)>
      sendToServer;
  boost::signals2::signal<void(uint8_t channelNumber, uint8_t flags,
                               std::vector<uint8_t> data)>
      sendToMobile;

  virtual ~ChannelHandler() = 0;
};
