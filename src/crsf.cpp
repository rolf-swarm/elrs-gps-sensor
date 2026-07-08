#include "crsf.h"

#include <math.h>

namespace crsf {
namespace {

static constexpr uint8_t CRC8_POLY = 0xD5;
static constexpr uint8_t GPS_LENGTH = 17;  // type + 15-byte payload + crc
static constexpr uint8_t GPS_CRC_INPUT_LEN = 16;  // type + 15-byte payload
static constexpr uint8_t MIN_FRAME_LENGTH = 2;  // type + crc
static constexpr uint32_t FRAME_TIMEOUT_MS = 10;

void putU8(uint8_t *buf, uint8_t &idx, uint8_t value) {
  buf[idx++] = value;
}

void putU16Be(uint8_t *buf, uint8_t &idx, uint16_t value) {
  buf[idx++] = static_cast<uint8_t>(value >> 8);
  buf[idx++] = static_cast<uint8_t>(value & 0xFF);
}

void putI32Be(uint8_t *buf, uint8_t &idx, int32_t value) {
  const uint32_t unsigned_value = static_cast<uint32_t>(value);
  buf[idx++] = static_cast<uint8_t>(unsigned_value >> 24);
  buf[idx++] = static_cast<uint8_t>(unsigned_value >> 16);
  buf[idx++] = static_cast<uint8_t>(unsigned_value >> 8);
  buf[idx++] = static_cast<uint8_t>(unsigned_value & 0xFF);
}

uint16_t encodeAltitude(int16_t altitude_m) {
  int32_t encoded = static_cast<int32_t>(altitude_m) + 1000;

  if (encoded < 0) {
    encoded = 0;
  } else if (encoded > 65535) {
    encoded = 65535;
  }

  return static_cast<uint16_t>(encoded);
}

bool isPlausibleAddress(uint8_t address) {
  return address == 0x00 || address == ADDR_FLIGHT_CONTROLLER ||
         address == ADDR_RADIO_TRANSMITTER || address == ADDR_CRSF_RECEIVER ||
         address == ADDR_CRSF_TRANSMITTER;
}

int16_t decodeNegativeDbm(uint8_t raw) {
  return static_cast<int16_t>(-static_cast<int16_t>(raw));
}

void clearLinkStatistics(LinkStatistics &link) {
  link.valid = false;
  link.last_update_ms = 0;
  link.uplink_rssi_ant1_dbm = 0;
  link.uplink_rssi_ant2_dbm = 0;
  link.uplink_rssi_percent = 0;
  link.uplink_rssi_percent_valid = false;
  link.uplink_link_quality = 0;
  link.uplink_snr_db = 0;
  link.active_antenna = 0;
  link.rf_profile = 0;
  link.uplink_power_raw = 0;
  link.uplink_power_mw = 0;
  link.downlink_rssi_dbm = 0;
  link.downlink_rssi_percent = 0;
  link.downlink_rssi_percent_valid = false;
  link.downlink_link_quality = 0;
  link.downlink_snr_db = 0;
  link.uplink_fps = 0;
  link.uplink_fps_valid = false;
}

void parseLinkStatisticsV2(LinkStatistics &link, const uint8_t *payload,
                           uint32_t now_ms) {
  link.valid = true;
  link.last_update_ms = now_ms;
  link.uplink_rssi_ant1_dbm = decodeNegativeDbm(payload[0]);
  link.uplink_rssi_ant2_dbm = decodeNegativeDbm(payload[1]);
  link.uplink_rssi_percent_valid = false;
  link.uplink_link_quality = payload[2];
  link.uplink_snr_db = static_cast<int8_t>(payload[3]);
  link.active_antenna = payload[4];
  link.rf_profile = payload[5];
  link.uplink_power_raw = payload[6];
  link.uplink_power_mw = powerMilliwattsFromRaw(payload[6]);
  link.downlink_rssi_dbm = decodeNegativeDbm(payload[7]);
  link.downlink_rssi_percent_valid = false;
  link.downlink_link_quality = payload[8];
  link.downlink_snr_db = static_cast<int8_t>(payload[9]);
  link.uplink_fps_valid = false;
}

void parseLinkStatisticsRx(LinkStatistics &link, const uint8_t *payload,
                           uint32_t now_ms) {
  link.valid = true;
  link.last_update_ms = now_ms;
  link.downlink_rssi_dbm = decodeNegativeDbm(payload[0]);
  link.downlink_rssi_percent = payload[1];
  link.downlink_rssi_percent_valid = true;
  link.downlink_link_quality = payload[2];
  link.downlink_snr_db = static_cast<int8_t>(payload[3]);
  link.uplink_power_raw = payload[4];
}

void parseLinkStatisticsTx(LinkStatistics &link, const uint8_t *payload,
                           uint32_t now_ms) {
  link.valid = true;
  link.last_update_ms = now_ms;
  link.uplink_rssi_ant1_dbm = decodeNegativeDbm(payload[0]);
  link.uplink_rssi_ant2_dbm = decodeNegativeDbm(payload[0]);
  link.uplink_rssi_percent = payload[1];
  link.uplink_rssi_percent_valid = true;
  link.uplink_link_quality = payload[2];
  link.uplink_snr_db = static_cast<int8_t>(payload[3]);
  link.uplink_fps = static_cast<uint16_t>(payload[5]) * 10U;
  link.uplink_fps_valid = true;
}

}  // namespace

uint8_t crc8(const uint8_t *data, size_t len) {
  uint8_t crc = 0;

  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];

