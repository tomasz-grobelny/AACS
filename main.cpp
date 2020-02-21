// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "AaCommunicator.h"
#include "Library.h"
#include "ManualResetEvent.h"
#include "ModeSwitcher.h"
#include "SocketCommunicator.h"
#include "Udc.h"
#include <iostream>

using namespace std;

string configFsBasePath = "/sys/kernel/config";

int main() {
  try {
    ManualResetEvent mre;
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
