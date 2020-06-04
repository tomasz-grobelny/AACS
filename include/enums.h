// Distributed under GPLv3 only as specified in repository's root LICENSE file

#pragma once

enum EncryptionType {
  Plain = 0,
  Encrypted = 1 << 3,
};

enum FrameType {
  First = 1,
  Last = 2,
  Bulk = First | Last,
};

enum MessageTypeFlags {
  Control = 0,
  Specific = 1 << 2,
};

enum MessageType {
  VersionRequest = 1,
  VersionResponse = 2,
  SslHandshake = 3,
  AuthComplete = 4,
  ServiceDiscoveryRequest = 5,
  ServiceDiscoveryResponse = 6,
  ChannelOpenRequest = 7,
  ChannelOpenResponse = 8,
  PingRequest = 0xb,
  PingResponse = 0xc,
  NavigationFocusRequest = 0x0d,
  NavigationFocusResponse = 0x0e,
  VoiceSessionRequest = 0x11,
  AudioFocusRequest = 0x12,
  AudioFocusResponse = 0x13,
};

enum MediaMessageType {
  MediaWithTimestampIndication = 0x0000,
  MediaIndication = 0x0001,
  SetupRequest = 0x8000,
  StartIndication = 0x8001,
  SetupResponse = 0x8003,
  MediaAckIndication = 0x8004,
  VideoFocusIndication = 0x8008,
};
