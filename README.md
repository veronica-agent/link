# Link

MIDI clock for a Game Boy. In your pocket.

A Flipper Zero app: USB MIDI on one side, Game Boy link on GPIO. Little Sound DJ slave and master, plus tap tempo.

This repository is **private** while the FAP is proven on a desk. Catalog / public GitHub come later.

## Modes

| Mode | LSDJ `sync` | What happens |
|------|-------------|--------------|
| SLAVE | `Slave` | DAW MIDI clock in → eight CLK edges per tick |
| MASTER | `Master` | Game Boy CLK in → MIDI clock out |
| TAP | `Slave` | Internal clock. No USB required. |

Groove for MIDI slave: **6 ticks/step**. Press Start on LSDJ so it shows WAIT, then start the DAW.

## Desk kit

- Flipper Zero (official firmware)
- [KBEmbedded GB Link](https://www.tindie.com/products/kbembedded/game-link-gpio-module-for-flipper-zero-game-boy/) (or a continuity-checked spliced cable)
- Game Boy Color, GBA SP, or Analogue Pocket
- LSDJ on a flash cart ([littlesounddj.com](https://www.littlesounddj.com) — we do not ship ROMs)
- USB **data** cable. Quit qFlipper first.

Original pinout (default):

| Link pin | Signal | Flipper |
|----------|--------|---------|
| 6 | GND | 8 (GND) |
| 5 | CLK | 6 (B2) |
| 3 | SI | 7 (C3) |
| 2 | SO | 5 (B3) |

Hold **Back** to switch **MLVK25** (MALVEKE Rev 2.5). Original DMG is 5 V and untested.

## Controls

| Button | Action |
|--------|--------|
| Left / Right | Mode |
| OK | Start / stop |
| Up / Down | TAP: BPM. SLAVE: clock divide |
| Hold Back | Pinout |
| Back | Exit (restores USB) |

MIDI notes on channel 16: 48 start, 49 stop, 50–53 divide 1/1 1/2 1/4 1/8.

## Build

Official firmware only. [ufbt](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
just sdk
just build          # dist/link.fap
just launch         # quit qFlipper first
```

Copy `dist/link.fap` to `apps/GPIO/` on the SD card if you are not launching over USB.

## License

Apache 2.0.

Clock behavior follows [ArduinoBoy](https://github.com/trash80/Arduinoboy) modes 1 and 2 by trash80 (GPL-2.0) as a spec. This tree is a new implementation. Pinout follows KBEmbedded Flipper GB Link. LSDJ is © Johan Kotlinski.
