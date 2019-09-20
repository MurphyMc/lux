#define _POSIX_C_SOURCE 200809L
#include <SDL.h>
#include "lux.h"
#include <stdlib.h>
#include <string.h>

#include "font.h"


#ifndef LUX_SWITCHER_KEY
#define LUX_SWITCHER_KEY   SDLK_BACKQUOTE
#endif
#ifndef LUX_SWITCHER_MODS
#define LUX_SWITCHER_MODS  (KMOD_LALT|KMOD_RALT)
#endif


SDL_Surface * screen;
SDL_Surface * font;


// -- Color ------------------------------------------------------------------

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
  Uint32 rmask = 0xff000000;
  Uint32 gmask = 0x00ff0000;
  Uint32 bmask = 0x0000ff00;
  Uint32 amask = 0x000000ff;
#else
  Uint32 bmask = 0x000000ff;
  Uint32 gmask = 0x0000ff00;
  Uint32 rmask = 0x00ff0000;
  Uint32 amask = 0xff000000;
#endif


// -- Cursor -----------------------------------------------------------------

/* Stolen from the SDL documentation, which stole it from the mailing list */
/* Creates a new mouse cursor from an XPM */

/* XPM */
static const char *arrow[] = {
  /* width height num_colors chars_per_pixel */
  "    32    32        3            1",
  /* colors */
  ". c #000000",
  "X c #ffffff",
  "  c None",
  /* pixels */
  "X                               ",
  "XX                              ",
  "X.X                             ",
  "X..X                            ",
  "X...X                           ",
  "X....X                          ",
  "X.....X                         ",
  "X......X                        ",
  "X.......X                       ",
  "X........X                      ",
  "X.....XXXXX                     ",
  "X..X..X                         ",
  "X.X X..X                        ",
  "XX  X..X                        ",
  "X    X..X                       ",
  "     X..X                       ",
  "      X..X                      ",
  "      X..X                      ",
  "       XX                       ",
  "                                ",
  "                                ",
  "                                ",
  "                                ",
  "                                ",
  "                                ",
  "                                ",
  "                                ",
  "                                ",
  "                                ",
  "                                ",
  "                                ",
  "                                ",
  "0,0"
};

static SDL_Cursor * create_cursor_xpm (const char *image[])
{
  int i, row, col;
  Uint8 data[4*32];
  Uint8 mask[4*32];
  int hot_x, hot_y;

  i = -1;
  for (row=0; row<32; ++row) {
    for (col=0; col<32; ++col) {
      if (col % 8) {
        data[i] <<= 1;
        mask[i] <<= 1;
      } else {
        ++i;
        data[i] = mask[i] = 0;
      }
      switch (image[4+row][col]) {
        case 'X':
          data[i] |= 0x01;
          mask[i] |= 0x01;
          break;
        case '.':
          mask[i] |= 0x01;
          break;
        case ' ':
          break;
      }
    }
  }
  sscanf(image[4+row], "%d,%d", &hot_x, &hot_y);
  return SDL_CreateCursor(data, mask, 32, 32, hot_x, hot_y);
}


// -- Keyboard ---------------------------------------------------------------

#define MAX_KEYS_DOWN 5
#define SDL_KEY_INVALID SDLK_LAST

static SDL_keysym _pressed_keys[MAX_KEYS_DOWN];
int _pressed_keys_count = 0;

#define MAX_FKEYS 15

FKey _key_fkeys[MAX_FKEYS] = {};
int _key_fkeys_count = 0;

// returns true on success
bool key_register_fkey (SDLKey key, SDLMod mods, void (*callback)(FKey*))
{
  if (_key_fkeys_count >= MAX_FKEYS) return false;
  if (!callback) return false;
  for (int i = 0; i < _key_fkeys_count; i++)
  {
    if (_key_fkeys[i].key == key && _key_fkeys[i].mods == mods) return false;
  }
  _key_fkeys[_key_fkeys_count].key = key;
  _key_fkeys[_key_fkeys_count].mods = mods;
  _key_fkeys[_key_fkeys_count].callback = callback;
  _key_fkeys_count++;
  return true;
}

bool key_unregister_fkey (SDLKey key, SDLMod mods)
{
  for (int i = 0; i < _key_fkeys_count; i++)
  {
    if (_key_fkeys[i].key == key && _key_fkeys[i].mods == mods)
    {
      _key_fkeys_count--;
      memmove(_key_fkeys+i, _key_fkeys+i+1, sizeof(_key_fkeys[0]) * _key_fkeys_count);
      return true;
    }
  }
  return false;
}

bool _key_maybe_invoke_fkey (SDLKey key, SDLMod mods)
{
  if (key < SDLK_F1 || key > SDLK_F15) return false;
  for (int i = 0; i < _key_fkeys_count; i++)
  {
    if (_key_fkeys[i].key == key && _key_fkeys[i].mods == mods)
    {
      _key_fkeys[i].callback(_key_fkeys+i);
      return true;
    }
  }
  return false;
}

void _key_init ()
{
  for (int i = 0; i < MAX_KEYS_DOWN; i++)
  {
    _pressed_keys[i].sym = SDL_KEY_INVALID;
  }
}

// Try to press a key.  Returns true if it was accepted.
bool _key_press (SDL_keysym * k)
{
  if (_pressed_keys_count >= MAX_KEYS_DOWN) return false;
  for (int i = 0; i < MAX_KEYS_DOWN; i++)
  {
    if (_pressed_keys[i].sym == SDL_KEY_INVALID)
    {
      _pressed_keys[i] = *k;
      _pressed_keys_count++;
      return true;
    }
  }
  return false;
}

// Try to release a key.  Returns true if it was released.
bool _key_release (SDL_keysym * k)
{
  if (_pressed_keys_count <= 0) return false;
  for (int i = 0; i < MAX_KEYS_DOWN; i++)
  {
    if (_pressed_keys[i].sym == k->sym)
    {
      _pressed_keys[i].sym = SDL_KEY_INVALID;
      _pressed_keys_count--;
      return true;
    }
  }
  return false;
}


// -- Appearance -------------------------------------------------------------

Theme theme;

Theme lux_get_theme ()
{
  return theme;
}

static uint32_t _bg_color;
static Uint32 _bg_color_sdl; // In SDL form for FillRect


static uint32_t _outline_color = 0x000000;
static uint32_t _edgetl_color = 0x9b9ccc;
static uint32_t _edgebr_color = 0x3b3b5c;
static uint32_t _face_color = 0x8181b5;
static uint32_t _title_color = 0xeeeef7;
static uint32_t _title_shd_color = 0x3b3b5c;

static uint32_t _de_outline_color = 0x000000;
static uint32_t _de_edgetl_color = 0xa8a8a8;
static uint32_t _de_edgebr_color = 0x3d3d3d;
static uint32_t _de_face_color = 0x808080;
static uint32_t _de_title_color = 0xeeeeee;
static uint32_t _de_title_shd_color = 0x3d3d3d;


// -- Geometry ---------------------------------------------------------------

bool rect_overlaps (SDL_Rect * r1, SDL_Rect * r2)
{

  if (rect_empty(r1) || rect_empty(r2)) return false;

  int r1x2 = r1->x+r1->w;
  int r1y2 = r1->y+r1->h;
  int r2x2 = r2->x+r2->w;
  int r2y2 = r2->y+r2->h;

  return (r1->x < r2x2 && r1x2 > r2->x
       && r1->y < r2y2 && r1y2 > r2->y);
}

