# Usage example
After installing this GStreamer plugin you can execute the following command:

```
gst-launch-1.0 videotestsrc ! x264enc ! video/x-h264,stream-format=byte-stream,profile=high,width=720,height=480 ! aavideosink socketName=socket
```

where `socket` is Unix domain socket filename opened by AAServer.
