#include "voipphone.h"

Sip *VOIPPhone::sip = NULL;
WiFiUDP VOIPPhone::rtpudp = WiFiUDP();
uint8_t VOIPPhone::amp_gain = AMP_GAIN;
uint8_t VOIPPhone::mic_gain = MIC_GAIN;
bool VOIPPhone::echocompensation = false;
uint8_t VOIPPhone::echodamping = 1;

VOIPPhone::VOIPPhone() {
}

int VOIPPhone::begin(const char *sipip, const char *sipuser, const char *sippasswd) {
  strcpy(myip, WiFi.localIP().toString().c_str());
  int connOK = rtpudp.begin(1234);
  if (connOK == 0) {
    DebugPrintln("rtpudp (port:1234) could not get socket");
    return VOIPPHONE_ERROR_RTPSOCKET;
  } else {
    DebugPrintln("rtpudp listen on (port:1234)");
  }
  DebugPrint("Init SIP...");
  sip = new Sip(caSipOut, sizeof(caSipOut));
  sip->Init(sipip, sipport, myip, sipport, sipuser, sippasswd);
  DebugPrintln("[OK]");
  
  // Единая инициализация аудио на I2S_NUM_0 (Full Duplex)
  int result = initi2samp();
  if (result != VOIPPHONE_OK) {
    return result;
  }
  return VOIPPHONE_OK;
}

//
// Dialing
//

void VOIPPhone::dial(const char *number, const char *id) {
  DebugPrintln("Dialing");
  i2s_start(I2S_NUM_0);
  DebugPrintln("I2S_NUM_0 start");
  rx_streamisrunning = true;
  sip->Dial(number, id, 15);
  DebugPrintln("sip->Dial executed");
}

bool VOIPPhone::isIncomingCall() {
  if (sip != NULL) {
    return sip->isIncomingCall();
  }
  return false;
}

void VOIPPhone::answerIncomingCall() {
  if (sip != NULL) {
    DebugPrintln("Answering incoming call");
    i2s_start(I2S_NUM_0);
    rx_streamisrunning = true;
    sip->AnswerIncomingCall();
    DebugPrintln("sip->AnswerIncomingCall executed");
  }
}

void VOIPPhone::AnswerIncomingCall() {
  answerIncomingCall(); 
}

void VOIPPhone::Bye(int cseq) {
  if (sip != NULL) {
    DebugPrintln("Sending BYE");
    int seq = (cseq != -1) ? cseq : sip->iLastCSeq;
    sip->Bye(seq);
  }
}

void VOIPPhone::bye() {
  Bye(-1);
}

//
// Loop
//

void VOIPPhone::loop() {
  handleSIP();

  static bool lastIncomingState = false;
  bool currentIncoming = sip != NULL ? sip->isIncomingCall() : false;
  if (currentIncoming != lastIncomingState) {
    Serial.printf("DEBUG_VOIP| Incoming call state changed: %s\n", currentIncoming ? "TRUE" : "FALSE");
    lastIncomingState = currentIncoming;
  }

  handleOutgoingRTP();
  handleIncommingRTP();
}

void VOIPPhone::handleSIP() {
  if (sip != NULL) {
    sip->HandleUdpPacket();
  }
}

void VOIPPhone::setMicGain(uint8_t gain) {
  mic_gain = gain;
}

void VOIPPhone::setAmpGain(uint8_t gain) {
  amp_gain = gain;
}

bool VOIPPhone::isBusy(void) {
  return sip->IsBusy();
}

//
// Единая инициализация I2S Full Duplex (микрофон + усилитель на I2S_NUM_0)
//

int VOIPPhone::initi2samp() {
  esp_err_t err;
  
  // Настраиваем пин SD (Shutdown) усилителя как выход и пока выключаем (LOW) для избежания щелчков
  pinMode(AMP_SD_PIN, OUTPUT);
  digitalWrite(AMP_SD_PIN, LOW);

  const i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX), // Прием и передача одновременно
      .sample_rate = 8000, 
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, 
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, 
      .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,    
      .dma_buf_count = 8,                          
      .dma_buf_len = 64,                           
      .use_apll = false
  };

  // Общие линии тактирования (14 и 12) и раздельные линии данных (динамик 27, микрофон 33)
  const i2s_pin_config_t pin_config = {
      .bck_io_num = 14,       // Общий BCLK
      .ws_io_num = 12,        // Общий WS / LRC
      .data_out_num = 27,     // DIN усилителя
      .data_in_num = 33       // DOUT микрофона (INMP441)
  };

  DebugPrintln("Init unified I2S Full Duplex on I2S_NUM_0...");
  err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed installing I2S driver: %d\n", err);
    return VOIPPHONE_ERROR_AMP_I2S_DRIVER;
  }
  
  err = i2s_set_pin(I2S_NUM_0, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Failed setting I2S pins: %d\n", err);
    return VOIPPHONE_ERROR_AMP_PIN;
  }
  
  i2s_zero_dma_buffer(I2S_NUM_0);
  
  // Включаем усилитель через пин SD после успешной инициализации драйвера
  digitalWrite(AMP_SD_PIN, HIGH);
  
  DebugPrintln("Unified I2S Full Duplex initialized successfully.");
  return VOIPPHONE_OK;
}

// Заглушка, чтобы старый код компилятора не ругался на отсутствие initi2smic
int VOIPPhone::initi2smic() {
  return VOIPPHONE_OK; 
}

//
// Read from microphone
//

