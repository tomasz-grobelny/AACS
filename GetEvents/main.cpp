#include "InputEvent.pb.h"
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

using namespace std;

uint8_t openChannel(int aaServerFd) {
  uint8_t buffer[3];
  buffer[0] = 0; // packet type == GetChannelNumberByChannelType
  buffer[1] = 1; // channel type == Input
  buffer[2] = 0; // specific?
  if (write(aaServerFd, buffer, sizeof buffer) != sizeof buffer)
    throw runtime_error("failed to write open channel");
  uint8_t channelNumber;
  if (read(aaServerFd, &channelNumber, sizeof(channelNumber)) !=
      sizeof(channelNumber))
    throw runtime_error("failed to read video channel number");
  cout << "channelNumber: " << to_string(channelNumber) << endl;
  buffer[0] = 1; // packet type == Raw
  buffer[1] = channelNumber;
  buffer[2] = 0; // specific? don't care
  if (write(aaServerFd, buffer, sizeof buffer) != sizeof buffer)
    throw runtime_error("failed to write open channel");
  return channelNumber;
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

Window *list_windows(Display *disp, unsigned long *len) {
  Atom prop = XInternAtom(disp, "_NET_CLIENT_LIST", False), type;
  int form;
  unsigned long remain;
  unsigned char *list;

  if (XGetWindowProperty(disp, XDefaultRootWindow(disp), prop, 0, 1024, False,
                         XA_WINDOW, &type, &form, len, &remain,
                         &list) != Success) {
    return 0;
  }

  return (Window *)list;
}

string get_name(Display *disp, Window win) {
  Atom prop = XInternAtom(disp, "WM_NAME", False), type;
  int form;
  unsigned long remain, len;
  unsigned char *list;

  if (XGetWindowProperty(disp, win, prop, 0, 1024, False, XA_STRING, &type,
                         &form, &len, &remain, &list) != Success) {
    return NULL;
  }
  string name((char *)list);
  free(list);
  return name;
}

Window get_window_by_name(Display *disp, string name) {
  unsigned long len;
  // memleak
  auto list = (Window *)list_windows(disp, &len);
  for (auto i = 0; i < (int)len; i++) {
    if (name == get_name(disp, list[i])) {
      return list[i];
    }
  }
  return None;
}

tuple<int, int> translate_coordinates(Display *disp, Window window, int x,
                                      int y) {
  int dest_x;
  int dest_y;
  Window unused;
  XTranslateCoordinates(disp, window, DefaultRootWindow(disp), x, y, &dest_x,
                        &dest_y, &unused);
  return make_tuple(dest_x, dest_y);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    cout << "Needs 2 arguments: socket and window name" << endl;
    exit(1);
  }
  auto fd = getSocketFd(argv[1]);
  openChannel(fd);

  Display *disp = XOpenDisplay(NULL);
  auto window = get_window_by_name(disp, argv[2]);

  const auto maxBufSize = 1024;
  uint8_t buffer[maxBufSize];
  while (true) {
    auto realSize = read(fd, buffer, maxBufSize);
    if (realSize <= 0)
      throw runtime_error("failed to read video channel number");
    class tag::aas::InputEvent ie;
    ie.ParseFromArray(buffer + 4, realSize - 4);
    cout << "===" << endl;
    cout << ie.DebugString() << endl;
    if (!ie.has_touch_event())
      continue;
    for (auto tl : ie.touch_event().touch_location()) {
      auto coords = translate_coordinates(disp, window, tl.x(), tl.y());
      cout << "x: " << get<0>(coords) << endl;
      cout << "y: " << get<1>(coords) << endl;
      cout << "pid: " << tl.pid() << endl;
      XTestFakeMotionEvent(disp, 0, get<0>(coords),
                           get<1>(coords), CurrentTime);
    }
    if (ie.touch_event().touch_action() == tag::aas::TouchAction::Press ||
        ie.touch_event().touch_action() == tag::aas::TouchAction::Down) {
      cout << "press" << endl;
      XTestFakeButtonEvent(disp, 1, True, CurrentTime);
    }
    if (ie.touch_event().touch_action() == tag::aas::TouchAction::Release ||
        ie.touch_event().touch_action() == tag::aas::TouchAction::Up) {
      cout << "release" << endl;
      XTestFakeButtonEvent(disp, 1, False, CurrentTime);
    }
    XFlush(disp);
  }
  return 0;
}
