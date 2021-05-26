// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "VideoChannelHandler.h"
#include "MediaChannelSetupResponse.pb.h"
#include "enums.h"
#include "utils.h"
#include <boost/mpl/back_inserter.hpp>
#include <gst/gstelement.h>
#include <gst/gstmemory.h>
#include <gst/gstpad.h>
#include <gst/gstutils.h>
#include <gst/gstvalue.h>
#include <iterator>

using namespace std;

VideoChannelHandler::VideoChannelHandler(uint8_t channelId, uint8_t numConfigs)
    : ChannelHandler(channelId), numVideoConfigs(numConfigs) {
  startTimestamp = 0;
}

VideoChannelHandler::~VideoChannelHandler() {}

bool VideoChannelHandler::handleMessageFromMobile(
    uint8_t channelId, uint8_t flags, const std::vector<uint8_t> &data) {
  const uint16_t *shortView = (const uint16_t *)(data.data());
  auto msgType = be16toh(shortView[0]);
  if (msgType == MessageType::ChannelOpenRequest) {
    sendChannelOpenResponse();
    return true;
  } else if (msgType == MediaMessageType::SetupRequest) {
    sendSetupResponse();
    sendVideoFocusIndication();
    return true;
  } else if (msgType == MediaMessageType::StartIndication) {
    return true;
  } else if (msgType == MediaMessageType::MediaIndication) {
    createAppSource();
    pushDataToPipeline(0, vector(data.begin() + 2, data.end()));
    sendAck();
    return true;
  } else if (msgType == MediaMessageType::MediaWithTimestampIndication) {
    auto ts = bytesToUInt64(data, 2);
    if (startTimestamp == 0) {
      startTimestamp = ts - 100'000;
    }
    auto localTs = (ts - startTimestamp);
    pushDataToPipeline(localTs * 1000,
                       vector(data.begin() + 2 + 8, data.end()));
    sendAck();
    return true;
  }
  return false;
}

void VideoChannelHandler::createAppSource() {
  app_source = gst_element_factory_make("appsrc", "app_source");
  auto srccaps = gst_caps_new_simple("video/x-h264", "stream-format",
                                     G_TYPE_STRING, "byte-stream", NULL);
  g_object_set(app_source, "caps", srccaps, NULL);
  g_object_set(app_source, "format", GST_FORMAT_TIME, NULL);
  g_object_set(app_source, "is-live", TRUE, NULL);
  gst_caps_unref(srccaps);

  auto h264parse = gst_element_factory_make("h264parse", "h264parse");
  auto avdec_h264 = gst_element_factory_make("avdec_h264", "avdec_h264");

  auto videoscale = gst_element_factory_make("videoscale", "videoscale");
  auto videorate = gst_element_factory_make("videorate", "videorate");
  auto videoconvert = gst_element_factory_make("videoconvert", "videoconvert");

  auto shmsink = gst_element_factory_make("shmsink", "shmsink");
  g_object_set(shmsink, "wait-for-connection", 0, NULL);
  g_object_set(shmsink, "sync", TRUE, NULL);
  g_object_set(shmsink, "shm-size", 800 * 480 * 4 * 22, NULL);
  g_object_set(shmsink, "socket-path", "/tmp/aacs_feed1", NULL);

  auto snowmixcaps =
      gst_caps_new_simple("video/x-raw", "width", G_TYPE_INT, 800, "height",
                          G_TYPE_INT, 480, "framerate", GST_TYPE_FRACTION, 30,
                          1, "format", G_TYPE_STRING, "BGRA", NULL);
  auto capsfilter_snowmix =
      gst_element_factory_make("capsfilter", "capsfilter_snowmix");
  g_object_set(capsfilter_snowmix, "caps", snowmixcaps, NULL);

  pipeline = gst_pipeline_new("main-pipeline");
  gst_bin_add_many(GST_BIN(pipeline), app_source, h264parse, avdec_h264,
                   videoconvert, videoscale, videorate, capsfilter_snowmix,
                   shmsink, NULL);
  GSTCHECK(gst_element_link_many(app_source, h264parse, avdec_h264,
                                 videoconvert, videoscale, videorate,
                                 capsfilter_snowmix, shmsink, NULL));
  gst_element_set_state(pipeline, GST_STATE_PLAYING);
}

void VideoChannelHandler::pushDataToPipeline(uint64_t ts,
                                             const std::vector<uint8_t> &data) {
  cout << "pushDataToPipeline" << endl;
  auto buffer = gst_buffer_new_and_alloc(data.size());
  if (ts) {
    GST_BUFFER_TIMESTAMP(buffer) = ts;
  }

  GstMapInfo map;
  gst_buffer_map(buffer, &map, GST_MAP_WRITE);
  copy(data.begin(), data.end(), map.data);
  gst_buffer_unmap(buffer, &map);

  GstFlowReturn ret;
  g_signal_emit_by_name(app_source, "push-buffer", buffer, &ret);
  gst_buffer_unref(buffer);

  if (ret != GST_FLOW_OK) {
    throw runtime_error("push-buffer failed");
  }
}

bool VideoChannelHandler::handleMessageFromServer(
    uint8_t channelId, bool specific, const std::vector<uint8_t> &data) {
  uint8_t flags = EncryptionType::Encrypted | FrameType::Bulk;
  if (specific) {
    flags |= MessageTypeFlags::Specific;
  }
  sendToMobile(channelId, flags, data);
  return true;
}

void VideoChannelHandler::sendChannelOpenResponse() {
  vector<uint8_t> msg;
  pushBackInt16(msg, MessageType::ChannelOpenResponse);
  msg.push_back(0x08);
  msg.push_back(0x00);
  sendToMobile(channelId,
               EncryptionType::Encrypted | FrameType::Bulk |
                   MessageTypeFlags::Specific,
               msg);
}

void VideoChannelHandler::sendSetupResponse() {
  vector<uint8_t> msg;
  pushBackInt16(msg, MediaMessageType::SetupResponse);
  tag::aas::MediaChannelSetupResponse mcsr;
  mcsr.set_unknown_field_1(2);
  mcsr.set_max_unacked(4);
  for (auto i = 0; i < numVideoConfigs; i++)
    mcsr.add_configs(i);
  auto mcsrStr = mcsr.SerializeAsString();
  copy(mcsrStr.begin(), mcsrStr.end(), back_inserter(msg));
  sendToMobile(channelId, EncryptionType::Encrypted | FrameType::Bulk, msg);
}

void VideoChannelHandler::sendVideoFocusIndication() {
  vector<uint8_t> msg;
  pushBackInt16(msg, MediaMessageType::VideoFocusIndication);
  msg.push_back(0x08);
  msg.push_back(0x01);
  msg.push_back(0x10);
  msg.push_back(0x00);
  sendToMobile(channelId, EncryptionType::Encrypted | FrameType::Bulk, msg);
}

void VideoChannelHandler::sendAck() {
  vector<uint8_t> msg;
  pushBackInt16(msg, MediaMessageType::MediaAckIndication);
  msg.push_back(0x08);
  msg.push_back(0x00);
  msg.push_back(0x10);
  msg.push_back(0x01);
  sendToMobile(channelId, EncryptionType::Encrypted | FrameType::Bulk, msg);
}
