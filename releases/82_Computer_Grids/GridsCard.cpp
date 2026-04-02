#include "GridsCard.h"

#include <cstring>

#include "pico/time.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wextra"
#endif
#include "tusb.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

namespace {
constexpr uint8_t kSysExStart = 0xF0;
constexpr uint8_t kSysExEnd = 0xF7;
constexpr uint8_t kManufacturer = 0x7D;
constexpr uint8_t kDevice = 0x63;  // local card id
constexpr uint8_t kCmdGetConfig = 0x01;
constexpr uint8_t kCmdSetConfig = 0x03;
constexpr uint8_t kCmdSaveConfig = 0x04;
constexpr uint32_t kLongPressSamples = 24000;
constexpr uint32_t kPickupDeadband = 96;

size_t Encode7Bit(const uint8_t* raw, size_t raw_len, uint8_t* out, size_t out_max) {
  size_t out_idx = 0;
  for (size_t i = 0; i < raw_len; i += 7) {
    if (out_idx >= out_max) break;
    uint8_t msb = 0;
    uint8_t block[7] = {};
    size_t block_len = raw_len - i;
    if (block_len > 7) block_len = 7;
    for (size_t j = 0; j < block_len; ++j) {
      const uint8_t b = raw[i + j];
      if (b & 0x80) msb |= static_cast<uint8_t>(1u << j);
      block[j] = static_cast<uint8_t>(b & 0x7F);
    }
    out[out_idx++] = msb;
    for (size_t j = 0; j < block_len; ++j) {
      if (out_idx >= out_max) return out_idx;
      out[out_idx++] = block[j];
    }
  }
  return out_idx;
}

size_t Decode7Bit(const uint8_t* in, size_t in_len, uint8_t* raw, size_t raw_max) {
  size_t in_idx = 0;
  size_t raw_idx = 0;
  while (in_idx < in_len && raw_idx < raw_max) {
    const uint8_t msb = in[in_idx++];
    for (size_t j = 0; j < 7 && in_idx < in_len && raw_idx < raw_max; ++j) {
      uint8_t b = in[in_idx++];
      if (msb & (1u << j)) b |= 0x80;
      raw[raw_idx++] = b;
    }
  }
  return raw_idx;
}

template <typename T>
T Clamp(T value, T lo, T hi) {
  if (value < lo) return lo;
  if (value > hi) return hi;
  return value;
}
}  // namespace

GridsCard::GridsCard() {
  store_.Load(false);
  critical_section_init(&cfg_cs_);
  cfg_ = store_.Get();
  SanitizeConfig(cfg_);
  engine_.Seed(static_cast<uint32_t>(UniqueCardID()));
  samples_per_tick_ = (kSampleRate * 600U) / (cfg_.bpm10 * 4U);
  if (samples_per_tick_ == 0) samples_per_tick_ = 1;

  normal_params_.fill = 2048;
  alt_params_.fill = cfg_.chaos * 32;
  alt_params_.x = cfg_.bpm10;
  alt_params_.y = cfg_.swing * 16;
}

void GridsCard::ProcessSample() {
  if (pending_sysex_cfg_valid_) {
    critical_section_enter_blocking(&cfg_cs_);
    if (pending_sysex_cfg_valid_) {
      cfg_ = pending_sysex_cfg_;
      pending_sysex_cfg_valid_ = false;
    }
    critical_section_exit(&cfg_cs_);
    samples_per_tick_ = (kSampleRate * 600U) / (cfg_.bpm10 * 4U);
    if (samples_per_tick_ == 0) samples_per_tick_ = 1;
  }

  sample_count_++;
  TickUiAndSwitch();
  TickPulseTimers();

  if (PulseIn2RisingEdge()) {
    engine_.Reset();
    next_tick_at_ = sample_count_ + samples_per_tick_;
  }

  const bool ext_clock = ExternalClockActive();
  bool fire_tick = false;
  if (ext_clock) {
    fire_tick = PulseIn1RisingEdge();
  } else if (sample_count_ >= next_tick_at_) {
    fire_tick = true;
    next_tick_at_ += samples_per_tick_;
  }

  if (fire_tick) {
    RefreshRuntimeParams();
    const uint16_t x = static_cast<uint16_t>(normal_params_.x);
    const uint16_t y = static_cast<uint16_t>(normal_params_.y);
    const uint16_t fill = static_cast<uint16_t>(normal_params_.fill);
    const auto outputs = engine_.Tick(x, y, fill, cfg_.chaos);
    TriggerOutputs(outputs);
    beat_led_countdown_ = CurrentPulseSamples();
  }

  // Drive LEDs from audio core to avoid cross-core display glitches.
  LedOn(0, beat_led_countdown_ > 0);           // beat blink
  LedBrightness(1, static_cast<uint16_t>(normal_params_.fill));
  LedOn(2, pulse_1_countdown_ > 0);            // lane 1 activity
  LedBrightness(3, static_cast<uint16_t>(normal_params_.x));
  LedOn(4, pulse_2_countdown_ > 0);            // lane 2 activity
  LedOn(5, cv1_pulse_countdown_ > 0 || cv2_pulse_countdown_ > 0 || alt_layer_);

  if (beat_led_countdown_ > 0) beat_led_countdown_--;
}

