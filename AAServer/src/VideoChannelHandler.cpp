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

using namespace std;

#define GSTCHECK(x) GstCheck(x, __LINE__)

static void GstCheck(gboolean returnCode, int line) {
  if (returnCode != TRUE)
    throw runtime_error("GStreamer function call failed at line " +
                        to_string(line));
}

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
  if (firstSample) {
    _this->openChannel();
  }
  if (buffer->pts == -1) {
    pushBackInt16(msgToHeadunit, MediaMessageType::MediaIndication);
  } else {
    pushBackInt16(msgToHeadunit,
                  MediaMessageType::MediaWithTimestampIndication);
    pushBackInt64(msgToHeadunit, buffer->pts / 1000);
  }
  GstMapInfo map;
  gst_buffer_map(buffer, &map, GST_MAP_READ);
  copy(map.data, map.data + map.size, back_inserter(msgToHeadunit));
  gst_buffer_unmap(buffer, &map);
  _this->sendToHeadunit(_this->channelId,
                        EncryptionType::Encrypted | FrameType::Bulk,
                        msgToHeadunit);

  gst_sample_unref(sample);
  firstSample = false;
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

  auto videotestsrc = gst_element_factory_make("videotestsrc", "videotestsrc");
  g_object_set(videotestsrc, "pattern", 0, NULL);
  g_object_set(G_OBJECT(videotestsrc), "is-live", TRUE, NULL);
  inputSelector = gst_element_factory_make("input-selector", "input-selector");

  g_object_set(G_OBJECT(inputSelector), "sync-mode", 1, NULL);

  auto queue = gst_element_factory_make("queue", "queue");
  auto videoconvert = gst_element_factory_make("videoconvert", "videoconvert");
  auto videoscale = gst_element_factory_make("videoscale", "videoscale");
  auto videorate = gst_element_factory_make("videorate", "videorate");
  auto x264enc = gst_element_factory_make("x264enc", "x264enc");
  g_object_set(x264enc, "speed-preset", 1, "key-int-max", 25, NULL);
  auto h264caps = gst_caps_new_simple(
      "video/x-h264", "stream-format", G_TYPE_STRING, "byte-stream", "profile",
      G_TYPE_STRING, "baseline", "width", G_TYPE_INT, 800, "height", G_TYPE_INT,
      480, "framerate", GST_TYPE_FRACTION, 30, 1, NULL);
  auto rawcaps =
      gst_caps_new_simple("video/x-raw", "width", G_TYPE_INT, 800, "height",
                          G_TYPE_INT, 480, "framerate", GST_TYPE_FRACTION, 30,
                          1, "format", G_TYPE_STRING, "I420", NULL);
  auto capsfilter_pre =
      gst_element_factory_make("capsfilter", "capsfilter_pre");
  g_object_set(capsfilter_pre, "caps", rawcaps, NULL);
  auto capsfilter_post =
      gst_element_factory_make("capsfilter", "capsfilter_post");
  g_object_set(capsfilter_post, "caps", rawcaps, NULL);

  gst_bin_add_many(GST_BIN(pipeline), videotestsrc, videoconvert, videoscale,
                   videorate, capsfilter_pre, inputSelector, capsfilter_post,
                   queue, x264enc, app_sink, NULL);

  GSTCHECK(gst_element_link_many(videotestsrc, videoconvert, videoscale,
                                 videorate, capsfilter_pre, NULL));

  sink_request_pad = gst_element_get_request_pad(inputSelector, "sink_%u");
  auto src_static_pad = gst_element_get_static_pad(capsfilter_pre, "src");

  if (gst_pad_link(src_static_pad, sink_request_pad) != GST_PAD_LINK_OK)
    throw runtime_error("gst_pad_link VideoChannelHandler failed");
  GSTCHECK(gst_element_link_many(inputSelector, capsfilter_post, queue, x264enc,
                                 NULL));
  GSTCHECK(gst_element_link_filtered(x264enc, app_sink, h264caps));
  gst_caps_unref(h264caps);
  gst_caps_unref(rawcaps);

  auto bus = gst_element_get_bus(pipeline);
  gst_bus_add_signal_watch(bus);
  g_signal_connect(G_OBJECT(bus), "message::error", (GCallback)error_cb, this);
  gst_object_unref(bus);

  gst_element_set_state(pipeline, GST_STATE_PLAYING);
}

VideoChannelHandler::~VideoChannelHandler() {}

