# goober – mini roulette on an ESP32-C3

Pocket roulette game: 1.3" OLED + rotary encoder + 2 buttons. Screens follow the original sketch: main menu with wheel, bets table with chip wheel, curved spinning band, zoom-in result.

Working with subagents: see `subagents.md`.

## Hardware
- ESP32-C3 SuperMini, 1.3" SH1106 128x64 I2C OLED at 0x3C (0.96" SSD1306 also supported: `OLED_SH1106 0`).
- All pins and tunables live in `include/config.h`.

| Part | Pin |
|---|---|
| OLED SDA / SCL | 3 / 4 |
| Encoder A / B / push | 0 / 20 / 1 |
| Confirm / Back | 10 / 21 |

GPIO 5 and 6 are shorted on the current wiring – avoid them.

## Build / flash (PlatformIO CLI)
```bash
pio run -t upload            # game (default env)
pio run -e diag -t upload    # pin/I2C diagnostic (src/diag/), read with: pio device monitor
```
After using `diag`, flash the game again. If upload says "Permission denied" on `/dev/ttyACM*`: user is in `uucp`, re-login (or prefix with `sg uucp -c "..."`).

## Code map (`src/`)
- `main.cpp` – loop, screen switching, transitions (menu↔bets morph, slides).
- `gfx.*` – Adafruit SH110X/SSD1306 + GFX wrapper: clipped primitives, wedges, bitmaps, fonts, easing. Screens only draw through this.
- `input.*` – encoder (ISR, quadrature) + debounced buttons.
- `game.*` – wheel order, pocket/wheel drawing (hub shows the Pontune mark), chips, bets, payouts, NVS balance save.
- `menu.cpp` (splash, menu, morph), `bets.cpp`, `spin.cpp` (flick physics + result), `ad.cpp` (pontune.app ad when broke).
- `pontune_logo.h` (long navbar logo, used in the ad) and `pontune_mark.h` (square logo, wheel hubs) – 1-bit bitmaps converted from pontune.app `mainlonglogo.png` / `mainlogo.png`.

## Game rules / controls
- Pockets 0–8; odd = WHITE, even = BLACK. Straight pays 8x, 1-4 / 5-8 / BLACK / WHITE pay 2x, 0 kills outside bets (every bet has the same 1/9 edge).
- Chips are % of the pre-spin balance (1/5/10/25/50%, ALL = what's left); they only change after a spin.
- Bets table: 0 on the left, then rows 1-4, 5-8, [1-4][5-8]; BLACK/WHITE below; bottom row = last 5 results (newest left, slides in after each spin). History (last 8) is saved to NVS with the balance.
- Bets screen: turn = move cursor, tap encoder = place bet (small wobble while pressed is ignored), hold encoder 300 ms = chip selector (`HOLD_MS` in `bets.cpp`; one continuous dip → fling animation). In the selector: turn to pick, tap/OK to accept, back to cancel. Back tap = undo, back hold = menu, OK = go to spin.
- Spin: flick the encoder either way; 3+ clicks within 160 ms launches instantly with speed from the flick rate (`FLICK_*` in `spin.cpp`), slower turning just nudges the band. OK = random spin.
- Balance hits $0 → 5 s unskippable pontune.app ad → reset to $1000.

## State
Feature-complete and running on hardware. Possible polish: tune spin speed/friction and flick threshold in `spin.cpp`, try `I2C_CLOCK_HZ 800000` for smoother frames, fix the GPIO 5/6 short if more pins are needed.
