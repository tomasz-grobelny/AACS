// Distributed under GPLv3 only as specified in repository's root LICENSE file

#pragma once
#include <cstdint>
#include <gst/base/gstbasesink.h>
#include <gst/gst.h>

#define GST_TYPE_AAVIDEOSINK (gst_aavideo_get_type())
#define GST_AAVIDEOSINK(obj)                                                   \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AAVIDEOSINK, GstAAVideoSink))
#define GST_AAVIDEOSINK_CLASS(klass)                                           \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AAVIDEOSINK, GstAAVideoSinkClass))
#define GST_IS_AAVIDEOSINK(obj)                                                \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_AAVIDEOSINK))
#define GST_IS_AAVIDEOSINK_CLASS(klass)                                        \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_AAVIDEOSINK))

struct GstAAVideoSink {
  GstBaseSink basesink;
  gchar *socketName;
  int aaServerFd;
  uint8_t channelNumber;
  gboolean firstFrame;
  gboolean isRaw;
};

struct GstAAVideoSinkClass {
  GstBaseSinkClass parent_class;
};

extern "C" {
GType gst_aavideo_get_type(void);
void gst_aavideo_finalize(GstAAVideoSink *test);
gboolean gst_aavideo_event(GstBaseSink *basesink, GstEvent *event);
GstFlowReturn gst_aavideo_render_buffer(GstBaseSink *basesink, GstBuffer *buf);
gboolean gst_aavideo_start(GstBaseSink *sink);
gboolean gst_aavideo_stop(GstBaseSink *sink);
}

enum AvMessageType {
  MediaWithTimestampIndication = 0x0000,
  MediaIndication = 0x0001,
  SetupRequest = 0x8000,
  StartIndication = 0x8001,
  SetupResponse = 0x8003,
  MediaAckIndication = 0x8004,
  VideoFocusIndication = 0x8008,
};

void gst_aavideo_sink_set_property(GObject *object, guint prop_id,
                                          const GValue *value,
                                          GParamSpec *pspec);
void gst_aavideo_sink_get_property(GObject *object, guint prop_id,
                                          GValue *value, GParamSpec *pspec);
gboolean gst_aavideo_sink_sink_event(GstPad *pad, GstObject *parent,
                                            GstEvent *event);
