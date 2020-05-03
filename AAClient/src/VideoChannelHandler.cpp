// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "VideoChannelHandler.h"
#include "MediaChannelSetupResponse.pb.h"
#include "enums.h"
#include "utils.h"
#include <boost/mpl/back_inserter.hpp>
#include <iterator>

using namespace std;

VideoChannelHandler::VideoChannelHandler(uint8_t channelId)
    : ChannelHandler(channelId) {}

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
    sendAvMediaIndication(data);
    sendAck();
    return true;
  } else if (msgType == MediaMessageType::MediaWithTimestampIndication) {
    sendAvMediaIndicationWithTimestamp(data);
    sendAck();
    return true;
  }
  return false;
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
  mcsr.set_unknown_field_2(1);
  mcsr.set_unknown_field_3(0);
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

void VideoChannelHandler::sendAvMediaIndication(
    const std::vector<uint8_t> &data) {
  std::vector<uint8_t> msgToServer;
  msgToServer.push_back(0x02); // av media indication
  msgToServer.push_back(0x7f); // prio
  copy(data.begin() + 2, data.end(), back_inserter(msgToServer));
  sendToServer(channelId, true, msgToServer);
}

void VideoChannelHandler::sendAvMediaIndicationWithTimestamp(
    const std::vector<uint8_t> &data) {
  std::vector<uint8_t> msgToServer;
  msgToServer.push_back(0x03); // av media indication with timestamp
  copy(data.begin() + 2, data.end(), back_inserter(msgToServer));
  sendToServer(channelId, true, msgToServer);
}
