// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "gstaavideosink.h"
#include <algorithm>
#include <gst/base/gstbasesink.h>
#include <gst/gst.h>
#include <gst/gstmemory.h>
#include <gst/gstpad.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>

using namespace std;

GST_DEBUG_CATEGORY_STATIC(gst_aavideo_sink_debug);
#define GST_CAT_DEFAULT gst_aavideo_sink_debug

enum { PROP_0, PROP_SOCKETNAME };

static GstStaticPadTemplate sinkTemplate =
    GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                            GST_STATIC_CAPS("video/x-h264;video/x-raw"));

#define gst_aavideo_parent_class parent_class
G_DEFINE_TYPE(GstAAVideoSink, gst_aavideo, GST_TYPE_BASE_SINK);

void gst_aavideo_class_init(GstAAVideoSinkClass *klass) {
  GstBaseSinkClass *basesink_class = GST_BASE_SINK_CLASS(klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS(klass);
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->set_property = gst_aavideo_sink_set_property;
  object_class->get_property = gst_aavideo_sink_get_property;
  object_class->finalize = (GObjectFinalizeFunc)gst_aavideo_finalize;

  g_object_class_install_property(
      object_class, PROP_SOCKETNAME,
      g_param_spec_string("socketName", "AAServer socket name",
                          "AAServer socket to connect to", NULL,
                          G_PARAM_READWRITE));

  gst_element_class_set_details_simple(
      gstelement_class, "AAVideoSink", "Send video to AAServer",
      "Sends H264 video stream to Android Auto Server",
      "Tomasz Grobelny <tomasz@grobelny.net>");

  gst_element_class_add_static_pad_template(gstelement_class, &sinkTemplate);

  basesink_class->render = GST_DEBUG_FUNCPTR(gst_aavideo_render_buffer);
  basesink_class->event = GST_DEBUG_FUNCPTR(gst_aavideo_event);
  basesink_class->start = GST_DEBUG_FUNCPTR(gst_aavideo_start);
  basesink_class->stop = GST_DEBUG_FUNCPTR(gst_aavideo_stop);
}

void gst_aavideo_init(GstAAVideoSink *sink) { GST_DEBUG("gst_aavideo_init"); }

void gst_aavideo_finalize(GstAAVideoSink *test) {
  G_OBJECT_CLASS(parent_class)->finalize((GObject *)test);
}

void gst_aavideo_sink_set_property(GObject *object, guint prop_id,
                                   const GValue *value, GParamSpec *pspec) {
  auto *sink = GST_AAVIDEOSINK(object);

  switch (prop_id) {
  case PROP_SOCKETNAME:
    g_free(sink->socketName);
    sink->socketName = g_value_dup_string(value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

void gst_aavideo_sink_get_property(GObject *object, guint prop_id,
                                   GValue *value, GParamSpec *pspec) {
  auto sink = GST_AAVIDEOSINK(object);

  switch (prop_id) {
  case PROP_SOCKETNAME:
    g_value_set_string(value, sink->socketName);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

gboolean gst_aavideo_event(GstBaseSink *basesink, GstEvent *event) {
  auto aavideosink = GST_AAVIDEOSINK(basesink);

  switch (GST_EVENT_TYPE(event)) {

  case GST_EVENT_EOS:
    break;

  case GST_EVENT_CAPS: {
    GstCaps *caps;
    gst_event_parse_caps(event, &caps);
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    auto name = gst_structure_get_name(structure);
    aavideosink->isRaw = (name == "video/x-raw"s);
    break;
  }
  default:
    break;
  }

  return GST_BASE_SINK_CLASS(parent_class)->event(basesink, event);
}

GstFlowReturn gst_aavideo_render_buffer(GstBaseSink *basesink, GstBuffer *buf) {
  auto aavideosink = GST_AAVIDEOSINK(basesink);

  GstMapInfo info;
  gst_buffer_map(buf, &info, GST_MAP_READ);

  auto messageType = aavideosink->firstFrame
                         ? AvMessageType::MediaIndication
                         : AvMessageType::MediaWithTimestampIndication;
  std::vector<uint8_t> plainMsg;
  plainMsg.push_back(0x01);                       // packet type
  plainMsg.push_back(aavideosink->channelNumber); // channel number
  plainMsg.push_back(0x00);                       // specific
  plainMsg.push_back(
      aavideosink->firstFrame
          ? (aavideosink->isRaw ? 0x04 : 0x02)
          : 0x03); // media (with timestamp) indication in video channel handler
  if (aavideosink->firstFrame)
    plainMsg.push_back(0xff);
  auto timeStamp = buf->pts / 1000;
  for (int s = 56; s >= 0 && !aavideosink->firstFrame; s -= 8) {
    plainMsg.push_back((timeStamp >> s) & 0xFF);
  }
  std::copy(info.data, info.data + info.size, std::back_inserter(plainMsg));

  auto bytesWritten =
      write(aavideosink->aaServerFd, plainMsg.data(), plainMsg.size());
  auto ret = bytesWritten == plainMsg.size() ? GST_FLOW_OK : GST_FLOW_EOS;
  gst_buffer_unmap(buf, &info);
  aavideosink->firstFrame = false;
  GST_DEBUG("render done: %d %d", (int)plainMsg.size(), (int)bytesWritten);
  return ret;
}

int getSocketFd(std::string socketName) {
  int fd;
  if ((fd = socket(PF_UNIX, SOCK_SEQPACKET, 0)) < 0) {
    throw runtime_error("socket failed");
  }
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, socketName.c_str());
  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    throw runtime_error("connect failed");
  }
  return fd;
}

void openChannel(GstAAVideoSink *aavideosink) {
  uint8_t buffer[3];
  buffer[0] = 0; // packet type == GetChannelNumberByChannelType
  buffer[1] = 0; // channel type == Video
  buffer[2] = 0; // specific?
  if (write(aavideosink->aaServerFd, buffer, sizeof buffer) != sizeof buffer)
    throw runtime_error("failed to write open channel");
  uint8_t channelNumber;
  if (read(aavideosink->aaServerFd, &channelNumber, sizeof(channelNumber)) !=
      sizeof(channelNumber))
    throw runtime_error("failed to read video channel number");
  aavideosink->channelNumber = channelNumber;
  GST_DEBUG("channelNumber: %d", channelNumber);
}

gboolean gst_aavideo_start(GstBaseSink *sink) {
  auto aavideosink = GST_AAVIDEOSINK(sink);
  aavideosink->aaServerFd = getSocketFd(aavideosink->socketName);
  aavideosink->firstFrame = true;
  openChannel(aavideosink);
  return TRUE;
}

gboolean gst_aavideo_stop(GstBaseSink *sink) {
  auto aavideosink = GST_AAVIDEOSINK(sink);
  close(aavideosink->aaServerFd);
  return TRUE;
}

static gboolean aavideosink_init(GstPlugin *aavideosink) {
  GST_DEBUG_CATEGORY_INIT(gst_aavideo_sink_debug, "aavideosink", 0,
                          "AAVideoSink");
  return gst_element_register(aavideosink, "aavideosink", GST_RANK_NONE,
                              GST_TYPE_AAVIDEOSINK);
}

#define PACKAGE "aavideosink"

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, AAVideoSink,
                  "Template aavideosink", aavideosink_init, "1.0.0", "LGPL",
                  "AAVideoSink",
                  "https://github.com/tomasz-grobelny/AAVideoSink")
