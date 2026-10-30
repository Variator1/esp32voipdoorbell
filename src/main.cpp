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
    Serial.println("LOG: Получен веб-запрос /disconnect");
    configManager.clearWifiSettings(false);
  });

  // 1. Обработчик для открытия страницы настроек по адресу http://192.168.50
  server->on("/set", HTTPMethod::HTTP_GET, [server](){
    Serial.println("LOG: Открываем кастомное окно настроек по адресу /set...");
    server->send_P(200, mimeHTML, HTML_settings);
  });

  // 2. Исправленный обработчик: ловит данные формы методом POST на адрес /set, как прописано в pages.h
  server->on("/set", HTTPMethod::HTTP_POST, [server](){
    Serial.println("LOG: [WEB-POST] Получены новые настройки формы со страницы /set!");

    // Считываем все текстовые поля напрямую по их именам из HTML-формы
    if (server->hasArg("device_name")) strncpy(config.device_name, server->arg("device_name").c_str(), 32);
    if (server->hasArg("telnr_1")) strncpy(config.telnr_1, server->arg("telnr_1").c_str(), 32);
    if (server->hasArg("telnr_2")) strncpy(config.telnr_2, server->arg("telnr_2").c_str(), 32);
    if (server->hasArg("telnr_3")) strncpy(config.telnr_3, server->arg("telnr_3").c_str(), 32);
    if (server->hasArg("sip_ip"))   strncpy(config.sip_ip, server->arg("sip_ip").c_str(), 32);
    if (server->hasArg("sip_user")) strncpy(config.sip_user, server->arg("sip_user").c_str(), 32);
    if (server->hasArg("sip_pass")) strncpy(config.sip_pass, server->arg("sip_pass").c_str(), 32);

    // Считываем числовые поля
    if (server->hasArg("amp_gain")) config.amp_gain = server->arg("amp_gain").toInt();
    if (server->hasArg("mic_gain")) config.mic_gain = server->arg("mic_gain").toInt();
    if (server->hasArg("tz_dst"))   config.tz_dst = server->arg("tz_dst").toInt();
    if (server->hasArg("tz_std"))   config.tz_std = server->arg("tz_std").toInt();
    if (server->hasArg("echodamping"))   config.echodamping = server->arg("echodamping").toInt();
    if (server->hasArg("echothreshold")) config.echothreshold = server->arg("echothreshold").toInt();

    // Считываем логический чекбокс эхоподавления
    if (server->hasArg("echocompensation")) {
      config.echocompensation = true;
    } else {
      config.echocompensation = false;
    }

    // Принудительно записываем обновленную структуру config во Flash-память прибора
    configManager.save(); 
    Serial.println("LOG: Настройки успешно сохранены в постоянную память (NVS).");

    // Отвечаем браузеру обычным текстом "OK", чтобы скрипт jQuery вывел статус "Settings Updated"
    server->send(200, "text/plain", "OK");
    
    Serial.println("LOG: Перезапуск платы через 2 секунды для запуска телефонии...");
    delay(2000);
    ESP.restart();
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
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Startup Autonomous Mode...");

  pinMode(4, INPUT_PULLUP);
  pinMode(2, INPUT_PULLUP);
  pinMode(15, INPUT_PULLUP);

  Serial.println("LOG: Initializing ConfigManager names...");
  configManager.setAPName("voipdoorbell");
  configManager.setAPFilename("/index.html");

  // БЛОК ДЕФОЛТНЫХ НАСТРОЕК ТЕПЕРЬ ПОЛНОСТЬЮ ЗАКОММЕНТИРОВАН!
  // Память не будет затираться при перезагрузках.
  /*
  strcpy(config.device_name, "Doorbell");
  strcpy(config.telnr_1, "100");
  strcpy(config.telnr_2, "101");
  strcpy(config.telnr_3, "102");
  strcpy(config.sip_ip, "192.168.1.10");
  strcpy(config.sip_user, "doorphone");
  strcpy(config.sip_pass, "password");
  meta.version = 1;
  config.tz_dst = 120;
  config.tz_std = 60;
  */
  
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
    
    // Безопасный запуск: активируем SIP только если IP-адрес задан (не пустой)
    if (strlen(config.sip_ip) >= 7) {
      
      if (!doorphonerunning) {
        Serial.print("LOG: Target Yate Server IP found: ");
        Serial.println(config.sip_ip);
        Serial.print("LOG: Starting SIP module...");
        
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

      // Опрос кнопок звонка
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
