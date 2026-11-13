/*
  =========================================================
  NOVIA HOME V3 - ESP32 DEVKIT V1
  Async Web Server + AP Offline + WiFi STA + Blynk + Save State

  Board       : ESP32 Dev Module / ESP32 DevKit V1
  Framework   : Arduino ESP32
  Required libraries:
    - Blynk
    - ESPAsyncWebServer
    - AsyncTCP
    - Preferences (included with ESP32 Arduino core)

  NOTE:
    Replace BLYNK_TEMPLATE_ID, BLYNK_TEMPLATE_NAME and
    BLYNK_AUTH_TOKEN with values from your Blynk project.
*/

#define BLYNK_TEMPLATE_ID   "TMPL6Yk8Hycr-"
#define BLYNK_TEMPLATE_NAME "Novia Home"
#define BLYNK_AUTH_TOKEN    "uUhUdnlB22cqJfKinHQu8yGMXLRbgzf1"
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <BlynkSimpleEsp32.h>
#include <Preferences.h>

// -------------------- WIFI --------------------

const char WIFI_SSID[] = "Tenda";
const char WIFI_PASSWORD[] = "Noke0710";

// AP tetap aktif walaupun router/internet tidak tersedia.
const char AP_SSID[] = "NoviaHome-AP";
const char AP_PASSWORD[] = "12345678"; // Minimal 8 karakter

// -------------------- RELAY --------------------

static const uint8_t RELAY_COUNT = 4;

const uint8_t relayPins[RELAY_COUNT] = {
  26, // Lampu Luar
  27, // Lampu Tengah
  14, // Lampu Kamar
  13  // Kipas Angin
};

// Relay aktif LOW: LOW = ON, HIGH = OFF.
bool relayStatus[RELAY_COUNT] = { false, false, false, false };

// -------------------- OBJECTS --------------------

AsyncWebServer server(80);
Preferences prefs;

// Blynk virtual pins V0, V1, V2, V3.
const uint8_t blynkPins[RELAY_COUNT] = { V0, V1, V2, V3 };

// Bit yang perlu disinkronkan ke Blynk.
// Ditulis oleh handler web, diproses dari loop().
volatile uint8_t pendingBlynkMask = 0;

// -------------------- TIMERS --------------------

const unsigned long WIFI_RETRY_INTERVAL  = 15000UL;
const unsigned long BLYNK_RETRY_INTERVAL = 10000UL;

unsigned long lastWiFiCheck = 0;
unsigned long lastBlynkCheck = 0;

// =========================================================
//                    RELAY + PREFERENCES
// =========================================================

void saveRelayState(uint8_t channel) {
  if (channel >= RELAY_COUNT) return;

  char key[4];
  snprintf(key, sizeof(key), "r%u", channel);
  prefs.putBool(key, relayStatus[channel]);
}

void loadRelayStates() {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    char key[4];
    snprintf(key, sizeof(key), "r%u", i);

    // Default OFF jika belum pernah tersimpan.
    relayStatus[i] = prefs.getBool(key, false);
    digitalWrite(relayPins[i], relayStatus[i] ? LOW : HIGH);
  }
}

void setRelay(uint8_t channel, bool turnOn, bool syncBlynk) {
  if (channel >= RELAY_COUNT) return;

  // Hindari penulisan flash jika status tidak berubah.
  if (relayStatus[channel] == turnOn) return;

  relayStatus[channel] = turnOn;
  digitalWrite(relayPins[channel], turnOn ? LOW : HIGH);
  saveRelayState(channel);

  // Jangan panggil Blynk.virtualWrite dari task AsyncWebServer.
  // Sinkronisasi dilakukan aman dari loop().
  if (syncBlynk) {
    pendingBlynkMask |= (uint8_t)(1U << channel);
  }
}

void setAllRelays(bool turnOn) {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    setRelay(i, turnOn, true);
  }
}

// =========================================================
//                         BLYNK
// =========================================================

BLYNK_WRITE(V0) {
  setRelay(0, param.asInt() != 0, false);
}

BLYNK_WRITE(V1) {
  setRelay(1, param.asInt() != 0, false);
}

BLYNK_WRITE(V2) {
  setRelay(2, param.asInt() != 0, false);
}

BLYNK_WRITE(V3) {
  setRelay(3, param.asInt() != 0, false);
}

