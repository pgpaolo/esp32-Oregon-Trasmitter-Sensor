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

struct Config {
    String ssid;
    String password;
    bool rfEnabled{true};
    bool rfInvert{false};
    uint16_t halfUs{OSV3_HALF_US_DEFAULT};
    uint8_t repeats{2};
    uint16_t repeatGapMs{120};

    bool thEnabled{true};
    uint8_t thChannel{1};
    uint8_t thId{0x42};
    float tempOffset{0};
    float humOffset{0};
    uint32_t thIntervalMs{48000};

    bool uvEnabled{true};
    uint8_t uvChannel{1};
    uint8_t uvId{0x63};
    float uvMultiplier{1};
    float uvOffset{0};
    uint32_t uvIntervalMs{60000};
};

struct Live {
    bool bmeOk{false};
    float temp{NAN};
    float hum{NAN};
    float pressure{NAN};
    float uv{NAN};
    String thFrame;
    String uvFrame;
    uint32_t thCount{0};
    uint32_t uvCount{0};
    uint32_t lastRead{0};
};

Config cfg;
Live live;
Preferences prefs;
WebServer web(80);
Adafruit_BME280 bme;
OregonTx radio(OREGON_TX_PIN);
uint32_t lastTh = 0;
uint32_t lastUv = 0;

uint8_t hexByte(const String &value, uint8_t fallback) {
    if (!value.length()) return fallback;
    char *end = nullptr;
    long v = strtol(value.c_str(), &end, 16);
    return (end && *end == 0 && v >= 0 && v <= 255) ? static_cast<uint8_t>(v) : fallback;
}

void loadConfig() {
    prefs.begin("oregon-tx", true);
    cfg.ssid = prefs.getString("ssid", "");
    cfg.password = prefs.getString("pass", "");
    cfg.rfEnabled = prefs.getBool("rf_en", true);
    cfg.rfInvert = prefs.getBool("rf_inv", false);
    cfg.halfUs = prefs.getUShort("half", OSV3_HALF_US_DEFAULT);
    cfg.repeats = prefs.getUChar("reps", 2);
    cfg.repeatGapMs = prefs.getUShort("gap", 120);
    cfg.thEnabled = prefs.getBool("th_en", true);
    cfg.thChannel = prefs.getUChar("th_ch", 1);
    cfg.thId = prefs.getUChar("th_id", 0x42);
    cfg.tempOffset = prefs.getFloat("t_off", 0);
    cfg.humOffset = prefs.getFloat("h_off", 0);
    cfg.thIntervalMs = prefs.getULong("th_ms", 48000);
    cfg.uvEnabled = prefs.getBool("uv_en", true);
    cfg.uvChannel = prefs.getUChar("uv_ch", 1);
    cfg.uvId = prefs.getUChar("uv_id", 0x63);
    cfg.uvMultiplier = prefs.getFloat("uv_mul", 1);
    cfg.uvOffset = prefs.getFloat("uv_off", 0);
    cfg.uvIntervalMs = prefs.getULong("uv_ms", 60000);
    prefs.end();

    cfg.halfUs = constrain(cfg.halfUs, (uint16_t)430, (uint16_t)550);
    cfg.repeats = constrain(cfg.repeats, (uint8_t)1, (uint8_t)4);
    cfg.thChannel = constrain(cfg.thChannel, (uint8_t)1, (uint8_t)3);
    cfg.uvChannel = constrain(cfg.uvChannel, (uint8_t)1, (uint8_t)3);
}

void saveConfig() {
    prefs.begin("oregon-tx", false);
    prefs.putString("ssid", cfg.ssid);
    prefs.putString("pass", cfg.password);
    prefs.putBool("rf_en", cfg.rfEnabled);
    prefs.putBool("rf_inv", cfg.rfInvert);
    prefs.putUShort("half", cfg.halfUs);
    prefs.putUChar("reps", cfg.repeats);
    prefs.putUShort("gap", cfg.repeatGapMs);
    prefs.putBool("th_en", cfg.thEnabled);
    prefs.putUChar("th_ch", cfg.thChannel);
    prefs.putUChar("th_id", cfg.thId);
    prefs.putFloat("t_off", cfg.tempOffset);
    prefs.putFloat("h_off", cfg.humOffset);
    prefs.putULong("th_ms", cfg.thIntervalMs);
    prefs.putBool("uv_en", cfg.uvEnabled);
    prefs.putUChar("uv_ch", cfg.uvChannel);
    prefs.putUChar("uv_id", cfg.uvId);
    prefs.putFloat("uv_mul", cfg.uvMultiplier);
    prefs.putFloat("uv_off", cfg.uvOffset);
    prefs.putULong("uv_ms", cfg.uvIntervalMs);
    prefs.end();
}

