/*
  ESP32-CAM (SD logging + Telegram alerts)
  - Menyimpan logs ke SD_MMC (logs.csv)
  - Menyimpan foto ke SD (/photos/IMG_<ms>.jpg)
  - Endpoint: /logs?lines=N  dan /photos/<name>
  - Gas alert >= GAS_THRESHOLD -> kirim Telegram (cooldown)
  - RFID UNAUTH -> ambil foto, simpan ke SD, kirim Telegram
  - Sesuaikan WIFI_SSID, WIFI_PASS, BOT_TOKEN, CHAT_ID
*/

#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "FS.h"
#include "SD_MMC.h"

// ----- CONFIG -----
const char* WIFI_SSID = "Eben";
const char* WIFI_PASS = "yyyyyyyy";
String BOT_TOKEN = "";
String CHAT_ID = "";

#define LED_WIFI 33

WebServer server(80);

String lastStatusJson = "{}";
String lastPhotoUrl = "";

// gas alert
const float GAS_THRESHOLD = 1.33; // volt
unsigned long lastGasAlert = 0;
const unsigned long GAS_ALERT_COOLDOWN = 5UL * 60UL * 1000UL; // 5 menit

// Log file
const char* LOG_PATH = "/logs.csv";
const char* PHOTO_DIR = "/photos";

// forward
void addCorsHeaders();
void initCamera();
void handleStatus();
void handleControl();
void handleCapture();
void sendPhotoToTelegram(camera_fb_t * fb);
void savePhotoToSD(camera_fb_t * fb, String &outPath);
void appendLogCSV(const String &line);
void handleLogs();
void handlePhotoServe();
void sendTelegramMessage(const String &text);
String urlEncode(const String &str);

void setup() {
  Serial.begin(9600);
  Serial.setTimeout(5);

  pinMode(LED_WIFI, OUTPUT);
  digitalWrite(LED_WIFI, LOW);

  // init SD_MMC
  if (!SD_MMC.begin()) {
    Serial.println("SD_MMC mount failed");
  } else {
    Serial.println("SD_MMC mounted");
    // create photos dir if not exists
    if (!SD_MMC.exists(PHOTO_DIR)) {
      SD_MMC.mkdir(PHOTO_DIR);
    }
    // create header if logs not exist
    if (!SD_MMC.exists(LOG_PATH)) {
      File f = SD_MMC.open(LOG_PATH, FILE_WRITE);
      if (f) {
        f.println("ts_ms,temp,hum,gas_raw,gas_v,gas_safe,r1,r2,r3,servoAngle");
        f.close();
      }
    }
  }

  initCamera();

  // connect to WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    if (millis() - start > 15000) break;
  }
  if(WiFi.status() == WL_CONNECTED){
    Serial.println();
    Serial.print("Connected. IP: ");
    Serial.println(WiFi.localIP());
    digitalWrite(LED_WIFI, HIGH);
  } else {
    Serial.println();
    Serial.println("WiFi connect failed (proceeding without).");
    digitalWrite(LED_WIFI, LOW);
  }

  // routes + CORS OPTIONS
  server.on("/control", HTTP_OPTIONS, [](){ addCorsHeaders(); server.send(204, "text/plain", ""); });
  server.on("/status", HTTP_OPTIONS, [](){ addCorsHeaders(); server.send(204, "text/plain", ""); });
  server.on("/capture", HTTP_OPTIONS, [](){ addCorsHeaders(); server.send(204, "text/plain", ""); });
  server.on("/logs", HTTP_GET, handleLogs); // new: return CSV tail
  server.on("/photos", HTTP_GET, handlePhotoServe); // serve list or single? we'll handle filename query
  server.on("/photo", HTTP_GET, [](){ // legacy: serve last captured via ?name=
    addCorsHeaders();
    if (!server.hasArg("name")) {
      server.send(400, "text/plain", "missing name");
      return;
    }
    String name = server.arg("name");
    String path = String(PHOTO_DIR) + "/" + name;
    if (!SD_MMC.exists(path.c_str())) {
      server.send(404, "text/plain", "not found");
      return;
    }
    File f = SD_MMC.open(path.c_str(), FILE_READ);
    if (!f) { server.send(500, "text/plain", "open fail"); return; }
    server.streamFile(f, "image/jpeg");
    f.close();
  });

  server.on("/status", HTTP_GET, handleStatus);
  server.on("/control", HTTP_POST, handleControl);
  server.on("/capture", HTTP_GET, handleCapture);

  server.begin();
  Serial.println("WebServer started");
}

