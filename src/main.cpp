#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include "HTTPUpdateServer.h"
#include <WiFiUdp.h>
#include "NTP.h"
#include "ConfigManager.h"
#include "voipphone.h"
#include "ClickButton.h"
#include <TimeLib.h>
#include "pages.h"
#include <ArduinoJson.h>

#define mDNSUpdate(c)  do {} while(0)
using WebServerClass = WebServer;
using HTTPUpdateServerClass = HTTPUpdateServer;

HTTPUpdateServer httpUpdate;
VOIPPhone doorphone;
bool doorphonerunning = false;

ClickButton button1(4, LOW, CLICKBTN_PULLUP);
ClickButton button2(2, LOW, CLICKBTN_PULLUP);
ClickButton button3(15, LOW, CLICKBTN_PULLUP);

bool isbusy = false;

const char *settingsHTML = (char *)"/settings.html";
const char *stylesCSS = (char *)"/styles.css";
const char *mainJS = (char *)"/main.js";

struct Config {
  char device_name[32];
  char telnr_1[32];
  char telnr_2[32];
  char telnr_3[32];
  char sip_user[32];
  char sip_pass[32];
  char sip_ip[32];
  uint8_t mic_gain;
  uint8_t amp_gain;
  int tz_dst;
  int tz_std;
  float lat;
  float lon;
  uint8_t light_r;
  uint8_t light_g;
  uint8_t light_b;
  uint8_t ring_r;
  uint8_t ring_g;
  uint8_t ring_b;
  bool echocompensation;
  uint8_t echodamping;
  uint echothreshold;
} config;

struct Metadata {
  int8_t version;
} meta;

ConfigManager configManager;

WiFiUDP wifiUdp;
NTP ntp(wifiUdp);

// 1. В режиме AP оставляем ConfigManager управлять своей стартовой страницей
void APCallback(WebServer *server) {
    // Не перехватываем "/", чтобы ConfigManager отдал стандартный файл точки доступа
    server->on("/styles.css", HTTPMethod::HTTP_GET, [server](){
        server->send_P(200, mimeCSS, CSS_styles);
    });
    Serial.println("LOG: [APCallback] AP Mode Callback triggered.");
}