    for (uint8_t bit = 0; bit < 8; bit++) {
      if ((crc & 0x80) != 0) {
        crc = static_cast<uint8_t>((crc << 1) ^ CRC8_POLY);
      } else {
        crc = static_cast<uint8_t>(crc << 1);
      }
    }
  }

  return crc;
}

uint16_t powerMilliwattsFromRaw(uint8_t raw_power) {
  static constexpr uint16_t POWER_LEVELS_MW[] = {
      0, 10, 25, 100, 500, 1000, 2000, 250, 50,
  };

  if (raw_power >= sizeof(POWER_LEVELS_MW) / sizeof(POWER_LEVELS_MW[0])) {
    return 0;
  }

  return POWER_LEVELS_MW[raw_power];
}

Parser::Parser()
    : position_(0),
      expected_length_(0),
      last_byte_ms_(0),
      status_() {
  clearLinkStatistics(status_.link);
  status_.last_frame_ms = 0;
  status_.last_rc_frame_ms = 0;
  status_.frames_ok = 0;
  status_.crc_errors = 0;
  status_.length_errors = 0;
}

bool Parser::processByte(uint8_t value, uint32_t now_ms) {
  if (position_ > 0 && now_ms - last_byte_ms_ > FRAME_TIMEOUT_MS) {
    resetFrame();
  }
  last_byte_ms_ = now_ms;

  if (position_ == 0) {
    if (!isPlausibleAddress(value)) {
      return false;
    }
    frame_[position_++] = value;
    return false;
  }

  if (position_ == 1) {
    if (value < MIN_FRAME_LENGTH ||
        value > (MAX_FRAME_SIZE - 2)) {
      status_.length_errors++;
      resetFrame();
      return false;
    }

    frame_[position_++] = value;
    expected_length_ = static_cast<uint8_t>(value + 2);
    return false;
  }

  frame_[position_++] = value;
  if (position_ < expected_length_) {
    return false;
  }

  const uint8_t frame_length = frame_[1];
  const uint8_t expected_crc = frame_[expected_length_ - 1];
  const uint8_t actual_crc = crc8(&frame_[2], frame_length - 1);

  if (actual_crc != expected_crc) {
    status_.crc_errors++;
    resetFrame();
    return false;
  }

  processFrame(now_ms);
  resetFrame();
  return true;
}

const ReceiverStatus &Parser::status() const {
  return status_;
}

void Parser::resetFrame() {
  position_ = 0;
  expected_length_ = 0;
}

void Parser::processFrame(uint32_t now_ms) {
  const uint8_t type = frame_[2];
  const uint8_t payload_length = static_cast<uint8_t>(frame_[1] - 2);
  const uint8_t *payload = &frame_[3];

  status_.frames_ok++;
  status_.last_frame_ms = now_ms;

  switch (type) {
    case FRAMETYPE_RC_CHANNELS_PACKED:
    case FRAMETYPE_SUBSET_RC_CHANNELS_PACKED:
      status_.last_rc_frame_ms = now_ms;
      break;

    case FRAMETYPE_LINK_STATISTICS:
      if (payload_length >= 10) {
        parseLinkStatisticsV2(status_.link, payload, now_ms);
      }
      break;

    case FRAMETYPE_LINK_STATISTICS_RX:
      if (payload_length >= 5) {
        parseLinkStatisticsRx(status_.link, payload, now_ms);
      }
      break;

    case FRAMETYPE_LINK_STATISTICS_TX:
      if (payload_length >= 6) {
        parseLinkStatisticsTx(status_.link, payload, now_ms);
      }
      break;

    default:
      break;
  }
}

size_t buildGpsFrame(const GpsTelemetry &gps, uint8_t *frame, size_t frame_len) {
  if (frame == nullptr || frame_len < GPS_FRAME_SIZE) {
    return 0;
  }

  if (!isfinite(gps.latitude_deg) || !isfinite(gps.longitude_deg)) {
    return 0;
  }

  const int32_t latitude =
      static_cast<int32_t>(llround(gps.latitude_deg * 10000000.0));
  const int32_t longitude =
      static_cast<int32_t>(llround(gps.longitude_deg * 10000000.0));

  uint8_t idx = 0;
  putU8(frame, idx, ADDR_FLIGHT_CONTROLLER);
  putU8(frame, idx, GPS_LENGTH);
  putU8(frame, idx, FRAMETYPE_GPS);
  putI32Be(frame, idx, latitude);
  putI32Be(frame, idx, longitude);
  putU16Be(frame, idx, gps.ground_speed_kmh_x100);
  putU16Be(frame, idx, gps.heading_deg_x100);
  putU16Be(frame, idx, encodeAltitude(gps.altitude_m));
  putU8(frame, idx, gps.satellites);

  frame[idx++] = crc8(&frame[2], GPS_CRC_INPUT_LEN);
  return idx;
}

size_t writeGpsTelemetry(Print &port, const GpsTelemetry &gps) {
  uint8_t frame[GPS_FRAME_SIZE];
  const size_t frame_len = buildGpsFrame(gps, frame, sizeof(frame));

  if (frame_len == 0) {
    return 0;
  }

  return port.write(frame, frame_len);
}

}  // namespace crsf
