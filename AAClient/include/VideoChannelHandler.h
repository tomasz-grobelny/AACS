// Distributed under GPLv3 only as specified in repository's root LICENSE file

#pragma once

#include "ChannelHandler.h"
#include <gst/gstelement.h>

class VideoChannelHandler : public ChannelHandler {
  void sendChannelOpenResponse();
  void sendSetupResponse();
  void sendVideoFocusIndication();
  void sendAck();

  GstElement *pipeline;
  GstElement* app_source;
  void createAppSource();
  void pushDataToPipeline(uint64_t ts, const std::vector<uint8_t>& data);
  uint64_t startTimestamp;
  uint8_t numVideoConfigs;

public:
  VideoChannelHandler(uint8_t channelId, uint8_t numConfigs);
  virtual ~VideoChannelHandler();
  virtual bool handleMessageFromMobile(uint8_t channelId, uint8_t flags,
                                       const std::vector<uint8_t> &data);
  virtual bool handleMessageFromServer(uint8_t channelId, bool specific,
                                       const std::vector<uint8_t> &data);
};