// Does r2 completely obscure r1?
bool rect_covered_by (SDL_Rect * r1, SDL_Rect * r2)
{
  if (r2->x > r1->x) return false;
  if (r2->y > r1->y) return false;
  if (r2->x+r2->w < r1->x+r1->w) return false;
  if (r2->y+r2->h < r1->y+r1->h) return false;
  return true;
}


#ifndef MIN
#define MIN(V1,V2) (((V1) < (V2)) ? (V1) : (V2))
#endif
#ifndef MAX
#define MAX(V1,V2) (((V1) > (V2)) ? (V1) : (V2))
#endif

static bool rect_clip_inside (SDL_Rect * r, SDL_Rect * clip)
{
  SDL_Rect old = *r;

  r->w = MIN(r->x + r->w, clip->x + clip->w) - r->x;
  r->h = MIN(r->y + r->h, clip->y + clip->h) - r->y;

  if (clip->x > r->x) { r->w -= (clip->x - r->x); r->x = clip->x; }
  if (clip->y > r->y) { r->h -= (clip->y - r->y); r->y = clip->y; }

  if (r->w < 0) r->w = 0;
  if (r->h < 0) r->h = 0;

  return rect_equal(&old, r);
}


// -- Window ----------------------------------------------------------------

#define RESIZE_T 1
#define RESIZE_R 2
#define RESIZE_B 4
#define RESIZE_L 8
#define RESIZE_NONE 0

bool _window_dirty = true;

//static const int _window_default_top = 20;
#define _window_default_top 20
static int _window_top_x = 20;
static int _next_w_x = -1; // _window_top_x;
static int _next_w_y = _window_default_top;

static Window * _top_window = NULL;
static Window * _mouse_window = NULL;

static Window * _switcher_below = NULL;
// When the window switcher is active, this is the window beneath where the
// currently switch-preview window should be restored if it's not switched
// to.  A special case is when _switcher_below == _top_window.  In that
// case, the switcher is active and the current preview window is the same
// window as when the switcher was activated.

static Window * captured = NULL;


Window * window_dirty (Window * w)
{
  w->dirty = true;
  _window_dirty = true;
  return w;
}

Window * window_dirty_chrome (Window * w)
{
  window_dirty(w);
  w->dirty_chrome = true;
  return w;
}

void _window_add_uncover_rect (SDL_Rect * r);

void window_close (Window * w)
{
  if (w == _mouse_window) _mouse_window = NULL;
  if (w == _switcher_below) _switcher_below = NULL;
  if (w == captured) captured = NULL; // Do we need to do more here?

  _top_window = w->below;
  if (_top_window == w)
  {
    _top_window = NULL;
  }
  else
  {
    w->below->above = w->above;
    w->above->below = w->below;
    window_dirty_chrome(_top_window);
  }

  SDL_Rect r;
  window_get_rect(w, &r);
  _window_add_uncover_rect(&r);
  void * pix = w->surf->pixels;
  SDL_FreeSurface(w->surf);
  if (pix)
  {
    if (w->pix_free) w->pix_free(w, pix);
    else free(pix);
  }
  free(w);

}

static void draw_window_chrome (SDL_Surface * surf, SDL_Rect * rrr, const char * caption, bool active, int button_state, int flags);

Window * window_create (int w, int h, const char * caption, int flags, void* (*pix_alloc)(size_t))
{
  w += OUTLINE*2 + EDGE_SIZE*2;
  h += OUTLINE*2 + EDGE_SIZE*2 + EMBOSS_SIZE*2 + TITLEBAR_H;
  Window * wnd = (Window *)calloc(sizeof(Window), 1);
  window_dirty(wnd);
  wnd->dirty_chrome = true;
  wnd->fixed = true;
  wnd->flags = flags;
  wnd->on_close = window_close;
  if (!pix_alloc) pix_alloc = malloc;
  wnd->pix_alloc = pix_alloc;
  void * pixels = pix_alloc(4 * w * h);
  memset(pixels, 0x7f, 4 * w * h);
  wnd->surf = SDL_CreateRGBSurfaceFrom(pixels, w, h, 32, w*4, bmask, gmask, rmask, 0);
  if (_next_w_x == -1) _next_w_x = _window_top_x;
  wnd->x = _next_w_x;
  wnd->y = _next_w_y;
  if (h + _next_w_y + 20 > screen->h && h + _window_default_top < screen->h)
  {
    _window_top_x += 50;
    if (_window_top_x + 30 > screen->w) _window_top_x = 20;
    _next_w_x = _window_top_x;
    wnd->x = _next_w_x;
    _next_w_y = _window_default_top;
    wnd->y = _next_w_y;
  }
  else
  {
    _next_w_y += 20;
    _next_w_x += 20;
    if (_next_w_x + 30 > screen->w) _next_w_x = 20;
  }

  if (caption) wnd->title = strdup(caption);
  else wnd->title = strdup("Window");

  if (!_top_window)
  {
    _top_window = wnd;
    wnd->above = wnd;
    wnd->below = wnd;
  }
  else
  {
    window_dirty_chrome(_top_window);
    wnd->below = _top_window;
    wnd->above = _top_window->above;

    _top_window->above->below = wnd;
    _top_window->above = wnd;

    _top_window = wnd;
  }
  return wnd;
}

bool window_is_top (Window * w)
{
  return _top_window == w;
}

bool window_is_bottom (Window * w)
{
  return _top_window->above == w;
}

Window * window_below (Window * w)
{
  Window * b = w->below;
  if (b == _top_window) return NULL;
  return b;
}

Window * window_above (Window * w)
{
  if (w == _top_window) return NULL;
  return w->above;
}

Window * window_raise (Window * w)
{
  if (w == _top_window) return w;
  if (!w) return w;

  window_dirty_chrome(_top_window); // Is there any way to avoid this?

  // Lower old window
  if (_top_window->mouse_button_state && _top_window->on_mouseup)
  {
    for (int i = 1; i <= 5; i++)
    {
      if (_top_window->mouse_button_state & SDL_BUTTON(i))
      {
        // synthesize button release
        w->on_mouseup(w, w->last_mouse_x, w->last_mouse_y, i, SDL_MOUSEBUTTONUP, false);
      }
    }
    _top_window->mouse_button_state = 0;
  }
  if (_top_window->on_keyup && _pressed_keys_count)
  {
    for (int i = 0; i < MAX_KEYS_DOWN; i++)
    {
      if (_pressed_keys[i].sym != SDL_KEY_INVALID)
      {
        // synthesize key up
        if (_key_release(_pressed_keys+i))
        {
          _top_window->on_keyup(_top_window, &_pressed_keys[i], false);
        }
      }
    }
  }

  if (_top_window->on_lower) _top_window->on_lower(_top_window);

  w->below->above = w->above;
  w->above->below = w->below;

  w->below = _top_window;
  w->above = _top_window->above;

  _top_window->above->below = w;
  _top_window->above = w;

  _top_window = w;
  if (_top_window->on_raise) _top_window->on_raise(_top_window);

  window_dirty(w);
  window_dirty_chrome(w);

  return w;
}

Window * window_set_title (Window * w, const char * title)
{
  if (!title) return w;
  if (w->title) free(w->title);
  w->title = strdup(title);
  window_dirty_chrome(w);
  return w;
}

void window_get_rect (Window * w, SDL_Rect * r)
{
  r->x = w->x;
  r->y = w->y;
  r->w = w->surf->w;
  r->h = w->surf->h;
}



