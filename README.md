# Intro
Many new cars have headunits that support Android Auto (AA). While the default use case (connect mobile via USB) is already useful (eg. for navigation), one may look at AA headunits as a more generic input/output device.

This project attempts to provide a basic layer to access Android Auto headunits as a video display and later maybe touchscreen, soundcard, etc.

This project attempts to be specific enough to provide a clear way to set it up. On the other hand it attempts to be generic enough by providing connectivity to already existing software components.

# Hardware
This software is being developed on Odroid N2 platform. Reasons behind choosing this platform:
* has USB OTG controller
* powerful enough to process video
* reasonable form factor, power requirements and price

If you want to try out this piece of code and/or help just buy Odroid N2. I do not have access to any other platform (ok, I tried BeagleBone Black, but it was too slow) so it would be extremely hard for me to provide any support. However, feel free to provide patches for other platforms.

# System
The software is being developed on:
* latest (as of this writing) stable version of Ubuntu (19.10)
* kernel 5.x - stock 4.9.x version has issues with gadget support
* latest git version of libusbgx (https://github.com/kopasiak/libusbgx)

# Installation
Instructions how to set up runtime and development environment can be found in the [installation instructions document](doc/INSTALL.md).

# Design
Android Auto Protocol is not documented. However, it has been reverse engineered and open source implementation exists (see https://github.com/opencardev/openauto). While this code does not use a single line from openauto (except for headunit key/cert), openauto is extremenly useful while developing and testing AACS.

There are several components that comprise AACS:
* AAServer is the component responsible for communication with car's headunit. When USB OTG connection is available AAServer starts Android Auto communication with headunit and start listening on a Unix socket for client connections.
* AAClient is the component responsible for communication with mobile device running Andoid Auto. It starts connection to AAServer to get available service description and then forwards all the traffic to AAServer.
* GetEvents is a small tool that listens for touch events from AAServer and uses XTest to forward them to specified application.
* more to come - several more components integrating AACS with generic system components are possible such as GStreamer audio sink, audio/video source, etc.

AACS depends on Snowmix for video mixing.

# Usage ideas
So what exactly could be displayed on headunit? Here are a few ideas:
* any Android application, including any offline navigation, eg. using https://anbox.io/
* video from camera using GStreamer's video4linux2 plugin
* any graphical Linux application (using GStreamer's ximagesrc plugin)

# Status
Currently AACS can do the following:
* it can act as proxy for Android Auto traffic,
* stream any video content to the headunit,
* ...stay tuned for more.
