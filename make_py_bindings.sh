#!/bin/bash

if [ -z "$CTYPESGEN" ]; then
  CTYPESGEN="$(which ctypesgen)"
  if [ -z "$CTYPESGEN" ]; then
    echo "No ctypesgen?"
    exit 1
  fi
fi

DIR=$(dirname -- "$(readlink -f -- "${BASH_SOURCE[0]}")")
cd "$DIR"

if [ -z "$SDL" ]; then
  # First look in this directory, then parent, then ask pkg-config
  if [[ -d SDL-1.2/include ]]; then
    SDL=SDL-1.2/include
  elif [[ -d ../SDL-1.2/include ]]; then
    SDL=../SDL-1.2/include
  else
    SDL=$(pkg-config --cflags-only-I sdl 2> /dev/null)
    SDL="${SDL#-I}"
    if [ -z "$SDL" ]; then
      echo "No SDL 1.2?"
      exit 2
    fi
  fi
fi

cmake --build .

echo "Using ctypesgen: $CTYPESGEN"
echo "Using SDL: $SDL"

exec $CTYPESGEN -L. -l./liblux.so -I$SDL lux.h $SDL/SDL_keysym.h -o lux_gui/liblux.py