static int _window_uncover_rect_count = 0;
//static const int _window_uncover_rect_max = 4;
#define _window_uncover_rect_max 4
static SDL_Rect _window_uncover_rects[_window_uncover_rect_max];

void _window_add_uncover_rect (SDL_Rect * r)
{
  if (rect_empty(r)) return;
  if (_window_uncover_rect_count >= _window_uncover_rect_max)
  {
    // Merge new rect into rect 0 (could do better)
    SDL_Rect * cr = _window_uncover_rects;
    if (r->x < cr->x) cr->x = r->x;
    if (r->y < cr->y) cr->y = r->y;
    if (r->x+r->w > cr->x+cr->w) cr->w = r->x+r->w-cr->x;
    if (r->y+r->h > cr->y+cr->h) cr->h = r->y+r->h-cr->y;
  }
  else
  {
    _window_uncover_rects[_window_uncover_rect_count] = *r;
    _window_uncover_rect_count++;
  }

  _window_dirty = true;
}

static void _window_mark_dirty_intersecting (SDL_Rect * r)
{
  Window * w = _top_window;
  while (w)
  {
    SDL_Rect wr;
    window_get_rect(w, &wr);
    if (rect_overlaps(r, &wr))
    {
      window_dirty(w);
      if (rect_covered_by(r, &wr)) break;
    }

    w = window_below(w);
  }
}

static bool _window_do_uncovers ()
{
  //int r = rand() & 0xff;
  //int g = rand() & 0xff;
  //int b = rand() & 0xff;

  if (_window_uncover_rect_count == 0) return false;
  for (int i = 0; i < _window_uncover_rect_count; i++)
  {
    //Uint32 col = r << 16 | g << 8 | b << 0;
    //r -= 10; g -= 10; b -= 10;
    SDL_Rect * cr = _window_uncover_rects+i;
    //printf("UNCOVERING...%i %i %i %i\n", cr->x,cr->y,cr->w,cr->h);
    SDL_FillRect(screen, cr, _bg_color_sdl);
    _window_mark_dirty_intersecting(cr);
  }
  _window_uncover_rect_count = 0;
  return true;
}

// Returns client rect in window coords
void window_get_client_rect (Window * w, SDL_Rect * r)
{
  r->x = EDGE_SIZE+OUTLINE;
  r->y = EDGE_SIZE+OUTLINE + 2*EMBOSS_SIZE + TITLEBAR_H;
  r->w = w->surf->w - 2*r->x;
  r->h = w->surf->h - r->y - EDGE_SIZE - OUTLINE;
}

// In window coords
#define WINDOW_BUTTON_NONE 0x7f
#define WINDOW_BUTTON_BIT(B) ((B) >= 0 ? (B) : (1) << (8+(B)))
void _window_get_button_rect (Window * w, SDL_Rect * r, int button)
{
  r->h = 2*EMBOSS_SIZE + TITLEBAR_H;
  r->w = r->h;
  r->y = OUTLINE + EDGE_SIZE;
  if (button >= 0)
    r->x = button * r->w + OUTLINE+EDGE_SIZE;
  else
    r->x = w->surf->w + button * r->w - OUTLINE-EDGE_SIZE;
}

Window * window_clear_client (Window * w, uint32_t color)
{
  SDL_Color z;
  sdlcolor(color, &z);
  Uint32 c = SDL_MapRGB(w->surf->format, z.r, z.g, z.b);
  SDL_Rect r;
  window_get_client_rect(w, &r);
  SDL_FillRect(w->surf, &r, c);
  window_dirty(w);
  return w;
}

bool _window_resize (Window * w, int resize, int x, int y)
{
  bool growx = false;
  bool growy = false;
  bool shrinkx = false;
  bool shrinky = false;
  SDL_Rect old_rect;
  window_get_rect(w, &old_rect);
  SDL_Rect old_crect;
  window_get_client_rect(w, &old_crect);
  int ww = w->surf->w;
  int xx = w->x;
  int hh = w->surf->h;
  int yy = w->y;
  if (resize & (RESIZE_L|RESIZE_R))
  {
    ww = w->surf->w + x * ((resize & RESIZE_L) ? -1 : 1);
    if (ww > WIN_MAX_W) ww = WIN_MAX_W;
    else if (ww < WIN_MIN_W) ww = WIN_MIN_W;
    if (resize & RESIZE_L) xx += (w->surf->w - ww);

    if (ww > w->surf->w) growx = true;
    else if (ww < w->surf->w) shrinkx = true;
  }
  if (resize & (RESIZE_T|RESIZE_B))
  {
    hh = w->surf->h + y * ((resize & RESIZE_T) ? -1 : 1);
    if (hh > WIN_MAX_H) hh = WIN_MAX_H;
    else if (hh < WIN_MIN_H) hh = WIN_MIN_H;
    if (resize & RESIZE_T) yy += (w->surf->h - hh);

    if (hh > w->surf->h) growy = true;
    else if (hh < w->surf->h) shrinky = true;
  }

  if (!growx && !growy && !shrinkx && !shrinky) return true; // or false?

  void * new_pixels = w->pix_alloc(4*ww*hh);
  if (!new_pixels) return false;

  SDL_Surface * new_surf = SDL_CreateRGBSurfaceFrom(new_pixels, ww, hh, 32, ww*4, bmask, gmask, rmask, 0);
  if (!new_surf)
  {
    if (w->pix_free) w->pix_free(w, new_pixels);
    else free(new_pixels);
    return false;
  }

  SDL_Surface * old_surf = w->surf;

  w->surf = new_surf;
  w->x = xx;
  w->y = yy;

  bool retain = true;
  if (w->on_resize) retain = !w->on_resize(w, old_surf);

  if (retain)
  {
    SDL_BlitSurface(old_surf, NULL, new_surf, NULL);
    Uint32 bgcolor = mapcolor(w->surf, w->bg_color);

    if (growx)
    {
      SDL_Rect r;
      r.x = old_crect.w + old_crect.x;
      r.y = 0;
      r.w = w->surf->w - r.x;
      r.h = w->surf->h;
      SDL_FillRect(w->surf, &r, bgcolor);
    }
    if (growy)
    {
      SDL_Rect r;
      r.x = 0;
      r.y = old_crect.h + old_crect.y;
      r.w = w->surf->w;
      r.h = w->surf->h - old_crect.h;
      SDL_FillRect(w->surf, &r, bgcolor);
    }
  }

  if (w->pix_free && old_surf->pixels) w->pix_free(w, old_surf->pixels);
  else free(old_surf->pixels);
  SDL_FreeSurface(old_surf);

  window_dirty(w);
  window_dirty_chrome(w);

  // This next code is similar to the window move code.
  // We should probaly try to refactor them...

  if (shrinkx || shrinky)
  {
    SDL_Rect xslice;
    SDL_Rect yslice;
    bool do_x = false;
    bool do_y = false;

    // This is a little sloppy in that the corners may be draw twice (once
    // when doing the horizontal and again when doing the vertical).

    if (shrinkx && (resize & RESIZE_L))
    {
      // Window moved to right, there's a blank to its left
      do_x = true;
      xslice = old_rect;
      xslice.w = MIN(w->x - old_rect.x, xslice.w);
      _window_add_uncover_rect(&xslice);
    }
    else if (shrinkx && (resize & RESIZE_R))
    {
      // Window moved to left, there's a blank to its right
      do_x = true;
      xslice = old_rect;
      xslice.x = MAX(w->x + w->surf->w, xslice.x);
      xslice.w = xslice.w - (xslice.x - old_rect.x);
      _window_add_uncover_rect(&xslice);
    }
    if (shrinky && (resize & RESIZE_T))
    {
      do_y = true;
      yslice = old_rect;
      yslice.h = MIN(w->y - old_rect.y, yslice.h);
      _window_add_uncover_rect(&yslice);
    }
    else if (shrinky && (resize & RESIZE_B))
    {
      do_y = true;
      yslice = old_rect;
      yslice.y = MAX(w->y + w->surf->h, yslice.y);
      yslice.h = yslice.h - (yslice.y - old_rect.y);
      _window_add_uncover_rect(&yslice);
    }

    // We might consider doing intersections only on the slices.
    // That's more intersection tests but maybe less redraw.
    // Or maybe only do it if the window is big?

    while (w)
    {
      SDL_Rect wr;
      window_get_rect(w, &wr);
      if (rect_overlaps(&old_rect, &wr)) w->dirty = true;

      w = window_below(w);
    }
  }

  return true;
}