void GridsCard::Housekeeping() {
  HandleTapTempo();
  HandleIncomingSysEx();

  bool should_save = false;
  ConfigStore::Data snapshot{};
  critical_section_enter_blocking(&cfg_cs_);
  if (pending_save_ && (time_us_64() - last_change_us_) > 1500000ULL) {
    snapshot = cfg_;
    pending_save_ = false;
    should_save = true;
  }
  critical_section_exit(&cfg_cs_);

  if (should_save) {
    store_.SaveData(snapshot);
  }
}

bool GridsCard::ExternalClockActive() {
  return Connected(Pulse1);
}

void GridsCard::TickUiAndSwitch() {
  const auto sw = SwitchVal();
  const bool down = (sw == Switch::Down);
  last_switch_changed_ = SwitchChanged();

  if (last_switch_changed_) {
    if (down) {
      switch_down_ = true;
      switch_down_start_ = sample_count_;
      long_press_consumed_ = false;
    } else {
      if (switch_down_ && !long_press_consumed_) {
        // Short press on release.
        if (!ExternalClockActive()) {
          const uint32_t now = sample_count_;
          const uint32_t dt = now - last_tap_sample_;
          if (last_tap_sample_ != 0 && dt > 2000 && dt < 48000 * 2) {
            const uint32_t bpm10 = (kSampleRate * 600U) / dt;
            if (bpm10 >= 400 && bpm10 <= 2600) {
              critical_section_enter_blocking(&cfg_cs_);
              cfg_.bpm10 = static_cast<uint16_t>(bpm10);
              critical_section_exit(&cfg_cs_);
              samples_per_tick_ = (kSampleRate * 600U) / (cfg_.bpm10 * 4U);
              MarkConfigDirty();
            }
          }
          last_tap_sample_ = now;
        }
      }
      switch_down_ = false;
    }
  }

  if (switch_down_ && !long_press_consumed_ && (sample_count_ - switch_down_start_) > kLongPressSamples) {
    alt_layer_ = !alt_layer_;
    long_press_consumed_ = true;
    main_latch_.picked_up = false;
    x_latch_.picked_up = false;
    y_latch_.picked_up = false;
  }
}

void GridsCard::HandleTapTempo() {}

int32_t GridsCard::ApplyPickup(Knob knob, KnobLayerState& state) {
  const int32_t raw = KnobVal(knob);
  if (state.picked_up) {
    state.stored = raw;
    return raw;
  }
  const int32_t delta = raw - state.stored;
  if (delta > -static_cast<int32_t>(kPickupDeadband) && delta < static_cast<int32_t>(kPickupDeadband)) {
    state.picked_up = true;
    state.stored = raw;
  }
  return state.stored;
}

