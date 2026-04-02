#pragma once

#include <cstddef>
#include <cstdint>

#define COMPUTERCARD_NOIMPL
#include "ComputerCard.h"
#include "ConfigStore.h"
#include "GridsEngine.h"
#include "pico/critical_section.h"

class GridsCard : public ComputerCard {
 public:
  GridsCard();
  void Housekeeping();
  /** Expose board ID for USB device-vs-host port selection (see 20_reverb). */
  ComputerCard::HardwareVersion_t HardwareRevision() const { return HardwareVersion(); }

 protected:
  void ProcessSample() override;

 private:
  struct KnobLayerState {
    int32_t stored = 0;
    bool picked_up = true;
  };

  struct RuntimeParams {
    int32_t x = 0;
    int32_t y = 0;
    int32_t fill = 0;
  };

  static constexpr uint32_t kSampleRate = 48000;

  void TickUiAndSwitch();
  void HandleTapTempo();
  void TriggerOutputs(const GridsEngine::Outputs& out);
  void TickPulseTimers();
  int32_t ApplyPickup(Knob knob, KnobLayerState& state);
  void RefreshRuntimeParams();
  void HandleIncomingSysEx();
  void SendConfigSysEx();
  void ReceiveConfigSysEx(const uint8_t* payload, size_t len);
  void MarkConfigDirty();
  static void SanitizeConfig(ConfigStore::Data& cfg);
  uint16_t CurrentPulseSamples() const;
  bool ExternalClockActive();

  ConfigStore store_;
  ConfigStore::Data cfg_{};
  ConfigStore::Data pending_sysex_cfg_{};
  critical_section_t cfg_cs_{};
  bool pending_sysex_cfg_valid_ = false;
  GridsEngine engine_;

  RuntimeParams normal_params_{};
  RuntimeParams alt_params_{};
  bool alt_layer_ = false;
  KnobLayerState main_latch_{};
  KnobLayerState x_latch_{};
  KnobLayerState y_latch_{};

  uint32_t sample_count_ = 0;
  uint32_t samples_per_tick_ = 6000;
  uint32_t next_tick_at_ = 0;
  uint32_t last_tap_sample_ = 0;

  bool switch_down_ = false;
  uint32_t switch_down_start_ = 0;
  bool long_press_consumed_ = false;
  bool last_switch_changed_ = false;

  uint16_t pulse_1_countdown_ = 0;
  uint16_t pulse_2_countdown_ = 0;
  uint16_t cv1_pulse_countdown_ = 0;
  uint16_t cv2_pulse_countdown_ = 0;
  uint16_t beat_led_countdown_ = 0;

  uint64_t last_change_us_ = 0;
  bool pending_save_ = false;
};

