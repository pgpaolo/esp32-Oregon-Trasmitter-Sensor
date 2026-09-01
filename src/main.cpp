#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Update.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include "app_config.h"
#include "oregon_tx.h"

namespace {

struct AppConfig {
    String nodeName{"oregon-tx-beta"};
    String wifiSsid;
    String wifiPassword;
    bool rfEnabled{true};
    bool rfInvert{false};
    uint16_t halfUs{OSV3_HALF_US_DEFAULT};

    bool thEnabled{true};
    uint8_t thChannel{1};
    uint8_t thRolling{0x42};
    float tempOffset{0.0f};
    float humOffset{0.0f};
    uint32_t thIntervalMs{48000};

    bool uvEnabled{true};
    uint8_t uvChannel{1};
    uint8_t uvRolling{0x63};
    float uvMultiplier{1.0f};
    float uvOffset{0.0f};
    uint32_t uvIntervalMs{60000};

    uint8_t repeats{2};
    uint16_t repeatGapMs{120};
};

struct LiveData {
    float temperatureC{NAN};
    float humidityPct{NAN};
    float pressureHpa{NAN};
    float uvIndex{NAN};
    bool bmeOk{false};
    uint32_t lastReadMs{0};
    uint32_t thTxCount{0};
    uint32_t uvTxCount{0};
    String lastThFrame;
    String lastUvFrame;
};

Preferences prefs;
WebServer server(80);
Adafruit_BME280 bme;
OregonTx radio(OREGON_TX_PIN);
AppConfig cfg;
LiveData live;
uint32_t lastThTx = 0;
uint32_t lastUvTx = 0;

String esc(const String &s) {
    String o = s;
    o.replace("&", "&amp;"); o.replace("<", "&lt;"); o.replace(">", "&gt;"); o.replace("\"", "&quot;");
    return o;
}

void saveConfig() {
    prefs.begin("oregon-tx", false);
    prefs.putString("node", cfg.nodeName);
    prefs.putString("ssid", cfg.wifiSsid);
    prefs.putString("pass", cfg.wifiPassword);
    prefs.putBool("rf_en", cfg.rfEnabled);
    prefs.putBool("rf_inv", cfg.rfInvert);
    prefs.putUShort("half", cfg.halfUs);
    prefs.putBool("th_en", cfg.thEnabled);
    prefs.putUChar("th_ch", cfg.thChannel);
    prefs.putUChar("th_id", cfg.thRolling);
    prefs.putFloat("t_off", cfg.tempOffset);
    prefs.putFloat("h_off", cfg.humOffset);
    prefs.putULong("th_ms", cfg.thIntervalMs);
    prefs.putBool("uv_en", cfg.uvEnabled);
    prefs.putUChar("uv_ch", cfg.uvChannel);
    prefs.putUChar("uv_id", cfg.uvRolling);
    prefs.putFloat("uv_mul", cfg.uvMultiplier);
    prefs.putFloat("uv_off", cfg.uvOffset);
    prefs.putULong("uv_ms", cfg.uvIntervalMs);
    prefs.putUChar("reps", cfg.repeats);
    prefs.putUShort("gap", cfg.repeatGapMs);
    prefs.end();
}

void loadConfig() {
    prefs.begin("oregon-tx", true);
    cfg.nodeName = prefs.getString("node", cfg.nodeName);
    cfg.wifiSsid = prefs.getString("ssid", "");
    cfg.wifiPassword = prefs.getString("pass", "");
    cfg.rfEnabled = prefs.getBool("rf_en", cfg.rfEnabled);
    cfg.rfInvert = prefs.getBool("rf_inv", cfg.rfInvert);
    cfg.halfUs = prefs.getUShort("half", cfg.halfUs);
    cfg.thEnabled = prefs.getBool("th_en", cfg.thEnabled);
    cfg.thChannel = prefs.getUChar("th_ch", cfg.thChannel);
    cfg.thRolling = prefs.getUChar("th_id", cfg.thRolling);
    cfg.tempOffset = prefs.getFloat("t_off", cfg.tempOffset);
    cfg.humOffset = prefs.getFloat("h_off", cfg.humOffset);
    cfg.thIntervalMs = prefs.getULong("th_ms", cfg.thIntervalMs);
    cfg.uvEnabled = prefs.getBool("uv_en", cfg.uvEnabled);
    cfg.uvChannel = prefs.getUChar("uv_ch", cfg.uvChannel);
    cfg.uvRolling = prefs.getUChar("uv_id", cfg.uvRolling);
    cfg.uvMultiplier = prefs.getFloat("uv_mul", cfg.uvMultiplier);
    cfg.uvOffset = prefs.getFloat("uv_off", cfg.uvOffset);
    cfg.uvIntervalMs = prefs.getULong("uv_ms", cfg.uvIntervalMs);
    cfg.repeats = prefs.getUChar("reps", cfg.repeats);
    cfg.repeatGapMs = prefs.getUShort("gap", cfg.repeatGapMs);
    prefs.end();

    cfg.thChannel = constrain(cfg.thChannel, static_cast<uint8_t>(1), static_cast<uint8_t>(3));
    cfg.uvChannel = constrain(cfg.uvChannel, static_cast<uint8_t>(1), static_cast<uint8_t>(3));
    cfg.repeats = constrain(cfg.repeats, static_cast<uint8_t>(1), static_cast<uint8_t>(4));
    cfg.halfUs = constrain(cfg.halfUs, static_cast<uint16_t>(430), static_cast<uint16_t>(550));
}

void readSensors() {
    if (live.bmeOk) {
        live.temperatureC = bme.readTemperature() + cfg.tempOffset;
        live.humidityPct = bme.readHumidity() + cfg.humOffset;
        live.pressureHpa = bme.readPressure() / 100.0f;
    }
    const int raw = analogRead(UV_ADC_PIN);
    // Generic beta mapping: ADC full scale -> UV 15.0. Calibrate multiplier/offset in Web UI.
    const float baseUv = (static_cast<float>(raw) / 4095.0f) * 15.0f;
    live.uvIndex = max(0.0f, baseUv * cfg.uvMultiplier + cfg.uvOffset);
    live.lastReadMs = millis();
}

bool txTH() {
    OregonFrame f;
    if (!radio.buildTHGR810(f, cfg.thChannel, cfg.thRolling, live.temperatureC, live.humidityPct, false)) return false;
    live.lastThFrame = radio.hex(f);
    if (cfg.rfEnabled) radio.transmit(f, cfg.repeats, cfg.repeatGapMs);
    ++live.thTxCount;
    Serial.printf("[TX] THGR810 CH%u ID=%02X T=%.1f RH=%.0f RAW=%s\n", cfg.thChannel, cfg.thRolling,
                  live.temperatureC, live.humidityPct, live.lastThFrame.c_str());
    return true;
}

bool txUV() {
    OregonFrame f;
    if (!radio.buildUVN800(f, cfg.uvChannel, cfg.uvRolling, live.uvIndex)) return false;
    live.lastUvFrame = radio.hex(f);
    if (cfg.rfEnabled) radio.transmit(f, cfg.repeats, cfg.repeatGapMs);
    ++live.uvTxCount;
    Serial.printf("[TX] UVN800 CH%u ID=%02X UV=%.1f RAW=%s\n", cfg.uvChannel, cfg.uvRolling,
                  live.uvIndex, live.lastUvFrame.c_str());
    return true;
}

String page() {
    readSensors();
    String h;
    h.reserve(12000);
    h += F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
    h += F("<title>Oregon TX Beta</title><style>body{font-family:system-ui;margin:0;background:#10141a;color:#e8edf2}main{max-width:980px;margin:auto;padding:22px}.card{background:#19212b;border:1px solid #2a3948;border-radius:14px;padding:18px;margin:14px 0}h1{margin:0}.beta{display:inline-block;background:#ffcc33;color:#111;padding:5px 10px;border-radius:999px;font-weight:700}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:12px}label{display:block;font-size:.86rem;color:#aebdcc;margin-top:10px}input,select{width:100%;box-sizing:border-box;background:#0f151c;color:#fff;border:1px solid #405064;border-radius:8px;padding:9px}button{background:#2f8cff;color:white;border:0;border-radius:8px;padding:10px 16px;margin:8px 5px 0 0;font-weight:700}code{word-break:break-all;color:#8fd3ff}.warn{color:#ffd86b}.ok{color:#86efac}a{color:#8fd3ff}</style></head><body><main>");
    h += "<span class='beta'>" + String(FW_STATUS) + "</span><h1>ESP32 Oregon Transmitter Sensor</h1><p>Firmware " + String(FW_VERSION) + "</p>";

    h += F("<div class='card'><h2>Live values</h2><div class='grid'>");
    h += "<div>Temperature<br><b>" + String(live.temperatureC,1) + " C</b></div>";
    h += "<div>Humidity<br><b>" + String(live.humidityPct,0) + " %</b></div>";
    h += "<div>Pressure<br><b>" + String(live.pressureHpa,1) + " hPa</b></div>";
    h += "<div>UV beta<br><b>" + String(live.uvIndex,1) + "</b></div></div>";
    h += "<p>BME280: <b class='" + String(live.bmeOk ? "ok">OK" : "warn">NOT FOUND") + "</b></p></div>";

    h += F("<form method='post' action='/save'><div class='card'><h2>Node / RF</h2><div class='grid'>");
    h += "<div><label>Node name</label><input name='node' value='" + esc(cfg.nodeName) + "'></div>";
    h += "<div><label>Wi-Fi SSID</label><input name='ssid' value='" + esc(cfg.wifiSsid) + "'></div>";
    h += "<div><label>Wi-Fi password</label><input type='password' name='pass' value='" + esc(cfg.wifiPassword) + "'></div>";
    h += "<div><label>OSV3 half bit us</label><input type='number' name='half' min='430' max='550' value='" + String(cfg.halfUs) + "'></div>";
    h += "<div><label>Repeats</label><input type='number' name='reps' min='1' max='4' value='" + String(cfg.repeats) + "'></div>";
    h += "<div><label>Repeat gap ms</label><input type='number' name='gap' min='20' max='1000' value='" + String(cfg.repeatGapMs) + "'></div></div>";
    h += "<label><input style='width:auto' type='checkbox' name='rf_en' " + String(cfg.rfEnabled?"checked":"") + "> RF enabled</label>";
    h += "<label><input style='width:auto' type='checkbox' name='rf_inv' " + String(cfg.rfInvert?"checked":"") + "> Invert OOK polarity</label></div>";

    h += F("<div class='card'><h2>Virtual sensor 1 - THGR810 / F824</h2><div class='grid'>");
    h += "<div><label>Channel</label><input name='th_ch' type='number' min='1' max='3' value='" + String(cfg.thChannel) + "'></div>";
    h += "<div><label>Rolling ID (hex)</label><input name='th_id' value='" + String(cfg.thRolling,HEX) + "'></div>";
    h += "<div><label>Temperature offset C</label><input name='t_off' value='" + String(cfg.tempOffset,2) + "'></div>";
    h += "<div><label>Humidity offset %</label><input name='h_off' value='" + String(cfg.humOffset,2) + "'></div>";
    h += "<div><label>TX interval ms</label><input name='th_ms' type='number' min='5000' value='" + String(cfg.thIntervalMs) + "'></div></div>";
    h += "<label><input style='width:auto' type='checkbox' name='th_en' " + String(cfg.thEnabled?"checked":"") + "> Enabled</label>";
    h += "<p>Last frame: <code>" + live.lastThFrame + "</code> | TX count " + String(live.thTxCount) + "</p></div>";

    h += F("<div class='card'><h2>Virtual sensor 2 - UVN800 / D874</h2><div class='grid'>");
    h += "<div><label>Channel</label><input name='uv_ch' type='number' min='1' max='3' value='" + String(cfg.uvChannel) + "'></div>";
    h += "<div><label>Rolling ID (hex)</label><input name='uv_id' value='" + String(cfg.uvRolling,HEX) + "'></div>";
    h += "<div><label>UV multiplier</label><input name='uv_mul' value='" + String(cfg.uvMultiplier,3) + "'></div>";
    h += "<div><label>UV offset</label><input name='uv_off' value='" + String(cfg.uvOffset,2) + "'></div>";
    h += "<div><label>TX interval ms</label><input name='uv_ms' type='number' min='5000' value='" + String(cfg.uvIntervalMs) + "'></div></div>";
    h += "<label><input style='width:auto' type='checkbox' name='uv_en' " + String(cfg.uvEnabled?"checked":"") + "> Enabled</label>";
    h += "<p>Last frame: <code>" + live.lastUvFrame + "</code> | TX count " + String(live.uvTxCount) + "</p></div>";

    h += F("<button type='submit'>Save configuration</button></form><form method='post' action='/tx'><button>Transmit now</button></form>");
    h += F("<div class='card'><h2>Firmware</h2><p class='warn'>Beta/testing build. Validate against LILYGO first, then WMR88/WMR200.</p><a href='/update'>Open firmware update page</a></div>");
    h += F("</main></body></html>");
    return h;
}

uint8_t parseHexByte(const String &s, uint8_t fallback) {
    if (!s.length()) return fallback;
    char *end = nullptr;
    const long v = strtol(s.c_str(), &end, 16);
    return (end && *end == '\0' && v >= 0 && v <= 255) ? static_cast<uint8_t>(v) : fallback;
}

void setupWeb() {
    server.on("/", HTTP_GET, [](){ server.send(200, "text/html; charset=utf-8", page()); });
    server.on("/save", HTTP_POST, [](){
        cfg.nodeName = server.arg("node");
        cfg.wifiSsid = server.arg("ssid");
        if (server.hasArg("pass")) cfg.wifiPassword = server.arg("pass");
        cfg.rfEnabled = server.hasArg("rf_en");
        cfg.rfInvert = server.hasArg("rf_inv");
        cfg.halfUs = constrain(server.arg("half").toInt(), 430L, 550L);
        cfg.repeats = constrain(server.arg("reps").toInt(), 1L, 4L);
        cfg.repeatGapMs = constrain(server.arg("gap").toInt(), 20L, 1000L);
        cfg.thEnabled = server.hasArg("th_en");
        cfg.thChannel = constrain(server.arg("th_ch").toInt(), 1L, 3L);
        cfg.thRolling = parseHexByte(server.arg("th_id"), cfg.thRolling);
        cfg.tempOffset = server.arg("t_off").toFloat();
        cfg.humOffset = server.arg("h_off").toFloat();
        cfg.thIntervalMs = max(5000UL, static_cast<uint32_t>(server.arg("th_ms").toInt()));
        cfg.uvEnabled = server.hasArg("uv_en");
        cfg.uvChannel = constrain(server.arg("uv_ch").toInt(), 1L, 3L);
        cfg.uvRolling = parseHexByte(server.arg("uv_id"), cfg.uvRolling);
        cfg.uvMultiplier = server.arg("uv_mul").toFloat();
        cfg.uvOffset = server.arg("uv_off").toFloat();
        cfg.uvIntervalMs = max(5000UL, static_cast<uint32_t>(server.arg("uv_ms").toInt()));
        saveConfig();
        radio.setHalfBitUs(cfg.halfUs); radio.setInvert(cfg.rfInvert);
        server.sendHeader("Location", "/"); server.send(303);
    });
    server.on("/tx", HTTP_POST, [](){ readSensors(); if (cfg.thEnabled) txTH(); if (cfg.uvEnabled) txUV(); server.sendHeader("Location", "/"); server.send(303); });
    server.on("/api/live", HTTP_GET, [](){
        readSensors();
        String j = "{\"status\":\"beta\",\"temperature_c\":" + String(live.temperatureC,2) +
                   ",\"humidity_pct\":" + String(live.humidityPct,1) + ",\"pressure_hpa\":" + String(live.pressureHpa,1) +
                   ",\"uv\":" + String(live.uvIndex,1) + ",\"th_frame\":\"" + live.lastThFrame + "\",\"uv_frame\":\"" + live.lastUvFrame + "\"}";
        server.send(200, "application/json", j);
    });
    server.on("/update", HTTP_GET, [](){
        server.send(200, "text/html", "<h2>Oregon TX firmware update - BETA</h2><form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='firmware' accept='.bin'><button>Upload</button></form>");
    });
    server.on("/update", HTTP_POST, [](){
        const bool ok = !Update.hasError(); server.send(200, "text/plain", ok ? "OK - rebooting" : "UPDATE FAILED"); if (ok) { delay(500); ESP.restart(); }
    }, [](){
        HTTPUpload &u = server.upload();
        if (u.status == UPLOAD_FILE_START) Update.begin(UPDATE_SIZE_UNKNOWN);
        else if (u.status == UPLOAD_FILE_WRITE) Update.write(u.buf, u.currentSize);
        else if (u.status == UPLOAD_FILE_END) Update.end(true);
    });
    server.begin();
}

void setupWifi() {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    if (cfg.wifiSsid.length()) {
        WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPassword.c_str());
        const uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) delay(250);
    }
    Serial.printf("[WEB] AP http://%s/ STA=%s\n", WiFi.softAPIP().toString().c_str(), WiFi.localIP().toString().c_str());
}

} // namespace

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.printf("\nESP32 Oregon Transmitter Sensor %s - %s\n", FW_VERSION, FW_STATUS);
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);
    loadConfig();
    Wire.begin(BME_SDA_PIN, BME_SCL_PIN);
    live.bmeOk = bme.begin(0x76, &Wire) || bme.begin(0x77, &Wire);
    analogReadResolution(12);
    radio.begin(); radio.setHalfBitUs(cfg.halfUs); radio.setInvert(cfg.rfInvert);
    setupWifi(); setupWeb();
    readSensors();
    lastThTx = millis(); lastUvTx = millis();
}

void loop() {
    server.handleClient();
    const uint32_t now = millis();
    if (now - live.lastReadMs > 2000) readSensors();
    if (cfg.thEnabled && now - lastThTx >= cfg.thIntervalMs) { txTH(); lastThTx = now; }
    if (cfg.uvEnabled && now - lastUvTx >= cfg.uvIntervalMs) { txUV(); lastUvTx = now; }
    delay(2);
}
