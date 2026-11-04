#ifndef _VOIPPHONE_h_
#define _VOIPPHONE_h_

#include <Arduino.h>
#include <driver/i2s.h>
#include <Ticker.h>
#include "sip.h"
#include "RTPPacket.h"
#include "g711.h"

extern bool DEBUG_MODE;

#define DebugPrintln(a) (DEBUG_MODE ? Serial.println(a) : false)
#define DebugPrint(a) (DEBUG_MODE ? Serial.print(a) : false)

// --- НАСТРОЙКИ ПИНОВ И ЧАСТОТЫ (вынесены для удобства) ---
#define SAMPLE_RATE         16000 // Ставим 16 кГц для стабильности I2S (как в рабочем тесте)

// Пины микрофона INMP441
#define MIC_BCLK_PIN        14
#define MIC_WS_PIN          15
#define MIC_DIN_PIN         32
#define MIC_LR_PIN          23 // LOW = Left channel

// Пины усилителя MAX98357A
#define AMP_BCLK_PIN        4
#define AMP_LRC_PIN         5
#define AMP_DOUT_PIN        18
#define AMP_SD_PIN          19 // HIGH = Normal operation (not shutdown)

#define MIC_GAIN            2
#define AMP_GAIN            6

#define VOIPPHONE_OK                1
#define VOIPPHONE_ERROR_RTPSOCKET      -1
#define VOIPPHONE_ERROR_AMP_I2S_DRIVER -2
#define VOIPPHONE_ERROR_AMP_PIN        -3
#define VOIPPHONE_ERROR_MIC_I2S_DRIVER -4
#define VOIPPHONE_ERROR_MIC_PIN        -5

class VOIPPhone {
  public:
    VOIPPhone();
    int begin(const char *sipip, const char *sipuser, const char *sippasswd);
    void dial(const char *number, const char *id);
    void loop(void);
    void setMicGain(uint8_t gain);
    void setAmpGain(uint8_t gain);
    bool isBusy(void);
    void setEchoCompensation(bool enable, long threshold_level, uint8_t damping);
    
    bool isIncomingCall();
    void AnswerIncomingCall();
    void answerIncomingCall(); 
    void Bye(int cseq = -1);    
    void bye();               

  private:
    int initi2samp(void);
    int initi2smic(void);
    static esp_err_t readFromMic(void *dest, size_t size, size_t *bytes_read);
    esp_err_t writeToAmp(const void *src, size_t size, size_t *bytes_written);
    static void tx_rtp(void);
    void handleIncommingRTP(void);
    void handleOutgoingRTP(void);
    void handleSIP(void);

    static const i2s_port_t I2S_PORT0 = I2S_NUM_0; // mems
    const i2s_port_t I2S_PORT1 = I2S_NUM_1;        // amplifier

    static WiFiUDP rtpudp;
   
    char caSipOut[2048];
    uint8_t rtpbuffer[2048];
    static Sip *sip;
    static uint8_t amp_gain;
    static uint8_t mic_gain;
    static uint8_t echodamping;
    static bool echocompensation;
    long level;
    long ec_threshold_level;
    uint8_t echodamping_value;
    Ticker tx_streamticker;
    bool tx_streamisrunning = false;
    bool rx_streamisrunning = false;
    int rtppkgsize = -1;
    size_t packetSize;
    
    // sip params
    int sipport = 5060;
    char myip[18];
};

#endif