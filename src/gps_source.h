#pragma once

#include <Arduino.h>

#include "gps_m10q.h"

struct GpsFix {
  bool valid;
  double latitude_deg;
  double longitude_deg;
  uint16_t ground_speed_kmh_x100;
  uint16_t heading_deg_x100;
  int16_t altitude_m;
  uint8_t satellites;
};

class GpsSource {
 public:
  void begin();
  void update();
  bool read(GpsFix &fix);
  const GpsFixData &latest() const;
  uint32_t activeBaud() const;

 private:
  void startBaud(uint8_t baud_index);
  void maybeAdvanceBaud();

  GpsM10Q gps_;
  uint8_t active_baud_index_;
  uint32_t baud_started_ms_;
  uint32_t passed_checksum_at_baud_start_;
};
