// Distributed under GPLv3 only as specified in repository's root LICENSE file

#pragma once

#include "ChannelHandler.h"
#include <gst/gst.h>
#include <thread>

class AudioChannelHandler : public ChannelHandler {
  bool gotSetupResponse;
  std::mutex m;
  std::condition_variable cv;
  bool channelOpened;

  void sendSetupRequest();
  void expectSetupResponse();
  void sendStartIndication();

  void openChannel();
  std::thread th;

public:
  AudioChannelHandler(uint8_t channelId);
  virtual void disconnected(int clientId);
  virtual bool handleMessageFromHeadunit(const Message &message);
  virtual bool handleMessageFromClient(int clientId, uint8_t channelId,
                                       bool specific,
                                       const std::vector<uint8_t> &data);
  virtual ~AudioChannelHandler();
};