void loop() {
  server.handleClient();

  // WiFi LED indicator dynamic
  if (WiFi.status() == WL_CONNECTED) digitalWrite(LED_WIFI, HIGH);
  else digitalWrite(LED_WIFI, LOW);

  // Read from Arduino over Serial (incoming JSON or EVENT RFID)
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) return;

    Serial.print("[RX Arduino] ");
    Serial.println(line);

    if (line.startsWith("EVENT RFID")) {
      // parse UID & event robustly
      int firstSpace = line.indexOf(' ');
      int secondSpace = line.indexOf(' ', firstSpace + 1);
      int lastSpace = line.lastIndexOf(' ');
      String uid = "";
      String ev = "";
      if (secondSpace >= 0 && lastSpace > secondSpace) {
        uid = line.substring(secondSpace + 1, lastSpace);
        ev = line.substring(lastSpace + 1);
        uid.trim();
        ev.trim();
      } else {
        int sp = line.indexOf(' ', 11); // after "EVENT RFID "
        if (sp > 0) {
          uid = line.substring(11, sp);
          ev = line.substring(sp + 1);
          uid.trim(); ev.trim();
        }
      }
      uid.toUpperCase();

      DynamicJsonDocument doc(512);
      doc["rfid"]["uid"] = uid;
      doc["rfid"]["authorized"] = (ev == "AUTH");
      doc["rfid"]["time"] = millis();
      String s; serializeJson(doc, s);
      lastStatusJson = s;

      if (ev == "UNAUTH") {
        camera_fb_t * fb = esp_camera_fb_get();
        if (fb) {
          // save photo to SD then send
          String savedPath;
          savePhotoToSD(fb, savedPath);
          sendPhotoToTelegram(fb);
          esp_camera_fb_return(fb);
          // set lastPhotoUrl to the served path
          if (savedPath.length()) lastPhotoUrl = String("http://") + WiFi.localIP().toString() + "/photo?name=" + savedPath.substring(savedPath.lastIndexOf('/')+1);
        } else {
          Serial.println("Camera capture failed");
        }
      }
    } else if (line.startsWith("{")) {
      // assume valid JSON from Arduino
      lastStatusJson = line;

      // parse JSON to check gas and to log
      DynamicJsonDocument ddoc(2048);
      DeserializationError derr = deserializeJson(ddoc, line);
      if (!derr) {
        // get values (guarding types)
        float t = ddoc["dht"]["temp"] | 0.0;
        float h = ddoc["dht"]["hum"] | 0.0;
        int gasRaw = ddoc["gas"]["raw"] | -1;
        float gasVal = 0.0;
        bool gasSafe = true;
        if (ddoc["gas"]["value"].is<float>() || ddoc["gas"]["value"].is<double>() || ddoc["gas"]["value"].is<int>()) {
          gasVal = ddoc["gas"]["value"].as<float>();
        } else if (ddoc["gas"]["raw"].is<int>()) {
          gasRaw = ddoc["gas"]["raw"].as<int>();
          gasVal = gasRaw / 1023.0 * 5.0;
        }
        if (ddoc["gas"]["safe"].is<bool>()) {
          gasSafe = ddoc["gas"]["safe"].as<bool>();
        } else {
          gasSafe = (gasVal < GAS_THRESHOLD);
        }
        int r1 = ddoc["relays"][0] | 0;
        int r2 = ddoc["relays"][1] | 0;
        int r3 = ddoc["relays"][2] | 0;
        int servoAngle = ddoc["servoAngle"] | 0;

        // build CSV line: ts_ms,temp,hum,gas_raw,gas_v,gas_safe,r1,r2,r3,servoAngle
        String csv;
        csv += String(millis()) + ",";
        csv += String(t,1) + ",";
        csv += String(h,1) + ",";
        csv += String(gasRaw) + ",";
        csv += String(gasVal,2) + ",";
        csv += (gasSafe ? "1" : "0") + String(",") ;
        csv += String(r1) + "," + String(r2) + "," + String(r3) + ",";
        csv += String(servoAngle);
        appendLogCSV(csv);

        // gas alert logic
        if (gasVal >= GAS_THRESHOLD) {
          unsigned long now = millis();
          if (now - lastGasAlert > GAS_ALERT_COOLDOWN) {
            lastGasAlert = now;
            Serial.println("Gas threshold exceeded, sending Telegram alert");
            sendTelegramMessage("ALERT: GAS BOCOR terdeteksi (nilai: " + String(gasVal, 2) + " V)");
          } else {
            Serial.println("Gas detected but in cooldown - no alert sent");
          }
        }
      } else {
        Serial.println("JSON parse error for Arduino line");
      }
    } else {
      // ignore or log other messages
    }
  }
}

void addCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// ========== /status ==========
void handleStatus() {
  addCorsHeaders();
  DynamicJsonDocument doc(1024);
  DeserializationError err = deserializeJson(doc, lastStatusJson);
  if (err) doc.clear();

  doc["camera_preview"] = String("http://") + WiFi.localIP().toString() + "/capture";
  doc["lastPhotoUrl"] = lastPhotoUrl;

  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// ========== /control ==========
void handleControl() {
  addCorsHeaders();
  String body = server.arg("plain");
  if (body.length() == 0) {
    server.send(400, "text/plain", "No body");
    return;
  }
  DynamicJsonDocument cmd(512);
  DeserializationError err = deserializeJson(cmd, body);
  if (err) {
    server.send(400, "text/plain", "Invalid JSON");
    return;
  }

  String type = cmd["type"] | "";
  if (type == "relay") {
    int idx = cmd["index"] | 1;
    bool val = cmd["value"] | false;
    String out = "CMD RELAY " + String(idx) + " " + String(val ? 1 : 0) + "\n";
    Serial.print(out);
    server.send(200, "application/json", "{\"ok\":true}");
  } else if (type == "servo") {
    int angle = cmd["angle"] | 0;
    String out = "CMD SERVO " + String(angle) + "\n";
    Serial.print(out);
    server.send(200, "application/json", "{\"ok\":true}");
  } else if (type == "capture_and_send") {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      server.send(500, "text/plain", "Camera failed");
      return;
    }
    String savedPath;
    savePhotoToSD(fb, savedPath);
    sendPhotoToTelegram(fb);
    esp_camera_fb_return(fb);
    if (savedPath.length()) lastPhotoUrl = String("http://") + WiFi.localIP().toString() + "/photo?name=" + savedPath.substring(savedPath.lastIndexOf('/')+1);
    server.send(200, "application/json", "{\"ok\":true}");
  } else {
    server.send(400, "text/plain", "Unknown command");
  }
}

// ========== /capture (returns jpeg) ==========
void handleCapture() {
  addCorsHeaders();
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Capture failed");
    return;
  }
  server.sendHeader("Content-Type", "image/jpeg");
  server.send_P(200, "image/jpeg", (const char*)fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

// ========== /logs?lines=N  (return tail) ==========
void handleLogs() {
  addCorsHeaders();
  int lines = 200;
  if (server.hasArg("lines")) lines = server.arg("lines").toInt();
  if (!SD_MMC.exists(LOG_PATH)) {
    server.send(404, "text/plain", "no logs");
    return;
  }
  File f = SD_MMC.open(LOG_PATH, FILE_READ);
  if (!f) { server.send(500, "text/plain", "open fail"); return; }

  // naive tail: read whole file (ok for moderate sizes). For large logs, implement ring buffer
  String all;
  while (f.available()) {
    all += (char)f.read();
  }
  f.close();

  // split lines
  std::vector<String> parts;
  int start = 0;
  while (start < (int)all.length()) {
    int nl = all.indexOf('\n', start);
    if (nl == -1) nl = all.length();
    String line = all.substring(start, nl);
    parts.push_back(line);
    start = nl + 1;
  }
  String out;
  int total = parts.size();
  int from = max(0, total - lines);
  for (int i = from; i < total; i++) {
    out += parts[i] + "\n";
  }
  server.send(200, "text/csv", out);
}

// Serve SD photo by query (see /photo?name=IMG_...)
void handlePhotoServe() {
  addCorsHeaders();
  if (!server.hasArg("name")) {
    // optionally list photos
    String list = "";
    File root = SD_MMC.open(PHOTO_DIR);
    if (!root) { server.send(500, "text/plain", "open photos fail"); return; }
    File file = root.openNextFile();
    while (file) {
      list += String(file.name()) + "\n";
      file = root.openNextFile();
    }
    root.close();
    server.send(200, "text/plain", list);
    return;
  }
  String name = server.arg("name");
  String path = String(PHOTO_DIR) + "/" + name;
  if (!SD_MMC.exists(path.c_str())) {
    server.send(404, "text/plain", "not found");
    return;
  }
  File f = SD_MMC.open(path.c_str(), FILE_READ);
  if (!f) { server.send(500, "text/plain", "open fail"); return; }
  server.streamFile(f, "image/jpeg");
  f.close();
}

// ========== camera init ==========
void initCamera() {
  camera_config_t config;
  // pin definitions (AI Thinker)
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
  } else {
    Serial.println("Camera init OK");
  }
}