// 2. В обычном режиме регистрируем расширенные настройки из pages.h
void APICallback(WebServer *server) {
    Serial.println("LOG: [APICallback] Регистрация маршрутов API...");

    server->on("/disconnect", HTTPMethod::HTTP_GET, [server](){
        Serial.println("LOG: [WEB] Очистка Wi-Fi настроек.");
        configManager.clearWifiSettings(false);
    });

    // Страница расширенных настроек
    server->on("/settings.html", HTTPMethod::HTTP_GET, [server](){
        Serial.println("LOG: [WEB] Запрос страницы /settings.html");
        server->send_P(200, mimeHTML, HTML_settings);
    });

    server->on("/styles.css", HTTPMethod::HTTP_GET, [server](){
        server->send_P(200, mimeCSS, CSS_styles);
    });

    server->on("/main.js", HTTPMethod::HTTP_GET, [server](){
        server->send_P(200, mimeJS, JS_main);
    });
    
    server->on("/datetime", HTTPMethod::HTTP_GET, [server](){
        server->send(200, "text/plain", "Local Mode");
    });

    httpUpdate.setup(server);
    Serial.println("LOG: [APICallback] Все маршруты успешно зарегистрированы.");
}

  // УДАЛЕНЫ ручные вызовы server->on("/settings", ...) — 
  // ConfigManager сам зарегистрирует их автоматически в startApi()!

  httpUpdate.setup(server);
  Serial.println("LOG: [APICallback] Все маршруты успешно зарегистрированы.");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("=========================================");
  Serial.println("Startup Autonomous Mode (с расширенными логами)...");
  Serial.println("=========================================");

  pinMode(4, INPUT_PULLUP);
  pinMode(2, INPUT_PULLUP);
  pinMode(15, INPUT_PULLUP);

  Serial.println("LOG: [SETUP] Инициализация имен ConfigManager...");
  configManager.setAPName("voipdoorbell");
  //configManager.setAPFilename("/index.html");

  Serial.println("LOG: [SETUP] Регистрация параметров в ConfigManager...");
  configManager.addParameter("device_name", config.device_name, 32);
  configManager.addParameter("telnr_1", config.telnr_1, 32);
  configManager.addParameter("telnr_2", config.telnr_2, 32);
  configManager.addParameter("telnr_3", config.telnr_3, 32);
  configManager.addParameter("sip_ip", config.sip_ip, 32);
  configManager.addParameter("sip_user", config.sip_user, 32);
  configManager.addParameter("sip_pass", config.sip_pass, 32);
  configManager.addParameter("mic_gain", &config.mic_gain);
  configManager.addParameter("amp_gain", &config.amp_gain);
  configManager.addParameter("tz_dst", &config.tz_dst);
  configManager.addParameter("tz_std", &config.tz_std);
  configManager.addParameter("lat", &config.lat);
  configManager.addParameter("lon", &config.lon);
  configManager.addParameter("light_r", &config.light_r);
  configManager.addParameter("light_g", &config.light_g);
  configManager.addParameter("light_b", &config.light_b);
  configManager.addParameter("ring_r", &config.ring_r);
  configManager.addParameter("ring_g", &config.ring_g);
  configManager.addParameter("ring_b", &config.ring_b);
  configManager.addParameter("echocompensation", &config.echocompensation);
  configManager.addParameter("echodamping", &config.echodamping);
  configManager.addParameter("echothreshold", &config.echothreshold);
  configManager.addParameter("version", &meta.version, get);
  
  Serial.println("LOG: [SETUP] Установка колбэков ConfigManager...");
  configManager.setAPCallback(APCallback);
  configManager.setAPICallback(APICallback);
  
  Serial.println("LOG: [SETUP] Запуск configManager.begin(). Чтение конфигурации из EEPROM...");
  configManager.begin(config);

  Serial.println("LOG: [SETUP] Текущие значения после чтения из памяти:");
  Serial.printf("  - Device Name: %s\n", config.device_name);
  Serial.printf("  - SIP IP: %s\n", config.sip_ip);
  Serial.printf("  - SIP User: %s\n", config.sip_user);
  Serial.printf("  - Tel 1: %s\n", config.telnr_1);
  Serial.printf("  - Echo Comp: %s\n", config.echocompensation ? "true" : "false");
  Serial.println("LOG: Setup finished successfully!");
}

void loop() {
  configManager.loop();

  if (WiFi.status() == WL_CONNECTED) {
    if (strlen(config.sip_ip) >= 7) {
      if (!doorphonerunning) {
        Serial.print("LOG: [SIP] Target Yate Server IP found: ");
        Serial.println(config.sip_ip);
        Serial.print("LOG: [SIP] Starting SIP module... ");
        
        if (int result = doorphone.begin(config.sip_ip, config.sip_user, config.sip_pass) == VOIPPHONE_OK) {
          doorphone.setAmpGain(config.amp_gain);
          doorphone.setMicGain(config.mic_gain);
          doorphone.setEchoCompensation(config.echocompensation, config.echothreshold, config.echodamping);
          Serial.println("[OK]");
          doorphonerunning = true;
        } else {
          Serial.print("[ERROR CODE:");
          Serial.print(result);
          Serial.println("]");
          delay(5000);
        }
      }

      if (doorphonerunning) {
        doorphone.loop();
      }

      button1.Update();
      button2.Update();
      button3.Update();

      if (button1.clicks == 1) {
        Serial.println("LOG: [BUTTON] Button1 pressed. Dialing via Yate...");
        doorphone.dial(config.telnr_1, config.device_name);
        isbusy = true;
      } else if (button2.clicks == 1) {
        Serial.println("LOG: [BUTTON] Button2 pressed. Dialing via Yate...");
        doorphone.dial(config.telnr_2, config.device_name);
        isbusy = true;
      } else if (button3.clicks == 1) {
        Serial.println("LOG: [BUTTON] Button3 pressed. Dialing via Yate...");
        doorphone.dial(config.telnr_3, config.device_name);
        isbusy = true;
      }
    }
  } else {
    if (doorphonerunning) {
      doorphonerunning = false;
      Serial.println("LOG: [WIFI] Wi-Fi status changed (disconnected).");
    }
  }

  if (isbusy && !doorphone.isBusy()) {
    isbusy = false;
    Serial.println("LOG: [SIP] Call finished / Hangup.");
  }
}