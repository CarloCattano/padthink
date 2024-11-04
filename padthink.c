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
#include <pthread.h>

#ifdef __linux__
  #include <linux/input.h>
  #include <unistd.h>
#endif

// TODO
//  - Change polling to internal (no external polling)

#define THINKPAD
// needs chmod to read permission on *device file
// sudo chmod 444 /dev/input/by-path/platform-i8042-serio-1-event-mouse
// or make the user a member of the input group

#ifdef THINKPAD
#define DEFAULT_DEVICE "/dev/input/by-path/platform-i8042-serio-1-event-mouse"
#define DEF_TOUCHPAD "/dev/input/by-path/pci-0000:00:15.0-platform-i2c_designware.0-event-mouse"
#endif

static t_class *padthink_class;

#ifdef __linux__
struct input_event tpoint_ev;
struct input_event tpad_ev;
#endif

char *device;

typedef struct _padthink {
  
  t_object      x_obj;
  t_int         open_fds[2];
  t_int         nipple_fd;
  t_int         touchpad_fd;

  t_int         polling;
  t_int         poll_interval;
  t_int         n_ret;
  t_int         t_ret;
  
  t_atom        nip_xy[2];
  t_atom        a_bt[4];
  t_atom        touchpad_xy[2];

  t_outlet      *out_nipple;
  t_outlet      *out_buttons;
  t_outlet      *out_touchpad;

}               t_padthink;


// abstracted read nipple and touchpad functionality
// - read nipple and touchpad events
// - store it in tpad_ev and tpoint_ev to be read by padthink_bang
//
void padthink_read(t_padthink *x) {
  x->n_ret = read(x->nipple_fd, &tpoint_ev, sizeof(struct input_event));
  if (x->n_ret != sizeof(struct input_event)) x->n_ret = -1;


  x->t_ret = read(x->touchpad_fd, &tpad_ev, sizeof(struct input_event));
  if (x->t_ret != sizeof(struct input_event)) x->t_ret = -1;
}

void padthink_bang(t_padthink *x) {

  // padthink_read(x);

  if (x->n_ret == -1 && x->t_ret == -1) {
    return;
  }

  int buttons[4] = {0, 0, 0, 0};

  if (x->t_ret != -1) {
    if (tpad_ev.code == 54 && tpad_ev.type == 3) {
      SETFLOAT(&x->touchpad_xy[1], tpad_ev.value);
    } else if (tpad_ev.code == 53 && tpad_ev.type == 3){
      SETFLOAT(&x->touchpad_xy[0], tpad_ev.value);
    } else if (tpad_ev.code == 0x110 && tpad_ev.type == 0x01) {
      SETFLOAT(&x->a_bt[3], tpad_ev.value);
      buttons[3] = tpad_ev.value;
    } 
    // post("Tp : %d %d %d", tpad_ev.type, tpad_ev.code, tpad_ev.value);
  }

  if(x->n_ret){
    // EV_REL - trackpoint relative movement
    if (tpoint_ev.type == EV_REL) {
      if (tpoint_ev.code == 1) {
        SETFLOAT(&x->nip_xy[0], tpoint_ev.value);
      } else if (tpoint_ev.code == 0) {
        SETFLOAT(&x->nip_xy[1], tpoint_ev.value);
      }
    }
    // EV_KEY - mouse button event
    if (tpoint_ev.code == BTN_LEFT || tpoint_ev.code == BTN_RIGHT || tpoint_ev.code == BTN_MIDDLE) {
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

  outlet_list(x->out_touchpad, &s_list, 2, x->touchpad_xy);
  outlet_list(x->out_buttons, &s_list, 4, x->a_bt);
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


void set_device(t_padthink *x, t_symbol *s) {

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

  x->polling = 1;
  x->poll_interval = 10000; // 10ms

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

  inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_symbol, &s_symbol);
  inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, &s_float);

  return (void *)x;
}


void *polling_thread(void *arg) {
  t_padthink *x = (t_padthink *)arg;
  while (x->polling) {
    padthink_read(x);
    padthink_bang(x);
    usleep(x->poll_interval); // Sleep for 10ms to prevent high CPU usage
  }
  return NULL;
}

void start_poll(t_padthink *x, t_symbol *s) {
  x->polling = 1;
  pthread_t thread_id;
  pthread_create(&thread_id, NULL, polling_thread, (void *)x);
  pthread_detach(thread_id);
}

void set_poll_sleep(t_padthink *x, t_floatarg f) {
  x->poll_interval = f * 1000;
}

void stop_poll(t_padthink *x) {
  x->polling = 0;
}

void padthink_free(t_padthink *x) {
  if (x->open_fds[0] != -1)
    close(x->open_fds[0]);
  if (x->open_fds[1] != -1)
    close(x->open_fds[1]);
}

void padthink_turnon(t_padthink *x, t_floatarg f) {
  if (f == 1) {
    start_poll(x, NULL);
  } else {
    x->polling = 0;
  }
}

void padthink_setup(void) {
  padthink_class = class_new(gensym("padthink"), (t_newmethod)padthink_new, (t_method)padthink_free,
                             sizeof(t_padthink), CLASS_DEFAULT, 0);

  // add method for float input
  class_addfloat(padthink_class, (t_method)padthink_turnon);

  class_addmethod(padthink_class, (t_method)set_device, gensym("set"), A_DEFSYM,0);
  class_addmethod(padthink_class, (t_method)start_poll, gensym("start"), A_DEFSYM, 0);
  class_addmethod(padthink_class, (t_method)set_poll_sleep, gensym("poll"), A_DEFFLOAT, 0);
  class_addmethod(padthink_class, (t_method)stop_poll, gensym("stop"), 0);

  class_sethelpsymbol(padthink_class, gensym("padthink-help"));
}
