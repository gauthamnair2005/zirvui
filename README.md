# ZirvUI — MOSIX Desktop Compositor (Reference GUI)

Graphical desktop compositor / graphical shell for **MOSIX** operating systems.
Connects to the [DisplayJet](https://github.com/gauthamnair2005/zirvdisplayjet)
kernel driver to render a complete desktop environment. Links against
[ZirvTK](https://github.com/gauthamnair2005/zirvtk) for the desktop compositor
widget system.

Part of the [Zirvium](https://github.com/gauthamnair2005/zirvium) reference
MOSIX implementation.

## Features

- **ZirvTK Desktop Compositor** — full Rust GUI toolkit with clock, taskbar, app launcher
- Procedural per-pixel background gradient
- Panel with launcher button and system clock
- App grid with SVG vector icons
- Particle effects and glass UI aesthetic
- DisplayJet hardware cursor
- VBE page-flipped double buffering for tear-free rendering

## Build

```bash
make
```

Produces `zirvui.elf` — static, freestanding, no-pie ELF64.
Linked against [ZirvTK](https://github.com/gauthamnair2005/zirvtk),
[zirvlibc](https://github.com/gauthamnair2005/zirvlibc),
and [zirvflux](https://github.com/gauthamnair2005/zirvflux).

## Dependencies

- [zirvtk](https://github.com/gauthamnair2005/zirvtk) — Rust desktop toolkit
- [zirvflux](https://github.com/gauthamnair2005/zirvflux) — Display framework
- [zirvlibc](https://github.com/gauthamnair2005/zirvlibc) — C library
- [zirvdisplayjet](https://github.com/gauthamnair2005/zirvdisplayjet) — Display driver

## License

GPLv3