esp_err_t VOIPPhone::readFromMic(void *dest, size_t size, size_t *bytes_read) {
  return i2s_read(I2S_NUM_0, dest, size, bytes_read, portMAX_DELAY);
}

//
// Write to amplifier (Safe check for NULL and 0 size)
//

esp_err_t VOIPPhone::writeToAmp(const void *src, size_t size, size_t *bytes_written) {
  if (src == nullptr || size == 0) {
    if (bytes_written) *bytes_written = 0;
    return ESP_FAIL;
  }
  return i2s_write(I2S_NUM_0, src, size, bytes_written, portMAX_DELAY);
}

//
// Send RTP packet
//

void VOIPPhone::tx_rtp(){
  static uint32_t last_send_time = 0;
  uint32_t now = millis();
  
  if (last_send_time != 0) {
    uint32_t diff = now - last_send_time;
    if (diff > 10000) {
      last_send_time = now;
    } else if (diff > 35) {
      Serial.printf("DEBUG_RTP| Warning: RTP packet interval too long: %u ms\n", diff);
    }
  }
  last_send_time = now;

  uint8_t temp[80];       // 80 сэмплов для 10 мс пакета
  size_t bytes_read = 0;
  int32_t i2s_buffer[80]; 
  
  static uint16_t sequenceNumber = 0;
  const uint32_t ssrc = 3124150;
  static uint32_t timestamp = 0;
  RTPPacket txRTPpkg;
  uint8_t packetBuffer[255];

  uint32_t t_start = micros();
  // Читаем порциями по 80 сэмплов с таймаутом 10 мс
  esp_err_t result = i2s_read(I2S_NUM_0, i2s_buffer, sizeof(i2s_buffer), &bytes_read, pdMS_TO_TICKS(10));
  uint32_t t_elapsed = micros() - t_start;

  if (t_elapsed > 10000) {
    Serial.printf("DEBUG_MIC| Warning: Slow I2S read: %u us, bytes: %u\n", t_elapsed, bytes_read);
  }

  if (result == ESP_OK && bytes_read > 0) {
    int samples_read = bytes_read / sizeof(int32_t);
    
    for(int i = 0; i < 80; i++) {
      int16_t sample = 0;
      if (i < samples_read) {
        int32_t raw_sample = i2s_buffer[i];
        sample = (int16_t)(raw_sample >> 16); 
      }
      temp[i] = ALaw_Encode(sample);
    }
  } else {
    // Если данные не успели накопиться — заполняем тишиной (A-law silence = 0xD5)
    memset(temp, 0xD5, 80); 
  }

  if(sequenceNumber == 0) {
    timestamp = 0;
  }
  
  txRTPpkg = RTPPacket(temp, sequenceNumber++, ssrc, timestamp += 80, 8, 80);
  int length = txRTPpkg.serialize(packetBuffer);
  
  IPAddress tempip;
  tempip.fromString(sip->GetSIPServerIP()); 
  int beginOK = rtpudp.beginPacket(tempip, atoi(sip->audioport));
  if (beginOK != 0) {
    rtpudp.write(packetBuffer, length);
    rtpudp.endPacket();
  } else {
    Serial.println("DEBUG_NET| Error: rtpudp.beginPacket failed!");
  }
}

//
// Handle Rx RTP packages Port 1234
//

void VOIPPhone::handleIncommingRTP(void) {
  int16_t buffer[500];
  RTPPacket rxRTPpkg;
  
  packetSize = rtpudp.parsePacket();
  if (packetSize > 0 && rx_streamisrunning) {
    packetSize = rtpudp.read(rtpbuffer, sizeof(rtpbuffer));
    
    rxRTPpkg = RTPPacket();
    rtppkgsize = rxRTPpkg.deserialize(rtpbuffer, packetSize);
    
    uint8_t *payload = rxRTPpkg.getPayload();

    if (payload != NULL && rtppkgsize > 0 && rtppkgsize <= 250) {
      for(int i = 0; i < rtppkgsize; i++) {
        buffer[i] = (ALaw_Decode(*(payload + i))) * amp_gain;
        
        if(echocompensation) {
          level = (level + abs(buffer[i])) / 2;
        }
      }
      
      if(echocompensation && (level > ec_threshold_level)) {
        echodamping = echodamping_value;
      } else {
        echodamping = 1;
      }
      
      size_t num_bytes_write = 0;
      size_t bytes_to_write = sizeof(int16_t) * rtppkgsize;
      
      if (bytes_to_write > 0) {
        writeToAmp(buffer, bytes_to_write, &num_bytes_write);
      }
    }
  } else {
    rtpudp.flush();
  }
}

//
// Handle Outgoing RTP stream
//

void VOIPPhone::handleOutgoingRTP(void) {
  if( sip->audioport[0] != '\0' && !tx_streamisrunning ) {
    tx_streamisrunning = true;
    DebugPrint("Starting stream...");
    i2s_start(I2S_NUM_0);
    tx_streamticker.attach_ms(10, VOIPPhone::tx_rtp);
    DebugPrintln("[OK]");
  } else if( sip->audioport[0] == '\0' && tx_streamisrunning ) {
    tx_streamisrunning = false;
    rx_streamisrunning = false;
    rtppkgsize = -1;
    DebugPrintln("Stream stopped.");
    tx_streamticker.detach();
    DebugPrint("Stopping i2s transmission...");
    i2s_stop(I2S_NUM_0);
    DebugPrintln("[OK]");
  }
}

void VOIPPhone::setEchoCompensation(bool enable, long threshold_level, uint8_t damping) {
  echocompensation = enable;
  ec_threshold_level = threshold_level;
  echodamping_value = damping;
}