Window * window_move (Window * w, int x, int y)
{
  bool dirty = false;
  SDL_Rect old_rect;
  window_get_rect(w, &old_rect);
  if (x != NOMOVE)
  {
    if (x > screen->w - (OUTLINE+EDGE_SIZE)) x = screen->w - (OUTLINE+EDGE_SIZE);
    else if (x < -(w->surf->w - (OUTLINE+EDGE_SIZE))) x = -(w->surf->w - (OUTLINE+EDGE_SIZE));
    if (w->x != x) dirty = true;
    w->x = x;
  }
  if (y != NOMOVE)
  {
    if (y > screen->h - (OUTLINE+EDGE_SIZE)) y = screen->h - (OUTLINE+EDGE_SIZE);
    else if (y < -(w->surf->h - (OUTLINE+EDGE_SIZE))) y = -(w->surf->h - (OUTLINE+EDGE_SIZE));
    if (w->y != y) dirty = true;
    w->y = y;
  }

  if (dirty)
  {
    SDL_Rect xslice;
    SDL_Rect yslice;
    bool do_x = false;
    bool do_y = false;

    // This is a little sloppy in that the corners may be draw twice (once
    // when doing the horizontal and again when doing the vertical).

    if (w->x > old_rect.x)
    {
      // Window moved to right, there's a blank to its left
      do_x = true;
      xslice = old_rect;
      xslice.w = MIN(w->x - old_rect.x, xslice.w);
      _window_add_uncover_rect(&xslice);
    }
    else if (w->x < old_rect.x)
    {
      // Window moved to left, there's a blank to its right
      do_x = true;
      xslice = old_rect;
      xslice.x = MAX(w->x + w->surf->w, xslice.x);
      xslice.w = xslice.w - (xslice.x - old_rect.x);
      _window_add_uncover_rect(&xslice);
    }
    if (w->y > old_rect.y)
    {
      do_y = true;
      yslice = old_rect;
      yslice.h = MIN(w->y - old_rect.y, yslice.h);
      _window_add_uncover_rect(&yslice);
    }
    else if (w->y < old_rect.y)
    {
      do_y = true;
      yslice = old_rect;
      yslice.y = MAX(w->y + w->surf->h, yslice.y);
      yslice.h = yslice.h - (yslice.y - old_rect.y);
      _window_add_uncover_rect(&yslice);
    }

    window_dirty(w);

    // We might consider doing intersections only on the slices.
    // That's more intersection tests but maybe less redraw.
    // Or maybe only do it if the window is big?

    while (w)
    {
      SDL_Rect wr;
      window_get_rect(w, &wr);
      if (rect_overlaps(&old_rect, &wr)) w->dirty = true;

      w = window_below(w);
    }
  }

  return w;
}

Window * window_under (int x, int y)
{
  Window * w = _top_window;
  Point p = {x,y};
  SDL_Rect r;
  while (w)
  {
    window_get_rect(w, &r);
    if (point_in_rect(&p, &r)) return w;
    w = window_below(w);
  }
  return NULL;
}

// Screen to local
void window_pt_screen_to_window (Window * w, Point * pt)
{
  pt->x -= w->x;
  pt->y -= w->y;
}

void window_pt_window_to_screen (Window * w, Point * pt)
{
  pt->x += w->x;
  pt->y += w->y;
}

void window_rect_screen_to_window (Window * w, SDL_Rect * pt)
{
  pt->x -= w->x;
  pt->y -= w->y;
}

void window_rect_window_to_screen (Window * w, SDL_Rect * pt)
{
  pt->x += w->x;
  pt->y += w->y;
}

void window_pt_client_to_screen (Window * w, Point * pt)
{
  pt->x -= (w->x+EDGE_SIZE+OUTLINE);
  pt->y -= (w->y+EDGE_SIZE+OUTLINE+2*EMBOSS_SIZE+TITLEBAR_H);
}

void window_pt_screen_to_client (Window * w, Point * pt)
{
  pt->x -= (w->x+EDGE_SIZE+OUTLINE);
  pt->y -= (w->y+EDGE_SIZE+OUTLINE+2*EMBOSS_SIZE+TITLEBAR_H);
}

void window_rect_client_to_screen (Window * w, SDL_Rect * pt)
{
  pt->x -= (w->x+EDGE_SIZE+OUTLINE);
  pt->y -= (w->y+EDGE_SIZE+OUTLINE+2*EMBOSS_SIZE+TITLEBAR_H);
}

void window_rect_screen_to_client (Window * w, SDL_Rect * pt)
{
  pt->x -= (w->x+EDGE_SIZE+OUTLINE);
  pt->y -= (w->y+EDGE_SIZE+OUTLINE+2*EMBOSS_SIZE+TITLEBAR_H);
}


bool window_contains_pt (Window * w, Point * pt)
{
  SDL_Rect r;
  window_get_rect(w, &r);
  return point_in_rect(pt, &r);
}

void _window_set_dirty_at_below (Window * w)
{
  window_dirty(w);
  while (w)
  {
    w->dirty = true;
    w = window_below(w);
  }
}

void _window_draw_mark_intersecting_above (Window * w)
{
  w = window_above(w);
  if (!w) return;
  SDL_Rect wr;
  window_get_rect(w, &wr);
  while (w)
  {
    if (!w->will_draw)
    {
      SDL_Rect r;
      window_get_rect(w, &r);
      if (rect_overlaps(&wr, &r))
      {
        w->will_draw = true;
        _window_draw_mark_intersecting_above(w);
      }
    }
    w = window_above(w);
  }
}

