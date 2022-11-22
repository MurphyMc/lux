#pragma once
#include <stdbool.h>

#include <SDL.h>

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

void sdlcolor (uint32_t c, SDL_Color * r);
Uint32 mapcolor (SDL_Surface * surf, uint32_t normal);

// -- Geometry ---------------------------------------------------------------

typedef struct Point
{
  int x;
  int y;
} Point;

#define point_in_rect(P,R) rect_contains_point(R,P)

// Draw a filled rectangle
void rect_fill (SDL_Surface * surf, SDL_Rect * r, Uint32 color);

// Create a rect which covers two rects
void rect_cover (SDL_Rect * dst, SDL_Rect * r1, SDL_Rect * r2);

// Area of rectangle
unsigned int rect_area (SDL_Rect * r);

// Adjust a rectangle
void rect_adj (SDL_Rect * r, int x, int y, int w, int h);

// Shrink all sizes by s pixels
void rect_shrink (SDL_Rect * r, int s);

// Check if rectangle is empty / zero size
bool rect_empty (SDL_Rect * r);

// Check if a rectangle contains a given point
bool rect_contains_point (SDL_Rect * r, Point * p);

// Check if two rectangles overlap
bool rect_overlaps (SDL_Rect * r1, SDL_Rect * r2);

// Check if r2 completely obscures r1
bool rect_covered_by (SDL_Rect * r1, SDL_Rect * r2);

// Check if two rectangles are the same (any empty rectangles are equal!)
bool rect_equal (SDL_Rect * r1, SDL_Rect * r2);

// -- Window ----------------------------------------------------------------

#define WIN_F_RESIZE   1 // Window is resizable
#define WIN_F_AUTOSURF 2 // Lux maintain's window's "surf"

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
  int w, h;
  int flags;
  uint32_t bg_color;
  int last_mouse_x, last_mouse_y;
  int mouse_button_state;
  SDL_Surface * surf;
  char * title;
  struct Window * above;
  struct Window * below;
  bool dirty;
  bool dirty_chrome;
  bool will_draw;
  bool (*on_draw)(struct Window *, SDL_Surface * scr, SDL_Rect scrrect); // Return true to skip drawing
  void (*on_close)(struct Window *);
  void (*on_destroy)(struct Window *); // Always happens; after close
  bool (*on_resize)(struct Window *, SDL_Surface * old_surface); // Return false if you didn't draw on new surf
  void (*on_resized)(struct Window *); // Fired once after resize (not during, like above)
  void (*on_raise)(struct Window *, bool raised);
  void (*on_lower)(struct Window *, bool raised);
  void (*on_mousedown)(struct Window *, int x, int y, int button, int type, bool raised);
  void (*on_mouseup)(struct Window *, int x, int y, int button, int type, bool raised);
  void (*on_mousemove)(struct Window *, int x, int y, int buttons, int dx, int dy);
  void (*on_mousein)(struct Window *, bool over);
  void (*on_mouseout)(struct Window *, bool over);
  void (*on_keydown)(struct Window *, SDL_keysym *, bool down);
  void (*on_keyup)(struct Window *, SDL_keysym *, bool down);
  void * opaque_ptr;    // This is like a single window prop
  int opaque_int;       // This is like a single window prop
  WindowProp * props;
  int props_count;
  uint8_t button_state; // Bits for titlebar buttons pressed
  SDL_Cursor * cursor;
  bool cursor_hidden;
} Window;

#define NOMOVE 0x7fFFff
#define NORESIZE NOMOVE

Window * window_dirty (Window * w);
void window_get_rect (Window * w, SDL_Rect * r);
void window_close (Window * w);
Window * window_create (int w, int h, const char * caption, int flags);
bool window_is_top (Window * w);
Window * window_below (Window * w);
Window * window_above (Window * w);
Window * window_raise (Window * w);
void window_get_rect (Window * w, SDL_Rect * r);
Window * window_set_title (Window * w, const char * title);
void window_cursor_show (Window * w, bool shown);
void window_cursor_set (Window * w, SDL_Cursor * c);
// Returns client rect in window coords
void window_get_client_rect (Window * w, SDL_Rect * r);
Window * window_clear_client (Window * w, uint32_t color);
Window * window_under (int x, int y);
Window * window_move (Window * w, int x, int y); // x/y can be NOMOVE
void window_resize (Window * w, int x, int y); // x/y can be NORESIZE

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
void * window_get_pixels (Window * w);
SDL_Surface * window_swap_surface (Window * wnd, SDL_Surface * surf);

// -- Misc -------------------------------------------------------------------

void lux_set_bg_color (uint32_t color);
bool lux_init (int w, int h, const char * window_name); // true on success
void lux_loop ();
bool lux_draw (); // true if drew
bool lux_do_event (SDL_Event * event); // true if not quit
int lux_poll_for_event (void);
int lux_wait_for_event (void);
bool lux_is_quitting (void);
void lux_terminate ();
int lux_sysfont_w ();
int lux_sysfont_h ();
Theme lux_get_theme ();

void text_draw (SDL_Surface * surf, const char * s, int x, int y, uint32_t color);

SDL_Surface * surface_create (int w, int h, int depth, void * buf, int pitch, int flags);
