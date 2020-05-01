// Distributed under GPLv3 only as specified in repository's root LICENSE file

#pragma once

#include "ChannelHandler.h"

class VideoChannelHandler : public ChannelHandler {
  bool gotSetupResponse;
  std::mutex m;
  std::condition_variable cv;
  bool channelOpened;

  void sendSetupRequest();
  void expectSetupResponse();
  void sendStartIndication();
  void openChannel();

public:
  VideoChannelHandler(uint8_t channelId);
  virtual void disconnected();
  virtual bool handleMessageFromHeadunit(const Message &message);
  virtual bool handleMessageFromClient(uint8_t channelId, bool specific,
                                       const std::vector<uint8_t> &data);
  virtual ~VideoChannelHandler();
};
