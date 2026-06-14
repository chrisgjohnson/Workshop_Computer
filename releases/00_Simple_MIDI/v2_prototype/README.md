

# MIDI Interface

Switch position at power-up selects the mode. Hold down to enter calibration mode (LEDs show a 'C'), otherwise card enters MIDI interface mode.

### MIDI in to CV
| Jack | MIDI |
|------|------|
| Audio Out 1 | CC 42, Channel 1 |
| Audio Out 2 | CC 42, Channel 2 |
| CV Out 1 | Pitch of most recent MIDI note on channel 1 that is still down, plus pitchbend. 1V/oct |
| CV Out 2 | Pitch of most recent MIDI note on channel 2 that is still down, plus pitchbend. 1V/oct  |
| Pulses Out 1 | High if any note held down on channel 1 |
| Pulses Out 2 | High if any note held down on channel 2 |

The LEDs indicate the state of these six jacks.

### CV/knobs to MIDI out

| CC | Source | 
|----|--------|
| 34 | Main Knob |
| 35 | Knob X |
| 36 | Knob Y | 
| 37 | Switch |
| 38 | Audio In 1 | 
| 39 | Audio In 2 |
| 40 | CV In 1 |
| 41 | CV In 2 | 
All sent on MIDI channel 1.

### Changes from Simple MIDI v0.6.6

| Area | v0.6.6 | simple_midi_2 |
|------|--------|---------------|
| **Note priority / polyphony** | No note stack — any Note Off immediately drops the gate, even if other notes are still held | Full last-note priority with note resume: gate stays high until all notes released; CV reverts to previously held note on release |
| **Pitch bend** | Not supported | ±2 semitones applied to CV out (1/256-semitone resolution) |
| **Switch CC value** | Raw ADC divided by 32 (range varies) | Fixed: Down = 0, Middle = 64, Up = 127 |
| **USB mode** | Device only (computer connects to card) | Auto-detects device vs host at startup |
| **Calibration method** | Hardware: knobs and switch, CV out only | Web browser: CV out, CV and audio in, oscillator tracking |


---

# EEPROM Memory Map: CV Input Calibration

Region starts at byte 88 (`EEPROM_INPUT_ADDR`), total 38 bytes (`EEPROM_INPUT_NUM_BYTES`).

| EEPROM address | Region offset | Size | Field | Notes |
|----------------|---------------|------|-------|-------|
| 88-89 | 0-1 | 2 B | Magic ID | `2002` (0x07D2), big-endian uint16 |
| 90-91 | 2-3 | 2 B | Reserved | Unused |
| 92-95 | 4-7 | 4 B | AudioIn1 `adcOffset` | int32 big-endian; ADC count at 0 V input |
| 96-99 | 8-11 | 4 B | AudioIn1 `mvPerAdcQ16` | int32 big-endian; mV per ADC count × 65536 |
| 100-103 | 12-15 | 4 B | AudioIn2 `adcOffset` | int32 big-endian |
| 104-107 | 16-19 | 4 B | AudioIn2 `mvPerAdcQ16` | int32 big-endian |
| 108-111 | 20-23 | 4 B | CVIn1 `adcOffset` | int32 big-endian |
| 112-115 | 24-27 | 4 B | CVIn1 `mvPerAdcQ16` | int32 big-endian |
| 116-119 | 28-31 | 4 B | CVIn2 `adcOffset` | int32 big-endian |
| 120-123 | 32-35 | 4 B | CVIn2 `mvPerAdcQ16` | int32 big-endian |
| 124-125 | 36-37 | 2 B | CRC-CCITT | Over bytes 88-123 (36 bytes), big-endian |

"ADC count" is the signed integer value returned by `AudioIn1()`/`AudioIn2()`/`CvIn1()`/`CvIn2()` (range −2047 to 2048 (!)) after oversampling, DNL correction, filtering. `adcOffset` is the value that reading returns at 0 V input.
