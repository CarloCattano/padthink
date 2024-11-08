#include <iostream>

#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

// signal handler
#include <signal.h>

int open_fd = -1;

void signal_handler(volatile sig_atomic_t signum) {
  if (open_fd != -1) {
    std::cout << "Closed file descriptor" << std::endl;
    close(open_fd);
  }
  exit(signum);
}

int main() {
  // int fd = open("/dev/input/by-path/"
  //               "pci-0000:00:15.0-platform-i2c_designware.0-event-mouse",
  // O_RDONLY | O_NONBLOCK);

  int fd = open("/dev/input/by-path/platform-i8042-serio-1-event-mouse",
                O_RDONLY | O_NONBLOCK);

  if (fd == -1) {
    std::cerr << "Error: Cannot open device" << std::endl;
    return 1;
  }

  open_fd = fd;

  struct sigaction sa;
  sa.sa_handler = signal_handler;

  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  struct input_event ev;

  while (true) {
    int ret = read(fd, &ev, sizeof(struct input_event));

    if (ret == -1 || ret != sizeof(struct input_event))
      continue;

    // touchpad x y
    // if (ev.code == 57 && ev.type == 3) { // touch event with counter
    //   std::cout << "Touch event: " << ev.value << std::endl;
    // } else if (ev.code == 54 && ev.type == 3)
    //   std::cout << "Touchpad X: " << ev.value << std::endl;
    // else if (ev.code == 53 && ev.type == 3) // touchpad y
    //   std::cout << "Touchpad Y: " << ev.value << std::endl;

    // if (ev.code == REL_Y && ev.type == EV_REL) {
    //   std::cout << "Relative Y: " << ev.value << std::endl;
    // } else if (ev.code == REL_X && ev.type == EV_REL) {
    //   std::cout << "Relative X: " << ev.value << std::endl;
    // } else {
    std::cout << "Code: " << ev.code << " Type: " << ev.type
              << " Value: " << ev.value << std::endl;
    // }

    // if (ev.code == 57 && ev.type == 3) {
    //   // std::cout << "57 event: " << ev.value << std::endl;
    // } else if (ev.code == 0 && ev.type == 3) {
    //   // std::cout << "Y TOUCHPAD VAL: " << ev.value << std::endl;
    // } else if (ev.code == 54 && ev.type == 3) {
    //   // std::cout << "Y TOUCHPAD VAL: " << ev.value << std::endl;
    // } else if (ev.code == 53 && ev.type == 3) {
    //   // std::cout << "X TOUCHPAD VAL: " << ev.value << std::endl;
    // } else if (ev.code == 330 && ev.type == EV_KEY) {
    //   // std::cout << "two finger click: " << ev.value << std::endl;
    // } else if (ev.code == 0 && ev.type == 0) {
    //   // std::cout << "0 event: " << ev.value << std::endl;
    // } else if (ev.code == 1 && ev.type == 3) {
    //   // std::cout << "WTF: " << ev.value << std::endl;
    // } else if (ev.code == 5 && ev.type == 4) {
    //   // std::cout << "5 event: " << ev.value << std::endl;
    // } else if (ev.code == 0x110 && ev.type == EV_KEY) {
    //   // std::cout << "Left button: " << ev.value << std::endl;
    //   std::cout << "middle trackpad click press" << ev.value << std::endl;
    // } else if (ev.code == BTN_RIGHT && ev.type == EV_KEY) {
    //   // std::cout << "Right button: " << ev.value << std::endl;
    // } else if (ev.code == BTN_MIDDLE && ev.type == EV_KEY) {
    //   // std::cout << "Middle button: " << ev.value << std::endl;
    // } else
    //   std::cout << "Code: " << ev.code << " Type: " << ev.type
    //             << " Value: " << ev.value << std::endl;
  }

  close(fd);
  open_fd = -1;
  return 0;
}
