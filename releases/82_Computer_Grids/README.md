# 82_Computer_Grids

Grids-inspired trigger sequencer firmware for the Music Thing Workshop Computer.

This card adapts the Mutable Instruments Grids concept to Workshop Computer hardware constraints (fewer panel controls and I/O), with:

- X/Y map control
- Fill macro control
- Internal clock with optional external clock on `PulseIn1`
- Reset on `PulseIn2`
- Alt layer on long-press (`Z`) with knob pickup/catch behavior
- USB MIDI SysEx configuration transport with persistent flash config

## Quick Start (User)

1. Flash `UF2/82_Computer_Grids.uf2`.
2. With no pulse clock patched to `PulseIn1`, internal clock runs automatically.
3. Turn:
   - `X` for map X position
   - `Y` for map Y position
   - `Main` for global fill macro
4. Patch outputs:
   - `PulseOut1` and `PulseOut2` for trigger lanes 1 and 2
   - `CVOut1` for trigger lane 3 (digital pulse-style output)
   - `CVOut2` for aux output (accent/clock/lane3 mirror, configurable)

## Controls

- **Knob `X`**: pattern map X position
- **Knob `Y`**: pattern map Y position
- **Knob `Main`**: global fill macro (all lanes)
- **Switch `Z` short press (Down press/release)**: tap tempo (internal clock mode)
- **Switch `Z` long press**: toggle alt layer with knob pickup/catch

### Alt Layer

When alt layer is active:

- `Main` adjusts chaos/randomness
- `X` adjusts BPM range mapping
- `Y` adjusts swing amount

Changes are persisted with deferred flash save.

## Inputs and Outputs

### Inputs

- **`PulseIn1`**: external clock input
  - If connected, firmware follows external clock edges.
  - If not connected, internal clock runs.
- **`PulseIn2`**: reset input
  - Rising edge resets pattern phase.
- **`CVIn1`**: map modulation input
  - Assignable target: X, Y, or blended XY (via config).
- **`CVIn2`**: fill macro modulation input
  - Amount configurable.

### Outputs

- **`PulseOut1`**: trigger lane 1
- **`PulseOut2`**: trigger lane 2
- **`CVOut1`**: trigger lane 3 (digital pulse behavior)
- **`CVOut2`**: aux output mode (accent / clock / lane3 mirror)

## LED Behavior

- **LED 0**: beat tick blink
- **LED 1**: fill amount brightness
- **LED 2**: lane 1 trigger activity
- **LED 3**: X control brightness
- **LED 4**: lane 2 trigger activity
- **LED 5**: lane 3/aux activity (or alt-layer indicator)

## Build

From this folder:

```bash
make
```

UF2 output is copied to:

- `UF2/82_Computer_Grids.uf2`

## Flash

Use your normal Workshop Computer flashing workflow (debug probe/OpenOCD or UF2 drag-and-drop workflow as used in this repo).

## Files of interest

- `main.cpp` - dual-core startup (`ComputerCard` audio on core 1, housekeeping + USB on core 0)
- `GridsCard.cpp` / `GridsCard.h` - card behavior, UI, clocking, I/O, SysEx handling
- `GridsEngine.cpp` / `GridsEngine.h` - pattern evaluation logic
- `GridsResources.cpp` / `GridsResources.h` - Grids map node resources used for interpolation
- `ConfigStore.cpp` / `ConfigStore.h` - persistent flash config
- `sysex_spec.json` - web/editor integration spec for config payloads
- `web/` - basic browser GUI for reading/writing config over Web MIDI SysEx

## Web GUI

A basic web editor is included in:

- `web/index.html`

Open it in a Chromium-based browser, click **Connect Web MIDI**, select the card MIDI input/output, then use:

- **Read From Device** - requests config via SysEx (`0x01`)
- **Write To Device** - sends current UI values via SysEx (`0x03`)
- **Start Monitor** - periodically polls current config and updates the UI

The web UI includes paired sliders + numeric fields for main continuous parameters.

The GUI implements the same 7-bit block packing scheme documented in `sysex_spec.json`.

USB MIDI bring-up matches **`20_reverb`**: **200 MHz** boot clock, **`tusb_config.h`** with **device + host** on rhport0, **`tinyusb_host`** linked, **100 ms** delay, then **`tud_init(0)`** or **`tuh_init(0)`** from **`USB_HOST_STATUS` / board revision** (same logic as reverb’s `usb_worker`), then **`board_init()`**. **`tud_task()` / `tuh_task()`** run on **core 0**; **48 kHz audio** runs on **core 1** (like reverb’s `audio_worker`). On 2025 boards, use the **UFP** USB-C port (toward the PC) for Web MIDI; the other port is USB host mode.

## Recent Changes

- Release folder follows numbered convention: `82_Computer_Grids`
- UF2 artifact now correctly named `82_Computer_Grids.uf2`
- SysEx config transport upgraded to 7-bit block packing (MIDI-safe)
- Web GUI now includes live monitor polling and slider+numeric paired controls
- LED driving moved to audio-thread path for reliable visible activity

## Attribution and licensing

This firmware includes adaptations derived from Mutable Instruments Grids:

- Pattern-map structure and interpolation approach inspired by Grids pattern generator
- Adapted node resource data used by the Grids map

Original project:

- Mutable Instruments / pichenettes eurorack Grids source:
  - [https://github.com/pichenettes/eurorack/tree/master/grids](https://github.com/pichenettes/eurorack/tree/master/grids)
- Module documentation:
  - [https://pichenettes.github.io/mutable-instruments-documentation/modules/grids/](https://pichenettes.github.io/mutable-instruments-documentation/modules/grids/)

The upstream Mutable Grids source is released under GPLv3 (or later). This folder is licensed accordingly; see `COPYING` for the folder-scoped licensing notice and caveat.

## Credit

- Original Grids design and firmware: Emilie Gillet / Mutable Instruments
- Workshop Computer adaptation in this folder: community adaptation for Music Thing Workshop Computer

