#define _GNU_SOURCE
#include <SDL.h>
#include "lux.h"
#include <unistd.h>

static int screen_width = 640, screen_height = 480;

static bool parse_dimensions (const char * d, int * ww, int * hh)
{
  int w = *ww, h = *hh;
  if (sscanf(d, "%i,%i", &w, &h) != 2)
  {
    if (sscanf(d, "%ix%i", &w, &h) != 2)
    {
      return false;
    }
  }

  if (w < 20) w = 20;
  if (h < 10) h = 10;

  *ww = w; *hh = h;

  return true;
}

void my_mouse_handler (Window * w, int x, int y, int buttons, int type, bool raised)
{
  int yy = 0;
  window_clear_client(w, 0x330033);
  char buf[64];
  if (type != SDL_MOUSEBUTTONDOWN) yy += lux_sysfont_h();
  sprintf(buf, "%i at %i,%i", buttons, x, y);
  text_draw(w->surf, buf, 0, yy, 0xffFFff);
}

void my_mousemove_handler (Window * w, int x, int y, int buttons, int dx, int dy)
{
  if (window_is_top(w)) return;
  window_clear_client(w, 0x000000);
  char buf[64];
  sprintf(buf, "%i at %i,%i", buttons, x, y);
  text_draw(w->surf, buf, 0, 0, 0xffFFff);
}

void my_key_handler (Window * w, SDL_keysym * k, bool down)
{
  SDL_Rect r;
  int yy = 0;
  window_clear_client(w, 0);
  char buf[64];
  char c = '?';
  if (k->sym < 128 && k->sym >= ' ') c = (char)k->sym;
  if (!down) yy += lux_sysfont_h();
  sprintf(buf, "%s code:%i mod:%i char:%c", down?"KEY":"key",k->sym, k->mod, c);
  text_draw(w->surf, buf, 0, yy, 0xffFFff);
}

static int num = 0;

void my_f1_handler (FKey * fkey)
{
  char buf[64];
  sprintf(buf, "Window %i", ++num);
  Window * w = window_create(200, 100, buf, WIN_F_RESIZE|WIN_F_AUTOSURF);
  w->on_mousedown = my_mouse_handler;
  w->on_mouseup = my_mouse_handler;
  w->on_mousemove = my_mousemove_handler;
  w->on_keydown = my_key_handler;
  w->on_keyup = my_key_handler;
  w->bg_color = lux_get_theme().win.face;
}


int main (int argc, char * argv[])
{
  int opt;
  uint32_t def_bg_color = 0x54699e;
  while ((opt = getopt(argc, argv, "d:")) != -1)
  {
    switch (opt)
    {
      case 'd':
        parse_dimensions(optarg, &screen_width, &screen_height);
        break;
    }
  }
  lux_set_bg_color(def_bg_color);


  lux_init(screen_width, screen_height, NULL);

  key_register_fkey(SDLK_F1, KMOD_NONE, my_f1_handler);

  lux_loop();

  return 0;
}
