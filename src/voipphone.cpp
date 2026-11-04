#include "voipphone.h"

//bool DEBUG_MODE = false;
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
  
  // i2s devices initialization
  int result = initi2smic();
  if(result != VOIPPHONE_OK) {
    return result;
  }
  result = initi2samp();
  if(result != VOIPPHONE_OK) {
    return result;
  }
  return VOIPPHONE_OK;
}

//
// Dialing
//

void VOIPPhone::dial(const char *number, const char *id) {
  DebugPrintln("Dialing");
  i2s_start(I2S_PORT1);
  DebugPrintln("I2SPORT1 start");
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
    i2s_start(I2S_PORT1);
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
// Initialize i2s amplifier (MAX98357A на I2S_PORT1)
//

//
// Initialize i2s amplifier (MAX98357A на I2S_PORT1)
//

int VOIPPhone::initi2samp() {
  esp_err_t err;

  // Инициализация пина SD (Shutdown/Mode) для MAX98357A
  pinMode(AMP_SD_PIN, OUTPUT);
  digitalWrite(AMP_SD_PIN, HIGH); // Включаем усилитель (нормальный режим работы)

  const i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = SAMPLE_RATE,                    // 16000 Гц (стабильная частота для железа)
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,  // 16 бит
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, 
      .communication_format = I2S_COMM_FORMAT_STAND_I2S, // Стандартный I2S формат
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 64,
      .use_apll = false,
      .tx_desc_auto_clear = true,                    // Предотвращает треск при паузах
      .fixed_mclk = 0
  };

  const i2s_pin_config_t pin_config = {
      .mck_io_num = I2S_PIN_NO_CHANGE,
      .bck_io_num = AMP_BCLK_PIN,    // Использован макрос из voipphone.h (4)
      .ws_io_num = AMP_LRC_PIN,      // Использован макрос из voipphone.h (5)
      .data_out_num = AMP_DOUT_PIN,  // Использован макрос из voipphone.h (18)
      .data_in_num = -1              // не используется
  };

  DebugPrintln("Init i2s amp...");
  err = i2s_driver_install(I2S_PORT1, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed installing amp driver: %d\n", err);
    return VOIPPHONE_ERROR_AMP_I2S_DRIVER;
  }
  err = i2s_set_pin(I2S_PORT1, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Failed setting amp pin: %d\n", err);
    return VOIPPHONE_ERROR_AMP_PIN;
  }
  i2s_zero_dma_buffer(I2S_PORT1);
  DebugPrintln("I2S amp driver installed.");
  return VOIPPHONE_OK;
}

//
// Initialize i2s microphone (INMP441 на I2S_PORT0)
//

int VOIPPhone::initi2smic() {
  esp_err_t err;

  // Инициализация пина L/R для микрофона INMP441
  pinMode(MIC_LR_PIN, OUTPUT);
  digitalWrite(MIC_LR_PIN, LOW); // LOW = Левый канал

  const i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = SAMPLE_RATE,                    // 16000 Гц
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,  // 16 бит
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S, // Стандартный I2S формат
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 64,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0
  };

  const i2s_pin_config_t pin_config = {
      .mck_io_num = I2S_PIN_NO_CHANGE,
      .bck_io_num = MIC_BCLK_PIN,    // Использован макрос из voipphone.h (14)
      .ws_io_num = MIC_WS_PIN,       // Использован макрос из voipphone.h (15)
      .data_out_num = -1,            // не используется
      .data_in_num = MIC_DIN_PIN     // Использован макрос из voipphone.h (32)
  };

  DebugPrintln("Init i2s mems...");
  err = i2s_driver_install(I2S_PORT0, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed installing mic driver: %d\n", err);
    return VOIPPHONE_ERROR_MIC_I2S_DRIVER;
  }
  err = i2s_set_pin(I2S_PORT0, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Failed setting mic pin: %d\n", err);
    return VOIPPHONE_ERROR_MIC_PIN;
  }
  DebugPrintln("I2S mic driver installed.");
  return VOIPPHONE_OK;
}

//
// Read from microphone
//

esp_err_t VOIPPhone::readFromMic(void *dest, size_t size, size_t *bytes_read) {
  return i2s_read(I2S_PORT0, dest, size, bytes_read, portMAX_DELAY);
}

esp_err_t VOIPPhone::writeToAmp(const void *src, size_t size, size_t *bytes_written) {
  return i2s_write(I2S_PORT1, src, size, bytes_written, portMAX_DELAY);
}

