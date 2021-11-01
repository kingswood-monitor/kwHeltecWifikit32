#ifndef H_KWHELTECWIFIKIT32
#define H_KWHELTECWIFIKIT32

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <RTClib.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"

// OLED pins
#define I2C_ADDRESS 0x3C
#define PIN_RST 16
#define PIN_SDA 4
#define PIN_SCL 15

// CHECK THESE !!!     <-----------------------------
#define MQTT_SOCKET_TIMEOUT 1
#define MQTT_RECONNECT_TIME_SECONDS 1

struct HeltecConfig
{
  const char *ssid;
  const char *pwd;
  IPAddress mqtt_host;
  bool rotateDisplay;
  const char *firmwareVersion;
  std::string topicRoot;
};

struct dataField
{
  std::string topicString;
  std::string fieldName;
  std::string units;
};

class kwHeltecWifikit32 
{
public:

  kwHeltecWifikit32( HeltecConfig config );
  
  // Data field methods
  uint8_t registerField(std::string fieldName, std::string units, std::string topicName, std::string sensorName);
  uint8_t registerMetaTopic(std::string topicName);
  
  // Initialise
  void init();
  
  // WiFi / MQTT methods
  void publish(uint8_t fieldID, uint16_t data);
  void publish(uint8_t fieldID, float data);
  
  // Real Time Clock methods
  bool isMidnight();
  
  // Display methods
  void update( uint8_t fieldID, uint16_t data );
  void update( uint8_t fieldID, float data );
  void update( uint8_t fieldID, const char* message );

  void run();
  
  char deviceID[16] = {0};
  std::vector<dataField> dataTopics;
  std::vector<std::string> metaTopics;
  std::vector<std::string> commands;
  uint8_t statusTopicID;
  
  bool hasRTC = false;       // true if an RTC module is present
  bool rtcWasAdjusted = false; // true if the RTC has been initialised with a time

private:

  HeltecConfig config;
  void getMacAddress();
  bool initWiFi(const char* SSID, const char* PWD);
  void initMTTQ(IPAddress mqtt_host);
  void updateSystemStatus(std::string statusMessage);
  static void mqttCallback(char* topic, byte* payload, unsigned int length);
  boolean mqttReconnect();
  void setUpForm();

  bool didInitialiseMTTQ = false;
  long lastReconnectAttempt = 0;
  uint8_t maxRows;
  uint8_t maxCols;
};

void clearValue(uint8_t row);

#endif