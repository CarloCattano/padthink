/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   test_dev_input_linux.cpp                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: carlo <no@way.zip>                         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/06 13:01:33 by carlo             #+#    #+#             */
/*                                                    ###   ########.fr       */
/*                                                                            */
/*    Just a helper to draft the codes and basic logic                        */
/*    needed for new functionalities to be implemented later                  */
/*    in the C external                                                       */
/*                                                                            */
/* ************************************************************************** */

#include <fcntl.h>
#include <iostream>
#include <linux/input.h>
#include <signal.h>
#include <unistd.h>

int open_fd = -1;

int ignorecodes[9][2] = {
    {0, 0},  // MESSAGE_ZERO
    {5, 4},  // TIMER
    {0, 3},  // trackpad x
    {1, 3},  // trackpad y
    {54, 3}, // TRACKPAD_X
    {53, 3}, // TRACKPAD_Y
    {57, 3}, // trackpad fingerdown
    {47, 3}, // scrolling 47, 3
    {325, 1} // touch?
};

int fingers[4][2] = {
    {330, 1}, // finger 1
    {333, 1}, // finger 2
    {334, 1}, // finger 3
    {335, 1}, // finger 4
};

int fingers_count = 0;

void fingers_down(input_event ev) {
  int size = sizeof(fingers) / sizeof(fingers[0]);
  for (int i = size - 1; i >= 0; i--) {
    if (fingers[i][0] == ev.code && fingers[i][1] == ev.type) {
      if (ev.value == 1) {
        fingers_count = i + 1;
        std::cout << i + 1 << " down" << std::endl;
        break;
      }
      // no fingers down
      if (i == 0) {
        fingers_count = 0;
        std::cout << "0 fingers down" << std::endl;
      }
    }
  }
}

void signal_handler(volatile sig_atomic_t signum) {
  if (open_fd != -1) {
    std::cout << "Closed file descriptor" << std::endl;
    close(open_fd);
  }
  exit(signum);
}

bool ignore_code(int code, int type) {

  int size = sizeof(ignorecodes) / sizeof(ignorecodes[0]);
  for (int i = 0; i < size; i++) {
    if (ignorecodes[i][0] == code && ignorecodes[i][1] == type)
      return false;
  }

  return true;
}

int main() {
  int fd = open("/dev/input/event9", O_RDONLY);

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

    if (!ignore_code(ev.code, ev.type))
      continue;

    if (ev.type == 1) {
      fingers_down(ev);
    }

    std::cout << "C " << ev.code << " T " << ev.type << " V " << ev.value
              << std::endl;
  }

  close(fd);
  open_fd = -1;
  return 0;
}
