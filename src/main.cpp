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
    server->send_P(200,mimeHTML,HTML_settings);
  });

  server->on("/styles.css", HTTPMethod::HTTP_GET, [server](){
    server->send_P(200,mimeCSS,CSS_styles);
  });

  server->on("/main.js", HTTPMethod::HTTP_GET, [server](){
    server->send_P(200,mimeJS,JS_main);
  });
  
  server->on("/datetime", HTTPMethod::HTTP_GET, [server](){
    server->send(200, "text/plain", "Local Mode (No Internet)");
  });

  server->on("/restart", HTTPMethod::HTTP_GET, [server](){
    ESP.restart();
    server->send(200, "text/plain", "restarting...");
  });

  server->on("/dial1", HTTPMethod::HTTP_GET, [server](){
    doorphone.dial(config.telnr_1,config.device_name);
    server->send(200, "text/plain", "ok");
  });

  server->on("/dial2", HTTPMethod::HTTP_GET, [server](){
    doorphone.dial(config.telnr_2,config.device_name);
    server->send(200, "text/plain", "ok");
  });

  server->on("/dial3", HTTPMethod::HTTP_GET, [server](){
    doorphone.dial(config.telnr_3,config.device_name);
    server->send(200, "text/plain", "ok");
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
  memset(&config, 0, sizeof(Config));

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
  // Вызываем loop только тогда, когда Wi-Fi стек стабилен
  if (WiFi.status() == WL_CONNECTED || WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    configManager.loop();
  }
  
  // Добавляем микро-паузу, чтобы фоновые потоки RTOS успевали инициализировать указатели
  delay(10);
}
