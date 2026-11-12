#pragma once
// ================= WIFI + WAKTU - NOVIA HOME V3 =================
// File ini khusus ngurus WiFi + NTP. Biar noviaHome1.ino makin bersih.
#include "config.h"

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  Serial.print("Connecting WiFi");
  // Jangan pakai while nunggu di sini, biar Blynk & Web tetap jalan.
  // Nanti reconnect di loop lewat cekKoneksi().
}

void initWaktu() {
  configTime(GMT_OFFSET, DAYLIGHT_OFFSET, "pool.ntp.org", "time.google.com", "time.ntp.org");
  Serial.println("NTP WIB Started");
}

void cekKoneksi() {
  unsigned long now = millis();
  // WIFI RECONNECT
  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastWiFiCheck >= WIFI_INTERVAL) {
      lastWiFiCheck = now;
      WiFi.reconnect();
      Serial.println("Reconnecting WiFi...");
    }
    return; // Kalau WiFi putus, jangan maksa connect Blynk
  }
  // BLYNK RECONNECT
  if (!Blynk.connected()) {
    if (now - lastBlynkCheck >= BLYNK_INTERVAL) {
      lastBlynkCheck = now;
      Serial.println("Connecting Blynk...");
      Blynk.connect(1000);
    }
  }
}
