#pragma once
#include <stdbool.h>


// -- Keyboard ---------------------------------------------------------------

typedef struct FKey
{
  SDLKey key;
  SDLMod mods;
  void (*callback)(struct FKey * k);
} FKey;


// returns true on success
bool key_register_fkey (SDLKey key, SDLMod mods, void (*callback)(FKey*));

bool key_unregister_fkey (SDLKey key, SDLMod mods);


// -- Appearance -------------------------------------------------------------

typedef struct WinColors
{
  Uint32 outline;
  Uint32 edge_tl;
  Uint32 edge_br;
  Uint32 face;
  Uint32 title;
  Uint32 title_shd;
} WinColors;

typedef struct Theme
{
  WinColors win;
  WinColors winde; // inactive
} Theme;


#define OUTLINE      1 /* Or 0 */
#define EMBOSS_SIZE  2
#define EDGE_SIZE    6
#define TITLEBAR_H  17

#define TITLE_BOLD   1


// -- Color ------------------------------------------------------------------

extern Uint32 rmask;
extern Uint32 gmask;
extern Uint32 bmask;
extern Uint32 amask;

static void sdlcolor (uint32_t c, SDL_Color * r)
{
  r->b = (c >>  0) & 0xff;
  r->g = (c >>  8) & 0xff;
  r->r = (c >> 16) & 0xff;
}

static Uint32 mapcolor (SDL_Surface * surf, uint32_t normal)
{
  SDL_Color c;
  sdlcolor(normal, &c);
  return SDL_MapRGB(surf->format, c.r, c.g, c.b);
}


// -- Geometry ---------------------------------------------------------------

typedef struct Point
{
  int x;
  int y;
} Point;

static void rect_adj (SDL_Rect * r, int x, int y, int w, int h)
{
  r->x += x;
  r->y += y;
  r->w += w;
  r->h += h;
}

static void rect_shrink (SDL_Rect * r, int s)
{
  rect_adj(r, s, s, -2*s, -2*s);
}

static bool rect_empty (SDL_Rect * r)
{
  return (r->w <= 0) || (r->h <= 0);
}

static bool rect_contains_point (SDL_Rect * r, Point * p)
{
  return (p->x >= r->x) && (p->x < (r->x + r->w))
      && (p->y >= r->y) && (p->y < (r->y + r->h));
}

#define point_in_rect(P,R) rect_contains_point(R,P)


static bool rect_equal (SDL_Rect * r1, SDL_Rect * r2)
{
  if (r1->x != r2->x) goto fail;
  if (r1->y != r2->y) goto fail;
  if (r1->w != r2->w) goto fail;
  if (r1->h != r2->h) goto fail;
  return true;
fail:
  if (rect_empty(r1) && rect_empty(r2)) return true;
  return false;
}

bool rect_overlaps (SDL_Rect * r1, SDL_Rect * r2);

// Does r2 completely obscure r1?
bool rect_covered_by (SDL_Rect * r1, SDL_Rect * r2);


// -- Window ----------------------------------------------------------------

#define WIN_F_RESIZE 1 // Window is resizable

#define WIN_MIN_W 40
#define WIN_MIN_H 40
#define WIN_MAX_W 4096
#define WIN_MAX_H 4096

struct Window;

typedef struct WindowProp
{
  int id;
  void (*free)(struct Window * w, struct WindowProp * p);
  union
  {
    void * ptr;
    intptr_t value;
    uintptr_t uvalue;
  };
} WindowProp;

