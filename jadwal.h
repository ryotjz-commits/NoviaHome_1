#pragma once
#include "config.h"
#include "relay.h"

void cekJadwal() {
  if (!timeReady) return;
  if (millis() - lastSchedulerCheck < SCHEDULER_INTERVAL) return;
  lastSchedulerCheck = millis();

  // ambil jam sekarang dari NTP, contoh: menitSekarang = jam*60 + menit
  // if (jadwal[i].aktif && menitSekarang >= jadwal[i].waktuNyala ...) setRelay(i, true);
}