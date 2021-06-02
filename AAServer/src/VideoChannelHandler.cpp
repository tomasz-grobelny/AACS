// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "VideoChannelHandler.h"
#include "ChannelHandler.h"
#include "enums.h"
#include "utils.h"
#include <boost/range/algorithm/max_element.hpp>
#include <gst/gstelement.h>
#include <gst/gstmemory.h>
#include <gst/gstpad.h>
#include <gst/gstutils.h>
#include <iostream>

#include <linux/types.h>
#include <thread>

using namespace std;

GstFlowReturn VideoChannelHandler::new_sample(GstElement *sink,
                                              VideoChannelHandler *_this) {
  static bool firstSample = true;
  GstSample *sample;
  g_signal_emit_by_name(sink, "pull-sample", &sample);
  if (!sample) {
    cout << "NOSAMPLE" << endl;
    return GST_FLOW_ERROR;
  }
  auto buffer = gst_sample_get_buffer(sample);

  vector<uint8_t> msgToHeadunit;
  GstMapInfo map;
  gst_buffer_map(buffer, &map, GST_MAP_READ);
  if (firstSample) {
    firstSample = false;
    pushBackInt16(msgToHeadunit, MediaMessageType::MediaIndication);
    auto needle06 = {0x00, 0x00, 0x01, 0x06};
    auto start06 = std::search(map.data, map.data + map.size, begin(needle06),
                               end(needle06));
    auto needle65 = {0x00, 0x00, 0x01, 0x65};
    auto start65 =
        search(map.data, map.data + map.size, needle65.begin(), needle65.end());
    copy(map.data, start06, back_inserter(msgToHeadunit));
    _this->sendToHeadunit(_this->channelId,
                          EncryptionType::Encrypted | FrameType::Bulk,
                          msgToHeadunit);
    msgToHeadunit.clear();
    pushBackInt16(msgToHeadunit,
                  MediaMessageType::MediaWithTimestampIndication);
    pushBackInt64(msgToHeadunit, buffer->pts / 1000);
    copy(start65, map.data + map.size, back_inserter(msgToHeadunit));
    _this->sendToHeadunit(_this->channelId,
                          EncryptionType::Encrypted | FrameType::Bulk,
                          msgToHeadunit);
  } else {
    pushBackInt16(msgToHeadunit,
                  MediaMessageType::MediaWithTimestampIndication);
    pushBackInt64(msgToHeadunit, buffer->pts / 1000);
    copy(map.data, map.data + map.size, back_inserter(msgToHeadunit));
    _this->sendToHeadunit(_this->channelId,
                          EncryptionType::Encrypted | FrameType::Bulk,
                          msgToHeadunit);
  }
  gst_buffer_unmap(buffer, &map);
  gst_sample_unref(sample);
  return GST_FLOW_OK;
}

static void error_cb(GstBus *bus, GstMessage *msg, VideoChannelHandler *_this) {
  cout << "ERROR" << endl;
}

