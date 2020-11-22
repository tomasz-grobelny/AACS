// Distributed under GPLv3 only as specified in repository's root LICENSE file

#pragma once

#include "ChannelHandler.h"
#include <gst/gst.h>

class VideoChannelHandler : public ChannelHandler {
  class PerClientData {
  public:
    uint8_t priority;
    uint64_t startTimestamp;
    GstElement *app_source;
    GstPad *sink_request_pad;
    GstPad *src_static_pad;
  };
  std::map<int, PerClientData> clientData;

  bool gotSetupResponse;
  std::mutex m;
  std::condition_variable cv;
  bool channelOpened;

  void sendSetupRequest();
  void expectSetupResponse();
  void sendStartIndication();

  GstElement *inputSelector;
  GstElement *pipeline;
  GstPad *sink_request_pad;
  void pushDataToPipeline(int clientId, uint64_t ts,
                          const std::vector<uint8_t> &data);

  void createAppSource(int clientId, uint8_t priority);
  void createRawAppSource(int clientId, uint8_t priority);
  static GstFlowReturn new_sample(GstElement *sink, VideoChannelHandler *_this);
  void openChannel();

public:
  VideoChannelHandler(uint8_t channelId);
  virtual void disconnected(int clientId);
  virtual bool handleMessageFromHeadunit(const Message &message);
  virtual bool handleMessageFromClient(int clientId, uint8_t channelId,
                                       bool specific,
                                       const std::vector<uint8_t> &data);
  virtual ~VideoChannelHandler();
};
