# EEPROM Memory Map — CV Input Calibration

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