VideoChannelHandler::VideoChannelHandler(uint8_t channelId)
    : ChannelHandler(channelId) {
  cout << "VideoChannelHandler: " << (int)channelId << endl;
  channelOpened = false;

  pipeline = gst_pipeline_new("main-pipeline");

  auto app_sink = gst_element_factory_make("appsink", "app_sink");
  g_object_set(app_sink, "emit-signals", TRUE, NULL);
  g_signal_connect(app_sink, "new-sample", G_CALLBACK(new_sample), this);

  auto queue = gst_element_factory_make("queue", "queue");
  auto videoconvert = gst_element_factory_make("videoconvert", "videoconvert");
  auto videoscale = gst_element_factory_make("videoscale", "videoscale");
  auto videorate = gst_element_factory_make("videorate", "videorate");
  auto x264enc = gst_element_factory_make("x264enc", "x264enc");
  g_object_set(x264enc, "speed-preset", 1, "key-int-max", 25, "aud", FALSE,
               "insert-vui", TRUE, NULL);
  auto h264caps = gst_caps_new_simple(
      "video/x-h264", "stream-format", G_TYPE_STRING, "byte-stream", "profile",
      G_TYPE_STRING, "baseline", "width", G_TYPE_INT, 800, "height", G_TYPE_INT,
      480, "framerate", GST_TYPE_FRACTION, 30, 1, NULL);
  auto capsfilter_h264 =
      gst_element_factory_make("capsfilter", "capsfilter_h264");
  g_object_set(capsfilter_h264, "caps", h264caps, NULL);
  auto rawcaps =
      gst_caps_new_simple("video/x-raw", "width", G_TYPE_INT, 800, "height",
                          G_TYPE_INT, 480, "framerate", GST_TYPE_FRACTION, 30,
                          1, "format", G_TYPE_STRING, "I420", NULL);
  auto capsfilter_pre =
      gst_element_factory_make("capsfilter", "capsfilter_pre");
  g_object_set(capsfilter_pre, "caps", rawcaps, NULL);

  auto shmsrc = gst_element_factory_make("shmsrc", "shmsrc");
  g_object_set(G_OBJECT(shmsrc), "socket-path", "/tmp/aacs_mixer", NULL);
  g_object_set(G_OBJECT(shmsrc), "is-live", TRUE, NULL);
  g_object_set(G_OBJECT(shmsrc), "do-timestamp", TRUE, NULL);
  auto queue_snowmix = gst_element_factory_make("queue", "queue_snowmix");
  g_object_set(G_OBJECT(queue_snowmix), "leaky", 2, NULL);
  g_object_set(G_OBJECT(queue_snowmix), "max-size-buffers", 2, NULL);
  auto snowmixcaps =
      gst_caps_new_simple("video/x-raw", "width", G_TYPE_INT, 800, "height",
                          G_TYPE_INT, 480, "framerate", GST_TYPE_FRACTION, 30,
                          1, "format", G_TYPE_STRING, "BGRA", NULL);
  auto capsfilter_snowmix =
      gst_element_factory_make("capsfilter", "capsfilter_snowmix");
  g_object_set(capsfilter_snowmix, "caps", snowmixcaps, NULL);

  gst_bin_add_many(GST_BIN(pipeline), shmsrc, queue_snowmix, capsfilter_snowmix,
                   videoconvert, videoscale, videorate, capsfilter_pre, queue,
                   x264enc, capsfilter_h264, app_sink, NULL);

  GSTCHECK(gst_element_link_many(shmsrc, queue_snowmix, capsfilter_snowmix,
                                 videoconvert, videoscale, videorate,
                                 capsfilter_pre, queue, x264enc,
                                 capsfilter_h264, app_sink, NULL));
  gst_caps_unref(h264caps);
  gst_caps_unref(rawcaps);
  gst_caps_unref(snowmixcaps);

  auto bus = gst_element_get_bus(pipeline);
  gst_bus_add_signal_watch(bus);
  g_signal_connect(G_OBJECT(bus), "message::error", (GCallback)error_cb, this);
  gst_object_unref(bus);

  auto th = std::thread([this]() {
    openChannel();
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
  });
  th.detach();
}

VideoChannelHandler::~VideoChannelHandler() {}

void VideoChannelHandler::openChannel() {
  channelOpened = true;
  ChannelHandler::openChannel();
  gotSetupResponse = false;
  sendSetupRequest();
  expectSetupResponse();
}

void VideoChannelHandler::disconnected(int clientId) {}

void VideoChannelHandler::sendSetupRequest() {
  std::vector<uint8_t> plainMsg;
  pushBackInt16(plainMsg, MediaMessageType::SetupRequest);
  plainMsg.push_back(0x08);
  plainMsg.push_back(0x03);
  sendToHeadunit(channelId, FrameType::Bulk | EncryptionType::Encrypted,
                 plainMsg);
}

void VideoChannelHandler::expectSetupResponse() {
  std::unique_lock<std::mutex> lk(m);
  cv.wait(lk, [=] { return gotSetupResponse; });
}

void VideoChannelHandler::sendStartIndication() {
  std::vector<uint8_t> plainMsg;
  pushBackInt16(plainMsg, MediaMessageType::StartIndication);
  plainMsg.push_back(0x08);
  plainMsg.push_back(0x00);
  plainMsg.push_back(0x10);
  plainMsg.push_back(0x01);
  sendToHeadunit(channelId, FrameType::Bulk | EncryptionType::Encrypted,
                 plainMsg);
}

bool VideoChannelHandler::handleMessageFromHeadunit(const Message &message) {
  if (!channelOpened) {
    ChannelHandler::sendToClient(-1, message.channel,
                                 message.flags & MessageTypeFlags::Specific,
                                 message.content);
    return true;
  }
  if (ChannelHandler::handleMessageFromHeadunit(message))
    return true;
  bool messageHandled = false;
  {
    std::unique_lock<std::mutex> lk(m);
    auto msg = message.content;
    const __u16 *shortView = (const __u16 *)(msg.data());
    auto messageType = be16_to_cpu(shortView[0]);
    if (messageType == MediaMessageType::SetupResponse) {
      gotSetupResponse = true;
      messageHandled = true;
    } else if (messageType == MediaMessageType::VideoFocusIndication) {
      sendStartIndication();
      messageHandled = true;
    } else if (messageType == MediaMessageType::MediaAckIndication) {
      messageHandled = true;
    }
  }
  cv.notify_all();
  return messageHandled;
}

bool VideoChannelHandler::handleMessageFromClient(int clientId,
                                                  uint8_t channelId,
                                                  bool specific,
                                                  const vector<uint8_t> &data) {
  // Video is routed through snowmix
  return false;
}