void readInputs() {
    if (live.bmeOk) {
        live.temp = bme.readTemperature() + cfg.tempOffset;
        live.hum = bme.readHumidity() + cfg.humOffset;
        live.pressure = bme.readPressure() / 100.0f;
    }
    const int adc = analogRead(UV_ADC_PIN);
    const float baseUv = ((float)adc / 4095.0f) * 15.0f;
    live.uv = max(0.0f, baseUv * cfg.uvMultiplier + cfg.uvOffset);
    live.lastRead = millis();
}

bool sendTH() {
    OregonFrame f;
    if (!radio.buildTHGR810(f, cfg.thChannel, cfg.thId, live.temp, live.hum, false)) return false;
    live.thFrame = radio.hex(f);
    if (cfg.rfEnabled) radio.transmit(f, cfg.repeats, cfg.repeatGapMs);
    live.thCount++;
    Serial.printf("[TX] THGR810 CH%u ID=%02X T=%.1f RH=%.0f RAW=%s\n",
                  cfg.thChannel, cfg.thId, live.temp, live.hum, live.thFrame.c_str());
    return true;
}

bool sendUV() {
    OregonFrame f;
    if (!radio.buildUVN800(f, cfg.uvChannel, cfg.uvId, live.uv)) return false;
    live.uvFrame = radio.hex(f);
    if (cfg.rfEnabled) radio.transmit(f, cfg.repeats, cfg.repeatGapMs);
    live.uvCount++;
    Serial.printf("[TX] UVN800 CH%u ID=%02X UV=%.1f RAW=%s\n",
                  cfg.uvChannel, cfg.uvId, live.uv, live.uvFrame.c_str());
    return true;
}

String checked(bool value) { return value ? " checked" : ""; }

