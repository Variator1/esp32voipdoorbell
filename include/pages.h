#ifndef PAGES_H_
#define PAGES_H_

const char HTML_settings[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="ru">
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="stylesheet" href="styles.css">
    <script src="main.js"></script>
    <title>ConfigManager</title>
  </head>
  <body>
    <div class="container">
      <h1 style="text-align: center;">Settings</h1>
      <form id="settings-form">
        <div class="field-group">
          <label>Device Name</label>
          <input name="device_name" type="text" size="32" data-type="text">
        </div>
        <div class="field-group">
          <label>Telefonnummer 1</label>
          <input name="telnr_1" type="text" size="32" data-type="text">
        </div>
        <div class="field-group">
          <label>Telefonnummer 2</label>
          <input name="telnr_2" type="text" size="32" data-type="text">
        </div>
        <div class="field-group">
          <label>Telefonnummer 3</label>
          <input name="telnr_3" type="text" size="32" data-type="text">
        </div>
        <div class="field-group">
          <label>SIP gateway IP</label>
          <input name="sip_ip" type="text" size="32" data-type="text">
        </div>
        <div class="field-group">
          <label>SIP Benutzer</label>
          <input name="sip_user" type="text" size="32" data-type="text">
        </div>
        <div class="field-group">
          <label>SIP Passwort</label>
          <input name="sip_pass" type="text" size="32" data-type="text">
        </div>
        <div class="field-group">
          <label>Amp gain</label>
          <input name="amp_gain" type="text" size="2" data-type="integer">
        </div>
        <div class="field-group">
          <label>Mic gain</label>
          <input name="mic_gain" type="text" size="2" data-type="integer">
        </div>
        <div class="field-group">
          <label>(DST) Minutes realtive to GMT</label>
          <input name="tz_dst" type="text" size="4" data-type="integer">
        </div>
        <div class="field-group">
          <label>(STD) Minutes realtive to GMT</label>
          <input name="tz_std" type="text" size="4" data-type="integer">
        </div>
        <div class="field-group">
          <label>LAT</label>
          <input name="lat" type="text" size="4" data-type="number">
        </div>
        <div class="field-group">
          <label>LON</label>
          <input name="lon" type="text" size="4" data-type="number">
        </div>
        <div class="field-group">
          <label>Light R</label>
          <input name="light_r" type="text" size="4" data-type="integer">
        </div>
        <div class="field-group">
          <label>Light G</label>
          <input name="light_g" type="text" size="4" data-type="integer">
        </div>
        <div class="field-group">
          <label>Light B</label>
          <input name="light_b" type="text" size="4" data-type="integer">
        </div>
        <div class="field-group">
          <label>Ring R</label>
          <input name="ring_r" type="text" size="4" data-type="integer">
        </div>
        <div class="field-group">
          <label>Ring G</label>
          <input name="ring_g" type="text" size="4" data-type="integer">
        </div>
        <div class="field-group">
          <label>Ring B</label>
          <input name="ring_b" type="text" size="4" data-type="integer">
        </div>
        <div class="field-group">
          <label>Echocompensation</label>
          <input name="echocompensation" type="checkbox" data-type="boolean">
        </div>
        <div class="field-group">
          <label>Echodamping</label>
          <input name="echodamping" type="text" size="4" data-type="integer">
        </div>
        <div class="field-group">
          <label>Echothreshold</label>
          <input name="echothreshold" type="text" size="4" data-type="integer">
        </div>
        <div class="button-container">
          <h2 id="status" style="color: green;"></h2>
          <button type="submit">Save</button>
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

const char JS_main[] PROGMEM = R"=====(
document.addEventListener("DOMContentLoaded", function() {
  // 1. Загружаем текущие настройки при открытии страницы
  fetch('/settings')
    .then(response => response.json())
    .then(data => {
      for (const [key, value] of Object.entries(data)) {
        const input = document.getElementsByName(key)[0];
        if (input) {
          const dataType = input.getAttribute("data-type");
          if (dataType === "boolean") {
            input.checked = Boolean(value);
          } else {
            input.value = value;
          }
        }
      }
    })
    .catch(err => console.error("Error loading settings:", err));

  // 2. Обработка отправки формы методом PUT на /settings
  const form = document.getElementById('settings-form');
  if (form) {
    form.addEventListener('submit', function(e) {
      e.preventDefault();
      document.getElementById("status").innerHTML = "";

      const formData = new FormData(form);
      const data = {};

      formData.forEach((value, key) => {
        const input = document.getElementsByName(key)[0];
        if (input) {
          const dataType = input.getAttribute("data-type");
          if (dataType === "integer") value = parseInt(value) || 0;
          if (dataType === "number") value = parseFloat(value) || 0;
        }
        data[key] = value;
      });

      // Обработка чебоксов (если выключены, в formData они не попадают)
      document.querySelectorAll('input[type=checkbox]').forEach(cb => {
        data[cb.name] = cb.checked;
      });

      // Отправка PUT-запроса
      fetch('/settings', {
        method: 'PUT',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(data)
      })
      .then(response => {
        if (response.ok || response.status === 204) {
          document.getElementById("status").innerHTML = "Settings Updated";
          console.log("Settings successfully saved:", data);
        } else {
          document.getElementById("status").innerHTML = "Error saving!";
        }
      })
      .catch(err => {
        console.error("Fetch error:", err);
        document.getElementById("status").innerHTML = "Connection error!";
      });
    });
  }
});
)=====";

const char CSS_styles[] PROGMEM = R"=====(
body {
    color: #434343;
    font-family: "Helvetica Neue",Helvetica,Arial,sans-serif;
    font-size: 14px;
    line-height: 1.42857142857143;
    padding: 20px;
}
.container {
    margin: 0 auto;
    max-width: 400px;
}
form .error {
    color: #8A1F11 !important;
    border-color: #8A1F11;
}
label.error {
    text-transform: none;
}
p.error {
    margin-bottom: 10px;
}
form .field-group {
    box-sizing: border-box;
    clear: both;
    padding: 4px 0;
    position: relative;
    margin: 1px 0;
    width: 100%;
}
form .field-group > label {
    color: #757575;
    display: block;
    margin: 0 0 5px 0;
    padding: 5px 0 0;
    position: relative;
    word-wrap: break-word;
}
input[type=text] {
    background: #fff;
    border: 1px solid #d0d0d0;
    border-radius: 2px;
    box-sizing: border-box;
    color: #434343;
    font-family: inherit;
    font-size: inherit;
    height: 2.14285714em;
    line-height: 1.4285714285714;
    padding: 4px 5px;
    margin: 0;
    width: 100%;
}
input[type=text]:focus {
    border-color: #4C669F;
    outline: 0;
}
.button-container {
    box-sizing: border-box;
    clear: both;
    margin: 1px 0 0;
    padding: 4px 0;
    position: relative;
    width: 100%;
}
button[type=submit] {
    box-sizing: border-box;
    background: #f5f5f5;
    border: 1px solid #bdbdbd;
    border-radius: 2px;
    color: #434343;
    cursor: pointer;
    display: inline-block;
    font-family: inherit;
    font-size: 14px;
    height: 2.14285714em;
    line-height: 1.42857143;
    margin: 0;
    padding: 4px 10px;
    text-decoration: none;
    vertical-align: baseline;
    white-space: nowrap;
}
)=====";
#endif