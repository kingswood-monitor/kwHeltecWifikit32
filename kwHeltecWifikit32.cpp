// Wifi Kit 32 firmware

#include <kwHeltecWifikit32.h>

SSD1306AsciiWire oled;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

char buf[10] = {0};

uint8_t col0 = 0;  // First value column
uint8_t col1 = 0;  // Last value column.
uint8_t rows;      // Rows per line.

// PUBLIC ///////////////////////////////////////////////////////////////////////

// kwHeltecWifikit32 constructor
kwHeltecWifikit32::kwHeltecWifikit32()
{
    getMacAddress();
    statusTopicID = registerMetaTopic("status");
    pinMode(LED, OUTPUT);
}

// Initalise the OLED display
void kwHeltecWifikit32::initDisplay(int pin_rst, int pin_sda, int pin_scl)
{
    initDisplay(pin_rst, pin_sda, pin_scl, false);
}

void kwHeltecWifikit32::initDisplay(int pin_rst, int pin_sda, int pin_scl, bool doRemap)
{  
    Wire.begin(pin_sda, pin_scl);
    Wire.setClock(400000L);

    oled.begin(&Adafruit128x64, I2C_ADDRESS, pin_rst);

    // NOTE - this gives a 4 x 13 usable display
    // oled.setRow(row * oled.fontRows());
    // oled.setCol(col * oled.fontWidth());
    oled.setFont(Callibri15);
    oled.setLetterSpacing(2);
    maxRows = oled.displayHeight() / (oled.fontRows() * 8); // 4
    maxCols = oled.displayWidth() / oled.fontWidth();       // 13

    oled.displayRemap(doRemap);
    oled.clear();
    
    updateSystemStatus(deviceID);
    delay(5000);
}

// Initialise the Wifi - return true if successful
bool kwHeltecWifikit32::initWiFi(const char* wifi_ssid, const char* wifi_pwd)
{
    updateSystemStatus("[->] WiFi");

    WiFi.disconnect();
    WiFi.mode(WIFI_MODE_STA);
    WiFi.begin(wifi_ssid);
    
    while (WiFi.status() != WL_CONNECTED) 
    {
        delay(500);
    };

    updateSystemStatus("WiFi OK");
    delay(500);

    return WiFi.status() == WL_CONNECTED;
}

// Initialise MTTQ
void kwHeltecWifikit32::initMTTQ(IPAddress mqtt_host, std::string topic_root)
{
    topicRoot = topic_root;
    mqttClient.setServer(mqtt_host, 1883);
    mqttClient.setCallback(mqttCallback);
    didInitialiseMTTQ = true;
}

// Register data topic
uint8_t kwHeltecWifikit32::registerDataTopic(std::string fieldName, std::string units, std::string topicName, std::string sensorName)
{
    std::string topicString = topicRoot + "/" + "data" + "/" + topicName + "/" + sensorName + "/" + deviceID;
    dataTopics.push_back(dataField{topicString, fieldName, units});
    
    return dataTopics.size() - 1;
}

// register meta topic
uint8_t kwHeltecWifikit32::registerMetaTopic(std::string topicName)
{
    std::string topicString = topicRoot + "/" + "meta" + "/" + topicName + "/" + deviceID;
    metaTopics.push_back(topicString);

    return metaTopics.size() - 1;
}

// Publish 

void clearValue(uint8_t row) {oled.clear(col0, col1, row, row + row - 1); }

void kwHeltecWifikit32::publish(uint8_t fieldID, uint16_t data)
{
    sprintf(buf, "%d", data);
    const char* topic = dataTopics[fieldID].topicString.c_str();
    mqttClient.publish(topic, buf);

    clearValue( fieldID * oled.fontRows());
    oled.print(data, 1);
}

void kwHeltecWifikit32::publish(uint8_t fieldID, float data)
{
    sprintf(buf, "%.1f", data);
    const char* topic = dataTopics[fieldID].topicString.c_str();
    mqttClient.publish(topic, buf);

    clearValue( fieldID * oled.fontRows());
    oled.print(data, 1);
}

void kwHeltecWifikit32::display()
{
    if (!didSetUpForm) { setUpForm() ;}
}

// Display the labeled data at the specified row
void kwHeltecWifikit32::setUpForm()
{
    oled.clear();
    // Setup form and find longest labels
    for (uint8_t i = 0; i < dataTopics.size(); ++i)
    {
        const char *fieldName = dataTopics[i].fieldName.c_str();
        oled.println(fieldName);
        uint8_t w = oled.strWidth(fieldName);
        col0 = col0 < w ? w : col0; 
    }

    col0 += 3;
    col1 = col0 + oled.strWidth("4000") + 2;

    // Print units
    for (uint8_t i = 0; i < dataTopics.size(); ++i)
    {
        oled.setCursor(col1 + 1, i * oled.fontRows());
        oled.print(dataTopics[i].units.c_str());
    }

    didSetUpForm = true;
}

// Run - keep MQTT alive and process commands
void kwHeltecWifikit32::run()
{   
    if (didInitialiseMTTQ)
    {
        if (!mqttClient.connected()) 
        {   
            digitalWrite(LED, LOW);
            
            long now = millis();
            if (now - lastReconnectAttempt > MQTT_RECONNECT_TIME_SECONDS * 1000) 
            {            
                lastReconnectAttempt = now; 
                updateSystemStatus("[->] MQTT");
                
                if (mqttReconnect()) 
                {
                    lastReconnectAttempt = millis();
                }
            }
            else {} // Wait for timer to expire 
        } 
        else 
        {   
            mqttClient.loop();
        }
    }
}


// PRIVATE ///////////////////////////////////////////////////////////////////////

// Populate the device ID
void kwHeltecWifikit32::getMacAddress()
{
    String theAddress = WiFi.macAddress();
    theAddress.replace(":", "");
    strcpy(deviceID, theAddress.c_str());
}

// Display system status
void kwHeltecWifikit32::updateSystemStatus(std::string statusMessage)
{
    oled.setRow(3 * oled.fontRows());
    oled.setCol(0);
    oled.print(statusMessage.c_str());
    oled.clearToEOL();
}

// Rconnect MQTT
boolean kwHeltecWifikit32::mqttReconnect() 
{
    updateSystemStatus("[->] MQTT");

    const char* statusTopic = metaTopics[statusTopicID].c_str();

    if (mqttClient.connect(deviceID, statusTopic, 2, true, "OFFLINE")) 
    {
        updateSystemStatus("ONLINE");
        digitalWrite(LED, HIGH);
        
        mqttClient.publish(statusTopic, "ONLINE");
        lastReconnectAttempt = millis();
    }

    return mqttClient.connected();
}

// MQTT callback function
void kwHeltecWifikit32::mqttCallback(char* topic, byte* payload, unsigned int length)
{
    
}