String htmlPage() {
    readInputs();
    String h;
    h.reserve(10000);
    h += F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
    h += F("<style>body{font-family:Arial;background:#111820;color:#e9eef4;margin:0}main{max-width:920px;margin:auto;padding:20px}.card{background:#1b2631;border:1px solid #334455;border-radius:12px;padding:16px;margin:14px 0}.beta{display:inline-block;background:#ffd43b;color:#111;padding:5px 10px;border-radius:20px;font-weight:bold}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:12px}input{width:100%;box-sizing:border-box;padding:8px;background:#10171f;color:white;border:1px solid #536577;border-radius:6px}label{display:block;margin-top:8px;color:#bdd0df}button{padding:10px 14px;margin:6px 4px 0 0;background:#2684ff;color:white;border:0;border-radius:7px;font-weight:bold}code{color:#8ed3ff;word-break:break-all}a{color:#8ed3ff}</style></head><body><main>");
    h += "<span class='beta'>" + String(FW_STATUS) + "</span><h1>ESP32 Oregon Transmitter Sensor</h1><p>Firmware " + String(FW_VERSION) + "</p>";

    h += F("<div class='card'><h2>Live</h2><div class='grid'>");
    h += "<div>Temperature<br><b>" + String(live.temp,1) + " C</b></div>";
    h += "<div>Humidity<br><b>" + String(live.hum,0) + " %</b></div>";
    h += "<div>Pressure<br><b>" + String(live.pressure,1) + " hPa</b></div>";
    h += "<div>UV<br><b>" + String(live.uv,1) + "</b></div></div>";
    h += "<p>BME280: <b>" + String(live.bmeOk ? "OK" : "NOT FOUND") + "</b></p></div>";

    h += F("<form method='post' action='/save'><div class='card'><h2>RF</h2><div class='grid'>");
    h += "<div><label>Wi-Fi SSID</label><input name='ssid' value='" + cfg.ssid + "'></div>";
    h += "<div><label>Wi-Fi password</label><input type='password' name='pass' value='" + cfg.password + "'></div>";
    h += "<div><label>OSV3 half-bit us</label><input name='half' type='number' min='430' max='550' value='" + String(cfg.halfUs) + "'></div>";
    h += "<div><label>Repeats</label><input name='reps' type='number' min='1' max='4' value='" + String(cfg.repeats) + "'></div>";
    h += "<div><label>Repeat gap ms</label><input name='gap' type='number' min='20' max='1000' value='" + String(cfg.repeatGapMs) + "'></div></div>";
    h += "<label><input style='width:auto' name='rf_en' type='checkbox'" + checked(cfg.rfEnabled) + "> RF enabled</label>";
    h += "<label><input style='width:auto' name='rf_inv' type='checkbox'" + checked(cfg.rfInvert) + "> Invert RF polarity</label></div>";

    h += F("<div class='card'><h2>Virtual THGR810 - F824</h2><div class='grid'>");
    h += "<div><label>Channel</label><input name='th_ch' type='number' min='1' max='3' value='" + String(cfg.thChannel) + "'></div>";
    h += "<div><label>Rolling ID hex</label><input name='th_id' value='" + String(cfg.thId, HEX) + "'></div>";
    h += "<div><label>Temp offset C</label><input name='t_off' value='" + String(cfg.tempOffset,2) + "'></div>";
    h += "<div><label>Humidity offset %</label><input name='h_off' value='" + String(cfg.humOffset,2) + "'></div>";
    h += "<div><label>Interval ms</label><input name='th_ms' type='number' min='5000' value='" + String(cfg.thIntervalMs) + "'></div></div>";
    h += "<label><input style='width:auto' name='th_en' type='checkbox'" + checked(cfg.thEnabled) + "> Enabled</label>";
    h += "<p>Last: <code>" + live.thFrame + "</code> | count " + String(live.thCount) + "</p></div>";

    h += F("<div class='card'><h2>Virtual UVN800 - D874</h2><div class='grid'>");
    h += "<div><label>Channel</label><input name='uv_ch' type='number' min='1' max='3' value='" + String(cfg.uvChannel) + "'></div>";
    h += "<div><label>Rolling ID hex</label><input name='uv_id' value='" + String(cfg.uvId, HEX) + "'></div>";
    h += "<div><label>UV multiplier</label><input name='uv_mul' value='" + String(cfg.uvMultiplier,3) + "'></div>";
    h += "<div><label>UV offset</label><input name='uv_off' value='" + String(cfg.uvOffset,2) + "'></div>";
    h += "<div><label>Interval ms</label><input name='uv_ms' type='number' min='5000' value='" + String(cfg.uvIntervalMs) + "'></div></div>";
    h += "<label><input style='width:auto' name='uv_en' type='checkbox'" + checked(cfg.uvEnabled) + "> Enabled</label>";
    h += "<p>Last: <code>" + live.uvFrame + "</code> | count " + String(live.uvCount) + "</p></div>";

    h += F("<button>Save configuration</button></form><form method='post' action='/tx'><button>Transmit now</button></form>");
    h += F("<div class='card'><h2>Beta notes</h2><p>THGR810 and UVN800 are the first hardware-validation targets. PCR800 and WGR800 encoders are present in the codebase but remain bench-testing profiles until console validation.</p><p><a href='/update'>Firmware update</a></p></div>");
    h += F("</main></body></html>");
    return h;
}

