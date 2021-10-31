#ifndef H_KWHELTECWIFIKIT32
#define H_KWHELTECWIFIKIT32

#define MQTT_SOCKET_TIMEOUT 1

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"
#include <kwTime.h>

#define I2C_ADDRESS 0x3C
#define MQTT_RECONNECT_TIME_SECONDS 5
#define MAX_TOPIC_BUFFER_LEN 50 // <- remove

struct dataField
{
  std::string topicString;
  std::string fieldName;
  std::string units;
};

class kwHeltecWifikit32 
{
public:
  kwHeltecWifikit32();
  
  // Display methods
  void initDisplay(int pin_rst, int pin_sda, int pin_scl);
  void initDisplay(int pin_rst, int pin_sda, int pin_scl, bool doRemap);
  void display();
  
  // WiFi / MQTT methods
  bool initWiFi(const char* SSID, const char* PWD);
  void initMTTQ(IPAddress mqtt_host, std::string topic_root);
  uint8_t registerDataTopic(std::string fieldName, std::string units, std::string topicName, std::string sensorName);
  uint8_t registerMetaTopic(std::string topicName);
  void publish(uint8_t fieldID, uint16_t data);
  void publish(uint8_t fieldID, float data);
  
  // Real Time methods
  void initTimeSync();
  bool isMidnight();
  
  void run();
  
  char deviceID[16] = {0};
  std::vector<dataField> dataTopics;
  std::vector<std::string> metaTopics;
  std::vector<std::string> commands;
  uint8_t statusTopicID;

private:
  void getMacAddress();
  void updateSystemStatus(std::string statusMessage);
  static void mqttCallback(char* topic, byte* payload, unsigned int length);
  boolean mqttReconnect();
  void setUpForm();

  bool didSetUpForm = false;

  std::string topicRoot = "kw_sensors";
  bool didInitialiseMTTQ = false;
  long lastReconnectAttempt = 0;
  uint8_t maxRows;
  uint8_t maxCols;
};

void clearValue(uint8_t row);

#endif