// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "AaCommunicator.h"
#include "ChannelType.h"
#include "Library.h"
#include "ManualResetEvent.h"
#include "ModeSwitcher.h"
#include "Packet.h"
#include "PacketType.h"
#include "SocketCommunicator.h"
#include "Udc.h"
#include <csignal>
#include <iostream>
#include <iterator>
#include <stdexcept>

using namespace std;

string configFsBasePath = "/sys/kernel/config";
ManualResetEvent mre;

void signal_handler(int signal) {
  cout << "Quitting..." << endl;
  mre.set();
  std::signal(SIGINT, SIG_DFL);
}

int main() {
  signal(SIGINT, signal_handler);
  try {
    Library lib(configFsBasePath);
    ModeSwitcher::handleSwitchToAccessoryMode(lib);
    AaCommunicator aac(lib);
    aac.setup(Udc::getUdcById(lib, 0));
    aac.error.connect([&](const std::exception &ex) {
      cout << "Error:" << endl;
      cout << ex.what() << endl;
      mre.set();
    });
    map<SocketClient *, int> clients;
    int hi = 0;
    aac.gotMessage.connect([&clients, &hi](int clientId, int channelNumber,
                                           bool specific,
                                           vector<uint8_t> data) {
      cout << hi++ << " data from headunit: " << channelNumber << " "
           << data.size() << endl;
      vector<uint8_t> msg;
      msg.push_back(channelNumber);
      msg.push_back(specific ? 0xff : 0x00);
      copy(data.begin(), data.end(), back_inserter(msg));
      for (auto cl : clients) {
        if (cl.second == clientId || clientId == -1)
          cl.first->sendMessage(msg);
      }
    });
    SocketCommunicator sc("./socket");
    int pi = 0;
    int clientCount = 0;
    sc.newClient.connect([&](SocketClient *scl) {
      clients.insert({scl, clientCount++});
      cout << "connect: " << clients[scl] << endl;
      scl->gotPacket.connect([&aac, scl, &pi, &clients](const Packet &p) {
        if (p.packetType == PacketType::GetChannelNumberByChannelType) {
          cout << "open channel: " << (int)p.channelNumber << endl;
          auto channelId =
              aac.getChannelNumberByChannelType((ChannelType)p.channelNumber);
          scl->sendMessage({channelId});
        } else if (p.packetType == PacketType::RawData) {
          cout << pi++ << " data from phone: " << (int)p.channelNumber << " "
               << (int)p.specific << " " << p.data.size() << endl;
          aac.sendToChannel(clients[scl], p.channelNumber, p.specific, p.data);
        } else if (p.packetType == PacketType::GetServiceDescriptor) {
          cout << "get service descriptor" << endl;
          scl->sendMessage(aac.getServiceDescriptor());
        } else {
          throw runtime_error("Unknown packetType");
        }
      });
      scl->disconnected.connect([&aac, &clients, scl]() {
        cout << "disconnected: " << clients[scl] << endl;
        aac.disconnected(clients[scl]);
        clients.erase(scl);
      });
    });
    mre.wait();
  } catch (const exception &ex) {
    cout << "Exception:" << endl;
    cout << ex.what() << endl;
    return 1;
  }
  return 0;
}
