# Au Fil de la Vie

An original 2D raylib game for the "au fil de la vie" jam theme.

You play as Sori, a small courier carrying a living thread across a ritual field. Touch spirit seeds in order, weave the line between them, and return home to seal each pattern. The woven thread can stun shadow creatures, so the path you create becomes your shield.

## Controls

- `WASD` or arrow keys: move
- `Space`: dash
- `Enter`: start / restart

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

## Run

```bash
./build/AuFilDeLaVie
```
