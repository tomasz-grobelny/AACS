// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "InputChannelHandler.h"
#include "InputChannel.pb.h"
#include "enums.h"
#include "utils.h"
#include <fmt/ranges.h>
#include <iostream>
#include <vector>

using namespace std;

InputChannelHandler::InputChannelHandler(uint8_t channelId,
                                         std::vector<int> availableButtons)
    : ChannelHandler(channelId), available_buttons(availableButtons) {
  cout << "InputChannelHandler: " << (int)channelId << endl;
}
InputChannelHandler::~InputChannelHandler() {}

void InputChannelHandler::sendHandshakeRequest() {
  std::vector<uint8_t> plainMsg;
  pushBackInt16(plainMsg, InputChannelMessageType::HandshakeRequest);
  tag::aas::InputChannelHandshakeRequest handshakeRequest;
  cout << fmt::format("Supported buttons ({}): {}", available_buttons.size(),
                      available_buttons)
       << endl;
  for (auto ab : available_buttons)
    handshakeRequest.add_available_buttons((tag::aas::ButtonCode_Enum)ab);
  int bufSize = handshakeRequest.ByteSize();
  uint8_t buffer[bufSize];
  if (!handshakeRequest.SerializeToArray(buffer, bufSize))
    throw aa_runtime_error("handshakeRequest.SerializeToArray failed");
  copy(buffer, buffer + bufSize, std::back_inserter(plainMsg));
  sendToHeadunit(channelId, FrameType::Bulk | EncryptionType::Encrypted,
                 plainMsg);
}

void InputChannelHandler::expectHandshakeResponse() {
  std::unique_lock<std::mutex> lk(m);
  cv.wait(lk, [=] { return gotHandshakeResponse; });
}

bool InputChannelHandler::handleMessageFromHeadunit(const Message &message) {
  if (ChannelHandler::handleMessageFromHeadunit(message))
    return true;
  bool messageHandled = false;
  {
    std::unique_lock<std::mutex> lk(m);
    auto msg = message.content;
    const __u16 *shortView = (const __u16 *)(msg.data());
    auto messageType = be16_to_cpu(shortView[0]);
    if (messageType == InputChannelMessageType::HandshakeResponse) {
      gotHandshakeResponse = true;
      messageHandled = true;
    } else if (messageType == InputChannelMessageType::Event) {
      for (auto &&rc : registered_clients)
        sendToClient(rc, channelId, 0x00, message.content);
      messageHandled = true;
    }
  }
  cv.notify_all();
  return messageHandled;
}

bool InputChannelHandler::handleMessageFromClient(int clientId,
                                                  uint8_t channelId,
                                                  bool specific,
                                                  const vector<uint8_t> &data) {
  registered_clients.insert(clientId);
  ChannelHandler::openChannel();
  gotHandshakeResponse = false;
  sendHandshakeRequest();
  expectHandshakeResponse();
  return true;
}

void InputChannelHandler::disconnected(int clientId) {
  registered_clients.erase(clientId);
}