void VideoChannelHandler::createAppSource(int clientId, uint8_t priority) {
  gst_element_set_state(pipeline, GST_STATE_PAUSED);

  auto app_source = gst_element_factory_make(
      "appsrc", ("app_source_" + to_string(clientId)).c_str());
  auto srccaps = gst_caps_new_simple("video/x-h264", "stream-format",
                                     G_TYPE_STRING, "byte-stream", NULL);
  g_object_set(app_source, "caps", srccaps, NULL);
  g_object_set(app_source, "format", GST_FORMAT_TIME, NULL);
  g_object_set(app_source, "is-live", TRUE, NULL);
  gst_caps_unref(srccaps);

  auto h264parse = gst_element_factory_make(
      "h264parse", ("h264parse_" + to_string(clientId)).c_str());
  auto avdec_h264 = gst_element_factory_make(
      "avdec_h264", ("avdec_h264_" + to_string(clientId)).c_str());

  auto videoscale = gst_element_factory_make(
      "videoscale", ("videoscale_" + to_string(clientId)).c_str());
  auto videorate = gst_element_factory_make(
      "videorate", ("videorate_" + to_string(clientId)).c_str());
  auto videoconvert = gst_element_factory_make(
      "videoconvert", ("videoconvert_" + to_string(clientId)).c_str());

  auto rawcaps =
      gst_caps_new_simple("video/x-raw", "width", G_TYPE_INT, 800, "height",
                          G_TYPE_INT, 480, "framerate", GST_TYPE_FRACTION, 30,
                          1, "format", G_TYPE_STRING, "I420", NULL);
  auto capsfilter_pre = gst_element_factory_make(
      "capsfilter", ("capsfilter_pre_" + to_string(clientId)).c_str());
  g_object_set(capsfilter_pre, "caps", rawcaps, NULL);

  gst_bin_add_many(GST_BIN(pipeline), app_source, h264parse, avdec_h264,
                   videoconvert, videoscale, videorate, capsfilter_pre, NULL);
  GSTCHECK(gst_element_link_many(app_source, h264parse, avdec_h264,
                                 videoconvert, videoscale, videorate,
                                 capsfilter_pre, NULL));

  auto sink_request_pad = gst_element_get_request_pad(inputSelector, "sink_%u");
  auto src_static_pad = gst_element_get_static_pad(capsfilter_pre, "src");
  if (gst_pad_link(src_static_pad, sink_request_pad) != GST_PAD_LINK_OK)
    throw runtime_error("gst_pad_link createAppSource failed");

  g_object_set(inputSelector, "active-pad", sink_request_pad, NULL);
  clientData[clientId].app_source = app_source;
  clientData[clientId].startTimestamp = 0;
  clientData[clientId].priority = priority;
  clientData[clientId].sink_request_pad = sink_request_pad;
  clientData[clientId].src_static_pad = src_static_pad;

  gst_element_set_state(pipeline, GST_STATE_PLAYING);
}

void VideoChannelHandler::createRawAppSource(int clientId, uint8_t priority) {
  gst_element_set_state(pipeline, GST_STATE_PAUSED);

  auto app_source = gst_element_factory_make(
      "appsrc", ("app_source_" + to_string(clientId)).c_str());
  auto srccaps = gst_caps_new_simple(
      "video/x-raw", "width", G_TYPE_INT, 800, "height", G_TYPE_INT, 480,
      "framerate", GST_TYPE_FRACTION, 30, 1, "pixel-aspect-ratio",
      GST_TYPE_FRACTION, 1, 1, "format", G_TYPE_STRING, "I420", NULL);
  g_object_set(app_source, "caps", srccaps, NULL);
  g_object_set(app_source, "format", GST_FORMAT_TIME, NULL);
  g_object_set(app_source, "is-live", TRUE, NULL);
  gst_caps_unref(srccaps);

  auto videoscale = gst_element_factory_make(
      "videoscale", ("videoscale_" + to_string(clientId)).c_str());
  auto videorate = gst_element_factory_make(
      "videorate", ("videorate_" + to_string(clientId)).c_str());
  auto videoconvert = gst_element_factory_make(
      "videoconvert", ("videoconvert_" + to_string(clientId)).c_str());

  auto rawcaps =
      gst_caps_new_simple("video/x-raw", "width", G_TYPE_INT, 800, "height",
                          G_TYPE_INT, 480, "framerate", GST_TYPE_FRACTION, 30,
                          1, "format", G_TYPE_STRING, "I420", NULL);
  auto capsfilter_pre = gst_element_factory_make(
      "capsfilter", ("capsfilter_pre_" + to_string(clientId)).c_str());
  g_object_set(capsfilter_pre, "caps", rawcaps, NULL);

  gst_bin_add_many(GST_BIN(pipeline), app_source, videoconvert, videoscale,
                   videorate, capsfilter_pre, NULL);
  GSTCHECK(gst_element_link_many(app_source, videoconvert, videoscale,
                                 videorate, capsfilter_pre, NULL));

  auto sink_request_pad = gst_element_get_request_pad(inputSelector, "sink_%u");
  auto src_static_pad = gst_element_get_static_pad(capsfilter_pre, "src");
  if (gst_pad_link(src_static_pad, sink_request_pad) != GST_PAD_LINK_OK)
    throw runtime_error("gst_pad_link createRawAppSource failed");

  g_object_set(inputSelector, "active-pad", sink_request_pad, NULL);
  clientData[clientId].app_source = app_source;
  clientData[clientId].startTimestamp = 0;
  clientData[clientId].priority = priority;
  clientData[clientId].sink_request_pad = sink_request_pad;
  clientData[clientId].src_static_pad = src_static_pad;

  gst_element_set_state(pipeline, GST_STATE_PLAYING);
}

