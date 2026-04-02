# 82_Computer_Grids

Grids-inspired trigger sequencer firmware for the Music Thing Workshop Computer.

This card adapts the Mutable Instruments Grids concept to Workshop Computer hardware constraints (fewer panel controls and I/O), with:

- X/Y map control (switch `Z` in **middle**)
- Fill macro control (switch `Z` in **middle**)
- Per-lane density with **Main / X / Y** when switch `Z` is **up** (pattern map is held from the last middle position)
- Internal clock with optional external clock on `PulseIn1`
- Reset on `PulseIn2`
- Alt layer on long-press (`Z`) with knob pickup/catch behavior
- USB MIDI SysEx configuration transport with persistent flash config

## Quick Start (User)

1. Flash `UF2/82_Computer_Grids.uf2`.
2. With no pulse clock patched to `PulseIn1`, internal clock runs automatically.
3. With `Z` in the **middle** (normal): turn `X` / `Y` for map, `Main` for fill macro. With `Z` **up**: `Main` = lane 1 density, `X` = lane 2, `Y` = lane 3 (map stays where you left it in middle).
4. Patch outputs:
   - `PulseOut1` and `PulseOut2` for trigger lanes 1 and 2
   - `CVOut1` for trigger lane 3 (digital pulse-style output)
   - `CVOut2` for aux output (accent/clock/lane3 mirror, configurable)

## Controls

### Switch `Z` — middle (normal)

- **Knob `X`**: pattern map X position
- **Knob `Y`**: pattern map Y position
- **Knob `Main`**: global fill macro (all lanes, with per-lane scale/offset from config)
- **Middle ↔ up**: when you flip between **middle** and **up**, the **current sound** is kept until you **move that knob** more than a small deadband (so the same physical position does not jump to a new meaning).

### Switch `Z` — up (per-lane density)

- **Knob `Main`**: density for trigger **lane 1** (`PulseOut1`)
- **Knob `X`**: density for **lane 2** (`PulseOut2`)
- **Knob `Y`**: density for **lane 3** (`CVOut1` pulse)
- **Pattern map** (X/Y on the Grids) uses the **held** knob map from the last **middle** position (`CVIn1` applies once you have “taken over” **X** or **Y** after switching — see Inputs below)

### Switch `Z` — down (momentary)

- **Short press (press then release)**: tap tempo (internal clock only)
- **Long press**: toggle alt layer (knob pickup/catch). With **middle** + alt: `Main` / `X` / `Y` adjust chaos, BPM, and swing. With **up**, knobs stay on per-lane density; randomness (chaos) is still whatever you last set in middle+alt.

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
  - While map knobs are **frozen** after a middle↔up change, CV1 is held off until you move **X** or **Y** past the takeover deadband (avoids double-applying the same offset).
- **`CVIn2`**: fill modulation input
  - **Middle**: adds to the global fill macro after **Main** has taken over following a middle↔up flip (initially on at power-up).
  - **Up**: adds to all three lane densities after **any** of **Main** / **X** / **Y** has taken over following a flip.

### Outputs

- **`PulseOut1`**: trigger lane 1
- **`PulseOut2`**: trigger lane 2
- **`CVOut1`**: trigger lane 3 (digital pulse behavior)
- **`CVOut2`**: aux output mode (accent / clock / lane3 mirror)

## LED Behavior

- **LED 0**: beat tick blink
- **LED 1**: fill macro brightness (**middle**) or lane 1 density (**up**)
- **LED 2**: lane 1 trigger activity
- **LED 3**: map X brightness (**middle**) or lane 2 density (**up**)
- **LED 4**: lane 2 trigger activity
- **LED 5**: lane 3/aux activity or alt-layer indicator (**middle**); lane 3 density brightness (**up**)

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
- **Apply To Device** - sends current UI values via SysEx (`0x03`) to update running behavior
- **Save To Card** - requests persistence to flash via SysEx (`0x04`)
- **Start Monitor** - periodically polls current config and updates the UI

The web UI includes paired sliders + numeric fields for main continuous parameters.

Connection notes:

- Use a USB-C **data** cable.
- Close Serial Monitor / other apps that may already own the device/MIDI port.
- Use a Chromium-family browser with Web MIDI SysEx enabled (iOS browsers are generally unsupported).

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

