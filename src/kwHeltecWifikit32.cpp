#include <kwHeltecWifikit32.h>

char buf[10] = { 0 };
char timeString[8] = { 0 };

uint8_t col0 = 0;  // First value column
uint8_t col1 = 0;  // Last value column.

SSD1306AsciiWire  oled;
WiFiClient        wifiClient;
WiFiMulti         wifiMulti;
PubSubClient      mqttClient( wifiClient );
WiFiUDP           Udp;
unsigned int      localPort = 8888;  // local port to listen for UDP packets
static const char ntpServerName[] = "us.pool.ntp.org";
const int         NTP_PACKET_SIZE = 48;
byte              packetBuffer[NTP_PACKET_SIZE];
const int         timeZone = 0;
time_t            lastDisplay = 0;  // when the digital clock was displayed

// PUBLIC
// ///////////////////////////////////////////////////////////////////////

// kwHeltecWifikit32 constructor
kwHeltecWifikit32::kwHeltecWifikit32( HeltecConfig config ) : config{ config }
{
  getMacAddress();
  statusTopicID = registerMetaTopic( "status" );
  pinMode( LED, OUTPUT );
}

// Initalise the display, WiFi, and MQTT
void kwHeltecWifikit32::init()
{
  Wire.begin( PIN_SDA, PIN_SCL );
  Wire.setClock( 400000L );

  oled.begin( &Adafruit128x64, I2C_ADDRESS, PIN_RST );
  oled.setFont( Callibri15 );
  oled.setLetterSpacing( 2 );
  maxRows = oled.displayHeight() / ( oled.fontRows() * 8 );  // 4
  maxCols = oled.displayWidth() / oled.fontWidth();          // 13
  oled.displayRemap( config.rotateDisplay );

  setUpForm();

  updateSystemStatus( deviceID );
  delay( 5000 );

  initWiFi();
  initMTTQ( config.mqtt_host );
  initTime();
}

void kwHeltecWifikit32::initTime()
{
  Udp.begin( localPort );
  setSyncProvider( getNtpTime );
  setSyncInterval( 60 );
}

// Display the labeled data at the specified row
void kwHeltecWifikit32::setUpForm()
{
  oled.clear();
  // Setup form and find longest labels
  for ( uint8_t i = 0; i < dataTopics.size(); ++i )
  {
    const char* fieldName = dataTopics[i].fieldName.c_str();
    oled.println( fieldName );
    uint8_t w = oled.strWidth( fieldName );
    col0 = col0 < w ? w : col0;
  }

  col0 += 3;
  col1 = col0 + oled.strWidth( "4000" ) + 2;

  // Print units
  for ( uint8_t i = 0; i < dataTopics.size(); ++i )
  {
    oled.setCursor( col1 + 1, i * oled.fontRows() );
    oled.print( dataTopics[i].units.c_str() );
  }
}

// Register data topic
uint8_t kwHeltecWifikit32::registerField( std::string fieldName,
                                          std::string units,
                                          std::string topicName,
                                          std::string sensorName )
{
  std::string topicString = config.topicRoot + "/" + "data" + "/" + topicName +
                            "/" + sensorName + "/" + deviceID;
  dataTopics.push_back( dataField{ topicString, fieldName, units } );

  return dataTopics.size() - 1;
}

// register meta topic
uint8_t kwHeltecWifikit32::registerMetaTopic( std::string topicName )
{
  std::string topicString =
      config.topicRoot + "/" + "meta" + "/" + topicName + "/" + deviceID;
  metaTopics.push_back( topicString );

  return metaTopics.size() - 1;
}

// Initialise the Wifi - return true if successful
bool kwHeltecWifikit32::initWiFi()
{
  updateSystemStatus( "[->] WiFi" );

  wifiMulti.addAP( config.ap1.ssid, config.ap1.pwd );
  wifiMulti.addAP( config.ap2.ssid, config.ap2.pwd );
  wifiMulti.addAP( config.ap3.ssid, config.ap3.pwd );

  while ( wifiMulti.run() != WL_CONNECTED ) { ; }

  updateSystemStatus( "WiFi: connected" );

  delay( 500 );

  return WiFi.status() == WL_CONNECTED;
}

// Initialise MTTQ
void kwHeltecWifikit32::initMTTQ( IPAddress mqtt_host )
{
  mqttClient.setServer( mqtt_host, 1883 );
  mqttClient.setCallback( mqttCallback );
  didInitialiseMTTQ = true;
}

// Return true if it is midnight
bool kwHeltecWifikit32::isMidnight()
{
  return ( timeStatus() != timeNotSet )
             ? hour() == 0 && minute() == 0 && second() == 0
             : false;
}

// Publish