bool _window_draw_all (bool force)
{
  bool any = _window_do_uncovers();

  if (!_top_window) return any;
  if (!force && !_window_dirty) return any;

  // This is not very efficient.  I think a decent way to improve it would be
  // caching overlap somehow so we don't need to iterate all windows to see
  // if they're overlapping.  An easy way would be to have windows all be
  // part of a "group" of windows that are touching each other.  This would
  // prevent needing to search *all* windows, and let you search only ones
  // which might possibly be.  Not sure if that's sufficient, though.
  Window * w = _top_window->above; // Bottom window
  while (w)
  {
    if (!w->will_draw)
    {
      if (force || w->dirty || w->dirty_chrome)
      {
        w->will_draw = true;
        w->dirty = false;
        _window_draw_mark_intersecting_above(w);
      }
    }
    w = window_above(w);
  }

  w = _top_window->above; // Bottom window
  while (w)
  {
    SDL_Rect r;
    window_get_rect(w, &r);
    if (w->will_draw)
    {
      if (w->dirty_chrome)
      {
        draw_window_chrome(w->surf, NULL, w->title, w == _top_window, w->button_state, w->flags);
        w->dirty_chrome = false;
      }
      w->will_draw = false;
      if (w->on_draw && w->on_draw(w, screen, r))
      {
        // Pass
      }
      else if (w->surf->pixels)
      {
        SDL_BlitSurface(w->surf, NULL, screen, &r);
      }
      any = true;
    }
    w = window_above(w);
  }

  _window_dirty = false;
  return any;
}

// -- Luxer ------------------------------------------------------------------

int lux_sysfont_h ()
{
  return FONT_H;
}

int lux_sysfont_w ()
{
  return FONT_W;
}

void lux_set_bg_color (uint32_t color)
{
  _bg_color = color;
  if (!screen) return;
  SDL_Color bg;
  sdlcolor(color, &bg);
  _bg_color_sdl = SDL_MapRGB(screen->format, bg.r, bg.g, bg.b);
}

static void _draw_x_part (SDL_Surface * surf, int x, int y, int dx, int dy, int size, Uint32 color)
{
  Uint32 * pix = (Uint32*)surf->pixels;
  int xx = x, yy = y;
  for (int i = 0; i < size; i++)
  {
    Uint32 * p = ((Uint32*)surf->pixels) + yy * surf->pitch/4 + xx;

    *p = color;

    xx += dx;
    yy += dy;
  }
}

static void draw_x (SDL_Surface * surf, int x, int y, int inner_size, int size, Uint32 color1, Uint32 color2)
{
  size /= 2;
  if (inner_size <= 0) inner_size = size / 2;

  SDL_LockSurface(surf);
  Uint32 * pix = (Uint32*)surf->pixels;

  const int i = inner_size;
  const int s = size-inner_size;

  // Top
  _draw_x_part(surf, x-0, y-i, -1, -1, size-inner_size, color2);
  _draw_x_part(surf, x-0, y-i,  1, -1, size-inner_size, color1);

  // Bottom
  _draw_x_part(surf, x-0, y+i, -1,  1, size-inner_size, color2);
  _draw_x_part(surf, x-0, y+i,  1,  1, size-inner_size, color2);

  // Left
  _draw_x_part(surf, x-i, y-0, -1, -1, size-inner_size, color1);
  _draw_x_part(surf, x-i, y-0, -1,  1, size-inner_size, color1);

  // Right
  _draw_x_part(surf, x+i, y-0,  1, -1, size-inner_size, color2);
  _draw_x_part(surf, x+i, y-0,  1,  1, size-inner_size, color2);


  // TL
  _draw_x_part(surf, x-s-1, y-i-s+1, -1,  1, inner_size-0, color1);

  // TR
  _draw_x_part(surf, x+s+0, y-i-s+1,  1,  1, inner_size-1, color1);

  // BL
  _draw_x_part(surf, x-i-s, y+s,  1,  1, inner_size, color1);

  // BR
  _draw_x_part(surf, x+s, y+i+s-1,  1,  -1, inner_size, color2);

  SDL_UnlockSurface(surf);
}

static void draw_corner_down (SDL_Surface * surf, int x, int y, int size, Uint32 color1, Uint32 color2)
{
  if (size == 0) return;
  // surf should be locked
  Uint32 * pix = (Uint32*)surf->pixels;
  pix += surf->pitch/4 * y + x;

  for (int y = 0; y < size; y++)
  {
    for (int x = 0; x <= y; x++)
    {
      pix[x] = color1;
    }
    for (int x = y + 1; x < size; x++)
    {
      pix[x] = color2;
    }
    pix += surf->pitch/4;
  }
}

static void draw_corner_up (SDL_Surface * surf, int x, int y, int size, Uint32 color1, Uint32 color2)
{
  if (size == 0) return;
  // surf should be locked
  Uint32 * pix = (Uint32*)surf->pixels;
  pix += surf->pitch/4 * y + x;

  for (int y = 0; y < size; y++)
  {
    for (int x = 0; x < size-y; x++)
    {
      pix[x] = color1;
    }
    for (int x = size-y; x < size; x++)
    {
      pix[x] = color2;
    }
    pix += surf->pitch/4;
  }
}

static void draw_rect (SDL_Surface * surf, SDL_Rect * rrr, int size, Uint32 color)
{
  SDL_Rect * rr;
  SDL_Rect rrrr;
  if (rrr)
  {
    rr = rrr;
  }
  else
  {
    rr = &rrrr;
    rrrr.x = 0;
    rrrr.y = 0;
    rrrr.w = surf->w;
    rrrr.y = surf->h;
  }

  SDL_Rect r;
  r.x = rr->x;
  r.y = rr->y;
  r.w = rr->w;
  r.h = size;
  SDL_FillRect(surf, &r, color);
  r.y = rr->y + rr->h - size;
  SDL_FillRect(surf, &r, color);
  r.y = rr->y + size;
  r.w = size;
  r.h = rr->h - 2*size;
  SDL_FillRect(surf, &r, color);
  r.x = rr->x + rr->w - size;
  SDL_FillRect(surf, &r, color);
}

static void draw_frame (SDL_Surface * surf, SDL_Rect * rrr, bool pushed, WinColors * wc)
{
  if (!wc) wc = &theme.win;
  SDL_LockSurface(surf);

  SDL_Rect * rr;
  SDL_Rect rrrr;
  if (rrr)
  {
    rr = rrr;
  }
  else
  {
    rr = &rrrr;
    rrrr.x = 0;
    rrrr.y = 0;
    rrrr.w = surf->w;
    rrrr.y = surf->h;
  }

  Uint32 tl = pushed ? wc->edge_br : wc->edge_tl;
  Uint32 br = pushed ? wc->edge_tl : wc->edge_br;

  draw_corner_up(surf, rr->x+rr->w-EMBOSS_SIZE, rr->y, EMBOSS_SIZE, tl, br);
  draw_corner_up(surf, rr->x, rr->y+rr->h-EMBOSS_SIZE, EMBOSS_SIZE, tl, br);

  SDL_Rect r;
  r.x = rr->x;
  r.y = rr->y;
  r.w = rr->w - EMBOSS_SIZE;
  r.h = EMBOSS_SIZE;
  SDL_FillRect(surf, &r, tl);
  r.x += EMBOSS_SIZE;
  r.y = rr->y + rr->h - EMBOSS_SIZE;
  SDL_FillRect(surf, &r, br);
  r.x = rr->x;
  r.y = rr->y + EMBOSS_SIZE;
  r.w = EMBOSS_SIZE;
  r.h = rr->h - 2*EMBOSS_SIZE;
  SDL_FillRect(surf, &r, tl);
  r.x = rr->x + rr->w - EMBOSS_SIZE;
  SDL_FillRect(surf, &r, br);

  SDL_UnlockSurface(surf);
}