void setupWeb() {
    web.on("/", HTTP_GET, [](){ web.send(200, "text/html; charset=utf-8", htmlPage()); });

    web.on("/save", HTTP_POST, [](){
        cfg.ssid = web.arg("ssid");
        if (web.hasArg("pass")) cfg.password = web.arg("pass");
        cfg.rfEnabled = web.hasArg("rf_en");
        cfg.rfInvert = web.hasArg("rf_inv");
        cfg.halfUs = constrain((long)web.arg("half").toInt(), 430L, 550L);
        cfg.repeats = constrain((long)web.arg("reps").toInt(), 1L, 4L);
        cfg.repeatGapMs = constrain((long)web.arg("gap").toInt(), 20L, 1000L);
        cfg.thEnabled = web.hasArg("th_en");
        cfg.thChannel = constrain((long)web.arg("th_ch").toInt(), 1L, 3L);
        cfg.thId = hexByte(web.arg("th_id"), cfg.thId);
        cfg.tempOffset = web.arg("t_off").toFloat();
        cfg.humOffset = web.arg("h_off").toFloat();
        cfg.thIntervalMs = max(5000UL, (uint32_t)web.arg("th_ms").toInt());
        cfg.uvEnabled = web.hasArg("uv_en");
        cfg.uvChannel = constrain((long)web.arg("uv_ch").toInt(), 1L, 3L);
        cfg.uvId = hexByte(web.arg("uv_id"), cfg.uvId);
        cfg.uvMultiplier = web.arg("uv_mul").toFloat();
        cfg.uvOffset = web.arg("uv_off").toFloat();
        cfg.uvIntervalMs = max(5000UL, (uint32_t)web.arg("uv_ms").toInt());
        saveConfig();
        radio.setHalfBitUs(cfg.halfUs);
        radio.setInvert(cfg.rfInvert);
        web.sendHeader("Location", "/");
        web.send(303);
    });

    web.on("/tx", HTTP_POST, [](){
        readInputs();
        if (cfg.thEnabled) sendTH();
        if (cfg.uvEnabled) sendUV();
        web.sendHeader("Location", "/");
        web.send(303);
    });

    web.on("/api/live", HTTP_GET, [](){
        readInputs();
        String json = "{\"status\":\"beta\",\"temperature_c\":" + String(live.temp,2) +
                      ",\"humidity_pct\":" + String(live.hum,1) +
                      ",\"pressure_hpa\":" + String(live.pressure,1) +
                      ",\"uv\":" + String(live.uv,1) +
                      ",\"th_frame\":\"" + live.thFrame + "\",\"uv_frame\":\"" + live.uvFrame + "\"}";
        web.send(200, "application/json", json);
    });

    web.on("/update", HTTP_GET, [](){
        web.send(200, "text/html", "<h2>Oregon TX BETA firmware update</h2><form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='firmware' accept='.bin'><button>Upload</button></form>");
    });

    web.on("/update", HTTP_POST, [](){
        bool ok = !Update.hasError();
        web.send(200, "text/plain", ok ? "OK - rebooting" : "UPDATE FAILED");
        if (ok) { delay(500); ESP.restart(); }
    }, [](){
        HTTPUpload &upload = web.upload();
        if (upload.status == UPLOAD_FILE_START) Update.begin(UPDATE_SIZE_UNKNOWN);
        else if (upload.status == UPLOAD_FILE_WRITE) Update.write(upload.buf, upload.currentSize);
        else if (upload.status == UPLOAD_FILE_END) Update.end(true);
    });

    web.begin();
}

void setupWifi() {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID);
    if (cfg.ssid.length()) {
        WiFi.begin(cfg.ssid.c_str(), cfg.password.c_str());
        uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) delay(250);
    }
    Serial.printf("[WEB] AP=%s STA=%s\n", WiFi.softAPIP().toString().c_str(), WiFi.localIP().toString().c_str());
}

} // namespace

void setup() {
    Serial.begin(115200);
    delay(250);
    Serial.printf("\nESP32 Oregon Transmitter Sensor %s - %s\n", FW_VERSION, FW_STATUS);
    loadConfig();

    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);
    Wire.begin(BME_SDA_PIN, BME_SCL_PIN);
    live.bmeOk = bme.begin(0x76, &Wire);
    if (!live.bmeOk) live.bmeOk = bme.begin(0x77, &Wire);
    analogReadResolution(12);

    radio.begin();
    radio.setHalfBitUs(cfg.halfUs);
    radio.setInvert(cfg.rfInvert);
    setupWifi();
    setupWeb();
    readInputs();
    lastTh = millis();
    lastUv = millis();
}

void loop() {
    web.handleClient();
    uint32_t now = millis();
    if (now - live.lastRead >= 2000) readInputs();
    if (cfg.thEnabled && now - lastTh >= cfg.thIntervalMs) { sendTH(); lastTh = now; }
    if (cfg.uvEnabled && now - lastUv >= cfg.uvIntervalMs) { sendUV(); lastUv = now; }
    delay(2);
}
