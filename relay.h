#pragma once
#include "config.h"

void initRelay() {
  for (int i = 0; i < JUMLAH_RELAY; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH); // relay mati
  }
}

void setRelay(int index, bool nyala) {
  if (index < 0 || index >= JUMLAH_RELAY) return;
  relayStatus[index] = nyala;
  digitalWrite(relayPins[index], nyala ? LOW : HIGH);
  preferences.putBool(("relay" + String(index)).c_str(), nyala);
}