
//By: Zahid
//instagram: mochskiz

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define EEPROM_SIZE 32

const char* ssid = "NAMA_WIFI_KAMU";
const char* password = "PASSWORD_WIFI_KAMU";

const int relayPins[2] = {26, 27};

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200); // WIB (UTC+7)
AsyncWebServer server(80);

bool manualMode[2] = {false, false};
bool relayState[2] = {false, false};
int onHour[2], onMinute[2], onSecond[2];
int offHour[2], offMinute[2], offSecond[2];

void setupRelayPins() {
  for (int i = 0; i < 2; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH); // Aktif LOW
  }
}

void loadSettings() {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < 2; i++) {
    onHour[i] = EEPROM.read(i * 6);
    onMinute[i] = EEPROM.read(i * 6 + 1);
    onSecond[i] = EEPROM.read(i * 6 + 2);
    offHour[i] = EEPROM.read(i * 6 + 3);
    offMinute[i] = EEPROM.read(i * 6 + 4);
    offSecond[i] = EEPROM.read(i * 6 + 5);
  }
  EEPROM.end();
}

void saveSettings() {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < 2; i++) {
    EEPROM.write(i * 6, onHour[i]);
    EEPROM.write(i * 6 + 1, onMinute[i]);
    EEPROM.write(i * 6 + 2, onSecond[i]);
    EEPROM.write(i * 6 + 3, offHour[i]);
    EEPROM.write(i * 6 + 4, offMinute[i]);
    EEPROM.write(i * 6 + 5, offSecond[i]);
  }
  EEPROM.commit();
  EEPROM.end();
}

void displayStatus() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("Jam: ");
  display.print(timeClient.getFormattedTime());

  display.setCursor(0, 10);
  display.print("R1: ");
  display.print(relayState[0] ? "ON " : "OFF");

  display.setCursor(64, 10);
  display.print("R2: ");
  display.print(relayState[1] ? "ON" : "OFF");

  display.setCursor(0, 20);
  display.print("IP: ");
  display.print(WiFi.localIP());

  display.display();
}

String htmlPage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <style>
    body { font-family: sans-serif; background: #f4f4f4; text-align: center; }
    .card { background: white; padding: 20px; margin: 20px auto; width: 300px; border-radius: 10px; box-shadow: 0 0 10px #aaa; }
    button { padding: 10px 20px; margin: 5px; border-radius: 5px; border: none; }
    .on { background: #4CAF50; color: white; }
    .off { background: #f44336; color: white; }
  </style>
</head>
<body>
  <h2>Kontrol Relay</h2>
  %RELAY_CARDS%
  <form action='/set' method='get'>
    <h3>Atur Jadwal Relay</h3>
    Relay:
    <select name='ch'>
      <option value='0'>Relay 1</option>
      <option value='1'>Relay 2</option>
    </select><br>
    Nyala (Hour:Min:Sec): 
    <input type='number' name='on_hour' min='0' max='23' placeholder="Jam"> :
    <input type='number' name='on_minute' min='0' max='59' placeholder="Menit"> :
    <input type='number' name='on_second' min='0' max='59' placeholder="Detik"><br>
    Mati (Hour:Min:Sec): 
    <input type='number' name='off_hour' min='0' max='23' placeholder="Jam"> :
    <input type='number' name='off_minute' min='0' max='59' placeholder="Menit"> :
    <input type='number' name='off_second' min='0' max='59' placeholder="Detik"><br>
    <input type='submit' value='Simpan'>
  </form>
</body>
</html>
)rawliteral";

  String cards = "";
  for (int i = 0; i < 2; i++) {
    cards += "<div class='card'>";
    cards += "<h3>Relay " + String(i + 1) + "</h3>";
    cards += "Status sekarang: <strong>" + String(relayState[i] ? "ON" : "OFF") + "</strong><br>";
    cards += "Mode: " + String(manualMode[i] ? "Manual" : "Otomatis") + "<br>";
    cards += "Nyala: " + String(onHour[i]) + ":" + String(onMinute[i]) + ":" + String(onSecond[i]) + "<br>";
    cards += "Mati: " + String(offHour[i]) + ":" + String(offMinute[i]) + ":" + String(offSecond[i]) + "<br>";
    cards += "<a href='/toggle?ch=" + String(i) + "'><button class='" + String(relayState[i] ? "off" : "on") + "'>TOGGLE</button></a>";
    cards += "<a href='/mode?ch=" + String(i) + "'><button>Switch Mode</button></a>";
    cards += "</div>";
  }

  html.replace("%RELAY_CARDS%", cards);
  return html;
}

void setupServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", htmlPage());
  });

  server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *request){
    int ch = request->getParam("ch")->value().toInt();
    if (manualMode[ch]) {
      relayState[ch] = !relayState[ch];
      digitalWrite(relayPins[ch], relayState[ch] ? LOW : HIGH);
    }
    request->redirect("/");
  });

  server.on("/mode", HTTP_GET, [](AsyncWebServerRequest *request){
    int ch = request->getParam("ch")->value().toInt();
    manualMode[ch] = !manualMode[ch];
    request->redirect("/");
  });

  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request){
    int ch = request->getParam("ch")->value().toInt();
    onHour[ch] = request->getParam("on_hour")->value().toInt();
    onMinute[ch] = request->getParam("on_minute")->value().toInt();
    onSecond[ch] = request->getParam("on_second")->value().toInt();
    offHour[ch] = request->getParam("off_hour")->value().toInt();
    offMinute[ch] = request->getParam("off_minute")->value().toInt();
    offSecond[ch] = request->getParam("off_second")->value().toInt();
    saveSettings();
    request->redirect("/");
  });

  server.begin();
}

void setupWiFiClient() {
  WiFi.begin(ssid, password);
  Serial.print("Menyambung ke WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi terhubung!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void setupOLED() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED tidak ditemukan!"));
    for (;;);
  }
  display.clearDisplay();
  display.display();
}

void setup() {
  Serial.begin(115200);
  setupRelayPins();
  setupWiFiClient();
  setupOLED();

  timeClient.begin();
  loadSettings();
  setupServer();
}

void loop() {
  timeClient.update();
  int hourNow = timeClient.getHours();
  int minuteNow = timeClient.getMinutes();
  int secondNow = timeClient.getSeconds();

  for (int i = 0; i < 2; i++) {
    if (!manualMode[i]) {
      bool on = (hourNow == onHour[i] && minuteNow == onMinute[i] && secondNow == onSecond[i]);
      bool off = (hourNow == offHour[i] && minuteNow == offMinute[i] && secondNow == offSecond[i]);
      if (on) {
        relayState[i] = true;
        digitalWrite(relayPins[i], LOW); // Relay ON
      }
      if (off) {
        relayState[i] = false;
        digitalWrite(relayPins[i], HIGH); // Relay OFF
      }
    }
  }

  displayStatus();
  delay(1000);
}