void text_draw (SDL_Surface * surf, const char * s, int x, int y, uint32_t color)
{
  static uint32_t prev_color = 0xff00ff;
  static SDL_Surface * prev_surf = 0;

  if (surf != prev_surf || color != prev_color)
  {
    prev_surf = surf;
    prev_color = color;

    int bg = 0xff00ff;
    if (bg == color) bg = 0;
    SDL_Color pal[2];
    sdlcolor(bg, pal+0);
    sdlcolor(color, pal+1);

    SDL_SetColors(font, pal, 0, 2);
    SDL_SetColorKey(font, SDL_SRCCOLORKEY, SDL_MapRGB(font->format, pal[0].r, pal[0].g, pal[0].b));
  }

  SDL_Rect srcrect = {0,0,FONT_W,FONT_H};
  SDL_Rect dstrect = {x,y,FONT_W,FONT_H};

  const char * c = s;
  while (*c)
  {
    if (*c == '\n')
    {
      dstrect.x = x;
      dstrect.y += FONT_H;
    }
    else if (*c == '\t')
    {
      dstrect.x = 4 * FONT_W;
    }
    else if (*c >= ' ')
    {
      srcrect.y = FONT_H * (*c);

      SDL_BlitSurface(font, &srcrect, surf, &dstrect);

      dstrect.x += FONT_W;
    }

    c++;
  }
}

static void draw_h_notch (SDL_Surface * surf, int x, int y, Uint32 color1, Uint32 color2)
{
  SDL_Rect notch;
  notch.x = x+EMBOSS_SIZE;
  notch.w = EDGE_SIZE-2*EMBOSS_SIZE;
  notch.h = EMBOSS_SIZE;
  notch.y = y;

  draw_corner_up(surf, x, y, EMBOSS_SIZE, color1, color2);

  // We draw this with size-1 because it looks a lot better...
  // .. but I'm not sure it will if we tweak the default sizes.
  // The basic idea is that we don't want the top-right corner to cut
  // all the way to the right edge.
  draw_corner_up(surf, x+EDGE_SIZE-1*EMBOSS_SIZE, y+EMBOSS_SIZE, EMBOSS_SIZE-1, color1, color2);

  SDL_FillRect(surf, &notch, color2);
  notch.y += EMBOSS_SIZE;
  SDL_FillRect(surf, &notch, color1);
}

static void draw_v_notch (SDL_Surface * surf, int x, int y, Uint32 color1, Uint32 color2)
{
  SDL_Rect notch;
  notch.x = x;
  notch.h = EDGE_SIZE-2*EMBOSS_SIZE;
  notch.w = EMBOSS_SIZE;
  notch.y = y+EMBOSS_SIZE;

  draw_corner_up(surf, x, y, EMBOSS_SIZE, color1, color2);

  // We draw this with size-1 because it looks a lot better...
  // .. but I'm not sure it will if we tweak the default sizes.
  // The basic idea is that we don't want the top-right corner to cut
  // all the way to the right edge.
  draw_corner_up(surf, x+EMBOSS_SIZE,y+EDGE_SIZE-1*EMBOSS_SIZE, EMBOSS_SIZE-1, color1, color2);

  SDL_FillRect(surf, &notch, color2);
  notch.x += EMBOSS_SIZE;
  SDL_FillRect(surf, &notch, color1);
}

static void draw_window_chrome (SDL_Surface * surf, SDL_Rect * rrr, const char * caption, bool active, int button_state, int flags)
{
  WinColors * wc = active ? &theme.win : &theme.winde;

  SDL_LockSurface(surf);
  SDL_Rect rrrr;
  SDL_Rect * rr = &rrrr;
  if (rrr)
  {
    rrrr = *rrr;
  }
  else
  {
    rrrr.x = 0;
    rrrr.y = 0;
    rrrr.w = surf->w;
    rrrr.h = surf->h;
  }

  SDL_Rect base_rect;
  if (flags & WIN_F_RESIZE) base_rect = *rr;

  if (OUTLINE)
  {
    draw_rect(surf, rr, OUTLINE, wc->outline);
  }

  rect_shrink(rr, OUTLINE);

  draw_frame(surf, rr, false, wc);
  rect_shrink(rr, EMBOSS_SIZE);
  draw_rect(surf, rr, EDGE_SIZE - 2*EMBOSS_SIZE, wc->face);
  //SDL_Rect titler = *rr;
  rect_shrink(rr, EDGE_SIZE - 2*EMBOSS_SIZE);
  //rr->y += (TITLEBAR_H + 2*EMBOSS_SIZE) - (EDGE_SIZE-2*EMBOSS_SIZE);
  //rr->h -= (TITLEBAR_H + 2*EMBOSS_SIZE) - (EDGE_SIZE-2*EMBOSS_SIZE);
  draw_frame(surf, rr, true, wc);

  rect_shrink(rr, EMBOSS_SIZE);
  rr->h = TITLEBAR_H + 2*EMBOSS_SIZE;
  //  draw_frame(surf, rr, false);
  //  rect_shrink(rr, EMBOSS_SIZE);
  //  SDL_FillRect(surf, rr, wc->face);

  if (flags & WIN_F_RESIZE)
  {
    // Everything else has zero overdraw, but this is sort of a hack. :(
    // Currently, the windows are always double buffered, so it doesn't
    // show, but it still makes me sad.
    const int ndist = TITLEBAR_H+EDGE_SIZE+EMBOSS_SIZE+OUTLINE;
    draw_h_notch(surf, base_rect.x + OUTLINE,                         base_rect.y + ndist, wc->edge_tl, wc->edge_br);
    draw_h_notch(surf, base_rect.x + base_rect.w-OUTLINE-EDGE_SIZE,   base_rect.y + ndist, wc->edge_tl, wc->edge_br);
    draw_h_notch(surf, base_rect.x + OUTLINE,                         base_rect.y + base_rect.h-ndist-2*EMBOSS_SIZE, wc->edge_tl, wc->edge_br);
    draw_h_notch(surf, base_rect.x + base_rect.w-OUTLINE-EDGE_SIZE,   base_rect.y + base_rect.h-ndist-2*EMBOSS_SIZE, wc->edge_tl, wc->edge_br);

    draw_v_notch(surf, base_rect.x + ndist,                           base_rect.y + OUTLINE, wc->edge_tl, wc->edge_br);
    draw_v_notch(surf, base_rect.x + ndist,                           base_rect.y + base_rect.h-OUTLINE-EDGE_SIZE, wc->edge_tl, wc->edge_br);
    draw_v_notch(surf, base_rect.x + base_rect.w-ndist-2*EMBOSS_SIZE, base_rect.y + OUTLINE, wc->edge_tl, wc->edge_br);
    draw_v_notch(surf, base_rect.x + base_rect.w-ndist-2*EMBOSS_SIZE, base_rect.y + base_rect.h-OUTLINE-EDGE_SIZE, wc->edge_tl, wc->edge_br);
  }

  SDL_Rect titler = *rr;

  // Upper-right button
  //TODO: "Press" button in.
  bool pressed = (button_state & WINDOW_BUTTON_BIT(-1));
  rr->x += rr->w - rr->h;
  rr->w = rr->h;
  draw_frame(surf, rr, pressed, wc);
  rect_shrink(rr, EMBOSS_SIZE);
  SDL_FillRect(surf, rr, wc->face);
  draw_x(surf, rr->x+rr->w/2+(pressed?1:0), rr->y+rr->h/2+(pressed?1:0), -1, rr->w-2,
         wc->edge_tl, wc->edge_br);

  titler.w -= titler.h;
  *rr = titler;
  draw_frame(surf, rr, false, wc);
  rect_shrink(rr, EMBOSS_SIZE);
  SDL_FillRect(surf, rr, wc->face);

  SDL_UnlockSurface(surf);

  if (caption)
  {
    SDL_Rect ocr;
    SDL_GetClipRect(surf, &ocr);
    SDL_SetClipRect(surf, &titler);

    int w = strlen(caption) * FONT_W;
    int x = titler.x + titler.w/2 - w/2;
    int y = titler.y + titler.h/2 - FONT_H / 2;

    text_draw(surf, caption, x+1, y+1, wc->title_shd);
    if (TITLE_BOLD) text_draw(surf, caption, x+2, y+1, wc->title_shd);
    text_draw(surf, caption, x, y, wc->title);
    if (TITLE_BOLD) text_draw(surf, caption, x+1, y, wc->title);

    SDL_SetClipRect(surf, &ocr);
  }
}


