#pragma once
#include "config.h"
#include "relay.h"

void handleRoot() {
  String html = "<h1>Novia Home</h1>";
  for (int i = 0; i < JUMLAH_LAMPU; i++) {
    html += "<a href='/lampu" + String(i) + "'>Lampu " + String(i+1) + "</a><br>";
  }
  server.send(200, "text/html", html);
}

void initWebDashboard() {
  server.on("/", handleRoot);
  // server.on("/lampu0", dst...)
  server.begin();
}