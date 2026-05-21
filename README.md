# zirvui — MOSIX Desktop Compositor (Reference GUI)

Graphical desktop compositor / graphical shell for **MOSIX** operating systems.
Connects to the [DisplayJet](https://github.com/gauthamnair2005/zirvdisplayjet)
kernel driver to render a complete desktop environment into a
MAEM-encrypted framebuffer surface.

Part of the [Zirvium](https://github.com/gauthamnair2005/zirvium) reference
MOSIX implementation. See the [MOSIX specification](https://github.com/gauthamnair2005/zirvworld)
for the full standard.

## Features

- Procedural per-pixel background gradient
- 28-pixel top panel with dark navy background
- Hamburger menu icon + "ZirvUI" branding
- System tray: WiFi, Bluetooth, speaker, battery, user, power icons
- Live uptime clock (MM:SS) using 5x7 bitmap font
- 96-glyph ASCII bitmap font (5x7) for all text rendering
- Fixed-point cos/sin lookup tables for circular icon geometry
- Full-frame encrypted writes via DisplayJet surface protocol

## Project Structure

```
src/
  main.c          Compositor main loop, rendering, DisplayJet protocol
  crt0.asm        Entry point (_start -> main -> SYS_EXIT)
Makefile          Freestanding x86_64-elf build
```

## Build

```bash
make
```

Produces `zirvui.elf` — static, freestanding, no-pie ELF64.
Linked against [zirvlibc](https://github.com/gauthamnair2005/zirvlibc)
and [displayjet.h](https://github.com/gauthamnair2005/zirvdisplayjet).

## Compositor Flow

1. `dj_connect()` — register as compositor
2. `dj_get_mode()` — query 1024x768 resolution
3. `dj_create_surface()` — allocate encrypted GPU surface
4. Render desktop into local 3 MB framebuffer
5. `dj_surface_write()` — encrypt with ChaCha20, upload
6. `dj_present()` — decrypt to scanout (~1 Hz)

## Dependencies

- [zirvlibc](https://github.com/gauthamnair2005/zirvlibc) — C library
- [zirvdisplayjet](https://github.com/gauthamnair2005/zirvdisplayjet) — Display driver

## Documentation

Full documentation: [zirvui.html](https://github.com/gauthamnair2005/zirvworld/blob/main/zirvui.html)