static void draw_randcolor (Window * w)
{
  if (w->opaque_int == 0)
  {
    w->opaque_int = (rand() & 0xff) << 16 | ((rand() & 0xff) << 8) | ((rand() & 0xff) << 0);
  }
  SDL_Rect r;
  window_get_client_rect(w, &r);
  SDL_FillRect(w->surf, &r, w->opaque_int);
}



static int screen_width = 640, screen_height = 480;

bool lux_init (int w, int h, const char * window_name)
{
  // We should probably break this down into a couple of functions
  // so that we can initialize Lux independently of SDL (for
  // integration into existing SDL apps).
  _key_init();

  screen_width = w;
  screen_height = h;
  screen = SDL_SetVideoMode(screen_width, screen_height, 32, 0);
  lux_set_bg_color(_bg_color);

  //TODO: Use this for generating colors?
  //const SDL_VideoInfo * info = SDL_GetVideoInfo();
  // info->vfmt->Rmask, info->vfmt->Gmask, info->vfmt->Bmask, info->vfmt->Amask

  SDL_FillRect(screen, NULL, _bg_color_sdl);

  theme.winde.edge_tl = mapcolor(screen, _de_edgetl_color);
  theme.winde.edge_br = mapcolor(screen, _de_edgebr_color);
  theme.winde.face = mapcolor(screen, _de_face_color);
  theme.winde.outline = mapcolor(screen, _de_outline_color);
  theme.winde.title = mapcolor(screen, _de_title_color);
  theme.winde.title_shd = mapcolor(screen, _de_title_shd_color);;

  theme.win.edge_tl = mapcolor(screen, _edgetl_color);
  theme.win.edge_br = mapcolor(screen, _edgebr_color);
  theme.win.face = mapcolor(screen, _face_color);
  theme.win.outline = mapcolor(screen, _outline_color);
  theme.win.title = mapcolor(screen, _title_color);
  theme.win.title_shd = mapcolor(screen, _title_shd_color);;

  SDL_SetCursor(create_cursor_xpm(arrow));

  if (!window_name) window_name = "Lux";
  SDL_WM_SetCaption(window_name, NULL);

  font = SDL_CreateRGBSurfaceFrom(font_raw, FONT_W, FONT_H * 256, 8, FONT_W, 0, 0, 0, 0);

  if (!screen || !font) return false;

  SDL_Flip(screen);

  return true;
}

static void _lux_terminate ()
{
  SDL_FreeSurface(font);
  SDL_Quit();
}

static void _unswitch ()
{
  // This undoes the currently-previewed window when the window switcher is
  // active, returning to the original non-switched state.

  Window * prv = _top_window;
  window_raise(window_below(_top_window));

  // Remove prv from window chain
  prv->above->below = prv->below;
  prv->below->above = prv->above;

  // Insert prv above _switcher_below
  prv->above = _switcher_below->above;
  prv->below = _switcher_below;
  prv->above->below = prv;
  _switcher_below->above = prv;
}


// Variables used by event loop
static bool quitting = false;
static Point drag_offset;
static SDL_Rect resize_start;
static bool drag = false;
static int resize = RESIZE_NONE;
static int button_captured = WINDOW_BUTTON_NONE;

void lux_terminate ()
{
  quitting = true;
}

bool lux_draw ()
{
  if (_window_draw_all(false))
  {
    SDL_Flip(screen);
    return true;
  }
  return false;
}

void lux_loop ()
{
  while (!quitting)
  {
    lux_draw();

    SDL_Event event;
    bool got_sdl_event = SDL_WaitEvent(&event);

    if (got_sdl_event)
    {
      if (!lux_do_event(&event)) break;
    }
  }
  lux_terminate();
}

