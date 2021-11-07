#ifndef H_KWHELTECWIFIKIT32
#define H_KWHELTECWIFIKIT32

#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"
#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <TimeLib.h>
#include <WiFiMulti.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <esp32-hal.h>

// OLED pins
#define I2C_ADDRESS 0x3C
#define PIN_RST     16
#define PIN_SDA     4
#define PIN_SCL     15

struct accessPoint {
  char* ssid;
  char* pwd;
};

struct HeltecConfig {
  accessPoint ap1;
  accessPoint ap2;
  accessPoint ap3;
  bool        rotateDisplay;
  const char* firmwareVersion;
};

struct dataField {
  std::string topicString;
  std::string fieldName;
  std::string units;
};

class kwHeltecWifikit32 {
 public:
  kwHeltecWifikit32( HeltecConfig config );

  // Initialise
  void init();

  // WiFi / MQTT methods
  void publish( uint8_t fieldID, uint16_t data );
  void publish( uint8_t fieldID, float data );

  // Real Time Clock methods
  bool isMidnight();

  // Display methods
  void update( uint8_t fieldID, uint16_t data );
  void update( uint8_t fieldID, float data );
  void update( uint8_t fieldID, const char* message );

  void run();

  char deviceID[16] = { 0 };

 private:
  HeltecConfig config;
  void         getMacAddress();
  bool         initWiFi();
  void         initTime();
  void         updateSystemStatus( std::string statusMessage );
  void         setUpForm();
};

void   initDisplay( bool isRotated );
void   clearValue( uint8_t row );
time_t getNtpTime();
void   sendNTPpacket( IPAddress& address );
void   onWsEvent( AsyncWebSocket* server, AsyncWebSocketClient* client,
                  AwsEventType type, void* arg, uint8_t* data, size_t len );

#endif