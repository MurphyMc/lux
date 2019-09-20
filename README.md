# Lux

Lux, or lux-gui, is a Lightweight User eXperience.  It's a small windowing
toolkit for making desktop-like user interfaces.  It's written hastily in C.
Patches are welcome.

It currently is tied to SDL for getting events and displaying output, and
it uses some SDL structures as part of its interface.  It should be possible
to port it to non-SDL systems (i.e., framebuffers plus some input), but
the easiest way to do this is surely to port at least parts of SDL to
the system (which is useful in itself).

It's my belief that the included font comes from X and is in the public
domain. Thanks go to Lars C. Hassing for making it available in an
easy-to-consume form.  More information on the font file can be found with
the [Antsy terminal emulator](https://github.com/MurphyMc/antsy).

## The Demo

Lux comes with a little demo program.  Press F1 to create a new window.
There is a keyboard-based window switcher which is bound to Alt-~ and
Alt-Shift-~ by default.

Compiling the demo is just a matter of compiling the two C files and linking
with SDL.  There's a cmake file which will do it for you:
```
cmake .
cmake --build .
./lux-demo
```

## Using Lux

Please feel free to write better documentation.  At present, the best way
to learn to use Lux is to read the demo program (it's short!), and then
look through the header file.  There are various other routines in the C
file which aren't exposed, but some of them are useful and maybe they
should be.  Submit a pull request or file an issue if you think something
vital is missing or should be exposed instead of hidden.

A very short crash course:

* There are functions for dealing with rectangles, which start with `rect_`.
* There are general functions starting with `lux_`.
* There are functions for dealing with windows, starting with `window_`.
* Windows are represented by the structure `Window`.
* Windows have a number of event-handling functions starting with `on_`,
  which you use to override window behavior.
* The F-keys can be used as global hotkeys.
* You can either let Lux run an event loop (`lux_loop`), or you can run
  your own SDL event loop, passing events to Lux (`lux_do_event`).