void kwHeltecWifikit32::publish( uint8_t fieldID, uint16_t data )
{
  sprintf( buf, "%d", data );
  const char* topic = dataTopics[fieldID].topicString.c_str();
  mqttClient.publish( topic, buf );
}

void kwHeltecWifikit32::publish( uint8_t fieldID, float data )
{
  sprintf( buf, "%.1f", data );
  const char* topic = dataTopics[fieldID].topicString.c_str();
  mqttClient.publish( topic, buf );
}

void clearValue( uint8_t row ) { oled.clear( col0, col1, row, row ); }

void kwHeltecWifikit32::update( uint8_t fieldID, uint16_t data )
{
  clearValue( fieldID * oled.fontRows() );
  oled.print( data, 1 );
}

void kwHeltecWifikit32::update( uint8_t fieldID, float data )
{
  clearValue( fieldID * oled.fontRows() );
  oled.print( data, 1 );
}

void kwHeltecWifikit32::update( uint8_t fieldID, const char* message )
{
  clearValue( fieldID * oled.fontRows() );
  oled.print( message );
}

// Display system status
void kwHeltecWifikit32::updateSystemStatus( std::string statusMessage )
{
  oled.setCursor( 0, 3 * oled.fontRows() ), oled.clearToEOL();
  oled.print( statusMessage.c_str() );
}

// Run - keep MQTT alive and process commands
void kwHeltecWifikit32::run()
{
  if ( didInitialiseMTTQ )
  {
    if ( !mqttClient.connected() )
    {
      digitalWrite( LED, LOW );

      long now = millis();
      if ( now - lastReconnectAttempt > MQTT_RECONNECT_TIME_SECONDS * 1000 )
      {
        lastReconnectAttempt = now;
        updateSystemStatus( "[->] MQTT" );

        if ( mqttReconnect() ) { lastReconnectAttempt = millis(); }
      } else
      {
      }  // Wait for timer to expire
    } else
    {
      mqttClient.loop();
    }
  }

  if ( timeStatus() != timeNotSet )
  {
    if ( now() != lastDisplay )
    {  // update the display only if time has changed
      char buf[12];
      lastDisplay = now();
      sprintf( buf, "%02d:%02d:%02d", hour(), minute(), second() );
      updateSystemStatus( buf );
    }
  } else
  {
    updateSystemStatus( "Time not set" );
  }
}

// PRIVATE
// ///////////////////////////////////////////////////////////////////////

// Populate the device ID
void kwHeltecWifikit32::getMacAddress()
{
  String theAddress = WiFi.macAddress();
  theAddress.replace( ":", "" );
  strcpy( deviceID, theAddress.c_str() );
}

// Rconnect MQTT
boolean kwHeltecWifikit32::mqttReconnect()
{
  updateSystemStatus( "[->] MQTT" );

  const char* statusTopic = metaTopics[statusTopicID].c_str();

  if ( mqttClient.connect( deviceID, statusTopic, 2, true, "OFFLINE" ) )
  {
    updateSystemStatus( "ONLINE" );
    digitalWrite( LED, HIGH );

    mqttClient.publish( statusTopic, "ONLINE" );
    lastReconnectAttempt = millis();
  }

  return mqttClient.connected();
}

// MQTT callback function
void kwHeltecWifikit32::mqttCallback( char* topic, byte* payload,
                                      unsigned int length )
{
}

time_t getNtpTime()
{
  IPAddress ntpServerIP;  // NTP server's ip address

  while ( Udp.parsePacket() > 0 )
    ;  // discard any previously received packets
  Serial.println( "Transmit NTP Request" );
  // get a random server from the pool
  WiFi.hostByName( ntpServerName, ntpServerIP );
  Serial.print( ntpServerName );
  Serial.print( ": " );
  Serial.println( ntpServerIP );
  sendNTPpacket( ntpServerIP );
  uint32_t beginWait = millis();
  while ( millis() - beginWait < 3000 )
  {
    int size = Udp.parsePacket();
    if ( size >= NTP_PACKET_SIZE )
    {
      Serial.println( "Receive NTP Response" );
      Udp.read( packetBuffer,
                NTP_PACKET_SIZE );  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println( "No NTP Response :-(" );
  return 0;  // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket( IPAddress& address )
{
  // set all bytes in the buffer to 0
  memset( packetBuffer, 0, NTP_PACKET_SIZE );
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;  // LI, Version, Mode
  packetBuffer[1] = 0;           // Stratum, or type of clock
  packetBuffer[2] = 6;           // Polling Interval
  packetBuffer[3] = 0xEC;        // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket( address, 123 );  // NTP requests are to port 123
  Udp.write( packetBuffer, NTP_PACKET_SIZE );
  Udp.endPacket();
}