//
// Send RTP packet every 20ms
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
  
  if (last_send_time == 0) {
    last_send_time = now;
  } else {
    last_send_time = now;
  }

  uint8_t temp[160];
  size_t bytes_read = 0;
  int16_t i2s_buffer[160]; 
  
  static uint16_t sequenceNumber = 0;
  const uint32_t ssrc = 3124150;
  static uint32_t timestamp = 0;
  RTPPacket txRTPpkg;
  uint8_t packetBuffer[255];

  esp_err_t result = readFromMic(i2s_buffer, sizeof(i2s_buffer), &bytes_read);

  if (result == ESP_OK && bytes_read > 0) {
    int samples_read = bytes_read / sizeof(int16_t);
    
    for(int i = 0; i < 160; i++) {
      int16_t sample = 0;
      if (i < samples_read) {
        int32_t raw_sample = i2s_buffer[i];
        // Буст микрофона через mic_gain
        raw_sample = raw_sample * mic_gain;
        if (raw_sample > 32767) raw_sample = 32767;
        if (raw_sample < -32768) raw_sample = -32768;
        sample = (int16_t)raw_sample;
      }
      temp[i] = ALaw_Encode(sample);
    }
  } else {
    memset(temp, 0xD5, 160); 
  }

  if(sequenceNumber == 0) {
    timestamp = 0;
  }
  
  txRTPpkg = RTPPacket(temp, sequenceNumber++, ssrc, timestamp += 160, 8, 160);
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
// Handle Rx RTP packages Port 1234 (с апсэмплингом 8кГц -> 16кГц)
//
void VOIPPhone::handleIncommingRTP(void) {
  int16_t upsampled_buffer[500]; 
  RTPPacket rxRTPpkg;
  
  packetSize = rtpudp.parsePacket();
  
  // 1. Отладка: фиксируем сам факт прихода пакета на UDP-порт
  if (packetSize > 0) {
    // Раскомментируйте строку ниже, если хотите видеть каждый пришедший пакет (будет много логов):
    // Serial.printf("DEBUG_RTP_RX| Packet size: %d from %s:%d\n", packetSize, rtpudp.remoteIP().toString().c_str(), rtpudp.remotePort());

    // Если пакет пришел, но мы еще не "в звонке" (флаг сброшен)
    if (!rx_streamisrunning) {
      Serial.println("DEBUG_RTP_RX| WARNING: RTP packet received, but rx_streamisrunning is FALSE (flushed)!");
    }
  }

  if (packetSize > 0 && rx_streamisrunning) {
    packetSize = rtpudp.read(rtpbuffer, sizeof(rtpbuffer));
    rxRTPpkg = RTPPacket();
    rtppkgsize = rxRTPpkg.deserialize(rtpbuffer, packetSize);
    
    // Проверка успешной распаковки RTP
    static uint32_t success_packets = 0;
    success_packets++;
    if (success_packets % 50 == 0) { // Выводим каждые 50 пакетов (~раз в секунду)
      Serial.printf("DEBUG_RTP_RX| Successfully processed %u packets. Payload size: %d\n", success_packets, rtppkgsize);
    }

    int dst_idx = 0;
    for(int i = 0; i < rtppkgsize; i++) {
      int16_t decoded_sample = (ALaw_Decode((*(rxRTPpkg.getPayload() + i)))) * amp_gain;
      
      if(echocompensation) {
        level = (level + abs(decoded_sample)) / 2;
      }
      
      // Апсэмплинг 8кГц -> 16кГц
      upsampled_buffer[dst_idx++] = decoded_sample;
      upsampled_buffer[dst_idx++] = decoded_sample; 
    }
    
    if(echocompensation && (level > ec_threshold_level)) {
      echodamping = echodamping_value;
    } else {
      echodamping = 1;
    }
    
    size_t num_bytes_write;
    esp_err_t write_res = writeToAmp(upsampled_buffer, sizeof(int16_t) * dst_idx, &num_bytes_write);
    
    // Отладка записи в усилитель (если вдруг зависает или возвращает ошибку)
    if (write_res != ESP_OK) {
      Serial.printf("DEBUG_AMP| Error writing to MAX98357A: 0x%x\n", write_res);
    }

  } else if (packetSize > 0) {
    rtpudp.flush();
  }
}

//
// Starting Tx RTP stream if call established
//
void VOIPPhone::handleOutgoingRTP(void) {
  if(sip->audioport[0] != '\0' && !tx_streamisrunning) {
    tx_streamisrunning = true;
    DebugPrint("Starting stream...");
    i2s_start(I2S_PORT0); // mems
    tx_streamticker.attach_ms(20, VOIPPhone::tx_rtp);
    DebugPrintln("[OK]");
  } else if(sip->audioport[0] == '\0' && tx_streamisrunning) {
    tx_streamisrunning = false;
    rx_streamisrunning = false;
    rtppkgsize = -1;
    DebugPrintln("Stream stopped.");
    tx_streamticker.detach();
    DebugPrint("Stopping i2s transmission...");
    i2s_stop(I2S_PORT0); // mems
    i2s_stop(I2S_PORT1); // amp
    DebugPrintln("[OK]");
  }
}

void VOIPPhone::setEchoCompensation(bool enable, long threshold_level, uint8_t damping) {
  echocompensation = enable;
  ec_threshold_level = threshold_level;
  echodamping_value = damping;
}