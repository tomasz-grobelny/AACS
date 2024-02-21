// Distributed under GPLv3 only as specified in repository's root LICENSE file

#pragma once

#include "ChannelHandler.h"
#include <set>
#include <vector>

class InputChannelHandler : public ChannelHandler {
  void sendHandshakeRequest();
  void expectHandshakeResponse();
  bool gotHandshakeResponse;
  std::mutex m;
  std::condition_variable cv;
  std::set<int> registered_clients;
  std::vector<int> available_buttons;

public:
  InputChannelHandler(uint8_t channelId, std::vector<int> available_buttons);
  virtual void disconnected(int clientId) override;
  virtual bool handleMessageFromHeadunit(const Message &message) override;
  virtual bool handleMessageFromClient(int clientId, uint8_t channelId, bool specific,
                          const std::vector<uint8_t> &data) override;
  virtual ~InputChannelHandler();
};
