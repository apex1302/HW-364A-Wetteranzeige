#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#include <Wire.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C
#define OLED_SDA 14   // D6 (GPIO14)
#define OLED_SCL 12   // D5 (GPIO12)

const char* ssid = "X";
const char* password = "KEY";

const String url = "http://api.open-meteo.com/v1/forecast"
                   "?latitude=48.73&longitude=9.27"
                   "&current_weather=true"
                   "&timezone=Europe/Berlin"
                   "&daily=temperature_2m_max,temperature_2m_min,weathercode";

struct Weather {
  float currTemp = 0.0;
  int currCode = 0;
  float todayMax = 0.0;
  float todayMin = 0.0;
  int todayCode = 0;
  float tomorrowMax = 0.0;
  float tomorrowMin = 0.0;
  int tomorrowCode = 0;
  float currWindSpeed = 0.0;
  int currWindDir = 0;
} weather;

int screen = 0;
unsigned long lastSwitch = 0;
const unsigned long interval = 10000; // 10 Sekunden

Adafruit_SSD1306* display;

String weatherDescFromCode(int code) {
  if (code == 0) return "klar";
  else if (code >= 1 && code <= 3) return "wolkig";
  else if (code == 45 || code == 48) return "Nebel";
  else if (code >= 51 && code <= 67) return "Regen";
  else if (code >= 71 && code <= 77) return "Schnee/R\u00e4hre";
  else if (code >= 80 && code <= 86) return "Regenschauer";
  else if (code >= 95) return "Gewitter";
  return "unbekannt";
}

String windDirectionToString(int degrees) {
  if (degrees >= 337 || degrees < 23) return "N";
  else if (degrees >= 23 && degrees < 68) return "NO";
  else if (degrees >= 68 && degrees < 113) return "O";
  else if (degrees >= 113 && degrees < 158) return "SO";
  else if (degrees >= 158 && degrees < 203) return "S";
  else if (degrees >= 203 && degrees < 248) return "SW";
  else if (degrees >= 248 && degrees < 293) return "W";
  else if (degrees >= 293 && degrees < 337) return "NW";
  else return "?";
}

void showMessageOnDisplay(const char* line1, const char* line2 = nullptr, const char* line3 = nullptr) {
  display->clearDisplay();
  display->setTextSize(1);
  display->setTextColor(SSD1306_WHITE);
  display->setCursor(0, 0);
  display->println(line1);
  if(line2) display->println(line2);
  if(line3) display->println(line3);
  display->display();
}

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println(F("Setup startet"));

  Wire.begin(OLED_SDA, OLED_SCL);
  display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
  if(!display->begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    showMessageOnDisplay("Display Fehler", "SSD1306 init failed");
    while(1);
  }
  display->clearDisplay();
  display->display();

  Serial.print("Verbinde mit WLAN: ");
  Serial.println(ssid);
  showMessageOnDisplay("Verbinde mit WLAN", ssid);

  WiFi.begin(ssid, password);

  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - wifiStart > 20000) {
      Serial.println("\nWLAN Verbindung fehlgeschlagen!");
      showMessageOnDisplay("WLAN Fehler", "Keine Verbindung");
      while(1);
    }
  }
  Serial.println("\nWLAN verbunden.");
  showMessageOnDisplay("WLAN verbunden", WiFi.localIP().toString().c_str());
  delay(2000);

  fetchWeather();
}

void loop() {
  if (millis() - lastSwitch > interval) {
    screen = (screen + 1) % 3;
    lastSwitch = millis();
    fetchWeather();
  }
  displayWeather(screen);
  delay(100);
}

void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Kein WLAN, kein Abruf");
    showMessageOnDisplay("WLAN getrennt", "Kein Abruf möglich");
    return;
  }

  Serial.println("Hole Wetterdaten...");
  showMessageOnDisplay("Wetterdaten abrufen...");

  WiFiClient client;
  HTTPClient http;
  http.begin(client, url);

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("Antwort erhalten:");
    Serial.println(payload);

    DynamicJsonDocument json(4096);
    DeserializationError err = deserializeJson(json, payload);

    if (!err) {
      weather.currTemp = json["current_weather"]["temperature"].as<float>();
      weather.currCode = json["current_weather"]["weathercode"].as<int>();
      weather.currWindSpeed = json["current_weather"]["windspeed"].as<float>();
      weather.currWindDir = json["current_weather"]["winddirection"].as<int>();

      weather.todayMax = json["daily"]["temperature_2m_max"][0].as<float>();
      weather.todayMin = json["daily"]["temperature_2m_min"][0].as<float>();
      weather.todayCode = json["daily"]["weathercode"][0].as<int>();

      weather.tomorrowMax = json["daily"]["temperature_2m_max"][1].as<float>();
      weather.tomorrowMin = json["daily"]["temperature_2m_min"][1].as<float>();
      weather.tomorrowCode = json["daily"]["weathercode"][1].as<int>();

      Serial.println("Wetterdaten erfolgreich aktualisiert.");
    } else {
      Serial.print("JSON Fehler: ");
      Serial.println(err.c_str());
      showMessageOnDisplay("JSON Fehler", err.c_str());
    }
  } else {
    Serial.printf("HTTP Fehler: %d\n", httpCode);
    char buf[32];
    snprintf(buf, sizeof(buf), "HTTP Fehler: %d", httpCode);
    showMessageOnDisplay("HTTP Fehler", buf);
  }
  http.end();
}

void displayWeather(int s) {
  display->clearDisplay();
  display->setTextSize(1);
  display->setTextColor(SSD1306_WHITE);
  display->setCursor(0, 0);

  char buf[32];

  switch (s) {
    case 0:
      display->println(F("Jetzt (Ostfildern):"));
      snprintf(buf, sizeof(buf), "%.1f C", weather.currTemp);
      display->println(buf);
      display->println();
      display->println(F("Heute:"));
      snprintf(buf, sizeof(buf), "Max %.1f C", weather.todayMax);
      display->println(buf);
      snprintf(buf, sizeof(buf), "Min %.1f C", weather.todayMin);
      display->println(buf);
      display->println(weatherDescFromCode(weather.todayCode));
      break;

    case 1:
      display->println(F("Jetzt (Ostfildern):"));
      snprintf(buf, sizeof(buf), "%.1f C", weather.currTemp);
      display->println(buf);
      display->println();
      display->println(F("Morgen:"));
      snprintf(buf, sizeof(buf), "Max %.1f C", weather.tomorrowMax);
      display->println(buf);
      snprintf(buf, sizeof(buf), "Min %.1f C", weather.tomorrowMin);
      display->println(buf);
      display->println(weatherDescFromCode(weather.tomorrowCode));
      break;

    case 2:
      display->println(F("Wind heute:"));
      snprintf(buf, sizeof(buf), "Stärke: %.1f km/h", weather.currWindSpeed);
      display->println(buf);
      snprintf(buf, sizeof(buf), "Richtung: %s", windDirectionToString(weather.currWindDir).c_str());
      display->println(buf);
      break;
  }
  display->display();
}