void VideoChannelHandler::openChannel() {
  channelOpened = true;
  ChannelHandler::openChannel();
  gotSetupResponse = false;
  sendSetupRequest();
  expectSetupResponse();
}

void VideoChannelHandler::disconnected(int clientId) {
  gst_element_set_state(pipeline, GST_STATE_PAUSED);
  auto appsrc = clientData[clientId].app_source;
  auto sink_to_unlink = clientData[clientId].sink_request_pad;
  auto src_to_unlink = clientData[clientId].src_static_pad;
  clientData.erase(clientId);
  GstFlowReturn ret;
  g_signal_emit_by_name(appsrc, "end-of-stream", &ret);
  auto next_channel = boost::range::max_element(clientData, [](auto a1,
                                                               auto a2) {
    return a1.second.priority < a2.second.priority ||
           (a1.second.priority == a2.second.priority && a1.first < a2.first);
  });
  auto srp = next_channel != clientData.end()
                 ? next_channel->second.sink_request_pad
                 : sink_request_pad;
  g_object_set(inputSelector, "active-pad", srp, NULL);
  gst_pad_unlink(src_to_unlink, sink_to_unlink);
  gst_bin_remove_many(GST_BIN(pipeline), appsrc, NULL);
  gst_element_set_state(pipeline, GST_STATE_PLAYING);
}

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
  plainMsg.push_back(0x00);
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

void VideoChannelHandler::pushDataToPipeline(int clientId, uint64_t ts,
                                             const vector<uint8_t> &data) {
  auto buffer = gst_buffer_new_and_alloc(data.size());
  if (ts) {
    GST_BUFFER_TIMESTAMP(buffer) = ts;
  }

  GstMapInfo map;
  gst_buffer_map(buffer, &map, GST_MAP_WRITE);
  copy(data.begin(), data.end(), map.data);
  gst_buffer_unmap(buffer, &map);

  GstFlowReturn ret;
  g_signal_emit_by_name(clientData[clientId].app_source, "push-buffer", buffer,
                        &ret);
  gst_buffer_unref(buffer);

  if (ret != GST_FLOW_OK) {
    throw runtime_error("push-buffer failed");
  }
}

bool VideoChannelHandler::handleMessageFromClient(int clientId,
                                                  uint8_t channelId,
                                                  bool specific,
                                                  const vector<uint8_t> &data) {
  if (data.size() == 0)
    return true;
  auto msgType = data[0];
  if (msgType == 0x02) {
    int priority = data[1];
    createAppSource(clientId, priority);
    pushDataToPipeline(clientId, 0,
                       vector<uint8_t>(data.begin() + 2, data.end()));
    return true;
  } else if (msgType == 0x04) {
    int priority = data[1];
    createRawAppSource(clientId, priority);
    pushDataToPipeline(clientId, 0,
                       vector<uint8_t>(data.begin() + 2, data.end()));
    return true;
  } else if (msgType == 0x03) {
    auto ts = bytesToUInt64(data, 1);
    if (clientData[clientId].startTimestamp == 0) {
      clientData[clientId].startTimestamp = ts - 100'000;
    }
    auto localTs = (ts - clientData[clientId].startTimestamp);
    pushDataToPipeline(clientId, localTs * 1000,
                       vector(data.begin() + 1 + 8, data.end()));
    return true;
  }
  return false;
}