typedef struct Window
{
  int x, y;
  int flags;
  uint32_t bg_color;
  void* (*pix_alloc)(size_t);
  void (*pix_free)(struct Window *, void *);
  int last_mouse_x, last_mouse_y;
  int mouse_button_state;
  SDL_Surface * surf;
  bool fixed;
  char * title;
  struct Window * above;
  struct Window * below;
  bool dirty;
  bool dirty_chrome;
  bool will_draw;
  bool (*on_draw)(struct Window *, SDL_Surface * scr, SDL_Rect scrrect); // Return true to skip drawing
  void (*on_close)(struct Window *);
  bool (*on_resize)(struct Window *, SDL_Surface * old_surface); // Return false if you didn't draw on new surf
  void (*on_raise)(struct Window *);
  void (*on_lower)(struct Window *);
  void (*on_mousedown)(struct Window *, int x, int y, int button, int type, bool raised);
  void (*on_mouseup)(struct Window *, int x, int y, int button, int type, bool raised);
  void (*on_mousemove)(struct Window *, int x, int y, int buttons, int dx, int dy);
  void (*on_mouseover)(struct Window *, bool over);
  void (*on_mouseout)(struct Window *, bool over);
  void (*on_keydown)(struct Window *, SDL_keysym *, bool down);
  void (*on_keyup)(struct Window *, SDL_keysym *, bool down);
  void * opaque_ptr;
  int opaque_int;
  WindowProp * props;
  int props_count;
  uint8_t button_state; // Bits for titlebar buttons pressed
} Window;

#define NOMOVE 0x7fFFff

Window * window_dirty (Window * w);
void window_get_rect (Window * w, SDL_Rect * r);
void window_close (Window * w);
Window * window_create (int w, int h, const char * caption, int flags, void* (*pix_alloc)(size_t));
bool window_is_top (Window * w);
Window * window_below (Window * w);
Window * window_above (Window * w);
Window * window_raise (Window * w);
void window_get_rect (Window * w, SDL_Rect * r);
// Returns client rect in window coords
void window_get_client_rect (Window * w, SDL_Rect * r);
Window * window_clear_client (Window * w, uint32_t color);
Window * window_under (int x, int y);
Window * window_move (Window * w, int x, int y); // x/y can be NOMOVE

void window_pt_screen_to_window (Window * w, Point * pt);
void window_pt_window_to_screen (Window * w, Point * pt);
void window_pt_client_to_screen (Window * w, Point * pt);
void window_pt_screen_to_client (Window * w, Point * pt);
void window_rect_screen_to_window (Window * w, SDL_Rect * r);
void window_rect_window_to_screen (Window * w, SDL_Rect * r);
void window_rect_client_to_screen (Window * w, SDL_Rect * r);
void window_rect_screen_to_client (Window * w, SDL_Rect * r);

bool window_contains_pt (Window * w, Point * pt); // pt in screen coords

// Windows can have properties.  These are a place to stash your own data
// inside a window so that you don't need to keep an external data structure
// which maps between a Window and your own data.
// Each property has an id, which is a non-negative interger.  You can use
// window_prop_id() to map a string to a numeric id.  You can call this
// again and again (which will cost you), or just do it once and save the
// return value to access the property in the future.
// You can access the same backing data with either a "prop" or a "ptr"
// function, so don't mix and match for the same id.  If a property
// has a free function, it's called on the property when the window closes.
// The idea is that the window "owns" the data, and you don't need to
// clean it up yourself in an on_close().  You can manually set a free
// function, but if you call window_ptr_set() and there isn't a free
// function already, it'll set the id up to just call free() on the data.
bool window_prop_set (Window * w, int id, intptr_t value);
intptr_t window_prop_get (Window * w, int id);
bool window_ptr_free_set (Window * w, int id, void (*freefunc)(Window *, WindowProp *));
bool window_ptr_set (Window * w, int id, void * value);
void * window_ptr_get (Window * w, int id);
int window_prop_id (const char * propname); // -1 on error

// -- SDLuxer ----------------------------------------------------------------

void lux_set_bg_color (uint32_t color);
bool lux_init (int w, int h, const char * window_name); // true on success
void lux_loop ();
bool lux_draw (); // true if drew
bool lux_do_event (SDL_Event * event); // true if not quit
void lux_terminate ();
int lux_sysfont_w ();
int lux_sysfont_h ();
Theme lux_get_theme ();
void text_draw (SDL_Surface * surf, const char * s, int x, int y, uint32_t color);