bool lux_do_event (SDL_Event * event)
{
  if (event->type == SDL_QUIT)
  {
    quitting = true;

    // This is tricky because normal window iteration doesn't
    // work if a window closes!  We might want to fix that...
    Window * w = _top_window;
    if (w)
    {
      Window * last = w->above;
      while (true)
      {
        Window * nxt = w->below;
        window_close(w);
        if (w == last) break;
        w = nxt;
      }
    }
    return false;
  }
  else if (event->type == SDL_KEYDOWN)
  {
    bool process = _switcher_below ? false : true;

    if ((event->key.keysym.sym == LUX_SWITCHER_KEY) && (event->key.keysym.mod & (LUX_SWITCHER_MODS)) && _top_window && !window_is_bottom(_top_window))
    {
      process = false;
      bool upwards = event->key.keysym.mod & (KMOD_LSHIFT|KMOD_RSHIFT);
      if (!_switcher_below || _switcher_below == _top_window)
      {
        Window * nxt = upwards ? _top_window->above : window_below(_top_window);
        _switcher_below = nxt->below;
        window_raise(nxt);
      }
      else
      {
        _unswitch();

        Window * nxt = (!upwards) ? _switcher_below : _switcher_below->above->above;
        if (nxt == _top_window)
        {
          _switcher_below = nxt;
        }
        else
        {
          _switcher_below = nxt->below;
          window_raise(nxt);
        }
      }
    }
    else if (_switcher_below && (event->key.keysym.sym < SDLK_NUMLOCK || event->key.keysym.sym > SDLK_COMPOSE))
    {
      process = false;
      if (_top_window && _switcher_below != _top_window)
      {
        // Hit a non-modifier while doing window switching -- cancel switching
        _unswitch();
      }
      _switcher_below = NULL;
    }

    if (event->key.keysym.sym >= SDLK_F1 && event->key.keysym.sym <= SDLK_F15)
    {
      process = process && !_key_maybe_invoke_fkey(event->key.keysym.sym, event->key.keysym.mod);
    }

    if (process && _top_window && _top_window->on_keydown)
    {
      if (_key_press(&event->key.keysym))
      {
        _top_window->on_keydown(_top_window, &event->key.keysym, true);
      }
    }
  }
  else if (event->type == SDL_KEYUP)
  {
    if (_switcher_below)
    {
      if (! (event->key.keysym.mod & (LUX_SWITCHER_MODS)) )
      {
        _switcher_below = NULL;
      }
    }
    else
    {
      if (_top_window && _top_window->on_keyup)
      {
        if (_key_release(&event->key.keysym))
        {
          _top_window->on_keyup(_top_window, &event->key.keysym, false);
        }
      }
    }
  }
  else if (event->type == SDL_ACTIVEEVENT)
  {
    // Do we do anything here?
  }
  else if (event->type == SDL_MOUSEMOTION)
  {
    int x = event->motion.x;
    int y = event->motion.y;

    if (drag && _top_window)
    {
      int dx = event->motion.x - drag_offset.x;
      int dy = event->motion.y - drag_offset.y;
      if (resize != RESIZE_NONE)
      {
        int ow = _top_window->surf->w;
        int oh = _top_window->surf->h;
        _window_resize(_top_window, resize, dx, dy);
        ow -= _top_window->surf->w;
        oh -= _top_window->surf->h;
        if (resize & RESIZE_L) ow *= -1;
        if (resize & RESIZE_T) oh *= -1;
        drag_offset.x -= ow;
        drag_offset.y -= oh;
      }
      else
      {
        window_move(_top_window, dx, dy);
      }
    }
    else
    {
      Window * w = captured;
      Point pt = {event->motion.x, event->motion.y};
      if (w)
      {
        window_pt_screen_to_client(w, &pt);
      }
      else
      {
        w = window_under(x, y);
        if (w)
        {
          if (_mouse_window && w != _mouse_window)
          {
            if (_mouse_window->on_mouseout) _mouse_window->on_mouseout(_mouse_window, false);
          }
          if (w->on_mouseover) w->on_mouseover(w, true);
          _mouse_window = w;

          window_pt_screen_to_client(w, &pt);
          SDL_Rect r;
          window_get_client_rect(w, &r);
          if (pt.x < 0 || pt.y < 0 || pt.x >= r.w || pt.y >= r.h)
          {
            w = NULL;
          }
        }
      }

      //printf("%p %s %i %i\n", w, captured ? "cap" : "nor", pt.x, pt.y);
      if (w)
      {
        w->last_mouse_x = pt.x;
        w->last_mouse_y = pt.y;
        w->mouse_button_state = event->motion.state;
        if (w->on_mousemove) w->on_mousemove(w, pt.x, pt.y, event->motion.state, event->motion.xrel, event->motion.yrel);
      }
    }
  }
  else if (event->type == SDL_MOUSEBUTTONUP)
  {
    bool all_up = (SDL_GetMouseState(NULL, NULL) == 0);

    drag = false;
    resize = RESIZE_NONE;
    SDL_WM_GrabInput(SDL_GRAB_OFF);

    int x = event->button.x;
    int y = event->button.y;
    Point pt = {x,y};

    Window * w = captured;
    if (!w)
    {
      w = window_under(x, y);
      if (w)
      {
        SDL_Rect r;
        window_get_client_rect(w, &r);
        window_rect_window_to_screen(w, &r);
        if (!point_in_rect(&pt, &r))
        {
          w = NULL;
        }
      }
    }
    else if (button_captured != WINDOW_BUTTON_NONE)
    {
      //printf("button captured allup:%i\n", all_up);
      if (all_up)
      {
        SDL_Rect br;
        _window_get_button_rect(w, &br, -1);
        window_rect_window_to_screen(w, &br);
        if (point_in_rect(&pt, &br))
        {
          if (w->button_state & WINDOW_BUTTON_BIT(-1))
          {
            if (w->on_close) w->on_close(w);
          }
        }

        w->button_state = 0;
        window_dirty_chrome(w);
        button_captured = WINDOW_BUTTON_NONE;
        w = NULL;
      }
    }

    if (w)
    {
      Point pt = {x,y};
      window_pt_screen_to_client(w, &pt);
      w->last_mouse_x = pt.x;
      w->last_mouse_y = pt.y;
      w->mouse_button_state = SDL_GetMouseState(NULL, NULL);
      if (w->on_mouseup) w->on_mouseup(w, pt.x, pt.y, event->button.button, event->button.type, false);
    }

    if (all_up) captured = NULL;
  }
  else if (event->type == SDL_MOUSEBUTTONDOWN)
  {
    SDL_WM_GrabInput(SDL_GRAB_ON);

    int x = event->button.x;
    int y = event->button.y;

    Window * w = captured ? captured : window_under(x, y);
    if (w)
    {
      bool raised = !window_is_top(w);
      if (raised) window_raise(w);

      if (_mouse_window && w != _mouse_window)
      {
        if (_mouse_window->on_mouseout) _mouse_window->on_mouseout(_mouse_window, false);
      }
      if (w->on_mouseover) w->on_mouseover(w, true);
      _mouse_window = w;

      SDL_Rect r;
      window_get_client_rect(w, &r);
      window_rect_window_to_screen(w, &r);
      Point pt = {x,y};
      if (point_in_rect(&pt, &r))
      {
        captured = w;
        window_pt_screen_to_client(w, &pt);
        w->last_mouse_x = pt.x;
        w->last_mouse_y = pt.y;
        w->mouse_button_state = SDL_GetMouseState(NULL, NULL);
        if (w->on_mousedown) w->on_mousedown(w, pt.x, pt.y, event->button.button, event->button.type, raised);
      }
      else
      {
        SDL_Rect br;
        _window_get_button_rect(w, &br, -1);
        //printf("%i %i %i %i\n", br.x,br.y,br.w,br.h);
        window_rect_window_to_screen(w, &br);
        if (point_in_rect(&pt, &br))
        {
          // Clicking button
          w->button_state |= WINDOW_BUTTON_BIT(-1);
          button_captured = WINDOW_BUTTON_BIT(-1);
          captured = w;
          window_dirty_chrome(w);
          //TODO: Wasteful to dirty the whole chrome here.
        }
        else
        {
          drag_offset.x = x;
          drag_offset.y = y;
          window_pt_screen_to_window(w, &drag_offset);
          drag = true;

          resize = RESIZE_NONE;

          if (w->flags & WIN_F_RESIZE)
          {
            SDL_Rect rr;
            window_get_rect(w, &rr);
            window_rect_screen_to_window(w, &rr);
            rect_shrink(&rr, OUTLINE);
            rect_shrink(&rr, EDGE_SIZE);

            if (drag_offset.x < rr.x) resize |= RESIZE_L;
            else if (drag_offset.x > rr.w+rr.x) resize |= RESIZE_R;

            if (drag_offset.y < rr.y) resize |= RESIZE_T;
            else if (drag_offset.y > rr.h+rr.y) resize |= RESIZE_B;

            if (resize)
            {
              // We know we're in the border now, so we do a relaxed check
              const int ndist = TITLEBAR_H+EDGE_SIZE+EMBOSS_SIZE+OUTLINE;
              SDL_Rect cr;
              window_get_rect(w, &cr);
              cr.x += ndist+EMBOSS_SIZE;
              cr.y += ndist+EMBOSS_SIZE;
              cr.w -= 2*ndist+2*EMBOSS_SIZE;
              cr.h -= 2*ndist+2*EMBOSS_SIZE;
              if (cr.w < 0) { cr.x = rr.x; cr.w = rr.w;};
              if (cr.h < 0) { cr.y = rr.y; cr.h = rr.h;};
              window_rect_screen_to_window(w, &cr);

              if (drag_offset.x > cr.w+cr.x) resize |= RESIZE_R;
              else if (drag_offset.x < cr.x) resize |= RESIZE_L;

              if (drag_offset.y > cr.h+cr.y) resize |= RESIZE_B;
              else if (drag_offset.y < cr.y) resize |= RESIZE_T;

              if (resize & RESIZE_B && resize & RESIZE_T) resize ^= RESIZE_T;
              if (resize & RESIZE_L && resize & RESIZE_R) resize ^= RESIZE_L;

              window_get_rect(w, &resize_start);

              window_pt_window_to_screen(w, &drag_offset);
            }
          }
        }
      }
    }
  }
  return true;
}
