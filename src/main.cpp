#include <Arduino.h>

#include "board_config.h"
#include "crsf.h"
#include "gps_source.h"
#include "status_display.h"

namespace {

static constexpr uint32_t USB_DEBUG_BAUD = 115200;
static constexpr uint32_t GPS_PERIOD_MS = 1000;

GpsSource gps_source;
crsf::Parser crsf_parser;
StatusDisplay status_display;
uint32_t last_gps_ms = 0;
uint32_t gps_telemetry_frames_sent = 0;

void readIncomingCrsf() {
  const uint32_t now_ms = millis();
  HardwareSerial &crsf_port = board::crsfSerial();
  while (crsf_port.available() > 0) {
    const int incoming = crsf_port.read();
    if (incoming >= 0) {
      crsf_parser.processByte(static_cast<uint8_t>(incoming), now_ms);
    }
  }
}

void printSentGps(const GpsFix &fix, size_t bytes_written) {
  Serial.print(F("sent GPS "));
  if (fix.valid) {
    Serial.print(fix.latitude_deg, 7);
    Serial.print(F(", "));
    Serial.print(fix.longitude_deg, 7);
  } else {
    Serial.print(F("placeholder"));
  }
  Serial.print(F(" sats="));
  Serial.print(fix.satellites);
  Serial.print(F(" bytes="));
  Serial.println(bytes_written);
}

void printGpsWaitStatus(const GpsFixData &fix) {
  Serial.print(F("waiting for GPS fix valid="));
  Serial.print(fix.location_valid ? 1 : 0);
  Serial.print(F(" baud="));
  Serial.print(gps_source.activeBaud());
  Serial.print(F(" rx_bytes="));
  Serial.print(fix.rx_bytes);
  Serial.print(F(" cksum_ok="));
  Serial.print(fix.passed_checksum);
  Serial.print(F(" cksum_bad="));
  Serial.print(fix.failed_checksum);
  Serial.print(F(" sats="));
  Serial.print(fix.satellites);
  Serial.print(F(" sats_view="));
  if (fix.satellites_in_view_valid) {
    Serial.print(fix.satellites_in_view);
  } else {
    Serial.print(F("n/a"));
  }
  Serial.print(F(" age_ms="));
  Serial.println(fix.location_age_ms);
}

void maybeSendGpsTelemetry() {
  const uint32_t now_ms = millis();
  if (now_ms - last_gps_ms < GPS_PERIOD_MS) {
    return;
  }

  last_gps_ms = now_ms;

  GpsFix fix;
  gps_source.read(fix);
  if (!fix.valid) {
    printGpsWaitStatus(gps_source.latest());
  }

  crsf::GpsTelemetry telemetry = {
      0.0,
      0.0,
      0,
      0,
      0,
      fix.satellites,
  };

  if (fix.valid) {
    telemetry.latitude_deg = fix.latitude_deg;
    telemetry.longitude_deg = fix.longitude_deg;
    telemetry.ground_speed_kmh_x100 = fix.ground_speed_kmh_x100;
    telemetry.heading_deg_x100 = fix.heading_deg_x100;
    telemetry.altitude_m = fix.altitude_m;
  }

  gps_telemetry_frames_sent++;
  const size_t bytes_written =
      crsf::writeGpsTelemetry(board::crsfSerial(), telemetry);
  printSentGps(fix, bytes_written);
}

}  // namespace

void setup() {
  Serial.begin(USB_DEBUG_BAUD);
  board::beginCrsfSerial(crsf::BAUD);

  gps_source.begin();
  status_display.begin();

  delay(1000);
  Serial.println(F("CRSF GPS telemetry real-GPS autobaud test started"));
  Serial.print(F("Build "));
  Serial.print(F(__DATE__));
  Serial.print(F(" "));
  Serial.println(F(__TIME__));
  Serial.println(F("No fixed-coordinate fallback is compiled in"));
  board::printPinSummary(Serial);
  Serial.println(F("GPS baud scan: 9600, 38400, 57600, 115200"));
}

void loop() {
  gps_source.update();
  readIncomingCrsf();
  maybeSendGpsTelemetry();
  status_display.update(crsf_parser.status(), gps_source.latest(),
                        gps_source.activeBaud(), gps_telemetry_frames_sent);
}
