#pragma once

#include <cstdint>

class GridsEngine {
 public:
  struct Outputs {
    bool lane1 = false;
    bool lane2 = false;
    bool lane3 = false;
    bool accent = false;
  };

  void Seed(uint32_t seed);
  void Reset();
  Outputs Tick(uint16_t x, uint16_t y, uint16_t fill, uint8_t chaos);

 private:
  uint8_t ReadDrumMap(uint8_t step, uint8_t instrument, uint8_t x, uint8_t y) const;
  uint8_t step_ = 0;
  uint32_t rng_ = 1;
  uint8_t part_perturbation_[3] = {};
};

