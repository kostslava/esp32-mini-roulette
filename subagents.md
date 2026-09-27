# Subagent workflow

Applies to every new chat in this workspace.

## Roles
- **Orchestrator**: the main chat model. It plans, designs the architecture,
  defines interfaces between modules, splits work into small tasks, reviews
  every subagent result, integrates, and builds/verifies the project.
- **Drones**: subagents that do the small, well-scoped implementation work.

## Drone model
- Preferred: `grok-4.7-medium`, otherwise `cursor-grok-4.6-medium`.
- No "fast" variants.
- If neither preferred model is available in the session, do NOT substitute
  silently. Tell the user which models are available and ask which to use
  (or whether the orchestrator should do the work itself).

## How to split work
- One drone task = one file or one self-contained function group
  (roughly < 200 lines of output). Never "build the whole game".
- The orchestrator writes the shared header / interfaces FIRST, so drones
  code against a fixed contract (function names, globals, pins, screen size).
- Each drone prompt must include:
  1. Exact file path(s) it may create or edit (and nothing else).
  2. The interface it must implement / may call (paste the relevant header).
  3. Hardware/library constraints (e.g. ESP32-C3, U8g2, SH1106 128x64).
  4. Acceptance criteria (compiles, no blocking delays, frame budget, etc.).
  5. "Do not modify other files. Report what you did and any assumptions."
- Independent tasks run in parallel; dependent ones run after their inputs exist.

## After drones finish
- Orchestrator reads every changed file, fixes inconsistencies, and runs the
  build (`pio run`) before reporting to the user. Don't trust drone reports
  without checking.

## Project notes (goober)
- Target: ESP32-C3 SuperMini, PlatformIO CLI (user edits in Zed).
- OLED: SH1106 128x64 I2C, SDA=20, SCL=21.
- Encoder: A=3, B=5, push=4. Confirm button=10. Back button=6.
- Buttons are active LOW (INPUT_PULLUP).
