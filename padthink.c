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

  t_atom        nip_xy[2];
  t_atom        a_bt[3];
  t_atom        touchpad_xy[2];

  t_outlet      *out_nipple;
  t_outlet      *out_buttons;
  t_outlet      *out_touchpad;

}               t_padthink;


void padthink_bang(t_padthink *x) {

  int n_ret = read(x->nipple_fd, &tpoint_ev, sizeof(struct input_event));
  if (n_ret != sizeof(struct input_event)) 
    n_ret = -1;

  int t_ret = read(x->touchpad_fd, &tpad_ev, sizeof(struct input_event));
  if (t_ret != sizeof(struct input_event)) 
    t_ret = -1;
  
  if (n_ret == -1 && t_ret == -1) 
    return;
  

  if (t_ret != -1) {
    if (tpad_ev.code == 57 && tpad_ev.type == 3) { // touch event with counter
      // post("Touchpad Y: %d", tpad_ev.value);
    } else if (tpad_ev.code == 54 && tpad_ev.type == 3) {
      SETFLOAT(&x->touchpad_xy[0], tpad_ev.value);
    } else if (tpad_ev.code == 53 && tpad_ev.type == 3){
      SETFLOAT(&x->touchpad_xy[1], tpad_ev.value);
    }
  }

  if(n_ret){
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
      int buttons[3] = {0, 0, 0};
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
    }
  }

  outlet_list(x->out_touchpad, &s_list, 2, x->touchpad_xy);
  outlet_list(x->out_buttons, &s_list, 3, x->a_bt);
  outlet_list(x->out_nipple, &s_list, 2, x->nip_xy);
}


static void init_atoms(t_padthink *x) {
  SETFLOAT(&x->nip_xy[0], 0);
  SETFLOAT(&x->nip_xy[1], 0);
  SETFLOAT(&x->a_bt[0], 0);
  SETFLOAT(&x->a_bt[1], 0);
  SETFLOAT(&x->a_bt[2], 0);
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
  inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_bang, &s_bang);

  return (void *)x;
}

void padthink_free(t_padthink *x) {
  if (x->open_fds[0] != -1)
    close(x->open_fds[0]);
  if (x->open_fds[1] != -1)
    close(x->open_fds[1]);
}

void padthink_setup(void) {
  padthink_class = class_new(gensym("padthink"), (t_newmethod)padthink_new, (t_method)padthink_free,
                             sizeof(t_padthink), CLASS_DEFAULT, 0);

  class_addbang(padthink_class, padthink_bang);

  class_addmethod(padthink_class, (t_method)set_device, gensym("set"), A_DEFSYM,0);
}
