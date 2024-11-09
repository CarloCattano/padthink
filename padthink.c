/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   padthink.c                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: carlo                                      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/02 19:20:07 by carlo             #+#    #+#             */
/*   Updated: 2024/11/04 12:25:11 by carlo            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "m_pd.h"
#include <fcntl.h>

#ifdef __linux__
  #include <linux/input.h>
  #include <poll.h>
  #include <unistd.h>
#endif

#define THINKPAD

#define SMOOTH 10

// needs chmod to read permission on *device file
// sudo chmod 444 /dev/input/by-path/platform-i8042-serio-1-event-mouse
// or make the user a member of the input group

#ifdef THINKPAD
#define DEFAULT_DEVICE "/dev/input/by-path/platform-i8042-serio-1-event-mouse"
#define DEF_TOUCHPAD                                                           \
  "/dev/input/by-path/pci-0000:00:15.0-platform-i2c_designware.0-event-mouse"
#endif



static t_class *padthink_class;

#ifdef __linux__
  struct input_event tpoint_ev;
  struct input_event tpad_ev;
#endif

char *device;

typedef struct _padthink {

  t_object x_obj;

  t_int open_fds[2];
  t_int nipple_fd;
  t_int touchpad_fd;

  t_int polling;
  t_int poll_interval;
  t_int n_ret;
  t_int t_ret;

  t_clock *x_clock;

  // poll fd strucst
  struct pollfd poll_fds[2];

  t_atom nip_xy[2];
  t_atom a_bt[4];
  t_atom touchpad_xy[2];

  t_outlet *out_nipple;
  t_outlet *out_buttons;
  t_outlet *out_touchpad;

} t_padthink;


void padthink_poll_callback(t_padthink *x);

// TODO: implement smoothing from sampling
//       values between readings offset

void padthink_process(t_padthink *x) {

  int buttons[4] = {0, 0, 0, 0};


  if (x->t_ret != -1) 
  {
    if (tpad_ev.code == 54 && tpad_ev.type == 3) {
      SETFLOAT(&x->touchpad_xy[1], tpad_ev.value);
    } 
    if (tpad_ev.code == 53 && tpad_ev.type == 3) {
      SETFLOAT(&x->touchpad_xy[0], tpad_ev.value);
    } 
    
    if (tpad_ev.code == 0x110 && tpad_ev.type == 0x01) {
      SETFLOAT(&x->a_bt[3], tpad_ev.value);
      buttons[3] = tpad_ev.value;
    }
  }
  if (x->n_ret) {
    // EV_REL - trackpoint relative movement
    if (tpoint_ev.type == EV_REL) {
      if (tpoint_ev.code == 1) {
        SETFLOAT(&x->nip_xy[0], tpoint_ev.value);
        }
      if (tpoint_ev.code == 0) {
        SETFLOAT(&x->nip_xy[1], tpoint_ev.value);
      }
    }
    // EV_KEY - mouse button event
    if (tpoint_ev.code == BTN_LEFT || tpoint_ev.code == BTN_RIGHT ||
        tpoint_ev.code == BTN_MIDDLE) 
    {
      if (tpoint_ev.code == BTN_LEFT) {
        buttons[0] = tpoint_ev.value;
      } else if (tpoint_ev.code == BTN_RIGHT) {
        buttons[1] = tpoint_ev.value;
      } else if (tpoint_ev.code == BTN_MIDDLE) {
        buttons[2] = tpoint_ev.value;
      }
      SETFLOAT(&x->a_bt[0], buttons[0]);
      SETFLOAT(&x->a_bt[1], buttons[1]);
      SETFLOAT(&x->a_bt[2], buttons[2]);
      SETFLOAT(&x->a_bt[3], buttons[3]);
    }
  }    

  outlet_list(x->out_buttons, &s_list, 4, x->a_bt);
  outlet_list(x->out_touchpad, &s_list, 2, x->touchpad_xy);
  outlet_list(x->out_nipple, &s_list, 2, x->nip_xy);
}
static void init_atoms(t_padthink *x) {
  SETFLOAT(&x->nip_xy[0], 0);
  SETFLOAT(&x->nip_xy[1], 0);

  SETFLOAT(&x->a_bt[0], 0);
  SETFLOAT(&x->a_bt[1], 0);
  SETFLOAT(&x->a_bt[2], 0);
  SETFLOAT(&x->a_bt[3], 0);

  SETFLOAT(&x->touchpad_xy[0], 0);
  SETFLOAT(&x->touchpad_xy[1], 0);
}

void set_touchpad_device(t_padthink *x, t_symbol *s) {

  close(x->open_fds[1]);
  x->touchpad_fd = open(s->s_name, O_RDONLY );

  if (x->touchpad_fd == -1) {
    post("Error: Cannot open given device");
    post("Using default device");

    x->touchpad_fd = open(DEF_TOUCHPAD, O_RDONLY );
    if (x->touchpad_fd == -1) {
      post("Error: Cannot open default device");
      return;
    }
    x->open_fds[1] = x->touchpad_fd;
  }

  x->open_fds[1] = x->touchpad_fd;
  return;
}
void set_trackpoint_device(t_padthink *x, t_symbol *s) {

  post("set_device: %s", s->s_name);
  close(x->open_fds[0]);

  x->nipple_fd = open(s->s_name, O_RDONLY | O_NONBLOCK);

  if (x->nipple_fd == -1) {
    post("Error: Cannot open given device");
    post("Using default device");

    x->nipple_fd = open(DEFAULT_DEVICE, O_RDONLY | O_NONBLOCK);
    if (x->nipple_fd == -1) {
      post("Error: Cannot open default device");
      return;
    }
    x->open_fds[0] = x->nipple_fd;
  }

  x->open_fds[0] = x->nipple_fd;
  return;
}

void *padthink_new(void) {
  t_padthink *x = (t_padthink *)pd_new(padthink_class);

  x->nipple_fd = open(DEFAULT_DEVICE, O_RDONLY | O_NONBLOCK);
  x->touchpad_fd = open(DEF_TOUCHPAD, O_RDONLY | O_NONBLOCK);

  x->polling = 0;
  x->poll_interval = 2; // 2 ? whats the unit in pd t_clock ? 

  x->x_clock = clock_new(x, (t_method)padthink_poll_callback);

  x->open_fds[0] = -1;
  x->open_fds[1] = -1;

  if (x->nipple_fd == -1) {
    post("Error: Cannot open trackpoint device");
    return NULL;
  }

  if (x->touchpad_fd == -1) {
    post("Error: Cannot open touchpad device");
    return NULL;
  }

  x->open_fds[0] = x->nipple_fd;
  x->open_fds[1] = x->touchpad_fd;

  init_atoms(x);

  x->out_nipple = outlet_new(&x->x_obj, &s_list);
  x->out_buttons = outlet_new(&x->x_obj, &s_list);
  x->out_touchpad = outlet_new(&x->x_obj, &s_list);

  return (void *)x;
}

void padthink_poll(t_padthink *x) {
  x->poll_fds[0].fd = x->open_fds[0];
  x->poll_fds[0].events = POLLIN;
  x->poll_fds[1].fd = x->open_fds[1];
  x->poll_fds[1].events = POLLIN;
  
  if (x->open_fds[0] == -1 || x->open_fds[1] == -1) {
    post("Error: Invalid file descriptors for polling.");
    return;
  }

  int ret = poll(x->poll_fds, 2, 0);
  if (ret == -1 || ret == 0)
      return;

  if (x->poll_fds[0].revents & POLLIN) {
      x->n_ret = read(x->open_fds[0], &tpoint_ev, sizeof(tpoint_ev));
      if (x->n_ret == -1) {
          perror("Read error from trackpoint");
      } else if (x->n_ret == sizeof(tpoint_ev)) {
          padthink_process(x);
      }
  }
  if (x->poll_fds[1].revents & POLLIN) {
      x->t_ret = read(x->open_fds[1], &tpad_ev, sizeof(tpad_ev));
      if (x->t_ret == -1) {
          perror("Read error from touchpad");
      } else if (x->t_ret == sizeof(tpad_ev)) {
          padthink_process(x);
      }
  }
}

void padthink_poll_callback(t_padthink *x) {
  padthink_poll(x);

  if (x->polling) {
    clock_delay(x->x_clock, x->poll_interval);
  }
}


void start_poll(t_padthink *x) {
  if (!x->polling) {
    x->polling = 1;
    clock_delay(x->x_clock, 0);
  }
}

void set_poll_sleep(t_padthink *x, t_floatarg f) {
  x->poll_interval = f * 1000;
}

void stop_poll(t_padthink *x) {
  if (x->polling) {
    x->polling = 0;
    clock_unset(x->x_clock);  // Stop the clock
  }
}

void padthink_free(t_padthink *x) {
  if (x->open_fds[0] != -1) {
    close(x->open_fds[0]);
    x->open_fds[0] = -1;
  }
  if (x->open_fds[1] != -1) {
    close(x->open_fds[1]);
    x->open_fds[1] = -1;
  }
  if (x->polling == 1) {
    stop_poll(x);
  }

  if (x->x_clock) {
    clock_free(x->x_clock);
  }
}

void padthink_turn_on(t_padthink *x, t_floatarg f) {
  f == 1 ? start_poll(x) : stop_poll(x);
}

void padthink_setup(void) {
  padthink_class =
      class_new(gensym("padthink"), (t_newmethod)padthink_new,
                (t_method)padthink_free, sizeof(t_padthink), CLASS_DEFAULT, 0);

  class_addfloat(padthink_class, (t_method)padthink_turn_on);

  class_addmethod(padthink_class, (t_method)set_touchpad_device,
                  gensym("touchpad"), A_DEFSYM, 0);
  class_addmethod(padthink_class, (t_method)set_trackpoint_device,
                  gensym("trackpoint"), A_DEFSYM, 0);
  class_addmethod(padthink_class, (t_method)start_poll, gensym("start"),
                  A_DEFSYM, 0);
  class_addmethod(padthink_class, (t_method)set_poll_sleep, gensym("poll"),
                  A_DEFFLOAT, 0);
  class_addmethod(padthink_class, (t_method)stop_poll, gensym("stop"), 0);

  class_sethelpsymbol(padthink_class, gensym("padthink-help"));
}
