#ifndef PAGES_H_
#define PAGES_H_

const char HTML_settings[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="ru">
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="stylesheet" href="styles.css">
    <title>ConfigManager</title>
  </head>
  <body>
    <div class="container">
      <h1 style="text-align: center;">Settings</h1>
      <!-- ИСПРАВЛЕНО: action теперь ведет строго на наш рабочий адрес /set -->
      <form method="post" action="/set">
        <div class="field-group">
          <label>Device Name</label>
          <input name="device_name" type="text" size="32">
        </div>
        <div class="field-group">
          <label>Telefonnummer 1</label>
          <input name="telnr_1" type="text" size="32">
        </div>
        <div class="field-group">
          <label>Telefonnummer 2</label>
          <input name="telnr_2" type="text" size="32">
        </div>
        <div class="field-group">
          <label>Telefonnummer 3</label>
          <input name="telnr_3" type="text" size="32">
        </div>
        <div class="field-group">
          <label>SIP gateway IP</label>
          <input name="sip_ip" type="text" size="32">
        </div>
        <div class="field-group">
          <label>SIP Benutzer</label>
          <input name="sip_user" type="text" size="32">
        </div>
        <div class="field-group">
          <label>SIP Passwort</label>
          <input name="sip_pass" type="text" size="32">
        </div>
        <div class="field-group">
          <label>Amp gain</label>
          <input name="amp_gain" type="text" size="2">
        </div>
        <div class="field-group">
          <label>Mic gain</label>
          <input name="mic_gain" type="text" size="2">
        </div>
        <div class="field-group">
          <label>(DST) Minutes realtive to GMT</label>
          <input name="tz_dst" type="text" size="4">
        </div>
        <div class="field-group">
          <label>(STD) Minutes realtive to GMT</label>
          <input name="tz_std" type="text" size="4">
        </div>
        <div class="field-group">
          <label>Echocompensation</label>
          <input name="echocompensation" type="checkbox">
        </div>
        <div class="field-group">
          <label>Echodamping</label>
          <input name="echodamping" type="text" size="4">
        </div>
        <div class="field-group">
          <label>Echothreshold</label>
          <input name="echothreshold" type="text" size="4">
        </div>
        <div class="button-container">
          <button type="submit" style="background:#28a745; color:#fff; border:1px solid #218838; width:100%; height:40px; font-size:16px; border-radius:4px; cursor:pointer;">Save Settings</button>
        </div>
      </form>
    </div>
  </body>
</html>
)=====";

const char HTML_index[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="ru">
    <head>
        <meta charset="utf-8">
        <link rel="stylesheet" href="styles.css">
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <title>ConfigManager</title>
    </head>
    <body>
        <div class="container">
            <h1 style="text-align: center;">Wifi Details</h1>
            <form method="post" action="/">
                <div class="field-group">
                    <label>SSID</label>
                    <input name="ssid" type="text" size="32">
                </div>
                <div class="field-group">
                    <label>Password</label>
                    <input name="password" type="text" size="64">
                </div>
                <div class="button-container">
                    <button type="submit">Save</button>
                </div>
            </form>
        </div>
    </body>
</html>
)=====";

// Скрипт main.js нам больше не нужен, оставляем его пустым заглушкой, чтобы не переписывать инклуды
const char JS_main[] PROGMEM = R"=====(
// Чистый JS используется напрямую в HTML
)=====";

const char CSS_styles[] PROGMEM = R"=====(
body {
    color: #434343;
    font-family: "Helvetica Neue",Helvetica,Arial,sans-serif;
    font-size: 14px;
    line-height: 1.42857142857143;
    padding: 20px;
    background: #f4f4f4;
}
.container {
    margin: 0 auto;
    max-width: 400px;
    background: #fff;
    padding: 20px;
    border-radius: 8px;
    box-shadow: 0 2px 10px rgba(0,0,0,0.1);
}
form .field-group {
    box-sizing: border-box;
    clear: both;
    padding: 4px 0;
    position: relative;
    margin: 5px 0;
    width: 100%;
}
form .field-group > label {
    color: #757575;
    display: block;
    margin: 0 0 5px 0;
    padding: 5px 0 0;
    position: relative;
}
input[type=text], input[type=password] {
    background: #fff;
    border: 1px solid #d0d0d0;
    border-radius: 2px;
    box-sizing: border-box;
    color: #434343;
    font-family: inherit;
    font-size: inherit;
    height: 35px;
    padding: 4px 8px;
    margin: 0;
    width: 100%;
}
input[type=text]:focus, input[type=password]:focus {
    border-color: #4C669F;
    outline: 0;
}
.button-container {
    box-sizing: border-box;
    clear: both;
    margin: 15px 0 0;
    width: 100%;
}
)=====";
#endif