void GridsCard::RefreshRuntimeParams() {
  RuntimeParams& active = alt_layer_ ? alt_params_ : normal_params_;
  active.fill = ApplyPickup(Knob::Main, main_latch_);
  active.x = ApplyPickup(Knob::X, x_latch_);
  active.y = ApplyPickup(Knob::Y, y_latch_);

  if (!alt_layer_) {
    int32_t x = active.x;
    int32_t y = active.y;
    int32_t fill = active.fill;

    if (Connected(CV1)) {
      const int32_t cv1 = (CVIn1() * cfg_.cv1_amount) / 64;
      if (cfg_.cv1_mode == ConfigStore::CV1ToX) x += cv1;
      if (cfg_.cv1_mode == ConfigStore::CV1ToY) y += cv1;
      if (cfg_.cv1_mode == ConfigStore::CV1ToBlend) {
        x += cv1 / 2;
        y += cv1 / 2;
      }
    }
    if (Connected(CV2)) {
      fill += (CVIn2() * cfg_.cv2_amount) / 64;
    }

    if (x < 0) x = 0;
    if (x > 4095) x = 4095;
    if (y < 0) y = 0;
    if (y > 4095) y = 4095;
    if (fill < 0) fill = 0;
    if (fill > 4095) fill = 4095;

    normal_params_.x = x;
    normal_params_.y = y;

    // Macro fill drives 3 lanes through preconfigured scaling.
    const int32_t laneBlend =
        ((fill * cfg_.lane1_fill_scale) + (fill * cfg_.lane2_fill_scale) + (fill * cfg_.lane3_fill_scale)) / 300;
    int32_t blended = laneBlend + cfg_.lane1_fill_offset + cfg_.lane2_fill_offset + cfg_.lane3_fill_offset;
    if (blended < 0) blended = 0;
    if (blended > 4095) blended = 4095;
    normal_params_.fill = blended;
  } else {
    // Alt layer edits advanced params.
    critical_section_enter_blocking(&cfg_cs_);
    cfg_.chaos = static_cast<uint8_t>(alt_params_.fill >> 5);
    if (cfg_.chaos > 127) cfg_.chaos = 127;
    cfg_.bpm10 = static_cast<uint16_t>(600 + (alt_params_.x * 2000) / 4095);
    cfg_.swing = static_cast<uint8_t>((alt_params_.y * 100) / 4095);
    critical_section_exit(&cfg_cs_);
    samples_per_tick_ = (kSampleRate * 600U) / (cfg_.bpm10 * 4U);
    if (samples_per_tick_ == 0) samples_per_tick_ = 1;
    MarkConfigDirty();
  }
}

uint16_t GridsCard::CurrentPulseSamples() const {
  const uint32_t ms = cfg_.pulse_ms ? cfg_.pulse_ms : 10;
  return static_cast<uint16_t>((kSampleRate * ms) / 1000);
}

void GridsCard::TriggerOutputs(const GridsEngine::Outputs& out) {
  const uint16_t len = CurrentPulseSamples();
  if (out.lane1) pulse_1_countdown_ = len;
  if (out.lane2) pulse_2_countdown_ = len;
  if (out.lane3) cv1_pulse_countdown_ = len;

  if (cfg_.aux_mode == ConfigStore::AuxAccent && out.accent) cv2_pulse_countdown_ = len;
  if (cfg_.aux_mode == ConfigStore::AuxClock) cv2_pulse_countdown_ = len;
  if (cfg_.aux_mode == ConfigStore::AuxLane3Mirror && out.lane3) cv2_pulse_countdown_ = len;
}

void GridsCard::TickPulseTimers() {
  const bool p1 = pulse_1_countdown_ > 0;
  const bool p2 = pulse_2_countdown_ > 0;
  PulseOut1(p1);
  PulseOut2(p2);
  CVOut1(cv1_pulse_countdown_ > 0 ? 2047 : -2048);
  CVOut2(cv2_pulse_countdown_ > 0 ? 2047 : -2048);

  if (pulse_1_countdown_ > 0) pulse_1_countdown_--;
  if (pulse_2_countdown_ > 0) pulse_2_countdown_--;
  if (cv1_pulse_countdown_ > 0) cv1_pulse_countdown_--;
  if (cv2_pulse_countdown_ > 0) cv2_pulse_countdown_--;
}

void GridsCard::HandleIncomingSysEx() {
  static uint8_t packet[256];
  while (tud_midi_available()) {
    const size_t len = tud_midi_stream_read(packet, sizeof(packet));
    if (len < 5 || packet[0] != kSysExStart || packet[len - 1] != kSysExEnd) continue;
    if (packet[1] != kManufacturer || packet[2] != kDevice) continue;
    const uint8_t cmd = packet[3];
    if (cmd == kCmdGetConfig) {
      SendConfigSysEx();
    } else if (cmd == kCmdSetConfig && len >= 6) {
      ReceiveConfigSysEx(&packet[4], len - 5);
    } else if (cmd == kCmdSaveConfig) {
      MarkConfigDirty();
    }
  }
}