// Save photo to SD_MMC and return path in outPath (e.g. /photos/IMG_12345.jpg)
void savePhotoToSD(camera_fb_t * fb, String &outPath) {
  outPath = "";
  if (!SD_MMC.begin()) {
    // already tried in setup, but double-check
    Serial.println("SD not mounted, cannot save");
    return;
  }
  String filename = String("/photos/IMG_") + String(millis()) + ".jpg";
  File f = SD_MMC.open(filename.c_str(), FILE_WRITE);
  if (!f) {
    Serial.println("Failed to open file for writing");
    return;
  }
  size_t written = f.write(fb->buf, fb->len);
  f.close();
  Serial.printf("Saved photo %s (%u bytes)\n", filename.c_str(), (unsigned)written);
  outPath = filename;
}

// Append a line to logs.csv
void appendLogCSV(const String &line) {
  if (!SD_MMC.begin()) {
    Serial.println("SD not mounted, cannot log");
    return;
  }
  File f = SD_MMC.open(LOG_PATH, FILE_APPEND);
  if (!f) {
    Serial.println("open logs fail");
    return;
  }
  f.println(line);
  f.close();
}

// ========== send photo to Telegram (unchanged, posts fb buffer) ==========
void sendPhotoToTelegram(camera_fb_t * fb) {
  if (BOT_TOKEN == "YOUR_BOT_TOKEN") {
    Serial.println("BOT_TOKEN belum diset!");
    return;
  }

  const char* host = "api.telegram.org";
  String url = "/bot" + BOT_TOKEN + "/sendPhoto";

  WiFiClientSecure client;
  client.setInsecure();
  if (!client.connect(host, 443)) {
    Serial.println("Failed connect to Telegram");
    return;
  }

  String boundary = "----ESP32Boundary" + String(millis());
  String head = "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n";
  head += CHAT_ID + "\r\n";
  head += "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"photo\"; filename=\"image.jpg\"\r\n";
  head += "Content-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";

  size_t contentLength = head.length() + fb->len + tail.length();

  String req = "POST " + url + " HTTP/1.1\r\n";
  req += "Host: " + String(host) + "\r\n";
  req += "User-Agent: ESP32CAM\r\n";
  req += "Connection: close\r\n";
  req += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
  req += "Content-Length: " + String(contentLength) + "\r\n\r\n";

  client.print(req);
  client.print(head);
  client.write(fb->buf, fb->len);
  client.print(tail);

  unsigned long timeout = millis();
  while (client.connected() && millis() - timeout < 7000) {
    while (client.available()) {
      String line = client.readStringUntil('\n');
      timeout = millis();
    }
    delay(1);
  }

  Serial.println("Photo uploaded to Telegram");
  client.stop();
  lastPhotoUrl = "";
}

// ========== send text message to Telegram (simple GET) ==========
void sendTelegramMessage(const String &text) {
  if (BOT_TOKEN == "YOUR_BOT_TOKEN") {
    Serial.println("BOT_TOKEN belum diset!");
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No WiFi - cannot send Telegram message");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  String encoded = urlEncode(text);
  String url = "https://api.telegram.org/bot" + BOT_TOKEN + "/sendMessage?chat_id=" + CHAT_ID + "&text=" + encoded;

  HTTPClient https;
  if (https.begin(client, url)) {
    int httpCode = https.GET();
    if (httpCode > 0) {
      Serial.printf("Telegram sendMessage HTTP %d\n", httpCode);
    } else {
      Serial.printf("HTTP request failed: %s\n", https.errorToString(httpCode).c_str());
    }
    https.end();
  } else {
    Serial.println("Failed to begin HTTPS for Telegram");
  }
}

// minimal url encode (space -> %20, others kept simple)
String urlEncode(const String &str) {
  String s = "";
  for (size_t i = 0; i < str.length(); i++) {
    char c = str[i];
    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      s += c;
    } else if (c == ' ') {
      s += "%20";
    } else {
      char buf[5];
      sprintf(buf, "%%%02X", (uint8_t)c);
      s += buf;
    }
  }
  return s;
}
