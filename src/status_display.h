#pragma once

#include <Arduino.h>
#include <U8g2lib.h>

#include "crsf.h"
#include "gps_m10q.h"

class StatusDisplay {
 public:
  StatusDisplay();

  void begin();
  void update(const crsf::ReceiverStatus &receiver_status,
              const GpsFixData &gps_fix, uint32_t gps_baud,
              uint32_t gps_telemetry_frames_sent);
  bool available() const;

 private:
  bool probeAddress(uint8_t address) const;
  void draw(const crsf::ReceiverStatus &receiver_status,
            const GpsFixData &gps_fix, uint32_t gps_baud,
            uint32_t gps_telemetry_frames_sent);

  U8G2_SSD1306_128X64_NONAME_F_HW_I2C display_;
  bool available_;
  uint8_t address_;
  uint32_t last_update_ms_;
};
