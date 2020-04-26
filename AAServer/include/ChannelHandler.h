// Distributed under GPLv3 only as specified in repository's root LICENSE file

#pragma once

#include "Message.h"
#include <boost/signals2.hpp>
#include <condition_variable>
#include <mutex>
#include <vector>

class ChannelHandler {
  bool gotChannelOpenResponse;
  void sendChannelOpenRequest();
  void expectChannelOpenResponse();
  std::mutex m;
  std::condition_variable cv;

protected:
  uint8_t channelId;

public:
  ChannelHandler(uint8_t channelId);
  virtual void openChannel() = 0;
  virtual void closeChannel() = 0;
  virtual bool handleMessageFromHeadunit(const Message &message) = 0;
  virtual bool handleMessageFromClient(uint8_t channelId, bool specific,
                                       const std::vector<uint8_t> &data) = 0;
  boost::signals2::signal<void(uint8_t channelNumber, bool specific,
                               std::vector<uint8_t> data)>
      sendToClient;
  boost::signals2::signal<void(uint8_t channelNumber, uint8_t flags,
                               std::vector<uint8_t> data)>
      sendToHeadunit;

  virtual ~ChannelHandler();
};
