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
  
  // Инициализация двух независимых I2S портов
  int result = initi2smic();
  if (result != VOIPPHONE_OK) return result;

  result = initi2samp();
  if (result != VOIPPHONE_OK) return result;

  return VOIPPHONE_OK;
}

//
// Dialing
//

void VOIPPhone::dial(const char *number, const char *id) {
  DebugPrintln("Dialing");
  i2s_start(I2S_NUM_0);
  i2s_start(I2S_NUM_1);
  DebugPrintln("I2S ports started");
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
    i2s_start(I2S_NUM_1);
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
// Инициализация микрофона INMP441 на I2S_NUM_0 (16 кГц)
//

int VOIPPhone::initi2smic() {
  esp_err_t err;

  pinMode(MIC_LR_PIN, OUTPUT);
  digitalWrite(MIC_LR_PIN, LOW); 

  const i2s_config_t mic_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 128,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  const i2s_pin_config_t mic_pins = {
    .mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = MIC_BCLK,
    .ws_io_num = MIC_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = MIC_DIN
  };

  DebugPrintln("Init Microphone INMP441 on I2S_NUM_0...");
  err = i2s_driver_install(I2S_NUM_0, &mic_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed installing mic I2S driver: %d\n", err);
    return VOIPPHONE_ERROR_MIC_I2S_DRIVER;
  }
  
  err = i2s_set_pin(I2S_NUM_0, &mic_pins);
  if (err != ESP_OK) {
    Serial.printf("Failed setting mic I2S pins: %d\n", err);
    return VOIPPHONE_ERROR_MIC_PIN;
  }
  
  i2s_start(I2S_NUM_0);
  DebugPrintln("Microphone initialized successfully.");
  return VOIPPHONE_OK;
}

//
// Инициализация усилителя MAX98357A на I2S_NUM_1 (16 кГц)
//

int VOIPPhone::initi2samp() {
  esp_err_t err;
  
  pinMode(AMP_SD_PIN, OUTPUT);
  digitalWrite(AMP_SD_PIN, LOW);

  const i2s_config_t amp_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 128,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  const i2s_pin_config_t amp_pins = {
    .mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = AMP_BCLK,
    .ws_io_num = AMP_LRC,
    .data_out_num = AMP_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  DebugPrintln("Init Amplifier MAX98357A on I2S_NUM_1...");
  err = i2s_driver_install(I2S_NUM_1, &amp_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed installing amp I2S driver: %d\n", err);
    return VOIPPHONE_ERROR_AMP_I2S_DRIVER;
  }
  
  err = i2s_set_pin(I2S_NUM_1, &amp_pins);
  if (err != ESP_OK) {
    Serial.printf("Failed setting amp I2S pins: %d\n", err);
    return VOIPPHONE_ERROR_AMP_PIN;
  }
  
  i2s_zero_dma_buffer(I2S_NUM_1);
  i2s_start(I2S_NUM_1);
  digitalWrite(AMP_SD_PIN, HIGH);
  
  DebugPrintln("Amplifier initialized successfully.");
  return VOIPPHONE_OK;
}

//
// Read from microphone (I2S_NUM_0)
//

esp_err_t VOIPPhone::readFromMic(void *dest, size_t size, size_t *bytes_read) {
  return i2s_read(I2S_NUM_0, dest, size, bytes_read, 0);
}

//
// Write to amplifier (I2S_NUM_1)
//

esp_err_t VOIPPhone::writeToAmp(const void *src, size_t size, size_t *bytes_written) {
  if (src == nullptr || size == 0) {
    if (bytes_written) *bytes_written = 0;
    return ESP_FAIL;
  }
  return i2s_write(I2S_NUM_1, src, size, bytes_written, 0);
}

//
// Send RTP packet (16kHz -> downsample to 8kHz)
//

void VOIPPhone::tx_rtp(){
  int32_t i2s_buffer[320]; 
  size_t bytes_read = 0;
  uint8_t temp[160]; 
  
  static uint16_t sequenceNumber = 0;
  const uint32_t ssrc = 3124150;
  static uint32_t timestamp = 0;
  RTPPacket txRTPpkg;
  uint8_t packetBuffer[255];

  esp_err_t result = i2s_read(I2S_NUM_0, i2s_buffer, sizeof(i2s_buffer), &bytes_read, 0);

  if (result == ESP_OK && bytes_read > 0) {
    int samples_read = bytes_read / sizeof(int32_t);
    int dst_idx = 0;
    
    for (int i = 0; i < samples_read && dst_idx < 160; i += 2) {
      int32_t raw_sample = i2s_buffer[i];
      int16_t sample = (int16_t)(raw_sample >> 16); 
      
      int32_t amplified = sample * mic_gain;
      if (amplified > 32767) amplified = 32767;
      if (amplified < -32768) amplified = -32768;

      temp[dst_idx++] = ALaw_Encode((int16_t)amplified);
    }
    
    while (dst_idx < 160) {
      temp[dst_idx++] = 0xD5;
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
  }
}

//
// Handle Rx RTP packages Port 1234 (8kHz -> upsample to 16kHz for I2S_NUM_1)
//

void VOIPPhone::handleIncommingRTP(void) {
  int16_t decoded_8khz[250];      
  int16_t upsampled_16khz[500];   
  RTPPacket rxRTPpkg;
  
  packetSize = rtpudp.parsePacket();
  if (packetSize > 0 && rx_streamisrunning) {
    packetSize = rtpudp.read(rtpbuffer, sizeof(rtpbuffer));
    
    rxRTPpkg = RTPPacket();
    rtppkgsize = rxRTPpkg.deserialize(rtpbuffer, packetSize);
    
    uint8_t *payload = rxRTPpkg.getPayload();

    if (payload != NULL && rtppkgsize > 0 && rtppkgsize <= 250) {
      int dst_idx = 0;
      
      for(int i = 0; i < rtppkgsize; i++) {
        decoded_8khz[i] = (ALaw_Decode(*(payload + i))) * amp_gain;
        
        if(echocompensation) {
          level = (level + abs(decoded_8khz[i])) / 2;
        }

        upsampled_16khz[dst_idx++] = decoded_8khz[i];
        upsampled_16khz[dst_idx++] = decoded_8khz[i];
      }
      
      if(echocompensation && (level > ec_threshold_level)) {
        echodamping = echodamping_value;
      } else {
        echodamping = 1;
      }
      
      size_t num_bytes_write = 0;
      size_t bytes_to_write = sizeof(int16_t) * dst_idx;
      
      if (bytes_to_write > 0) {
        writeToAmp(upsampled_16khz, bytes_to_write, &num_bytes_write);
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
    i2s_start(I2S_NUM_1);
    tx_streamticker.attach_ms(20, VOIPPhone::tx_rtp);
    DebugPrintln("[OK]");
  } else if( sip->audioport[0] == '\0' && tx_streamisrunning ) {
    tx_streamisrunning = false;
    rx_streamisrunning = false;
    rtppkgsize = -1;
    DebugPrintln("Stream stopped.");
    tx_streamticker.detach();
    DebugPrint("Stopping i2s transmission...");
    i2s_stop(I2S_NUM_0);
    i2s_stop(I2S_NUM_1);
    DebugPrintln("[OK]");
  }
}

void VOIPPhone::setEchoCompensation(bool enable, long threshold_level, uint8_t damping) {
  echocompensation = enable;
  ec_threshold_level = threshold_level;
  echodamping_value = damping;
}