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
