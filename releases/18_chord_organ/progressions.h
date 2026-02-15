// progressions.h
// Chord progression patterns for Workshop Computer Chord Organ
// Root note offsets in semitones from the base root

#ifndef PROGRESSIONS_H
#define PROGRESSIONS_H

#include <cstdint>

constexpr int kProgressionCount = 10;      // 10 progression patterns (0 = off)
constexpr int kMaxProgressionLength = 8;   // Max steps per progression

// Root note offsets in semitones from the base root
// -1 indicates end of progression (wraps to start)
constexpr int8_t kProgressions[kProgressionCount][kMaxProgressionLength] = {
    // 0: Off (not used, Knob Y at minimum bypasses sequencer)
    { 0, 0, 0, 0, 0, 0, 0, 0 },

    // 1: I-IV-V-I (Classic pop/rock: C-F-G-C)
    { 0, 5, 7, 0, -1, -1, -1, -1 },

    // 2: I-V-vi-IV (Pop progression: C-G-Am-F)
    { 0, 7, 9, 5, -1, -1, -1, -1 },

    // 3: ii-V-I (Jazz turnaround: Dm-G-C)
    { 2, 7, 0, -1, -1, -1, -1, -1 },

    // 4: I-vi-IV-V (50s progression: C-Am-F-G)
    { 0, 9, 5, 7, -1, -1, -1, -1 },

    // 5: I-IV-I-V (Blues progression)
    { 0, 5, 0, 7, -1, -1, -1, -1 },

    // 6: vi-IV-I-V (Sensitive progression: Am-F-C-G)
    { 9, 5, 0, 7, -1, -1, -1, -1 },

    // 7: I-bVII-IV-I (Mixolydian: C-Bb-F-C)
    { 0, 10, 5, 0, -1, -1, -1, -1 },

    // 8: Ascending chromatic (C-C#-D-D#-E-F-F#-G)
    { 0, 1, 2, 3, 4, 5, 6, 7 },

    // 9: Circle of fifths (C-G-D-A-E-B-F#-C#)
    { 0, 7, 2, 9, 4, 11, 6, 1 },
};

// Human-readable progression names (for documentation/debugging)
constexpr const char* kProgressionNames[kProgressionCount] = {
    "Off",
    "I-IV-V-I (Pop/Rock)",
    "I-V-vi-IV (Pop)",
    "ii-V-I (Jazz)",
    "I-vi-IV-V (50s)",
    "I-IV-I-V (Blues)",
    "vi-IV-I-V (Sensitive)",
    "I-bVII-IV-I (Mix)",
    "Chromatic",
    "Circle of 5ths"
};

#endif // PROGRESSIONS_H
