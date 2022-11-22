"""
This is just a very simple demo of lux from Python

See luxutil.py for more.
"""
from lux_gui import *

lux_set_bg_color(0x402060)

lux_init(640, 480, None)

t = run(True)

with lock:
  w = window_create(256, 256, "My Window", WIN_F_AUTOSURF)
  w.contents.on_close = on_close
  windows.append(w)

t.join()
