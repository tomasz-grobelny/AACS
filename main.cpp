// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "AaCommunicator.h"
#include "Library.h"
#include "ManualResetEvent.h"
#include "ModeSwitcher.h"
#include "SocketCommunicator.h"
#include "Udc.h"
#include <csignal>
#include <iostream>

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
    SocketCommunicator sc("./socket");
    sc.newClient.connect([&](SocketClient *scl) {
      cout << "openChannel" << endl;
      aac.openChannel(ChannelType::Video);
      scl->gotPacket.connect([&aac](const Packet &p) {
        cout << "sendToChannel" << endl;
        aac.sendToChannel(ChannelType::Video, p.data);
      });
      scl->disconnected.connect([&aac]() {
        cout << "closeChannel" << endl;
        aac.closeChannel(ChannelType::Video);
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