void GridsCard::SendConfigSysEx() {
  ConfigStore::Data snapshot{};
  critical_section_enter_blocking(&cfg_cs_);
  snapshot = cfg_;
  critical_section_exit(&cfg_cs_);
  const uint8_t* raw = reinterpret_cast<const uint8_t*>(&snapshot);
  const size_t raw_len = sizeof(ConfigStore::Data);
  uint8_t msg[192] = {};
  size_t out = 0;
  msg[out++] = kSysExStart;
  msg[out++] = kManufacturer;
  msg[out++] = kDevice;
  msg[out++] = 0x02;
  out += Encode7Bit(raw, raw_len, &msg[out], sizeof(msg) - out - 1);
  msg[out++] = kSysExEnd;
  tud_midi_stream_write(0, msg, out);
}

void GridsCard::ReceiveConfigSysEx(const uint8_t* payload, size_t len) {
  if (len == 0) return;
  uint8_t decoded[sizeof(ConfigStore::Data)] = {};
  const size_t decoded_len = Decode7Bit(payload, len, decoded, sizeof(decoded));
  if (decoded_len != sizeof(ConfigStore::Data)) return;
  ConfigStore::Data incoming;
  std::memcpy(&incoming, decoded, sizeof(incoming));
  if (incoming.magic != ConfigStore::kMagic) return;
  SanitizeConfig(incoming);
  incoming.version = ConfigStore::kVersion;
  critical_section_enter_blocking(&cfg_cs_);
  pending_sysex_cfg_ = incoming;
  pending_sysex_cfg_valid_ = true;
  critical_section_exit(&cfg_cs_);
}

void GridsCard::MarkConfigDirty() {
  critical_section_enter_blocking(&cfg_cs_);
  pending_save_ = true;
  last_change_us_ = time_us_64();
  critical_section_exit(&cfg_cs_);
}

void GridsCard::SanitizeConfig(ConfigStore::Data& cfg) {
  cfg.magic = ConfigStore::kMagic;
  cfg.version = ConfigStore::kVersion;
  cfg.bpm10 = Clamp<uint16_t>(cfg.bpm10, 400, 2600);
  cfg.swing = Clamp<uint8_t>(cfg.swing, 0, 100);
  cfg.chaos = Clamp<uint8_t>(cfg.chaos, 0, 127);
  cfg.cv1_mode = (cfg.cv1_mode <= static_cast<uint8_t>(ConfigStore::CV1ToBlend))
                     ? cfg.cv1_mode
                     : static_cast<uint8_t>(ConfigStore::CV1ToBlend);
  cfg.cv2_mode = ConfigStore::CV2ToFill;
  cfg.cv1_amount = Clamp<int8_t>(cfg.cv1_amount, -127, 127);
  cfg.cv2_amount = Clamp<int8_t>(cfg.cv2_amount, -127, 127);
  cfg.lane1_fill_scale = Clamp<uint8_t>(cfg.lane1_fill_scale, 0, 200);
  cfg.lane2_fill_scale = Clamp<uint8_t>(cfg.lane2_fill_scale, 0, 200);
  cfg.lane3_fill_scale = Clamp<uint8_t>(cfg.lane3_fill_scale, 0, 200);
  cfg.lane1_fill_offset = Clamp<int8_t>(cfg.lane1_fill_offset, -127, 127);
  cfg.lane2_fill_offset = Clamp<int8_t>(cfg.lane2_fill_offset, -127, 127);
  cfg.lane3_fill_offset = Clamp<int8_t>(cfg.lane3_fill_offset, -127, 127);
  cfg.aux_mode = (cfg.aux_mode <= static_cast<uint8_t>(ConfigStore::AuxLane3Mirror))
                     ? cfg.aux_mode
                     : static_cast<uint8_t>(ConfigStore::AuxAccent);
  cfg.pulse_ms = Clamp<uint8_t>(cfg.pulse_ms, 1, 40);
  for (size_t i = 0; i < sizeof(cfg.reserved); ++i) cfg.reserved[i] = 0;
}