BLYNK_CONNECTED() {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    Blynk.virtualWrite(blynkPins[i], relayStatus[i] ? 1 : 0);
  }
  Serial.println(F("[BLYNK] Connected; state synchronized."));
}

void processPendingBlynkWrites() {
  if (!Blynk.connected()) return;

  uint8_t mask = pendingBlynkMask;
  if (mask == 0) return;

  // Kosongkan bit yang akan diproses.
  pendingBlynkMask &= (uint8_t)(~mask);

  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    if (mask & (uint8_t)(1U << i)) {
      Blynk.virtualWrite(blynkPins[i], relayStatus[i] ? 1 : 0);
    }
  }
}

// =========================================================
//                     WEB PAGE HELPERS
// =========================================================

String deviceCard(const char *name, const char *symbol, uint8_t channel) {
  const bool isOn = relayStatus[channel];

  String html;
  html.reserve(600);

  html += F("<div class='device' id='device");
  html += String(channel);
  html += F("'><div class='device-top'><div class='device-icon'>");
  html += symbol;
  html += F("</div><span class='badge ");
  html += isOn ? F("active") : F("inactive");
  html += F("' id='badge");
  html += String(channel);
  html += F("'>");
  html += isOn ? F("ON") : F("OFF");
  html += F("</span></div><div class='device-name'>");
  html += name;
  html += F("</div><div class='device-desc' id='desc");
  html += String(channel);
  html += F("'>");
  html += isOn ? F("Perangkat aktif") : F("Perangkat tidak aktif");
  html += F("</div><button class='control ");
  html += isOn ? F("btn-off") : F("btn-on");
  html += F("' id='btn");
  html += String(channel);
  html += F("' data-ch='");
  html += String(channel);
  html += F("'>");
  html += isOn ? F("MATIKAN") : F("NYALAKAN");
  html += F("</button></div>");

  return html;
}

// =========================================================
//                         WEB UI
// =========================================================

