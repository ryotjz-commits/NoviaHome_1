#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <BlynkSimpleEsp32.h>

#define BLYNK_TEMPLATE_ID "TMPLxxxx"
#define BLYNK_TEMPLATE_NAME "NoviaHome"
#define BLYNK_AUTH_TOKEN "token_lu"

char ssid[9] = "nama wifi";
char pass[13] = "pass wifi";

WebServer server(80);
Preferences preferences;

const long GMT_OFFSET = 25200;
const int DAYLIGHT_OFFSET = 0;
const unsigned long SCHEDULER_INTERVAL = 30000;
unsigned long lastSchedulerCheck = 0;
bool timeReady = false;

const uint8_t JUMLAH_RELAY = 4;
const uint8_t JUMLAH_LAMPU = 4;
const uint8_t relayPins[4] = {16, 17, 18, 19};
bool relayStatus[4] = {false, false, false, false};

struct JadwalLampu {
  uint16_t waktuNyala;
  uint16_t waktuMati;
  bool aktif;
};
JadwalLampu jadwal[4];