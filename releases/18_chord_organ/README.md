# 18 Chord Organ

Chord Organ for the Music Thing Modular Workshop Computer. Replicates the behaviour of the standalone Chord-Organ module: 16 chords, up to 8 voices per chord, 1V/oct root control, glide, and optional stacked (detuned doubled) voices.

## Controls

| Input | Function |
|-------|----------|
| **Main knob** | Chord selection (0–15). Summed with CV 1. |
| **CV 1** | Chord CV: add to Main knob for voltage-controlled chord. |
| **X knob** | Root transpose (semitones). |
| **CV 2** | Root note (1V/oct). |
| **Audio 1** | VCA control: 0V to +5V controls output volume (0% to 100%). Full volume when disconnected. |
| **Pulse 1** | Reset / trigger: retriggers chord update and reset pulse/LED. |
| **Pulse 2** | Cycle waveform (sine → triangle → square → saw) on each trigger. |
| **Switch Down** | Cycle waveform (sine → triangle → square → saw). |

## Outputs

| Output | Function |
|--------|----------|
| **Audio 1 & 2** | Mixed chord (same on both). |
| **CV 1** | Highest note in current chord (1V/oct, calibrated). |
| **CV 2** | (Available for future use). |
| **Pulse 1** | Brief pulse on chord or root change (or Pulse 1 in rising edge). |
| **LEDs 0–3** | Chord index in binary (0–15). |
| **LED 4** | Reset indicator (on while reset pulse is high). |
| **LED 5** | Waveform index (brightness). |

## Chord list (built-in)

1. Major  
2. Minor  
3. Major 7th  
4. Minor 7th  
5. Major 9th  
6. Minor 9th  
7. Suspended 4th  
8. Power 5th  
9. Power 4th  
10. Major 6th  
11. Minor 6th  
12. Diminished  
13. Augmented  
14. Root  
15. Sub octave  
16. 2 up 1 down octaves  

## Build

Requires Pico SDK and (optionally) `PICO_SDK_PATH` set.

**Toolchain:** Arm GCC **15.x** can fail with `cannot find -lg` when linking the Pico SDK boot stage. Use one of:

- **Arm GNU Toolchain 10.3-2021.10** (discontinued embedded): https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads — pick **arm-none-eabi** for your OS; includes `libg.a`.
- **Arm GNU Toolchain 13.2 or 14.2** from the main downloads page (older releases) — these usually still ship the expected newlib layout.

Install one of the above, put its `bin` directory first in your `PATH`, then build.

```bash
cd 18_chord_organ
mkdir build && cd build
cmake ..
make
```

Copy `chord_organ.uf2` to the Workshop Computer when in bootloader mode.

## Firmware

- **Platform:** Pico SDK (C++), ComputerCard
- **Sample rate:** 48 kHz
- **Features:** Glide (50 ms), optional stacked mode (compile-time)
