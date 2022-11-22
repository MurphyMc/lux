"""
A simple demo of using liblux from Python

.. *or*, a little library for using liblux from Python.
"""
try:
  from .pyliblux import *
except Exception:
  import os
  import sys
  sys.path.append( os.path.dirname(os.path.abspath(__file__)) )
  from pyliblux import *

from time import sleep
import threading
from ctypes import *


lock = threading.Lock()

should_quit = False

window_func_type = CFUNCTYPE(UNCHECKED(None), POINTER(struct_Window))
fkey_handler_type = key_register_fkey.argtypes[2]

windows = []


def run_loop (on_exit=None):
  while not lux_is_quitting():
    with lock:
      lux_draw()
      p = lux_poll_for_event()
    if p == -1:
      sleep(0.016*2)
      with lock:
        for w in windows:
          if getattr(w, 'auto_redraw', False):
            window_dirty(w)
  lux_terminate()
  if on_exit: on_exit()


@window_func_type
def on_close (w):
  lux_terminate()


def is_quit ():
  return should_quit


def run (on_exit=None):
  if on_exit is True:
    def exit_func ():
      #print("Quitting")
      global should_quit
      should_quit = True
    on_exit = exit_func

  t = threading.Thread(target=run_loop, args=(on_exit,))
  t.start()
  return t



if __name__ == '__main__':
  lux_set_bg_color(0x402060)

  lux_init(640, 480, None)

  t = run(True)

  with lock:
    @fkey_handler_type
    def my_f1_handler (fkey):
      w = window_create(256, 256, "My Window", WIN_F_AUTOSURF)
      windows.append(w)

    key_register_fkey(SDLK_F1, KMOD_NONE, my_f1_handler)

    w = window_create(256, 256, "My Window", WIN_F_AUTOSURF)
    w.contents.on_close = on_close
    w.auto_redraw = True # Oooh, special feature of our draw loop

    windows.append(w)

    pix = cast(window_get_pixels(w), POINTER(c_uint))

  i = 0
  while not should_quit:
    for y in range(0, 256):
      for x in range(0, 256):
        pix[x+y*256] = (i % 1024) << 4
    sleep(.01)
    i += 1

  t.join()
