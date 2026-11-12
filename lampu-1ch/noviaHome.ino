#include "config.h"
#include "relay.h"
#include "jadwal.h"
#include "dashboard_web.h"
#include "dashboard_blynk.h"
#include "wifi_waktu.h"

void setup() {
  Serial.begin(115200);
  initRelay();
  initWiFi();
  initWaktu();
  initWebDashboard();
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
}

void loop() {
  Blynk.run();
  server.handleClient();
  cekJadwal();
}