String halamanWeb() {
  String html;
  html.reserve(9000);

  html += R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="theme-color" content="#151613">
<title>Novia Home</title>
<style>
:root{--bg:#151613;--surface:#20211d;--surface2:#292a24;--text:#f2f0e9;--muted:#92938a;--gold:#c5a875;--green:#9bbd91;--red:#d58c83;--border:rgba(255,255,255,.07)}
*{box-sizing:border-box}
body{margin:0;padding:25px 17px 35px;background:var(--bg);color:var(--text);font-family:Arial,sans-serif;font-size:13px;line-height:1.6}
.container{max-width:460px;margin:auto}
.header{display:flex;justify-content:space-between;align-items:center;margin-bottom:28px}
.brand{display:flex;align-items:center;gap:11px}
.brand-icon{width:43px;height:43px;display:grid;place-items:center;border-radius:13px;background:#29271f;color:var(--gold);font-size:22px}
.brand h1{margin:0;font-size:17px;font-weight:600;letter-spacing:-.4px}
.brand p{margin:2px 0 0;color:var(--muted);font-size:10px;letter-spacing:1px;text-transform:uppercase}
.connection{display:flex;align-items:center;gap:7px;font-size:9px;color:var(--muted)}
.dot{width:7px;height:7px;border-radius:50%;background:var(--red)}
.dot.online{background:var(--green);box-shadow:0 0 8px rgba(155,189,145,.5)}
.hero{padding:24px;border:1px solid var(--border);border-radius:18px;background:linear-gradient(135deg,#302b20,#22231e);margin-bottom:28px}
.hero-label{color:var(--gold);font-size:10px;letter-spacing:1.7px}
.hero h2{margin:9px 0 3px;font-size:23px;font-weight:500;letter-spacing:-.8px}
.hero p{color:#aaa99f;margin:0;font-size:11px}
.section-title{display:flex;justify-content:space-between;align-items:center;margin-bottom:14px}
.section-title h3{margin:0;font-size:12px;font-weight:600;letter-spacing:.8px;text-transform:uppercase}
.section-title span{color:var(--muted);font-size:10px}
.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px}
.device{min-width:0;padding:16px;border-radius:15px;background:var(--surface);border:1px solid var(--border)}
.device-top{display:flex;align-items:center;justify-content:space-between;margin-bottom:19px}
.device-icon{width:39px;height:39px;display:grid;place-items:center;border-radius:11px;background:var(--surface2);color:var(--gold);font-size:19px}
.badge{padding:4px 9px;border-radius:20px;font-size:9px;letter-spacing:.6px}
.active{background:rgba(155,189,145,.13);color:var(--green)}
.inactive{background:rgba(213,140,131,.12);color:var(--red)}
.device-name{font-size:13px;font-weight:600}
.device-desc{min-height:18px;margin:3px 0 15px;color:var(--muted);font-size:10px}
.control{display:block;width:100%;padding:10px 5px;border:0;border-radius:9px;font-size:10px;font-weight:600;letter-spacing:.7px;cursor:pointer;transition:.2s}
.btn-on{background:var(--gold);color:#201d16}
.btn-off{background:#35302a;color:#e0a39a}
.control:active,.master button:active{transform:scale(.97)}
button:disabled{opacity:.55;cursor:wait}
.master{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:23px}
.master button{padding:13px 5px;border:0;border-radius:10px;font-size:10px;font-weight:600;letter-spacing:.5px;cursor:pointer}
.all-on{background:var(--gold);color:#201d16}
.all-off{background:var(--surface2);color:var(--text);border:1px solid var(--border)!important}
.footer{margin-top:32px;padding-top:17px;border-top:1px solid var(--border);color:#77786f;font-size:10px;text-align:center;letter-spacing:.6px}
@media(max-width:350px){body{padding:18px 12px}.device{padding:12px}.grid{gap:8px}}
</style>
</head>
<body>
<div class="container">
<div class="header">
<div class="brand"><div class="brand-icon">⌂</div><div><h1>Novia Home</h1><p>Smart Living System</p></div></div>
<div class="connection"><span class="dot" id="statusDot"></span><span id="statusText">MEMERIKSA</span></div>
</div>
<div class="hero"><div class="hero-label">HOME AUTOMATION</div><h2>Kontrol Rumah</h2><p>Kelola perangkat rumah dalam satu tempat.</p></div>
<div class="section-title"><h3>Perangkat Saya</h3><span>04 Perangkat</span></div>
<div class="grid">
)rawliteral";

  html += deviceCard("Lampu Luar", "💡", 0);
  html += deviceCard("Lampu Tengah", "✦", 1);
  html += deviceCard("Lampu Kamar", "⌂", 2);
  html += deviceCard("Kipas Angin", "❋", 3);

  html += R"rawliteral(
</div>
<div class="master">
<button class="all-on" data-all="on">NYALAKAN SEMUA</button>
<button class="all-off" data-all="off">MATIKAN SEMUA</button>
</div>
<div class="footer">NOVIA HOME · ESP32 SMART SYSTEM V3</div>
</div>
<script>
async function updateStatus(){
  try{
    const response=await fetch('/state',{cache:'no-store'});
    if(!response.ok)throw new Error('HTTP '+response.status);
    const data=await response.json();
    for(let i=0;i<4;i++){
      const on=data.relay[i]===1;
      const badge=document.getElementById('badge'+i);
      const desc=document.getElementById('desc'+i);
      const btn=document.getElementById('btn'+i);
      if(!badge||!desc||!btn)continue;
      badge.textContent=on?'ON':'OFF';
      badge.className='badge '+(on?'active':'inactive');
      desc.textContent=on?'Perangkat aktif':'Perangkat tidak aktif';
      btn.textContent=on?'MATIKAN':'NYALAKAN';
      btn.className='control '+(on?'btn-off':'btn-on');
    }
    document.getElementById('statusDot').classList.toggle('online',data.blynk);
    document.getElementById('statusText').textContent=
      data.blynk?'BLYNK ONLINE':data.wifi?'LOKAL / BLYNK OFFLINE':'WIFI OFFLINE - AP AKTIF';
  }catch(error){
    document.getElementById('statusText').textContent='KONEKSI TERPUTUS';
  }
}
document.addEventListener('click',async function(e){
  const btn=e.target.closest('button');
  if(!btn)return;
  let url='';
  if(btn.dataset.ch!==undefined)url='/toggle?ch='+encodeURIComponent(btn.dataset.ch);
  else if(btn.dataset.all)url='/all-'+btn.dataset.all;
  else return;
  document.querySelectorAll('button').forEach(b=>b.disabled=true);
  try{
    const response=await fetch(url,{cache:'no-store'});
    if(!response.ok)throw new Error('HTTP '+response.status);
    await updateStatus();
  }catch(error){
    document.getElementById('statusText').textContent='PERINTAH GAGAL';
  }finally{
    document.querySelectorAll('button').forEach(b=>b.disabled=false);
  }
});
updateStatus();
setInterval(updateStatus,2000);
</script>
</body>
</html>
)rawliteral";

  return html;
}

// =========================================================
//                      WEB SERVER ROUTES
// =========================================================

void setupRoutes() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html; charset=utf-8", halamanWeb());
  });

  server.on("/state", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json;
    json.reserve(120);

    json = F("{\"relay\":[");
    for (uint8_t i = 0; i < RELAY_COUNT; i++) {
      json += relayStatus[i] ? '1' : '0';
      if (i < RELAY_COUNT - 1) json += ',';
    }

    json += F("],\"blynk\":");
    json += Blynk.connected() ? F("true") : F("false");
    json += F(",\"wifi\":");
    json += (WiFi.status() == WL_CONNECTED) ? F("true") : F("false");
    json += F(",\"ap\":true}");

    AsyncWebServerResponse *response =
      request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });

  server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("ch")) {
      request->send(400, "text/plain", "Parameter ch diperlukan");
      return;
    }

    const String value = request->getParam("ch")->value();
    const int channel = value.toInt();

    // Tolak parameter selain angka channel 0..3.
    if (channel < 0 || channel >= RELAY_COUNT ||
        value.length() != 1 || value[0] < '0' || value[0] > '3') {
      request->send(400, "text/plain", "Channel tidak valid");
      return;
    }

    setRelay((uint8_t)channel, !relayStatus[channel], true);
    request->send(200, "text/plain", "OK");
  });

  server.on("/all-on", HTTP_GET, [](AsyncWebServerRequest *request) {
    setAllRelays(true);
    request->send(200, "text/plain", "OK");
  });

  server.on("/all-off", HTTP_GET, [](AsyncWebServerRequest *request) {
    setAllRelays(false);
    request->send(200, "text/plain", "OK");
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not Found");
  });

  server.begin();
  Serial.println(F("[WEB] Async Web Server started on port 80."));
}

