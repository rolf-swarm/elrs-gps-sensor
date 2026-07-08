#pragma once

#include <Arduino.h>

namespace crsf {

static constexpr uint32_t BAUD = 420000;
static constexpr uint8_t ADDR_FLIGHT_CONTROLLER = 0xC8;
static constexpr uint8_t ADDR_RADIO_TRANSMITTER = 0xEA;
static constexpr uint8_t ADDR_CRSF_RECEIVER = 0xEC;
static constexpr uint8_t ADDR_CRSF_TRANSMITTER = 0xEE;
static constexpr uint8_t FRAMETYPE_GPS = 0x02;
static constexpr uint8_t FRAMETYPE_LINK_STATISTICS = 0x14;
static constexpr uint8_t FRAMETYPE_RC_CHANNELS_PACKED = 0x16;
static constexpr uint8_t FRAMETYPE_SUBSET_RC_CHANNELS_PACKED = 0x17;
static constexpr uint8_t FRAMETYPE_LINK_STATISTICS_RX = 0x1C;
static constexpr uint8_t FRAMETYPE_LINK_STATISTICS_TX = 0x1D;
static constexpr size_t GPS_FRAME_SIZE = 19;
static constexpr size_t MAX_FRAME_SIZE = 64;

struct GpsTelemetry {
  double latitude_deg;
  double longitude_deg;
  uint16_t ground_speed_kmh_x100;
  uint16_t heading_deg_x100;
  int16_t altitude_m;
  uint8_t satellites;
};

struct LinkStatistics {
  bool valid;
  uint32_t last_update_ms;
  int16_t uplink_rssi_ant1_dbm;
  int16_t uplink_rssi_ant2_dbm;
  uint8_t uplink_rssi_percent;
  bool uplink_rssi_percent_valid;
  uint8_t uplink_link_quality;
  int8_t uplink_snr_db;
  uint8_t active_antenna;
  uint8_t rf_profile;
  uint8_t uplink_power_raw;
  uint16_t uplink_power_mw;
  int16_t downlink_rssi_dbm;
  uint8_t downlink_rssi_percent;
  bool downlink_rssi_percent_valid;
  uint8_t downlink_link_quality;
  int8_t downlink_snr_db;
  uint16_t uplink_fps;
  bool uplink_fps_valid;
};

struct ReceiverStatus {
  LinkStatistics link;
  uint32_t last_frame_ms;
  uint32_t last_rc_frame_ms;
  uint32_t frames_ok;
  uint32_t crc_errors;
  uint32_t length_errors;
};

class Parser {
 public:
  Parser();

  bool processByte(uint8_t value, uint32_t now_ms);
  const ReceiverStatus &status() const;

 private:
  void resetFrame();
  void processFrame(uint32_t now_ms);

  uint8_t frame_[MAX_FRAME_SIZE];
  uint8_t position_;
  uint8_t expected_length_;
  uint32_t last_byte_ms_;
  ReceiverStatus status_;
};

uint8_t crc8(const uint8_t *data, size_t len);
uint16_t powerMilliwattsFromRaw(uint8_t raw_power);
size_t buildGpsFrame(const GpsTelemetry &gps, uint8_t *frame, size_t frame_len);
size_t writeGpsTelemetry(Print &port, const GpsTelemetry &gps);

}  // namespace crsf
