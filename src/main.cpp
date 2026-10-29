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

bool isbusy=false;

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

void APCallback(WebServer *server) {
    server->on("/styles.css", HTTPMethod::HTTP_GET, [server](){
        server->send_P(200,mimeCSS,CSS_styles);
    });
    Serial.println("LOG: AP Mode Callback triggered.");
}

void APICallback(WebServer *server) {
  server->on("/disconnect", HTTPMethod::HTTP_GET, [server](){
    configManager.clearWifiSettings(false);
  });

  server->on("/settings.html", HTTPMethod::HTTP_GET, [server](){
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

  // ФИНАЛЬНЫЙ ИСПРАВЛЕННЫЙ ОБРАБОТЧИК СОХРАНЕНИЯ
  server->on("/config", HTTPMethod::HTTP_POST, [server](){
    Serial.println("LOG: Получен запрос на сохранение настроек...");

    // Ручной перехват текстовых полей (проверяем все возможные варианты имен из HTML)
    if (server->hasArg("device_name")) strncpy(config.device_name, server->arg("device_name").c_str(), 32);
    
    if (server->hasArg("telnr_1")) strncpy(config.telnr_1, server->arg("telnr_1").c_str(), 32);
    if (server->hasArg("telnr_2")) strncpy(config.telnr_2, server->arg("telnr_2").c_str(), 32);
    if (server->hasArg("telnr_3")) strncpy(config.telnr_3, server->arg("telnr_3").c_str(), 32);
    
    if (server->hasArg("sip_ip"))  strncpy(config.sip_ip, server->arg("sip_ip").c_str(), 32);
    if (server->hasArg("sip_gateway")) strncpy(config.sip_ip, server->arg("sip_gateway").c_str(), 32); // альтернативное имя
    
    if (server->hasArg("sip_user")) strncpy(config.sip_user, server->arg("sip_user").c_str(), 32);
    if (server->hasArg("sip_user_name")) strncpy(config.sip_user, server->arg("sip_user_name").c_str(), 32);
    
    if (server->hasArg("sip_pass")) strncpy(config.sip_pass, server->arg("sip_pass").c_str(), 32);
    if (server->hasArg("sip_password")) strncpy(config.sip_pass, server->arg("sip_password").c_str(), 32);

    // Ручной перехват настроек звука
    if (server->hasArg("amp_gain")) config.amp_gain = server->arg("amp_gain").toInt();
    if (server->hasArg("mic_gain")) config.mic_gain = server->arg("mic_gain").toInt();
    
    if (server->hasArg("echocompensation")) {
      config.echocompensation = (server->arg("echocompensation") == "true" || server->arg("echocompensation") == "1");
    } else {
      config.echocompensation = false;
    }

    // Принудительно сохраняем обновленную структуру в энергонезависимую память через ConfigManager
    configManager.save(); 

    // Отправляем красивый HTML-ответ в браузер
    String response = "<html><head><meta charset='utf-8'><style>body{font-family:sans-serif;text-align:center;padding-top:50px;}</style></head>";
    response += "<body><h2>Настройки SIP успешно сохранены!</h2>";
    response += "<p>Плата перезагружается. Через 10 секунд она зарегистрируется на Yate-сервере: <b>" + String(config.sip_ip) + "</b></p></body></html>";
    server->send(200, "text/html", response);
    
    Serial.println("LOG: Настройки сохранены. Перезапуск системы...");
    delay(2000);
    ESP.restart();
  });

  httpUpdate.setup(server);
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Startup Autonomous Mode...");

  pinMode(4, INPUT_PULLUP);
  pinMode(2, INPUT_PULLUP);
  pinMode(15, INPUT_PULLUP);

  Serial.println("LOG: Pinmodes set, cleaning config strings...");
  // Жестко обнуляем массивы памяти, чтобы ConfigManager не читал мусор из NVS
  //memset(&config, 0, sizeof(Config));

  strcpy(config.device_name, "Doorbell");
  strcpy(config.telnr_1, "100");
  strcpy(config.telnr_2, "101");
  strcpy(config.telnr_3, "102");
  strcpy(config.sip_ip, "192.168.1.10"); // Дефолтный IP вашего Yate-сервера
  strcpy(config.sip_user, "doorphone");
  strcpy(config.sip_pass, "password");

  meta.version = 1;
  config.tz_dst = 120;
  config.tz_std = 60;
  
  Serial.println("LOG: Initializing ConfigManager names...");
  configManager.setAPName("voipdoorbell");
  configManager.setAPFilename("/index.html");
  
  Serial.println("LOG: Adding parameters to ConfigManager...");
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
  
  Serial.println("LOG: Setting ConfigManager callbacks...");
  configManager.setAPCallback(APCallback);
  configManager.setAPICallback(APICallback);
  
  Serial.println("LOG: Running configManager.begin()...");
  configManager.begin(config);

  Serial.println("LOG: Setup finished successfully!");
}

void loop() {
  // Менеджер веб-страниц должен крутиться ВСЕГДА на первой строчке
  configManager.loop();

  // Если Wi-Fi подключен к роутеру (IP: 192.168.50.4)
  if (WiFi.status() == WL_CONNECTED) {
    
    // ЗАЩИТА: Запускаем SIP только если IP-адрес сервера Yate БЫЛ ИЗМЕНЕН в настройках 
    // (не равен дефолтному "192.168.1.10" и строка заполнена)
    if (strlen(config.sip_ip) >= 7 && strcmp(config.sip_ip, "192.168.1.10") != 0) {
      
      if (!doorphonerunning) {
        Serial.print("LOG: Valid Yate Server IP found! Starting SIP module...");
        if (int result = doorphone.begin(config.sip_ip, config.sip_user, config.sip_pass) == VOIPPHONE_OK) {
          Serial.println("[OK]");
          doorphonerunning = true;
        } else {
          Serial.print("[ERROR CODE:");
          Serial.print(result);
          Serial.println("]");
          delay(5000);
        }
      }

      // Голосовой движок обрабатываем только если SIP успешно запущен
      if (doorphonerunning) {
        doorphone.loop();
      }

      // Опрос кнопок
      button1.Update();
      button2.Update();
      button3.Update();

      if (button1.clicks == 1) {
        Serial.println("Button1 pressed. Dialing via Yate...");
        doorphone.dial(config.telnr_1, config.device_name);
        isbusy = true;
      } else if (button2.clicks == 1) {
        Serial.println("Button2 pressed. Dialing via Yate...");
        doorphone.dial(config.telnr_2, config.device_name);
        isbusy = true;
      } else if (button3.clicks == 1) {
        Serial.println("Button3 pressed. Dialing via Yate...");
        doorphone.dial(config.telnr_3, config.device_name);
        isbusy = true;
      }
    }
  } else {
    if (doorphonerunning) {
      doorphonerunning = false;
      Serial.println("LOG: Wi-Fi status changed (disconnected).");
    }
  }

  if (isbusy && !doorphone.isBusy()) {
    isbusy = false;
    Serial.println("LOG: Call finished / Hangup.");
  }
}