// =========================================================
//                           SETUP
// =========================================================

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println(F("================================"));
  Serial.println(F("       NOVIA HOME V3"));
  Serial.println(F("================================"));

  // Set semua relay OFF secepat mungkin saat boot.
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);
  }

  // Buka Preferences satu kali selama perangkat berjalan.
  if (!prefs.begin("novia", false)) {
    Serial.println(F("[NVS] Gagal membuka Preferences."));
  } else {
    loadRelayStates();
    Serial.println(F("[NVS] Relay state berhasil dimuat."));
  }

  // AP aktif terus, baik WiFi router tersedia maupun tidak.
  WiFi.mode(WIFI_AP_STA);
  WiFi.setAutoReconnect(true);

  if (WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Serial.print(F("[AP] SSID: "));
    Serial.println(AP_SSID);
    Serial.print(F("[AP] IP: "));
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println(F("[AP] Gagal memulai Access Point."));
  }

  // Mulai koneksi router tanpa menahan setup selama 10 detik.
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print(F("[WIFI] Menghubungkan ke router"));

  // Blynk dikonfigurasi; koneksi dicoba berkala dari loop().
  Blynk.config(BLYNK_AUTH_TOKEN);

  // Web server lokal langsung tersedia walaupun internet mati.
  setupRoutes();

  Serial.println();
  Serial.println(F("[SYSTEM] NOVIA HOME V3 siap."));
  Serial.print(F("[SYSTEM] AP URL: http://"));
  Serial.println(WiFi.softAPIP());
}

// =========================================================
//                            LOOP
// =========================================================

void loop() {
  // Blynk.run() hanya dijalankan di loop utama.
  if (Blynk.connected()) {
    Blynk.run();
  }

  processPendingBlynkWrites();

  const unsigned long now = millis();

  // Coba sambungkan kembali ke router secara berkala.
  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastWiFiCheck >= WIFI_RETRY_INTERVAL) {
      lastWiFiCheck = now;
      Serial.println(F("[WIFI] Mencoba koneksi ulang..."));
      WiFi.reconnect();
    }
  } else {
    // Coba Blynk hanya jika WiFi sudah tersambung.
    if (!Blynk.connected() &&
        now - lastBlynkCheck >= BLYNK_RETRY_INTERVAL) {
      lastBlynkCheck = now;
      Serial.println(F("[BLYNK] Mencoba koneksi..."));
      Blynk.connect(1000);
    }
  }

  delay(2);
}
